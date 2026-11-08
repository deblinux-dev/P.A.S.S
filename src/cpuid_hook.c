/* P.A.S.S. — cpuid_hook.c
 * Маскирование результатов CPUID и XGETBV: программа видит
 * «целевой» процессор + (опционально) эмулируемые фичи.
 * Благодаря этому диспетчеры (glibc ifunc, компиляторные
 * multiversion-ветки) сами выбирают пути, которые P.A.S.S.
 * умеет исполнять.                                             */
#include "pass.h"


/* Битовые карты: (leaf, subleaf) → {ecx_mask, edx_mask, ebx_mask} */
typedef struct {
    uint32_t leaf, subleaf;
    uint32_t eax_set, eax_clr;
    uint32_t ebx_set, ebx_clr;
    uint32_t ecx_set, ecx_clr;
    uint32_t edx_set, edx_clr;
} cpuid_rule_t;

/* Таблица соответствия «фича → бит CPUID» */
typedef struct { feat_t f; int leaf, subleaf; int reg; int bit; } feat_bit_t;
/* reg: 0=eax 1=ebx 2=ecx 3=edx */
static const feat_bit_t feat_bits[] = {
    { F_SSE3,   1, 0, 2,  0 },
    { F_PCLMUL, 1, 0, 2,  1 },
    { F_SSSE3,  1, 0, 2,  9 },
    { F_FMA,    1, 0, 2, 12 },
    { F_SSE41,  1, 0, 2, 19 },
    { F_SSE42,  1, 0, 2, 20 },
    { F_MOVBE,  1, 0, 2, 22 },
    { F_POPCNT, 1, 0, 2, 23 },
    { F_AES,    1, 0, 2, 25 },
    { F_XSAVE,  1, 0, 2, 26 },
    { F_OSXSAVE,1, 0, 2, 27 },
    { F_AVX,    1, 0, 2, 28 },
    { F_F16C,   1, 0, 2, 29 },
    { F_BMI1,   7, 0, 1,  3 },
    { F_AVX2,   7, 0, 1,  5 },
    { F_BMI2,   7, 0, 1,  8 },
    { F_LZCNT,  0x80000001u, 0, 2, 5 },
    { F_PREFW,  0x80000001u, 0, 2, 8 },
};

int g_no_spoof = 0;

/* Вычисленная витрина CPUID (заполняется в cpuid_init) */
static struct {
    uint32_t max_leaf, max_ext_leaf;
    uint32_t l1[4];      /* leaf 1      */
    uint32_t l7[4];      /* leaf 7.0    */
    uint32_t l81[4];     /* 0x80000001  */
    uint32_t ld0_eax;    /* leaf 0xD.0  (маскирован под XCR0) */
    uint32_t ld1_size;   /* leaf 0xD.1 EAX: размер XSAVE-области */
    int      have_l7, have_l81, have_ld;
} g_cpuid;

/* Эмулируемые фичи (заявлены в CPUID, если expose_emulated).
 * -emu_mask <hex> переопределяет набор (бисекция: бит k = feat_t k). */
uint64_t g_emu_mask_override = 0;
int g_emu_mask_set_ = 0;

static featset_t emulated_set(void)
{
    if (g_emu_mask_set_)
        return (featset_t)g_emu_mask_override;
    return FEAT(F_SSE41) | FEAT(F_SSE42) | FEAT(F_POPCNT) | FEAT(F_AES) |
           FEAT(F_PCLMUL) | FEAT(F_AVX) | FEAT(F_AVX2) | FEAT(F_FMA) |
           FEAT(F_BMI1) | FEAT(F_BMI2) | FEAT(F_MOVBE) | FEAT(F_LZCNT) |
           FEAT(F_F16C);
}

/* Итоговая витрина фич: (host ∩ profile) ∪ (эмулируемые ∧ expose) */
featset_t exposed_features(void)
{
    featset_t exposed = g_cfg.profile & g_cfg.host;
    if (g_cfg.expose_emulated) {
        featset_t emu = emulated_set();
        /* светим только то, что реально умеем эмулировать;
           хостовая поддержка не требуется — эмулируем сами */
        exposed |= emu;
    }
    /* AVX без OSXSAVE/XSAVE ломает диспетчеры: добавляем при expose */
    if (has(exposed, F_AVX)) {
        exposed |= FEAT(F_XSAVE) | FEAT(F_OSXSAVE);
    }
    return exposed;
}

void cpuid_init(void)
{
    uint32_t a, b, c, d;
    featset_t exposed = exposed_features();

    memset(&g_cpuid, 0, sizeof(g_cpuid));
    pass_cpuid(0, &a, &b, &c, &d);
    g_cpuid.max_leaf = a;
    pass_cpuid(0x80000000, &a, &b, &c, &d);
    g_cpuid.max_ext_leaf = a;

    if (pass_cpuid(1, &a, &b, &c, &d)) {
        g_cpuid.l1[0] = a; g_cpuid.l1[1] = b;
        g_cpuid.l1[2] = c; g_cpuid.l1[3] = d;
    }
    if (pass_cpuid_count(7, 0, &a, &b, &c, &d)) {
        g_cpuid.have_l7 = 1;
        g_cpuid.l7[0] = a; g_cpuid.l7[1] = b;
        g_cpuid.l7[2] = c; g_cpuid.l7[3] = d;
    }
    if (pass_cpuid(0x80000001, &a, &b, &c, &d)) {
        g_cpuid.have_l81 = 1;
        g_cpuid.l81[0] = a; g_cpuid.l81[1] = b;
        g_cpuid.l81[2] = c; g_cpuid.l81[3] = d;
    }
    if (pass_cpuid_count(0xD, 0, &a, &b, &c, &d)) {
        g_cpuid.have_ld = 1;
    }

    /* применяем витрину к копиям листьев */
    uint32_t regs1[4] = { g_cpuid.l1[0], g_cpuid.l1[1], g_cpuid.l1[2], g_cpuid.l1[3] };
    for (size_t i = 0; i < sizeof(feat_bits)/sizeof(feat_bits[0]); i++) {
        const feat_bit_t *fb = &feat_bits[i];
        int expose = has(exposed, fb->f);
        if (fb->leaf != 1) continue;
        if (expose) regs1[fb->reg] |=  (1u << fb->bit);
        else        regs1[fb->reg] &= ~(1u << fb->bit);
    }
    /* AVX скрыт → гасим OSXSAVE/XSAVE/XCR0-связанные биты и leaf D */
    if (!has(exposed, F_OSXSAVE)) {
        regs1[2] &= ~(1u << 26);
        regs1[2] &= ~(1u << 27);
    } else {
        regs1[2] |= (1u << 26) | (1u << 27);
    }
    g_cpuid.l1[0] = regs1[0]; g_cpuid.l1[1] = regs1[1];
    g_cpuid.l1[2] = regs1[2]; g_cpuid.l1[3] = regs1[3];

    if (g_cpuid.have_l7) {
        uint32_t regs7[4] = { g_cpuid.l7[0], g_cpuid.l7[1], g_cpuid.l7[2], g_cpuid.l7[3] };
        for (size_t i = 0; i < sizeof(feat_bits)/sizeof(feat_bits[0]); i++) {
            const feat_bit_t *fb = &feat_bits[i];
            if (fb->leaf != 7) continue;
            if (has(exposed, fb->f)) regs7[fb->reg] |=  (1u << fb->bit);
            else                     regs7[fb->reg] &= ~(1u << fb->bit);
        }
        /* AVX-512 и прочее, что не эмулируем — всегда глушим */
        regs7[1] &= ~((1u<<16)|(1u<<17)|(1u<<18)|(1u<<19)|(1u<<20)|(1u<<21)|
                      (1u<<22)|(1u<<23)|(1u<<24)|(1u<<25)|(1u<<26)|(1u<<27)|
                      (1u<<28)|(1u<<29)|(1u<<30)|(1u<<31));
        regs7[2] &= ~(1u<<1);   /* AVX512VBMI etc */
        g_cpuid.l7[0] = regs7[0]; g_cpuid.l7[1] = regs7[1];
        g_cpuid.l7[2] = regs7[2]; g_cpuid.l7[3] = regs7[3];
    }
    if (g_cpuid.have_l81) {
        uint32_t regs8[4] = { g_cpuid.l81[0], g_cpuid.l81[1], g_cpuid.l81[2], g_cpuid.l81[3] };
        for (size_t i = 0; i < sizeof(feat_bits)/sizeof(feat_bits[0]); i++) {
            const feat_bit_t *fb = &feat_bits[i];
            if (fb->leaf != 0x80000001u) continue;
            if (has(exposed, fb->f)) regs8[fb->reg] |=  (1u << fb->bit);
            else                     regs8[fb->reg] &= ~(1u << fb->bit);
        }
        /* AVX-512 (EDX 16..27) — глушим */
        regs8[3] &= ~(((1u<<16)-1) << 16);
        g_cpuid.l81[0] = regs8[0]; g_cpuid.l81[1] = regs8[1];
        g_cpuid.l81[2] = regs8[2]; g_cpuid.l81[3] = regs8[3];
    }
    /* лист 0xD: xsave-витрина — СОГЛАСОВАНА с подменённым XCR0 (0x7 или 0x3):
       EAX = только биты выставленных состояний; подклеивать хостовые биты
       (AVX-512/PT/PKRU/AMX) нельзя — glibc сверяет XCR0 с листом D,
       рассинхрон даёт порчу при xsave-инициализации (баг v0.1).        */
    if (has(exposed, F_XSAVE)) {
        g_cpuid.ld0_eax = 0x3 | (has(exposed, F_AVX) ? 0x4 : 0);
        /* размер XSAVE-области для выставленных состояний:
           legacy 512 + header 64 = 576; +AVX-область 256 = 832          */
        g_cpuid.ld1_size = has(exposed, F_AVX) ? 832 : 576;
    } else {
        g_cpuid.ld0_eax = 0;   /* XSAVE скрыт — программа не должна звать xsave */
        g_cpuid.ld1_size = 0;
    }
    /* флаг «спуфинг не нужен» (native): cpuid проходит нативно */
    {
        featset_t emu = emulated_set();
        featset_t cur = g_cfg.profile & g_cfg.host;
        if (g_cfg.expose_emulated) cur |= emu;
        if (cur == g_cfg.host && !(~g_cfg.host & g_cfg.profile))
            g_no_spoof = 1;
    }

    if (g_cfg.verbose) {
        dr_fprintf(STDERR, "[P.A.S.S.] host leaf1 ecx=%08x edx=%08x, spoofed l1 ecx=%08x edx=%08x\n",
                   g_cpuid.l1[2] & 0, 0, g_cpuid.l1[2], g_cpuid.l1[3]);
        {
            uint32_t ra, rb, rc, rd;
            if (pass_cpuid(1, &ra, &rb, &rc, &rd))
                dr_fprintf(STDERR, "[P.A.S.S.] live cpuid(1): ecx=%08x edx=%08x\n", rc, rd);
            else
                dr_fprintf(STDERR, "[P.A.S.S.] live cpuid(1) FAILED\n");
        }
        dr_fprintf(STDERR, "[P.A.S.S.] exposed: sse4.1=%d sse4.2=%d popcnt=%d avx=%d avx2=%d fma=%d bmi1=%d bmi2=%d aes=%d\n",
                   has(exposed, F_SSE41), has(exposed, F_SSE42),
                   has(exposed, F_POPCNT), has(exposed, F_AVX),
                   has(exposed, F_AVX2), has(exposed, F_FMA),
                   has(exposed, F_BMI1), has(exposed, F_BMI2),
                   has(exposed, F_AES));
    }
}

/* ─────────── clean call: подменённый CPUID ───────────
 * Вход:  BUF_CPU+0  = leaf, BUF_CPU+8 = subleaf
 * Выход: BUF_CPU+16..40 = eax, ebx, ecx, edx                    */
void emu_cpuid(void)
{
    void *dc = dr_get_current_drcontext();
    pass_ensure_tls(dc);
    uint8_t *buf = (uint8_t *)drmgr_get_tls_field(dc, g_tls_idx_buf);
    /* раскладка 8-байтовых слотов, согласована с перезагрузкой в asm:
       [0]=leaf, [1]=subleaf, [2..5]=eax,ebx,ecx,edx                   */
    uint64_t *cpu = (uint64_t *)(buf + BUF_CPU);
    uint32_t leaf = (uint32_t)cpu[0], sub = (uint32_t)cpu[1];
    uint32_t out[4] = { 0, 0, 0, 0 };

    if (leaf == 0) {
        out[0] = g_cpuid.max_leaf;
        out[1] = 0x756e6547; out[2] = 0x6c65746e; out[3] = 0x49656e69; /* GenuineIntel */
    } else if (leaf == 1) {
        out[0] = g_cpuid.l1[0]; out[1] = g_cpuid.l1[1];
        out[2] = g_cpuid.l1[2]; out[3] = g_cpuid.l1[3];
    } else if (leaf == 7 && sub == 0) {
        out[0] = g_cpuid.have_l7 ? g_cpuid.l7[0] : 0;
        out[1] = g_cpuid.have_l7 ? g_cpuid.l7[1] : 0;
        out[2] = g_cpuid.have_l7 ? g_cpuid.l7[2] : 0;
        out[3] = g_cpuid.have_l7 ? g_cpuid.l7[3] : 0;
    } else if (leaf == 0xD) {
        if (sub == 0) {
            out[0] = g_cpuid.ld0_eax;
            /* ECX:EDX (верхние биты XCR0-состояний) — глушим */
            out[2] = 0; out[3] = 0;
        } else if (sub == 1) {
            out[0] = g_cpuid.ld1_size;  /* размер области для XCR0 */
            out[1] = 0;                 /* XSS-размер (супервизор-состояния выкл) */
            out[2] = 0; out[3] = 0;
        } else {
            /* AVX-512/PT/PKRU/AMX и пр. — состояния не выставлены:
               размер 0, адреса 0 (как на CPU без этих состояний) */
            out[0] = 0; out[1] = 0; out[2] = 0; out[3] = 0;
        }
    } else if (leaf == 0x80000000u) {
        out[0] = g_cpuid.max_ext_leaf;
    } else if (leaf == 0x80000001u) {
        out[0] = g_cpuid.l81[0]; out[1] = g_cpuid.l81[1];
        out[2] = g_cpuid.l81[2]; out[3] = g_cpuid.l81[3];
    } else {
        /* остальные листья — прозрачно от хоста */
        uint32_t a, b, c, d;
        if (leaf < 0x80000000u) {
            if (pass_cpuid_count(leaf, sub, &a, &b, &c, &d)) {
                out[0] = a; out[1] = b; out[2] = c; out[3] = d;
            }
        } else {
            if (pass_cpuid(leaf, &a, &b, &c, &d)) {
                out[0] = a; out[1] = b; out[2] = c; out[3] = d;
            }
        }
    }
    cpu[2] = out[0]; cpu[3] = out[1]; cpu[4] = out[2]; cpu[5] = out[3];
    if (g_cfg.verbose >= 2)
        dr_fprintf(STDERR, "[P.A.S.S.] emu_cpuid leaf=%08x sub=%u -> eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
                   leaf, sub, out[0], out[1], out[2], out[3]);
}

void rewrite_cpuid(void *dc, instrlist_t *bb, instr_t *where)
{
    /* mov [BUF_CPU+0], rax ; mov [BUF_CPU+8], rcx ; call emu_cpuid ;
       mov rax/rbx/rcx/rdx, [BUF_CPU+16..40]                        */
    carrier_begin(dc, bb, where);
    pass_ins(dc, bb, where, INSTR_CREATE_mov_st(
        dc, buf_opnd(BUF_CPU + 0, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, where, INSTR_CREATE_mov_st(
        dc, buf_opnd(BUF_CPU + 8, OPSZ_8), opnd_create_reg(DR_REG_RCX)));
    dr_insert_clean_call(dc, bb, where, (void *)emu_cpuid, false, 0);
    pass_ins(dc, bb, where, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_CPU + 16, OPSZ_8)));
    pass_ins(dc, bb, where, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RBX), buf_opnd(BUF_CPU + 24, OPSZ_8)));
    pass_ins(dc, bb, where, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RCX), buf_opnd(BUF_CPU + 32, OPSZ_8)));
    pass_ins(dc, bb, where, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RDX), buf_opnd(BUF_CPU + 40, OPSZ_8)));
    carrier_end(dc, bb, where, 0);
    instrlist_remove(bb, where);
    instr_destroy(dc, where);
}

void rewrite_xgetbv(void *dc, instrlist_t *bb, instr_t *where)
{
    /* xgetbv → eax/edx = фейковый XCR0: x87|SSE (+AVX если светим) */
    ptr_int_t xcr0 = 0x3;   /* XCR0_CW | XCR0_SSE */
    featset_t exposed = exposed_features();
    if (has(exposed, F_AVX)) xcr0 |= 0x4;
    pass_ins(dc, bb, where, INSTR_CREATE_mov_imm(
        dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT32((int)xcr0)));
    pass_ins(dc, bb, where, INSTR_CREATE_mov_imm(
        dc, opnd_create_reg(DR_REG_EDX), OPND_CREATE_INT32(0)));
    instrlist_remove(bb, where);
    instr_destroy(dc, where);
}
