/* P.A.S.S. — trap.c
 * Trap-fallback (гибридная стратегия): инструкции, пропущенные
 * статическим рескрайнингом (например, сгенерированные JIT-ом),
 * ловятся по SIGILL (Linux) и эмулируются по месту. Результат —
 * модификация контекста прерванного потока и продолжение.
 * Windows: тот же путь через exception event (VEH).              */
#include "pass.h"
#include <signal.h>
#include <stdlib.h>

/* Предохранитель trap-цикла: подряд идущие неудачи эмуляции на одном и
 * том же PC. Некоторые рантаймы перехватывают SIGILL и возвращают
 * управление на ту же инструкцию — без счётчика это выглядит как
 * безмолвное зависание со 100% CPU и без единого сообщения (баг
 * bun.exe, повторённый на Linux). Порог 64 — законный SIGILL-probing
 * (детекция инструкций через сигнал) всегда либо продвигает PC
 * обработчиком, либо редок; 64 подряд = замкнутый цикл. */
#define TRAP_FAIL_LIMIT 64
static app_pc g_last_fail_pc;
static int g_fail_streak;

/* громкое завершение при замкнутом trap-цикле: молчаливое зависание
 * со 100% CPU хуже честного краха с диагнозом */
static void trap_deadloop_abort(void *dc, instr_t *ins)
{
    dr_fprintf(STDERR,
               "P.A.S.S.: замкнутый trap-цикл — приложение возвращает\n"
               "управление на одну и ту же инструкцию %d раз подряд:\n"
               "  ", TRAP_FAIL_LIMIT);
    instr_disassemble(dc, ins, STDERR);
    dr_fprintf(STDERR,
               "\nP.A.S.S. не может её эмулировать. Чтобы не зависать\n"
               "безмолвно со 100%% CPU — завершаю процесс (код 86).\n");
    dr_exit_process(86);
}

/* Эмуляция инструкции из контекста сигнала.
 * Возвращает 1 при успехе (pc в mc продвинут), 0 — не смогли. */
static int trap_emulate(void *dc, dr_mcontext_t *mc)
{
    instr_t *ins = instr_create(dc);
    if (ins == NULL)
        return 0;
    if (decode(dc, mc->pc, ins) == NULL || !instr_valid(ins)) {
        instr_destroy(dc, ins);
        return 0;
    }
    int opc = instr_get_opcode(ins);
    opnd_size_t dsz = (instr_num_dsts(ins) > 0) ?
                      opnd_get_size(instr_get_dst(ins, 0)) : OPSZ_NA;
    int is_vex = instr_bytes_vex(ins);

    /* предохранитель trap-цикла — считается для ЛЮБОГО SIGILL: замкнутый
     * цикл «инструкция → SIGILL → обработчик возвращает на неё же» выглядит
     * одинаково и для неэмулируемых опкодов, и для неизвестных нам
     * (passthrough). Законный probing всегда продвигает PC обработчиком
     * или уходит longjmp'ом — подряд один и тот же PC не встречается. */
    if (g_last_fail_pc == mc->pc)
        g_fail_streak++;
    else {
        g_last_fail_pc = mc->pc;
        g_fail_streak = 1;
    }

    /* диагностика: каждое trap-срабатывание видно в -verbose 1.
     * Без этого «закрытый» trap-цикл (одна и та же инструкция снова и
     * снова) выглядит как безмолвный спин 100% CPU. */
    if (g_cfg.verbose >= 1)
        dr_fprintf(STDERR,
                   "[P.A.S.S.] trap: SIGILL opc=%d @ " PIFX " (%d/%d)\n",
                   opc, (ptr_uint_t)mc->pc, g_fail_streak, TRAP_FAIL_LIMIT);

    if (opc_supported(opc, dsz) && opc != OP_cpuid && opc != OP_xgetbv) {
        if (g_fail_streak >= TRAP_FAIL_LIMIT)
            trap_deadloop_abort(dc, ins);
        /* B-2011 diag: ud2-доставка — дамп контекста, чтобы опознать
         * источник SIGILL (JSC probe-сентинел / Zig unreachable / иное).
         * Гейт verbose>=1 (B-2012: доставка SIGILL в app-обработчик —
         * чистый repro: bun -target native -cpu max → rc=132). */
        if (opc == OP_ud2 && g_cfg.verbose >= 1) {
            uint8_t *p = (uint8_t *)mc->pc;
            dr_fprintf(STDERR,
                       "[P.A.S.S.][UD2DUMP] pc=" PIFX " rax=" PIFX
                       " rcx=" PIFX " rdx=" PIFX " rbx=" PIFX
                       " rsp=" PIFX " rbp=" PIFX " rsi=" PIFX
                       " rdi=" PIFX " r8=" PIFX " r9=" PIFX
                       " r10=" PIFX " r11=" PIFX " fl=%08x\n  bytes:",
                       (ptr_uint_t)mc->pc, mc->xax, mc->xcx, mc->xdx,
                       mc->xbx, mc->xsp, mc->xbp, mc->xsi, mc->xdi,
                       mc->r8, mc->r9, mc->r10, mc->r11,
                       (unsigned)mc->xflags);
            for (int b = -16; b < 8; b++)
                dr_fprintf(STDERR, " %02x", p[b]);
            dr_fprintf(STDERR, "\n");
        }
        instr_destroy(dc, ins);
        return 0;   /* инструкция легальна — SIGILL по другой причине */
    }

    /* cpuid/xgetbv → простая подмена регистров */
    if (opc == OP_cpuid) {
        uint32_t leaf = (uint32_t)mc->xax, sub = (uint32_t)mc->xcx;
        uint32_t a = 0, b = 0, c = 0, d = 0;
        /* эмулируем через тот же движок витрины: заполняем BUF_CPU */
        void *thdc = dr_get_current_drcontext();
        uint8_t *buf = (uint8_t *)drmgr_get_tls_field(thdc, g_tls_idx_buf);
        uint32_t *cpu = (uint32_t *)(buf + BUF_CPU);
        cpu[0] = leaf; cpu[1] = sub;
        emu_cpuid();
        a = cpu[2]; b = cpu[3]; c = cpu[4]; d = cpu[5];
        mc->xax = a; mc->xbx = b; mc->xcx = c; mc->xdx = d;
        mc->pc += instr_length(dc, ins);
        instr_destroy(dc, ins);
        return 1;
    }
    if (opc == OP_xgetbv) {
        mc->xax = has(exposed_features(), F_AVX) ? 0x7 : 0x3;
        mc->xdx = 0;
        mc->pc += instr_length(dc, ins);
        instr_destroy(dc, ins);
        return 1;
    }

    /* остальное — через generic-эмулятор на контексте сигнала */
    emu_desc_t *d = (emu_desc_t *)dr_global_alloc(sizeof(emu_desc_t));
    memset(d, 0, sizeof(*d));
    d->opc = opc;
    d->ndst = instr_num_dsts(ins); if (d->ndst > 2) d->ndst = 2;
    d->nsrc = instr_num_srcs(ins); if (d->nsrc > 5) d->nsrc = 5;
    for (int k = 0; k < d->ndst; k++) d->dsts[k] = instr_get_dst(ins, k);
    for (int k = 0; k < d->nsrc; k++) d->srcs[k] = instr_get_src(ins, k);
    /* #115: флаги — по IR-модели инструкции, как в статическом пути
     * (build_desc, rewrite.c: instr_get_eflags → flags_def → popf).
     * IR-модель покрывает ВСЕ флагоопределяющие опкоды — ptest/vtest,
     * все 8 форм PCMPxSTRx (§7.4 #111), adcx/adox, lzcnt/tzcnt/popcnt/
     * bzhi/pdep/pext, vcmpss/sd (§7.5), blendv-класс. Прежний жёсткий
     * список (ptest, pcmpe-формы, adcx/adox) отбрасывал результат эмуляции
     * для остальных: ZF/PF/CF, вычисленные emu в BUF_FLAGS, не
     * возвращались приложению — ветвление шло по stale-флагам (тот же
     * механизм, что «malloc(): corrupted top size» §6.x: сместившийся
     * DF у node 8000+ эмуляций). Симптом B-2004: паника аллокатора bun
     * «memory allocation of N bytes failed» на реальном CPU без AVX —
     * trap-путь на нём исполняется, на CI-хосте с AVX невидим. */
    {
        uint efl = instr_get_eflags(ins, DR_QUERY_DEFAULT);
        d->flags_def = (efl & (EFLAGS_WRITE_CF | EFLAGS_WRITE_ZF |
                               EFLAGS_WRITE_SF | EFLAGS_WRITE_OF |
                               EFLAGS_WRITE_PF | EFLAGS_WRITE_AF |
                               EFLAGS_WRITE_DF)) != 0;
    }
    if (opc == OP_blendvps || opc == OP_blendvpd || opc == OP_pblendvb ||
        opc == OP_pcmpestri || opc == OP_pcmpestrm ||
        opc == OP_vpcmpestri || opc == OP_vpcmpestrm)
        d->uses_xmm0 = 1;
    if (opc == OP_mulx) d->gpr_src |= (1u << gpr_idx(DR_REG_RDX));
    d->is_ymm = (dsz == OPSZ_32);
    d->is_vex = (uint8_t)is_vex;
    /* rip-относительные операнды: app-PC следующей инструкции */
    d->pc_next = dr_fragment_app_pc(mc->pc) + instr_length(dc, ins);

    /* строим состояние из mc + shadow и запускаем универсальный код:
       используем BUF как рабочую область (спил всех GPR/XMM) */
    void *thdc = dr_get_current_drcontext();
    uint8_t *buf = (uint8_t *)drmgr_get_tls_field(thdc, g_tls_idx_buf);
    uint8_t *raw = (uint8_t *)drmgr_get_tls_field(thdc, g_tls_idx_raw);

    uint64_t *gpr = (uint64_t *)(buf + BUF_GPR(0));
    gpr[0] = mc->xax; gpr[1] = mc->xcx; gpr[2] = mc->xdx; gpr[3] = mc->xbx;
    gpr[4] = mc->xsp; gpr[5] = mc->xbp; gpr[6] = mc->xsi; gpr[7] = mc->xdi;
    gpr[8] = mc->r8;  gpr[9] = mc->r9;  gpr[10] = mc->r10; gpr[11] = mc->r11;
    gpr[12] = mc->r12; gpr[13] = mc->r13; gpr[14] = mc->r14; gpr[15] = mc->r15;
    /* xmm: из mc (если есть) */
    for (int k = 0; k < 16; k++) {
        memcpy(buf + BUF_XMM(k), &mc->simd[k], 16);
    }
    /* флаги из mc */
    *(uint32_t *)(buf + BUF_FLAGS) = (uint32_t)mc->xflags;

    if (!emu_generic(d)) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.] trap: НЕТ эмуляции opc=%d @ " PIFX
                   " — передаём приложению: ", opc, (ptr_uint_t)mc->pc);
        instr_disassemble(dc, ins, STDERR);
        dr_fprintf(STDERR, " (серия %d/%d)\n", g_fail_streak, TRAP_FAIL_LIMIT);
        if (g_fail_streak >= TRAP_FAIL_LIMIT)
            trap_deadloop_abort(dc, ins);
        dr_global_free(d, sizeof(emu_desc_t));
        instr_destroy(dc, ins);
        return 0;   /* нет эмуляции — SIGILL уйдёт приложению (громкое падение) */
    }

    /* обратная запись */
    mc->xax = gpr[0]; mc->xcx = gpr[1]; mc->xdx = gpr[2]; mc->xbx = gpr[3];
    mc->xsp = gpr[4]; mc->xbp = gpr[5]; mc->xsi = gpr[6]; mc->xdi = gpr[7];
    mc->r8 = gpr[8];  mc->r9 = gpr[9];  mc->r10 = gpr[10]; mc->r11 = gpr[11];
    mc->r12 = gpr[12]; mc->r13 = gpr[13]; mc->r14 = gpr[14]; mc->r15 = gpr[15];
    for (int k = 0; k < 16; k++) {
        memcpy(&mc->simd[k], buf + BUF_XMM(k), 16);
    }
    if (d->flags_def)
        mc->xflags = (*(uint32_t *)(buf + BUF_FLAGS) & 0xFFD5) |
                     (mc->xflags & ~0xFFD5ull);
    mc->pc += instr_length(dc, ins);

    dr_global_free(d, sizeof(emu_desc_t));
    instr_destroy(dc, ins);
    PASS_ATOMIC_INC32(&g_stat_trap);
    PASS_ATOMIC_INC32(&g_dyn_trap);
    (void)raw;
    return 1;
}

#ifdef UNIX
static dr_signal_action_t pass_signal_event(void *dc, dr_siginfo_t *info)
{
    /* диагностика падений: транслятированный PC приложения + адрес сбоя */
    if ((info->sig == SIGSEGV || info->sig == SIGABRT || info->sig == SIGBUS) &&
        g_cfg.verbose >= 1) {
        app_pc pc = (info->mcontext != NULL) ? info->mcontext->pc
                  : (info->raw_mcontext_valid && info->raw_mcontext != NULL
                     ? info->raw_mcontext->pc : NULL);
        dr_fprintf(STDERR,
                   "[P.A.S.S.] SIGSEGV @ app_pc " PIFX
                   " access=" PIFX "\n  rax=" PIFX " rbx=" PIFX
                   " rcx=" PIFX " rdx=" PIFX " rdi=" PIFX " rsi=" PIFX
                   " rsp=" PIFX " rbp=" PIFX "\n  r8=" PIFX " r9=" PIFX
                   " r10=" PIFX " r11=" PIFX " r12=" PIFX " r13=" PIFX
                   " r14=" PIFX " r15=" PIFX "\n",
                   pc ? (ptr_uint_t)pc : 0,
                   (ptr_uint_t)info->access_address,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xax : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xbx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xcx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xdx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xdi : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xsi : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xsp : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xbp : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r8 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r9 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r10 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r11 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r12 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r13 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r14 : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->r15 : 0);
        /* B-2014: rbp-трейс — bun собран с frame pointers; покадрово
         * печатаем адреса возврата, чтобы локализовать, где ломается
         * живой аргумент (rsi) при вложенных вызовах. */
        if (pc != NULL && info->mcontext != NULL) {
            ptr_uint_t rbp = (ptr_uint_t)info->mcontext->xbp;
            int f;
            for (f = 0; f < 12 && rbp; f++) {
                ptr_uint_t ret = 0, next = 0;
                if (!dr_safe_read((void *)(rbp + 8), sizeof(ret), &ret, NULL) ||
                    !dr_safe_read((void *)rbp, sizeof(next), &next, NULL))
                    break;
                dr_fprintf(STDERR, "  frame[%02d] rbp=" PIFX " ret=" PIFX "\n",
                           f, rbp, ret);
                if (next <= rbp)
                    break; /* цепочка сошла на нет или в другой стек */
                rbp = next;
            }
        }
    }
    /* B-2009: не наша инструкция — DR_SIGNAL_DELIVER (доставка как при
     * нативном исполнении). Намеренные ud2-trap'ы JIT-кода JSC (OSR)
     * обрабатываются самим приложением; «пустой console.log» и краш fs
     * были вызваны НЕ доставкой, а потерей входа флагов эмулятора —
     * захват (2) обязан заполнять BUF_FLAGS (вход emu), а не только
     * BUF_FLAGS0 (пред-оп для restore_state). */
    if (info->sig != SIGILL)
        return DR_SIGNAL_DELIVER;
    if (!info->raw_mcontext_valid || info->raw_mcontext == NULL)
        return DR_SIGNAL_DELIVER;
    dr_mcontext_t *mc = info->raw_mcontext;
    if (trap_emulate(dc, mc)) {
        return DR_SIGNAL_REDIRECT;
    }
    /* не наша инструкция — отдать приложению как при нативном исполнении */
    return DR_SIGNAL_DELIVER;
}
#else
/* Windows: настоящий trap-fallback через exception event.
 * B-2015, контракт семантики (dr_events.h + drmgr.h, drmgr — passthrough):
 *   true  = исключение ДОСТАВЛЯЕТСЯ приложению (app VEH/SEH-цепочке);
 *   false = переисполнить инструкцию по raw_mcontext (для fault-фастпата);
 *   «обработано и продолжить» = dr_redirect_execution() из колбэка.
 * exc->mcontext — ТРАНСЛИРОВАННЫЙ app-контекст: правим регистры,
 * pc += len и редиректим — DR продолжит с нового app-PC.            */
#ifndef EXCEPTION_ILLEGAL_INSTRUCTION
#define EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001D
#endif
/* B-2015: программные исключения, которые НАТИВНАЯ Windows глотает тихо.
 * RaiseException(code, 0, ...) идёт через RtlRaiseException, который
 * кладёт в доставленный контекст RIP = АДРЕС ВОЗВРАТА (инструкция после
 * вызова RaiseException) — «обработать» = продолжить оттуда: ровно то,
 * что делает __except(EXECUTE_HANDLER)-обёртка MSVC-макроса SetThreadName
 * и системная логика OutputDebugString.
 * Под DR 11.91 эти коды уходили в unhandled → DR завершал процесс с кодом
 * исключения (натурный репрод Godot 4.5: NtTerminateProcess(self,
 * 0x406D1388) — «окно появилось и закрылось», exit 0x406D1388).
 * 0x406D1388  — MSVC thread-name (драйверы NVIDIA/AMD/Intel, Vulkan-слой);
 * 0x40010006  — DBG_PRINTEXCEPTION_C  (OutputDebugStringA);
 * 0x4001000A  — DBG_PRINTEXCEPTION_WIDE_C (OutputDebugStringW, Win10+). */
static bool exc_is_swallowed(unsigned int code)
{
    return code == 0x406D1388u || code == 0x40010006u || code == 0x4001000Au;
}
/* не-AV коды, всегда значимые для диагностики (редкие, не JIT-мусор) */
static bool exc_is_diag(unsigned int code)
{
    return code == 0xC000001Du /* illegal instr */ ||
           code == 0xC0000409u /* fast-fail (Zig panic → abort) */ ||
           code == 0xC00000FDu /* stack overflow */ ||
           exc_is_swallowed(code);
}
static bool pass_exception_event(void *dc, dr_exception_t *exc)
{
    if (exc == NULL || exc->mcontext == NULL)
        return false;
    {
        static volatile int exclog = -1;
        unsigned int code =
            (exc->record != NULL) ? (unsigned)exc->record->ExceptionCode : 0;
        int log_av = 0;
        if (exclog == -1)
            exclog = (getenv("PASS_EXCLOG") != NULL) ? 1 : 0;
        /* B-2015: не-AV значимые коды — ВСЕГДА (гейт getenv ненадёжен в
         * инжектируемом процессе — см. pass.c §7.3); AV (0xC0000005) —
         * только под PASS_EXCLOG, но при нём первые 32 всё равно дают
         * полный breadcrumbs-фотофинал фатального AV. */
        if (code == 0xC0000005u) {
            static volatile long av_logged;
            if (exclog && av_logged < 32) {
                av_logged++;
                log_av = 1;
            }
        }
        if (exc->record != NULL && (exc_is_diag(code) || log_av))
            dr_fprintf(STDERR,
                       "[EXC] code=0x%08x addr=" PIFX " rawpc=" PIFX
                       " apppc=" PIFX "\n",
                       (unsigned)exc->record->ExceptionCode,
                       (exc->record->ExceptionCode == 0xC0000005 &&
                        exc->record->NumberParameters >= 2)
                           ? (ptr_uint_t)exc->record->ExceptionInformation[1]
                           : 0,
                       (ptr_uint_t)exc->raw_mcontext->pc,
                       (ptr_uint_t)exc->mcontext->pc);
        /* глотание системных исключений: продолжение с доставленного RIP
         * (= адрес возврата из RaiseException) — контекст УЖЕ правильный,
         * править нечего. Возврат после redirect не важен. */
        if (exc_is_swallowed(code)) {
            if (g_cfg.verbose)
                dr_fprintf(STDERR,
                           "[P.A.S.S.] swallow benign exception 0x%08x "
                           "(нативно глотается системой)\n", code);
            dr_redirect_execution(exc->mcontext);
            return true;
        }
    }
    if (exc->record == NULL ||
        exc->record->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
        return false;
    if (g_cfg.verbose) {
        dr_fprintf(STDERR, "[P.A.S.S.] trap (Win): illegal instr @ " PIFX "\n",
                   (ptr_uint_t)exc->mcontext->pc);
    }
    if (trap_emulate(dc, exc->mcontext)) {
        /* B-2015: СЕМАНТИКА ИСПРАВЛЕНА. Контракт DR (dr_events.h):
         * «true = exception is DELIVERED to the application's handler»,
         * «false = faulting instruction is RE-EXECUTED». Прежний код
         * возвращал true после эмуляции → эмулированное #UD уходило в
         * app-обработчик (продолжения не было), а «не наша» ветка с false
         * переисполняла ту же инструкцию вечно. Правильное «handled,
         * continue с продвинутым pc» — dr_redirect_execution. */
        dr_redirect_execution(exc->mcontext);
        return true;
    }
    /* не наша инструкция — ДОСТАВИТЬ приложению (true = deliver to app
     * по контракту DR; нет обработчика → DR завершит с кодом 0xC000001D,
     * как нативно). */
    return true;
}

/* ── B-2015: перехват В ИСТОЧНИКЕ — drwrap-замена kernel32!RaiseException ──
 * Хронология недостижимых уровней (CI-циклы):
 *  (1) drmgr exception event — «Note: not called for RaiseException»;
 *  (2) наш VEH (первый в цепочке, CONTINUE_EXECUTION) — на Server 2025
 *      доставки НЕ существует вовсе;
 *  (3) pre-syscall NtRaiseException — raiser до syscall НЕ доходит;
 *  (4) fake-success NtTerminateProcess — БЕСКОНЕЧНЫЙ ЦИКЛ: fake → ntdll
 *      переразводит через RtlRaiseStatus → шорт-кат → terminate → fake...
 * Замена ФУНКЦИИ — единственный уровень до всей OS-механики: для кодов-
 * глотаторов просто ВЫХОДИМ (эквивалент тихого __except макроса
 * SetThreadName), для остальных честно реплицируем kernel32!RaiseException
 * через ntdll!RtlRaiseException (сама RaiseException заменена — прямой
 * вызов ntdll избегает рекурсии).                                   */
#include "drwrap.h"

static VOID (NTAPI *g_pfn_RtlRaiseException)(PEXCEPTION_RECORD);

static VOID NTAPI pass_RaiseException_repl(DWORD code, DWORD flags,
                                           SIZE_T nargs, const ULONG_PTR *args)
{
    unsigned uc = (unsigned)code;
    {   /* B-2015 diag: замена жива? печатаем КАЖДЫЙ вызов (кап 30) */
        static volatile long repl_seen;
        if (repl_seen < 30) {
            repl_seen++;
            dr_fprintf(STDERR, "[P.A.S.S.][REPL] RaiseException code=0x%08x\n", uc);
        }
    }
    if (uc == 0x406D1388u || uc == 0x40010006u || uc == 0x4001000Au) {
        extern int g_n_raise_swallowed; /* strace.c */
        if (g_n_raise_swallowed < 50)
            dr_fprintf(STDERR,
                       "[P.A.S.S.] swallow RaiseException(0x%08x) — "
                       "thread-name/ODS (drwrap-replace, B-2015)\n", uc);
        g_n_raise_swallowed++;
        return; /* тихое продолжение: приложение видит обычный возврат */
    }
    /* честная реплика kernel32!RaiseException для остальных кодов */
    if (g_pfn_RtlRaiseException != NULL) {
        EXCEPTION_RECORD rec;
        SIZE_T n = nargs;
        memset(&rec, 0, sizeof(rec));
        rec.ExceptionCode = code;   /* DWORD, NTSTATUS-тип не нужен */
        rec.ExceptionFlags = flags & EXCEPTION_NONCONTINUABLE;
        if (n > EXCEPTION_MAXIMUM_PARAMETERS)
            n = EXCEPTION_MAXIMUM_PARAMETERS;
        rec.NumberParameters = (ULONG)n;
        for (SIZE_T i = 0; i < n; i++)
            rec.ExceptionInformation[i] = args[i];
        g_pfn_RtlRaiseException(&rec);
    }
}

void pass_raise_hook_install(void)
{
    module_data_t *k32md, *ntdmd;
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE kbase = GetModuleHandleA("kernelbase.dll");
    HMODULE ntd = GetModuleHandleA("ntdll.dll");
    void *rtl;
    void *a1, *a2, *a3;
    if (ntd == NULL)
        return;
    rtl = (void *)GetProcAddress(ntd, "RtlRaiseException");
    g_pfn_RtlRaiseException = (VOID (NTAPI *)(PEXCEPTION_RECORD))rtl;
    if (!drwrap_init()) {
        dr_fprintf(STDERR, "[P.A.S.S.] drwrap_init FAILED\n");
        return;
    }
    /* B-2015 diag: регистрируем ВСЕ возможные адреса RaiseException:
     * kernel32 (может быть форвардер-стаб), kernelbase (реальное тело)
     * и dr_get_proc_address (LSB-нормализация DR). */
    a1 = (void *)GetProcAddress(k32, "RaiseException");
    a2 = (kbase != NULL) ? (void *)GetProcAddress(kbase, "RaiseException") : NULL;
    k32md = dr_lookup_module_by_name("KERNEL32.DLL");
    a3 = (k32md != NULL) ? (void *)dr_get_proc_address(k32md->handle, "RaiseException") : NULL;
    if (k32md != NULL)
        dr_free_module_data(k32md);
    dr_fprintf(STDERR,
               "[P.A.S.S.] RaiseException addrs: k32=" PIFX " kbase=" PIFX
               " drpa=" PIFX " rtl=" PIFX "\n",
               (ptr_uint_t)a1, (ptr_uint_t)a2, (ptr_uint_t)a3, (ptr_uint_t)rtl);
    {
        int ok = 0;
        if (a1 != NULL && drwrap_replace(a1, (void *)pass_RaiseException_repl, false))
            ok++;
        if (a2 != NULL && a2 != a1 &&
            drwrap_replace(a2, (void *)pass_RaiseException_repl, false))
            ok++;
        if (a3 != NULL && a3 != a1 && a3 != a2 &&
            drwrap_replace(a3, (void *)pass_RaiseException_repl, false))
            ok++;
        if (ok > 0)
            dr_fprintf(STDERR,
                       "[P.A.S.S.] RaiseException hook installed x%d (B-2015: "
                       "0x406D1388/ODS глотаются в источнике)\n", ok);
        else
            dr_fprintf(STDERR, "[P.A.S.S.] drwrap_replace(RaiseException) FAILED\n");
    }
}
#endif

void trap_init(void)
{
#ifdef UNIX
    drmgr_register_signal_event(pass_signal_event);
#else
    drmgr_register_exception_event(pass_exception_event);
#endif
}

void trap_exit(void)
{
}
