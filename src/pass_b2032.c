/* ============================================================
 * P.A.S.S. — pass_b2032.c (B-2032/B-2033, v0.3.7-diag)
 * ------------------------------------------------------------
 * B-2032: SCRING + HANGWATCH — диагностика ЗАБЛОКИРОВАННОГО
 * процесса (OpenCode/bun 1.4.4, новая проблема №1 после
 * pagemap-фикса B-2031: OOM снят, но процесс стоит — CPU не
 * занят (НЕ спин B-2024), WS ~35 МБ, `opencode --version`
 * молчит; гипотезы: блокирующий syscall (сеть/обновление),
 * дедлок VEH, путь 1.4.4).
 *
 * ЧТО НЕ ХВАТАЛО SPINWATCH (B-2024): он печатает КАЖДЫЙ такт
 * независимо от состояния (шум) и не знает, какой syscall был
 * последним у каждого потока. Здесь:
 *   * SCRING — пер-поточное кольцо последних syscall'ов
 *     (pre/post фазы, аргументы, результат). Пишется ТОЛЬКО
 *     когда hangwatch активен (-hangwatch / авто с -strace) —
 *     нулевой оверхед в остальных конфигурациях;
 *   * HANGWATCH — поток-наблюдатель: если глобальный счётчик
 *     syscall-активности стоит дольше порога (стоп = блок в
 *     syscall'е ЛИБО user-mode спин без syscall'ов) — такт
 *     дампа: suspend всех, app-PC каждого потока + where_am_i
 *     + стек, кольцо SCRING (с пометкой pre-без-post =
 *     кандидат блокирующего вызова), статистика EXCSTAT, resume.
 *
 * Вместе это закрывает пункт (а) плана «что дальше» из коммита
 * B-2028..B-2031: «снять последние сисколлы через -strace» —
 * теперь видно даже без -strace (печать только по факту
 * зависания) и без килобайтов шума трейсера.
 *
 * B-2033: EXCSTAT — статистика исключений по КОДАМ и САЙТАМ
 * (Blender 5.2.5: CPU-проверка пройдена (IFP-спуф B-2030),
 * «Blender quit» без диалога, но уходит в цикл исключений
 * после MESA/ZINK vkCreateInstance failed + 0x406d1388).
 * Считает ВСЕГДА (гейт -noexcstats): AV на guard-страницах JIT
 * — норма (B-2016), единичные diag-коды печатает trap.c; цикл
 * же видно по прогресс-строке каждого 1000-го исключения
 * кода + полному дампу в итогах/на hangwatch-такте.
 *
 * Потокобезопасность: слот SCRING закрепляется за потоком
 * атомарно (PASS_ATOMIC_INC32), пишет только владелец; dump
 * идёт под dr_suspend_all_other_threads. TLS-поле выделяется
 * лениво (первый syscall потока).
 *
 * АДРЕСНАЯ АРИФМЕТИКА: печать модулей через dr_lookup_module
 * (универсально; bun-специфика SPINW не нужна — OpenCode =
 * другой образ).
 * ============================================================ */
#include "pass.h"
#include <stdlib.h>

/* ───────────────────────── SCRING ───────────────────────── */
#define SCRING_SLOTS 96      /* потоков с кольцами */
#define SCRING_DEPTH 8       /* записей на поток   */

typedef struct {
    uint64_t   seq;          /* глобальный seq записи (0 = пусто) */
    int        sysnum;
    int        phase;        /* 0 = pre, 1 = post */
    ptr_int_t  a0, a1, a2;   /* pre: аргументы; post: a0 = результат */
} scrent_t;

typedef struct {
    thread_id_t tid;
    unsigned    idx;         /* кольцевой индекс (запись №idx) */
    scrent_t    e[SCRING_DEPTH];
} scslot_t;

static scslot_t       g_scring[SCRING_SLOTS];
static volatile long  g_scring_next = 0;  /* следующий свободный слот */
static volatile uint64_t g_scring_seq;    /* счётчик pre-записей = активность */
static volatile long  g_scring_on = 0;    /* кольцо активно? (гейт оверхеда) */

/* TLS-индекс слота (extern в pass.h, регистрируется в pass.c
 * рядом с g_tls_idx_ring — конвенция B-2019) */
int g_tls_idx_scring = -1;

static scslot_t *sc_slot(void *drcontext)
{
    scslot_t *s;
    if (drcontext == NULL)
        return NULL;
    s = (scslot_t *)drmgr_get_tls_field(drcontext, g_tls_idx_scring);
    if (s == NULL) {
        long slot = PASS_ATOMIC_INC32(&g_scring_next) /* возвращает старое */;
        if (slot < 0 || slot >= SCRING_SLOTS)
            return NULL;         /* слоты кончились — кольцо этому потоку
                                  * не пишется (diag деградирует, не роняя) */
        s = &g_scring[slot];
        s->tid = dr_get_thread_id(drcontext);
        s->idx = 0;
        drmgr_set_tls_field(drcontext, g_tls_idx_scring, s);
    }
    return s;
}

void pass_scring_note_pre(void *drcontext, int sysnum,
                          ptr_int_t a0, ptr_int_t a1, ptr_int_t a2)
{
    scslot_t *s;
    if (!g_scring_on)
        return;
    s = sc_slot(drcontext);
    if (s == NULL)
        return;
    {
        uint64_t seq = ++g_scring_seq;
        scrent_t *e = &s->e[s->idx % SCRING_DEPTH];
        e->seq = seq; e->sysnum = sysnum; e->phase = 0;
        e->a0 = a0; e->a1 = a1; e->a2 = a2;
        s->idx++;
    }
}

void pass_scring_note_post(void *drcontext, int sysnum, ptr_int_t res)
{
    scslot_t *s;
    if (!g_scring_on)
        return;
    s = sc_slot(drcontext);
    if (s == NULL)
        return;
    {
        /* post пишется в СЛЕДУЮЩУЮ запись (не поверх pre): в дампе
         * видно пару «вошёл → вернулся»; если post нет — поток сидит
         * в syscall'е (кандидат блокировки). */
        uint64_t seq = ++g_scring_seq;
        scrent_t *e = &s->e[s->idx % SCRING_DEPTH];
        e->seq = seq; e->sysnum = sysnum; e->phase = 1;
        e->a0 = res;
        s->idx++;
    }
}

uint64_t pass_scring_seq(void) { return g_scring_seq; }

/* печать одного адреса как module+RVA / 0x… (по образцу
 * pass_diag_stack_scan — strace.c) */
static void b32_pc(char *buf, size_t n, uint64_t pc)
{
    module_data_t *md = (pc >= 0x10000) ? dr_lookup_module((app_pc)(uintptr_t)pc)
                                        : NULL;
    if (md != NULL) {
        const char *nm = dr_module_preferred_name(md);
        dr_snprintf(buf, n, "%s+0x%llx",
                    nm != NULL ? nm : "?",
                    (unsigned long long)(pc - (uint64_t)(uintptr_t)md->start));
        dr_free_module_data(md);
    } else {
        dr_snprintf(buf, n, "0x%llx", (unsigned long long)pc);
    }
}

/* Имена NT-сисколлов (Windows) — общий доступ к таблице strace.c */
#ifdef WINDOWS
extern const char *pass_nt_name(int sysnum);
#endif

static const char *sc_phase_str(int phase)
{
    return phase == 0 ? "pre" : "ret";
}

void pass_scring_dump(void)
{
    long nslots = g_scring_next;
    if (nslots > SCRING_SLOTS) nslots = SCRING_SLOTS;
    dr_fprintf(STDERR,
               "[P.A.S.S.][SCRING] ── последние syscall'ы по потокам"
               " (слот=нить, свежие внизу; 'pre' без 'ret' = сидит в"
               " syscall'е):\n");
    for (long k = 0; k < nslots; k++) {
        scslot_t *s = &g_scring[k];
        unsigned start, cnt, i;
        if (s->tid == 0)
            continue;
        cnt = s->idx < SCRING_DEPTH ? s->idx : SCRING_DEPTH;
        start = s->idx - cnt;
        dr_fprintf(STDERR, "[P.A.S.S.][SCRING] tid=%llu (%u записей):\n",
                   (unsigned long long)s->tid, cnt);
        for (i = 0; i < cnt; i++) {
            scrent_t *e = &s->e[(start + i) % SCRING_DEPTH];
#ifdef WINDOWS
            const char *nm = pass_nt_name(e->sysnum);
            if (nm != NULL)
                dr_fprintf(STDERR,
                           "  [%llu] %-3s SC %d %s %p %p %p\n",
                           (unsigned long long)e->seq, sc_phase_str(e->phase),
                           e->sysnum, nm,
                           (void *)(ptr_uint_t)e->a0,
                           (void *)(ptr_uint_t)e->a1,
                           (void *)(ptr_uint_t)e->a2);
            else
#endif
                dr_fprintf(STDERR,
                           "  [%llu] %-3s SC %d %p %p %p\n",
                           (unsigned long long)e->seq, sc_phase_str(e->phase),
                           e->sysnum,
                           (void *)(ptr_uint_t)e->a0,
                           (void *)(ptr_uint_t)e->a1,
                           (void *)(ptr_uint_t)e->a2);
        }
        /* маркер незакрытого pre */
        if (cnt > 0) {
            scrent_t *last = &s->e[(s->idx - 1) % SCRING_DEPTH];
            if (last->phase == 0)
                dr_fprintf(STDERR,
                           "  >>> tid=%llu: последний pre БЕЗ возврата —"
                           " вероятный БЛОКИРУЮЩИЙ syscall (см. выше)\n",
                           (unsigned long long)s->tid);
        }
    }
    if (nslots == 0)
        dr_fprintf(STDERR,
                   "[P.A.S.S.][SCRING] записей нет (кольцо не активно или"
                   " сисколлов не было)\n");
}

/* ───────────────────────── EXCSTAT (B-2033) ───────────────────────── */
#define EXCSTAT_CODES 24
#define EXCSTAT_SITES 24

typedef struct {
    unsigned   code;
    uint64_t   n;
} exccode_t;
typedef struct {
    unsigned   code;
    app_pc     site;
    uint64_t   n;
} excsite_t;

static exccode_t g_exc_codes[EXCSTAT_CODES];
static excsite_t g_exc_sites[EXCSTAT_SITES];
static uint64_t  g_exc_total;
static uint64_t  g_exc_dropped;    /* сайты мимо таблицы */

/* -noexcstats: extern из pass.h, определение в pass.c (конвенция B-2025) */

static const char *exc_code_name(unsigned code)
{
    switch (code) {
    case 0xC0000005u: return "access-violation";
    case 0xC000001Du: return "illegal-instruction(#UD)";
    case 0xC00000FDu: return "stack-overflow";
    case 0xC0000409u: return "fast-fail";
    case 0xC000041Du: return "user-callback-exc";
    case 0x80000003u: return "breakpoint";
    case 0xE06D7363u: return "C++-EH";
    case 0x406D1388u: return "thread-name";
    case 0x40010006u: return "ODS-A";
    case 0x4001000Au: return "ODS-W";
#ifdef UNIX
    case 4:  return "SIGILL";
    case 7:  return "SIGBUS";
    case 8:  return "SIGFPE";
    case 11: return "SIGSEGV";
    case 6:  return "SIGABRT";
#endif
    default:   return NULL;
    }
}

void pass_excstat_note(unsigned code, app_pc site)
{
    exccode_t *c = NULL;
    excsite_t *s = NULL;
    int k;
    if (g_no_excstats)
        return;
    g_exc_total++;
    for (k = 0; k < EXCSTAT_CODES; k++) {
        if (g_exc_codes[k].code == code) { c = &g_exc_codes[k]; break; }
        if (g_exc_codes[k].code == 0 && c == NULL) c = &g_exc_codes[k];
    }
    if (c != NULL) {
        if (c->code == 0) c->code = code;
        c->n++;
        /* прогресс-строка: каждое 1000-е исключение кода — живой
         * счётчик при цикле (Blender 5.2.5). Первое 1000-е и далее. */
        if (c->n % 1000 == 0) {
            char sb[96];
            b32_pc(sb, sizeof(sb), (uint64_t)(uintptr_t)site);
            dr_fprintf(STDERR,
                       "[P.A.S.S.][EXCSTAT] code 0x%08x (%s): уже %llu"
                       " исключений — сайт %s — похоже на ЦИКЛ\n",
                       code,
                       exc_code_name(code) != NULL ? exc_code_name(code) : "?",
                       (unsigned long long)c->n, sb);
        }
    }
    for (k = 0; k < EXCSTAT_SITES; k++) {
        if (g_exc_sites[k].code == code && g_exc_sites[k].site == site) {
            s = &g_exc_sites[k]; break;
        }
        if (g_exc_sites[k].code == 0 && s == NULL) s = &g_exc_sites[k];
    }
    if (s != NULL) {
        if (s->code == 0) { s->code = code; s->site = site; }
        s->n++;
    } else {
        g_exc_dropped++;
    }
}

static void excstat_dump_one(unsigned code, uint64_t n)
{
    const char *nm = exc_code_name(code);
    dr_fprintf(STDERR, "  0x%08x %-22s %10llu\n", code,
               nm != NULL ? nm : "", (unsigned long long)n);
}

void pass_excstat_dump(void)
{
    int k, any = 0;
    if (g_no_excstats || g_exc_total == 0)
        return;
    dr_fprintf(STDERR,
               "[P.A.S.S.][EXCSTAT] ── исключения по кодам (всего %llu):\n",
               (unsigned long long)g_exc_total);
    for (k = 0; k < EXCSTAT_CODES; k++)
        if (g_exc_codes[k].code != 0) { excstat_dump_one(g_exc_codes[k].code, g_exc_codes[k].n); any = 1; }
    if (!any)
        dr_fprintf(STDERR, "  (таблица пуста)\n");
    dr_fprintf(STDERR,
               "[P.A.S.S.][EXCSTAT] ── топ-сайты (code × app-PC × count):\n");
    any = 0;
    for (k = 0; k < EXCSTAT_SITES; k++) {
        char b[96];
        if (g_exc_sites[k].code == 0) continue;
        b32_pc(b, sizeof(b), (uint64_t)(uintptr_t)g_exc_sites[k].site);
        dr_fprintf(STDERR, "  0x%08x %-18s %10llu\n",
                   g_exc_sites[k].code, b,
                   (unsigned long long)g_exc_sites[k].n);
        any = 1;
    }
    if (!any)
        dr_fprintf(STDERR, "  (таблица пуста)\n");
    if (g_exc_dropped > 0)
        dr_fprintf(STDERR,
                   "  (сайтов мимо таблицы: %llu)\n",
                   (unsigned long long)g_exc_dropped);
}

/* ───────────────────────── HANGWATCH (B-2032) ───────────────────────── */
#define HW_ROUNDS      40      /* тактов дампа до сдачи */
#define HW_POLL_MS     500     /* период опроса активности */
#define HW_STACK_Q     8       /* qword стека на поток */

static volatile long g_hw_stall_ms = 8000;   /* порог тишины */
static volatile long g_hw_delay_ms = 15000;  /* старт-задержка */
static volatile long g_hw_stop = 0;

static void hw_round(int round, uint64_t seq_at_trigger)
{
    void **dcs = NULL;
    uint n = 0, flags = 0;
    int ok = dr_suspend_all_other_threads(&dcs, &n, &flags);
    if (!ok && (dcs == NULL || n == 0)) {
        dr_fprintf(STDERR,
                   "[P.A.S.S.][HANGW] такт %d: suspend FAILED\n", round);
        return;
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.][HANGW] ══ такт %d: syscall-активность СТОЯЛА"
               " (seq=%llu), потоков=%u — дамп состояния:\n",
               round, (unsigned long long)seq_at_trigger, n);
    for (uint i = 0; i < n; i++) {
        dr_mcontext_t mc;
        void *wtag = NULL;
        app_pc tag;
        int w;
        thread_id_t tid = dr_get_thread_id(dcs[i]);
        char b1[96], b2[96];
        memset(&mc, 0, sizeof(mc));
        mc.size = sizeof(mc);
        mc.flags = DR_MC_INTEGER | DR_MC_CONTROL;
        if (!dr_get_mcontext(dcs[i], &mc)) {
            dr_fprintf(STDERR,
                       "[P.A.S.S.][HANGW] tid=%llu: mcontext FAILED\n",
                       (unsigned long long)tid);
            continue;
        }
        w = (int)dr_where_am_i(dcs[i], mc.pc, &wtag);
        tag = (app_pc)(uintptr_t)wtag;
        b32_pc(b1, sizeof(b1), (uint64_t)(uintptr_t)mc.pc);
        b32_pc(b2, sizeof(b2), (uint64_t)(uintptr_t)tag);
        dr_fprintf(STDERR,
                   "[P.A.S.S.][HANGW] tid=%llu pc=%s w=%d tag=%s"
                   " rsp=%llx rax=%llx rcx=%llx rdx=%llx\n",
                   (unsigned long long)tid, b1, w, b2,
                   (unsigned long long)mc.xsp,
                   (unsigned long long)mc.xax,
                   (unsigned long long)mc.xcx,
                   (unsigned long long)mc.xdx);
        dr_fprintf(STDERR, "[P.A.S.S.][HANGW]   ст:");
        for (int q = 0; q < HW_STACK_Q; q++) {
            uint64_t v = 0;
            if (dr_safe_read((void *)(uintptr_t)(mc.xsp + (size_t)q * 8),
                             sizeof(v), &v, NULL)) {
                b32_pc(b1, sizeof(b1), v);
                dr_fprintf(STDERR, " %s", b1);
            } else {
                dr_fprintf(STDERR, " ?");
            }
        }
        dr_fprintf(STDERR, "\n");
    }
    dr_resume_all_other_threads(dcs, n);
    dr_global_free(dcs, (size_t)n * sizeof(void *));
    /* кольцо и статистика — под suspend уже НЕ нужны (свои данные) */
    pass_scring_dump();
    pass_excstat_dump();
}

static void hw_thread(void *arg)
{
    uint64_t last_seq = pass_scring_seq();
    uint64_t seq_now;
    long quiet_ms = 0;
    int round = 0;
    long delay = g_hw_delay_ms;
    (void)arg;
    for (long d = 0; d < delay && !g_hw_stop; d += HW_POLL_MS)
        dr_sleep(HW_POLL_MS);
    if (g_hw_stop) return;
    dr_fprintf(STDERR,
               "[P.A.S.S.][HANGW] B-2032: наблюдатель запущен (порог"
               " тишины %ld мс, задержка старта %ld мс, тактов<=%d).\n"
               "[P.A.S.S.][HANGW] стоп активности = блок в syscall'е ЛИБО"
               " user-mode спин без syscall'ов — в обоих случаях дамп.\n",
               g_hw_stall_ms, g_hw_delay_ms, HW_ROUNDS);
    while (!g_hw_stop && round < HW_ROUNDS) {
        dr_sleep(HW_POLL_MS);
        if (g_hw_stop) break;
        seq_now = pass_scring_seq();
        if (seq_now != last_seq) {
            last_seq = seq_now;
            quiet_ms = 0;
            continue;
        }
        quiet_ms += HW_POLL_MS;
        if (quiet_ms >= g_hw_stall_ms) {
            round++;
            hw_round(round, seq_now);
            /* после дампа — пауза вдвое больше порога, чтобы лог не
             * заливался одинаковыми тактами (активность не проверяем
             * между тактами: если процесс ожил, следующий опрос это
             * покажет и дампов больше не будет) */
            quiet_ms = -(g_hw_stall_ms);   /* следующий дамп через 2×порог */
        }
    }
    dr_fprintf(STDERR,
               "[P.A.S.S.][HANGW] B-2032: наблюдатель завершён"
               " (лимит %d тактов либо выход процесса)\n", HW_ROUNDS);
}

static void hw_exit_cb(void) { g_hw_stop = 1; }

void pass_b2032_hangwatch_start(long stall_ms, long delay_ms)
{
    if (stall_ms < 1000)   stall_ms = 1000;
    if (stall_ms > 600000) stall_ms = 600000;
    if (delay_ms < 0)      delay_ms = 0;
    if (delay_ms > 600000) delay_ms = 600000;
    g_hw_stall_ms = stall_ms;
    g_hw_delay_ms = delay_ms;
    g_scring_on = 1;      /* включаем кольцо (гейт оверхеда) */
    drmgr_register_exit_event(hw_exit_cb);   /* dr_api.h: прямой
                                  * dr_register_exit_event перехвачен
                                  * deprecated-макросом drmgr.h */
    if (dr_create_client_thread(hw_thread, NULL))
        dr_fprintf(STDERR,
                   "[P.A.S.S.][HANGW] B-2032: поток-наблюдатель запущен"
                   " (stall=%ldмс, delay=%ldмс; SCRING включён)\n",
                   stall_ms, delay_ms);
    else
        dr_fprintf(STDERR,
                   "[P.A.S.S.][HANGW] B-2032: dr_create_client_thread"
                   " FAILED — наблюдателя не будет\n");
}

uint64_t pass_excstat_total(void) { return g_exc_total; }
