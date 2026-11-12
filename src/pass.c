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
        "  ║              Substitution  v0.2.1            ║\n"
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
        }
    }
}

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
    dr_fprintf(STDERR, "[P.A.S.S.] ─────────────────────────────────────────\n\n");
}

DR_EXPORT
void dr_client_main(client_id_t id, int argc, const char *argv[])
{
    dr_fprintf(STDERR, "[P.A.S.S.] client base " PIFX "\n",
               (ptr_uint_t)dr_get_client_base(id));
#ifdef WINDOWS
    /* §7.3: карта модулей в ФАЙЛ рядом с клиентом (stderr-хвост 200Б
     * её затирает): rерайты opc=675/662 @ 0x7ff7...d52/d56 — чей код? */
    {
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
    (void)id;
    dr_set_client_name("P.A.S.S.", "https://example.invalid/pass");
    dr_fprintf(STDERR, "[P.A.S.S.] build stamp B-2003\n");

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
         * фильтр дешёвый (2 сравнения), лог только для rip в клиенте */
        extern void pass_veh_install(void *, size_t);
        pass_veh_install((void *)dr_get_client_base(id), 0x30000);
    }
#endif

    memset(&g_cfg, 0, sizeof(g_cfg));
    parse_options(argc, argv);

    features_init();
    g_cfg.profile = profile_from_name(g_cfg.target_name);

    /* TLS */
    drmgr_init();
    g_tls_idx_raw = drmgr_register_tls_field();
    g_tls_idx_buf = drmgr_register_tls_field();
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
    if (g_cfg.enable_trap)
        trap_init();

    drmgr_register_thread_init_event(pass_thread_init);
    drmgr_register_thread_exit_event(pass_thread_exit);
    /* PASS_STAGE: 1 = без bb-события (диагностика) */
    int stage = 2;
    {
        const char *st = getenv("PASS_STAGE");
        if (st != NULL) stage = atoi(st);
    }
    if (stage >= 2)
        drmgr_register_bb_app2app_event(pass_bb_event, NULL);
    drmgr_register_exit_event(pass_exit);

    if (g_cfg.verbose)
        print_banner();
}
