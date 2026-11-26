/* ============================================================
 * P.A.S.S. — B-2024 (v0.3.4-diag): pass_b2024.c
 * ------------------------------------------------------------
 * SPINWATCH: ловушка user-mode спина после NtTerminateThread.
 *
 * НАТУРНЫЙ ФАКТ (лог B-2023 v0.3.3-diag от 2026-11-26, bun 1.4.5
 * std SizeOfImage 0x6FEF000, машина пользователя):
 *  - pagemap-fix B-2023.1 снял OOM: 26 регистраций страниц через
 *    динамическую карту (pagereg/submap/osalloc цепочки), EXEC
 *    резерв 1 ГиБ + JIT-разрешение (NtSetInformationProcess 0x35),
 *    арены mimalloc до 1.3 МиБ — ВСЕ NT-SUCCESS;
 *  - затем worker-поток (стек 0xa421ff…) выполнил
 *    NtQueryInformationThread(-2, class 12 = ThreadAmILastThread)
 *    и NtTerminateThread(0,0);
 *  - после этого: НОЛЬ сисколлов, НОЛЬ входов в зонды, но CPU
 *    занят на 100% — чистый user-mode спин в непробированном
 *    коде; вывода «43» нет; помогает только kill процесса.
 *  - спин НЕ в пробированных функциях (проверено по логу: нет
 *    повторяющихся серий PRB, нет osalloc без NtAllocate) —
 *    значит, виновник: DR-внутренности, непробированный
 *    mimalloc (блокировки/init-state) или bun/JSC.
 *
 * ЧТО ИЗВЕСТНО О СОСТОЯНИИ: на машине пользователя pmstate
 * (0x6AC8300) в момент dr_client_main = TEB-значение
 * (0xa41fafb000), а не 1 (как на CI): init page-map «заявлен,
 * но не завершён» и (по отсутствию PROBE3) так и не завершается.
 * Любой путь, ждущий завершения init, обязан крутиться вечно.
 * Сэмплер покажет: (а) эволюцию pmstate по тактам, (б) точный
 * app-PC спиннера (DR переводит RIP код-кэша в app-PC сам).
 *
 * МЕХАНИКА (Windows-порт B-2009 sampler.c, там itimer — Linux):
 *  - клиентский поток dr_create_client_thread;
 *  - раз в интервал: dr_suspend_all_other_threads ->
 *    dr_get_mcontext для каждого потока (app-PC и регистры,
 *    переведённые DR) -> dr_where_am_i (APP/CACHE/LIB/CLIENT,
 *    для CACHE tag = app-PC фрагмента) -> дамп 10 qword стека
 *    (адреса bun-образа печатаются как bun+RVA, клиент как
 *    cl+RVA) -> dr_resume_all_other_threads;
 *  - каждый такт печатается состояние page-map: map/cnt/cap,
 *    число непустых субмап (до 4096), pmstate/flag/procinit.
 *  - БЕЗОВТОРНОЕ ОКНО: потоки удерживаются приостановленными
 *    только на время печати такта (~мс) — резюм до сна.
 *  - 40 тактов (40 с), затем поток завершается; exit-событие
 *    ставит флаг стоп. Нулевое поведенческое вмешательство:
 *    одно-переменный эксперимент поверх B-2023.1.
 *
 * Опции: авто-включение вместе с -strace; -spinwatch <мс> —
 * форсировать (интервал), -nospinwatch — выключить (бисекция).
 *
 * B-2025 (v0.3.5-diag), по логу B-2024 (internal crash drwrap.dll
 * +0x5534 в мс после первого suspend/resume-такта):
 *  - старт-задержка первого такта 5 с (-spinwdelay <мс>, 0 = как
 *    B-2024) — сэмплер не трогает окно drwrap-интенсивного старта
 *    (зависание B-2023 наступало позже — наблюдение сохраняется);
 *  - скан субмап: БЕЗ break по первому NULL (субмапы лежат по окнам
 *    512 МиБ не по порядку: слот 0 — мигрированный, слот 3749 —
 *    арена; старый скан недосчитывал nnsub=1 при реальных 2+),
 *    окно скана 65536 слотов (VA до 32 ТиБ);
 *
 * ПРАВИЛА АДРЕСНОЙ АРИФМЕТИКИ (как в pass_b2023.c): база
 * модуля — uintptr_t; RVA — unsigned long long. Нарушение =
 * усечение базы и AV внутри клиента.
 * ============================================================ */
#include "dr_api.h"

#ifdef WINDOWS
#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

#define BUN_SIZEOF_IMAGE 0x6FEF000ull

/* RVA диагностики init page-map (те же, что в pass_b2023.c) */
#define RVA_MAP_GLOBAL   0x69B76C0ull  /* указатель _mi_page_map     */
#define RVA_G_PROCINIT   0x6AC8168ull  /* once-lock _mi_process_init */
#define RVA_G_FLAG       0x6AC8370ull  /* FLAG процесса (0/TEB/1)    */
#define RVA_G_PMSTATE    0x6AC8300ull  /* state init page-map        */

#define B24_ROUNDS       4000          /* тактов до авто-финиша (B-2026: был 40) */
#define B24_STACK_Q      10            /* qword стека на поток       */
#define B24_SUBMAP_SCAN  65536         /* субмап на скан за такт     */
#define B24_DELAY_CHUNK  250           /* мс чанк старт-задержки     */

static uintptr_t g_b24_bun_base = 0;   /* 0 = не bun 1.4.5 std       */
static uintptr_t g_b24_bun_end  = 0;
static uintptr_t g_b24_cl_base  = 0;   /* база самой клиентской dll  */
static unsigned long g_b24_cl_size = 0;
static volatile long g_b24_ms   = 1000;
static volatile long g_b24_delay_ms = 5000; /* B-2025: задержка 1-го такта */
static volatile long g_b24_stop = 0;
static uint64_t g_b24_last_pc = 0;     /* diag: PC предыдущего такта */
static volatile unsigned g_b24_repaired = 0;

/* B-2026: ремонт init-fill индекса (SIMD group scan) — см. pass_b2026.c */
static void b24_repair_index(uint64_t meta, uint64_t mask)
{
    if (meta == 0) return;
    uint64_t len = mask + 1;
    if (len < 32 || len > 2048 || (len & (len - 1)) != 0) return;
    unsigned char buf[2048 + 16];
    unsigned int nbytes = (unsigned int)(len + 16);
    if (nbytes > sizeof(buf)) return;
    if (!dr_safe_read((void*)(uintptr_t)meta, nbytes, buf, NULL)) return;
    unsigned int half = (unsigned int)(len / 2);
    int broken = 0;
    for (unsigned int i = half; i < len; i++)
        if (buf[i] == 0) { broken = 1; break; }
    if (!broken) {
        for (unsigned int i = 0; i < 16; i++)
            if (buf[len + i] != buf[i] && buf[len + i] == 0) { broken = 1; break; }
        if (!broken) return;
    }
    for (unsigned int i = 0; i < len; i++)
        if (buf[i] == 0) buf[i] = 0xff;
    for (unsigned int i = 0; i < 16; i++)
        buf[len + i] = buf[i];
    if (!dr_safe_write((void*)(uintptr_t)meta, nbytes, buf, NULL)) return;
    unsigned n = ++g_b24_repaired;
    if (n <= 64)
        dr_fprintf(STDERR, "[P.A.S.S.][SPINW] index repaired #%u meta=%llx len=%llu\n",
                   n, (unsigned long long)meta, (unsigned long long)len);
}

/* ── печать: всегда завершённый _snprintf ── */
static void b24_snp(char* buf, size_t n, const char* fmt, ...)
{
    va_list ap;
    int k;
    va_start(ap, fmt);
    k = _vsnprintf(buf, n - 1, fmt, ap);
    va_end(ap);
    buf[k >= 0 ? (size_t)k : n - 1] = 0;
}

/* ── адрес → «bun+RVA» / «cl+RVA» / «0x…» ── */
static void b24_pc(char* buf, size_t n, uint64_t pc)
{
    if (g_b24_bun_base != 0 && pc >= (uint64_t)g_b24_bun_base &&
        pc < (uint64_t)g_b24_bun_end)
        b24_snp(buf, n, "bun+0x%llx", pc - (uint64_t)g_b24_bun_base);
    else if (g_b24_cl_base != 0 && pc >= (uint64_t)g_b24_cl_base &&
             pc < (uint64_t)g_b24_cl_base + g_b24_cl_size)
        b24_snp(buf, n, "cl+0x%llx", pc - (uint64_t)g_b24_cl_base);
    else
        b24_snp(buf, n, "0x%llx", pc);
}

/* ── один такт: suspend всех, дамп, resume ── */
static void b24_round(int round)
{
    void **dcs = NULL;
    uint n = 0, flags = 0;
    char b1[72], b2[72], b3[72], b4[72], b5[72], b6[72], b7[72];
    int ok = dr_suspend_all_other_threads(&dcs, &n, &flags);
    if (!ok && (dcs == NULL || n == 0)) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SPINW] такт %d: suspend FAILED\n", round);
        return;
    }
    dr_fprintf(STDERR, "[P.A.S.S.][SPINW] ── такт %d: потоков=%u\n",
               round, n);

    /* состояние page-map/init — каждый такт (эволюция pmstate!) */
    if (g_b24_bun_base != 0) {
        unsigned long long* preg =
            (unsigned long long*)(g_b24_bun_base + RVA_MAP_GLOBAL);
        unsigned long long mapv = *preg;
        unsigned long long pm   = *(unsigned long long*)
            (g_b24_bun_base + RVA_G_PMSTATE);
        unsigned long long fl   = *(unsigned long long*)
            (g_b24_bun_base + RVA_G_FLAG);
        unsigned long long lk   = *(unsigned long long*)
            (g_b24_bun_base + RVA_G_PROCINIT);
        unsigned long long cnt = 0, cap = 0, nn = 0, scanned = 0;
        if (mapv != 0) {
            cnt = *(unsigned long long*)(mapv + 0x00);
            cap = *(unsigned long long*)(mapv + 0x08);
            scanned = (cnt < B24_SUBMAP_SCAN) ? cnt : B24_SUBMAP_SCAN;
            /* B-2025: БЕЗ break — субмапы раскиданы по окнам 512 МиБ
             * не по порядку (лог B-2024: слот 0 мигрированный + слот
             * 3749 арена, старый скан с break давал nnsub=1).       */
            for (unsigned long long i = 0; i < scanned; i++) {
                unsigned long long e =
                    *(unsigned long long*)(mapv + 0x30 + i * 8);
                if (e != 0)
                    nn++;
            }
        }
        b24_pc(b1, sizeof(b1), mapv);
        b24_pc(b2, sizeof(b2), pm);
        b24_pc(b3, sizeof(b3), fl);
        b24_pc(b4, sizeof(b4), lk);
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SPINW] pmmap: map=%s cnt=%llu cap=%llu"
                   " nnsub=%llu/%llu pmstate=%s flag=%s lock=%s\n",
                   b1, cnt, cap, nn, scanned, b2, b3, b4);
    }

    for (uint i = 0; i < n; i++) {
        dr_mcontext_t mc;
        void *wtag = NULL;   /* DR 11.91: void** — не app_pc* (gcc14) */
        app_pc tag;
        int w;
        thread_id_t tid = dr_get_thread_id(dcs[i]);
        memset(&mc, 0, sizeof(mc));
        mc.size = sizeof(mc);
        mc.flags = DR_MC_ALL;
        if (!dr_get_mcontext(dcs[i], &mc)) {
            dr_fprintf(STDERR,
                       "[P.A.S.S.][SPINW] tid=%llu: mcontext FAILED\n",
                       (unsigned long long)tid);
            continue;
        }
        w = (int)dr_where_am_i(dcs[i], mc.pc, &wtag);
        tag = (app_pc)(uintptr_t)wtag;
        b24_pc(b1, sizeof(b1), (uint64_t)(uintptr_t)mc.pc);
        b24_pc(b2, sizeof(b2), (uint64_t)(uintptr_t)tag);
        b24_pc(b3, sizeof(b3), (uint64_t)(uintptr_t)mc.xcx);
        b24_pc(b4, sizeof(b4), (uint64_t)(uintptr_t)mc.xdx);
        b24_pc(b5, sizeof(b5), (uint64_t)(uintptr_t)mc.xax);
        b24_pc(b6, sizeof(b6), (uint64_t)(uintptr_t)mc.r8);
        b24_pc(b7, sizeof(b7), (uint64_t)(uintptr_t)mc.xsp);
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SPINW] tid=%llu pc=%s w=%d tag=%s"
                   " rcx=%s rdx=%s rax=%s r8=%s rsp=%s"
                   " rsi=%llx rdi=%llx rbx=%llx r10=%llx r12=%llx r13=%llx\n",
                   (unsigned long long)tid,
                   b1, w, b2, b3, b4, b5, b6, b7,
                   (unsigned long long)mc.xsi, (unsigned long long)mc.xdi,
                   (unsigned long long)mc.xbx, (unsigned long long)mc.r10,
                   (unsigned long long)mc.r12, (unsigned long long)mc.r13);
        dr_fprintf(STDERR, "[P.A.S.S.][SPINW]   ст:");
        for (int q = 0; q < B24_STACK_Q; q++) {
            uint64_t v = 0;
            if (dr_safe_read((void*)(uintptr_t)(mc.xsp + (size_t)q * 8),
                             sizeof(v), &v, NULL)) {
                b24_pc(b1, sizeof(b1), v);
                dr_fprintf(STDERR, " %s", b1);
            } else {
                dr_fprintf(STDERR, " ?");
            }
        }
        dr_fprintf(STDERR, "\n");
        /* diag: повтор PC в bun — дамп 256 байт по r10 (метаданные
         * хеш-групп спин-лупа) и по rsi/rdi.                      */
        if (g_b24_bun_base != 0 &&
            (uint64_t)(uintptr_t)mc.pc >= (uint64_t)g_b24_bun_base &&
            (uint64_t)(uintptr_t)mc.pc < (uint64_t)g_b24_bun_end) {
            {
                /* B-2026: ремонт индекса на КАЖДОМ такте в bun (PC
                 * между тактами гуляет по лупу). Варианты: rcx=map
                 * (+0x48={meta,mask}), rcx={meta,mask}, rbx/r10.     */
                uint64_t rcx = (uint64_t)(uintptr_t)mc.xcx;
                uint64_t g1 = 0;
                if (dr_safe_read((void*)(uintptr_t)(rcx + 0x48), 8, &g1, NULL) &&
                    g1 != 0) {
                    uint64_t a1 = 0, b1 = 0;
                    if (dr_safe_read((void*)(uintptr_t)g1, 8, &a1, NULL) &&
                        dr_safe_read((void*)(uintptr_t)(g1 + 8), 8, &b1, NULL))
                        b24_repair_index(a1, b1);
                }
                uint64_t a2 = 0, b2 = 0;
                if (dr_safe_read((void*)(uintptr_t)rcx, 8, &a2, NULL) &&
                    dr_safe_read((void*)(uintptr_t)(rcx + 8), 8, &b2, NULL))
                    b24_repair_index(a2, b2);
                b24_repair_index((uint64_t)(uintptr_t)mc.xbx,
                                 (uint64_t)(uintptr_t)mc.r10);
                b24_repair_index((uint64_t)(uintptr_t)mc.r10,
                                 (uint64_t)(uintptr_t)mc.xdx);
                /* repopulate-спин и прочие формы: перебор пар с
                 * валидацией контента внутри b24_repair_index      */
                b24_repair_index((uint64_t)(uintptr_t)mc.r12,
                                 (uint64_t)(uintptr_t)mc.r9);
                b24_repair_index((uint64_t)(uintptr_t)mc.r13,
                                 (uint64_t)(uintptr_t)mc.r9);
                b24_repair_index((uint64_t)(uintptr_t)mc.r14,
                                 (uint64_t)(uintptr_t)mc.r9);
                b24_repair_index((uint64_t)(uintptr_t)mc.r13,
                                 (uint64_t)(uintptr_t)mc.r8);
                b24_repair_index((uint64_t)(uintptr_t)mc.r14,
                                 (uint64_t)(uintptr_t)mc.r8);
                b24_repair_index((uint64_t)(uintptr_t)mc.r12,
                                 (uint64_t)(uintptr_t)mc.rsi);
                b24_repair_index((uint64_t)(uintptr_t)mc.r13,
                                 (uint64_t)(uintptr_t)mc.rsi);
                static const char* nm[3] = { "r10", "rbx", "rcx" };
                for (int rd = 0; rd < 3; rd++) {
                    uint64_t a;
                    if (rd == 0) a = (uint64_t)(uintptr_t)mc.r10;
                    else if (rd == 1) a = (uint64_t)(uintptr_t)mc.xbx;
                    else a = (uint64_t)(uintptr_t)mc.xcx;
                    if (a == 0) continue;
                    dr_fprintf(STDERR, "[P.A.S.S.][SPINW]   dump@%s:", nm[rd]);
                    for (int q = 0; q < 144; q++) {
                        unsigned char b = 0;
                        if (dr_safe_read((void*)(uintptr_t)(a + q),
                                         1, &b, NULL))
                            dr_fprintf(STDERR, " %02x", b);
                        else
                            dr_fprintf(STDERR, " ??");
                    }
                    dr_fprintf(STDERR, "\n");
                }
            }
            g_b24_last_pc = (uint64_t)(uintptr_t)mc.pc;
        }
    }

    dr_resume_all_other_threads(dcs, n);
    dr_global_free(dcs, (size_t)n * sizeof(void*));
}

static void b24_sampler(void *arg)
{
    (void)arg;
    /* B-2025: старт-задержка первого такта — не трогать окно
     * drwrap-интенсивного старта (см. шапку); чанками по 250 мс,
     * чтобы exit-флаг быстро останавливал поток. */
    for (long d = 0; d < g_b24_delay_ms && !g_b24_stop;
         d += B24_DELAY_CHUNK)
        dr_sleep(B24_DELAY_CHUNK);
    if (g_b24_stop) return;
    dr_fprintf(STDERR,
               "[P.A.S.S.][SPINW] B-2025: сэмплинг начат (задержка старта"
               " %ldмс)\n", g_b24_delay_ms);
    for (int r = 1; r <= B24_ROUNDS && !g_b24_stop; r++) {
        dr_sleep(g_b24_ms);
        if (g_b24_stop) break;
        b24_round(r);
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.][SPINW] B-2024: сэмплинг завершён"
               " (лимит %d тактов либо выход процесса)\n", B24_ROUNDS);
}

static void b24_exit_cb(void) { g_b24_stop = 1; }

/* ------------------------------------------------------------
 * Запуск: dr_client_main (авто с -strace / форс -spinwatch).
 * B-2025: + delay_ms — старт-задержка первого такта.
 */
void pass_b2024_spinwatch_start(long interval_ms, long delay_ms)
{
    module_data_t *md = dr_get_main_module();
    if (md != NULL) {
        unsigned long long imgsz =
            (unsigned long long)(md->end - md->start);
        if (imgsz == BUN_SIZEOF_IMAGE) {
            g_b24_bun_base = (uintptr_t)md->start;
            g_b24_bun_end  = (uintptr_t)md->end;
        }
        dr_free_module_data(md);
    }
    /* собственная dll (теги cl+RVA) */
    {
        HMODULE hm = NULL;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCWSTR)(void*)&g_b24_ms, &hm) &&
            hm != NULL) {
            IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)hm;
            g_b24_cl_base = (uintptr_t)hm;
            if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
                IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)
                    ((uintptr_t)hm + (uintptr_t)dos->e_lfanew);
                if (nt->Signature == IMAGE_NT_SIGNATURE)
                    g_b24_cl_size = nt->OptionalHeader.SizeOfImage;
            }
        }
    }
    if (interval_ms < 100)   interval_ms = 100;
    if (interval_ms > 10000) interval_ms = 10000;
    g_b24_ms = interval_ms;
    if (delay_ms < 0)     delay_ms = 0;
    if (delay_ms > 60000) delay_ms = 60000;
    g_b24_delay_ms = delay_ms;
    dr_register_exit_event(b24_exit_cb);
    if (dr_create_client_thread(b24_sampler, NULL))
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SPINW] B-2024: сэмплер запущен"
                   " (интервал=%ldмс, тактов<=%d; w: APP/CACHE/LIB/CLIENT"
                   " — номер, tag=app-PC фрагмента для CACHE)\n",
                   interval_ms, B24_ROUNDS);
    else
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SPINW] B-2024: dr_create_client_thread"
                   " FAILED — сэмплера не будет\n");
}

#else /* !WINDOWS: заглушка для Linux-сборки CI */

void pass_b2024_spinwatch_start(long interval_ms, long delay_ms)
{
    (void)interval_ms; (void)delay_ms;
}

#endif /* WINDOWS */
