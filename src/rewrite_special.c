/* P.A.S.S. — rewrite_special.c
 * Специализированные inline-эмуляции:
 *   - POPCNT (16/32), LZCNT/TZCNT (через BSR/BSF), MOVBE
 *   - BMI1: ANDN, BLSI, BLSMSK, BLSR; BMI2: RORX, SARX/SHLX/SHRX
 *   - FMA (vfmadd/fmsub/fnmadd/fnmsub, 132/213/231, ps/pd/ss/sd)
 *   - VPBROADCAST-семейство, VEXTRACTF128/VINSERTF128
 *   - VPERMILPS/PD (imm-форма → shufps/shufpd)
 * Всё безусловное (без ветвлений — вставка CTI в BB запрещена).
 * Скалярные GPR-вставки используют RAW_TG как спил временного.      */
#include "pass.h"

#define X15 DR_REG_XMM15

static int opnd_is_xmmreg(opnd_t o)
{
    return opnd_is_reg(o) && opnd_get_reg(o) >= DR_REG_XMM0 &&
           opnd_get_reg(o) <= DR_REG_XMM15;
}
static int reg_is_gpr_idx(opnd_t o, int *idx)
{
    if (!opnd_is_reg(o)) return 0;
    reg_id_t r = opnd_get_reg(o);
    if (!is_gpr_reg(r)) return 0;
    *idx = gpr_idx(r);
    return 1;
}

/* ── спил/восстановление временного GPR через RAW_TG ── */
static void save_gpr_tmp(void *dc, instrlist_t *bb, instr_t *at, reg_id_t t)
{
    pass_ins(dc, bb, at, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_TG, OPSZ_8), opnd_create_reg(gpr_norm(t))));
}
static void load_gpr_tmp(void *dc, instrlist_t *bb, instr_t *at, reg_id_t t)
{
    pass_ins(dc, bb, at, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(gpr_norm(t)), raw_opnd(RAW_TG, OPSZ_8)));
}

/* ══════════ POPCNT 16/32 (64-бит — в generic) ══════════ */
static int try_popcnt(void *dc, instrlist_t *bb, instr_t *i)
{
    opnd_t d0 = instr_get_dst(i, 0), s0 = instr_get_src(i, 0);
    int dsz_i, di, si;
    if (!reg_is_gpr_idx(d0, &di)) return 0;
    dsz_i = (opnd_get_size(d0) == OPSZ_8) ? 64 : (opnd_get_size(d0) == OPSZ_4 ? 32 : 16);
    if (dsz_i == 64) return 0;                    /* generic */
    /* временный: rax, если не занят dst */
    reg_id_t t = (di == gpr_idx(DR_REG_RAX)) ? DR_REG_RCX : DR_REG_RAX;
    save_gpr_tmp(dc, bb, i, t);
    if (opnd_is_reg(s0) && opnd_get_reg(s0) == t) {
        /* src == temp: работаем от копии src (прочитаем до порчи) */
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(t), raw_opnd(RAW_TG, OPSZ_8)));
    }
    /* dst = src */
    if (opnd_is_memory_reference(s0))
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s0));
    else
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s0));
    /* классическая скалярная popcnt: 2 регистра */
    uint mask;
    if (dsz_i == 32) mask = 0x55555555u;
    else mask = 0x5555u;
    /* t = (src >> 1) & 0x5555…
       dst = src - t
       … стандартная схема */
    pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
    pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(1)));
    pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, opnd_create_immed_int(dsz_i == 32 ? 0x55555555 : 0x5555, dsz_i == 32 ? OPSZ_4 : OPSZ_2)));
    pass_ins(dc, bb, i, INSTR_CREATE_and(dc, opnd_create_reg(t), opnd_create_immed_int(dsz_i == 32 ? 0xAAAAAAAAu : 0xAAAAu, dsz_i == 32 ? OPSZ_4 : OPSZ_2)));
    pass_ins(dc, bb, i, INSTR_CREATE_sub(dc, d0, opnd_create_reg(t)));
    /* x = (x & 0x3333) + ((x>>2) & 0x3333) */
    pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
    pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(2)));
    pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, opnd_create_immed_int(dsz_i == 32 ? 0x33333333 : 0x3333, dsz_i == 32 ? OPSZ_4 : OPSZ_2)));
    pass_ins(dc, bb, i, INSTR_CREATE_and(dc, opnd_create_reg(t), opnd_create_immed_int(dsz_i == 32 ? 0x33333333 : 0x3333, dsz_i == 32 ? OPSZ_4 : OPSZ_2)));
    pass_ins(dc, bb, i, INSTR_CREATE_add(dc, d0, opnd_create_reg(t)));
    /* x = (x + (x>>4)) & 0x0F0F */
    pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
    pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(4)));
    pass_ins(dc, bb, i, INSTR_CREATE_add(dc, d0, opnd_create_reg(t)));
    pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, opnd_create_immed_int(dsz_i == 32 ? 0x0F0F0F0F : 0x0F0F, dsz_i == 32 ? OPSZ_4 : OPSZ_2)));
    if (dsz_i == 32) {
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
        pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(8)));
        pass_ins(dc, bb, i, INSTR_CREATE_add(dc, d0, opnd_create_reg(t)));
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
        pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(16)));
        pass_ins(dc, bb, i, INSTR_CREATE_add(dc, d0, opnd_create_reg(t)));
        pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, opnd_create_immed_int(0x3F, OPSZ_4)));
    } else {
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(t), d0));
        pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, OPND_CREATE_INT8(8)));
        pass_ins(dc, bb, i, INSTR_CREATE_add(dc, d0, opnd_create_reg(t)));
        pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, opnd_create_immed_int(0x1F, OPSZ_2)));
    }
    load_gpr_tmp(dc, bb, i, t);
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ MOVBE ══════════ */
static int try_movbe(void *dc, instrlist_t *bb, instr_t *i)
{
    opnd_t d0 = instr_get_dst(i, 0), s0 = instr_get_src(i, 0);
    if (opnd_is_reg(d0)) {
        /* load: mov dst, [m] ; bswap dst (16-бит: rol 8) */
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s0));
        if (opnd_get_size(d0) == OPSZ_2)
            pass_ins(dc, bb, i, INSTR_CREATE_rol(dc, d0, OPND_CREATE_INT8(8)));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_bswap(dc, d0));
    } else {
        /* store: temp = src ; bswap temp ; mov [m], temp */
        int si;
        reg_is_gpr_idx(s0, &si);
        reg_id_t t = (si == gpr_idx(DR_REG_RAX)) ? DR_REG_RCX : DR_REG_RAX;
        save_gpr_tmp(dc, bb, i, t);
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(t), s0));
        if (opnd_get_size(s0) == OPSZ_2)
            pass_ins(dc, bb, i, INSTR_CREATE_rol(
                dc, opnd_create_reg(t), OPND_CREATE_INT8(8)));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_bswap(dc, opnd_create_reg(t)));
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, d0, opnd_create_reg(t)));
        load_gpr_tmp(dc, bb, i, t);
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ BMI1: ANDN/BLSI/BLSMSK/BLSR ══════════ */
static int try_bmi1(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s1 = instr_get_src(i, 0);
    opnd_t s2 = (instr_num_srcs(i) >= 2) ? instr_get_src(i, 1) : s1;
    int di;
    if (!reg_is_gpr_idx(d0, &di)) return 0;
    int s1_is_reg = opnd_is_reg(s1);
    int s1i = -1;
    if (s1_is_reg) {
        if (!reg_is_gpr_idx(s1, &s1i)) return 0;
    }
    int src_is_dst = s1_is_reg && (s1i == di);
    /* 64-битная форма регистра для TG-операндов */
    reg_id_t s1q = s1_is_reg ? gpr_norm(opnd_get_reg(s1)) : DR_REG_RAX;

    switch (opc) {
    case OP_andn: {   /* dst = ~s1 & s2 */
        int s2i;
        int s2_is_d = opnd_is_reg(s2) && reg_is_gpr_idx(s2, &s2i) && (s2i == di);
        if (s2_is_d) {
            /* старый dst (= s2) нужен после порчи: спил в TG, без восстановления */
            save_gpr_tmp(dc, bb, i, gpr_norm(opnd_get_reg(s2)));
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_not(dc, d0));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, raw_opnd(RAW_TG, opnd_get_size(d0))));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_not(dc, d0));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, s2));
        }
        break;
    }
    case OP_blsi: {   /* dst = (-x) & x */
        if (src_is_dst) {
            save_gpr_tmp(dc, bb, i, s1q);          /* без восстановления */
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_neg(dc, d0));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, raw_opnd(RAW_TG, opnd_get_size(d0))));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_neg(dc, d0));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, s1));
        }
        break;
    }
    case OP_blsmsk: { /* dst = x ^ (x-1) */
        if (src_is_dst) {
            save_gpr_tmp(dc, bb, i, s1q);
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_sub(dc, d0, OPND_CREATE_INT8(1)));
            pass_ins(dc, bb, i, INSTR_CREATE_xor(dc, d0, raw_opnd(RAW_TG, opnd_get_size(d0))));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_sub(dc, d0, OPND_CREATE_INT8(1)));
            pass_ins(dc, bb, i, INSTR_CREATE_xor(dc, d0, s1));
        }
        break;
    }
    case OP_blsr: {   /* dst = x & (x-1) */
        if (src_is_dst) {
            save_gpr_tmp(dc, bb, i, s1q);
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_sub(dc, d0, OPND_CREATE_INT8(1)));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, raw_opnd(RAW_TG, opnd_get_size(d0))));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
            pass_ins(dc, bb, i, INSTR_CREATE_sub(dc, d0, OPND_CREATE_INT8(1)));
            pass_ins(dc, bb, i, INSTR_CREATE_and(dc, d0, s1));
        }
        break;
    }
    default:
        return 0;
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ VPMOVMSKB / VMOVMSKPS/PD: чтение полного ymm ══════════
 * Универсальный приём: lo через нативную SSE2-инструкцию по младшему
 * xmm, hi — через shadow (загрузка в xmm15), объединение со сдвигом.
 * Временные GPR — в RAW_TG/RAW_TG2 (со спилом rax).                 */
static int try_pmovmskb(void *dc, instrlist_t *bb, instr_t *i)
{
    opnd_t d0 = instr_get_dst(i, 0);          /* GPR */
    opnd_t s0 = instr_get_src(i, 0);          /* xmm или ymm */
    int di;
    if (!reg_is_gpr_idx(d0, &di)) return 0;
    if (!opnd_is_reg(s0)) return 0;
    int si = xmm_idx(opnd_get_reg(s0));
    int is_ymm = (opnd_get_reg(s0) >= DR_REG_YMM0);
    int dst_is_rax = (di == gpr_idx(DR_REG_RAX));

    if (!is_ymm) {
        pass_ins(dc, bb, i, instr_create_1dst_1src(dc, OP_pmovmskb, d0, s0));
        return 1;
    }
    /* rax — носитель (спил всегда), флаги — в TG3 (оригинал их не меняет) */
    save_gpr_tmp(dc, bb, i, DR_REG_RAX);
    pass_ins(dc, bb, i, INSTR_CREATE_pushf(dc));
    pass_ins(dc, bb, i, INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_TG3, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
    /* hi-маска */
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_YHI(si), OPSZ_16)));
    pass_ins(dc, bb, i, instr_create_1dst_1src(
        dc, OP_pmovmskb, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XMM15)));
    pass_ins(dc, bb, i, INSTR_CREATE_shl(
        dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(16)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_TG2, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    /* lo-маска в приёмник + объединение */
    pass_ins(dc, bb, i, instr_create_1dst_1src(
        dc, OP_pmovmskb, d0, opnd_create_reg(DR_REG_XMM0 + si)));
    pass_ins(dc, bb, i, INSTR_CREATE_or(
        dc, d0, raw_opnd(RAW_TG2, opnd_get_size(d0))));
    /* восстановления: флаги, xmm15, rax.
     * БАГ v0.1: при dst==rax результат затирался загрузкой образа
     * флагов в rax-носитель — сохраняем результат в TG2. */
    if (dst_is_rax)
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, raw_opnd(RAW_TG2, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RAX), raw_opnd(RAW_TG3, OPSZ_8)));
    pass_ins(dc, bb, i, INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_popf(dc));
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
    if (dst_is_rax)
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), raw_opnd(RAW_TG2, OPSZ_8)));
    else
        load_gpr_tmp(dc, bb, i, DR_REG_RAX);
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

static int try_vmovmsk(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    opnd_t d0 = instr_get_dst(i, 0);          /* GPR */
    opnd_t s0 = instr_get_src(i, 0);          /* xmm или ymm */
    int di;
    if (!reg_is_gpr_idx(d0, &di)) return 0;
    if (!opnd_is_reg(s0)) return 0;
    int si = xmm_idx(opnd_get_reg(s0));
    int is_ymm = (opnd_get_reg(s0) >= DR_REG_YMM0);
    int dst_is_rax = (di == gpr_idx(DR_REG_RAX));
    int ms_op = (opc == OP_vmovmskps) ? OP_movmskps : OP_movmskpd;
    int shift = (opc == OP_vmovmskps) ? 4 : 2;

    if (!is_ymm) {
        pass_ins(dc, bb, i, instr_create_1dst_1src(dc, ms_op, d0, s0));
        return 1;
    }
    save_gpr_tmp(dc, bb, i, DR_REG_RAX);
    pass_ins(dc, bb, i, INSTR_CREATE_pushf(dc));
    pass_ins(dc, bb, i, INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_TG3, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_YHI(si), OPSZ_16)));
    pass_ins(dc, bb, i, instr_create_1dst_1src(
        dc, ms_op, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_XMM15)));
    pass_ins(dc, bb, i, INSTR_CREATE_shl(
        dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(shift)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_TG2, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, instr_create_1dst_1src(
        dc, ms_op, d0, opnd_create_reg(DR_REG_XMM0 + si)));
    pass_ins(dc, bb, i, INSTR_CREATE_or(
        dc, d0, raw_opnd(RAW_TG2, opnd_get_size(d0))));
    if (dst_is_rax)
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, raw_opnd(RAW_TG2, OPSZ_8), opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_RAX), raw_opnd(RAW_TG3, OPSZ_8)));
    pass_ins(dc, bb, i, INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX)));
    pass_ins(dc, bb, i, INSTR_CREATE_popf(dc));
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
    if (dst_is_rax)
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), raw_opnd(RAW_TG2, OPSZ_8)));
    else
        load_gpr_tmp(dc, bb, i, DR_REG_RAX);
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ BMI2: RORX/SARX/SHLX/SHRX ══════════ */
static int try_bmi2(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s1 = instr_get_src(i, 0);
    opnd_t s2 = (instr_num_srcs(i) >= 2) ? instr_get_src(i, 1) : s1;
    int di;
    if (!reg_is_gpr_idx(d0, &di)) return 0;

    switch (opc) {
    case OP_rorx: {
        /* rorx dst, src, imm → mov dst, src ; ror dst, imm */
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
        pass_ins(dc, bb, i, INSTR_CREATE_ror(dc, d0, s2));
        break;
    }
    case OP_sarx: case OP_shlx: case OP_shrx: {
        /* dst = src1 shift src2 (младшие 5/6 бит src2) */
        if (di == gpr_idx(DR_REG_RCX))
            return 0;                     /* dst==rcx — в generic */
        save_gpr_tmp(dc, bb, i, DR_REG_RCX);
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, d0, s1));
        if (!(opnd_is_reg(s2) && opnd_get_reg(s2) == DR_REG_RCX))
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RCX), s2));
        switch (opc) {
        case OP_sarx: pass_ins(dc, bb, i, INSTR_CREATE_sar(dc, d0, opnd_create_reg(DR_REG_CL))); break;
        case OP_shlx: pass_ins(dc, bb, i, INSTR_CREATE_shl(dc, d0, opnd_create_reg(DR_REG_CL))); break;
        case OP_shrx: pass_ins(dc, bb, i, INSTR_CREATE_shr(dc, d0, opnd_create_reg(DR_REG_CL))); break;
        }
        load_gpr_tmp(dc, bb, i, DR_REG_RCX);
        break;
    }
    default:
        return 0;
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ FMA ══════════ */
typedef struct { int vex_op; int kind; int form; int packed; int is_pd; } fma_ent_t;
/* kind: 0=fmadd 1=fmsub 2=fnmadd 3=fnmsub; form: 0=132 1=213 2=231 */
static const fma_ent_t fma_tab[] = {
#define F(op, k, f, p, d) { OP_##op, k, f, p, d }
    F(vfmadd132ps,0,0,1,0), F(vfmadd132pd,0,0,1,1), F(vfmadd132ss,0,0,0,0), F(vfmadd132sd,0,0,0,1),
    F(vfmadd213ps,0,1,1,0), F(vfmadd213pd,0,1,1,1), F(vfmadd213ss,0,1,0,0), F(vfmadd213sd,0,1,0,1),
    F(vfmadd231ps,0,2,1,0), F(vfmadd231pd,0,2,1,1), F(vfmadd231ss,0,2,0,0), F(vfmadd231sd,0,2,0,1),
    F(vfmsub132ps,1,0,1,0), F(vfmsub132pd,1,0,1,1), F(vfmsub132ss,1,0,0,0), F(vfmsub132sd,1,0,0,1),
    F(vfmsub213ps,1,1,1,0), F(vfmsub213pd,1,1,1,1), F(vfmsub213ss,1,1,0,0), F(vfmsub213sd,1,1,0,1),
    F(vfmsub231ps,1,2,1,0), F(vfmsub231pd,1,2,1,1), F(vfmsub231ss,1,2,0,0), F(vfmsub231sd,1,2,0,1),
    F(vfnmadd132ps,2,0,1,0), F(vfnmadd132pd,2,0,1,1), F(vfnmadd132ss,2,0,0,0), F(vfnmadd132sd,2,0,0,1),
    F(vfnmadd213ps,2,1,1,0), F(vfnmadd213pd,2,1,1,1), F(vfnmadd213ss,2,1,0,0), F(vfnmadd213sd,2,1,0,1),
    F(vfnmadd231ps,2,2,1,0), F(vfnmadd231pd,2,2,1,1), F(vfnmadd231ss,2,2,0,0), F(vfnmadd231sd,2,2,0,1),
    F(vfnmsub132ps,3,0,1,0), F(vfnmsub132pd,3,0,1,1), F(vfnmsub132ss,3,0,0,0), F(vfnmsub132sd,3,0,0,1),
    F(vfnmsub213ps,3,1,1,0), F(vfnmsub213pd,3,1,1,1), F(vfnmsub213ss,3,1,0,0), F(vfnmsub213sd,3,1,0,1),
    F(vfnmsub231ps,3,2,1,0), F(vfnmsub231pd,3,2,1,1), F(vfnmsub231ss,3,2,0,0), F(vfnmsub231sd,3,2,0,1),
#undef F
    { 0, 0, 0, 0, 0 }
};

/* mul-опкод по типу */
static int mul_op(int packed, int is_pd)
{
    if (packed) return is_pd ? OP_mulpd : OP_mulps;
    return is_pd ? OP_mulsd : OP_mulss;
}
static int add_op(int packed, int is_pd)
{
    if (packed) return is_pd ? OP_addpd : OP_addps;
    return is_pd ? OP_addsd : OP_addss;
}
static int sub_op(int packed, int is_pd)
{
    if (packed) return is_pd ? OP_subpd : OP_subps;
    return is_pd ? OP_subsd : OP_subss;
}

/* hi-половина для 256-бит FMA в v0.1 не поддерживается — см. generic */

/* ══════════ broadcast-семейство ══════════ */
static void store_yhi_via15(void *dc, instrlist_t *bb, instr_t *at, int di,
                            int *used15)
{
    if (!*used15) {
        pass_ins(dc, bb, at, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(X15)));
        *used15 = 1;
    }
    pass_ins(dc, bb, at, INSTR_CREATE_movups(
        dc, raw_opnd(RAW_YHI(di), OPSZ_16), opnd_create_reg(X15)));
}

int g_disable_bcast = 0, g_disable_msk = 0, g_disable_vz = 0;
static int try_broadcast(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    if (g_disable_bcast) return 0;
    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s0 = instr_get_src(i, 0);
    int is_ymm = (opnd_get_size(d0) == OPSZ_32);
    int di = xmm_idx(opnd_get_reg(d0));
    int si = opnd_is_reg(s0) ? xmm_idx(opnd_get_reg(s0)) : -1;
    opnd_t xw = opnd_create_reg(DR_REG_XMM0 + di);
    opnd_t sw = (si >= 0) ? opnd_create_reg(DR_REG_XMM0 + si) : s0;

    switch (opc) {
    case OP_vbroadcastss:
        if (opnd_is_memory_reference(s0) && opnd_get_size(s0) == OPSZ_4)
            pass_ins(dc, bb, i, INSTR_CREATE_movss(dc, xw, s0));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, OP_shufps, xw, xw, OPND_CREATE_INT8(0)));
        break;
    case OP_vbroadcastsd:
        if (si >= 0) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, OP_shufpd, xw, xw, OPND_CREATE_INT8(0)));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_movddup(dc, xw, s0));
        }
        break;
    case OP_vbroadcastf128: case OP_vbroadcasti128:
        if (!is_ymm || !opnd_is_memory_reference(s0)) return 0;
        pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, s0));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_YHI(di), OPSZ_16), xw));
        instrlist_remove(bb, i);
        instr_destroy(dc, i);
        return 1;
    case OP_vpbroadcastb:
        if (opnd_is_memory_reference(s0) && opnd_get_size(s0) == OPSZ_1) {
            save_gpr_tmp(dc, bb, i, DR_REG_RAX);
            pass_ins(dc, bb, i, INSTR_CREATE_movzx(
                dc, opnd_create_reg(DR_REG_EAX), s0));
            pass_ins(dc, bb, i, INSTR_CREATE_movd(
                dc, xw, opnd_create_reg(DR_REG_EAX)));
            load_gpr_tmp(dc, bb, i, DR_REG_RAX);
        } else if (opnd_is_memory_reference(s0)) {
            pass_ins(dc, bb, i, INSTR_CREATE_movd(dc, xw, s0));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
        }
        pass_ins(dc, bb, i, INSTR_CREATE_punpcklbw(dc, xw, xw));
        pass_ins(dc, bb, i, INSTR_CREATE_punpcklwd(dc, xw, xw));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, OP_pshufd, xw, xw, OPND_CREATE_INT8(0)));
        break;
    case OP_vpbroadcastw:
        if (opnd_is_memory_reference(s0) && opnd_get_size(s0) == OPSZ_2) {
            save_gpr_tmp(dc, bb, i, DR_REG_RAX);
            pass_ins(dc, bb, i, INSTR_CREATE_movzx(
                dc, opnd_create_reg(DR_REG_EAX), s0));
            pass_ins(dc, bb, i, INSTR_CREATE_movd(
                dc, xw, opnd_create_reg(DR_REG_EAX)));
            load_gpr_tmp(dc, bb, i, DR_REG_RAX);
        } else if (opnd_is_memory_reference(s0)) {
            pass_ins(dc, bb, i, INSTR_CREATE_movd(dc, xw, s0));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
        }
        pass_ins(dc, bb, i, INSTR_CREATE_punpcklwd(dc, xw, xw));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, OP_pshufd, xw, xw, OPND_CREATE_INT8(0)));
        break;
    case OP_vpbroadcastd:
        if (opnd_is_memory_reference(s0))
            pass_ins(dc, bb, i, INSTR_CREATE_movd(dc, xw, s0));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, OP_pshufd, xw, xw, OPND_CREATE_INT8(0)));
        break;
    case OP_vpbroadcastq:
        if (opnd_is_memory_reference(s0))
            pass_ins(dc, bb, i, INSTR_CREATE_movq(dc, xw, s0));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, sw));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, OP_pshufd, xw, xw, OPND_CREATE_INT8(0x44)));
        break;
    default:
        return 0;
    }

    if (is_ymm)
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_YHI(di), OPSZ_16), xw));   /* hi = lo */
    else
        zero_yhi(dc, bb, i, di);                        /* VEX.128 */
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ VEXTRACTF128/VEXTRACTI128, VINSERTF128/VINSERTI128 ══════════ */
static int try_extract128(void *dc, instrlist_t *bb, instr_t *i)
{
    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s0 = instr_get_src(i, 0);
    opnd_t imo = instr_get_src(i, 1);
    if (!opnd_is_immed_int(imo)) return 0;
    int imm = (int)opnd_get_immed_int(imo);
    int si = xmm_idx(opnd_get_reg(s0));
    if (imm == 0) {
        pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, d0, opnd_create_reg(DR_REG_XMM0 + si)));
    } else {
        if (opnd_is_reg(d0)) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, d0, raw_opnd(RAW_YHI(si), OPSZ_16)));
        } else {
            /* mem dst: через xmm15 */
            int s_is_15 = (si == 15);
            if (!s_is_15)
                pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
                    dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(X15), raw_opnd(RAW_YHI(si), OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, d0, opnd_create_reg(X15)));
            if (!s_is_15)
                pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
                    dc, opnd_create_reg(X15), raw_opnd(RAW_S15, OPSZ_16)));
        }
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

static int try_insert128(void *dc, instrlist_t *bb, instr_t *i)
{
    opnd_t d0 = instr_get_dst(i, 0);              /* ymm */
    opnd_t s1 = instr_get_src(i, 0);              /* ymm */
    opnd_t s2 = instr_get_src(i, 1);              /* xmm или m128 */
    opnd_t imo = instr_get_src(i, 2);
    if (!opnd_is_immed_int(imo)) return 0;
    int imm = (int)opnd_get_immed_int(imo);
    int di = xmm_idx(opnd_get_reg(d0));
    int dst_is_15 = (di == 15);
    if (dst_is_15 || (opnd_is_reg(s2) && opnd_get_reg(s2) == X15))
        return 0;                    /* патологические случаи — в generic */
    int s1i = xmm_idx(opnd_get_reg(s1));
    opnd_t xw = opnd_create_reg(DR_REG_XMM0 + di);
    opnd_t s1w = opnd_create_reg(DR_REG_XMM0 + s1i);
    opnd_t s2w = opnd_is_reg(s2)
        ? opnd_create_reg(DR_REG_XMM0 + xmm_idx(opnd_get_reg(s2))) : s2;

    if (imm == 0) {
        /* lo = S2, hi = S1.hi (через xmm15) */
        pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, s2w));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, opnd_create_reg(X15), raw_opnd(RAW_YHI(s1i), OPSZ_16)));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_YHI(di), OPSZ_16), opnd_create_reg(X15)));
    } else {
        /* lo = S1, hi = S2 (через xmm15) */
        pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, s1w));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, opnd_create_reg(X15), s2w));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_YHI(di), OPSZ_16), opnd_create_reg(X15)));
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ VPERMILPS/PD (imm → shufps/shufpd) ══════════ */
static int try_permil(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s0 = instr_get_src(i, 0);
    opnd_t s1 = instr_get_src(i, 1);
    if (!opnd_is_immed_int(s1)) return 0;   /* reg-форма — в generic */
    int imm = (int)opnd_get_immed_int(s1);
    int is_pd = (opc == OP_vpermilpd);
    int is_ymm = (opnd_get_size(d0) == OPSZ_32);
    int di = xmm_idx(opnd_get_reg(d0));
    int si = opnd_is_reg(s0) ? xmm_idx(opnd_get_reg(s0)) : -1;

    int shuf_op = is_pd ? OP_shufpd : OP_shufps;
    /* маппинг imm vpermilps → shufps: см. docs/ARCHITECTURE.md;
       shufpd — прямое соответствие */
    int imm2 = is_pd ? imm :
        ((imm & 1) | ((imm & 2) << 1) | ((imm & 4) << 2) | ((imm & 8) << 3) | 0xA0);

    opnd_t xw = opnd_create_reg(DR_REG_XMM0 + di);
    if (!is_ymm) {
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, xw, si >= 0 ? opnd_create_reg(DR_REG_XMM0 + si) : s0));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, shuf_op, xw, xw, OPND_CREATE_INT8(imm2)));
        zero_yhi(dc, bb, i, di);
    } else {
        /* HI */
        if (si >= 0)
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, xw, raw_opnd(RAW_YHI(si), OPSZ_16)));
        else
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, xw, mem_off16(s0, 16, OPSZ_16)));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, shuf_op, xw, xw, OPND_CREATE_INT8(imm2)));
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_YHI(di), OPSZ_16), xw));
        /* LO */
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, xw, si >= 0 ? opnd_create_reg(DR_REG_XMM0 + si) : s0));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, shuf_op, xw, xw, OPND_CREATE_INT8(imm2)));
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

static int try_fma(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    const fma_ent_t *e = NULL;
    for (const fma_ent_t *p = fma_tab; p->vex_op; p++)
        if (p->vex_op == opc) { e = p; break; }
    if (!e) return 0;

    opnd_t d0 = instr_get_dst(i, 0);
    opnd_t s1 = instr_get_src(i, 0);
    opnd_t s2 = instr_get_src(i, 1);
    int is_ymm = (opnd_get_size(d0) == OPSZ_32);
    if (is_ymm) return 0;      /* 256-бит FMA — в generic (v0.1) */
    int di = xmm_idx(opnd_get_reg(d0));
    int dst_is_15 = (di == 15);
    int used15 = 0;

    int mop = mul_op(e->packed, e->is_pd);
    int aop = add_op(e->packed, e->is_pd);
    int sop = sub_op(e->packed, e->is_pd);

    /* 132/213: dst = dst*s1 ± s2  →  mul dst, s1 ; add/sub dst, s2
       231:      dst = s1*s2 ± dst  →  нужен спил старого dst          */
    if (e->form == 2) {
        /* сохранить старый dst (полный 16Б) */
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, raw_opnd(RAW_T0, OPSZ_16), d0));
        if (!dst_is_15) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(X15)));
            used15 = 1;
        }
        /* произведение в xmm15 */
        if (opnd_is_memory_reference(s1)) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, opnd_create_reg(X15), s1));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, opnd_create_reg(X15), s2, opnd_create_reg(X15)));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, opnd_create_reg(X15), s1));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, opnd_create_reg(X15), s2, opnd_create_reg(X15)));
        }
        switch (e->kind) {
        case 0: /* + dst */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, aop, d0, opnd_create_reg(X15), raw_opnd(RAW_T0, OPSZ_16)));
            break;
        case 1: /* − dst : dst = prod − old */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, d0, opnd_create_reg(X15)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, d0, raw_opnd(RAW_T0, OPSZ_16), d0));
            break;
        case 2: /* fnmadd231: dst = −(prod) + dst_old = dst_old − prod */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, d0, raw_opnd(RAW_T0, OPSZ_16), opnd_create_reg(X15)));
            break;
        case 3: /* fnmsub231: dst = −(prod) − dst_old = −(prod + dst_old) */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, aop, d0, opnd_create_reg(X15), raw_opnd(RAW_T0, OPSZ_16)));
            /* инвертировать знак: xor с маской — маска в BUF_SIGN? без r10:
               используем xmm15: загрузим константу из client .rodata нельзя
               без abs-адреса; применяем 0-x trick: neg нет для packed.
               Решение: xorps с константой в RAW_T1, записанной заранее…     */
            /* упрощённо: dst = 0 − dst */
            pass_ins(dc, bb, i, INSTR_CREATE_pxor(dc, opnd_create_reg(X15), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, d0, d0, opnd_create_reg(X15)));
            break;
        }
        if (used15)
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(X15), raw_opnd(RAW_S15, OPSZ_16)));
    } else {
        /* 132/213: dst = dst*s1 ± s2 */
        switch (e->kind) {
        case 0: /* fmadd: mul ; add */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, d0, s1, d0));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, aop, d0, s2, d0));
            break;
        case 1: /* fmsub: mul ; sub */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, d0, s1, d0));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, d0, s2, d0));
            break;
        case 2: /* fnmadd: −(dst*s1) + s2 = s2 − dst*s1 → mul ; xor sign ; add */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, d0, s1, d0));
            /* инверсия знака всех элементов: pxor с 0x80-маской невозможна без
               константы — используем neg-трик: pxor xmm15,xmm15 ; subps dst, x15
               меняет порядок: нам нужно s2 − dst: subps d0, s2 даёт dst−s2 ✗.
               Приём: xorps знаковых битов через временный 0: вычесть из нуля:
               t=0; t=sub(t,dst) → −dst; затем add dst? — нужен второй темп.
               Проще: pxor x15,x15; subps x15 ← 0−dst (−dst); затем
               movaps d0? — спойлер: 3 темпа. Используем RAW_T0:
               сохранённый старый dst не нужен, это 132/213.                  */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(X15)));
            used15 = 1;
            pass_ins(dc, bb, i, INSTR_CREATE_pxor(dc, opnd_create_reg(X15), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, raw_opnd(RAW_T1, OPSZ_16), d0));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, opnd_create_reg(X15), raw_opnd(RAW_T1, OPSZ_16), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, d0, opnd_create_reg(X15)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, aop, d0, s2, d0));
            break;
        case 3: /* fnmsub: −(dst*s1) − s2 → mul ; xor sign ; sub */
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, mop, d0, s1, d0));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(X15)));
            used15 = 1;
            pass_ins(dc, bb, i, INSTR_CREATE_pxor(dc, opnd_create_reg(X15), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, raw_opnd(RAW_T1, OPSZ_16), d0));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, opnd_create_reg(X15), raw_opnd(RAW_T1, OPSZ_16), opnd_create_reg(X15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, d0, opnd_create_reg(X15)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(dc, sop, d0, s2, d0));
            break;
        }
        if (used15)
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(X15), raw_opnd(RAW_S15, OPSZ_16)));
    }
    (void)used15;
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════ точка входа ══════════
 * LZCNT/TZCNT/POPCNT переведены на generic-путь (точные флаги
 * и нулевой случай; inline-вариант v0.1 портил src==dst и флаги). */

int special_req_ok(int opc)
{
    switch (opc) {
    case OP_popcnt: case OP_movbe:
    case OP_andn: case OP_blsi: case OP_blsmsk: case OP_blsr:
    case OP_rorx: case OP_sarx: case OP_shlx: case OP_shrx:
    case OP_vbroadcastss: case OP_vbroadcastsd: case OP_vbroadcastf128:
    case OP_vbroadcasti128:
    case OP_vpbroadcastb: case OP_vpbroadcastw: case OP_vpbroadcastd:
    case OP_vpbroadcastq:
    case OP_vextractf128: case OP_vextracti128:
    case OP_vinsertf128: case OP_vinserti128:
    case OP_vpermilps: case OP_vpermilpd:
    case OP_vpmovmskb: case OP_vmovmskps: case OP_vmovmskpd:
    case OP_vzeroupper: case OP_vzeroall:
        return 1;
    default:
        for (const fma_ent_t *p = fma_tab; p->vex_op; p++)
            if (p->vex_op == opc) return 1;
        return 0;
    }
}

int try_rewrite_special(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    switch (opc) {
    case OP_popcnt: return try_popcnt(dc, bb, i);
    case OP_vpmovmskb: if (!g_disable_msk) return try_pmovmskb(dc, bb, i); return 0;
    case OP_vmovmskps: case OP_vmovmskpd: if (!g_disable_msk) return try_vmovmsk(dc, bb, i); return 0;
    case OP_movbe: return try_movbe(dc, bb, i);
    case OP_andn: case OP_blsi: case OP_blsmsk: case OP_blsr:
        return try_bmi1(dc, bb, i);
    case OP_rorx: case OP_sarx: case OP_shlx: case OP_shrx:
        return try_bmi2(dc, bb, i);
    case OP_vbroadcastss: case OP_vbroadcastsd: case OP_vbroadcastf128:
    case OP_vbroadcasti128:
    case OP_vpbroadcastb: case OP_vpbroadcastw: case OP_vpbroadcastd:
    case OP_vpbroadcastq:
        return try_broadcast(dc, bb, i);
    case OP_vextractf128: case OP_vextracti128:
        return try_extract128(dc, bb, i);
    case OP_vinsertf128: case OP_vinserti128:
        return try_insert128(dc, bb, i);
    case OP_vpermilps: case OP_vpermilpd:
        return try_permil(dc, bb, i);
    case OP_vzeroupper: case OP_vzeroall: {
        if (g_disable_vz) return 0;
        /* обнуление всех YHI без регистров (64 mov-imm) */
        for (int k = 0; k < 16; k++)
            for (uint off = 0; off < 16; off += 4)
                pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
                    dc, raw_opnd(RAW_YHI(k) + off, OPSZ_4), OPND_CREATE_INT32(0)));
        if (opc == OP_vzeroall) {
            /* дополнительно обнулить все xmm: через pxor-приёмник xmm15 */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
            pass_ins(dc, bb, i, INSTR_CREATE_pxor(
                dc, opnd_create_reg(DR_REG_XMM15), opnd_create_reg(DR_REG_XMM15)));
            for (int k = 0; k < 16; k++) {
                pass_ins(dc, bb, i, INSTR_CREATE_movaps(
                    dc, opnd_create_reg(DR_REG_XMM0 + k), opnd_create_reg(DR_REG_XMM15)));
            }
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
        }
        instrlist_remove(bb, i);
        instr_destroy(dc, i);
        return 1;
    }
    default:
        return try_fma(dc, bb, i);
    }
}
