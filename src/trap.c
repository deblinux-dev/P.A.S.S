/* P.A.S.S. — trap.c
 * Trap-fallback (гибридная стратегия): инструкции, пропущенные
 * статическим рескрайнингом (например, сгенерированные JIT-ом),
 * ловятся по SIGILL (Linux) и эмулируются по месту. Результат —
 * модификация контекста прерванного потока и продолжение.
 * Windows: v0.1 только диагностика (exception event).              */
#include "pass.h"
#include <signal.h>
#include <stdlib.h>

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

    if (opc_supported(opc, dsz) && opc != OP_cpuid && opc != OP_xgetbv) {
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
    d->nsrc = instr_num_srcs(ins); if (d->nsrc > 4) d->nsrc = 4;
    for (int k = 0; k < d->ndst; k++) d->dsts[k] = instr_get_dst(ins, k);
    for (int k = 0; k < d->nsrc; k++) d->srcs[k] = instr_get_src(ins, k);
    d->flags_def = (opc == OP_ptest || opc == OP_pcmpestri ||
                    opc == OP_pcmpistri || opc == OP_adcx || opc == OP_adox);
    if (opc == OP_blendvps || opc == OP_blendvpd || opc == OP_pblendvb ||
        opc == OP_pcmpestri || opc == OP_pcmpestrm)
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
                   " rsp=" PIFX " rbp=" PIFX "\n",
                   pc ? (ptr_uint_t)pc : 0,
                   (ptr_uint_t)info->access_address,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xax : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xbx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xcx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xdx : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xdi : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xsi : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xsp : 0,
                   info->mcontext != NULL ? (ptr_uint_t)info->mcontext->xbp : 0);
    }
    if (info->sig != SIGILL)
        return DR_SIGNAL_DELIVER;
    if (!info->raw_mcontext_valid || info->raw_mcontext == NULL)
        return DR_SIGNAL_DELIVER;
    dr_mcontext_t *mc = info->raw_mcontext;
    if (trap_emulate(dc, mc)) {
        return DR_SIGNAL_REDIRECT;
    }
    /* не наша инструкция — отдать приложению (оно и так бы упало) */
    return DR_SIGNAL_DELIVER;
}
#else
/* Windows: настоящий trap-fallback через exception event.
 * Сигнатура drmgr: bool (*)(dc, dr_exception_t*) — true = исключение
 * обработано (не доставлять приложению). exc->mcontext — RAW-контекст
 * в точке сбоя: правим регистры и pc += len, DR продолжит исполнение. */
#ifndef EXCEPTION_ILLEGAL_INSTRUCTION
#define EXCEPTION_ILLEGAL_INSTRUCTION 0xC000001D
#endif
static bool pass_exception_event(void *dc, dr_exception_t *exc)
{
    if (exc == NULL || exc->mcontext == NULL)
        return false;
    if (exc->code != EXCEPTION_ILLEGAL_INSTRUCTION)
        return false;
    if (g_cfg.verbose) {
        dr_fprintf(STDERR, "[P.A.S.S.] trap (Win): illegal instr @ " PIFX "\n",
                   (ptr_uint_t)exc->mcontext->pc);
    }
    if (trap_emulate(dc, exc->mcontext)) {
        return true;   /* эмулировано — продолжить со следующим PC */
    }
    /* не наша инструкция — отдать приложению (громкое падение) */
    return false;
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
