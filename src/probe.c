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
    ret = drwrap_get_retval(wrapcxt);
    if (ret != 0)
        return;
    nulls = g_probe_nulls[idx];
    if (nulls >= PROBE_NULL_CAP)
        return;
    dr_atomic_add32_return_sum((volatile int *)&g_probe_nulls[idx], 1);
    dr_fprintf(STDERR, "[P.A.S.S.][PROBE] %s -> NULL (вызов #%d, отказ #%d)\n",
               g_probe_rvas[idx].name, g_probe_calls[idx], nulls + 1);
    probe_heap_dump((app_pc)(ptr_uint_t)user_data);
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
        if (drwrap_wrap(fn, g_pre[k], g_post[k]))
            n_ok++;
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.] probe-alloc: %u/%u зондов установлено (B-2019)\n",
               n_ok, (unsigned)PROBE_N);
}

void pass_probe_install(void)
{
    module_data_t *md;
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
