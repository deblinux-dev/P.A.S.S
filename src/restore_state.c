/* P.A.S.S. — restore_state.c (B-2009)
 * Корректная рекреация app-контекста для ПЕРЕПИСАННЫХ последовательностей.
 *
 * Проблема: наши мета-инструкции несут translation-поле (нужно для
 * доставки фолтов приложения через fault-proxy и для якорей рекреации).
 * DR при SMC-flush/deletion-redirect рекреирует app-контекст потока,
 * застрявшего внутри такой последовательности: без restore_state callback
 * DR возвращает СЫРЫЙ кэш-контекст — r10 = база BUF, rax = мусор от гарда,
 * флаги от test, dst-регистры уже материализованы (6) — приложение
 * возобновляется на pc(i) с искажённым состоянием (натурный симптом:
 * заморозка bun 1.2.14 на JIT-холодном патчировании — JSC пишет в RWX-
 * страницы через memcpy → SMC-flush → рекреация → спин без syscall'ов;
 * репрод tests/data/t_smc.c, Windows-симптом B-2008 — бесконечные
 * retranslate-циклы generic: opc=...).
 *
 * Решение (per DR API): dr_register_restore_state_ex_event + пред-оп
 * банки в BUF (BUF_FLAGS0/BUF_GPRD/BUF_XMMD — BUF_FLAGS и BUF_GPR(k)
 * перезаписываются обёрткой emu_generic после-оп значениями).
 *
 * Правила callback:
 *  1. касаемся только прерываний ВНУТРИ наших последовательностей
 *     (locate по recreate-ilist: note == PASS_META_NOTE на инструкции,
 *     содержащей raw pc; фолбэк — якорь dst->pc в реестре переписанных pc);
 *  2. Case B (фолт-доставка fault-proxy): BUF_TG != 0 — контекст уже
 *     приготовлен прокси (rax/flags = app, r10 = адрес фолта) — НЕ трогаем;
 *  3. r10 читается из gs-RAW_SV10 только на потоке-владельце
 *     (drcontext совпадает с текущим) — cross-thread рекреация этот слот
 *     не трогает (gs чужой);
 *  4. восстановление = пред-оп состояние: приложение возобновляется на
 *     pc(i) и РЕИСПОЛНЯЕТ операцию целиком (детерминированно).
 *                                                                  */
#define _GNU_SOURCE
#include "pass.h"

static int g_dbg_calls = 0;

/* B-2012 diag: счётчики recreate-пути (бисект «порча при перезаписи») */
uint32_t g_rs_ours = 0;      /* recreate попал в нашу последовательность */
uint32_t g_rs_skip_tg = 0;   /* скипнуты по BUF_TG != 0 (Case B)         */
uint32_t g_rs_restored = 0;  /* реально восстановлено                    */
uint32_t g_rs_nobuf = 0;     /* buf == NULL                              */

/* ── реестр переписанных pc: pc → (gpr_dst, xmm_dst, cap_flags) ── */
typedef struct rwrec {
    void            *pc;
    struct rwrec    *next;
    uint32_t         gpr_dst;
    uint32_t         xmm_dst;
    uint32_t         cap_flags;
} rwrec_t;

#define RW_HBITS 12
#define RW_HSIZE (1u << RW_HBITS)
static rwrec_t  *g_rw[RW_HSIZE];
static void     *g_rw_lock;

static uint rw_hash(void *pc)
{
    uintptr_t v = (uintptr_t)pc;
    v ^= v >> 17;
    v *= (uintptr_t)0x9E3779B97F4A7C15ull;
    return (uint)((v >> 8) & (RW_HSIZE - 1));
}

void pass_rw_record(void *pc, uint32_t gpr_dst, uint32_t xmm_dst, int cap_flags)
{
    uint h = rw_hash(pc);
    dr_mutex_lock(g_rw_lock);
    rwrec_t *r;
    for (r = g_rw[h]; r != NULL && r->pc != pc; r = r->next) { }
    if (r == NULL) {
        r = (rwrec_t *)dr_global_alloc(sizeof(rwrec_t));
        r->pc = pc;
        r->next = g_rw[h];
        g_rw[h] = r;
    }
    r->gpr_dst = gpr_dst;
    r->xmm_dst = xmm_dst;
    r->cap_flags = (uint32_t)cap_flags;
    dr_mutex_unlock(g_rw_lock);
}

static rwrec_t *pass_rw_find(void *pc)
{
    uint h = rw_hash(pc);
    dr_mutex_lock(g_rw_lock);
    rwrec_t *r;
    for (r = g_rw[h]; r != NULL && r->pc != pc; r = r->next) { }
    dr_mutex_unlock(g_rw_lock);
    return r;
}

/* reg_t-слот dr_mcontext по gpr_idx (RAX=0..R15=15) */
static reg_t *mc_gpr_slot(dr_mcontext_t *mc, int k)
{
    switch (k) {
    case 0:  return &mc->xax;
    case 1:  return &mc->xcx;
    case 2:  return &mc->xdx;
    case 3:  return &mc->xbx;
    case 4:  return &mc->xsp;
    case 5:  return &mc->xbp;
    case 6:  return &mc->xsi;
    case 7:  return &mc->xdi;
    case 8:  return &mc->r8;
    case 9:  return &mc->r9;
    case 10: return &mc->r10;
    case 11: return &mc->r11;
    case 12: return &mc->r12;
    case 13: return &mc->r13;
    case 14: return &mc->r14;
    case 15: return &mc->r15;
    }
    return NULL;
}

/* drmgr-вариант: restore_memory передаётся отдельно, tag внутри info */
static bool pass_restore_state_ex(void *drcontext, bool restore_memory,
                                  dr_restore_state_info_t *info)
{
    void *tag = info->fragment_info.tag;
    dr_mcontext_t *dst = info->mcontext;
    (void)restore_memory;
    rwrec_t *rec = NULL;
    bool in_ours = false;
    if (g_cfg.verbose >= 3 && g_dbg_calls < 300) {
        g_dbg_calls++;
        dr_fprintf(STDERR,
                   "[P.A.S.S.] xl8: raw=" PIFX " dstpc=" PIFX
                   " ilist=%d tag=" PIFX " rm=%d\n",
                   (info->raw_mcontext_valid && info->raw_mcontext)
                       ? (ptr_uint_t)info->raw_mcontext->pc : 0,
                   (ptr_uint_t)dst->pc,
                   info->fragment_info.ilist != NULL,
                   (ptr_uint_t)info->fragment_info.tag,
                   (int)restore_memory);
    }

    /* (1) точная локализация: инструкция recreate-ilist, содержащая
     * raw pc, помечена нашим note? */
    if (info->fragment_info.ilist != NULL && info->raw_mcontext_valid &&
        info->raw_mcontext != NULL &&
        info->fragment_info.cache_start_pc != NULL) {
        byte *cur = info->fragment_info.cache_start_pc;
        byte *raw = info->raw_mcontext->pc;
        for (instr_t *in = instrlist_first(info->fragment_info.ilist);
             in != NULL; in = instr_get_next(in)) {
            int len = instr_length(drcontext, in);
            if (raw >= cur && (len <= 0 ? raw == cur
                                        : raw < cur + len)) {
                if (instr_get_note(in) == PASS_META_NOTE)
                    in_ours = true;
                break;
            }
            cur += len;
        }
    }
    if (in_ours || info->fragment_info.ilist == NULL) {
        rec = pass_rw_find(dst->pc);
        if (rec != NULL)
            in_ours = true;
    }
    if (!in_ours || rec == NULL)
        return true;

    /* (2) Case B: фолт-доставка fault-proxy — контекст приготовлен
     * прокси; рестарт не нужен */
    uint8_t *buf = (uint8_t *)drmgr_get_tls_field(drcontext, g_tls_idx_buf);
    if (buf == NULL) {
        g_rs_nobuf++;
        return true;
    }
    g_rs_ours++;
    if (*(uint64_t *)(buf + BUF_TG) != 0) {
        g_rs_skip_tg++;
        return true;
    }

    /* (3) пред-оп восстановление: приложение возобновится на pc(i)
     * и реисполнит операцию целиком. r10 — из BUF_R10 (обёртка emu_generic
     * кладёт туда app-r10 на входе; drmgr-TLS — безопасно cross-thread,
     * в отличие от gs-RAW_SV10). */
    dst->r10 = *(uint64_t *)(buf + BUF_R10);
    if (rec->cap_flags)
        dst->xflags = *(uint64_t *)(buf + BUF_FLAGS0);
    dst->rax = *(uint64_t *)(buf + BUF_GPRD(0));
    /* B-2010: во время стаба rsp = per-thread scratch-стек (перекл.
     * (1b)); app rsp гарантированно лежит в BUF_GPR(4) (заливка (4)).
     * Без этого фолт внутри переписанной последовательности доставился
     * бы приложению с rsp = scratch-стек — SEH/сигнальные хендлеры
     * исполнялись бы на чужом стеке. */
    dst->xsp = *(uint64_t *)(buf + BUF_GPR(gpr_idx(DR_REG_RSP)));
    for (int k = 0; k < 16; k++) {
        if (rec->gpr_dst & (1u << k)) {
            reg_t *slot = mc_gpr_slot(dst, k);
            if (slot != NULL)
                *slot = *(uint64_t *)(buf + BUF_GPRD(k));
        }
    }
    for (int k = 0; k < 16; k++) {
        if (rec->xmm_dst & (1u << k))
            memcpy(&dst->simd[k], buf + BUF_XMMD(k), 16);
    }
    g_rs_restored++;
    return true;
}

void pass_restore_state_init(void)
{
    g_rw_lock = dr_mutex_create();
    drmgr_register_restore_state_ex_event(pass_restore_state_ex);
}
