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
/* §7.3: setjmp/longjmp НА WINDOWS УДАЛЕНЫ. MSVC x64 ЛЮБОЙ setjmp
 * (включая _setjmp) строится на SEH-фреймворке (0x19930522-магия,
 * client RVA 0x2470b); под DR clean call регистрация EH-фрейма в
 * нестандартном контексте отравляет SEH-цепочку потока — первый же
 * exception идёт в наш фильтр и падает в нём (internal crash без
 * [EXC]). Альтернатива — abort БЕЗ unwind: emu_safe_* при неудаче
 * лишь ЗАПИСЫВАЮТ фолт (первый выигрывает) и продолжают: дальнейшие
 * доступы тоже safe (фолтов больше нет), мусор в BUF отбрасывается
 * прокси-путём доставки. Linux-сборка использует тот же механизм. */
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
static int emu_generic_impl(emu_desc_t *d);

static void emu_fault_record(estate_t *st, byte *addr, int is_write)
{
    /* §7.3: слоты в BUF (r10-относительные), а не в RAW (gs): в контексте
     * доставки сигнала gs-база ненадёжна, а r10-носитель жив до (7).
     * Записывается ПЕРВЫЙ фолт (BUF_TG==0); без longjmp — см. шапку. */
    if (st->buf != NULL && *(uint64_t *)(st->buf + BUF_TG) == 0) {
        *(uint64_t *)(st->buf + BUF_TG) = 1u | ((uint64_t)(is_write & 1) << 1);
        *(uint64_t *)(st->buf + BUF_FAULT) = (uint64_t)(uintptr_t)addr;
    }
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

/* §7.3 run #106 (C): легаси-формы, где неявный первый источник V = DST.
 * DR-модели (проверено standalone-декодером, см. commit):
 *   1dst_2src [W(rm), V(dst)] — pcmpgtq/pcmpeqq/pmulld/pmuldq/packusdw/
 *                              семейство pmin/pmax/aesenc; blendps/pd —
 *                              декодер НЕ добавляет неявный V (макрос
 *                              1dst_3src от декодера отличается!)
 *   1dst_3src [W, imm, V]     — palignr/dpps/dppd/mpsadbw/roundss/
 *                              roundsd/pclmulqdq
 * Для всех них fn(r, S1, S2) ждёт (V, W): S1 = D, S2 = srcs[0].
 * Прочие легаси-формы блочного кейса — W-класс (fn читает только S1=W:
 * roundps/pd, pmovsx|zx-, phminposuw, aeskeygenassist, aesimc).      */
static int legacy_vw(int opc)
{
    switch (opc) {
    case OP_pcmpgtq: case OP_pcmpeqq: case OP_pmulld: case OP_pmuldq:
    case OP_packusdw:
    case OP_pminub: case OP_pmaxub: case OP_pminuw: case OP_pmaxuw:
    case OP_pminud: case OP_pmaxud: case OP_pminsb: case OP_pmaxsb:
    case OP_pminsw: case OP_pmaxsw: case OP_pminsd: case OP_pmaxsd:
    case OP_aesenc: case OP_aesdec: case OP_aesenclast: case OP_aesdeclast:
    case OP_blendps: case OP_blendpd:
    case OP_palignr: case OP_dpps: case OP_dppd: case OP_mpsadbw:
    case OP_roundss: case OP_roundsd: case OP_insertps:
    case OP_pclmulqdq:
        return 1;
    default:
        return 0;
    }
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

/* ── целочисленная VEX-семья для r10-fallback (§7.3 run #99/#100,
 * bun/JSC: r10-операнды JIT уводят vpcmpeqb и пр. в generic) ──
 * Байтовые/логические — поэлементно; насыщение — по SDM.      */
BLOCK1(b_vpand,  { for (int i = 0; i < 16; i++) r[i] = s1[i] & s2[i]; })
BLOCK1(b_vpor,   { for (int i = 0; i < 16; i++) r[i] = s1[i] | s2[i]; })
BLOCK1(b_vpxor,  { for (int i = 0; i < 16; i++) r[i] = s1[i] ^ s2[i]; })
BLOCK1(b_vpandn, { for (int i = 0; i < 16; i++) r[i] = (~s1[i]) & s2[i]; })

/* поэлементная арифметика ширины T: T, счёт, выражение над a,b */
#define IBLOCK(name, T, cnt, expr) \
BLOCK1(name, { \
    for (int i = 0; i < cnt; i++) { \
        T a, b, o; \
        memcpy(&a, s1 + i*sizeof(T), sizeof(T)); \
        memcpy(&b, s2 + i*sizeof(T), sizeof(T)); \
        (void)a; (void)b; expr; \
        memcpy(r + i*sizeof(T), &o, sizeof(T)); \
    } \
})
IBLOCK(b_vpaddb,  uint8_t,  16, o = (uint8_t)(a + b))
IBLOCK(b_vpaddw,  uint16_t,  8, o = (uint16_t)(a + b))
IBLOCK(b_vpaddd,  uint32_t,  4, o = (uint32_t)(a + b))
IBLOCK(b_vpaddq,  uint64_t,  2, o = (uint64_t)(a + b))
IBLOCK(b_vpsubb,  uint8_t,  16, o = (uint8_t)(a - b))
IBLOCK(b_vpsubw,  uint16_t,  8, o = (uint16_t)(a - b))
IBLOCK(b_vpsubd,  uint32_t,  4, o = (uint32_t)(a - b))
IBLOCK(b_vpsubq,  uint64_t,  2, o = (uint64_t)(a - b))
/* насыщающие: знаковые — кламп к [-128,127]/[-32768,32767],
 * беззнаковые — к [0,255]/[0,65535]; порядок SDM: psubb(x,y)=x-y */
IBLOCK(b_vpaddsb,  int8_t, 16, { int32_t t = (int32_t)a + b; \
    o = (int8_t)(t > 127 ? 127 : t < -128 ? -128 : t); })
IBLOCK(b_vpaddsw,  int16_t, 8, { int32_t t = (int32_t)a + b; \
    o = (int16_t)(t > 32767 ? 32767 : t < -32768 ? -32768 : t); })
IBLOCK(b_vpsubsb,  int8_t, 16, { int32_t t = (int32_t)a - b; \
    o = (int8_t)(t > 127 ? 127 : t < -128 ? -128 : t); })
IBLOCK(b_vpsubsw,  int16_t, 8, { int32_t t = (int32_t)a - b; \
    o = (int16_t)(t > 32767 ? 32767 : t < -32768 ? -32768 : t); })
IBLOCK(b_vpaddusb, uint8_t, 16, { int32_t t = (int32_t)a + b; \
    o = (uint8_t)(t > 255 ? 255 : t); })
IBLOCK(b_vpaddusw, uint16_t, 8, { int32_t t = (int32_t)a + b; \
    o = (uint16_t)(t > 65535 ? 65535 : t); })
IBLOCK(b_vpsubusb, uint8_t, 16, { int32_t t = (int32_t)a - b; \
    o = (uint8_t)(t < 0 ? 0 : t); })
IBLOCK(b_vpsubusw, uint16_t, 8, { int32_t t = (int32_t)a - b; \
    o = (uint16_t)(t < 0 ? 0 : t); })
/* умножения: младшие/старшие половины произведения */
IBLOCK(b_vpmullw,  uint16_t, 8, { int32_t t = (int32_t)(int16_t)a * (int16_t)b; \
    o = (uint16_t)(uint32_t)(t & 0xFFFF); })
IBLOCK(b_vpmulhw,  uint16_t, 8, { int32_t t = (int32_t)(int16_t)a * (int16_t)b; \
    o = (uint16_t)(uint32_t)((t >> 16) & 0xFFFF); })
IBLOCK(b_vpmulhuw, uint16_t, 8, { uint32_t t = (uint32_t)a * b; \
    o = (uint16_t)(t >> 16); })
/* PMADDWD: dword[i] = p1(2i)+p2(2i+1), знаково */
BLOCK1(b_vpmaddwd, {
    for (int i = 0; i < 4; i++) {
        int32_t p0 = (int32_t)(int16_t)ld16(s1 + i*4)     * (int16_t)ld16(s2 + i*4);
        int32_t p1 = (int32_t)(int16_t)ld16(s1 + i*4 + 2) * (int16_t)ld16(s2 + i*4 + 2);
        st32(r + i*4, (uint32_t)(p0 + p1));
    }
})
/* средние: (a+b+1)>>1 */
IBLOCK(b_vpavgb,  uint8_t,  16, o = (uint8_t)(((uint32_t)a + b + 1) >> 1))
IBLOCK(b_vpavgw,  uint16_t,  8, o = (uint16_t)(((uint32_t)a + b + 1) >> 1))
/* сравнения: VEX-формы pcmpeq и pcmpgt (b/w/d) */
IBLOCK(b_vpcmpeqb, uint8_t,  16, o = (a == b) ? (uint8_t)0xFF : 0)
IBLOCK(b_vpcmpeqw, uint16_t,  8, o = (a == b) ? (uint16_t)0xFFFF : 0)
IBLOCK(b_vpcmpeqd, uint32_t,  4, o = (a == b) ? 0xFFFFFFFFu : 0)
IBLOCK(b_vpcmpgtb, int8_t,  16, o = (a > b) ? (uint8_t)0xFF : 0)
IBLOCK(b_vpcmpgtw, int16_t,  8, o = (a > b) ? (uint16_t)0xFFFF : 0)
IBLOCK(b_vpcmpgtd, int32_t,  4, o = (a > b) ? 0xFFFFFFFFu : 0)

/* imm-сдвиги слов/двойных слов (VPSLL/RL W/D/Q imm, поканально в 256).
 * SDM: count > ширины элемента → результат 0 (не count & (w-1)!)     */
#define SHIFTI(name, T, cnt, bits, lshift) \
BLOCK1(name, { \
    for (int i = 0; i < cnt; i++) { \
        T a, o = 0; \
        memcpy(&a, s1 + i*sizeof(T), sizeof(T)); \
        if (imm < bits) { \
            if (lshift) o = (T)(a << imm); \
            else o = (T)(a >> imm); \
        } \
        memcpy(r + i*sizeof(T), &o, sizeof(T)); \
    } \
})
SHIFTI(b_vpsllw, uint16_t, 8, 16, 1)
SHIFTI(b_vpsrlw, uint16_t, 8, 16, 0)
SHIFTI(b_vpslld, uint32_t, 4, 32, 1)
SHIFTI(b_vpsrld, uint32_t, 4, 32, 0)
SHIFTI(b_vpsllq, uint64_t, 2, 64, 1)
SHIFTI(b_vpsrlq, uint64_t, 2, 64, 0)
/* байтовые сдвиги в 128-битной полосе (imm за границей — ноль) */
BLOCK1(b_vpslldq, {
    int k = imm > 15 ? 16 : imm;
    for (int i = 0; i < 16; i++)
        r[i] = (i >= k) ? s1[i - k] : 0;
})
BLOCK1(b_vpsrldq, {
    int k = imm > 15 ? 16 : imm;
    for (int i = 0; i < 16; i++)
        r[i] = (i + k < 16) ? s1[i + k] : 0;
})
/* упаковки со насыщением (128→нижняя половина из s1, верхняя из s2) */
BLOCK1(b_vpacksswb, {
    for (int i = 0; i < 8; i++) {
        int16_t v; memcpy(&v, s1 + i*2, 2);
        r[i] = (uint8_t)(v > 127 ? 127 : v < -128 ? -128 : v);
    }
    for (int i = 0; i < 8; i++) {
        int16_t v; memcpy(&v, s2 + i*2, 2);
        r[8+i] = (uint8_t)(v > 127 ? 127 : v < -128 ? -128 : v);
    }
})
BLOCK1(b_vpackuswb, {
    for (int i = 0; i < 8; i++) {
        int16_t v; memcpy(&v, s1 + i*2, 2);
        r[i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    for (int i = 0; i < 8; i++) {
        int16_t v; memcpy(&v, s2 + i*2, 2);
        r[8+i] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
})
BLOCK1(b_vpackssdw, {
    for (int i = 0; i < 4; i++) {
        int32_t v; memcpy(&v, s1 + i*4, 4);
        int16_t o16 = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        memcpy(r + i*2, &o16, 2);
    }
    for (int i = 0; i < 4; i++) {
        int32_t v; memcpy(&v, s2 + i*4, 4);
        int16_t o16 = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        memcpy(r + 8 + i*2, &o16, 2);
    }
})
/* пересечения (unpck): interleave низких/высоких элементов */
#define UNPCK(name, T, half, idx) \
BLOCK1(name, { \
    int n = 16 / (int)sizeof(T); \
    for (int i = 0; i < n/2; i++) { \
        T v; \
        memcpy(&v, s1 + ((half==0?i:i+n/2))*sizeof(T), sizeof(T)); \
        memcpy(r + (2*i)*sizeof(T), &v, sizeof(T)); \
        memcpy(&v, s2 + ((half==0?i:i+n/2))*sizeof(T), sizeof(T)); \
        memcpy(r + (2*i+1)*sizeof(T), &v, sizeof(T)); \
    } \
})
UNPCK(b_vpunpcklbw, uint8_t,  0, 0)
UNPCK(b_vpunpckhbw, uint8_t,  1, 0)
UNPCK(b_vpunpcklwd, uint16_t, 0, 0)
UNPCK(b_vpunpckhwd, uint16_t, 1, 0)
UNPCK(b_vpunpckldq, uint32_t, 0, 0)
UNPCK(b_vpunpckhdq, uint32_t, 1, 0)
BLOCK1(b_vpunpcklqdq, {
    memcpy(r,     s1,     8);
    memcpy(r + 8, s2,     8);
})
BLOCK1(b_vpunpckhqdq, {
    memcpy(r,     s1 + 8, 8);
    memcpy(r + 8, s2 + 8, 8);
})
/* vpblendw: imm-маска по словам (128-бит, AVX2 нет) */
BLOCK1(b_vpblendw, {
    for (int i = 0; i < 8; i++) {
        uint16_t v = (imm & (1u << i)) ? ld16(s2 + i*2) : ld16(s1 + i*2);
        st16(r + i*2, v);
    }
})
/* vpshufb: поканальная перестановка байтов по контрольным байтам W */
BLOCK1(b_vpshufb, {
    for (int i = 0; i < 16; i++) {
        uint8_t c = s2[i];
        r[i] = (c & 0x80) ? 0 : s1[c & 0x0F];
    }
})
/* vpsadbw: 8 байтовых |diff| в 16-бит сумму, нуль-расширение до qword */
BLOCK1(b_vpsadbw, {
    for (int q = 0; q < 2; q++) {
        uint16_t s = 0;
        for (int i = 0; i < 8; i++)
            s += (uint16_t)abs((int)s1[q*8+i] - (int)s2[q*8+i]);
        uint64_t v = s;
        memcpy(r + q*8, &v, 8);
    }
})
/* vpalignr: байтовый сдвиг пары s2:s1 в полосе (SSSE3-семантика) */
BLOCK1(b_vpalignr, {
    uint8_t cat[32];
    memcpy(cat,     s2, 16);
    memcpy(cat + 16, s1, 16);
    int k = imm > 31 ? 32 : imm;
    for (int i = 0; i < 16; i++)
        r[i] = (i + k < 32) ? cat[i + k] : 0;
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
    /* §7.3 run #106 (F): эмпирия (native mpsadbw $0x44/данные t0/t1):
     * 8 SAD-слов: слово j — скользящее ОКНО по V (байт-шаг!), против
     * фиксированного 4-байтового блока W. offV = imm8[2]*4 (байт),
     * offW = imm8[1:0]*4 (байт). Было 4 слова с dword-шагом по V. */
    int offA = ((imm >> 2) & 1) * 4;
    int offB = (imm & 3) * 4;
    for (int j = 0; j < 8; j++) {
        unsigned s = 0;
        for (int k = 0; k < 4; k++) {
            int a = s1[offA + j + k], b = s2[offB + k];
            s += (unsigned)(a > b ? a - b : b - a);
        }
        st16(r + j*2, (uint16_t)s);
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

/* BLEND-семья: imm-биты = ПОЛ i на канал (эмпирия run #106: native
 * blendps $0xA5 → lanes 0,2 из SRC; vblendps $0xA5 → то же; blendpd
 * $0x0A → lane 1 из SRC). Было (1 << (4+i)) — биты 4-7 для ОБОИХ
 * половин: 128-бит формы читали чужие биты imm. 256-битные формы:
 * VPBLENDPS/VPBLENDD — hi-половина = старшие биты (imm >> 4),
 * VBLENDPD — 4 канала, hi = imm >> 2 (все — тот же imm8).          */
static void blend_imm_block(uint8_t *r, const uint8_t *s1, const uint8_t *s2,
                            uint imm, int is_pd, int nlanes)
{
    int esz = is_pd ? 8 : 4;
    for (int i = 0; i < nlanes; i++)
        memcpy(r + i*esz, (imm & (1u << i)) ? s2 + i*esz : s1 + i*esz, esz);
}

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

/* BLENDPS/PD (imm) — вытеснены blend_imm_block (§7.3 run #106 (D)) */
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
/* VPBLENDD — вытеснен blend_imm_block (§7.3 run #106 (D)) */

/* INSERTPS */
BLOCK1(b_insertps, {
    /* §7.3 run #106 (G): эмпирия (native insertps $0x13, t0/t1 →
     * [0, 0, V2, V3]): zmask = imm8[3:0] (4 бита!), dsel = imm8[5:4],
     * ssel = imm8[7:6]; zmask перекрывает вставку. Было zmask=imm[7:6],
     * ssel=imm[3:2].                                       */
    int zmask = (int)(imm & 0xF);
    int dsel  = (int)((imm >> 4) & 3);
    int ssel  = (int)((imm >> 6) & 3);
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

/* §7.3 run #106 (H): обратная таблица — построена из AES_SBOX выше
 * (инверсия перестановки), нужна AESDEC/AESDECLAST (InvSubBytes). */
static const uint8_t AES_INV_SBOX[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
    0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
    0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
    0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
    0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
    0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
    0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
    0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
    0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
    0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
    0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0c, 0x7d
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
        /* §7.3 run #106 (H): эмпирия t_s41 — AESDEC ≠ AESDECLAST:
         * AESDEC = InvShiftRows → InvSubBytes → InvMixColumns → XOR
         * (InvMixColumns входит в круглую инструкцию, в DECLAST — нет). */
        inv_shift_rows(r);
        for (int i = 0; i < 16; i++) r[i] = AES_INV_SBOX[r[i]];
        inv_mix_cols(r);
        for (int i = 0; i < 16; i++) r[i] ^= s2[i];
        break;
    case OP_aesdeclast:
        inv_shift_rows(r);
        for (int i = 0; i < 16; i++) r[i] = AES_INV_SBOX[r[i]];
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
    /* §7.3 run #106 (I): семантика вскрыта нативным пробником
     * (kgprobe, вход 01..10, imm=0 → 6b6fc530 6fc5306b d7ab76ca
     * ab76cad7): выход строится из НЕЧЁТНЫХ dword'ов источника:
     *   DEST[31:0]   = SubWord(X1)
     *   DEST[63:32]  = SubWord(RotWord(X1)) XOR RCON (байт 0)
     *   DEST[95:64]  = SubWord(X3)
     *   DEST[127:96] = SubWord(RotWord(X3)) XOR RCON (байт 0)
     * X1 = s[4:8], X3 = s[12:16], RotWord = байт-ротация влево,
     * RCON = imm8 (imm=3 даёт XOR 0x03 в байте 0 — проверено). */
    uint8_t rc = (uint8_t)imm;
    uint8_t r[16];
    for (int h = 0; h < 2; h++) {
        const uint8_t *x = s + 4 + h * 8;      /* X1, X3 */
        uint8_t *dst = r + h * 8;
        dst[0] = AES_SBOX[x[0]]; dst[1] = AES_SBOX[x[1]];
        dst[2] = AES_SBOX[x[2]]; dst[3] = AES_SBOX[x[3]];
        dst[4] = AES_SBOX[x[1]] ^ rc;
        dst[5] = AES_SBOX[x[2]];
        dst[6] = AES_SBOX[x[3]];
        dst[7] = AES_SBOX[x[0]];
    }
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
    /* §7.4 #111: esz учитывает и vmaskmovpd (8Б) — раньше только
     * vpmaskmovq, и pd-форма считала 8 dword-элементов (чтение маски
     * за 32Б маскрегистра, запись за буфер) */
    int esz = (d->opc == OP_vpmaskmovq || d->opc == OP_vmaskmovpd) ? 8 : 4;
    return d->is_ymm ? (32 / esz) : (16 / esz);
}

static void pmaskmov_op(estate_t *st, emu_desc_t *d)
{
    /* §7.4 раунд #111: DR-модели (irmodel на gcc-эмите интринзиков):
     *  load  (dst0=reg): srcs[0]=МАСКА (xmm), srcs[1]=mem;
     *  store (dst0=mem): srcs[0]=данные (xmm), srcs[1]=маска (xmm).
     * Раньше load-форма читала mem/mask НАОБОРОТ (модель gather не
     * переносится: у gather индекс — в mem-операнде, у maskmov маска —
     * отдельный регистр) — и store-форма вообще не различалась.
     * vmaskmovps/pd — та же механика движения данных (esz 4/8).       */
    int esz = (d->opc == OP_vpmaskmovq || d->opc == OP_vmaskmovpd) ? 8 : 4;
    int n = pmask_elems(d);
    int is_store = opnd_is_memory_reference(d->dsts[0]);
    opnd_t mem, mask, data;
    if (is_store) {
        /* store-форма — порядок srcs у DR РАЗНЫЙ в двух семействах
         * (irmodel с различимыми регистрами YMM0=188, YMM1=189):
         *  vpmaskmovd/q: srcs=[data, mask]  (s0=r188, s1=r189)
         *  vmaskmovps/pd: srcs=[mask, data] (s0=r189, s1=r188) */
        mem  = d->dsts[0];
        if (d->opc == OP_vmaskmovps || d->opc == OP_vmaskmovpd) {
            mask = d->srcs[0];
            data = d->srcs[1];
        } else {
            data = d->srcs[0];
            mask = d->srcs[1];
        }
    } else {
        /* load-форма: [dst, mask, mem] — srcs[0]=маска, srcs[1]=mem;
         * незамаскированные элементы = 0 (SDM), не старое значение dst */
        mem  = d->srcs[1];
        mask = d->srcs[0];
        data = d->dsts[0];
    }
    /* maskmov: элементы ПОДРЯД (base + i*esz) — индексного регистра нет
     * (это не gather); mem_addr уже вычисляет полный app-адрес. */
    byte *base = mem_addr(st, mem);
    uint8_t mvb[32], dvb[32];
    const uint8_t *mv;
    int mi = is_vec_reg(mask) ? xmm_idx(opnd_get_reg(mask)) : -1;
    if (mi >= 0) {
        memcpy(mvb, EX(st, mi), 16);
        if (d->is_ymm) memcpy(mvb + 16, EY(st, mi), 16);
        mv = mvb;
    } else {
        mv = EX(st, 0);
    }
    int di = is_vec_reg(data) ? xmm_idx(opnd_get_reg(data)) : 0;
    memcpy(dvb, EX(st, di), 16);
    if (d->is_ymm) memcpy(dvb + 16, EY(st, di), 16);

    if (is_store) {
        for (int i = 0; i < n; i++) {
            uint64_t m = (esz == 4) ? ld32(mv + i*4) : ld64(mv + i*8);
            if (!(m >> (esz*8 - 1))) continue;
            byte *a = base + (uint64_t)i * esz;
            uint64_t w = (esz == 4) ? ld32(dvb + i*4) : ld64(dvb + i*8);
            emu_safe_write(st, a, &w, esz);
        }
    } else {
        uint8_t r[32];
        memset(r, 0, 32);          /* SDM: незамаскированные = 0 */
        for (int i = 0; i < n; i++) {
            uint64_t m = (esz == 4) ? ld32(mv + i*4) : ld64(mv + i*8);
            if (!(m >> (esz*8 - 1))) continue;
            byte *a = base + (uint64_t)i * esz;
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

/* ══════════════════ полуходы vmovlps/hps/lpd/hpd (§7.4 #111) ══════════════════
 * irmodel (gcc-эмит _mm_loadh_pd/_mm_storeh_pd/...):
 *  load  (dst0=reg):  nsrc=2, s0=V(merge-база), s1=m64;
 *  store (dst0=mem):  nsrc=1, s0=данные (xmm).
 * Семантика: VMOVLPD/HPS — mem64 → lo(dst), верх = V (тень);
 * VMOVHPD/HPS — mem64 → hi(dst) (в тень!), lo = V; VEX.128 — верх
 * результата (255:128) в ноль → EY зануляется после записи.      */
static void halfmove_op(estate_t *st, emu_desc_t *d)
{
    int is_h = (d->opc == OP_vmovhps || d->opc == OP_vmovhpd);
    if (opnd_is_memory_reference(d->dsts[0])) {
        /* store: mem64 = lo (lps/lpd) или hi из тени (hps/hpd) */
        int di = xmm_idx(opnd_get_reg(d->srcs[0]));
        byte *a = mem_addr(st, d->dsts[0]);
        uint64_t v = is_h ? ld64(EY(st, di)) : ld64(EX(st, di));
        emu_safe_write(st, a, &v, 8);
        return;
    }
    int di = xmm_idx(opnd_get_reg(d->dsts[0]));
    uint64_t half;
    if (opnd_is_memory_reference(d->srcs[1])) {
        uint64_t t = 0;
        emu_safe_read(st, mem_addr(st, d->srcs[1]), &t, 8);
        half = t;
    } else {
        int si = xmm_idx(opnd_get_reg(d->srcs[1]));
        half = ld64(EX(st, si));
    }
    /* V (srcs[0]) — база слияния: ВЕСЬ его 128-битный вид лежит в
     * реальном xmm (спил); hi64 результата = hi64(V) для lpd/lps,
     * lo64 результата = lo64(V) для hpd/hps (баг t_vx41: нативный
     * bl1.c — vmovlpd давал мусорный верх вместо V.hi). */
    uint8_t vfull[16];
    memcpy(vfull, EX(st, xmm_idx(opnd_get_reg(d->srcs[0]))), 16);
    uint8_t out[16];
    memcpy(out, vfull, 16);
    if (is_h) st64(out + 8, half);      /* hpd/hps: hi = mem64 */
    else     st64(out, half);           /* lpd/lps: lo = mem64 */
    memcpy(EX(st, di), out, 16);
    memset(EY(st, di), 0, 16);          /* VEX.128: ymm-верх в ноль */
}

/* ── pmovsx/zx 256-бит: сколько байт младшей половины потребляет lane0 ── */
static int is_widen(int opc)
{
    switch (opc) {
    case OP_pmovsxbw: case OP_vpmovsxbw: case OP_pmovsxbd: case OP_vpmovsxbd:
    case OP_pmovsxbq: case OP_vpmovsxbq: case OP_pmovsxwd: case OP_vpmovsxwd:
    case OP_pmovsxwq: case OP_vpmovsxwq: case OP_pmovsxdq: case OP_vpmovsxdq:
    case OP_pmovzxbw: case OP_vpmovzxbw: case OP_pmovzxbd: case OP_vpmovzxbd:
    case OP_pmovzxbq: case OP_vpmovzxbq: case OP_pmovzxwd: case OP_vpmovzxwd:
    case OP_pmovzxwq: case OP_vpmovzxwq: case OP_pmovzxdq: case OP_vpmovzxdq:
        return 1;
    default:
        return 0;
    }
}
static int widen_off(int opc)
{
    switch (opc) {
    case OP_pmovsxbw: case OP_vpmovsxbw: case OP_pmovzxbw: case OP_vpmovzxbw:
        return 8;                          /* 8 байт → 8 слов */
    case OP_pmovsxbd: case OP_vpmovsxbd: case OP_pmovzxbd: case OP_vpmovzxbd:
        return 4;                          /* 4 байта → 4 дворда */
    case OP_pmovsxbq: case OP_vpmovsxbq: case OP_pmovzxbq: case OP_vpmovzxbq:
        return 2;                          /* 2 байта → 2 кворда */
    case OP_pmovsxwd: case OP_vpmovsxwd: case OP_pmovzxwd: case OP_vpmovzxwd:
        return 8;                          /* 4 слова → 4 дворда */
    case OP_pmovsxwq: case OP_vpmovsxwq: case OP_pmovzxwq: case OP_vpmovzxwq:
        return 4;                          /* 2 слова → 2 кворда */
    case OP_pmovsxdq: case OP_vpmovsxdq: case OP_pmovzxdq: case OP_vpmovzxdq:
        return 8;                          /* 2 дворда → 2 кворда */
    default:
        return 0;
    }
}

/* ══════════════════ cvt-ширины и permil (§7.4 раунд #111) ══════════════════
 * VEX-формы конверсий НЕ расщепляются механически (256-битная форма
 * читает только младшую половину источника) — потому они в generic,
 * а не в inline-карте. Семантика сверена натурной пробой (probe5). */

/* int32 → double: точная конверсия */
static double cvt_i2d(uint32_t raw)
{
    return (double)(int32_t)raw;
}

/* double → int32: переполнение/NaN → 0x80000000 (integer indefinite, SDM).
 * Округление: nearest-even эмулируем магическим числом (дефолт MXCSR);
 * полная поддержка нестандартного MXCSR — v0.3.                          */
static int32_t cvt_near_i32(double v)
{
    if (!(v == v)) return (int32_t)0x80000000;
    if (v >= 2147483648.0 || v < -2147483648.0) return (int32_t)0x80000000;
    double t = v + 6755399441055744.0;              /* 1.5 * 2^52 */
    return (int32_t)(int64_t)(t - 6755399441055744.0);
}

static int32_t cvt_trunc_i32(double v)
{
    if (!(v == v)) return (int32_t)0x80000000;
    if (v >= 2147483648.0 || v < -2147483648.0) return (int32_t)0x80000000;
    return (int32_t)v;                              /* C cast = cvttsd2si */
}

/* VCVTDQ2PD: ne = размер dst / 8; источник даёт ne int32 (младшие биты) */
static void cvt_dq2pd(estate_t *st, emu_desc_t *d)
{
    int di = xmm_idx(opnd_get_reg(d->dsts[0]));
    int nd = d->is_ymm ? 4 : 2;
    uint8_t s[16];
    memset(s, 0, 16);
    if (opnd_is_memory_reference(d->srcs[0])) {
        size_t nb = (size_t)nd * 4;
        uint64_t v = 0;
        emu_safe_read(st, mem_addr(st, d->srcs[0]), &v, nb);
        memcpy(s, &v, nb);
    } else {
        memcpy(s, EX(st, xmm_idx(opnd_get_reg(d->srcs[0]))), 16);
    }
    /* пишем через tmp: EX — слот 16Б, старшие doubles живут в EY */
    uint8_t tmp[32];
    memset(tmp, 0, 32);
    for (int i = 0; i < nd; i++) {
        double val = cvt_i2d(ld32(s + i*4));
        memcpy(tmp + i*8, &val, 8);
    }
    memcpy(EX(st, di), tmp, 16);
    if (d->is_ymm) memcpy(EY(st, di), tmp + 16, 16);
    else memset(EY(st, di), 0, 16);                 /* VEX.128: верх в ноль */
}

/* VCVTPD2DQ / VCVTTPD2DQ: ne = размер src / 8 (128→2, 256→4);
 * результат в младшей половине dst, верх = 0. */
static void cvt_pd2dq(estate_t *st, emu_desc_t *d, int trunc)
{
    int di = xmm_idx(opnd_get_reg(d->dsts[0]));
    uint8_t s[32];
    memset(s, 0, 32);
    int ns;
    if (opnd_is_memory_reference(d->srcs[0])) {
        opnd_size_t msz = opnd_get_size(d->srcs[0]);
        ns = (int)opnd_size_in_bytes(msz) / 8;
        if (ns < 1) ns = 1;
        if (ns > 4) ns = 4;
        size_t nb = (size_t)ns * 8;
        uint8_t tmp[32];
        memset(tmp, 0, 32);
        uint64_t acc[4] = {0, 0, 0, 0};
        emu_safe_read(st, mem_addr(st, d->srcs[0]), acc, nb);
        memcpy(tmp, acc, nb);
        memcpy(s, tmp, 32);
    } else {
        int si = xmm_idx(opnd_get_reg(d->srcs[0]));
        memcpy(s, EX(st, si), 16);
        if (d->is_ymm || opnd_get_size(d->srcs[0]) == OPSZ_32)
            memcpy(s + 16, EY(st, si), 16);
        ns = (d->is_ymm || opnd_get_size(d->srcs[0]) == OPSZ_32) ? 4 : 2;
    }
    uint8_t *dv = EX(st, di);
    memset(dv, 0, 16);
    memset(EY(st, di), 0, 16);                      /* верх dst = 0 */
    for (int i = 0; i < ns; i++) {
        double val;
        memcpy(&val, s + i*8, 8);
        int32_t r = trunc ? cvt_trunc_i32(val) : cvt_near_i32(val);
        st32(dv + i*4, (uint32_t)r);
    }
}

/* VPERMILPS/PD: imm-форма (srcs[1]=imm) и reg-форма (srcs[1]=ctrl).
 * PS: выборка dword по 2 битам; PD: выборка qword по 1 биту.
 * 256-бит: поканально (каждый 128-бит lane независимо).             */
static void permil_op(estate_t *st, emu_desc_t *d)
{
    int is_pd = (d->opc == OP_vpermilpd);
    int nl = is_pd ? 2 : 4;                         /* элементов на lane */
    int lanes = d->is_ymm ? 2 : 1;
    uint8_t s[32];
    memset(s, 0, 32);
    if (opnd_is_memory_reference(d->srcs[0])) {
        uint64_t acc[4] = {0, 0, 0, 0};
        size_t nb = (size_t)(d->is_ymm ? 32 : 16);
        emu_safe_read(st, mem_addr(st, d->srcs[0]), acc, nb);
        memcpy(s, acc, nb);
    } else {
        int si = xmm_idx(opnd_get_reg(d->srcs[0]));
        memcpy(s, EX(st, si), 16);
        if (d->is_ymm) memcpy(s + 16, EY(st, si), 16);
    }
    int ctrl_imm = opnd_is_immed_int(d->srcs[1]);
    uint imm = ctrl_imm ? (uint)opnd_get_immed_int(d->srcs[1]) : 0;
    uint8_t c[32];
    memset(c, 0, 32);
    if (!ctrl_imm) {
        if (opnd_is_memory_reference(d->srcs[1])) {
            uint64_t acc[4] = {0, 0, 0, 0};
            size_t nb = (size_t)(d->is_ymm ? 32 : 16);
            emu_safe_read(st, mem_addr(st, d->srcs[1]), acc, nb);
            memcpy(c, acc, nb);
        } else {
            int ci = xmm_idx(opnd_get_reg(d->srcs[1]));
            memcpy(c, EX(st, ci), 16);
            if (d->is_ymm) memcpy(c + 16, EY(st, ci), 16);
        }
    }
    int di = xmm_idx(opnd_get_reg(d->dsts[0]));
    uint8_t outbuf[32];
    uint8_t *out = outbuf;
    memset(out, 0, 32);
    for (int lane = 0; lane < lanes; lane++) {
        const uint8_t *sb = s + lane*16;
        uint8_t *ob = out + lane*16;
        const uint8_t *cb = c + lane*16;
        for (int i = 0; i < nl; i++) {
            int sel;
            if (ctrl_imm) {
                /* imm-форма: ОДНИ И ТЕ ЖЕ биты для обоих lane (SDM:
                 * каждый 128-бит lane переставляется одним и тем же imm) */
                sel = is_pd ? (int)((imm >> i) & 1)
                            : (int)((imm >> (2*i)) & 3);
            } else {
                /* PD-reg: селектор — БИТ 1 чётного dword (натурный
                 * пробник vp2.c: dword0 bit1 → q0, dword2 bit1 → q1) */
                sel = is_pd ? (int)((ld32(cb + i*8) >> 1) & 1)
                            : (int)(ld32(cb + i*4) & 3);
            }
            size_t esz = is_pd ? 8 : 4;
            memcpy(ob + i*esz, sb + sel*esz, esz);
        }
    }
    /* запись результата целиком (баг t_vx41: lo-lane ymm-формы не
     * копировалась обратно из scratch) */
    memcpy(EX(st, di), out, 16);
    if (d->is_ymm) memcpy(EY(st, di), out + 16, 16);
    else memset(EY(st, di), 0, 16);
}

/* ══════════════════ строковые SSE4.2 ══════════════════
 * §7.4 раунд #111: полная семантика по SDM §4.1, выверенная НАТУРНОЙ
 * пробой (probe_istr/probe2/probe3/probe4: 180 кейсов t_istr.S native
 * vs pass). Ключевые правила:
 *  - SRC1 (modrm.reg) = множество для equal-any / пары для ranges /
 *    needle для equal-ordered; SRC2 (modrm.rm) = ИНДЕКСИРУЕМАЯ строка —
 *    индекс IntRes1 всегда идёт по элементам SRC2;
 *  - ESTRx: длины из EAX/EDX (абсолютное значение, насыщение до ne,
 *    0 = пустая строка); ISTRx: неявные NUL-терминированные длины ОБЕИХ
 *    строк (первый нулевой элемент режет валидность);
 *  - IntRes1[i] = (valid1[i] && valid2[i]) ? agg : 0;
 *  - полярность (imm8[4]): IntRes2 = IntRes1 XOR 1 по ВСЕМ элементам
 *    (включая невалидные — натурно: estri la=0 neg → ECX=0);
 *  - masked (imm8[5]): невалидные элементы SRC2 → IntRes2[i] = 0
 *    (натурно: msk-neg la=16 lb=0 → ECX=16; la=0 lb=16 → ECX=0);
 *  - equal-ordered: конец SRC2 (i+j >= len2) → остаток needle считается
 *    совпадением (tail-правило, натурно: "MNO\0" в хвосте → ECX=12);
 *  - ranges: пары валидны только целиком (2k+1 < len1), lo > hi —
 *    мёртвая пара, знак по imm8[1];
 *  - ECX = первый/последний (imm8[6]) установленный бит IntRes2, нет
 *    бит → ne (16/8); XMM0 (strm): бит-маска (imm6=0): byte → 2Б,
 *    word → 1Б, хвост нулями; unit-маска (imm6=1): FF/FFFF на элемент;
 *  - флаги: CF = IntRes2 != 0; OF = IntRes2[0]; ZF = len2 < ne;
 *    SF = len1 < ne (для ISTRx эквивалентно «есть NUL» — NUL и есть
 *    терминатор); AF = PF = 0.                                     */
static void string_op(estate_t *st, emu_desc_t *d, int is_estr, int is_stri)
{
    uint8_t tmpa[16], tmpb[16];
    const uint8_t *a = get_block16(st, d->srcs[0], tmpa);   /* SRC1 */
    const uint8_t *b = get_block16(st, d->srcs[1], tmpb);   /* SRC2 */
    uint imm = (d->nsrc >= 3 && opnd_is_immed_int(d->srcs[2]))
               ? (uint)opnd_get_immed_int(d->srcs[2]) : 0;

    int sz = (imm & 1) ? 2 : 1;
    int ne = 16 / sz;
    int sgn = (int)(imm & 2);
    int agg = (int)((imm >> 2) & 3);
    int neg = (int)((imm >> 4) & 1);
    int msk = (int)((imm >> 5) & 1);
    int msb = (int)((imm >> 6) & 1);

    /* валидные длины */
    int len1, len2;
    if (is_estr) {
        /* длина: абсолютное значение EAX/EDX с насыщением до ne;
         * размер регистра — по IR (EAX 4Б, RAX при REX.W 8Б) */
        uint64_t r1 = 0, r2 = 0;
        if (d->nsrc >= 4 && opnd_is_reg(d->srcs[3])) {
            r1 = *EG(st, gpr_idx(opnd_get_reg(d->srcs[3])));
            if (opnd_get_size(d->srcs[3]) == OPSZ_4) r1 &= 0xFFFFFFFFull;
            else if (opnd_get_size(d->srcs[3]) == OPSZ_2) r1 &= 0xFFFFull;
        }
        if (d->nsrc >= 5 && opnd_is_reg(d->srcs[4])) {
            r2 = *EG(st, gpr_idx(opnd_get_reg(d->srcs[4])));
            if (opnd_get_size(d->srcs[4]) == OPSZ_4) r2 &= 0xFFFFFFFFull;
            else if (opnd_get_size(d->srcs[4]) == OPSZ_2) r2 &= 0xFFFFull;
        }
        int64_t l1 = (int64_t)r1, l2 = (int64_t)r2;
        if (l1 < 0) l1 = -l1;
        if (l2 < 0) l2 = -l2;
        len1 = (l1 > ne) ? ne : (int)l1;
        len2 = (l2 > ne) ? ne : (int)l2;
    } else {
        /* ISTRx: неявные NUL-терминированные длины обеих строк */
        len1 = ne; len2 = ne;
        for (int i = 0; i < ne; i++) {
            int nul = (sz == 1) ? (a[i] == 0) : (ld16(a + i*2) == 0);
            if (nul) { len1 = i; break; }
        }
        for (int i = 0; i < ne; i++) {
            int nul = (sz == 1) ? (b[i] == 0) : (ld16(b + i*2) == 0);
            if (nul) { len2 = i; break; }
        }
    }

    /* IntRes1: агрегация по элементам SRC2 */
    int res[16];
    for (int i = 0; i < ne; i++) {
        res[i] = 0;
        if (i >= len2) continue;               /* SRC2[i] невалиден */
        uint64_t bv = (sz == 1) ? b[i] : ld16(b + i*2);
        switch (agg) {
        case 0:                                /* equal any: b[i] ∈ a[j] */
            for (int j = 0; j < len1; j++) {
                uint64_t av = (sz == 1) ? a[j] : ld16(a + j*2);
                if (av == bv) { res[i] = 1; break; }
            }
            break;
        case 1:                                /* ranges: пары из SRC1 */
            for (int k = 0; k + 1 < len1; k += 2) {
                uint64_t lo = (sz == 1) ? a[k]   : ld16(a + k*2);
                uint64_t hi = (sz == 1) ? a[k+1] : ld16(a + (k+1)*2);
                if (sgn) {
                    int64_t slo = (sz == 1) ? (int8_t)lo : (int16_t)lo;
                    int64_t shi = (sz == 1) ? (int8_t)hi : (int16_t)hi;
                    int64_t sv  = (sz == 1) ? (int8_t)bv : (int16_t)bv;
                    if (slo <= sv && sv <= shi) { res[i] = 1; break; }
                } else {
                    if (lo <= bv && bv <= hi) { res[i] = 1; break; }
                }
            }
            break;
        case 2:                                /* equal each */
            if (i < len1) {
                uint64_t av = (sz == 1) ? a[i] : ld16(a + i*2);
                res[i] = (av == bv);
            }
            break;
        case 3:                                /* equal ordered: needle SRC1 */
            res[i] = 1;
            for (int j = 0; j < len1; j++) {
                int bi = i + j;
                if (bi >= ne || bi >= len2) break;   /* конец SRC2 = match */
                uint64_t av = (sz == 1) ? a[j] : ld16(a + j*2);
                uint64_t bv2 = (sz == 1) ? b[bi] : ld16(b + bi*2);
                if (av != bv2) { res[i] = 0; break; }
            }
            break;
        }
    }

    /* IntRes2: полярность (по всем), masked — невалидные SRC2 → 0 */
    for (int i = 0; i < ne; i++) {
        int v = (i < len2) ? res[i] : 0;
        res[i] = neg ? (v ^ 1) : v;
        if (msk && i >= len2) res[i] = 0;
    }

    int any = 0;
    for (int i = 0; i < ne; i++) if (res[i]) any = 1;

    uint32_t fl = 0;
    if (any) fl |= FCF;
    if (res[0]) fl |= FOF;
    if (len2 < ne) fl |= FZF;
    if (len1 < ne) fl |= FSF;

    if (is_stri) {
        int idx = ne;
        if (msb) {
            for (int i = ne - 1; i >= 0; i--)
                if (res[i]) { idx = i; break; }
        } else {
            for (int i = 0; i < ne; i++)
                if (res[i]) { idx = i; break; }
        }
        *EG(st, 1) = (uint64_t)(uint32_t)idx;  /* ECX, нулевое расширение */
    } else {
        /* XMM0 = выходная маска */
        uint8_t r[16];
        memset(r, 0, 16);
        if (msb) {                             /* unit-маска */
            for (int i = 0; i < ne; i++) {
                if (sz == 1) r[i] = res[i] ? 0xFF : 0x00;
                else st16(r + i*2, res[i] ? 0xFFFF : 0x0000);
            }
        } else {                               /* бит-маска: byte → 2Б, word → 1Б */
            int nbytes = (sz == 1) ? 2 : 1;
            for (int i = 0; i < ne; i++)
                if (res[i]) r[i / 8] |= (uint8_t)(1u << (i % 8));
            (void)nbytes;
        }
        memcpy(EX(st, 0), r, 16);
    }
    flags_setmask(st, fl, FCF|FZF|FSF|FOF|FAF|FPF);
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

/* §7.3: гард «тихого пропуска» — страховой путь generic-стаба
 * (rewrite.c, после clean call). Возврат 0 в стабе = операция
 * удалена, но не эмулирована: тихая порча shadow. Здесь она
 * невозможна по построению (таблицы в lockstep), но класс слишком
 * дорог (паника аллокатора bun) — громкая остановка с именем
 * опкода, как у ловушки UNENCODABLE.                             */
void emu_stub_unsupported(emu_desc_t *d)
{
    dr_fprintf(STDERR,
               "[P.A.S.S.] СТАБ: emu вернула 0 (opc=%d, "
               "необработанный опкод добрался до generic-стаба) — "
               "громкая остановка (гард тихого пропуска §7.3)\n",
               d->opc);
    dr_flush_file(STDERR);
    dr_abort();
}

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
        mc0.flags = DR_MC_INTEGER | DR_MC_CONTROL;
        if (buf0 != NULL && dr_get_mcontext(dc0, &mc0)) {
            *(uint32_t *)(buf0 + BUF_FLAGS) = (uint32_t)mc0.xflags;
            *(uint64_t *)(buf0 + BUF_R10) = (uint64_t)mc0.r10;
        }
    }
    int rc = emu_generic_impl(d);
    /* §7.3: код возврата — в BUF_RC. rax после clean call с fp=1
     * затирается DR-эпилогом (call dr-хелпера после fxrstor), поэтому
     * гард стаба (rewrite.c, 5*) читает СЛОТ, а не rax. TLS к этому
     * моменту уже создан (pass_ensure_tls внутри impl).            */
    {
        void *dc1 = dr_get_current_drcontext();
        uint8_t *buf1 = (uint8_t *)drmgr_get_tls_field(dc1, g_tls_idx_buf);
        if (buf1 != NULL)
            *(uint64_t *)(buf1 + BUF_RC) = (uint64_t)(unsigned)rc;
    }
    if (getenv("PASS_EXCLOG") != NULL && buf0 != NULL &&
        *(uint64_t *)(buf0 + BUF_TG) != 0)
        dr_fprintf(STDERR, "[EMUFLT] opc=%d addr=" PIFX " w=%d\n",
                   d->opc,
                   (ptr_uint_t)*(uint64_t *)(buf0 + BUF_FAULT),
                   (int)(*(uint64_t *)(buf0 + BUF_TG) >> 1));
    return rc;
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
    /* §7.3 run #106 (E): imm может стоять НЕ последним в srcs! DR
     * декодирует легаси-формы palignr/dpps/dppd/mpsadbw/roundss/sd/
     * pclmulqdq/blendps/pd как [W, imm, V] — imm в СЕРЕДИНЕ. Старое
     * чтение srcs[nsrc-1] давало imm=0 (например blendps: все каналы
     * из dst). Сканируем все srcs от конца к началу.                  */
    for (int k = d->nsrc - 1; k >= 0; k--) {
        if (k >= 4) continue;
        if (opnd_is_immed_int(d->srcs[k])) {
            imm = (uint8_t)opnd_get_immed_int(d->srcs[k]);
            break;
        }
    }

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
    case OP_pclmulqdq: case OP_vpclmulqdq:
    /* §7.4 раунд #111: VEX-алиасы уже-эмулируемых SSE4.1/4.2/AES —
     * тот же класс дыры, что vpsubusb (t_iemu 19/20/23): без меток
     * опкоды проваливались в left-native (на дев-хосте — мусор тени,
     * на Core 2 — SIGILL/trap, а trap без case — громкая остановка). */
    case OP_vpcmpeqq: case OP_vpcmpgtq: case OP_vpmulld: case OP_vpmuldq:
    case OP_vpackusdw: case OP_vphminposuw: case OP_vmpsadbw:
    case OP_vroundps: case OP_vroundpd: case OP_vroundss: case OP_vroundsd:
    case OP_vdpps: case OP_vdppd:
    case OP_vblendps: case OP_vblendpd:
    case OP_vpminuw: case OP_vpmaxuw: case OP_vpminud: case OP_vpmaxud:
    case OP_vpminsb: case OP_vpmaxsb: case OP_vpminsd: case OP_vpmaxsd:
    case OP_vpmovsxbw: case OP_vpmovsxbd: case OP_vpmovsxbq:
    case OP_vpmovsxwd: case OP_vpmovsxwq: case OP_vpmovsxdq:
    case OP_vpmovzxbw: case OP_vpmovzxbd: case OP_vpmovzxbq:
    case OP_vpmovzxwd: case OP_vpmovzxwq: case OP_vpmovzxdq:
    case OP_vinsertps:
    case OP_vaesenc: case OP_vaesdec: case OP_vaesenclast: case OP_vaesdeclast:
    case OP_vaesimc: case OP_vaeskeygenassist:
    /* §7.3 run #100: целочисленная VEX-семья через r10-fallback
     * (bun/JSC) — см. BLOCK1/IBLOCK определения выше */
    case OP_vpand: case OP_vpandn: case OP_vpor: case OP_vpxor:
    case OP_vpaddb: case OP_vpaddw: case OP_vpaddd: case OP_vpaddq:
    case OP_vpsubb: case OP_vpsubw: case OP_vpsubd: case OP_vpsubq:
    case OP_vpaddsb: case OP_vpaddsw:
    case OP_vpaddusb: case OP_vpaddusw:
    case OP_vpsubsb: case OP_vpsubsw:
    case OP_vpsubusb: case OP_vpsubusw:
    case OP_vpmullw: case OP_vpmulhw: case OP_vpmulhuw: case OP_vpmaddwd:
    case OP_vpavgb: case OP_vpavgw:
    case OP_vpcmpeqb: case OP_vpcmpeqw: case OP_vpcmpeqd:
    case OP_vpcmpgtb: case OP_vpcmpgtw: case OP_vpcmpgtd:
    case OP_vpsllw: case OP_vpsrlw: case OP_vpslld: case OP_vpsrld:
    case OP_vpsllq: case OP_vpsrlq: case OP_vpslldq: case OP_vpsrldq:
    case OP_vpacksswb: case OP_vpackssdw: case OP_vpackuswb:
    case OP_vpunpcklbw: case OP_vpunpckhbw:
    case OP_vpunpcklwd: case OP_vpunpckhwd:
    case OP_vpunpckldq: case OP_vpunpckhdq:
    case OP_vpunpcklqdq: case OP_vpunpckhqdq:
    case OP_vunpcklps: case OP_vunpckhps:
    case OP_vunpcklpd: case OP_vunpckhpd:
    case OP_vpblendw: case OP_vpshufb: case OP_vpsadbw: case OP_vpalignr: {
        void (*fn)(uint8_t *, const uint8_t *, const uint8_t *, uint) = NULL;
        uint8_t S1[32], S2[32], D[32];
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        memcpy(D, EX(&st, di), 16);
        if (d->is_ymm) memcpy(D + 16, EY(&st, di), 16);
        /* источники: reg → EX+EY, mem → чтение 32/16/4Б по размеру операнда.
         * §7.3 run #106 (C): стейджинг различает VEX/легаси-модели:
         *  VEX: канон srcs[0]=V(Hx), srcs[1]=W (imm-форма [V,W,imm]).
         *  легаси V-формы (legacy_vw): V=DST, W=srcs[0] — раньше S1=
         *  srcs[0]=W (правильно только для VEX): pcmpgtq перевёрнут,
         *  packusdw перепутал половины, а imm-формы [W,imm,V] читали
         *  S2 из нулей (blendps/palignr/dpps/mpsadbw/roundss/sd/
         *  insertps/pclmulqdq).                                        */
        if (d->is_vex && d->opc == OP_vinsertps) {
            /* §7.4 #111: VINSERTPS DR IR = [imm, V, W] — imm ПЕРВЫЙ
             * (irmodel: vinsertps $imm, %xmm2, %xmm0 → srcs=[imm, xmm0=V,
             * xmm2=W]); билдер макрос (d,s1,s2,i) тут не совпадает */
            get_block32(&st, d->srcs[1], S1);       /* V */
            get_block32(&st, d->srcs[2], S2);       /* W (m32) */
        } else if (d->is_vex) {
            get_block32(&st, d->srcs[0], S1);
            if (d->nsrc >= 2 && !opnd_is_immed_int(d->srcs[1]))
                get_block32(&st, d->srcs[1], S2);
            else
                memset(S2, 0, 32);
        } else if (legacy_vw(d->opc)) {
            memcpy(S1, D, 32);                    /* V = dst (неявный источник) */
            get_block32(&st, d->srcs[0], S2);     /* W = rm */
        } else {
            get_block32(&st, d->srcs[0], S1);     /* W-формы: fn читает S1 */
            memset(S2, 0, 32);
        }

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
        /* §7.4 #111: VEX-алиасы → те же реализации (стейджинг (C)
         * унифицировал S1/S2 для обеих форм) */
        case OP_vpcmpgtq:  fn = b_pcmpgtq;  break;
        case OP_vpcmpeqq:  fn = b_pcmpeqq;  break;
        case OP_vpmulld:   fn = b_pmulld;   break;
        case OP_vpmuldq:   fn = b_pmuldq;   break;
        case OP_vpackusdw: fn = b_packusdw; break;
        case OP_vphminposuw: fn = b_phminposuw; break;
        case OP_vmpsadbw:  fn = b_mpsadbw;  break;
        case OP_vroundps:  fn = b_roundps;  break;
        case OP_vroundpd:  fn = b_roundpd;  break;
        case OP_vroundss:  fn = b_roundss;  break;
        case OP_vroundsd:  fn = b_roundsd;  break;
        case OP_vdpps:     fn = b_dpps;     break;
        case OP_vdppd:     fn = b_dppd;     break;
        case OP_vpminuw: fn = b_pminuw; break;
        case OP_vpmaxuw: fn = b_pmaxuw; break;
        case OP_vpminud: fn = b_pminud; break;
        case OP_vpmaxud: fn = b_pmaxud; break;
        case OP_vpminsb: fn = b_pminsb; break;
        case OP_vpmaxsb: fn = b_pmaxsb; break;
        case OP_vpminsd: fn = b_pminsd; break;
        case OP_vpmaxsd: fn = b_pmaxsd; break;
        case OP_vpmovsxbw: fn = b_pmovsxbw; break;
        case OP_vpmovsxbd: fn = b_pmovsxbd; break;
        case OP_vpmovsxbq: fn = b_pmovsxbq; break;
        case OP_vpmovsxwd: fn = b_pmovsxwd; break;
        case OP_vpmovsxwq: fn = b_pmovsxwq; break;
        case OP_vpmovsxdq: fn = b_pmovsxdq; break;
        case OP_vpmovzxbw: fn = b_pmovzxbw; break;
        case OP_vpmovzxbd: fn = b_pmovzxbd; break;
        case OP_vpmovzxbq: fn = b_pmovzxbq; break;
        case OP_vpmovzxwd: fn = b_pmovzxwd; break;
        case OP_vpmovzxwq: fn = b_pmovzxwq; break;
        case OP_vpmovzxdq: fn = b_pmovzxdq; break;
        case OP_vinsertps: fn = b_insertps; break;
        case OP_vpsllvd: fn = b_vpsllvd; break;
        case OP_vpsrlvd: fn = b_vpsrlvd; break;
        case OP_vpsravd: fn = b_vpsravd; break;
        case OP_vpsllvq: fn = b_vpsllvq; break;
        case OP_vpsrlvq: fn = b_vpsrlvq; break;
        case OP_vpsravq: fn = b_vpsravq; break;
        case OP_insertps: fn = b_insertps; break;
        /* §7.3 run #100: целочисленная VEX-семья (r10-fallback) */
        case OP_vpand:  fn = b_vpand;  break;
        case OP_vpandn: fn = b_vpandn; break;
        case OP_vpor:   fn = b_vpor;   break;
        case OP_vpxor:  fn = b_vpxor;  break;
        case OP_vpaddb: fn = b_vpaddb; break;
        case OP_vpaddw: fn = b_vpaddw; break;
        case OP_vpaddd: fn = b_vpaddd; break;
        case OP_vpaddq: fn = b_vpaddq; break;
        case OP_vpsubb: fn = b_vpsubb; break;
        case OP_vpsubw: fn = b_vpsubw; break;
        case OP_vpsubd: fn = b_vpsubd; break;
        case OP_vpsubq: fn = b_vpsubq; break;
        case OP_vpaddsb:  fn = b_vpaddsb;  break;
        case OP_vpaddsw:  fn = b_vpaddsw;  break;
        case OP_vpaddusb: fn = b_vpaddusb; break;
        case OP_vpaddusw: fn = b_vpaddusw; break;
        case OP_vpsubsb:  fn = b_vpsubsb;  break;
        case OP_vpsubsw:  fn = b_vpsubsw;  break;
        case OP_vpsubusb: fn = b_vpsubusb; break;
        case OP_vpsubusw: fn = b_vpsubusw; break;
        case OP_vpmullw:  fn = b_vpmullw;  break;
        case OP_vpmulhw:  fn = b_vpmulhw;  break;
        case OP_vpmulhuw: fn = b_vpmulhuw; break;
        case OP_vpmaddwd: fn = b_vpmaddwd; break;
        case OP_vpavgb:   fn = b_vpavgb;   break;
        case OP_vpavgw:   fn = b_vpavgw;   break;
        case OP_vpcmpeqb: fn = b_vpcmpeqb; break;
        case OP_vpcmpeqw: fn = b_vpcmpeqw; break;
        case OP_vpcmpeqd: fn = b_vpcmpeqd; break;
        case OP_vpcmpgtb: fn = b_vpcmpgtb; break;
        case OP_vpcmpgtw: fn = b_vpcmpgtw; break;
        case OP_vpcmpgtd: fn = b_vpcmpgtd; break;
        case OP_vpsllw:  fn = b_vpsllw;  break;
        case OP_vpsrlw:  fn = b_vpsrlw;  break;
        case OP_vpslld:  fn = b_vpslld;  break;
        case OP_vpsrld:  fn = b_vpsrld;  break;
        case OP_vpsllq:  fn = b_vpsllq;  break;
        case OP_vpsrlq:  fn = b_vpsrlq;  break;
        case OP_vpslldq: fn = b_vpslldq; break;
        case OP_vpsrldq: fn = b_vpsrldq; break;
        case OP_vpacksswb: fn = b_vpacksswb; break;
        case OP_vpackssdw: fn = b_vpackssdw; break;
        case OP_vpackuswb: fn = b_vpackuswb; break;
        case OP_vpunpcklbw: fn = b_vpunpcklbw; break;
        case OP_vpunpckhbw: fn = b_vpunpckhbw; break;
        case OP_vpunpcklwd: fn = b_vpunpcklwd; break;
        case OP_vpunpckhwd: fn = b_vpunpckhwd; break;
        case OP_vpunpckldq: fn = b_vpunpckldq; break;
        case OP_vpunpckhdq: fn = b_vpunpckhdq; break;
        case OP_vpunpcklqdq: fn = b_vpunpcklqdq; break;
        case OP_vpunpckhqdq: fn = b_vpunpckhqdq; break;
        case OP_vunpcklps: fn = b_vpunpckldq; break;   /* та же interleave */
        case OP_vunpckhps: fn = b_vpunpckhdq; break;
        case OP_vunpcklpd: fn = b_vpunpcklqdq; break;
        case OP_vunpckhpd: fn = b_vpunpckhqdq; break;
        case OP_vpblendw: fn = b_vpblendw; break;
        case OP_vpshufb:  fn = b_vpshufb;  break;
        case OP_vpsadbw:  fn = b_vpsadbw;  break;
        case OP_vpalignr: fn = b_vpalignr; break;
        default: break;
        }

        if (d->opc == OP_blendps || d->opc == OP_blendpd ||
            d->opc == OP_vpblendd ||
            d->opc == OP_vblendps || d->opc == OP_vblendpd) {
            /* §7.3 run #106 (D): см. blend_imm_block. imm-маска —
             * младший ниббл на половину; hi-половина 256-битных форм:
             * ps/d — imm>>4, pd — imm>>2.                        */
            int is_pd = (d->opc == OP_blendpd || d->opc == OP_vblendpd);
            int nl = is_pd ? 2 : 4;
            blend_imm_block(D, S1, S2, imm, is_pd, nl);
            if (d->is_ymm)
                blend_imm_block(D + 16, S1 + 16, S2 + 16, imm >> nl,
                                is_pd, nl);
        } else if (d->opc == OP_aesenc || d->opc == OP_aesdec ||
            d->opc == OP_aesenclast || d->opc == OP_aesdeclast ||
            d->opc == OP_aesimc ||
            d->opc == OP_vaesenc || d->opc == OP_vaesdec ||
            d->opc == OP_vaesenclast || d->opc == OP_vaesdeclast ||
            d->opc == OP_vaesimc) {
            /* §7.3 run #106: SWAP больше не нужен — стейджинг (C) даёт
             * S1=V(состояние), S2=W(ключ) для ОБОИХ форм:
             *  legacy [W(key), V(state)] → S1=D=V, S2=srcs[0]=W;
             *  VEX [V, W] → S1=srcs[0]=V, S2=srcs[1]=W.
             * aes_block ждёт (state, key) — теперь единообразно.
             * aesimc (unary, 1dst_1src): LEGACY_W/VEX S1=W — раньше
             * legacy aesimc проходил через swap → aes_block(D, 0, W)
             * → НУЛЕВОЙ результат. KAT: selftest (vaesenc), aestest,
             * t_s41 (легаси aesenc/aesimc).                             */
            /* VEX-алиасы нормализуем к легаси-опкоду — aes_block
             * диспегчится по нему */
            int aopc = d->opc;
            if (aopc == OP_vaesenc) aopc = OP_aesenc;
            else if (aopc == OP_vaesdec) aopc = OP_aesdec;
            else if (aopc == OP_vaesenclast) aopc = OP_aesenclast;
            else if (aopc == OP_vaesdeclast) aopc = OP_aesdeclast;
            else if (aopc == OP_vaesimc) aopc = OP_aesimc;
            aes_block(D, S1, S2, aopc);
            if (d->is_ymm) aes_block(D + 16, S1 + 16, S2 + 16, aopc);
        } else if (d->opc == OP_aeskeygenassist ||
                   d->opc == OP_vaeskeygenassist) {
            aes_keygen(D, S1, imm);
            if (d->is_ymm) aes_keygen(D + 16, S1 + 16, imm);
        } else if (d->opc == OP_pclmulqdq || d->opc == OP_vpclmulqdq) {
            pclmul_block(D, S1, S2, imm);
            if (d->is_ymm) pclmul_block(D + 16, S1 + 16, S2 + 16, imm);
        } else if (fn != NULL) {
            if (d->opc == OP_vmpsadbw && d->is_ymm) {
                /* §7.4 #111: VMPSADBW ymm — СВОИ биты на lane1:
                 * V-offset = imm8[3] (не [2]), W-block = imm8[5:4] (не [1:0]) */
                b_mpsadbw(D, S1, S2, imm & 7);
                b_mpsadbw(D + 16, S1 + 16, S2 + 16,
                          ((imm >> 3) & 1) << 2 | ((imm >> 4) & 3));
            } else if (d->is_ymm && is_widen(d->opc)) {
                /* pmovsx/zx 256: расширение — lane1 читает ВТОРУЮ
                 * половину младших 128Б источника, не старшие 128Б */
                fn(D, S1, S2, imm);
                fn(D + 16, S1 + widen_off(d->opc), S2 + 16, imm);
            } else {
                fn(D, S1, S2, imm);
                if (d->is_ymm) fn(D + 16, S1 + 16, S2 + 16, imm);
            }
        } else {
            break;
        }
        memcpy(EX(&st, di), D, 16);
        if (d->is_ymm) memcpy(EY(&st, di), D + 16, 16);
        else if (d->is_vex) memset(EY(&st, di), 0, 16); /* VEX.128: верх в ноль */
        break;
    }

    /* ── extract/pinsr: смешанные GPR/xmm ── */
    case OP_extractps: case OP_vextractps: {
        /* §7.4 #111: DR IR порядок РАЗНЫЙ: legacy [xmm, imm], VEX
         * [imm, xmm] (irmodel: vextractps $1, %xmm0, %ecx →
         * srcs=[imm, r77]) — раньше srcs[0] читался как xmm и для
         * VEX-формы xmm_idx(imm) давал мусорный индекс BUF (краш) */
        opnd_t xo = opnd_is_immed_int(d->srcs[0]) ? d->srcs[1] : d->srcs[0];
        const uint8_t *sv = EX(&st, xmm_idx(opnd_get_reg(xo)));
        uint32_t v = ld32(sv + (imm & 3) * 4);
        put_gpr_val(&st, d->dsts[0], v);
        break;
    }
    /* ── broadcast-семья: generic-путь (-nobcast / отказ inline) ──
     * §7.3: при -nobcast инструкция оставалась НАТИВНОЙ: результат
     * старшей половины писался в РЕАЛЬНЫЙ ymm (не в RAW_YHI-shadow,
     * который читают последующие стабы) → мусор в векторе → паника
     * аллокатора bun. Семантика SDM: broadcast младшего элемента
     * (mem-источник — точный размер, reg — элемент 0 lo-половины);
     * VEX.128 — верх в ноль, VEX.256 — дубль в hi.               */
    case OP_vbroadcastss: case OP_vbroadcastsd:
    case OP_vbroadcastf128: case OP_vbroadcasti128:
    case OP_vpbroadcastb: case OP_vpbroadcastw:
    case OP_vpbroadcastd: case OP_vpbroadcastq: {
        opnd_t s0 = d->srcs[0];
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        uint8_t D[32], elem[8] = {0};
        int esz;
        if (g_cfg.verbose >= 3)
            dr_fprintf(STDERR, "[P.A.S.S.][BC] opc=%d nsrc=%d mem=%d vex=%d is_ymm=%d\n",
                       d->opc, d->nsrc, opnd_is_memory_reference(s0),
                       d->is_vex, d->is_ymm);
        switch (d->opc) {
        case OP_vbroadcastss: case OP_vpbroadcastd: esz = 4; break;
        case OP_vbroadcastsd: case OP_vpbroadcastq: esz = 8; break;
        case OP_vpbroadcastw: esz = 2; break;
        default: esz = 1; break;                       /* vpbroadcastb */
        }
        if (d->opc == OP_vbroadcastf128 || d->opc == OP_vbroadcasti128) {
            /* только ymm-форма с m128 (SDM): иначе — в trap громко */
            if (!d->is_ymm || !opnd_is_memory_reference(s0)) {
                if (g_cfg.verbose >= 3)
                    dr_fprintf(STDERR, "[P.A.S.S.][BC] f128/i128 отказ: is_ymm=%d mem=%d\n",
                               d->is_ymm, opnd_is_memory_reference(s0));
                return 0;
            }
            emu_safe_read(&st, mem_addr(&st, s0), D, 16);
            memcpy(D + 16, D, 16);
        } else {
            if (opnd_is_memory_reference(s0)) {
                emu_safe_read(&st, mem_addr(&st, s0), elem, esz);
            } else if (is_vec_reg(s0)) {
                memcpy(elem, EX(&st, xmm_idx(opnd_get_reg(s0))), esz);
            } else {
                if (g_cfg.verbose >= 3)
                    dr_fprintf(STDERR, "[P.A.S.S.][BC] патология: src не mem и не vec-reg\n");
                return 0;   /* патологическая форма — trap-fallback */
            }
            int n = 16 / esz;                  /* элементов в 16Б половине */
            for (int q = 0; q < n; q++) memcpy(D + q * esz, elem, esz);
            if (d->is_ymm)
                for (int q = 0; q < n; q++) memcpy(D + 16 + q * esz, elem, esz);
        }
        memcpy(EX(&st, di), D, 16);
        if (d->is_ymm) memcpy(EY(&st, di), D + 16, 16);
        else memset(EY(&st, di), 0, 16);               /* VEX.128: верх 0 */
        break;
    }
    /* ── скалярная SSE-арифметика (VEX, fallback r10-форм) ──
     * §7.3 run #98: bun/JSC (r10-операнды JIT) уводит vaddsd и др.
     * в generic через r10-ветку диспетчера; emu их не умел → молча
     * пропускал (#97: op#686=10 в --version — bun выживал случайно,
     * rc=3 в JS). Inline-карта покрывает обычные формы; здесь —
     * только fallback. DR-канон VEX NDS: srcs[0]=V (Hx), srcs[1]=W
     * (reg/mem). dst = {результат elem0, СТАРШАЯ часть из V};
     * VEX.128 скаляр — верх ymm в ноль.
     * ТОЧНОСТЬ: клиент -mno-sse → x87. ss-семья: 64-битный
     * промежуточный хазард безопасен (2p+2=50 ≤ 64) — корректное
     * округление. sd add/sub: p+2 ≤ 64 — точны. sd mul/div: 2p+2=108
     * > 64 — теоретическое двойное округление ~2^-60 (документировано,
     * приоритет v0.3 — SSE-математика). min/max: семантика SSE (NaN
     * и равенство → ВТОРОЙ источник). sqrt: IEEE-точно.             */
    case OP_vaddss: case OP_vaddsd:
    case OP_vsubss: case OP_vsubsd:
    case OP_vmulss: case OP_vmulsd:
    case OP_vdivss: case OP_vdivsd:
    case OP_vminss: case OP_vminsd:
    case OP_vmaxss: case OP_vmaxsd:
    case OP_vsqrtss: case OP_vsqrtsd: {
        if (d->nsrc < 2 || !is_vec_reg(d->srcs[0])) return 0;
        int vi = xmm_idx(opnd_get_reg(d->srcs[0]));
        int di = xmm_idx(opnd_get_reg(d->dsts[0]));
        int esz = (d->opc == OP_vaddsd || d->opc == OP_vsubsd ||
                   d->opc == OP_vmulsd || d->opc == OP_vdivsd ||
                   d->opc == OP_vminsd || d->opc == OP_vmaxsd ||
                   d->opc == OP_vsqrtsd) ? 8 : 4;
        const uint8_t *V = EX(&st, vi);
        uint8_t W[16];
        if (opnd_is_memory_reference(d->srcs[1])) {
            emu_safe_read(&st, mem_addr(&st, d->srcs[1]), W, esz);
        } else if (is_vec_reg(d->srcs[1])) {
            memcpy(W, EX(&st, xmm_idx(opnd_get_reg(d->srcs[1]))), esz);
        } else {
            return 0;                     /* GPR-формы vcvtsi2* — не сюда */
        }
        uint8_t D[16];
        memcpy(D, V, 16);                 /* merge: старшая часть из V */
        if (esz == 8) {
            double a, b, o;
            memcpy(&a, V, 8);
            memcpy(&b, W, 8);
            switch (d->opc) {
            case OP_vaddsd: o = a + b; break;
            case OP_vsubsd: o = a - b; break;
            case OP_vmulsd: o = a * b; break;
            case OP_vdivsd: o = a / b; break;
            case OP_vsqrtsd: o = sqrt(b); break;
            case OP_vminsd:
                o = (isnan(a) || isnan(b)) ? b : (a < b ? a : b); break;
            default: /* vmaxsd */
                o = (isnan(a) || isnan(b)) ? b : (a > b ? a : b); break;
            }
            memcpy(D, &o, 8);
        } else {
            float a, b, o;
            memcpy(&a, V, 4);
            memcpy(&b, W, 4);
            switch (d->opc) {
            case OP_vaddss: o = (float)(a + b); break;
            case OP_vsubss: o = (float)(a - b); break;
            case OP_vmulss: o = (float)(a * b); break;
            case OP_vdivss: o = (float)(a / b); break;
            case OP_vsqrtss: o = (float)sqrt((double)b); break;
            case OP_vminss:
                o = (isnan(a) || isnan(b)) ? b : (a < b ? a : b); break;
            default: /* vmaxss */
                o = (isnan(a) || isnan(b)) ? b : (a > b ? a : b); break;
            }
            memcpy(D, &o, 4);
        }
        memcpy(EX(&st, di), D, 16);
        memset(EY(&st, di), 0, 16);       /* VEX.128 скаляр — верх 0 */
        break;
    }
    /* ── VMOV*: r10-fallback загрузок/сохранений (vmovd/q, aps/ups,
     * dqa/dqu, ss/sd 2-op, lddqu, ntdq/ntdqa) ── JSC/JIT обращается
     * к данным через r10-адресацию — inline-карта отклоняет, generic
     * обязан копировать 16/32Б между mem и shadow. VEX.128 reg-dst
     * обнуляет верх (инвариант shadow). 3-op vmovss/sd (V-merge).  */
    case OP_vmovd: case OP_vmovq: {
        opnd_t d0 = d->dsts[0];
        opnd_t s0 = d->srcs[0];
        int esz = (opnd_get_size(d0) == OPSZ_8 ||
                   opnd_get_size(s0) == OPSZ_8) ? 8 : 4;
        if (is_vec_reg(d0)) {                 /* GPR/mem → xmm */
            int di = xmm_idx(opnd_get_reg(d0));
            uint64_t v = 0;
            if (opnd_is_reg(s0) && is_gpr_reg(opnd_get_reg(s0)))
                v = *EG(&st, gpr_idx(opnd_get_reg(s0)));
            else if (opnd_is_memory_reference(s0))
                emu_safe_read(&st, mem_addr(&st, s0), &v, esz);
            else return 0;
            uint8_t D[16] = {0};
            memcpy(D, &v, esz);
            memcpy(EX(&st, di), D, 16);
            memset(EY(&st, di), 0, 16);       /* VEX.128: верх 0 */
        } else if (is_vec_reg(s0)) {          /* xmm → GPR/mem */
            int si = xmm_idx(opnd_get_reg(s0));
            uint64_t v = 0;
            memcpy(&v, EX(&st, si), esz);
            if (opnd_is_reg(d0) && is_gpr_reg(opnd_get_reg(d0)))
                put_gpr_val(&st, d0, v);
            else if (opnd_is_memory_reference(d0))
                emu_safe_write(&st, mem_addr(&st, d0), &v, esz);
            else return 0;
        } else return 0;
        break;
    }
    case OP_vmovaps: case OP_vmovups:
    case OP_vmovdqa: case OP_vmovdqu:
    case OP_vmovss: case OP_vmovsd:
    case OP_vlddqu: case OP_vmovntdq: case OP_vmovntdqa:
    case OP_vmovntps: case OP_vmovntpd: {
        opnd_t d0 = d->dsts[0];
        opnd_t s0 = d->srcs[0];
        int scalar = (d->opc == OP_vmovss || d->opc == OP_vmovsd);
        int esz = (d->opc == OP_vmovsd || d->opc == OP_vmovntpd) ? 8
                : (d->opc == OP_vmovss || d->opc == OP_vmovntps) ? 4 : 16;
        if (is_vec_reg(d0)) {
            int di = xmm_idx(opnd_get_reg(d0));
            if (opnd_is_memory_reference(s0)) {
                if (scalar) {
                    uint8_t D[16] = {0};
                    emu_safe_read(&st, mem_addr(&st, s0), D, esz);
                    memcpy(EX(&st, di), D, 16);      /* верх 0 */
                } else {
                    int full = (opnd_get_size(d0) == OPSZ_32);
                    emu_safe_read(&st, mem_addr(&st, s0), EX(&st, di), 16);
                    if (full) emu_safe_read(&st, mem_addr(&st, s0) + 16,
                                            EY(&st, di), 16);
                    else memset(EY(&st, di), 0, 16);
                }
            } else if (is_vec_reg(s0)) {
                int si = xmm_idx(opnd_get_reg(s0));
                if (scalar && d->nsrc >= 2 && is_vec_reg(d->srcs[1])) {
                    /* 3-op vmovss/sd: dst = {V верх, W elem0} */
                    int wi = xmm_idx(opnd_get_reg(d->srcs[1]));
                    uint8_t D[16];
                    memcpy(D, EX(&st, si), 16);
                    memcpy(D, EX(&st, wi), esz);
                    memcpy(EX(&st, di), D, 16);
                } else {
                    memcpy(EX(&st, di), EX(&st, si), 16);
                    if (opnd_get_size(d0) == OPSZ_32) {
                        if (opnd_get_size(s0) == OPSZ_32)
                            memcpy(EY(&st, di), EY(&st, si), 16);
                        else memset(EY(&st, di), 0, 16);
                    } else memset(EY(&st, di), 0, 16);
                }
            } else return 0;
        } else if (opnd_is_memory_reference(d0) && is_vec_reg(s0)) {
            int si = xmm_idx(opnd_get_reg(s0));
            if (scalar) {
                emu_safe_write(&st, mem_addr(&st, d0), EX(&st, si), esz);
            } else {
                int full = (opnd_get_size(s0) == OPSZ_32);
                emu_safe_write(&st, mem_addr(&st, d0), EX(&st, si), 16);
                if (full) emu_safe_write(&st, mem_addr(&st, d0) + 16,
                                         EY(&st, si), 16);
            }
        } else return 0;
        break;
    }
    /* ── vpmovmskb (r10-fallback: dst GPR, у JSC r10/movmskb) ── */
    case OP_vpmovmskb: {
        if (!is_vec_reg(d->srcs[0])) return 0;
        int vi = xmm_idx(opnd_get_reg(d->srcs[0]));
        const uint8_t *S = EX(&st, vi);
        const uint8_t *H = d->is_ymm ? EY(&st, vi) : NULL;
        uint32_t m = 0;
        for (int i = 0; i < 16; i++)
            if (S[i] & 0x80) m |= 1u << i;
        if (d->is_ymm)
            for (int i = 0; i < 16; i++)
                if (H[i] & 0x80) m |= 1u << (i + 16);
        put_gpr_val(&st, d->dsts[0], m);
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
     * Читаем через get_block32: reg → EX+EY, mem → чтение по размеру.
     * §7.3 run #106 (A): DR-модель PTEST = 0dst_2src (INSTR_CREATE_ptest
     * = instr_create_0dst_2src; проверено irmodel на decode) — операнды
     * лежат в srcs[0]/srcs[1], а dsts[0] НЕ СУЩЕСТВУЕТ (ndst=0). Чтение
     * dsts[0] возвращало мусорный opnd_t → xmm_idx() от мусорного reg →
     * чтение чужого слота BUF → неверные флаги после ptest.           */
    case OP_ptest: case OP_vptest: {
        uint8_t s1b[32], s2v[32];
        get_block32(&st, d->srcs[0], s1b);          /* 0dst_2src: s1 в srcs[0] */
        get_block32(&st, d->srcs[1], s2v);          /* s2: reg или память */
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
        get_block32(&st, d->srcs[0], s1b);          /* 0dst_2src: см. (A) выше */
        get_block32(&st, d->srcs[1], s2v);
        vtest(&st, s1b, s2v, d->opc == OP_vtestpd);
        break;
    }

    /* ── переменные blendv ──
     * §7.3 run #106 (C-2): DR-модели форм РАЗЛИЧАЮТСЯ!
     *  legacy blendvps/pd: 1dst_3src [W(rm), XMM0(маска), V(dst)] —
     *      вектор-источник в srcs[0], маска — неявный XMM0.
     *  VEX vblendvps/pd (без case-метки в диспетчер не доходит,
     *      ветка для устойчивости): [V, W, mask].
     * Раньше вектор читался из srcs[1] — у legacy это МАСКА XMM0:
     * blend выбирал элементы из маски вместо источника.            */
    case OP_blendvps: case OP_blendvpd:
    case OP_vblendvps: case OP_vblendvpd: {
        /* источник может быть памятью: берём полные 32Б через get_block32,
         * младшая половина — s2v, старшая — s2v+16 (для reg это EY(idx)). */
        uint8_t s2v[32], m32[32], s1v[32];
        uint8_t *dv = EX(&st, xmm_idx(opnd_get_reg(d->dsts[0])));
        const uint8_t *m, *s1;
        if (d->is_vex) {
            /* §7.4 #111: VEX-форма [V, W, mask] — незамаскированные
             * элементы берутся из V (srcs[0]), НЕ из старого dst
             * (раньше merge-базой был мусорный спил dst-регистра) */
            get_block32(&st, d->srcs[0], s1v);          /* V */
            get_block32(&st, d->srcs[1], s2v);          /* W */
            get_block32(&st, d->srcs[2], m32);          /* маска */
            m = m32;
            s1 = s1v;
        } else {
            get_block32(&st, d->srcs[0], s2v);          /* legacy: W в srcs[0] */
            m = EX(&st, 0);                             /* маска — XMM0 */
            s1 = dv;                                    /* V = старый dst */
        }
        if (d->opc == OP_blendvps || d->opc == OP_vblendvps)
            b_blendvps_m(dv, s1, s2v, m);
        else
            b_blendvpd_m(dv, s1, s2v, m);
        if (d->is_ymm) {
            int di = xmm_idx(opnd_get_reg(d->dsts[0]));
            const uint8_t *mhi = d->is_vex ? (m32 + 16) : EY(&st, 0);
            const uint8_t *vhi = d->is_vex ? (s1v + 16) : EY(&st, di);
            if (d->opc == OP_blendvps || d->opc == OP_vblendvps)
                b_blendvps_m(EY(&st, di), vhi, s2v + 16, mhi);
            else
                b_blendvpd_m(EY(&st, di), vhi, s2v + 16, mhi);
        } else if (d->is_vex) {
            int di = xmm_idx(opnd_get_reg(d->dsts[0]));
            memset(EY(&st, di), 0, 16);                     /* VEX.128 */
        }
        break;
    }
    case OP_pblendvb: case OP_vpblendvb: {
        uint8_t *dv = EX(&st, xmm_idx(opnd_get_reg(d->dsts[0])));
        /* legacy pblendvb: [W(rm), XMM0(маска), V(dst)] — W в srcs[0]
         * (раньше src2_block брал srcs[1] = маску!); VEX vpblendvb:
         * [V, W, mask] — без case-метки не доходит, ветка для устойчивости */
        uint8_t t1[32], t3[32];
        const uint8_t *s1b, *s2b, *m;
        if (d->is_vex) {
            /* VEX [V, W, mask]: база — srcs[0], маска — srcs[2]
             * (раньше маска читалась из XMM0 → 0x25252525-мусор) */
            s1b = get_block16(&st, d->srcs[0], t1);
            s2b = get_block16(&st, d->srcs[1], t2);
            m   = get_block16(&st, d->srcs[2], t3);
        } else {
            s1b = dv;                                   /* V = старый dst */
            s2b = get_block16(&st, d->srcs[0], t2);
            m   = EX(&st, 0);
        }
        b_pblendvb_m(dv, s1b, s2b, m);
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

    /* ── vpmaskmov / vmaskmov / gather ──
     * §7.4 #111: load и store — один opc (различие в dst: reg/mem),
     * ветвление внутри pmaskmov_op; vmaskmovps/pd (AVX1) — та же
     * механика движения данных.                                  */
    case OP_vpmaskmovd: case OP_vpmaskmovq:
    case OP_vmaskmovps: case OP_vmaskmovpd:
        pmaskmov_op(&st, d);
        break;
    case OP_vpgatherdd: case OP_vpgatherdq: case OP_vpgatherqd:
    case OP_vpgatherqq: case OP_vgatherdps: case OP_vgatherdpd:
    case OP_vgatherqps: case OP_vgatherqpd:
        gather_op(&st, d);
        break;

    /* ── cvt-ширины и permil (§7.4 #111) ── */
    case OP_vcvtdq2pd:
        cvt_dq2pd(&st, d);
        break;
    case OP_vcvtpd2dq:
        cvt_pd2dq(&st, d, 0);
        break;
    case OP_vcvttpd2dq:
        cvt_pd2dq(&st, d, 1);
        break;
    case OP_vpermilps: case OP_vpermilpd:
        permil_op(&st, d);
        break;
    case OP_vmovlps: case OP_vmovhps: case OP_vmovlpd: case OP_vmovhpd:
        halfmove_op(&st, d);
        break;

    /* ── строковые ──
     * §7.4 #111: VEX-формы имеют ТОТ ЖЕ результат/флаги (irmodel:
     * dst=ECX/XMM0, srcs=[A,B,imm,EAX,EDX] у всех восьми) — общий
     * string_op, семантика выверена t_istr.S (нативная проба). */
    case OP_pcmpestri: case OP_vpcmpestri:
        string_op(&st, d, 1, 1); break;
    case OP_pcmpestrm: case OP_vpcmpestrm:
        string_op(&st, d, 1, 0); break;
    case OP_pcmpistri: case OP_vpcmpistri:
        string_op(&st, d, 0, 1); break;
    case OP_pcmpistrm: case OP_vpcmpistrm:
        string_op(&st, d, 0, 0); break;

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
