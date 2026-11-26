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

#define B24_ROUNDS       40            /* тактов до авто-финиша      */
#define B24_STACK_Q      10            /* qword стека на поток       */
#define B24_SUBMAP_SCAN  4096          /* субмап на скан за такт     */

static uintptr_t g_b24_bun_base = 0;   /* 0 = не bun 1.4.5 std       */
static uintptr_t g_b24_bun_end  = 0;
static uintptr_t g_b24_cl_base  = 0;   /* база самой клиентской dll  */
static unsigned long g_b24_cl_size = 0;
static volatile long g_b24_ms   = 1000;
static volatile long g_b24_stop = 0;

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
            for (unsigned long long i = 0; i < scanned; i++) {
                unsigned long long e =
                    *(unsigned long long*)(mapv + 0x30 + i * 8);
                if (e == 0) break;      /* субмапы выдаются по порядку */
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
                   " rcx=%s rdx=%s rax=%s r8=%s rsp=%s\n",
                   (unsigned long long)tid,
                   b1, w, b2, b3, b4, b5, b6, b7);
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
    }

    dr_resume_all_other_threads(dcs, n);
    dr_global_free(dcs, (size_t)n * sizeof(void*));
}

static void b24_sampler(void *arg)
{
    (void)arg;
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
 */
void pass_b2024_spinwatch_start(long interval_ms)
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

void pass_b2024_spinwatch_start(long interval_ms)
{
    (void)interval_ms;
}

#endif /* WINDOWS */
