/* ═══════════════════════════════════════════════════════════════════════
 * P.A.S.S. — B-2019 (v0.2.9): ТОЧЕЧНЫЕ ЗОНДЫ АЛЛОКАТОРА bun
 *
 * Разбор натурного лога B-2018 (стек паники + .pdata-анализ стандартной
 * сборки bun-windows-x64 1.4.5, internal name bun-profile.exe):
 *
 *   hashbrown/RawVec grow (0x5298ba0) → realloc TLS-heap (0xc440) →
 *   alloc (0x5f10) → grow-page (0x6250) → класс-страница (0x120dd70) /
 *   page-get (0x6830) → page-make (0x120db40) → seg-get (0x72a0) →
 *   bitmap find (0x89c0) → claim (0x8810: CAS + POPCNT + SETE) /
 *   scan (0x9100: TZCNT) → NULL → report (0x11f9780: «unable to
 *   allocate memory (%zu bytes)» / «allocation request is too large»)
 *   → xor eax,eax (0x66e8 — найден на стеке паники) → Rust
 *   handle_alloc_error → «memory allocation of 5085 bytes failed».
 *
 * Все NT-вызовы при этом SUCCESS — отказ ВНУТРЕННИЙ, на уровне
 * учётки страниц (bitmap claim/scan = POPCNT/TZCNT — то, что мы
 * эмулируем). Зонды фиксируют: (а) текст и аргумент report'а,
 * (б) какой уровень цепочки первым вернул NULL, (в) состояние heap
 * (cur-page/avail/bump/счётчики) в момент отказа, (г) исходы claim/scan.
 *
 * RVAs привязаны к SizeOfImage=0x6fef000 стандартной сборки 1.4.5;
 * при другой сборке зонды пропускаются с печатью size/timestamp (по
 * этому отпечатку таблица расширяется офлайн, .pdata-анализ как в B-2019).
 * ═══════════════════════════════════════════════════════════════════════ */
#include "dr_api.h"

#ifdef WINDOWS
#include "drwrap.h"
#include <string.h>
#include <stdint.h>   /* B-2020: uint64_t для extern-объявлений pass_b2020_* */

/* B-2020: bitrace + сегментная форензика (pass_b2020.c) */
extern void pass_b2020_prb(const char* name, uint64_t rcx, uint64_t rdx,
                           uint64_t r8, uint64_t r9, uint64_t xsp);
extern void pass_b2020_probe_addr(const char* name, uint64_t addr);
extern void pass_b2020_seg_dump(uint64_t heap_ptr, uint64_t seg_base,
                                uint64_t commit_addr);
static uint64_t g_b2_last_seg = 0;   /* база сегмента из зонда findpg */

/* B-2022: барьер-зонды mimalloc (pass_b2022.c): 4 ветки отказа внутри
 * mi_arenas_page_alloc_fresh + 4 входа (pagereg/submap/osalloc/oscommit).
 * Точки mid-function — бранч-таргеты и пост-коллы => начала BB DR,
 * поэтому drwrap-чистый вызов срабатывает; post-колбэк осмыслен только
 * для настоящих входов ([rsp] в середине функции — не ret-адрес). */
extern int          pass_b2022_probe_count(void);
extern const char *pass_b2022_probe_tag(int i);
extern unsigned long pass_b2022_probe_rva(int i);

/* B-2023: зонды цепочки init page-map (pass_b2023.c) — доказать,
 * где обрывается _mi_process_init -> 0x2500 под P.A.S.S. Точки
 * редкие (стартовые), pre-only; фикс карты ставится клиентом в
 * dr_client_main, зонды фиксируют поведение ШТАТНОЙ цепочки. */
extern int          pass_b2023_probe_count(void);
extern const char *pass_b2023_probe_tag(int i);
extern unsigned long pass_b2023_probe_rva(int i);
#define B3P_MAX        8
#define B3P_PRINT_CAP  64
static volatile int g_b3p_calls[B3P_MAX];
static volatile int g_b3p_printed[B3P_MAX];
#define B2P_MAX        8
#define B2P_PRINT_CAP  32   /* строк печати на барьер-зонд */
#define B2P_NULL_CAP   24   /* строк "-> NULL" на входовой зонд */
#define BUN145_TIMESTAMP 0x6afea993ull  /* TimeDateStamp bun 1.4.5 std */

typedef struct {
    int         rva;
    const char *name;
    int         nargs;   /* сколько аргументов печатать в pre */
    int         is_report;
} probe_ent_t;

/* Порядок = порядок в таблице; индексы используются в pre/post. */
static const probe_ent_t g_probe_rvas[] = {
    { 0x11f9780, "report",   3, 1 },
    { 0x6250,    "grow",     2, 0 },
    { 0x120dd70, "classpg",  2, 0 },
    { 0x6830,    "pageget",  2, 0 },
    { 0x120db40, "pagemake", 2, 0 },
    { 0x72a0,    "segget",   2, 0 },
    { 0x89c0,    "findpg",   2, 0 },
    { 0x8810,    "claim",    4, 0 },
    { 0x9100,    "scan",     4, 0 },
};
#define PROBE_N        (sizeof(g_probe_rvas) / sizeof(g_probe_rvas[0]))
#define PROBE_PRINT_CAP 48   /* строк pre-печати на зонд (горячие зонды) */
#define PROBE_NULL_CAP  24   /* строк "-> NULL" на зонд */

static volatile int g_probe_calls[PROBE_N];
static volatile int g_probe_printed[PROBE_N];
static volatile int g_probe_nulls[PROBE_N];

/* B-2022: счётчики/капы барьер-зондов (размер фикс. B2P_MAX) */
static volatile int g_b2p_calls[B2P_MAX];
static volatile int g_b2p_printed[B2P_MAX];
static volatile int g_b2p_nulls[B2P_MAX];

/* Дамп состояния heap-структуры аллокатора (см. поля из анализа):
 *  +0x410 cur page, +0x430 avail, +0x438/0x440 хвосты, +0x530 счётчик
 *  аллокаций-до-housekeeping, +0x534 счётчик, +0x538 bump-ptr (или -1). */
static void probe_heap_dump(app_pc heap)
{
    static const int offs[] = { 0x410, 0x430, 0x438, 0x440,
                                0x530, 0x534, 0x538, 0x560 };
    if (heap == NULL)
        return;
    dr_fprintf(STDERR, "[P.A.S.S.][PROBE]   heap " PIFX ":",
               (ptr_uint_t)heap);
    for (unsigned k = 0; k < sizeof(offs) / sizeof(offs[0]); k++) {
        ptr_int_t v = 0;
        if (dr_safe_read(heap + offs[k], sizeof(v), &v, NULL))
            dr_fprintf(STDERR, " +0x%x=" PIFX, offs[k], v);
    }
    dr_fprintf(STDERR, "\n");
}

static void probe_print_args(int idx, dr_mcontext_t *mc)
{
    const probe_ent_t *p = &g_probe_rvas[idx];
    if (idx == 6)   /* findpg: rcx = база сегмента (для seg-форензики) */
        g_b2_last_seg = (uint64_t)(uintptr_t)mc->rcx;
    dr_fprintf(STDERR, "[P.A.S.S.][PROBE] %s(heap=" PIFX,
               p->name, (ptr_uint_t)mc->rcx);
    if (p->is_report) {
        char fmt[120];
        fmt[0] = 0;
        if (mc->rdx != 0)
            (void)dr_safe_read((void *)(ptr_uint_t)mc->rdx, sizeof(fmt) - 1,
                               fmt, NULL);
        fmt[sizeof(fmt) - 1] = 0;
        dr_fprintf(STDERR, " lvl=" PIFX " fmt=\"%s\" a=" PIFX ")",
                   (ptr_uint_t)mc->rcx, fmt, (ptr_uint_t)mc->r8);
    } else {
        dr_fprintf(STDERR, " a1=" PIFX " a2=" PIFX,
                   (ptr_uint_t)mc->rdx, (ptr_uint_t)mc->r8);
        if (p->nargs >= 4)
            dr_fprintf(STDERR, " a3=" PIFX " a4=" PIFX,
                       (ptr_uint_t)mc->r9,
                       (ptr_uint_t)mc->xdi); /* rdi = 5-й «аргумент»-остаток */
        dr_fprintf(STDERR, ")");
    }
    dr_fprintf(STDERR, "\n");
    /* B-2020: сырые аргументы + retaddr — переотображение конвенций
     * вызовов и граф вызовов (retaddr зонда grow = место аллокации 5085). */
    pass_b2020_prb(p->name,
                   (uint64_t)(uintptr_t)mc->rcx, (uint64_t)(uintptr_t)mc->rdx,
                   (uint64_t)(uintptr_t)mc->r8,  (uint64_t)(uintptr_t)mc->r9,
                   (uint64_t)(uintptr_t)mc->xsp);
}

static void probe_pre(void *wrapcxt, void **user_data, int idx)
{
    dr_mcontext_t *mc;
    int printed;
    if (wrapcxt == NULL || idx < 0 || (unsigned)idx >= PROBE_N)
        return;
    mc = drwrap_get_mcontext(wrapcxt);
    if (mc == NULL)
        return;
    *user_data = (void *)(ptr_uint_t)mc->rcx; /* heap-указатель для post */
    g_probe_calls[idx]++;
    if (dr_atomic_add32_return_sum((volatile int *)&g_probe_printed[idx], 1)
        <= PROBE_PRINT_CAP)
        probe_print_args(idx, mc);
    (void)printed;
}

static void probe_post(void *wrapcxt, void *user_data, int idx)
{
    ptr_int_t ret;
    int nulls;
    if (wrapcxt == NULL || idx < 0 || (unsigned)idx >= PROBE_N)
        return; /* unwind-артефакт drwrap */
    ret = (ptr_int_t)(uintptr_t)drwrap_get_retval(wrapcxt);
    if (ret != 0)
        return;
    nulls = g_probe_nulls[idx];
    if (nulls >= PROBE_NULL_CAP)
        return;
    dr_atomic_add32_return_sum((volatile int *)&g_probe_nulls[idx], 1);
    dr_fprintf(STDERR, "[P.A.S.S.][PROBE] %s -> NULL (вызов #%d, отказ #%d)\n",
               g_probe_rvas[idx].name, g_probe_calls[idx], nulls + 1);
    probe_heap_dump((app_pc)(ptr_uint_t)user_data);
    /* B-2020: сегментная форензика на первом отказе segget —
     * состояние битовой карты страниц/заголовков сегмента аллокатора. */
    if (idx == 5)
        pass_b2020_seg_dump((uint64_t)(uintptr_t)user_data,
                            g_b2_last_seg, 0);
}

/* Генерация пар cb на каждый зонд (drwrap не сообщает индекс зонда). */
#define PROBE_CBS(i, NAME)                                              \
    static void pre_##NAME(void *w, void **ud)  { probe_pre(w, ud, (i)); } \
    static void post_##NAME(void *w, void *ud)  { probe_post(w, ud, (i)); }
PROBE_CBS(0, report)
PROBE_CBS(1, grow)
PROBE_CBS(2, classpg)
PROBE_CBS(3, pageget)
PROBE_CBS(4, pagemake)
PROBE_CBS(5, segget)
PROBE_CBS(6, findpg)
PROBE_CBS(7, claim)
PROBE_CBS(8, scan)
typedef void (*pre_cb_t)(void *, void **);
typedef void (*post_cb_t)(void *, void *);
static const pre_cb_t g_pre[PROBE_N] = {
    pre_report, pre_grow, pre_classpg,
    pre_pageget, pre_pagemake, pre_segget,
    pre_findpg, pre_claim, pre_scan };
static const post_cb_t g_post[PROBE_N] = {
    post_report, post_grow, post_classpg,
    post_pageget, post_pagemake, post_segget,
    post_findpg, post_claim, post_scan };

/* B-2022: pre-колбэк барьер-зонда — печать rcx/rdx/r8/r9 + PRB-rawargs.
 * pass_b2020_prb под тем же капом печати (иначе горячие osalloc/oscommit
 * зальют и лог, и следующий вывод).
 * ret для mid-function точек = [rsp] (верх стека, НЕ ret-адрес) — при
 * разборе лога учитывать; для *-enter — настоящий ret-адрес.        */
static void b2p_pre(void *wrapcxt, void **user_data, int idx)
{
    dr_mcontext_t *mc;
    if (wrapcxt == NULL || idx < 0 || idx >= B2P_MAX)
        return;
    mc = drwrap_get_mcontext(wrapcxt);
    if (mc == NULL)
        return;
    *user_data = (void *)(ptr_uint_t)mc->rcx;
    if (dr_atomic_add32_return_sum((volatile int *)&g_b2p_printed[idx], 1)
        <= B2P_PRINT_CAP) {
        const char *tag = pass_b2022_probe_tag(idx);
        dr_fprintf(STDERR,
                   "[P.A.S.S.][PROBE2] %s(rcx=" PIFX " rdx=" PIFX
                   " r8=" PIFX " r9=" PIFX ")\n",
                   tag,
                   (ptr_uint_t)mc->rcx, (ptr_uint_t)mc->rdx,
                   (ptr_uint_t)mc->r8,  (ptr_uint_t)mc->r9);
        pass_b2020_prb(tag,
                       (uint64_t)(uintptr_t)mc->rcx,
                       (uint64_t)(uintptr_t)mc->rdx,
                       (uint64_t)(uintptr_t)mc->r8,
                       (uint64_t)(uintptr_t)mc->r9,
                       (uint64_t)(uintptr_t)mc->xsp);
    }
}

/* B-2022: post только для *-enter-зондов: ret==0 (NULL/false) —
 * решающий сигнал (osalloc/oscommit/pagereg/submap отвергли). */
static void b2p_post(void *wrapcxt, void *user_data, int idx)
{
    ptr_int_t ret;
    int nulls;
    if (wrapcxt == NULL || idx < 0 || idx >= B2P_MAX)
        return;
    ret = (ptr_int_t)(uintptr_t)drwrap_get_retval(wrapcxt);
    if (ret != 0)
        return;
    nulls = g_b2p_nulls[idx];
    if (nulls >= B2P_NULL_CAP)
        return;
    dr_atomic_add32_return_sum((volatile int *)&g_b2p_nulls[idx], 1);
    dr_fprintf(STDERR,
               "[P.A.S.S.][PROBE2] %s -> NULL/false (вызов #%d, отказ #%d)\n",
               pass_b2022_probe_tag(idx),
               g_b2p_calls[idx], nulls + 1);
}

static void b2p_count(void *wrapcxt, void **user_data, int idx)
{
    if (wrapcxt != NULL && idx >= 0 && idx < B2P_MAX)
        g_b2p_calls[idx]++;
    (void)user_data;
}

/* Генерация пар cb на каждый барьер-зонд (drwrap не сообщает индекс). */
#define B2P_CBS(i)                                                  \
    static void pre2_##i(void *w, void **ud)                       \
    { b2p_count(w, ud, (i)); b2p_pre(w, ud, (i)); }                \
    static void post2_##i(void *w, void *ud) { b2p_post(w, ud, (i)); }
B2P_CBS(0)
B2P_CBS(1)
B2P_CBS(2)
B2P_CBS(3)
B2P_CBS(4)
B2P_CBS(5)
B2P_CBS(6)
B2P_CBS(7)
static const pre_cb_t g_pre2[B2P_MAX] = {
    pre2_0, pre2_1, pre2_2, pre2_3,
    pre2_4, pre2_5, pre2_6, pre2_7 };
static const post_cb_t g_post2[B2P_MAX] = {
    post2_0, post2_1, post2_2, post2_3,
    post2_4, post2_5, post2_6, post2_7 };

/* B-2023: pre-колбэк зондов init-цепочки (печать rcx/rdx/r8/r9). */
static void b3p_pre(void *wrapcxt, void **user_data, int idx)
{
    dr_mcontext_t *mc;
    if (wrapcxt == NULL || idx < 0 || idx >= B3P_MAX)
        return;
    mc = drwrap_get_mcontext(wrapcxt);
    if (mc == NULL)
        return;
    *user_data = (void *)(ptr_uint_t)mc->rcx;
    (void)dr_atomic_add32_return_sum((volatile int *)&g_b3p_calls[idx], 1);
    if (dr_atomic_add32_return_sum((volatile int *)&g_b3p_printed[idx], 1)
        <= B3P_PRINT_CAP) {
        const char *tag = pass_b2023_probe_tag(idx);
        dr_fprintf(STDERR,
                   "[P.A.S.S.][PROBE3] %s(rcx=" PIFX " rdx=" PIFX
                   " r8=" PIFX " r9=" PIFX ")\n",
                   tag,
                   (ptr_uint_t)mc->rcx, (ptr_uint_t)mc->rdx,
                   (ptr_uint_t)mc->r8,  (ptr_uint_t)mc->r9);
        pass_b2020_prb(tag,
                       (uint64_t)(uintptr_t)mc->rcx,
                       (uint64_t)(uintptr_t)mc->rdx,
                       (uint64_t)(uintptr_t)mc->r8,
                       (uint64_t)(uintptr_t)mc->r9,
                       (uint64_t)(uintptr_t)mc->xsp);
    }
}

#define B3P_CBS(i)                                                  \
    static void pre3_##i(void *w, void **ud) { b3p_pre(w, ud, (i)); }
B3P_CBS(0)
B3P_CBS(1)
B3P_CBS(2)
B3P_CBS(3)
B3P_CBS(4)
B3P_CBS(5)
B3P_CBS(6)
B3P_CBS(7)
static const pre_cb_t g_pre3[B3P_MAX] = {
    pre3_0, pre3_1, pre3_2, pre3_3,
    pre3_4, pre3_5, pre3_6, pre3_7 };

/* Отпечаток сборки: SizeOfImage стандартной bun-windows-x64 1.4.5. */
#define BUN145_SIZEOF_IMAGE 0x6fef000ull

static void probe_install_for(module_data_t *md)
{
    const char *nm = dr_module_preferred_name(md);
    unsigned long long imgsz = (unsigned long long)(md->end - md->start);
    unsigned ts = 0, n_ok = 0;
    /* PE TimeDateStamp: DOS e_lfanew по смещению 0x3c → NT+8 */
    int e_lfanew = 0;
    if (dr_safe_read((byte *)md->start + 0x3c, sizeof(e_lfanew),
                     &e_lfanew, NULL) && e_lfanew > 0 && e_lfanew < 0x400)
        (void)dr_safe_read((byte *)md->start + e_lfanew + 8, sizeof(ts),
                           &ts, NULL);
    dr_fprintf(STDERR,
               "[P.A.S.S.] probe-alloc: модуль %s size=0x%llx ts=0x%x"
               " (ожидается 0x%llx для bun 1.4.5 std)\n",
               nm != NULL ? nm : "?", imgsz, ts, BUN145_SIZEOF_IMAGE);
    if (imgsz != BUN145_SIZEOF_IMAGE) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.] probe-alloc: SKIPPED — другая сборка bun,"
                   " таблица RVA невалидна (пришли лог — соберу таблицу)\n");
        return;
    }
    for (unsigned k = 0; k < PROBE_N; k++) {
        app_pc fn = md->start + g_probe_rvas[k].rva;
        pass_b2020_probe_addr(g_probe_rvas[k].name,
                              (uint64_t)(uintptr_t)fn);   /* B-2020: code-дамп */
        if (drwrap_wrap(fn, g_pre[k], g_post[k]))
            n_ok++;
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.] probe-alloc: %u/%u зондов установлено (B-2019)\n",
               n_ok, (unsigned)PROBE_N);

    /* B-2022: 8 барьер-зондов mimalloc — только при совпадении И
     * SizeOfImage, И TimeDateStamp: точки сидят ВНУТРИ функций
     * (бранч-таргеты/пост-коллы), привязка жёстче входовой.
     * B-2025: -noprobe2 — бисекция краша drwrap.dll+0x5534. */
    {
        int cnt = pass_b2022_probe_count();
        unsigned n2 = 0, n2_ok = 0;
        extern int g_no_probe2;
        if (g_no_probe2) {
            dr_fprintf(STDERR,
                       "[P.A.S.S.] probe-alloc: барьер-зонды B-2022 SKIPPED"
                       " (-noprobe2, бисекция B-2025)\n");
        } else if (ts == (unsigned)BUN145_TIMESTAMP && cnt > 0 && cnt <= B2P_MAX) {
            for (int k = 0; k < cnt; k++) {
                app_pc fn = md->start + (ptr_uint_t)pass_b2022_probe_rva(k);
                const char *tag = pass_b2022_probe_tag(k);
                bool is_entry = strstr(tag, "-enter") != NULL;
                pass_b2020_probe_addr(tag, (uint64_t)(uintptr_t)fn);
                if (drwrap_wrap(fn, g_pre2[k], is_entry ? g_post2[k] : NULL))
                    n2_ok++;
                n2++;
            }
        } else if (cnt > 0) {
            dr_fprintf(STDERR,
                       "[P.A.S.S.] probe-alloc: барьер-зонды B-2022 SKIPPED"
                       " — TimeDateStamp 0x%x != 0x%llx\n",
                       ts, BUN145_TIMESTAMP);
        }
        if (n2 > 0)
            dr_fprintf(STDERR,
                       "[P.A.S.S.] probe-alloc: %u/%u барьер-зондов mimalloc"
                       " установлено (B-2022)\n", n2_ok, n2);
    }

    /* B-2023: 8 зондов init-цепочки page-map (procinit -> pm-gate ->
     * pm-call -> pminit -> claim/lose/alloc/done). Решающие сигналы:
     * procinit НЕ сработал = _mi_process_init не запускался;
     * pminit-lose = cmpxchg state проигран (тихий выход БЕЗ карты);
     * pminit-alloc = аллокация карты достигнута (далее osalloc).
     * B-2025: -noprobe3 — бисекция краша drwrap.dll+0x5534. */
    {
        int cnt3 = pass_b2023_probe_count();
        unsigned n3 = 0, n3_ok = 0;
        extern int g_no_probe3;
        if (g_no_probe3) {
            dr_fprintf(STDERR,
                       "[P.A.S.S.] probe-alloc: init-зонды B-2023 SKIPPED"
                       " (-noprobe3, бисекция B-2025)\n");
        } else if (ts == (unsigned)BUN145_TIMESTAMP && cnt3 > 0 && cnt3 <= B3P_MAX) {
            for (int k = 0; k < cnt3; k++) {
                app_pc fn = md->start + (ptr_uint_t)pass_b2023_probe_rva(k);
                const char *tag = pass_b2023_probe_tag(k);
                pass_b2020_probe_addr(tag, (uint64_t)(uintptr_t)fn);
                if (drwrap_wrap(fn, g_pre3[k], NULL))
                    n3_ok++;
                n3++;
            }
        }
        if (n3 > 0)
            dr_fprintf(STDERR,
                       "[P.A.S.S.] probe-alloc: %u/%u зондов init-цепочки"
                       " page-map установлено (B-2023)\n", n3_ok, n3);
    }
}

void pass_probe_install(void)
{
    module_data_t *md;
    extern int pass_drwrap_ensure(void); /* gcc14: не полагаемся на pass.h */
    if (!pass_drwrap_ensure())
        return;
    md = dr_get_main_module();
    if (md == NULL)
        return;
    probe_install_for(md);
    dr_free_module_data(md);
}

#else /* LINUX: зонды bun/Windows бессмысленны — тело пустое */
void pass_probe_install(void) { }
#endif /* WINDOWS */
