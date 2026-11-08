/* ============================================================
 * P.A.S.S. — Performance Adaptive SIMD Substitution
 * ------------------------------------------------------------
 * Клиент DynamoRIO: выборочная эмуляция неподдерживаемых
 * инструкций (SSE4.1/4.2, AVX/AVX2, FMA, BMI1/2, AES-NI и пр.)
 * поверх старых CPU. Поддерживаемые инструкции проходят
 * нативно (passthrough через кодовый кэш DR).
 *
 * Гибридная стратегия:
 *   1. Статический рескрайнинг при трансляции базовых блоков.
 *   2. Trap-fallback: SIGILL/exception → эмуляция по месту
 *      (страховка для JIT-кода и пропущенных случаев).
 *   3. Маскирование CPUID/XGETBV — диспетчеры (glibc ifunc,
 *      компиляторные multiversion-ветки) сами выбирают пути,
 *      которые мы умеем исполнять.
 * ============================================================ */
#ifndef PASS_H
#define PASS_H

#include "dr_api.h"
#include "drmgr.h"
#include <stdint.h>
#include <string.h>

/* ────────────────────── портируемость ──────────────────────
 * GCC/Clang: C11-атомики + cpuid.h; MSVC: Interlocked + __cpuid. */
#ifdef _MSC_VER
#include <intrin.h>
#define PASS_ATOMIC_INC32(p) \
    _InterlockedExchangeAdd((volatile long *)(p), 1)
static __inline int pass_cpuid(uint32_t leaf, uint32_t *a, uint32_t *b,
                               uint32_t *c, uint32_t *d)
{
    int info[4] = { 0, 0, 0, 0 };
    __cpuid(info, (int)leaf);
    *a = (uint32_t)info[0]; *b = (uint32_t)info[1];
    *c = (uint32_t)info[2]; *d = (uint32_t)info[3];
    return 1;
}
static __inline int pass_cpuid_count(uint32_t leaf, uint32_t sub,
                                     uint32_t *a, uint32_t *b,
                                     uint32_t *c, uint32_t *d)
{
    int info[4] = { 0, 0, 0, 0 };
    __cpuidex(info, (int)leaf, (int)sub);
    *a = (uint32_t)info[0]; *b = (uint32_t)info[1];
    *c = (uint32_t)info[2]; *d = (uint32_t)info[3];
    return 1;
}
#else
#define PASS_ATOMIC_INC32(p) __atomic_fetch_add((p), 1, __ATOMIC_RELAXED)
#include <cpuid.h>
static __inline int pass_cpuid(uint32_t leaf, uint32_t *a, uint32_t *b,
                               uint32_t *c, uint32_t *d)
{
    return __get_cpuid(leaf, a, b, c, d);
}
static __inline int pass_cpuid_count(uint32_t leaf, uint32_t sub,
                                     uint32_t *a, uint32_t *b,
                                     uint32_t *c, uint32_t *d)
{
    return __get_cpuid_count(leaf, sub, a, b, c, d);
}
#endif

/* ────────────────────────── Фичи ────────────────────────── */

typedef enum {
    F_SSE3 = 0, F_SSSE3, F_SSE41, F_SSE42, F_POPCNT, F_AES, F_PCLMUL,
    F_AVX, F_AVX2, F_FMA, F_BMI1, F_BMI2, F_MOVBE, F_LZCNT, F_F16C,
    F_XSAVE, F_OSXSAVE, F_PREFW,
    F_COUNT
} feat_t;

#define FEATSET_MAX 64
typedef uint64_t featset_t;

static inline featset_t FEAT(feat_t f) { return ((featset_t)1) << (int)f; }
static inline int       has(featset_t s, feat_t f) { return (int)((s >> (int)f) & 1); }

/* ────────────────────── Конфигурация ────────────────────── */

typedef struct {
    const char *target_name;
    featset_t   profile;        /* что «имеет» целевой (старый) CPU   */
    featset_t   host;           /* реальный хост                      */
    int         expose_emulated;/* светить эмулируемые фичи в CPUID   */
    int         enable_inline;  /* inline-эмуляция                    */
    int         enable_generic; /* clean-call эмуляция                */
    int         enable_trap;    /* SIGILL/VEH fallback                */
    int         verbose;
    int         shadow_active;  /* ymm-shadow включён (после 1-го VEX.256) */
    int         shadow_flushed; /* кэш сброшен после активации        */
} pass_config_t;

extern pass_config_t g_cfg;
extern int   g_tls_idx_raw;   /* drmgr TLS: база RAW TLS (segment base)  */
extern int   g_tls_idx_buf;   /* drmgr TLS: per-thread BUF               */
extern reg_id_t g_tls_seg;    /* сегмент RAW TLS (gs на Linux x64)       */
extern uint  g_tls_raw_off;   /* смещение RAW TLS                        */

/* Статистика */
#define OPC_MAX 2048
extern uint32_t g_stat_static[OPC_MAX];   /* переписано при трансляции  */
extern uint32_t g_stat_generic[OPC_MAX];  /* исполнено через clean call */
extern uint32_t g_stat_unsupported[OPC_MAX];
extern uint32_t g_stat_trap;              /* срабатываний trap-fallback */
extern uint32_t g_stat_bbs;

/* ─────────── RAW TLS (быстрые inline-доступы через gs:) ───────────
 * hi-половины ymm + временные слоты. Инвариант shadow-режима:
 *   hardware xmm_r = младшие 128 бит логического ymm_r,
 *   RAW_YHI(r)     = старшие 128 бит логического ymm_r.
 * Legacy-SSE запись в xmm_r сохраняет старшие биты (действий нет);
 * VEX.128 запись обнуляет RAW_YHI(r) (2 mov-imm, без регистров);
 * VEX.256 читает/пишет обе половины.                               */
#define RAW_YHI(i)  (0u   + (i)*16u)   /* 16 × 16Б: 0..383  */
#define RAW_T0      384u              /* staging mem-операндов   */
#define RAW_T1      400u
#define RAW_S15     416u              /* спил xmm15              */
#define RAW_TG      432u              /* спил временного GPR     */
#define RAW_TG2     440u              /* второй GPR-темп         */
#define RAW_TG3     448u              /* образ eflags            */
#define RAW_SV10    456u              /* спил носителя r10 (generic-путь).
                                         ВАЖНО: в v0.1 r10 спилился в
                                         RAW_YHI(0) — порча тени ymm0  */
#define RAW_T2      464u              /* третий staging (алиасинг dst==src) */
#define RAW_SIZE    480u              /* 60 слотов по 8Б         */

/* ───────────── per-thread BUF (доступ C-кода через r10) ─────────────
 * Спил-образ регистров для generic-пути (clean call).               */
#define BUF_GPR(i)  (0u   + (i)*8u)    /* RAX..R15 → 0..15         */
#define BUF_XMM(i)  (128u + (i)*16u)   /* образ xmm: 128..383      */
#define BUF_FLAGS   384u               /* образ eflags             */
#define BUF_CPU     392u               /* cpuid scratch 6×8Б       */
#define BUF_SIZE    440u

/* Индексы регистров (xmm_idx принимает и XMM, и YMM; gpr_idx
 * нормализует любой ширинный GPR — EAX/AX/AL → RAX)               */
static inline reg_id_t gpr_norm(reg_id_t r) { return reg_resize_to_opsz(r, OPSZ_8); }
static inline int gpr_idx(reg_id_t r) { return (int)(gpr_norm(r) - DR_REG_RAX); }
static inline int is_gpr_reg(reg_id_t r) {
    reg_id_t n = gpr_norm(r);
    return n >= DR_REG_RAX && n <= DR_REG_R15;
}
static inline int xmm_idx(reg_id_t r) {
    if (r >= DR_REG_YMM0 && r <= DR_REG_YMM15)
        return (int)(r - DR_REG_YMM0);
    return (int)(r - DR_REG_XMM0);
}

/* ── определение VEX/EVEX по сырым байтам (в DR 11.9x нет
 * публичного instr_is_vex/evex): C4/C5 = VEX, 62 = EVEX ── */
static inline int instr_bytes_vex(instr_t *i)
{
    const byte *b = instr_get_raw_bits(i);
    return b != NULL && (b[0] == 0xC4 || b[0] == 0xC5);
}
static inline int instr_bytes_evex(instr_t *i)
{
    const byte *b = instr_get_raw_bits(i);
    return b != NULL && b[0] == 0x62;
}

/* ─────────────────────── features.c ─────────────────────── */
void      features_init(void);
featset_t profile_from_name(const char *name);
const char *feat_name(int f);
int       opc_supported(int opc, opnd_size_t dsz);
int       pass_is_generic_opc(int opc);   /* опкод эмулируется generic-путём */

/* ─────────────────────── cpuid_hook.c ────────────────────── */
void cpuid_init(void);
extern int g_no_spoof;
featset_t exposed_features(void);
void emu_cpuid(void);                                   /* clean call */
void rewrite_cpuid(void *dc, instrlist_t *bb, instr_t *where);
void rewrite_xgetbv(void *dc, instrlist_t *bb, instr_t *where);

/* ────────────────────────── emu.c ────────────────────────── */
typedef struct {
    int      opc;
    int      nsrc, ndst;
    opnd_t   srcs[4];
    opnd_t   dsts[2];
    uint32_t gpr_src, gpr_dst;   /* битовые маски индексов RAX..R15 */
    uint32_t xmm_src, xmm_dst;   /* битовые маски индексов XMM0..15 */
    uint8_t  flags_use, flags_def;
    uint8_t  uses_xmm0;          /* implicit xmm0 (blendv, pcmp*str*) */
    uint8_t  is_ymm;             /* 256-битная операция               */
    uint8_t  is_vex;             /* VEX-форма: xmm-dst обнуляет верх YMM */
    app_pc   pc_next;            /* app-PC следующей инструкции (для
                                  * rip-относительных mem-операндов)  */
} emu_desc_t;

int  emu_generic(emu_desc_t *d);            /* clean call, 1 = обработано */
void emu_vzeroupper(void);                  /* clean call */
void emu_vzeroall(void);                    /* clean call */

extern uint64_t g_dyn_generic;
extern uint64_t g_dyn_trap;

/* ───────────────────────── rewrite.c ─────────────────────── */
void pass_ensure_tls(void *dc);
void pass_rewrite_init(void);
dr_emit_flags_t pass_bb_event(void *dc, void *tag, instrlist_t *bb,
                              bool for_trace, bool translating);
int  rewrite_generic_instr(void *dc, instrlist_t *bb, instr_t *i);

/* Хелперы генерации кода (реализация в rewrite.c) */
void pass_ins(void *dc, instrlist_t *bb, instr_t *at, instr_t *in);
opnd_t mem_off16(opnd_t mem, int disp_add, opnd_size_t sz); /* копия mem-операнда со сдвигом */
opnd_t buf_opnd(uint off, opnd_size_t sz);                  /* [r10 + off]        */
opnd_t raw_opnd(uint off, opnd_size_t sz);                  /* [gs: raw_off+off]  */
void   carrier_begin(void *dc, instrlist_t *bb, instr_t *at); /* r10 ← база BUF  */
void   carrier_end(void *dc, instrlist_t *bb, instr_t *at, uint32_t gpr_dst);
void   zero_yhi(void *dc, instrlist_t *bb, instr_t *at, int xmm_idx); /* RAW_YHI[x] = 0 */

/* Таблицы (реализация в rewrite_vex.c / rewrite_special.c / emu.c) */
typedef struct {
    int vex_op;      /* OP_vxxx */
    int legacy_op;   /* OP_xxx  */
    int form;        /* VF_*    */
    feat_t req;      /* требуемая фича для нативного исполнения */
} vex_map_ent_t;

enum { VF_NDS = 0, VF_ND2, VF_SHIFT, VF_DIR, VF_SPECIAL };
const vex_map_ent_t *vex_map_lookup(int opc);
int  try_rewrite_vex(void *dc, instrlist_t *bb, instr_t *i);
int  try_rewrite_special(void *dc, instrlist_t *bb, instr_t *i);
int  special_req_ok(int opc);   /* есть ли вообще обработчик */

/* ────────────────────────── trap.c ────────────────────────── */
void trap_init(void);
void trap_exit(void);

#endif /* PASS_H */
