/* P.A.S.S. — pass.c
 * Точка входа клиента DynamoRIO: разбор опций, инициализация
 * TLS, профилей, событий; сводная статистика при выходе.        */
#include "pass.h"
#include <stdlib.h>

/* ── профиль → human-readable сводка ── */
static void print_banner(void)
{
    dr_fprintf(STDERR,
        "\n"
        "  ╔══════════════════════════════════════════════╗\n"
        "  ║  P.A.S.S. — Performance Adaptive SIMD        ║\n"
        "  ║              Substitution  v0.2.9            ║\n"
        "  ╚══════════════════════════════════════════════╝\n"
        "  target=%s  expose_emulated=%d  inline=%d generic=%d trap=%d\n\n",
        g_cfg.target_name, g_cfg.expose_emulated, g_cfg.enable_inline,
        g_cfg.enable_generic, g_cfg.enable_trap);
}

static void parse_options(int argc, const char *argv[])
{
    /* argv[0] — путь клиента, дальше ключи */
    g_cfg.target_name = "core2";
    g_cfg.expose_emulated = 1;
    g_cfg.enable_inline = 1;
    g_cfg.enable_generic = 1;
    g_cfg.enable_trap = 1;
    g_cfg.verbose = 0;
    g_strace = 0;
    for (int k = 1; k < argc; k++) {
        const char *a = argv[k];
        if (strcmp(a, "-target") == 0 && k + 1 < argc) {
            g_cfg.target_name = argv[++k];
        } else if (strcmp(a, "-expose_emulated") == 0 && k + 1 < argc) {
            g_cfg.expose_emulated = atoi(argv[++k]);
        } else if (strcmp(a, "-inline") == 0 && k + 1 < argc) {
            g_cfg.enable_inline = atoi(argv[++k]);
        } else if (strcmp(a, "-generic") == 0 && k + 1 < argc) {
            g_cfg.enable_generic = atoi(argv[++k]);
        } else if (strcmp(a, "-trap") == 0 && k + 1 < argc) {
            g_cfg.enable_trap = atoi(argv[++k]);
        } else if (strcmp(a, "-verbose") == 0 && k + 1 < argc) {
            g_cfg.verbose = atoi(argv[++k]);
        } else if (strcmp(a, "-emu_mask") == 0 && k + 1 < argc) {
            extern uint64_t g_emu_mask_override;
            extern int g_emu_mask_set_;
            g_emu_mask_override = strtoull(argv[++k], NULL, 16);
            g_emu_mask_set_ = 1;
        } else if (strcmp(a, "-nospoof") == 0) {
            extern int g_no_spoof; g_no_spoof = 1;
        } else if (strcmp(a, "-no256") == 0) {
            extern int g_disable_256; g_disable_256 = 1;
        } else if (strcmp(a, "-no128") == 0) {
            extern int g_disable_128; g_disable_128 = 1;
        } else if (strcmp(a, "-nospecial") == 0) {
            extern int g_disable_special; g_disable_special = 1;
        } else if (strcmp(a, "-nobcast") == 0) {
            extern int g_disable_bcast; g_disable_bcast = 1;
        } else if (strcmp(a, "-nomsk") == 0) {
            extern int g_disable_msk; g_disable_msk = 1;
        } else if (strcmp(a, "-novz") == 0) {
            extern int g_disable_vz; g_disable_vz = 1;
        } else if (strcmp(a, "-noveh") == 0) {
            g_no_veh = 1;
        } else if (strcmp(a, "-noproxy") == 0) {
            g_no_proxy = 1;
        } else if (strcmp(a, "-stage") == 0 && k + 1 < argc) {
            g_stage_override = atoi(argv[++k]);
        } else if (strcmp(a, "-specialonly") == 0 && k + 1 < argc) {
            g_special_only = atoi(argv[++k]);
        } else if (strcmp(a, "-skipopc") == 0 && k + 1 < argc) {
            /* B-2012: "1067,1064,..." — опкоды оставить нативными (bisect) */
            extern uint8_t g_skip_opc[2048];
            char tmp[256];
            strncpy(tmp, argv[++k], sizeof(tmp) - 1);
            tmp[sizeof(tmp) - 1] = 0;
            for (char *tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
                int op = atoi(tok);
                if (op > 0 && op < 2048)
                    g_skip_opc[op] = 1;
            }
        } else if (strcmp(a, "-strace") == 0) {
            g_strace = 1;
        } else if (strcmp(a, "-probealloc") == 0) {
            g_probe_alloc = 1;
        } else if (strcmp(a, "-noprobealloc") == 0) {
            g_probe_alloc = 0;
        } else if (strcmp(a, "-sampler") == 0 && k + 1 < argc) {
            /* B-2009 diag: PC-сэмплинг (dr_set_itimer), мс */
            g_sampler_ms = atoi(argv[++k]);
        } else if (strcmp(a, "-fuzz") == 0) {
            g_do_fuzz = 1;
        }
    }
}

int g_no_veh = 0;        /* -noveh    */
int g_no_proxy = 0;      /* -noproxy  */
int g_stage_override = -1; /* -stage N */
int g_sampler_ms = 0;     /* -sampler <мс>: PC-сэмплинг (diag B-2009) */
int g_do_fuzz = 0;       /* -fuzz: перебор карт рерайтера */
int g_strace = 0;        /* -strace: syscall-трейсер (diag B-2010) */
int g_special_only = -1; /* -specialonly N: спец-путь только для одного opc */
int g_probe_alloc = -1;  /* B-2019: -probealloc/-noprobealloc; -1 = авто
                          * (включаются вместе с -strace на Windows)  */

/* ════ B-2019: per-thread кольцо последних generic-опов ════
 * (переезд из emu.c: глобальное кольцо перемешивало нити — в логе
 * B-2018 кольцо «перед паникой» содержало опы watcher-потока с
 * NtDeviceIoControlFile, а не паникующей нити.)                   */
#define GENERIC_RING_N 32
typedef struct {
    int    opc[GENERIC_RING_N];
    app_pc pc[GENERIC_RING_N];   /* app-PC каждого опа (B-2017) */
    unsigned idx;                /* глобальный счётчик записей нити */
} diag_ring_t;
static int g_tls_idx_ring;
static diag_ring_t *g_main_ring;   /* первое аллоцированное кольцо */
static diag_ring_t *g_last_active; /* последнее кольцо, куда писали —
 * для exit-дампа: DR на exit/init даёт dcontext, отличный от
 * execution-контекста (натурно: ноты шли в 0x...dc0, а ensure на
 * init в 0x...3c8), поэтому dc==NULL берём последнее активное */
static int          g_fallback_ring_opc[GENERIC_RING_N];
static app_pc       g_fallback_ring_pc[GENERIC_RING_N];
static volatile unsigned g_fallback_ring_idx;

void pass_generic_ring_note(int opc, app_pc pc)
{
    void *dc = dr_get_current_drcontext();
    diag_ring_t *r = (dc != NULL)
        ? (diag_ring_t *)drmgr_get_tls_field(dc, g_tls_idx_ring) : NULL;
    if (r == NULL) {
        /* нить без TLS (ранний trap) — общий fallback */
        unsigned i = g_fallback_ring_idx++ & (GENERIC_RING_N - 1);
        g_fallback_ring_opc[i] = opc;
        g_fallback_ring_pc[i]  = pc;
        return;
    }
    unsigned i = r->idx++ & (GENERIC_RING_N - 1);
    r->opc[i] = opc;
    r->pc[i]  = pc;
    g_last_active = r; /* без блокировки: гонка = чужое кольцо в exit-дампе,
                        * что и раньше давал глобальный буфер B-2016 */
}

void pass_diag_ring_get(void *dc, int **opc, app_pc **pc, unsigned *idx,
                        unsigned *is_global)
{
    static diag_ring_t fallback_dummy;
    diag_ring_t *r = NULL;
    if (dc != NULL)
        r = (diag_ring_t *)drmgr_get_tls_field(dc, g_tls_idx_ring);
    else
        r = (g_last_active != NULL) ? g_last_active : g_main_ring;
    if (r != NULL) {
        *opc = r->opc; *pc = r->pc; *idx = r->idx; *is_global = 0;
    } else {
        /* fallback-кольцо — копия в dummy, чтобы вернуть стабильные
         * указатели (печать идёт сразу после вызова — без гонок важнее
         * целостность: snapshot без блокировки, как и раньше).     */
        for (int k = 0; k < GENERIC_RING_N; k++) {
            fallback_dummy.opc[k] = g_fallback_ring_opc[k];
            fallback_dummy.pc[k]  = g_fallback_ring_pc[k];
        }
        fallback_dummy.idx = g_fallback_ring_idx;
        *opc = fallback_dummy.opc; *pc = fallback_dummy.pc;
        *idx = fallback_dummy.idx; *is_global = 1;
    }
}
/* ════ конец B-2019 кольца ════ */

/* Ленивая инициализация TLS: thread-init не вызывается для главного
 * потока (он уже под DR до регистрации события), поэтому первый
 * clean call инициализирует состояние сам.                        */
void pass_ensure_tls(void *dc)
{
    if (drmgr_get_tls_field(dc, g_tls_idx_buf) != NULL)
        return;
    void *raw_base = dr_get_dr_segment_base(g_tls_seg);
    uint8_t *buf = (uint8_t *)dr_thread_alloc(dc, BUF_SIZE);
    memset(buf, 0, BUF_SIZE);
    /* БАГФИКС §6.16: C-код должен видеть РОВНО ту же память, что и
     * inline-доступы raw_opnd(off) = gs:(g_tls_raw_off + off).
     * Раньше сохранялся голый segment base — все чтения/записи тени
     * EY()/emu-путей уходили на 0xe0 байт мимо (тихая порча/крахи). */
    drmgr_set_tls_field(dc, g_tls_idx_raw,
                        (uint8_t *)raw_base + g_tls_raw_off);
    drmgr_set_tls_field(dc, g_tls_idx_buf, buf);
    /* B-2019: per-thread кольцо generic-опов; первая нить = главная */
    diag_ring_t *ring = (diag_ring_t *)dr_thread_alloc(dc, sizeof(diag_ring_t));
    memset(ring, 0, sizeof(*ring));
    drmgr_set_tls_field(dc, g_tls_idx_ring, ring);
    if (g_main_ring == NULL)
        g_main_ring = ring;
}

static void pass_thread_init(void *dc)
{
    pass_ensure_tls(dc);
}

static void pass_thread_exit(void *dc)
{
    uint8_t *buf = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_buf);
    if (buf != NULL)
        dr_thread_free(dc, buf, BUF_SIZE);
    /* B-2019: кольцо НЕ освобождаем — DR вызывает thread-exit РАНЬШЕ
     * exit-события, и dr_thread_free затирает первые 8 байт блока
     * (free-list заголовок): exit-дамп читал opc[0..1]=0 при живых
     * pcs (натурный репрод t_flags). Утечка одного кольца (540Б) на
     * нить до конца процесса — дешевле, чем UAF-дамп.              */
}

static const char *opc_name(int opc)
{
    /* короткое имя через дизассемблер не нужно — печатаем номер */
    static char buf[32];
    dr_snprintf(buf, sizeof(buf), "op#%d", opc);
    return buf;
}

static void pass_exit(void)
{
    uint32_t static_total = 0, generic_total = 0, unsup_total = 0;
    int top[5] = { -1, -1, -1, -1, -1 };
    for (int k = 0; k < OPC_MAX; k++) {
        static_total += g_stat_static[k];
        generic_total += g_stat_generic[k];
        unsup_total += g_stat_unsupported[k];
    }
    /* топ-5 generic по динамике */
    for (int t = 0; t < 5; t++) {
        int best = -1;
        for (int k = 0; k < OPC_MAX; k++) {
            if (g_stat_generic[k] == 0) continue;
            bool taken = false;
            for (int q = 0; q < t; q++) if (top[q] == k) taken = true;
            if (!taken && (best < 0 || g_stat_generic[k] > g_stat_generic[best]))
                best = k;
        }
        top[t] = best;
    }

    dr_fprintf(STDERR,
        "\n[P.A.S.S.] ── итоги ──────────────────────────────────\n"
        "  target            : %s\n"
        "  базовых блоков    : %u\n"
        "  inline-переписано : %u (статически)\n"
        "  generic-эмуляций  : %llu (динамически)\n"
        "  trap-срабатываний : %u\n"
        "  без эмуляции      : %u (статически; поймает trap)\n",
        g_cfg.target_name, g_stat_bbs, static_total,
        (unsigned long long)g_dyn_generic, g_stat_trap, unsup_total);
    if (top[0] >= 0) {
        dr_fprintf(STDERR, "  top generic: ");
        for (int t = 0; t < 5 && top[t] >= 0; t++)
            dr_fprintf(STDERR, "%s=%u ", opc_name(top[t]), g_stat_generic[top[t]]);
        dr_fprintf(STDERR, "\n");
    }
    /* B-2016: последние generic-опы — если процесс умер без итогов
     * (fastfail), у пользователя не будет и этой строки; здесь она для
     * живых прогонов (быстрая видимость хвоста эмуляции). */
    /* B-2019: dc=NULL → кольцо = последнее активное (g_last_active):
     * DR на exit-событии даёт dcontext, отличный от execution — ноты
     * уходили в другое кольцо и exit-дамп печатал пустоту.          */
    pass_diag_dump("exit", NULL);
    /* B-2012: топ-10 статических перезаписей per-opc (бисект inline-пути) */
    if (static_total > 0) {
        int stop[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        for (int t = 0; t < 10; t++) {
            int best = -1;
            for (int k = 0; k < OPC_MAX; k++) {
                if (g_stat_static[k] == 0) continue;
                bool taken = false;
                for (int q = 0; q < t; q++) if (stop[q] == k) taken = true;
                if (!taken && (best < 0 || g_stat_static[k] > g_stat_static[best]))
                    best = k;
            }
            stop[t] = best;
        }
        dr_fprintf(STDERR, "  top static: ");
        for (int t = 0; t < 10 && stop[t] >= 0; t++)
            dr_fprintf(STDERR, "%s=%u ", opc_name(stop[t]), g_stat_static[stop[t]]);
        dr_fprintf(STDERR, "\n");
    }
    /* §7.3: топ-5 НЕобработанных опкодов (left-native/trap-класс).
     * Пусто в здоровом прогоне; ненулевой — прямое имя виновника
     * класса «молчаливый пропуск → мусор → паника аллокатора».     */
    if (unsup_total > 0) {
        int utop[5] = { -1, -1, -1, -1, -1 };
        for (int t = 0; t < 5; t++) {
            int best = -1;
            for (int k = 0; k < OPC_MAX; k++) {
                if (g_stat_unsupported[k] == 0) continue;
                bool taken = false;
                for (int q = 0; q < t; q++) if (utop[q] == k) taken = true;
                if (!taken && (best < 0 || g_stat_unsupported[k] > g_stat_unsupported[best]))
                    best = k;
            }
            utop[t] = best;
        }
        dr_fprintf(STDERR, "  top unsupported: ");
        for (int t = 0; t < 5 && utop[t] >= 0; t++)
            dr_fprintf(STDERR, "%s=%u ", opc_name(utop[t]), g_stat_unsupported[utop[t]]);
        dr_fprintf(STDERR, "\n");
    }
    dr_fprintf(STDERR, "  recreate: ours=%u skipTG=%u restored=%u nobuf=%u\n",
               g_rs_ours, g_rs_skip_tg, g_rs_restored, g_rs_nobuf);
#ifdef WINDOWS
    {
        extern int g_n_raise_swallowed; /* strace.c: pre-syscall глотание */
        if (g_n_raise_swallowed > 0)
            dr_fprintf(STDERR,
                       "  исключений thread-name/ODS проглочено: %d "
                       "(B-2015: без этого Godot и драйверы графики умирали)\n",
                       g_n_raise_swallowed);
    }
#endif
    dr_fprintf(STDERR, "[P.A.S.S.] ─────────────────────────────────────────\n\n");
}

DR_EXPORT
void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_fprintf(STDERR, "[P.A.S.S.] client base " PIFX "\n",
               (ptr_uint_t)dr_get_client_base(id));
#ifdef LINUX
    /* §7.3: DR загружен кастомным лоадером (анонимно в maps) — печатаем
     * все exec-регионы для привязки crash PC к rva libdynamorio */
    {
        FILE *mf = fopen("/proc/self/maps", "r");
        if (mf != NULL) {
            char line[512];
            while (fgets(line, sizeof(line), mf) != NULL)
                if (strstr(line, "r-xp") != NULL)
                    dr_fprintf(STDERR, "[MAP] %s", line);
            fclose(mf);
        }
    }
#endif
#ifdef WINDOWS
    /* B-2015: modmap переехал НИЖЕ (после parse_options, гейт -verbose 2) */
#endif
    (void)id;
    dr_set_client_name("P.A.S.S.", "https://example.invalid/pass");
    dr_fprintf(STDERR, "[P.A.S.S.] build stamp B-2019 (v0.2.9)\n");

    /* СНАЧАЛА опции: гейты (-noveh/-noproxy/-stage) влияют на всё ниже */
    memset(&g_cfg, 0, sizeof(g_cfg));
    parse_options(argc, argv);

#ifdef LINUX
    /* временная диагностика: перехват SIGSEGV для скана стека краша —
     * только при PASS_SEGVDUMP=1 (иначе подменяет обработчик DR)     */
    {
        const char *sv = getenv("PASS_SEGVDUMP");
        if (sv != NULL) {
            extern void pass_segv_dump_install(void);
            pass_segv_dump_install();
        }
    }
#else
    {
        /* §7.3: VEH без env-гейта — getenv в инжектируемом процессе
         * ненадёжен (diag: PASS_SEGVDUMP/PASS_EXCLOG не доходили);
         * фильтр дешёвый, лог через WriteFile напрямую (не dr_fprintf —
         * vsnprintf-машинария DR — сам участок fault#1).
         * -noveh — бисекция fault#1 без VEH-перехватчика.            */
        extern void pass_veh_install(void *, size_t);
        if (!g_no_veh)
            pass_veh_install((void *)dr_get_client_base(id), 0x30000);
    }
    /* B-2015: перехват RaiseException в источнике (drwrap-замена):
     * 0x406D1388/ODS глотаются до любой OS-механики (фикс Godot) */
    {
        extern void pass_raise_hook_install(void);
        pass_raise_hook_install();
    }
#endif

    features_init();
    g_cfg.profile = profile_from_name(g_cfg.target_name);

    /* TLS */
    drmgr_init();
    g_tls_idx_raw = drmgr_register_tls_field();
    g_tls_idx_buf = drmgr_register_tls_field();
    g_tls_idx_ring = drmgr_register_tls_field(); /* B-2019: кольцо diag */
    uint align = 16;
    if (!dr_raw_tls_calloc(&g_tls_seg, &g_tls_raw_off,
                           RAW_SIZE / 8, align)) {
        dr_fprintf(STDERR, "[P.A.S.S.] FATAL: dr_raw_tls_calloc failed\n");
        dr_abort();
    }
    /* главный поток: thread-init для него уже не придёт — инициализируем сейчас */
    pass_ensure_tls(dr_get_current_drcontext());

    cpuid_init();
    pass_rewrite_init();
    /* B-2009: restore_state — корректная рекреация app-контекста
     * переписанных последовательностей (SMC-flush/deletion-redirect;
     * заморозка bun/JSC на холодном патчировании JIT-страниц) */
    pass_restore_state_init();
    /* §7.3: перебор карт рерайтера (ловушка UNENCODABLE без bun).
     * ТОЛЬКО клиентская опция: getenv в инжектируемом процессе
     * ненадёжен даже на Linux (возвращал мусор → ложный запуск).   */
    if (g_do_fuzz) {
        extern void pass_fuzz_run(void);
        pass_fuzz_run();
    }
    if (g_cfg.enable_trap)
        trap_init();
    /* B-2010 diag: syscall-трейсер (опция -strace) */
    /* B-2013: безусловно — модуль содержит не только diag-трейсер
     * (-strace), но и постоянный фикс close_range (см. strace.c) */
    pass_strace_init();

#ifdef WINDOWS
    /* B-2019: точечные зонды аллокатора bun (drwrap по RVA стандартной
     * сборки 1.4.5). Авто = вместе с -strace; форс -probealloc / гейт
     * -noprobealloc. Тело pass_probe_install: только Windows.        */
    {
        extern void pass_probe_install(void);
        int on = (g_probe_alloc < 0) ? (g_strace ? 1 : 0) : g_probe_alloc;
        if (on)
            pass_probe_install();
    }
#endif

#ifdef WINDOWS
    /* B-2015: modmap после parse_options (нужен g_cfg.verbose) */
    if (g_cfg.verbose >= 2) {
        file_t f = dr_open_file("modmap.txt", DR_FILE_WRITE_OVERWRITE);
        if (f != INVALID_FILE) {
            dr_module_iterator_t *mi = dr_module_iterator_start();
            while (dr_module_iterator_hasnext(mi)) {
                module_data_t *md = dr_module_iterator_next(mi);
                dr_fprintf(f, "[MOD] " PIFX ".." PIFX " %s\n",
                           (ptr_uint_t)md->start, (ptr_uint_t)md->end,
                           dr_module_preferred_name(md) != NULL
                               ? dr_module_preferred_name(md) : "?");
                dr_free_module_data(md);
            }
            dr_module_iterator_stop(mi);
            dr_close_file(f);
        }
    }
#endif

    /* B-2009 diag: сэмплер app-PC (dr_set_itimer); getenv в инжектируемом
     * процессе ненадёжен — только клиентская опция -sampler <мс>. */
#ifdef LINUX
    if (g_sampler_ms > 0) {
        extern void pass_sampler_start(long interval_ms);
        pass_sampler_start(g_sampler_ms);
    }
#endif

    drmgr_register_thread_init_event(pass_thread_init);
    drmgr_register_thread_exit_event(pass_thread_exit);
    /* PASS_STAGE: 1 = без bb-события (диагностика). env в инжектируемом
     * процессе ненадёжен — дублируем клиентской опцией -stage N.     */
    int stage = 2;
    {
        const char *st = getenv("PASS_STAGE");
        if (st != NULL) stage = atoi(st);
    }
    if (g_stage_override >= 0)
        stage = g_stage_override;
    if (stage >= 2)
        drmgr_register_bb_app2app_event(pass_bb_event, NULL);
    drmgr_register_exit_event(pass_exit);

    if (g_cfg.verbose)
        print_banner();
}
