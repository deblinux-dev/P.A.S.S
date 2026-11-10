/* P.A.S.S. — rewrite_vex.c
 * Механический рерайтер VEX → legacy SSE.
 *
 * Формы:
 *   VF_DIR   — перемещения: dst полностью из src (movaps/movups/...).
 *   VF_ND2   — «чистые» 2-операндные: dst = f(src) (pshufd, pabsb,
 *              sqrtps, cvtdq2ps, movmskps, comiss, lddqu...).
 *   VF_SHIFT — деструктивные в legacy сдвиги: movaps dst, s1 ; op dst, cnt
 *              (psllw/d/q, psrlw/d/q, psraw/d, pslldq, psrldq).
 *   VF_NDS   — merge-форма: movaps dst, s1 ; op dst, s2 (вся арифметика,
 *              скалярные merge-операции, перестановки-комбайны).
 *
 * VEX.128: прямая перезапись. VEX.256: расщепление на половины,
 * старшая — через shadow-файл RAW_YHI (gs:адресация, без регистров),
 * scratch — xmm15 со спилом в RAW_S15.
 *
 * Инвариант shadow: hardware xmm_r = lo(ymm_r), RAW_YHI(r) = hi(ymm_r).
 * После VEX.128-записи — обнуление RAW_YHI(dst) (2 mov-imm).          */
#include "pass.h"

#define E(v, l, f, r) { OP_##v, OP_##l, f, r }
static const vex_map_ent_t vex_map[] = {
    /* ── VF_NDS: арифметика float/int, сравнения, комбинированные ── */
    E(vaddps, addps, VF_NDS, F_AVX),   E(vaddpd, addpd, VF_NDS, F_AVX),
    E(vsubps, subps, VF_NDS, F_AVX),   E(vsubpd, subpd, VF_NDS, F_AVX),
    E(vmulps, mulps, VF_NDS, F_AVX),   E(vmulpd, mulpd, VF_NDS, F_AVX),
    E(vdivps, divps, VF_NDS, F_AVX),   E(vdivpd, divpd, VF_NDS, F_AVX),
    E(vminps, minps, VF_NDS, F_AVX),   E(vminpd, minpd, VF_NDS, F_AVX),
    E(vmaxps, maxps, VF_NDS, F_AVX),   E(vmaxpd, maxpd, VF_NDS, F_AVX),
    E(vandps, andps, VF_NDS, F_AVX),   E(vandpd, andpd, VF_NDS, F_AVX),
    E(vandnps, andnps, VF_NDS, F_AVX), E(vandnpd, andnpd, VF_NDS, F_AVX),
    E(vorps, orps, VF_NDS, F_AVX),     E(vorpd, orpd, VF_NDS, F_AVX),
    E(vxorps, xorps, VF_NDS, F_AVX),   E(vxorpd, xorpd, VF_NDS, F_AVX),
    E(vaddsubps, addsubps, VF_NDS, F_AVX), E(vaddsubpd, addsubpd, VF_NDS, F_AVX),
    E(vhaddps, haddps, VF_NDS, F_AVX), E(vhaddpd, haddpd, VF_NDS, F_AVX),
    E(vhsubps, hsubps, VF_NDS, F_AVX), E(vhsubpd, hsubpd, VF_NDS, F_AVX),
    E(vunpcklps, unpcklps, VF_NDS, F_AVX), E(vunpckhps, unpckhps, VF_NDS, F_AVX),
    E(vunpcklpd, unpcklpd, VF_NDS, F_AVX), E(vunpckhpd, unpckhpd, VF_NDS, F_AVX),
    E(vshufps, shufps, VF_NDS, F_AVX), E(vshufpd, shufpd, VF_NDS, F_AVX),
    E(vcmpps, cmpps, VF_NDS, F_AVX),   E(vcmppd, cmppd, VF_NDS, F_AVX),
    E(vpacksswb, packsswb, VF_NDS, F_AVX), E(vpackssdw, packssdw, VF_NDS, F_AVX),
    E(vpackuswb, packuswb, VF_NDS, F_AVX),
    E(vpaddb, paddb, VF_NDS, F_AVX), E(vpaddw, paddw, VF_NDS, F_AVX),
    E(vpaddd, paddd, VF_NDS, F_AVX), E(vpaddq, paddq, VF_NDS, F_AVX),
    E(vpsubb, psubb, VF_NDS, F_AVX), E(vpsubw, psubw, VF_NDS, F_AVX),
    E(vpsubd, psubd, VF_NDS, F_AVX), E(vpsubq, psubq, VF_NDS, F_AVX),
    E(vpaddsb, paddsb, VF_NDS, F_AVX), E(vpaddsw, paddsw, VF_NDS, F_AVX),
    E(vpaddusb, paddusb, VF_NDS, F_AVX), E(vpaddusw, paddusw, VF_NDS, F_AVX),
    E(vpsubsb, psubsb, VF_NDS, F_AVX), E(vpsubsw, psubsw, VF_NDS, F_AVX),
    E(vpavgb, pavgb, VF_NDS, F_AVX), E(vpavgw, pavgw, VF_NDS, F_AVX),
    E(vpand, pand, VF_NDS, F_AVX), E(vpandn, pandn, VF_NDS, F_AVX),
    E(vpor, por, VF_NDS, F_AVX), E(vpxor, pxor, VF_NDS, F_AVX),
    E(vpmulhw, pmulhw, VF_NDS, F_AVX), E(vpmullw, pmullw, VF_NDS, F_AVX),
    E(vpmuludq, pmuludq, VF_NDS, F_AVX), E(vpmulhrsw, pmulhrsw, VF_NDS, F_AVX),
    E(vpmaddwd, pmaddwd, VF_NDS, F_AVX), E(vpmaddubsw, pmaddubsw, VF_NDS, F_AVX),
    E(vpsadbw, psadbw, VF_NDS, F_AVX),
    E(vpcmpgtb, pcmpgtb, VF_NDS, F_AVX), E(vpcmpgtw, pcmpgtw, VF_NDS, F_AVX),
    E(vpcmpgtd, pcmpgtd, VF_NDS, F_AVX),
    E(vpcmpeqb, pcmpeqb, VF_NDS, F_AVX), E(vpcmpeqw, pcmpeqw, VF_NDS, F_AVX),
    E(vpcmpeqd, pcmpeqd, VF_NDS, F_AVX),
    E(vpunpckhbw, punpckhbw, VF_NDS, F_AVX), E(vpunpckhwd, punpckhwd, VF_NDS, F_AVX),
    E(vpunpckhdq, punpckhdq, VF_NDS, F_AVX), E(vpunpckhqdq, punpckhqdq, VF_NDS, F_AVX),
    E(vpunpcklbw, punpcklbw, VF_NDS, F_AVX), E(vpunpcklwd, punpcklwd, VF_NDS, F_AVX),
    E(vpunpckldq, punpckldq, VF_NDS, F_AVX), E(vpunpcklqdq, punpcklqdq, VF_NDS, F_AVX),
    E(vpshufb, pshufb, VF_NDS, F_AVX),
    E(vpsignb, psignb, VF_NDS, F_AVX), E(vpsignw, psignw, VF_NDS, F_AVX),
    E(vpsignd, psignd, VF_NDS, F_AVX),
    E(vphaddw, phaddw, VF_NDS, F_AVX), E(vphaddd, phaddd, VF_NDS, F_AVX),
    E(vphaddsw, phaddsw, VF_NDS, F_AVX),
    E(vphsubw, phsubw, VF_NDS, F_AVX), E(vphsubd, phsubd, VF_NDS, F_AVX),
    E(vphsubsw, phsubsw, VF_NDS, F_AVX),
    E(vpmaxub, pmaxub, VF_NDS, F_AVX), E(vpmaxsw, pmaxsw, VF_NDS, F_AVX),
    E(vpminub, pminub, VF_NDS, F_AVX), E(vpminsw, pminsw, VF_NDS, F_AVX),
    E(vpblendw, pblendw, VF_NDS, F_AVX),
    E(vpalignr, palignr, VF_NDS, F_AVX),
    /* скалярные merge */
    E(vaddss, addss, VF_NDS, F_AVX), E(vaddsd, addsd, VF_NDS, F_AVX),
    E(vsubss, subss, VF_NDS, F_AVX), E(vsubsd, subsd, VF_NDS, F_AVX),
    E(vmulss, mulss, VF_NDS, F_AVX), E(vmulsd, mulsd, VF_NDS, F_AVX),
    E(vdivss, divss, VF_NDS, F_AVX), E(vdivsd, divsd, VF_NDS, F_AVX),
    E(vminss, minss, VF_NDS, F_AVX), E(vminsd, minsd, VF_NDS, F_AVX),
    E(vmaxss, maxss, VF_NDS, F_AVX), E(vmaxsd, maxsd, VF_NDS, F_AVX),
    E(vcmpss, cmpss, VF_NDS, F_AVX), E(vcmpsd, cmpsd, VF_NDS, F_AVX),
    E(vsqrtss, sqrtss, VF_NDS, F_AVX), E(vsqrtsd, sqrtsd, VF_NDS, F_AVX),
    E(vrsqrtss, rsqrtss, VF_NDS, F_AVX), E(vrcpss, rcpss, VF_NDS, F_AVX),
    E(vcvtsi2ss, cvtsi2ss, VF_NDS, F_AVX), E(vcvtsi2sd, cvtsi2sd, VF_NDS, F_AVX),
    E(vcvtss2sd, cvtss2sd, VF_NDS, F_AVX), E(vcvtsd2ss, cvtsd2ss, VF_NDS, F_AVX),
    /* ── VF_SHIFT: сдвиги, деструктивные в legacy ── */
    E(vpsllw, psllw, VF_SHIFT, F_AVX), E(vpslld, pslld, VF_SHIFT, F_AVX),
    E(vpsllq, psllq, VF_SHIFT, F_AVX),
    E(vpsrlw, psrlw, VF_SHIFT, F_AVX), E(vpsrld, psrld, VF_SHIFT, F_AVX),
    E(vpsrlq, psrlq, VF_SHIFT, F_AVX),
    E(vpsraw, psraw, VF_SHIFT, F_AVX), E(vpsrad, psrad, VF_SHIFT, F_AVX),
    E(vpslldq, pslldq, VF_SHIFT, F_AVX), E(vpsrldq, psrldq, VF_SHIFT, F_AVX),
    /* ── VF_ND2: чистые (dst = f(src)) ── */
    E(vcvtdq2ps, cvtdq2ps, VF_ND2, F_AVX), E(vcvtps2dq, cvtps2dq, VF_ND2, F_AVX),
    E(vcvttps2dq, cvttps2dq, VF_ND2, F_AVX),
    E(vsqrtps, sqrtps, VF_ND2, F_AVX), E(vsqrtpd, sqrtpd, VF_ND2, F_AVX),
    E(vrcpps, rcpps, VF_ND2, F_AVX),   E(vrsqrtps, rsqrtps, VF_ND2, F_AVX),
    E(vcomiss, comiss, VF_ND2, F_AVX), E(vcomisd, comisd, VF_ND2, F_AVX),
    E(vucomiss, ucomiss, VF_ND2, F_AVX), E(vucomisd, ucomisd, VF_ND2, F_AVX),
    E(vpabsb, pabsb, VF_ND2, F_AVX), E(vpabsw, pabsw, VF_ND2, F_AVX),
    E(vpabsd, pabsd, VF_ND2, F_AVX),
    E(vpshufd, pshufd, VF_ND2, F_AVX), E(vpshuflw, pshuflw, VF_ND2, F_AVX),
    E(vpshufhw, pshufhw, VF_ND2, F_AVX),
    E(vcvtss2si, cvtss2si, VF_ND2, F_AVX), E(vcvtsd2si, cvtsd2si, VF_ND2, F_AVX),
    E(vcvttss2si, cvttss2si, VF_ND2, F_AVX), E(vcvttsd2si, cvttsd2si, VF_ND2, F_AVX),
    /* ── VF_DIR: перемещения ── */
    E(vmovaps, movaps, VF_DIR, F_AVX), E(vmovapd, movapd, VF_DIR, F_AVX),
    E(vmovups, movups, VF_DIR, F_AVX), E(vmovupd, movupd, VF_DIR, F_AVX),
    /* БАГ v0.1: vmovdqa/vmovdqu/vmovss/vmovsd отсутствовали в карте —
       уходили в default (passthrough) и исполнялись нативно: 256-битные
       store писали мусор из аппаратной старшей половины вместо тени. */
    E(vmovdqa, movaps, VF_DIR, F_AVX), E(vmovdqu, movups, VF_DIR, F_AVX),
    E(vmovss, movss, VF_NDS, F_AVX),   E(vmovsd, movsd, VF_NDS, F_AVX),
    E(vmovntps, movntps, VF_DIR, F_AVX), E(vmovntpd, movntpd, VF_DIR, F_AVX),
    E(vmovntdq, movntdq, VF_DIR, F_AVX),
    E(vmovd, movd, VF_DIR, F_AVX), E(vmovq, movq, VF_DIR, F_AVX),
    E(vlddqu, lddqu, VF_DIR, F_AVX),
    E(vldmxcsr, ldmxcsr, VF_DIR, F_AVX), E(vstmxcsr, stmxcsr, VF_DIR, F_AVX),
    { 0, 0, 0, F_AVX }
};
#undef E

const vex_map_ent_t *vex_map_lookup(int opc)
{
    for (const vex_map_ent_t *e = vex_map; e->vex_op != 0; e++)
        if (e->vex_op == opc) return e;
    return NULL;
}

/* ══════════════ вспомогательные ══════════════ */

static int opnd_is_xmmreg(opnd_t o)
{
    return opnd_is_reg(o) && opnd_get_reg(o) >= DR_REG_XMM0 &&
           opnd_get_reg(o) <= DR_REG_XMM15;
}

/* Нормализация xmm-регистрового операнда до полного OPSZ_16.
 * У app-операндов скалярных VEX-форм (vcvtsi2sd/vmulss и т.п.) DR
 * ставит размер 8Б — movaps с таким операндом НЕкодируем и роняет
 * релизный кодировщик DR (SIGSEGV при выбросе BB, §6.14).          */
static opnd_t xmm16(opnd_t o)
{
    if (opnd_is_xmmreg(o))
        return opnd_create_reg(opnd_get_reg(o));
    return o;
}

/* stage mem→RAW_T через xmm15 (со спилом при первом использовании) */
static void stage_mem(void *dc, instrlist_t *bb, instr_t *at, opnd_t mem,
                      uint tls_slot, int *used15)
{
    /* читаем ровно 16 байт, независимо от размера исходного операнда */
    mem = mem_off16(mem, 0, OPSZ_16);
    if (!*used15) {
        pass_ins(dc, bb, at, INSTR_CREATE_movups(
            dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        *used15 = 1;
    }
    pass_ins(dc, bb, at, INSTR_CREATE_movups(
        dc, opnd_create_reg(DR_REG_XMM15), mem));
    pass_ins(dc, bb, at, INSTR_CREATE_movups(
        dc, raw_opnd(tls_slot, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
}

static void restore15(void *dc, instrlist_t *bb, instr_t *at, int used15, int dst_is_15)
{
    if (used15 && !dst_is_15)
        pass_ins(dc, bb, at, INSTR_CREATE_movups(
            dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
}

/* ══════════════ 128-битный путь ══════════════ */

static int rewrite_128(void *dc, instrlist_t *bb, instr_t *i,
                       const vex_map_ent_t *e)
{
    /* защитный guard: 0-dst формы сюда доходить не должны (см. rewrite_0dst);
     * при неожиданном попадании — trap-fallback вместо краша на dsts[0] */
    if (instr_num_dsts(i) == 0)
        return 0;
    opnd_t d0 = instr_get_dst(i, 0);
    int nsrc = instr_num_srcs(i);
    opnd_t s1 = instr_get_src(i, 0);
    opnd_t s2 = (nsrc >= 2) ? instr_get_src(i, 1) : s1;
    opnd_t imm = (nsrc >= 3) ? instr_get_src(i, 2) : OPND_CREATE_INT32(0);
    int has_imm = (nsrc >= 3) && opnd_is_immed_int(instr_get_src(i, 2));
    int dst_is_15 = opnd_is_reg(d0) && opnd_get_reg(d0) == DR_REG_XMM15;
    int used15 = 0;
    opnd_t s1eff = s1, s2eff = s2;

    /* mem-источники арифметики требуют выравнивания в legacy — stage */
    if (e->form != VF_DIR) {
        if (opnd_is_memory_reference(s1) && e->form == VF_ND2) {
            stage_mem(dc, bb, i, s1, RAW_T0, &used15);
            s1eff = raw_opnd(RAW_T0, opnd_get_size(s1));
        }
        if (opnd_is_memory_reference(s2) && (e->form == VF_NDS || e->form == VF_SHIFT)) {
            stage_mem(dc, bb, i, s2, RAW_T0, &used15);
            s2eff = raw_opnd(RAW_T0, opnd_get_size(s2));
        }
    }

    switch (e->form) {
    case VF_DIR:
        if ((e->vex_op == OP_vmovd || e->vex_op == OP_vmovq) &&
            opnd_is_reg(d0) && is_gpr_reg(opnd_get_reg(d0)) && opnd_is_reg(s1)) {
            /* XMM → GPR: через xmm15 и staging-слот — тот же маршрут,
             * что у stage_mem (прямая легаси-форма movq/movd xmm→r64
             * segv-ится в кодировщике DR 11.9x, как и movups xmm→mem
             * не через xmm15).                                    */
            int si2 = xmm_idx(opnd_get_reg(s1));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                opnd_create_reg(DR_REG_XMM0 + si2)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_T0, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
                dc, opnd_create_reg(gpr_norm(opnd_get_reg(d0))),
                raw_opnd(RAW_T0, OPSZ_8)));
        } else if ((e->vex_op == OP_vmovd || e->vex_op == OP_vmovq) &&
                   opnd_is_reg(d0) && opnd_is_reg(s1) &&
                   opnd_get_reg(d0) >= DR_REG_XMM0 && opnd_get_reg(d0) <= DR_REG_XMM15 &&
                   is_gpr_reg(opnd_get_reg(s1))) {
            /* GPR → XMM: через T0 (прямой movq r64→xmm тоже segv-ится
             * в кодировщике). Верхние 8 байт результата — нули.
             * §7.3: xmm15 используется как носитель — ОБЯЗАТЕЛЕН спил
             * в RAW_S15 и восстановление (если dst != xmm15): раньше
             * ветка молча гробила живые данные приложения в xmm15
             * (в векторизованном MixColumns gcc держит там константы
             * умножения — KAT расходился с железом).                */
            reg_id_t g = gpr_norm(opnd_get_reg(s1));
            int di2 = xmm_idx(opnd_get_reg(d0));
            int spill15 = (di2 != 15);
            if (spill15)
                pass_ins(dc, bb, i, INSTR_CREATE_movups(
                    dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
            pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
                dc, raw_opnd(RAW_T0, OPSZ_8), opnd_create_reg(g)));
            pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
                dc, raw_opnd(RAW_T0 + 8, OPSZ_4), OPND_CREATE_INT32(0)));
            pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
                dc, raw_opnd(RAW_T0 + 12, OPSZ_4), OPND_CREATE_INT32(0)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_T0, OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_movaps(
                dc, opnd_create_reg(DR_REG_XMM0 + di2), opnd_create_reg(DR_REG_XMM15)));
            if (spill15)
                pass_ins(dc, bb, i, INSTR_CREATE_movups(
                    dc, opnd_create_reg(DR_REG_XMM15), raw_opnd(RAW_S15, OPSZ_16)));
        } else if (e->vex_op == OP_vmovd || e->vex_op == OP_vmovq) {
            /* GPR↔mem направления: vmovd двигает ровно 4 байта (верх
             * xmm обнуляется), vmovq — 8. Прежняя «нормализация» в
             * OPSZ_8 превращала vmovd m32 в movq: читались 4 лишних
             * байта стека в xmm[4..7], мусор расползался через
             * unpck/blend (срыв KAT MixColumns, §7.3).              */
            opnd_size_t msz = (e->vex_op == OP_vmovq) ? OPSZ_8 : OPSZ_4;
            opnd_t dd = d0, ss = s1;
            if (opnd_is_memory_reference(dd)) opnd_set_size(&dd, msz);
            if (opnd_is_memory_reference(ss)) opnd_set_size(&ss, msz);
            pass_ins(dc, bb, i, instr_create_1dst_1src(dc, e->legacy_op, dd, ss));
        } else {
            pass_ins(dc, bb, i, instr_create_1dst_1src(dc, e->legacy_op, d0, s1));
        }
        break;
    case VF_ND2:
        if (opnd_is_immed_int(s2))
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, d0, s1eff, s2));
        else
            pass_ins(dc, bb, i, instr_create_1dst_1src(
                dc, e->legacy_op, d0, s1eff));
        break;
    case VF_SHIFT:
        /* movaps dst, s1 ; op dst, count */
        pass_ins(dc, bb, i, INSTR_CREATE_movaps(dc, xmm16(d0), xmm16(s1)));
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, e->legacy_op, d0, s2eff, d0));
        break;
    case VF_NDS:
    default:
        /* vmovss/vmovsd 2-формы: movss dst, m32; 3-формы: movaps dst, s1 ; movss dst, s2 */
        if ((e->vex_op == OP_vmovss || e->vex_op == OP_vmovsd)) {
            if (nsrc >= 2) {
                pass_ins(dc, bb, i, INSTR_CREATE_movaps(dc, xmm16(d0), xmm16(s1)));
                pass_ins(dc, bb, i, instr_create_1dst_1src(dc, e->legacy_op, d0, s2eff));
            } else {
                pass_ins(dc, bb, i, instr_create_1dst_1src(dc, e->legacy_op, d0, s1));
            }
        } else if (e->legacy_op == OP_cvtsi2ss || e->legacy_op == OP_cvtsi2sd) {
            /* VEX cvtsi2s* xmm1, xmm2, r/m: слияние через movaps, затем
             * легаси cvtsi2s* — в DR она с ОДНИМ источником (сливной
             * операнд — сам dst; 2src-конструкция некодируема → крах
             * релизного кодировщика DR, §6.14).                       */
            pass_ins(dc, bb, i, INSTR_CREATE_movaps(dc, xmm16(d0), xmm16(s1)));
            pass_ins(dc, bb, i, instr_create_1dst_1src(
                dc, e->legacy_op, xmm16(d0), s2eff));
        } else if (has_imm) {
            pass_ins(dc, bb, i, INSTR_CREATE_movaps(dc, xmm16(d0), xmm16(s1)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, d0, s2eff, imm));
        } else {
            pass_ins(dc, bb, i, INSTR_CREATE_movaps(dc, xmm16(d0), xmm16(s1)));
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, d0, s2eff, d0));
        }
        break;
    }

    /* VEX.128-запись в xmm обнуляет старшие половины */
    if (opnd_is_xmmreg(d0) && opnd_get_size(d0) == OPSZ_16 &&
        e->vex_op != OP_vcomiss && e->vex_op != OP_vcomisd &&
        e->vex_op != OP_vucomiss && e->vex_op != OP_vucomisd &&
        e->vex_op != OP_vmovmskps && e->vex_op != OP_vmovmskpd &&
        e->vex_op != OP_vldmxcsr && e->vex_op != OP_vstmxcsr)
        zero_yhi(dc, bb, i, xmm_idx(opnd_get_reg(d0)));

    restore15(dc, bb, i, used15, dst_is_15);

    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ══════════════ 256-битный путь (split) ══════════════ */

/* Удалить исходную инструкцию после успешной эмуляции.
 * БАГ v0.1: rewrite_256 оставлял оригинал в BB — на хосте с AVX он
 * исполнялся нативно и затирал результат (а 256-битные store писали
 * в память мусор из аппаратной старшей половины — порча на старте
 * статической glibc, SIGSEGV). Сгенерированный код обязан быть
 * 100 % legacy SSE: любые movups — только по xmm-операндам.      */
static void remove_orig(void *dc, instrlist_t *bb, instr_t *i)
{
    (void)dc;
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
}

static int rewrite_256(void *dc, instrlist_t *bb, instr_t *i,
                       const vex_map_ent_t *e)
{
    /* защитный guard: см. rewrite_128 */
    if (instr_num_dsts(i) == 0)
        return 0;
    opnd_t d0 = instr_get_dst(i, 0);
    int nsrc = instr_num_srcs(i);
    opnd_t s1 = instr_get_src(i, 0);
    opnd_t s2 = (nsrc >= 2) ? instr_get_src(i, 1) : s1;
    opnd_t imm = (nsrc >= 3) ? instr_get_src(i, 2) : OPND_CREATE_INT32(0);
    int has_imm = (nsrc >= 3) && opnd_is_immed_int(instr_get_src(i, 2));
    int dst_is_reg = opnd_is_reg(d0);
    int di = dst_is_reg ? xmm_idx(opnd_get_reg(d0)) : -1;
    int dst_is_15 = dst_is_reg && (di == 15);
    int used15 = 0;
    int s1_is_reg = opnd_is_reg(s1);
    int s1i = s1_is_reg ? xmm_idx(opnd_get_reg(s1)) : -1;
    int s2_is_reg = opnd_is_reg(s2);
    int s2i = s2_is_reg ? xmm_idx(opnd_get_reg(s2)) : -1;

    /* ── VF_DIR: перемещения 256 бит ── */
    if (e->form == VF_DIR) {
        if (dst_is_reg && s1_is_reg) {
            if (di == s1i) {
                remove_orig(dc, bb, i);
                return 1;                     /* no-op */
            }
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                raw_opnd(RAW_YHI(s1i), OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_YHI(di), OPSZ_16),
                opnd_create_reg(DR_REG_XMM15)));
            /* только xmm-операнды: movups с ymm кодируется как VEX.256! */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM0 + di),
                opnd_create_reg(DR_REG_XMM0 + s1i)));
            remove_orig(dc, bb, i);
            return 1;
        }
        if (dst_is_reg && !s1_is_reg) {
            /* load: hi из [mem+16], lo из [mem] */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                mem_off16(s1, 16, OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_YHI(di), OPSZ_16),
                opnd_create_reg(DR_REG_XMM15)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM0 + di),
                mem_off16(s1, 0, OPSZ_16)));
            remove_orig(dc, bb, i);
            return 1;
        }
        /* store: [mem] ← ymm (lo, затем hi через xmm15) */
        if (!s1_is_reg) return 0;
        int s1_is_15 = (s1i == 15);
        if (!s1_is_15) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        }
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, mem_off16(d0, 0, OPSZ_16),
            opnd_create_reg(DR_REG_XMM0 + s1i)));
        if (!s1_is_15) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                raw_opnd(RAW_YHI(s1i), OPSZ_16)));
        }
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, mem_off16(d0, 16, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        if (!s1_is_15) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                raw_opnd(RAW_S15, OPSZ_16)));
        }
        remove_orig(dc, bb, i);
        return 1;
    }

    /* ── арифметика: xw = младший xmm приёмника ── */
    opnd_t xw = opnd_create_reg(DR_REG_XMM0 + di);

    /* ── алиасинг dst == src (БАГ v0.1): xw = младший xmm приёмника
     * перезаписывается до чтения перекрытых половин. Например
     * vpcmpeqb ymm2,ymm1,ymm2: staging HI затирает xmm2 (= lo(s2)) —
     * сравнение младших половин шло «сам с собой». Стадируем
     * перекрытые половины в RAW_T0/T1 до любых записей в xw.       */
    int s1_alias = s1_is_reg && (s1i == di);
    int s2_alias = s2_is_reg && (s2i == di);
    opnd_t s1lo = s1_is_reg ? opnd_create_reg(DR_REG_XMM0 + s1i)
                            : mem_off16(s1, 0, OPSZ_16);
    if (e->form != VF_DIR && (s1_alias || s2_alias)) {
        if (!used15) {
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_S15, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
            used15 = 1;
        }
        if (s1_alias) {                     /* lo(s1) → T0 (общий для s1/s2) */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                opnd_create_reg(DR_REG_XMM0 + s1i)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_T0, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        }
        if (s2_alias && !s1_alias) {        /* lo(s2) → T0 */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                opnd_create_reg(DR_REG_XMM0 + s2i)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_T0, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        }
        if (s2_alias) {                     /* hi(s2) → T1 (до записи YHI(dst)) */
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, opnd_create_reg(DR_REG_XMM15),
                raw_opnd(RAW_YHI(s2i), OPSZ_16)));
            pass_ins(dc, bb, i, INSTR_CREATE_movups(
                dc, raw_opnd(RAW_T1, OPSZ_16), opnd_create_reg(DR_REG_XMM15)));
        }
        if (s1_alias)
            s1lo = raw_opnd(RAW_T0, OPSZ_16);
    }

    /* операнд S2: старшая и младшая половины */
    opnd_t s2hi, s2lo;
    if (e->form == VF_NDS) {
        if (s2_is_reg) {
            if (s2_alias) {
                s2hi = raw_opnd(RAW_T1, OPSZ_16);
                s2lo = raw_opnd(RAW_T0, OPSZ_16);
            } else {
                s2hi = raw_opnd(RAW_YHI(s2i), OPSZ_16);
                s2lo = opnd_create_reg(DR_REG_XMM0 + s2i);
            }
        } else if (opnd_is_memory_reference(s2)) {
            /* при s1_alias слот T0 занят lo(s1) — mem-стадия в T2/T1 */
            if (s1_alias) {
                stage_mem(dc, bb, i, mem_off16(s2, 16, OPSZ_16), RAW_T2, &used15);
                s2hi = raw_opnd(RAW_T2, OPSZ_16);
                stage_mem(dc, bb, i, s2, RAW_T1, &used15);
                s2lo = raw_opnd(RAW_T1, OPSZ_16);
            } else {
                stage_mem(dc, bb, i, mem_off16(s2, 16, OPSZ_16), RAW_T1, &used15);
                s2hi = raw_opnd(RAW_T1, OPSZ_16);
                stage_mem(dc, bb, i, s2, RAW_T0, &used15);
                s2lo = raw_opnd(RAW_T0, OPSZ_16);
            }
        } else {
            s2hi = s2; s2lo = s2;   /* imm (не используется для NDS) */
        }
    } else if (e->form == VF_SHIFT) {
        s2hi = s2; s2lo = s2;       /* счётчик: xmm-рег или imm */
        if (s2_alias)
            s2hi = s2lo = raw_opnd(RAW_T0, OPSZ_16);
        /* s1_alias для SHIFT покрывается s1lo→T0 в LO-секции */
    } else {                        /* VF_ND2: данные уже в xw */
        s2hi = xw; s2lo = xw;
    }

    /* ── HI: xw ← hi(s1); op; [YHI(dst)] ← xw ── */
    if (s1_is_reg)
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, xw, raw_opnd(RAW_YHI(s1i), OPSZ_16)));
    else
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, xw, mem_off16(s1, 16, OPSZ_16)));
    if (e->form == VF_NDS) {
        if (has_imm)
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, s2hi, imm));
        else
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, s2hi, xw));
    } else if (e->form == VF_SHIFT) {
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, e->legacy_op, xw, s2hi, xw));
    } else { /* VF_ND2 */
        if (opnd_is_immed_int(s2))
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, xw, s2));
        else
            pass_ins(dc, bb, i, instr_create_1dst_1src(
                dc, e->legacy_op, xw, xw));
    }
    pass_ins(dc, bb, i, INSTR_CREATE_movups(
        dc, raw_opnd(RAW_YHI(di), OPSZ_16), xw));

    /* ── LO: xw ← lo(s1); op (результат остаётся в xmm-dst) ── */
    pass_ins(dc, bb, i, INSTR_CREATE_movups(dc, xw, s1lo));
    if (e->form == VF_NDS) {
        if (has_imm)
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, s2lo, imm));
        else
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, s2lo, xw));
    } else if (e->form == VF_SHIFT) {
        pass_ins(dc, bb, i, instr_create_1dst_2src(
            dc, e->legacy_op, xw, s2lo, xw));
    } else {
        if (opnd_is_immed_int(s2))
            pass_ins(dc, bb, i, instr_create_1dst_2src(
                dc, e->legacy_op, xw, xw, s2));
        else
            pass_ins(dc, bb, i, instr_create_1dst_1src(
                dc, e->legacy_op, xw, xw));
    }

    restore15(dc, bb, i, used15, dst_is_15);
    remove_orig(dc, bb, i);
    return 1;
}

/* ══════════════ точка входа ══════════════ */

/* §7.3: comis-семья (comiss/comisd/ucomiss/ucomisd) и ldmxcsr — legacy-формы
 * в DR моделируются БЕЗ dst-операндов (xx, xx, V, W): пишут только флаги/MXCSR.
 * Инструкция с 0 dsts имеет dsts==NULL, и любой instr_get_dst(i, 0) в релизной
 * сборке DR (CLIENT_ASSERT вырезан) — детерминированный SIGSEGV на dsts[0]
 * (Windows CI: 0xC0000005 @ dynamorio.dll+0x7250c на bun/selftest; Linux:
 * OPND-TRAP opc=662 OP_vucomisd ndst=0 nsrc=2). VEX-форма семантически
 * идентична legacy (недеструктивное чтение), поэтому — прямая замена на
 * legacy-инструкцию с теми же операндами.                                   */
static int rewrite_0dst(void *dc, instrlist_t *bb, instr_t *i,
                        const vex_map_ent_t *e)
{
    if (e->vex_op == OP_vldmxcsr) {
        pass_ins(dc, bb, i, INSTR_CREATE_ldmxcsr(dc, instr_get_src(i, 0)));
    } else {
        /* comis: srcs = (V, W) — порядок совпадает с legacy */
        pass_ins(dc, bb, i, instr_create_0dst_2src(dc, e->legacy_op,
                                                   instr_get_src(i, 0),
                                                   instr_get_src(i, 1)));
    }
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

extern int g_disable_256, g_disable_128;
int try_rewrite_vex(void *dc, instrlist_t *bb, instr_t *i)
{
    int opc = instr_get_opcode(i);
    const vex_map_ent_t *e = vex_map_lookup(opc);
    if (e == NULL || e->form == VF_SPECIAL)
        return 0;

    /* 0-dst формы (comis-семья, ldmxcsr) — отдельный путь, см. выше */
    if (instr_num_dsts(i) == 0)
        return rewrite_0dst(dc, bb, i, e);

    /* widen-формы с разными размерами src/dst (vcvtps2pd ymm, xmm) — в generic */
    if (instr_num_dsts(i) > 0 &&
        opnd_is_reg(instr_get_dst(i, 0)) &&
        opnd_get_size(instr_get_dst(i, 0)) == OPSZ_32) {
        for (int k = 0; k < instr_num_srcs(i); k++) {
            opnd_t s = instr_get_src(i, k);
            if (opnd_is_reg(s) && opnd_get_size(s) != OPSZ_32)
                return 0;
        }
        return rewrite_256(dc, bb, i, e);
    }
    if (instr_num_dsts(i) > 0 &&
        opnd_is_memory_reference(instr_get_dst(i, 0)) &&
        opnd_get_size(instr_get_dst(i, 0)) == OPSZ_32) {
        return rewrite_256(dc, bb, i, e);       /* 256-битный store */
    }
    return rewrite_128(dc, bb, i, e);
}
