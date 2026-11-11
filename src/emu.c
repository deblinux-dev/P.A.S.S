/* P.A.S.S. — emu.c
 * Универсальный эмулятор «сложных» инструкций через clean call.
 * Работает с образами регистров в per-thread BUF и shadow-файлом
 * старших половин ymm в RAW TLS.
 *
 * ВАЖНО: файл компилируется с -mno-sse (см. CMakeLists) — код
 * эмулятора не трогает xmm/ymm приложения. Плавающая точка —
 * через x87.                                                 */
#include "pass.h"
#include <setjmp.h>
#include <math.h>

typedef struct {
    uint8_t *buf;   /* per-thread BUF (спилы GPR/XMM, флаги, CPUID) */
    uint8_t *raw;   /* RAW TLS base (YHI-shadow)                    */
    app_pc    pc_next;         /* для rip-относительных адресов */
} estate_t;

static inline uint64_t *EG(estate_t *st, int i) { return (uint64_t *)(st->buf + BUF_GPR(i)); }
static inline uint8_t  *EX(estate_t *st, int i) { return st->buf + BUF_XMM(i); }
static inline uint8_t  *EY(estate_t *st, int i) { return st->raw + RAW_YHI(i); }
static inline uint32_t *EFLAG(estate_t *st)     { return (uint32_t *)(st->buf + BUF_FLAGS); }

/* ── загрузка/сохранение скаляров ── */
static inline uint64_t ld64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }
static inline uint32_t ld32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint16_t ld16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline uint8_t  ld8 (const uint8_t *p) { return p[0]; }
static inline void st64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline void st32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void st16(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }
static inline void st8 (uint8_t *p, uint8_t v)  { p[0] = v; }

/* ── флаги ── */
#define FCF (1u<<0)
#define FPF (1u<<2)
#define FAF (1u<<4)
#define FZF (1u<<6)
#define FSF (1u<<7)
#define FDF (1u<<10)
#define FOF (1u<<11)

static uint32_t flags_get(estate_t *st) { return *EFLAG(st); }
static void flags_setmask(estate_t *st, uint32_t set, uint32_t clr)
{
    uint32_t fl = *EFLAG(st);
    fl = (fl & ~clr) | set;
    *EFLAG(st) = fl;
}
/* задать арифметические флаги по результату; биты из keep НЕ трогаются.
 * §6.22: реальные Intel-микросхемы (наблюдение на Comet Lake) НЕ пишут
 * PF для ANDN/BZHI/BEXTR/BLSI/BLSMSK/BLSR/SARX/SHLX/SHRX/RORX/MULX —
 * эмуляция обязана сохранять PF нетронутым, иначе приложения, где
 * флаги переживают инструкцию (bun.exe: бесконечный спин), ломаются. */
static void flags_arith_k(estate_t *st, uint64_t res, uint64_t mask,
                          int carry, int overflow, uint32_t keep)
{
    uint32_t fl = 0;
    if ((res & mask) == 0) fl |= FZF;
    if (res & (mask ^ (mask >> 1))) fl |= FSF;              /* старший бит */
    if (carry) fl |= FCF;
    if (overflow) fl |= FOF;
    /* PF по младшему байту: 1 при ЧЁТНОМ числе единиц (канон x86) */
    {
        int b = (int)(res & 0xFF), p = 0;
        while (b) { p ^= (b & 1); b >>= 1; }
        if (p == 0) fl |= FPF;
    }
    flags_setmask(st, fl & ~keep, (FCF|FPF|FAF|FZF|FSF|FOF) & ~keep);
}

static void flags_arith(estate_t *st, uint64_t res, uint64_t mask, int carry, int overflow)
{
    flags_arith_k(st, res, mask, carry, overflow, 0);
}

/* ── доступ к операндам ── */
static byte *mem_addr(estate_t *st, opnd_t op)
{
    /* rip-относительные операнды DR нормализует в rel_addr с уже
     * вычисленным абсолютным адресом (mc-путь даёт мусор) — §6.17 */
    if (opnd_is_rel_addr(op))
        return (byte *)opnd_get_addr(op);
    dr_mcontext_t mc;
    memset(&mc, 0, sizeof(mc));
    mc.size = sizeof(mc);
    mc.flags = DR_MC_INTEGER | DR_MC_CONTROL;
    mc.xax = *EG(st, 0); mc.xcx = *EG(st, 1); mc.xdx = *EG(st, 2);
    mc.xbx = *EG(st, 3); mc.xsp = *EG(st, 4); mc.xbp = *EG(st, 5);
    mc.xsi = *EG(st, 6); mc.xdi = *EG(st, 7);
    mc.r8 = *EG(st, 8);  mc.r9 = *EG(st, 9);  mc.r10 = *EG(st, 10);
    mc.r11 = *EG(st, 11); mc.r12 = *EG(st, 12); mc.r13 = *EG(st, 13);
    mc.r14 = *EG(st, 14); mc.r15 = *EG(st, 15);
    mc.pc = st->pc_next;                 /* rip-относительная база */
    return opnd_compute_address(op, &mc);
}

/* ── §7.3: прозрачность фолтов generic-пути ──
 * Mem-операнды эмулируемых инструкций разыменуются из clean call в
 * клиентской DLL. ЛЕГИТИМНЫЙ page fault приложения (SEH-пробы CRT/кучи
 * Windows, guard-page стека) внутри clean call DR классифицирует как
 * internal crash — приложение не получает исключение (проверено:
 * dr_register_signal_event для клиентских фолтов не вызывается;
 * bun.exe умирал в первые 8с старта). Все доступы к памяти приложения
 * — через dr_safe_read/dr_safe_write; при невозможности — записать
 * адрес/тип в RAW_FAULT/RAW_TG и прервать эмуляцию longjmp'ом.
 * Сгенерированный код после clean call исполняет ПРОКСИ-доступ по
 * реальному адресу уже в кэше (rewrite.c): фолт в кэше DR транслирует
 * в app-PC инструкции — SEH/сигналы работают как нативно. Inline-путь
 * прозрачен изначально (там реальные инструкции).                 */
#ifdef _MSC_VER
# define PASS_TLS __declspec(thread)
#else
# define PASS_TLS __thread
#endif
static PASS_TLS jmp_buf g_emu_jmp;
static int emu_generic_impl(emu_desc_t *d);

static void emu_fault_record(estate_t *st, byte *addr, int is_write)
{
    /* §7.3: слоты в BUF (r10-относительные), а не в RAW (gs): в контексте
     * доставки сигнала gs-база ненадёжна, а r10-носитель жив до (7). */
    if (st->buf != NULL) {
        *(uint64_t *)(st->buf + BUF_TG) = 1u | ((uint64_t)(is_write & 1) << 1);
        *(uint64_t *)(st->buf + BUF_FAULT) = (uint64_t)(uintptr_t)addr;
    }
    longjmp(g_emu_jmp, 1);
}

static void emu_safe_read(estate_t *st, void *src, void *dst, size_t n)
{
    size_t done = 0;
    if (!dr_safe_read(src, n, dst, &done) || done != n)
        emu_fault_record(st, (byte *)src, 0);
}

static void emu_safe_write(estate_t *st, void *dst, const void *src, size_t n)
{
    size_t done = 0;
    if (!dr_safe_write(dst, n, src, &done) || done != n)
        emu_fault_record(st, (byte *)dst, 1);
}

static int is_xmm_reg(opnd_t o)
{
    return opnd_is_reg(o) && opnd_get_reg(o) >= DR_REG_XMM0 &&
           opnd_get_reg(o) <= DR_REG_XMM15;
}

/* векторный регистр (xmm ИЛИ ymm): до багфикса §6.15 YMM-операнды
 * здесь не распознавались — generic-операции над ymm читали нули */
static int is_vec_reg(opnd_t o)
{
    if (!opnd_is_reg(o)) return 0;
    reg_id_t r = opnd_get_reg(o);
    return (r >= DR_REG_XMM0 && r <= DR_REG_XMM15) ||
           (r >= DR_REG_YMM0 && r <= DR_REG_YMM15);
}

/* 16-байтный блок из регистра или памяти (tmp для mem) */
static const uint8_t *get_block16(estate_t *st, opnd_t o, uint8_t *tmp)
{
    if (is_vec_reg(o))  return EX(st, xmm_idx(opnd_get_reg(o)));
    if (opnd_is_memory_reference(o)) {
        emu_safe_read(st, mem_addr(st, o), tmp, 16);
        return tmp;
    }
    memset(tmp, 0, 16);
    return tmp;
}
static void put_block16(estate_t *st, opnd_t o, const uint8_t *v)
{
    if (is_vec_reg(o))      memcpy(EX(st, xmm_idx(opnd_get_reg(o))), v, 16);
    else if (opnd_is_memory_reference(o)) emu_safe_write(st, mem_addr(st, o), v, 16);
}

/* 32-байтный блок: reg → EX+EY (или shadow), mem → чтение по фактическому
 * размеру операнда (OPSZ_32/16/4) с обнулением остатка — важно для
 * insertps/pinsr с m32-операндами, чтобы не читать лишнюю память. */
static void get_block32(estate_t *st, opnd_t o, uint8_t *out32)
{
    memset(out32, 0, 32);
    if (is_vec_reg(o)) {
        int i = xmm_idx(opnd_get_reg(o));
        memcpy(out32, EX(st, i), 16);
        memcpy(out32 + 16, EY(st, i), 16);
    } else if (opnd_is_memory_reference(o)) {
        size_t n = 32;
        switch (opnd_get_size(o)) {
        case OPSZ_1: n = 1; break;
        case OPSZ_2: n = 2; break;
        case OPSZ_4: n = 4; break;
        case OPSZ_8: n = 8; break;
        case OPSZ_16: n = 16; break;
        default: n = 32; break;
        }
        emu_safe_read(st, mem_addr(st, o), out32, n);
    }
}

/* GPR: чтение с учётом размера */
static uint64_t get_gpr_val(estate_t *st, opnd_t o)
{
    if (opnd_is_reg(o) && is_gpr_reg(opnd_get_reg(o)))
        return *EG(st, gpr_idx(opnd_get_reg(o)));
    if (opnd_is_memory_reference(o)) {
        byte *a = mem_addr(st, o);
        size_t n = (opnd_get_size(o) == OPSZ_1) ? 1
                 : (opnd_get_size(o) == OPSZ_2) ? 2
                 : (opnd_get_size(o) == OPSZ_4) ? 4 : 8;
        uint64_t v = 0;
        emu_safe_read(st, a, &v, n);
        return v;
    }
    if (opnd_is_immed_int(o)) return (uint64_t)opnd_get_immed_int(o);
    return 0;
}
/* GPR: запись с учётом размера (32Б пишется с обнулением верха) */
static void put_gpr_val(estate_t *st, opnd_t o, uint64_t val)
{
    if (opnd_is_reg(o) && is_gpr_reg(opnd_get_reg(o))) {
        int i = gpr_idx(opnd_get_reg(o));
        switch (opnd_get_size(o)) {
        case OPSZ_1: *EG(st, i) = (*EG(st, i) & ~0xFFull) | (val & 0xFF); return;
        case OPSZ_2: *EG(st, i) = (*EG(st, i) & ~0xFFFFull) | (val & 0xFFFF); return;
        case OPSZ_4: *EG(st, i) = (val & 0xFFFFFFFFull); return;
        default:     *EG(st, i) = val; return;
        }
    }
    if (opnd_is_memory_reference(o)) {
        byte *a = mem_addr(st, o);
        size_t n = (opnd_get_size(o) == OPSZ_1) ? 1
                 : (opnd_get_size(o) == OPSZ_2) ? 2
                 : (opnd_get_size(o) == OPSZ_4) ? 4 : 8;
        uint64_t tmp = 0;
        switch (n) {
        case 1: tmp = (uint8_t)val; break;
        case 2: tmp = (uint16_t)val; break;
        case 4: tmp = (uint32_t)val; break;
        default: tmp = val; break;
        }
        if (g_cfg.verbose >= 4)
            dr_fprintf(STDERR, "[P.A.S.S.][MEMW] " PIFX " <- %02x (sz %d)\n",
                       (ptr_uint_t)a, (unsigned)(val & 0xFF),
                       (int)opnd_get_size(o));
        emu_safe_write(st, a, &tmp, n);
    }
}

/* ══════════════════ блочные (16Б) операции ══════════════════
 * Каждая функция пишет результат в локальный буфер r[], затем
 * копирует в d — алиасинг d/s1/s2 безопасен.                    */

#define BLOCK1(name, ...) \
static void name(uint8_t *d, const uint8_t *s1, const uint8_t *s2, uint imm) \
{ uint8_t r[16]; (void)imm; { __VA_ARGS__ } memcpy(d, r, 16); }

/* PCMPGTQ: 64-битное знаковое сравнение */
BLOCK1(b_pcmpgtq, {
    for (int q = 0; q < 2; q++) {
        int64_t a = (int64_t)ld64(s1 + q*8), b = (int64_t)ld64(s2 + q*8);
        st64(r + q*8, a > b ? ~(uint64_t)0 : 0);
    }
})
/* PCMPEQQ */
BLOCK1(b_pcmpeqq, {
    for (int q = 0; q < 2; q++)
        st64(r + q*8, ld64(s1 + q*8) == ld64(s2 + q*8) ? ~(uint64_t)0 : 0);
})

#define MINMAX_B(name, cond, T, cnt, ld, st) \
BLOCK1(name, { \
    for (int i = 0; i < cnt; i++) { \
        T a = (T)ld(s1 + i*sizeof(T)), b = (T)ld(s2 + i*sizeof(T)); \
        T m = (cond) ? a : b; \
        st(r + i*sizeof(T), (uint64_t)m); \
    } \
})

/* pmin/pmax для 8/16/32, знаковые и беззнаковые */
MINMAX_B(b_pminub, a < b,  uint8_t,  16, ld8,  st8)
MINMAX_B(b_pmaxub, a > b,  uint8_t,  16, ld8,  st8)
MINMAX_B(b_pminuw, a < b, uint16_t, 8, ld16, st16)
MINMAX_B(b_pmaxuw, a > b, uint16_t, 8, ld16, st16)
MINMAX_B(b_pminud, a < b, uint32_t, 4, ld32, st32)
MINMAX_B(b_pmaxud, a > b, uint32_t, 4, ld32, st32)
MINMAX_B(b_pminsb, a < b,  int8_t,  16, ld8,  st8)
MINMAX_B(b_pmaxsb, a > b,  int8_t,  16, ld8,  st8)
MINMAX_B(b_pminsw, a < b, int16_t, 8, ld16, st16)
MINMAX_B(b_pmaxsw, a > b, int16_t, 8, ld16, st16)
MINMAX_B(b_pminsd, a < b, int32_t, 4, ld32, st32)
MINMAX_B(b_pmaxsd, a > b, int32_t, 4, ld32, st32)

/* PACKUSDW: dword→word, насыщение беззнаковое */
BLOCK1(b_packusdw, {
    for (int i = 0; i < 4; i++) {
        int32_t a = (int32_t)ld32(s1 + i*4), b = (int32_t)ld32(s2 + i*4);
        uint16_t ua = a < 0 ? 0 : (a > 65535 ? 65535 : (uint16_t)a);
        uint16_t ub = b < 0 ? 0 : (b > 65535 ? 65535 : (uint16_t)b);
        st16(r + i*2, ua);
        st16(r + 8 + i*2, ub);
    }
})

/* PMULLD: знаковое 32×32→32 */
BLOCK1(b_pmulld, {
    for (int i = 0; i < 4; i++)
        st32(r + i*4, (uint32_t)((int64_t)(int32_t)ld32(s1+i*4) * (int32_t)ld32(s2+i*4)));
})
/* PMULDQ: знаковое 32×32→64 (чётные dword) */
BLOCK1(b_pmuldq, {
    for (int i = 0; i < 2; i++)
        st64(r + i*8, (uint64_t)((int64_t)(int32_t)ld32(s1+i*8) * (int64_t)(int32_t)ld32(s2+i*8)));
})

/* PHMINPOSUW */
BLOCK1(b_phminposuw, {
    uint16_t mv = 0xFFFF; int mi = 0;
    for (int i = 0; i < 8; i++) {
        uint16_t v = ld16(s1 + i*2);
        if (v < mv) { mv = v; mi = i; }
    }
    memset(r, 0, 16);
    st16(r, mv);
    r[2] = (uint8_t)mi;
})

/* MPSADBW */
BLOCK1(b_mpsadbw, {
    int offA = ((imm >> 2) & 1) * 4;
    int offB = (imm & 3) * 4;
    for (int i = 0; i < 4; i++) {
        unsigned s = 0;
        for (int k = 0; k < 4; k++) {
            int a = s1[offA + i + k], b = s2[offB + k];
            s += (unsigned)(a > b ? a - b : b - a);
        }
        st16(r + i*2, (uint16_t)s);
    }
})

/* PTEST: ZF=(d&s)==0, CF=(~d&s)==0 */
static void b_ptest(estate_t *st, uint8_t *d, const uint8_t *s)
{
    int zf = 1, cf = 1;
    for (int i = 0; i < 16; i++) {
        if (d[i] & s[i]) zf = 0;
        if (~d[i] & s[i]) cf = 0;
    }
    flags_setmask(st, (zf ? FZF : 0) | (cf ? FCF : 0), FCF|FZF|FSF|FOF|FAF|FPF);
}

/* ROUNDPS/PD/SS/SD */
static inline float roundf_mode(float x, int m)
{
    switch (m) {
    case 1: return floorf(x);
    case 2: return ceilf(x);
    case 3: return truncf(x);
    default: return nearbyintf(x);
    }
}
static inline double roundd_mode(double x, int m)
{
    switch (m) {
    case 1: return floor(x);
    case 2: return ceil(x);
    case 3: return trunc(x);
    default: return nearbyint(x);
    }
}
BLOCK1(b_roundps, {
    for (int i = 0; i < 4; i++) {
        float v; memcpy(&v, s1 + i*4, 4);
        float o = roundf_mode(v, (int)(imm & 3));
        memcpy(r + i*4, &o, 4);
    }
})
BLOCK1(b_roundpd, {
    for (int i = 0; i < 2; i++) {
        double v; memcpy(&v, s1 + i*8, 8);
        double o = roundd_mode(v, (int)(imm & 3));
        memcpy(r + i*8, &o, 8);
    }
})
/* скалярные:lane0 из s2, остальное из s1(dst) */
BLOCK1(b_roundss, {
    memcpy(r, s1, 16);
    float v; memcpy(&v, s2, 4);
    float o = roundf_mode(v, (int)(imm & 3));
    memcpy(r, &o, 4);
})
BLOCK1(b_roundsd, {
    memcpy(r, s1, 16);
    double v; memcpy(&v, s2, 8);
    double o = roundd_mode(v, (int)(imm & 3));
    memcpy(r, &o, 8);
})

/* DPPS/DPPD */
BLOCK1(b_dpps, {
    float sum = 0.0f;
    memset(r, 0, 16);
    for (int i = 0; i < 4; i++) {
        if (imm & (1 << (4+i))) {
            float a, b; memcpy(&a, s1 + i*4, 4); memcpy(&b, s2 + i*4, 4);
            sum = (float)(sum + (float)(a * b));
        }
    }
    for (int i = 0; i < 4; i++)
        if (imm & (1 << i)) memcpy(r + i*4, &sum, 4);
})
BLOCK1(b_dppd, {
    double sum = 0.0;
    memset(r, 0, 16);
    for (int i = 0; i < 2; i++) {
        if (imm & (1 << (4+i))) {
            double a, b; memcpy(&a, s1 + i*8, 8); memcpy(&b, s2 + i*8, 8);
            sum = sum + a * b;
        }
    }
    for (int i = 0; i < 2; i++)
        if (imm & (1 << i)) memcpy(r + i*8, &sum, 8);
})

/* BLENDPS/PD (imm) */
BLOCK1(b_blendps, {
    for (int i = 0; i < 4; i++)
        memcpy(r + i*4, (imm & (1 << (4+i))) ? s2 + i*4 : s1 + i*4, 4);
})
BLOCK1(b_blendpd, {
    for (int i = 0; i < 2; i++)
        memcpy(r + i*8, (imm & (1 << (4+i))) ? s2 + i*8 : s1 + i*8, 8);
})
/* BLENDVPS/PD: маска = m (xmm0), знак элемента маски */
static void b_blendvps_m(uint8_t *d, const uint8_t *s1, const uint8_t *s2, const uint8_t *m)
{
    uint8_t r[16];
    for (int i = 0; i < 4; i++)
        memcpy(r + i*4, (ld32(m + i*4) >> 31) ? s2 + i*4 : s1 + i*4, 4);
    memcpy(d, r, 16);
}
static void b_blendvpd_m(uint8_t *d, const uint8_t *s1, const uint8_t *s2, const uint8_t *m)
{
    uint8_t r[16];
    for (int i = 0; i < 2; i++)
        memcpy(r + i*8, (ld64(m + i*8) >> 63) ? s2 + i*8 : s1 + i*8, 8);
    memcpy(d, r, 16);
}
/* PBLENDVB: побайтно */
static void b_pblendvb_m(uint8_t *d, const uint8_t *s1, const uint8_t *s2, const uint8_t *m)
{
    uint8_t r[16];
    for (int i = 0; i < 16; i++)
        r[i] = (m[i] & 0x80) ? s2[i] : s1[i];
    memcpy(d, r, 16);
}
/* VPBLENDD (imm, dword) */
BLOCK1(b_vpblendd, {
    for (int i = 0; i < 4; i++)
        memcpy(r + i*4, (imm & (1 << (4+i))) ? s2 + i*4 : s1 + i*4, 4);
})

/* INSERTPS */
BLOCK1(b_insertps, {
    int zmask = (int)((imm >> 6) & 3);
    int dsel  = (int)((imm >> 4) & 3);
    int ssel  = (int)((imm >> 2) & 3);
    uint32_t v = ld32(s2 + ssel*4);
    memset(r, 0, 16);
    for (int i = 0; i < 4; i++) {
        uint32_t val = ld32(s1 + i*4);
        if (zmask & (1 << i)) val = 0;
        else if (i == dsel)   val = v;
        st32(r + i*4, val);
    }
})

/* PMOVSX/ZX (расширения) */
#define PMOVX(name, SRC_T, DST_T, NSRC, sld, sst, SIGN) \
BLOCK1(name, { \
    for (int i = 0; i < NSRC; i++) { \
        SRC_T v = (SRC_T)sld(s1 + i*sizeof(SRC_T)); \
        DST_T e = SIGN ? (DST_T)(int64_t)(SRC_T##_s(v)) : (DST_T)v; \
        sst(r + i*sizeof(DST_T), (uint64_t)e); \
    } \
})

/* проще: ручные варианты */
#define PMOV_SEXT(name, SRC_T, DST_T, NSRC, sld, sst) \
BLOCK1(name, { \
    for (int i = 0; i < NSRC; i++) { \
        int64_t v = (int64_t)(SRC_T)sld(s1 + i*sizeof(SRC_T)); \
        sst(r + i*sizeof(DST_T), (uint64_t)(DST_T)v); \
    } \
})
#define PMOV_ZEXT(name, SRC_T, DST_T, NSRC, sld, sst) \
BLOCK1(name, { \
    for (int i = 0; i < NSRC; i++) { \
        uint64_t v = (uint64_t)(SRC_T)sld(s1 + i*sizeof(SRC_T)); \
        sst(r + i*sizeof(DST_T), (uint64_t)(DST_T)v); \
    } \
})
PMOV_SEXT(b_pmovsxbw, int8_t,  int16_t, 8, ld8,  st16)
PMOV_SEXT(b_pmovsxbd, int8_t,  int32_t, 4, ld8,  st32)
PMOV_SEXT(b_pmovsxbq, int8_t,  int64_t, 2, ld8,  st64)
PMOV_SEXT(b_pmovsxwd, int16_t, int32_t, 4, ld16, st32)
PMOV_SEXT(b_pmovsxwq, int16_t, int64_t, 2, ld16, st64)
PMOV_SEXT(b_pmovsxdq, int32_t, int64_t, 2, ld32, st64)
PMOV_ZEXT(b_pmovzxbw, uint8_t,  uint16_t, 8, ld8,  st16)
PMOV_ZEXT(b_pmovzxbd, uint8_t,  uint32_t, 4, ld8,  st32)
PMOV_ZEXT(b_pmovzxbq, uint8_t,  uint64_t, 2, ld8,  st64)
PMOV_ZEXT(b_pmovzxwd, uint16_t, uint32_t, 4, ld16, st32)
PMOV_ZEXT(b_pmovzxwq, uint16_t, uint64_t, 2, ld16, st64)
PMOV_ZEXT(b_pmovzxdq, uint32_t, uint64_t, 2, ld32, st64)

/* ══════════════════ AES-NI ══════════════════ */
static const uint8_t AES_SBOX[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};
static uint8_t xt(uint8_t a) { return (uint8_t)((a << 1) ^ ((a & 0x80) ? 0x1b : 0)); }
static uint8_t gmul(uint8_t a, uint8_t b)
{
    uint8_t p = 0;
    while (b) {
        if (b & 1) p ^= a;
        a = xt(a);
        b >>= 1;
    }
    return p;
}
static void shift_rows(uint8_t *s)
{
    uint8_t t = s[1]; s[1] = s[5];  s[5] = s[9];  s[9] = s[13]; s[13] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[3]; s[3] = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = t;
}
static void inv_shift_rows(uint8_t *s)
{
    uint8_t t = s[13]; s[13] = s[9]; s[9] = s[5]; s[5] = s[1]; s[1] = t;
    t = s[2]; s[2] = s[10]; s[10] = t; t = s[6]; s[6] = s[14]; s[14] = t;
    t = s[3]; s[3] = s[7]; s[7] = s[11]; s[11] = s[15]; s[15] = t;
}
static void mix_cols(uint8_t *s)
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = s + c*4;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        p[0] = (uint8_t)(gmul(a0,2) ^ gmul(a1,3) ^ a2 ^ a3);
        p[1] = (uint8_t)(a0 ^ gmul(a1,2) ^ gmul(a2,3) ^ a3);
        p[2] = (uint8_t)(a0 ^ a1 ^ gmul(a2,2) ^ gmul(a3,3));
        p[3] = (uint8_t)(gmul(a0,3) ^ a1 ^ a2 ^ gmul(a3,2));
    }
}
static void inv_mix_cols(uint8_t *s)
{
    for (int c = 0; c < 4; c++) {
        uint8_t *p = s + c*4;
        uint8_t a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
        p[0] = (uint8_t)(gmul(a0,14) ^ gmul(a1,11) ^ gmul(a2,13) ^ gmul(a3,9));
        p[1] = (uint8_t)(gmul(a0,9)  ^ gmul(a1,14) ^ gmul(a2,11) ^ gmul(a3,13));
        p[2] = (uint8_t)(gmul(a0,13) ^ gmul(a1,9)  ^ gmul(a2,14) ^ gmul(a3,11));
        p[3] = (uint8_t)(gmul(a0,11) ^ gmul(a1,13) ^ gmul(a2,9)  ^ gmul(a3,14));
    }
}
static void aes_block(uint8_t *d, const uint8_t *s1, const uint8_t *s2, int opc)
{
    uint8_t r[16];
    memcpy(r, s1, 16);
    switch (opc) {
    case OP_aesenc:
        for (int i = 0; i < 16; i++) r[i] = AES_SBOX[r[i]];
        shift_rows(r);
        mix_cols(r);
        for (int i = 0; i < 16; i++) r[i] ^= s2[i];
        break;
    case OP_aesenclast:
        for (int i = 0; i < 16; i++) r[i] = AES_SBOX[r[i]];
        shift_rows(r);
        for (int i = 0; i < 16; i++) r[i] ^= s2[i];
        break;
    case OP_aesdec:
        for (int i = 0; i < 16; i++) r[i] = AES_SBOX[r[i]]; /* упрощ.: INV_SBOX см. ниже */
        inv_shift_rows(r);
        inv_mix_cols(r);
        for (int i = 0; i < 16; i++) r[i] ^= s2[i];
        break;
    case OP_aesdeclast:
        for (int i = 0; i < 16; i++) r[i] = AES_SBOX[r[i]];
        inv_shift_rows(r);
        for (int i = 0; i < 16; i++) r[i] ^= s2[i];
        break;
    case OP_aesimc:
        inv_mix_cols(r);
        break;
    default:
        break;
    }
    memcpy(d, r, 16);
}
static void aes_keygen(uint8_t *d, const uint8_t *s, uint imm)
{
    static const uint8_t rcon_tab[10] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36};
    uint8_t rc = (imm & 0xF) < 10 ? rcon_tab[imm & 0xF] : 0;
    uint8_t r[16];
    uint32_t x = ld32(s);
    uint32_t xrot = (x >> 8) | (x << 24); /* RotWord */
    uint32_t sub = ((uint32_t)AES_SBOX[xrot & 0xFF]) |
                   ((uint32_t)AES_SBOX[(xrot >> 8) & 0xFF] << 8) |
                   ((uint32_t)AES_SBOX[(xrot >> 16) & 0xFF] << 16) |
                   ((uint32_t)AES_SBOX[(xrot >> 24) & 0xFF] << 24);
    uint32_t x1 = sub ^ ((uint32_t)rc << 24);
    uint32_t x2 = ((uint32_t)AES_SBOX[x & 0xFF]) |
                  ((uint32_t)AES_SBOX[(x >> 8) & 0xFF] << 8) |
                  ((uint32_t)AES_SBOX[(x >> 16) & 0xFF] << 16) |
                  ((uint32_t)AES_SBOX[(x >> 24) & 0xFF] << 24);
    uint64_t hi = ld64(s + 8);
    uint32_t hrot = (uint32_t)((hi >> 32) ^ (hi << 32)); /* ROR 64 на 32 */
    uint32_t x3 = ((uint32_t)AES_SBOX[hrot & 0xFF]) |
                  ((uint32_t)AES_SBOX[(hrot >> 8) & 0xFF] << 8) |
                  ((uint32_t)AES_SBOX[(hrot >> 16) & 0xFF] << 16) |
                  ((uint32_t)AES_SBOX[(hrot >> 24) & 0xFF] << 24);
    x3 ^= ((uint32_t)rc << 24);
    st32(r, x1); st32(r + 4, x2); st32(r + 8, x1); st32(r + 12, x3);
    memcpy(d, r, 16);
}

/* ══════════════════ PCLMULQDQ ══════════════════ */
/* 64×64 → 128-битное произведение без __int128 (MSVC x86 его не даёт):
 * schoolbook по 32-битным леммам, §6.20. */
static void mul64x64(uint64_t x, uint64_t y, uint64_t *lo, uint64_t *hi)
{
    uint32_t x0 = (uint32_t)x, x1 = (uint32_t)(x >> 32);
    uint32_t y0 = (uint32_t)y, y1 = (uint32_t)(y >> 32);
    uint64_t p00 = (uint64_t)x0 * y0;
    uint64_t p01 = (uint64_t)x0 * y1;
    uint64_t p10 = (uint64_t)x1 * y0;
    uint64_t p11 = (uint64_t)x1 * y1;
    uint64_t m1  = p01 + (p00 >> 32);           /* средний разряд */
    uint64_t m2  = p10 + (uint32_t)m1;
    *lo = ((uint64_t)(uint32_t)m2 << 32) | (uint32_t)p00;
    *hi = p11 + (m2 >> 32) + (m1 >> 32);
}

static void pclmul_block(uint8_t *d, const uint8_t *s1, const uint8_t *s2, uint imm)
{
    uint64_t a = ld64(s1 + ((imm & 1) ? 8 : 0));
    uint64_t b = ld64(s2 + ((imm & 16) ? 8 : 0));
    uint64_t lo = 0, hi = 0;
    for (int i = 0; i < 64; i++) {
        if ((b >> i) & 1) {
            if (i == 0) {
                lo ^= a;
            } else {
                lo ^= a << i;
                hi ^= a >> (64 - i);
            }
        }
    }
    st64(d, lo);
    st64(d + 8, hi);
}

/* ══════════════════ переменные сдвиги AVX2 ══════════════════ */
BLOCK1(b_vpsllvd, {
    for (int i = 0; i < 4; i++)
        st32(r + i*4, ld32(s1+i*4) << (ld32(s2+i*4) & 31));
})
BLOCK1(b_vpsrlvd, {
    for (int i = 0; i < 4; i++)
        st32(r + i*4, ld32(s1+i*4) >> (ld32(s2+i*4) & 31));
})
BLOCK1(b_vpsravd, {
    for (int i = 0; i < 4; i++)
        st32(r + i*4, (uint32_t)((int32_t)ld32(s1+i*4) >> (ld32(s2+i*4) & 31)));
})
BLOCK1(b_vpsllvq, {
    for (int i = 0; i < 2; i++)
        st64(r + i*8, ld64(s1+i*8) << (ld64(s2+i*8) & 63));
})
BLOCK1(b_vpsrlvq, {
    for (int i = 0; i < 2; i++)
        st64(r + i*8, ld64(s1+i*8) >> (ld64(s2+i*8) & 63));
})
BLOCK1(b_vpsravq, {
    for (int i = 0; i < 2; i++)
        st64(r + i*8, (uint64_t)((int64_t)ld64(s1+i*8) >> (ld64(s2+i*8) & 63)));
})

/* ══════════════════ 32-байтные (cross-lane) операции ══════════════════ */
static void wide_vpermq(uint8_t *d, const uint8_t *s, uint imm)
{
    uint8_t r[32];
    for (int i = 0; i < 4; i++) {
        int sel = (int)((imm >> (i*2)) & 3);
        memcpy(r + i*8, s + sel*8, 8);
    }
    memcpy(d, r, 32);
}

/* ── FMA (двойное округление mul→add; см. README «Известные ограничения»).
 * kind: 0=fmadd 1=fmsub 2=fnmadd 3=fnmsub (по знаку произведения/слагаемого)
 * alt : 0=нет 1=fmaddsub (чётные lane: −prod+c, нечётные: +prod+c)
 *              2=fmsubadd (чётные: +prod+c, нечётные: −prod+c)
 * f231: 1 → произведение s1*s2, слагаемое dst (132/213: d*s1 и слагаемое s2)
 * Семантика сверена с нативным железом (fma_sem, Xeon FMA): lane0 = sub. */
static inline float ldf32(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static inline uint64_t memcpy_bits(double v) { uint64_t b; memcpy(&b, &v, 8); return b; }

static void fma_block(uint8_t *d, const uint8_t *s1, const uint8_t *s2,
                      int kind, int is_pd, int alt, int form)
{
    int n = is_pd ? 2 : 4;               /* lane на 16-байтовую половину */
    for (int i = 0; i < n; i++) {
        int neg = (kind >= 2), sub = (kind & 1);
        if (alt == 1)      { neg = !(i & 1); sub = 0; }
        else if (alt == 2) { neg = 0;        sub = (i & 1); }
        /* форма определяет пару множителей и слагаемое (SDM):
         * 213: dst = src1×dst + src2   → p = s1*d, c = s2
         * 231: dst = src1×src2 + dst   → p = s1*s2, c = d
         * 132: dst = dst×src2 + src1   → p = d*s2, c = s1
         * Ветвление по is_pd ДО вычислений: чтение неинициализированных
         * float'ов (UB) ломало и double-путь после оптимизации.        */
        if (is_pd) {
            /* ВАЖНО: именно memcpy в double! ld64() возвращает БИТЫ как
             * uint64_t — присваивание double дало бы целочисленное
             * преобразование (0x3FF8... → 4.6e18), §6.18.              */
            double a, q1, q2;
            memcpy(&a,  d + i*8, 8);
            memcpy(&q1, s1 + i*8, 8);
            memcpy(&q2, s2 + i*8, 8);
            double p, c;
            if (form == 1)      { p = q1*q2; c = a;  }
            else if (form == 2) { p = a*q2;  c = q1; }
            else                { p = q1*a;  c = q2; }
            double r = (neg ? -p : p) + (sub ? -c : c);
            uint64_t bits; memcpy(&bits, &r, 8);
            st64(d + i*8, bits);
        } else {
            float af, q1f, q2f;
            memcpy(&af,  d + i*4, 4);
            memcpy(&q1f, s1 + i*4, 4);
            memcpy(&q2f, s2 + i*4, 4);
            float pf, cf;
            if (form == 1)      { pf = q1f*q2f; cf = af;  }
            else if (form == 2) { pf = af*q2f;  cf = q1f; }
            else                { pf = q1f*af;  cf = q2f; }
            float r = (neg ? -pf : pf) + (sub ? -cf : cf);
            uint32_t bits; memcpy(&bits, &r, 4);
            st32(d + i*4, bits);
        }
    }
}

static void wide_vpermd(uint8_t *d, const uint8_t *idx, const uint8_t *tab)
{
    uint8_t r[32];
    for (int i = 0; i < 8; i++) {
        int sel = (int)(ld32(idx + i*4) & 7);
        memcpy(r + i*4, tab + sel*4, 4);
    }
    memcpy(d, r, 32);
}
static void wide_vperm2x128(uint8_t *d, const uint8_t *s1, const uint8_t *s2, uint imm)
{
    uint8_t r[32];
    const uint8_t *h[4] = { s1, s1 + 16, s2, s2 + 16 };
    int sel0 = (int)(imm & 3), sel1 = (int)((imm >> 4) & 3);
    if (imm & 8)       memset(r, 0, 16);
    else               memcpy(r, h[sel0], 16);
    if (imm & 128)     memset(r + 16, 0, 16);
    else               memcpy(r + 16, h[sel1], 16);
    memcpy(d, r, 32);
}
/* VCVTPS2PD: xmm(4 float) → ymm(4 double) */
static void wide_cvtps2pd(uint8_t *d, const uint8_t *s)
{
    uint8_t r[32];
    for (int i = 0; i < 4; i++) {
        float f; memcpy(&f, s + i*4, 4);
        double o = (double)f;
        memcpy(r + i*8, &o, 8);
    }
    memcpy(d, r, 32);
}
/* VCVTPD2PS: ymm(4 double) → xmm(4 float) */
static void wide_cvtpd2ps(uint8_t *d, const uint8_t *s)
{
    uint8_t r[16];
    for (int i = 0; i < 4; i++) {
        double v; memcpy(&v, s + i*8, 8);
        float o = (float)v;
        memcpy(r + i*4, &o, 4);
    }
    memcpy(d, r, 16);
}
/* маскируемые load/store (vpmaskmovd/q): dst, mem(base+index), mask */
static int pmask_elems(emu_desc_t *d)
{
    int esz = (d->opc == OP_vpmaskmovq) ? 8 : 4;
    return d->is_ymm ? (32 / esz) : (16 / esz);
}

static void pmaskmov_op(estate_t *st, emu_desc_t *d, int is_store)
{
    opnd_t mem  = d->srcs[0];
    opnd_t mask = d->srcs[1];
    int esz = (d->opc == OP_vpmaskmovq) ? 8 : 4;
    int n = pmask_elems(d);
    byte *base = mem_addr(st, mem);
    reg_id_t idxreg = opnd_get_index(mem);
    int scale = opnd_get_scale(mem);
    int disp  = opnd_get_disp(mem);
    uint64_t basev = 0;
    if (is_gpr_reg(opnd_get_base(mem)))
        basev = *EG(st, gpr_idx(opnd_get_base(mem)));
    /* маска: ymm-форма читает полные 32Б (EX+EY) */
    uint8_t mvb[32];
    const uint8_t *mv;
    if (is_vec_reg(mask)) {
        int mi = xmm_idx(opnd_get_reg(mask));
        memcpy(mvb, EX(st, mi), 16);
        memcpy(mvb + 16, EY(st, mi), 16);
        mv = mvb;
    } else {
        mv = EX(st, 0);
    }

    if (is_store) {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t tmp[32];
        memcpy(tmp, EX(st, di), 16);
        if (d->is_ymm) memcpy(tmp + 16, EY(st, di), 16);
        for (int i = 0; i < n; i++) {
            uint64_t m = (esz == 4) ? ld32(mv + i*4) : ld64(mv + i*8);
            if (!(m >> (esz*8 - 1))) continue;
            int64_t idx = (int64_t)(int32_t)ld32(EX(st, xmm_idx(idxreg)) + i*4);
            byte *a = (byte *)(basev + idx*scale + disp);
            uint64_t w = (esz == 4) ? ld32(tmp + i*4) : ld64(tmp + i*8);
            emu_safe_write(st, a, &w, esz);
        }
    } else {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t r[32];
        memcpy(r, EX(st, di), 16);
        if (d->is_ymm) memcpy(r + 16, EY(st, di), 16);
        for (int i = 0; i < n; i++) {
            uint64_t m = (esz == 4) ? ld32(mv + i*4) : ld64(mv + i*8);
            if (!(m >> (esz*8 - 1))) continue;
            int64_t idx = (int64_t)(int32_t)ld32(EX(st, xmm_idx(idxreg)) + i*4);
            byte *a = (byte *)(basev + idx*scale + disp);
            uint64_t v = 0;
            emu_safe_read(st, a, &v, esz);
            if (esz == 4) st32(r + i*4, (uint32_t)v); else st64(r + i*8, v);
        }
        memcpy(EX(st, di), r, 16);
        if (d->is_ymm) memcpy(EY(st, di), r + 16, 16);
    }
}

/* gather: dst, mem(base + xmm_index*scale), mask */
static void gather_op(estate_t *st, emu_desc_t *d)
{
    opnd_t mem  = d->srcs[0];
    opnd_t mask = d->srcs[1];
    int data_sz = 4;                    /* dd/dps/qd/qps — 4Б данных */
    int idx_sz  = 4;                    /* dd/dps/dq/dpd — 4Б индексы */
    switch (d->opc) {
    case OP_vpgatherdq: case OP_vgatherdpd: idx_sz = 4; data_sz = 8; break;
    case OP_vpgatherqd: case OP_vgatherqps: idx_sz = 8; data_sz = 4; break;
    case OP_vpgatherqq: case OP_vgatherqpd: idx_sz = 8; data_sz = 8; break;
    default: break;
    }
    int di = xmm_idx(opnd_get_reg(d->dsts[0]));
    int n = (d->is_ymm ? 32 : 16) / data_sz;
    if (idx_sz == 8 && data_sz == 4)
        n = (d->is_ymm ? 32 : 16) / idx_sz;   /* qd/qps: лимит по индексам */

    uint64_t basev = 0;
    if (is_gpr_reg(opnd_get_base(mem)))
        basev = *EG(st, gpr_idx(opnd_get_base(mem)));
    int scale = opnd_get_scale(mem);
    int disp  = opnd_get_disp(mem);
    int ii = xmm_idx(opnd_get_index(mem));
    /* маска: ymm-форма читает полные 32Б (EX+EY) */
    uint8_t mvb[32];
    const uint8_t *mv;
    if (is_vec_reg(mask)) {
        int mi = xmm_idx(opnd_get_reg(mask));
        memcpy(mvb, EX(st, mi), 16);
        memcpy(mvb + 16, EY(st, mi), 16);
        mv = mvb;
    } else {
        mv = EX(st, 0);
    }

    uint8_t r[32];
    memcpy(r, EX(st, di), 16);
    if (d->is_ymm) memcpy(r + 16, EY(st, di), 16);
    if (idx_sz == 8 && data_sz == 4 && !d->is_ymm)
        memset(r + 8, 0, 8);           /* VPGATHERQD xmm: верх обнуляется */

    for (int i = 0; i < n; i++) {
        uint64_t m = (data_sz == 4) ? ld32(mv + i*4) : ld64(mv + i*8);
        if (!(m >> (data_sz*8 - 1))) continue;
        int64_t idx = (idx_sz == 4)
            ? (int64_t)(int32_t)ld32(EX(st, ii) + i*4)
            : (int64_t)ld64(EX(st, ii) + i*8);
        byte *a = (byte *)(basev + idx*scale + disp);
        uint64_t v = 0;
        emu_safe_read(st, a, &v, data_sz);
        if (data_sz == 4) st32(r + i*4, (uint32_t)v); else st64(r + i*8, v);
    }
    memcpy(EX(st, di), r, 16);
    if (d->is_ymm) memcpy(EY(st, di), r + 16, 16);
}

/* ══════════════════ строковые SSE4.2 (экспериментально) ══════════════════ */
typedef struct { uint8_t valid[16]; uint8_t res[16]; int nelem; } strsim_t;

static void string_sim(estate_t *st, emu_desc_t *d,
                       const uint8_t *a, const uint8_t *b, const uint8_t *maskA,
                       uint imm, strsim_t *out)
{
    int sz = (int)(imm & 1) ? 2 : 1;             /* 0=byte 1=word */
    int agg = (int)((imm >> 2) & 3);
    int polarity = (int)((imm >> 4) & 1);
    int ne = 16 / sz;
    out->nelem = ne;

    for (int i = 0; i < ne; i++) {
        uint64_t av = (sz == 1) ? a[i] : ld16(a + i*2);
        uint64_t bv = (sz == 1) ? b[i] : ld16(b + i*2);
        uint64_t mv = maskA ? ((sz == 1) ? (maskA[i] & 0x80) : (ld16(maskA + i*2) & 0x8000)) : 1;
        out->valid[i] = (uint8_t)(maskA ? !!mv : 1);
        int r = 0;
        switch (agg) {
        case 0: /* equal any */
            for (int j = 0; j < ne && !r; j++)
                if (out->valid[j] && av == bv) r = 1;
            break;
        case 1: /* ranges */
            for (int j = 0; j + 1 < ne && !r; j += 2)
                if (out->valid[j] && out->valid[j+1]) {
                    uint64_t lo = (sz == 1) ? b[j] : ld16(b + j*2);
                    uint64_t hi = (sz == 1) ? b[j+1] : ld16(b + (j+1)*2);
                    if (av >= lo && av <= hi) r = 1;
                }
            break;
        case 2: /* equal each */
            r = (out->valid[i] && av == bv) ? 1 : 0;
            break;
        case 3: { /* equal ordered: B — подстрока A, начиная с позиции i */
            int ok = 1;
            for (int k = 0; k < ne; k++) {
                int ai = i + k;
                if (ai >= ne) break;   /* конец A — остаток трактуем как совпадение */
                uint64_t av2 = (sz == 1) ? a[ai] : ld16(a + ai*2);
                uint64_t bv2 = (sz == 1) ? b[k] : ld16(b + k*2);
                if (!out->valid[ai]) break;  /* невалидный элемент — совпадение */
                if (av2 != bv2) { ok = 0; break; }
            }
            r = ok;
            break;
        }
        }
        out->res[i] = (uint8_t)r;
    }
    if (polarity) {
        for (int i = 0; i < ne; i++)
            if (out->valid[i]) out->res[i] ^= 1;
        /* для диапазонов SDM допускает инверсию только на валидных */
    }
}

static void string_op(estate_t *st, emu_desc_t *d, int is_estr, int is_stri)
{
    uint8_t tmpb[16];
    const uint8_t *a = EX(st, xmm_idx(opnd_get_reg(d->dsts[0])));
    const uint8_t *b = get_block16(st, d->srcs[0], tmpb);
    const uint8_t *maskA = NULL;
    uint imm;
    if (d->nsrc >= 2 && opnd_is_immed_int(d->srcs[1])) imm = (uint)opnd_get_immed_int(d->srcs[1]);
    else imm = 0;
    if (is_estr) maskA = EX(st, 0);

    strsim_t sim;
    string_sim(st, d, a, b, maskA, imm, &sim);
    int ne = sim.nelem;
    int any = 0;
    for (int i = 0; i < ne; i++) if (sim.res[i]) any = 1;
    int msb_i = -1, lsb_i = -1;
    for (int i = 0; i < ne; i++) if (sim.res[i]) { lsb_i = i; break; }
    for (int i = ne - 1; i >= 0; i--) if (sim.res[i]) { msb_i = i; break; }

    if (is_stri) {
        int want_msb = (int)((imm >> 6) & 1);
        /* при отрицательной поляритете ищем ноль */
        int neg = (int)((imm >> 4) & 1);
        int idx = ne;
        if (any || neg) {
            if (neg) {
                for (int i = 0; i < ne; i++)
                    if (!sim.res[i] && sim.valid[i]) { idx = i; break; }
                if (want_msb) {
                    idx = ne;
                    for (int i = ne - 1; i >= 0; i--)
                        if (!sim.res[i] && sim.valid[i]) { idx = i; break; }
                }
            } else {
                idx = want_msb ? (msb_i >= 0 ? msb_i : ne) : (lsb_i >= 0 ? lsb_i : ne);
            }
        }
        *EG(st, 1) = (uint64_t)(uint32_t)idx;   /* ECX */
        int invA = 0, invB = 0;
        for (int i = 0; i < ne; i++) {
            if (!sim.valid[i]) invA++;
            if (is_estr && !sim.valid[i]) invB++; /* упрощ. */
        }
        (void)invB;
        uint32_t fl = (any ? FCF : 0) | (sim.res[0] ? FOF : 0);
        if (invA) fl |= FSF;
        flags_setmask(st, fl, FCF|FZF|FSF|FOF|FAF|FPF);
    } else {
        /* STRM: маска в XMM0 */
        int bit_mask_form = (int)((imm >> 6) & 1);
        uint8_t r[16];
        memset(r, 0, 16);
        if (bit_mask_form) {
            for (int i = 0; i < ne; i++)
                if (sim.res[i]) r[i / 8] |= (uint8_t)(1u << (i % 8));
        } else {
            for (int i = 0; i < ne; i++)
                if (sim.res[i]) {
                    if (sim.nelem == 16) r[i] = 0xFF;
                    else st16(r + i*2, 0xFFFF);
                }
        }
        memcpy(EX(st, 0), r, 16);
        uint32_t fl = (any ? FCF : 0) | (sim.res[0] ? FOF : 0);
        flags_setmask(st, fl, FCF|FZF|FSF|FOF|FAF|FPF);
    }
}

/* ══════════════════ GPR-операции ══════════════════ */

static void gpr_crc32(estate_t *st, emu_desc_t *d)
{
    /* CRC-32C (Castagnoli), полином 0x1EDC6F41 (отражённый 0x82F63B78) */
    opnd_t src = d->srcs[0];
    opnd_t dst = d->dsts[0];
    uint64_t crc = get_gpr_val(st, dst) & 0xFFFFFFFF; /* 32-битная база */
    uint64_t val;
    int nbytes;
    switch (opnd_get_size(src)) {
    case OPSZ_1: nbytes = 1; break;
    case OPSZ_2: nbytes = 2; break;
    case OPSZ_8: nbytes = 8; break;
    default:     nbytes = 4; break;
    }
    val = get_gpr_val(st, src);
    for (int k = 0; k < nbytes; k++) {
        crc ^= ((val >> (8*k)) & 0xFF);
        for (int b = 0; b < 8; b++)
            crc = (crc >> 1) ^ (0x82F63B78ull & (-(int64_t)(crc & 1)));
    }
    put_gpr_val(st, dst, crc & 0xFFFFFFFF);
}

static void gpr_pext(estate_t *st, emu_desc_t *d)
{
    /* DR IR для pext: srcs[0]=маска, srcs[1]=данные (проверено
     * дифф-тестом v0.2: иначе результат бит-сдвинут) */
    uint64_t msk = get_gpr_val(st, d->srcs[0]);
    uint64_t src = get_gpr_val(st, d->srcs[1]);
    uint64_t res = 0; int bit = 0;
    for (int i = 0; i < 64; i++)
        if ((msk >> i) & 1) {
            if ((src >> i) & 1) res |= 1ull << bit;
            bit++;
        }
    put_gpr_val(st, d->dsts[0], res);
    /* §6.22: флаги PDEP/PEXT — детерминированно по SDM (ZF/SF/PF по
     * результату, CF/OF=0); реальные Intel отдают часть флагов
     * микрокодом нерегулярно, но приложения флаги PDEP/PEXT не
     * потребляют — детерминированность важнее. */
    flags_arith_k(st, res,
                  (opnd_get_size(d->dsts[0]) == OPSZ_8) ? ~0ull : 0xFFFFFFFFull,
                  0, 0, 0);
}
static void gpr_pdep(estate_t *st, emu_desc_t *d)
{
    uint64_t msk = get_gpr_val(st, d->srcs[0]);
    uint64_t src = get_gpr_val(st, d->srcs[1]);
    uint64_t res = 0; int bit = 0;
    for (int i = 0; i < 64; i++)
        if ((msk >> i) & 1) {
            if ((src >> bit) & 1) res |= 1ull << i;
            bit++;
        }
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res,
                  (opnd_get_size(d->dsts[0]) == OPSZ_8) ? ~0ull : 0xFFFFFFFFull,
                  0, 0, 0);
}
static void gpr_bzhi(estate_t *st, emu_desc_t *d)
{
    uint64_t src = get_gpr_val(st, d->srcs[0]);
    uint64_t idx = get_gpr_val(st, d->srcs[1]) & 0xFF;
    int width = (opnd_get_size(d->dsts[0]) == OPSZ_4) ? 32 : 64;
    uint64_t res, cf = 0;
    if (idx >= (uint64_t)width) { res = src; cf = 1; }
    else res = idx ? (src & ((1ull << idx) - 1)) : 0;
    put_gpr_val(st, d->dsts[0], res);
    if (g_cfg.verbose >= 4)
        dr_fprintf(STDERR, "[P.A.S.S.] bzhi: src=%016llx idx=%d w=%d -> res=%016llx\n",
                   (unsigned long long)src, (int)idx, width, (unsigned long long)res);
    flags_arith_k(st, res, (width == 32) ? 0xFFFFFFFFull : ~0ull, (int)cf, 0, FPF);
}
static void gpr_bextr(estate_t *st, emu_desc_t *d)
{
    uint64_t src = get_gpr_val(st, d->srcs[0]);
    uint64_t ctrl = get_gpr_val(st, d->srcs[1]);
    int width = (opnd_get_size(d->dsts[0]) == OPSZ_4) ? 32 : 64;
    uint64_t start = ctrl & 0xFF, len = (ctrl >> 8) & 0xFF;
    uint64_t v = (start < 64) ? (src >> start) : 0;
    uint64_t mask = (len >= 64) ? ~0ull : ((len == 0) ? 0 : ((1ull << len) - 1));
    uint64_t res = v & mask;
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res, (width == 32) ? 0xFFFFFFFFull : ~0ull, 0, 0, FPF);
}
/* ══════════════════ BMI1/BMI2 (§6.22: generic-путь с точными флагами
 * реального Intel; inline-вариант rewrite_special искажал CF у blsi/
 * blsmsk, ставил PF там, где Intel его не пишет, и портил флаги
 * shl-расширениями sarx/shlx/shrx — bun.exe уходил в бесконечный
 * спин, KAT давал крах DR при sarx dst==rcx) ══════════════════ */
static void gpr_andn(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t s1 = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t s2 = get_gpr_val(st, d->srcs[1]) & mask;
    uint64_t res = (~s1) & s2;
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res, mask, 0, 0, FPF);
}
static void gpr_blsi(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t res = v & ((~v + 1) & mask);          /* x & -x */
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res, mask, (v != 0), 0, FPF);
}
static void gpr_blsmsk(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t res = (v ^ (v - 1)) & mask;
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res, mask, (v == 0), 0, FPF);
}
static void gpr_blsr(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t res = v & (v - 1);
    put_gpr_val(st, d->dsts[0], res);
    flags_arith_k(st, res, mask, (v == 0), 0, FPF);
}
static void gpr_bmi2shift(estate_t *st, emu_desc_t *d)
{
    /* sarx/shlx/shrx: флаги НЕ пишутся вовсе — сохраняем все */
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t c = get_gpr_val(st, d->srcs[1]) & ((w == 64) ? 63 : 31);
    uint64_t res;
    switch (d->opc) {
    case OP_sarx: res = (w == 32)
                        ? (uint64_t)(uint32_t)((int32_t)v >> (int)c)
                        : (uint64_t)((int64_t)v >> (int)c); break;
    case OP_shlx: res = (v << c) & mask; break;
    default:      res = v >> c; break;   /* shrx */
    }
    put_gpr_val(st, d->dsts[0], res);
}
static void gpr_rorx(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 : 32;
    uint64_t mask = (w == 64) ? ~0ull : 0xFFFFFFFFull;
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t c = get_gpr_val(st, d->srcs[1]) & ((w == 64) ? 63 : 31);
    uint64_t res = (w == 32)
        ? (uint64_t)(((uint32_t)v >> (int)c) | ((uint32_t)v << (int)((32 - c) & 31)))
        : ((v >> c) | (v << ((64 - c) & 63)));
    put_gpr_val(st, d->dsts[0], res);
}
static void gpr_mulx(estate_t *st, emu_desc_t *d)
{
    /* MULX: vvvv (DR dsts[0]) ← МЛАДШАЯ половина RDX*src,
     *       modrm.reg (DR dsts[1]) ← старшая. Проверено нативным
     * дизасмом gcc (_mulx_u64): vvvv=lo, reg=hi (§7.3, SWAP-баг v0.2:
     * got/want менялись местами во всех 400 KAT mulx).            */
    uint64_t rdx = *EG(st, 2);  /* RDX */
    uint64_t src = get_gpr_val(st, d->srcs[0]);
    int is32 = (opnd_get_size(d->srcs[0]) == OPSZ_4);
    uint64_t lo, hi;
    if (is32) {
        uint64_t pr = (uint64_t)((uint32_t)rdx * (uint32_t)src);
        lo = pr & 0xFFFFFFFF; hi = pr >> 32;
    } else {
        mul64x64(rdx, src, &lo, &hi);
    }
    put_gpr_val(st, d->dsts[0], lo);
    put_gpr_val(st, d->dsts[1], hi);
}
static void gpr_adcx_adox(estate_t *st, emu_desc_t *d)
{
    int is_adox = (d->opc == OP_adox);
    uint64_t a = get_gpr_val(st, d->dsts[0]);
    uint64_t b = get_gpr_val(st, d->srcs[0]);
    int is32 = (opnd_get_size(d->dsts[0]) == OPSZ_4);
    uint64_t mask = is32 ? 0xFFFFFFFFull : ~0ull;
    uint32_t fl = flags_get(st);
    uint64_t cin = is_adox ? ((fl >> 11) & 1) : (fl & 1);
    uint64_t res = (a & mask) + (b & mask) + cin;
    uint64_t carry = res >> (is32 ? 32 : 64);
    if (is_adox) {
        int of = 0;
        if (is32) {
            uint32_t r32 = (uint32_t)res;
            of = ((~((uint32_t)a ^ (uint32_t)b)) & ((uint32_t)a ^ r32)) >> 31;
        } else {
            of = (int)(((~(a ^ b)) & (a ^ res)) >> 63);
        }
        flags_setmask(st, of ? FOF : 0, FOF);
    } else {
        flags_setmask(st, carry ? FCF : 0, FCF);
    }
    put_gpr_val(st, d->dsts[0], res & mask);
}
/* ══════════════════ POPCNT / LZCNT / TZCNT ══════════════════ */
#ifdef _MSC_VER
# define PASS_POPCNT64(x) __popcnt64(x)
static inline int pass_clz64(uint64_t v)   { unsigned long i; _BitScanReverse64(&i, v); return 63 - (int)i; }
static inline int pass_ctz64(uint64_t v)   { unsigned long i; _BitScanForward64(&i, v); return (int)i; }
#else
# define PASS_POPCNT64(x) __builtin_popcountll(x)
static inline int pass_clz64(uint64_t v)   { return __builtin_clzll(v); }
static inline int pass_ctz64(uint64_t v)   { return __builtin_ctzll(v); }
#endif

/* Канонические флаги LZCNT/TZCNT (F3-класс):
 * OF=AF=0; CF=1 при нулевом источнике; ZF=(рез==0); SF=старший бит результата.
 * PF: реальные CPU (наблюдение на Skylake-сервере) НЕ выставляют чётность
 * для lzcnt/tzcnt — держим PF=0 ради неотличимости от нативного исполнения.
 * Результат нул-расширяется до 64 бит.                                  */
static void flags_count(estate_t *st, uint64_t res, int width, int cf)
{
    uint32_t fl = 0;
    if (res == 0) fl |= FZF;
    if (cf)       fl |= FCF;
    if ((res >> (width - 1)) & 1) fl |= FSF;
    flags_setmask(st, fl, FCF|FPF|FAF|FZF|FSF|FOF);
}

static void gpr_popcnt64(estate_t *st, emu_desc_t *d)
{
    uint64_t v = get_gpr_val(st, d->srcs[0]);
    int w = (opnd_get_size(d->srcs[0]) == OPSZ_8) ? 64 :
            (opnd_get_size(d->srcs[0]) == OPSZ_4 ? 32 : 16);
    uint64_t mask = (w == 64) ? ~0ull : ((1ull << w) - 1);
    uint64_t res = PASS_POPCNT64(v & mask);
    /* результат — нул-расширенный до полного регистра */
    put_gpr_val(st, opnd_create_reg(gpr_norm(opnd_get_reg(d->dsts[0]))), res);
    flags_count(st, res, w, 0);
}

static void gpr_lzcnt(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 :
            (opnd_get_size(d->dsts[0]) == OPSZ_4 ? 32 : 16);
    uint64_t mask = (w == 64) ? ~0ull : ((1ull << w) - 1);
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t res;
    int cf = (v == 0);
    if (cf)
        res = (uint64_t)w;                          /* LZCNT(0) = ширина */
    else
        res = (uint64_t)(pass_clz64(v) - (64 - w)); /* clz в разрядности w */
    put_gpr_val(st, opnd_create_reg(gpr_norm(opnd_get_reg(d->dsts[0]))), res);
    flags_count(st, res, w, cf);
}

static void gpr_tzcnt(estate_t *st, emu_desc_t *d)
{
    int w = (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 64 :
            (opnd_get_size(d->dsts[0]) == OPSZ_4 ? 32 : 16);
    uint64_t mask = (w == 64) ? ~0ull : ((1ull << w) - 1);
    uint64_t v = get_gpr_val(st, d->srcs[0]) & mask;
    uint64_t res;
    int cf = (v == 0);
    if (cf)
        res = (uint64_t)w;                          /* TZCNT(0) = ширина */
    else
        res = (uint64_t)pass_ctz64(v);
    put_gpr_val(st, opnd_create_reg(gpr_norm(opnd_get_reg(d->dsts[0]))), res);
    flags_count(st, res, w, cf);
}

/* ══════════════════ VTESTPS/PD ══════════════════ */
static void vtest(estate_t *st, const uint8_t *d, const uint8_t *s, int is_pd)
{
    int zf = 1, cf = 1;
    int n = is_pd ? 2 : 4;
    for (int i = 0; i < n; i++) {
        uint64_t a = ld64(d + i*8), b = ld64(s + i*8);
        if ((a & b) >> 63) zf = 0;
        if ((a & ~b) >> 63) cf = 0;
    }
    flags_setmask(st, (zf ? FZF : 0) | (cf ? FCF : 0), FCF|FZF|FSF|FOF|FAF|FPF);
}

/* ══════════════════ диспетчер emu_generic ══════════════════ */

/* получить блок s2 (регистр или память) для блочной операции */
static const uint8_t *src2_block(estate_t *st, emu_desc_t *d, uint8_t *tmp)
{
    if (d->nsrc >= 2) return get_block16(st, d->srcs[1], tmp);
    return tmp;
}
static const uint8_t *src1_block(estate_t *st, emu_desc_t *d, uint8_t *tmp)
{
    if (d->nsrc >= 1) return get_block16(st, d->srcs[0], tmp);
    return tmp;
}

/* pd vs ps у упакованных FMA-форм */
static int fma_is_pd(int opc)
{
    switch (opc) {
    case OP_vfmadd132pd: case OP_vfmadd213pd: case OP_vfmadd231pd:
    case OP_vfmsub132pd: case OP_vfmsub213pd: case OP_vfmsub231pd:
    case OP_vfnmadd132pd: case OP_vfnmadd213pd: case OP_vfnmadd231pd:
    case OP_vfnmsub132pd: case OP_vfnmsub213pd: case OP_vfnmsub231pd:
    case OP_vfmaddsub132pd: case OP_vfmaddsub213pd: case OP_vfmaddsub231pd:
    case OP_vfmsubadd132pd: case OP_vfmsubadd213pd: case OP_vfmsubadd231pd:
        return 1;
    default:
        return 0;
    }
}

/* обработка одного FMA-случая (см. свитч emu_generic): 16Б нижняя половина
 * + 16Б старшая (если ymm); для 132/213 произведение — старый dst */
#define fma_case(kind_, alt_, form_)                                        \
    do {                                                                    \
        int di_ = xmm_idx(opnd_get_reg(d->dsts[0]));                        \
        uint8_t D_[32], S1_[32], S2_[32];                                   \
        get_block32(&st, d->dsts[0], D_);                                   \
        get_block32(&st, d->srcs[0], S1_);                                  \
        get_block32(&st, d->srcs[1], S2_);                                  \
        fma_block(D_, S1_, S2_, (kind_), fma_is_pd(d->opc), (alt_),         \
                  (form_));                                                 \
        if (d->is_ymm) {                                                    \
            fma_block(D_ + 16, S1_ + 16, S2_ + 16, (kind_),                 \
                      fma_is_pd(d->opc), (alt_), (form_));                  \
            memcpy(EY(&st, di_), D_ + 16, 16);                              \
        } else if (d->is_vex && opnd_is_reg(d->dsts[0])) {                  \
            memset(EY(&st, di_), 0, 16);   /* VEX.128: верх YMM в ноль */   \
        }                                                                   \
        memcpy(EX(&st, di_), D_, 16);                                       \
    } while (0)

int emu_generic(emu_desc_t *d)
{
    /* §7.3: обёртка прозрачности фолтов — emu_safe_read/write при
     * недоступной памяти приложения записывают адрес/тип в RAW_FAULT/
     * RAW_TG и longjmp'ят сюда; сгенерированный код после clean call
     * исполнит настоящий доступ по этому адресу уже в кэше (см.
     * rewrite.c), и фолт доставится приложению как при нативном
     * исполнении. */
    void *dc0 = dr_get_current_drcontext();
    uint8_t *buf0 = (uint8_t *)drmgr_get_tls_field(dc0, g_tls_idx_buf);
    if (buf0 != NULL) {
        *(uint64_t *)(buf0 + BUF_TG) = 0;
        *(uint64_t *)(buf0 + BUF_FAULT) = 0;
    }
    /* app-флаги → BUF_FLAGS всегда: их восстанавливает безусловный
     * (6)-рестор (прокси (5b) портит флаги test/shr) */
    {
        uint8_t *buf0 = (uint8_t *)drmgr_get_tls_field(dc0, g_tls_idx_buf);
        dr_mcontext_t mc0;
        memset(&mc0, 0, sizeof(mc0));
        mc0.size = sizeof(mc0);
        mc0.flags = DR_MC_CONTROL;
        if (buf0 != NULL && dr_get_mcontext(dc0, &mc0))
            *(uint32_t *)(buf0 + BUF_FLAGS) = (uint32_t)mc0.xflags;
    }
    if (setjmp(g_emu_jmp) != 0)
        return 1;              /* фолт записан — эмуляция прервана */
    return emu_generic_impl(d);
}

int emu_generic_impl(emu_desc_t *d)
{
    void *dc = dr_get_current_drcontext();
    estate_t st;
    pass_ensure_tls(dc);
    st.buf = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_buf);
    st.raw = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_raw);
    st.pc_next = d->pc_next;

    if (d->opc < OPC_MAX) {
        PASS_ATOMIC_INC32(&g_stat_generic[d->opc]);
    }
    PASS_ATOMIC_INC32(&g_dyn_generic);
    /* §7.3 diag (PASS_CMPBUF=1): сверить спилы в BUF с фактическим
     * контекстом приложения. Спилы-инструкции вставлены ДО clean call
     * и регистров не меняют, поэтому app-контекст (сохранённый DR-шным
     * прологом clean call при save_fpstate=true) обязан совпадать с
     * BUF. Расхождение = спил не достиг буфера (carrier-десинк) или
     * ушёл мимо BUF. Сравниваем только спиленные слоты: при fp=false
     * mc.simd невалиден.                                                  */
    {
        static int cmpbuf = -1;
        if (cmpbuf == -1)
            cmpbuf = (getenv("PASS_CMPBUF") != NULL) ? 1 : 0;
        if (cmpbuf && (d->xmm_src | d->xmm_dst) != 0) {
            dr_mcontext_t mc;
            memset(&mc, 0, sizeof(mc));
            mc.size = sizeof(mc);
            mc.flags = (dr_mcontext_flags_t)DR_MC_ALL;
            if (dr_get_mcontext(dc, &mc)) {
                for (int k = 0; k < 16; k++) {
                    if (!((d->xmm_src | d->xmm_dst) & (1u << k))) continue;
                    if (memcmp(st.buf + BUF_XMM(k), &mc.simd[k], 16) != 0) {
                        const uint8_t *bv = st.buf + BUF_XMM(k);
                        const uint8_t *mv = (const uint8_t *)&mc.simd[k];
                        dr_fprintf(STDERR,
                                   "[CMPBUF] opc=%d MISMATCH xmm%d\n"
                                   "  BUF: %02x%02x%02x%02x%02x%02x%02x%02x"
                                   "%02x%02x%02x%02x%02x%02x%02x%02x\n"
                                   "  MC : %02x%02x%02x%02x%02x%02x%02x%02x"
                                   "%02x%02x%02x%02x%02x%02x%02x%02x\n",
                                   d->opc, k,
                                   bv[0], bv[1], bv[2], bv[3], bv[4], bv[5],
                                   bv[6], bv[7], bv[8], bv[9], bv[10], bv[11],
                                   bv[12], bv[13], bv[14], bv[15],
                                   mv[0], mv[1], mv[2], mv[3], mv[4], mv[5],
                                   mv[6], mv[7], mv[8], mv[9], mv[10], mv[11],
                                   mv[12], mv[13], mv[14], mv[15]);
                    }
                }
            }
        }
    }
    /* §6.24: прогресс-счётчик — отличить «медленно, но работает» от
     * бесконечного цикла: растёт — идёт работа, стоит — зацикливание */
    if (g_cfg.verbose >= 1 && (g_dyn_generic % 500000) == 0)
        dr_fprintf(STDERR, "[P.A.S.S.] generic: %llu эмуляций\n",
                   (unsigned long long)g_dyn_generic);
    if (g_cfg.verbose >= 5) {
        dr_fprintf(STDERR, "[P.A.S.S.][BUF] opc=%d xmm0-3:", d->opc);
        for (int k = 0; k < 4; k++) {
            const uint8_t *b = st.buf + BUF_XMM(k);
            dr_fprintf(STDERR, " %02x%02x%02x%02x", b[0], b[1], b[2], b[3]);
        }
        dr_fprintf(STDERR, " xmm6:");
        {
            const uint8_t *b = st.buf + BUF_XMM(6);
            dr_fprintf(STDERR, " %02x%02x%02x%02x%02x%02x%02x%02x",
                       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
        }
        dr_fprintf(STDERR, "\n");
    }
    uint8_t t1[16], t2[16], imm = 0;
    if (d->nsrc >= 1 && opnd_is_immed_int(d->srcs[d->nsrc - 1]))
        imm = (uint8_t)opnd_get_immed_int(d->srcs[d->nsrc - 1]);

    switch (d->opc) {
    /* ── блочные 16Б/32Б ── */
    case OP_pcmpgtq: case OP_pcmpeqq: case OP_pmulld: case OP_pmuldq:
    case OP_packusdw: case OP_phminposuw: case OP_mpsadbw:
    case OP_roundps: case OP_roundpd: case OP_roundss: case OP_roundsd:
    case OP_dpps: case OP_dppd:
    case OP_blendps: case OP_blendpd: case OP_vpblendd:
    case OP_pminub: case OP_pmaxub: case OP_pminuw: case OP_pmaxuw:
    case OP_pminud: case OP_pmaxud: case OP_pminsb: case OP_pmaxsb:
    case OP_pminsw: case OP_pmaxsw: case OP_pminsd: case OP_pmaxsd:
    case OP_pmovsxbw: case OP_pmovsxbd: case OP_pmovsxbq:
    case OP_pmovsxwd: case OP_pmovsxwq: case OP_pmovsxdq:
    case OP_pmovzxbw: case OP_pmovzxbd: case OP_pmovzxbq:
    case OP_pmovzxwd: case OP_pmovzxwq: case OP_pmovzxdq:
    case OP_vpsllvd: case OP_vpsrlvd: case OP_vpsravd:
    case OP_vpsllvq: case OP_vpsrlvq: case OP_vpsravq:
    case OP_insertps:
    case OP_aesenc: case OP_aesdec: case OP_aesenclast: case OP_aesdeclast:
    case OP_aesimc: case OP_aeskeygenassist:
    case OP_pclmulqdq: case OP_vpclmulqdq: {
        void (*fn)(uint8_t *, const uint8_t *, const uint8_t *, uint) = NULL;
        uint8_t S1[32], S2[32], D[32];
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        /* источники: reg → EX+EY, mem → чтение 32/16/4Б по размеру операнда */
        get_block32(&st, d->srcs[0], S1);
        if (d->nsrc >= 2 && !opnd_is_immed_int(d->srcs[1]))
            get_block32(&st, d->srcs[1], S2);
        else
            memset(S2, 0, 32);
        memcpy(D, EX(&st, di), 16);
        if (d->is_ymm) memcpy(D + 16, EY(&st, di), 16);

        switch (d->opc) {
        case OP_pcmpgtq:  fn = b_pcmpgtq;  break;
        case OP_pcmpeqq:  fn = b_pcmpeqq;  break;
        case OP_pmulld:   fn = b_pmulld;   break;
        case OP_pmuldq:   fn = b_pmuldq;   break;
        case OP_packusdw: fn = b_packusdw; break;
        case OP_phminposuw: fn = b_phminposuw; break;
        case OP_mpsadbw:  fn = b_mpsadbw;  break;
        case OP_roundps:  fn = b_roundps;  break;
        case OP_roundpd:  fn = b_roundpd;  break;
        case OP_roundss:  fn = b_roundss;  break;
        case OP_roundsd:  fn = b_roundsd;  break;
        case OP_dpps:     fn = b_dpps;     break;
        case OP_dppd:     fn = b_dppd;     break;
        case OP_blendps:  fn = b_blendps;  break;
        case OP_blendpd:  fn = b_blendpd;  break;
        case OP_vpblendd: fn = b_vpblendd; break;
        case OP_pminub: fn = b_pminub; break;
        case OP_pmaxub: fn = b_pmaxub; break;
        case OP_pminuw: fn = b_pminuw; break;
        case OP_pmaxuw: fn = b_pmaxuw; break;
        case OP_pminud: fn = b_pminud; break;
        case OP_pmaxud: fn = b_pmaxud; break;
        case OP_pminsb: fn = b_pminsb; break;
        case OP_pmaxsb: fn = b_pmaxsb; break;
        case OP_pminsw: fn = b_pminsw; break;
        case OP_pmaxsw: fn = b_pmaxsw; break;
        case OP_pminsd: fn = b_pminsd; break;
        case OP_pmaxsd: fn = b_pmaxsd; break;
        case OP_pmovsxbw: fn = b_pmovsxbw; break;
        case OP_pmovsxbd: fn = b_pmovsxbd; break;
        case OP_pmovsxbq: fn = b_pmovsxbq; break;
        case OP_pmovsxwd: fn = b_pmovsxwd; break;
        case OP_pmovsxwq: fn = b_pmovsxwq; break;
        case OP_pmovsxdq: fn = b_pmovsxdq; break;
        case OP_pmovzxbw: fn = b_pmovzxbw; break;
        case OP_pmovzxbd: fn = b_pmovzxbd; break;
        case OP_pmovzxbq: fn = b_pmovzxbq; break;
        case OP_pmovzxwd: fn = b_pmovzxwd; break;
        case OP_pmovzxwq: fn = b_pmovzxwq; break;
        case OP_pmovzxdq: fn = b_pmovzxdq; break;
        case OP_vpsllvd: fn = b_vpsllvd; break;
        case OP_vpsrlvd: fn = b_vpsrlvd; break;
        case OP_vpsravd: fn = b_vpsravd; break;
        case OP_vpsllvq: fn = b_vpsllvq; break;
        case OP_vpsrlvq: fn = b_vpsrlvq; break;
        case OP_vpsravq: fn = b_vpsravq; break;
        case OP_insertps: fn = b_insertps; break;
        default: break;
        }

        if (d->opc == OP_aesenc || d->opc == OP_aesdec ||
            d->opc == OP_aesenclast || d->opc == OP_aesdeclast ||
            d->opc == OP_aesimc) {
            /* §7.3 SWAP: порядок источников различается между формами!
             * legacy (деструктивная 2-op): DR srcs = [Wdq(rm), Vdq(reg)]
             *   → srcs[0] = КЛЮЧ (rm), srcs[1] = СОСТОЯНИЕ (reg).
             * VEX (vaesenc, сюда ведёт и trap-fallback): srcs = [Hx, Wx]
             *   → srcs[0] = СОСТОЯНИЕ, srcs[1] = КЛЮЧ.
             * Без различения: legacy считал aesenc(ключ, сотс.), VEX —
             * наоборот. KAT: selftest (vaesenc) и aestest (aesenc).    */
            if (d->is_vex)
                aes_block(D, S1, S2, d->opc);
            else
                aes_block(D, S2, S1, d->opc);
            if (d->is_ymm) aes_block(D + 16, S1 + 16, S2 + 16, d->opc);
        } else if (d->opc == OP_aeskeygenassist) {
            aes_keygen(D, S1, imm);
            if (d->is_ymm) aes_keygen(D + 16, S1 + 16, imm);
        } else if (d->opc == OP_pclmulqdq || d->opc == OP_vpclmulqdq) {
            pclmul_block(D, S1, S2, imm);
            if (d->is_ymm) pclmul_block(D + 16, S1 + 16, S2 + 16, imm);
        } else if (fn != NULL) {
            fn(D, S1, S2, imm);
            if (d->is_ymm) fn(D + 16, S1 + 16, S2 + 16, imm);
        } else {
            break;
        }
        memcpy(EX(&st, di), D, 16);
        if (d->is_ymm) memcpy(EY(&st, di), D + 16, 16);
        else if (d->is_vex) memset(EY(&st, di), 0, 16); /* VEX.128: верх в ноль */
        break;
    }

    /* ── extract/pinsr: смешанные GPR/xmm ── */
    case OP_extractps: {
        /* dst GPR/mem ← src xmm dword imm */
        const uint8_t *sv = EX(&st, xmm_idx(opnd_get_reg(d->srcs[0])));
        uint32_t v = ld32(sv + (imm & 3) * 4);
        put_gpr_val(&st, d->dsts[0], v);
        break;
    }
    case OP_pextrb: case OP_pextrw: case OP_pextrd:
    case OP_vpextrb: case OP_vpextrw: case OP_vpextrd: {   /* vpextrq = vpextrd */
        const uint8_t *sv = EX(&st, xmm_idx(opnd_get_reg(d->srcs[0])));
        if (g_cfg.verbose >= 4)
            dr_fprintf(STDERR, "[P.A.S.S.][PEXTR] opc=%d imm=%u src=%02x%02x%02x%02x%02x%02x%02x%02x\n",
                       d->opc, imm, sv[0], sv[1], sv[2], sv[3],
                       sv[4], sv[5], sv[6], sv[7]);
        uint64_t v = 0;
        /* §6.23: размер ЭЛЕМЕНТА — по опкоду, не по размеру dst:
         * pextrw/pextrb пишут в 32-битный GPR (нул-расширение), но
         * элемент — слово/байт; прежний код брал OPSZ_4 у r32 и читал
         * dword: pextrw $3 возвращал 0 вместо слова (срыв bun.exe). */
        int esz = (d->opc == OP_pextrb || d->opc == OP_vpextrb) ? 1
                : (d->opc == OP_pextrw || d->opc == OP_vpextrw) ? 2
                : (opnd_get_size(d->dsts[0]) == OPSZ_8) ? 8 : 4;
        switch (esz) {
        case 1: v = sv[imm & 15]; break;
        case 2: v = ld16(sv + (imm & 7) * 2); break;
        case 4: v = ld32(sv + (imm & 3) * 4); break;
        case 8: v = ld64(sv + (imm & 1) * 8); break;
        }
        /* §7.3: mem-приёмник пишем размером ЭЛЕМЕНТА (по опкоду), не по
         * opnd_get_size — DR отдаёт mem-операнду OPSZ_2, и st16 затирал
         * СОСЕДНИЙ байт out[i+1] нулём (перепутанный KAT vpextrb-сторов). */
        if (opnd_is_memory_reference(d->dsts[0])) {
            byte *a = mem_addr(&st, d->dsts[0]);
            int wes = (d->opc == OP_pextrb || d->opc == OP_vpextrb) ? 1
                    : (d->opc == OP_pextrw || d->opc == OP_vpextrw) ? 2 : 4;
            uint64_t tmp = (uint64_t)v;   /* §7.3: запись размером элемента */
            emu_safe_write(&st, a, &tmp, wes);
        } else {
            put_gpr_val(&st, d->dsts[0], v);
        }
        break;
    }
    case OP_pinsrb: case OP_pinsrd:   /* OP_pinsrq == OP_pinsrd (алиас) */
    case OP_vpinsrb: case OP_vpinsrw: case OP_vpinsrd: { /* vpinsrq = vpinsrd */
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t *dv = EX(&st, di);
        /* §7.3: порядок srcs зависит от формы!
         * VEX (DR: Vdq ← H15_dq, Rd_Mb, Ib): srcs[0] = xmm-БАЗА слияния,
         *   srcs[1] = GPR/mem-источник, imm = srcs[2].
         * legacy (Vdq ← Rd_Mb, Ib): srcs[0] = GPR/mem, база = старый dst.
         * Прежний код читал srcs[0] как GPR и в VEX-форме получал мусор:
         * vpinsrb возвращал 0x25-заполненный регистр (срыв bun.exe).  */
        opnd_t gsrc = d->is_vex ? d->srcs[1] : d->srcs[0];
        uint64_t v;
        if (d->is_vex) {
            /* база слияния — xmm-операнд srcs[0]; копируем в dv (dst
             * может не совпадать с базой: vpinsrb xmm0, xmm4, eax, 5) */
            memcpy(dv, EX(&st, xmm_idx(opnd_get_reg(d->srcs[0]))), 16);
        }
        int esz = (d->opc == OP_pinsrb || d->opc == OP_vpinsrb) ? 1
                : (d->opc == OP_pinsrw || d->opc == OP_vpinsrw) ? 2
                : (opnd_get_size(gsrc) == OPSZ_8) ? 8 : 4;
        if (opnd_is_memory_reference(gsrc)) {
            const uint8_t *mp = mem_addr(&st, gsrc);
            uint64_t v64 = 0;
            emu_safe_read(&st, (void *)mp, &v64, esz);
            v = v64;
        } else {
            v = get_gpr_val(&st, gsrc);
        }
        if (esz == 1)           dv[imm & 15] = (uint8_t)v;
        else if (esz == 2)      st16(dv + (imm & 7) * 2, (uint16_t)v);
        else if (esz == 8)      st64(dv + (imm & 1) * 8, v);
        else                    st32(dv + (imm & 3) * 4, (uint32_t)v);
        if (d->is_vex) memset(EY(&st, di), 0, 16);      /* VEX.128 */
        break;
    }

    /* ── PTEST / VTEST ── источник может быть памятью (y/xmm vs m256/m128)!
     * Читаем через get_block32: reg → EX+EY, mem → чтение по размеру.     */
    case OP_ptest: {
        uint8_t s1b[32], s2v[32];
        get_block32(&st, d->dsts[0], s1b);          /* dst — всегда reg */
        get_block32(&st, d->srcs[1], s2v);          /* reg или память */
        if (d->is_ymm) {
            int zf = 1, cf = 1;
            for (int i = 0; i < 32; i++) {
                if (s1b[i] & s2v[i]) zf = 0;
                if (~s1b[i] & s2v[i]) cf = 0;
            }
            flags_setmask(&st, (zf ? FZF : 0) | (cf ? FCF : 0), FCF|FZF|FSF|FOF|FAF|FPF);
        } else {
            b_ptest(&st, s1b, s2v);
        }
        break;
    }
    case OP_vtestps: case OP_vtestpd: {
        uint8_t s1b[32], s2v[32];
        get_block32(&st, d->dsts[0], s1b);
        get_block32(&st, d->srcs[1], s2v);
        vtest(&st, s1b, s2v, d->opc == OP_vtestpd);
        break;
    }

    /* ── переменные blendv ── */
    case OP_blendvps: case OP_blendvpd: {
        /* src2 может быть памятью: берём полные 32Б через get_block32,
         * младшая половина — s2v, старшая — s2v+16 (для reg это EY(idx)). */
        uint8_t s2v[32];
        uint8_t *dv = EX(&st, xmm_idx(opnd_get_reg(d->dsts[0])));
        get_block32(&st, d->srcs[1], s2v);
        const uint8_t *m = EX(&st, 0);
        if (d->opc == OP_blendvps) b_blendvps_m(dv, dv, s2v, m);
        else                        b_blendvpd_m(dv, dv, s2v, m);
        if (d->is_ymm) {
            int di = xmm_idx(opnd_get_reg(d->dsts[0]));
            if (d->opc == OP_blendvps) b_blendvps_m(EY(&st, di), EY(&st, di), s2v + 16, EY(&st, 0));
            else                        b_blendvpd_m(EY(&st, di), EY(&st, di), s2v + 16, EY(&st, 0));
        } else if (d->is_vex) {
            int di = xmm_idx(opnd_get_reg(d->dsts[0]));
            memset(EY(&st, di), 0, 16);                     /* VEX.128 */
        }
        break;
    }
    case OP_pblendvb: {
        uint8_t *dv = EX(&st, xmm_idx(opnd_get_reg(d->dsts[0])));
        const uint8_t *s2b = src2_block(&st, d, t2);
        const uint8_t *m = EX(&st, 0);
        b_pblendvb_m(dv, dv, s2b, m);
        break;
    }

    /* ── 32-байтные cross-lane ── источники могут быть памятью (BUG FIX:
     * vpermq $i, m256, ymm раньше читал тень по мусорному индексу из
     * opnd_get_reg(mem) → SIGSEGV/тихая порча, см. ARCHITECTURE §6.13) */
    case OP_vpermq: {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t s[32];
        get_block32(&st, d->srcs[0], s);            /* reg или память */
        wide_vpermq(s /* пишет в локальный буфер — безопасно */, s, imm);
        memcpy(EX(&st, di), s, 16);
        memcpy(EY(&st, di), s + 16, 16);
        break;
    }
    case OP_vpermd: {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t idxs[32], tab[32];
        get_block32(&st, d->srcs[0], idxs);
        get_block32(&st, d->srcs[1], tab);
        wide_vpermd(idxs, idxs, tab);
        memcpy(EX(&st, di), idxs, 16);
        memcpy(EY(&st, di), idxs + 16, 16);
        break;
    }
    case OP_vperm2f128: case OP_vperm2i128: {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t s1[32], s2[32], r[32];
        get_block32(&st, d->srcs[0], s1);
        get_block32(&st, d->srcs[1], s2);
        wide_vperm2x128(r, s1, s2, imm);
        memcpy(EX(&st, di), r, 16);
        memcpy(EY(&st, di), r + 16, 16);
        break;
    }
    case OP_vcvtps2pd: {
        /* src xmm/m128 16Б → dst ymm 32Б */
        if (d->is_ymm) {
            uint8_t s16[16];
            int di = xmm_idx(opnd_get_reg(d->dsts[0]));
            get_block16(&st, d->srcs[0], s16);      /* reg или память */
            wide_cvtps2pd(EX(&st, di), s16);
            break;
        }
        /* 128-битная форма: cvtps2pd нативна — сюда не попадаем */
        break;
    }
    case OP_vcvtpd2ps: {
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        if (d->is_ymm) {
            uint8_t s[32];
            get_block32(&st, d->srcs[0], s);        /* reg или память */
            wide_cvtpd2ps(EX(&st, di), s);
        } else {
            uint8_t s16[16];
            get_block16(&st, d->srcs[0], s16);
            wide_cvtpd2ps(EX(&st, di), s16);
        }
        break;
    }

    /* ── FMA (двойное округление; 256-битных форм нет в inline-пути
     * rewrite_special — сюда ведут generic-диспетчер и trap-фоллбэк).
     * kind: 0=fmadd 1=fmsub 2=fnmadd 3=fnmsub; alt: 1=fmaddsub 2=fmsubadd;
     * f231: произведение s1*s2 и слагаемое dst.                     ── */
    case OP_vfmadd132ps: case OP_vfmadd132pd: fma_case(0, 0, 2); break;
    case OP_vfmadd213ps: case OP_vfmadd213pd: fma_case(0, 0, 0); break;
    case OP_vfmadd231ps: case OP_vfmadd231pd: fma_case(0, 0, 1); break;
    case OP_vfmsub132ps: case OP_vfmsub132pd: fma_case(1, 0, 2); break;
    case OP_vfmsub213ps: case OP_vfmsub213pd: fma_case(1, 0, 0); break;
    case OP_vfmsub231ps: case OP_vfmsub231pd: fma_case(1, 0, 1); break;
    case OP_vfnmadd132ps: case OP_vfnmadd132pd: fma_case(2, 0, 2); break;
    case OP_vfnmadd213ps: case OP_vfnmadd213pd: fma_case(2, 0, 0); break;
    case OP_vfnmadd231ps: case OP_vfnmadd231pd: fma_case(2, 0, 1); break;
    case OP_vfnmsub132ps: case OP_vfnmsub132pd: fma_case(3, 0, 2); break;
    case OP_vfnmsub213ps: case OP_vfnmsub213pd: fma_case(3, 0, 0); break;
    case OP_vfnmsub231ps: case OP_vfnmsub231pd: fma_case(3, 0, 1); break;
    case OP_vfmaddsub132ps: case OP_vfmaddsub132pd: fma_case(0, 1, 2); break;
    case OP_vfmaddsub213ps: case OP_vfmaddsub213pd: fma_case(0, 1, 0); break;
    case OP_vfmaddsub231ps: case OP_vfmaddsub231pd: fma_case(0, 1, 1); break;
    case OP_vfmsubadd132ps: case OP_vfmsubadd132pd: fma_case(0, 2, 2); break;
    case OP_vfmsubadd213ps: case OP_vfmsubadd213pd: fma_case(0, 2, 0); break;
    case OP_vfmsubadd231ps: case OP_vfmsubadd231pd: fma_case(0, 2, 1); break;

    /* ── vpmaskmov / gather ── */
    case OP_vpmaskmovd: case OP_vpmaskmovq:
        pmaskmov_op(&st, d, 0);
        break;
    case OP_vpgatherdd: case OP_vpgatherdq: case OP_vpgatherqd:
    case OP_vpgatherqq: case OP_vgatherdps: case OP_vgatherdpd:
    case OP_vgatherqps: case OP_vgatherqpd:
        gather_op(&st, d);
        break;

    /* ── строковые ── */
    case OP_pcmpestri: string_op(&st, d, 1, 1); break;
    case OP_pcmpestrm: string_op(&st, d, 1, 0); break;
    case OP_pcmpistri: string_op(&st, d, 0, 1); break;
    case OP_pcmpistrm: string_op(&st, d, 0, 0); break;

    /* ── vzeroupper / vzeroall ── */
    case OP_vzeroupper:
        for (int i = 0; i < 16; i++) memset(EY(&st, i), 0, 16);
        break;
    case OP_vzeroall:
        for (int i = 0; i < 16; i++) { memset(EX(&st, i), 0, 16); memset(EY(&st, i), 0, 16); }
        break;

    /* ── GPR ── */
    case OP_crc32:  gpr_crc32(&st, d); break;
    case OP_pext:   gpr_pext(&st, d); break;
    case OP_pdep:   gpr_pdep(&st, d); break;
    case OP_bzhi:   gpr_bzhi(&st, d); break;
    case OP_bextr:  gpr_bextr(&st, d); break;
    case OP_mulx:   gpr_mulx(&st, d); break;
    case OP_adcx: case OP_adox: gpr_adcx_adox(&st, d); break;
    case OP_popcnt: gpr_popcnt64(&st, d); break;
    case OP_lzcnt:  gpr_lzcnt(&st, d); break;
    case OP_tzcnt:  gpr_tzcnt(&st, d); break;
    case OP_andn:   gpr_andn(&st, d); break;
    case OP_blsi:   gpr_blsi(&st, d); break;
    case OP_blsmsk: gpr_blsmsk(&st, d); break;
    case OP_blsr:   gpr_blsr(&st, d); break;
    case OP_sarx: case OP_shlx: case OP_shrx: gpr_bmi2shift(&st, d); break;
    case OP_rorx:   gpr_rorx(&st, d); break;

    default:
        PASS_ATOMIC_INC32(&g_stat_unsupported[d->opc < OPC_MAX ? d->opc : 0]);
        return 0;   /* не обработано — trap не должен «глотать» инструкцию */
    }
    return 1;
}

/* vzeroupper/vzeroall без спилов (быстрый путь) */
void emu_vzeroupper(void)
{
    void *dc = dr_get_current_drcontext();
    pass_ensure_tls(dc);
    uint8_t *raw = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_raw);
    if (raw) memset(raw + RAW_YHI(0), 0, 16 * 16);
    if (g_cfg.verbose >= 3)
        dr_fprintf(STDERR, "[P.A.S.S.] vzeroupper emu ok\n");
}

void emu_vzeroall(void)
{
    void *dc = dr_get_current_drcontext();
    pass_ensure_tls(dc);
    uint8_t *raw = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_raw);
    uint8_t *buf = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_buf);
    if (raw) memset(raw + RAW_YHI(0), 0, 16 * 16);
    if (buf) memset(buf + BUF_XMM(0), 0, 16 * 16);   /* xmm-образ — клиент перезагрузит */
}
