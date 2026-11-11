/* P.A.S.S. — rewrite.c
 * Проход по базовым блокам, диспетчер перезаписи и генератор
 * generic-пути (спил регистров → clean call → восстановление). */
#include "pass.h"

/* ─────────────── общие хелперы генерации ─────────────── */

void pass_ins(void *dc, instrlist_t *bb, instr_t *at, instr_t *in)
{
    (void)dc;
    /* Инструкции, обращающиеся к клиентскому RAW TLS (gs:disp),
     * помечаем мета: иначе DR манглит gs как сегмент приложения
     * (база приложения = 0) и запись уходит по нулевому адресу.
     * Инструкции с app-памятью остаются app — их сбои транслируются
     * корректно (fallback-семантика).                              */
    int uses_tls = 0, uses_app_mem = 0;
    for (int k = 0; k < instr_num_srcs(in) && !uses_app_mem; k++) {
        opnd_t o = instr_get_src(in, k);
        if (opnd_is_far_base_disp(o) && opnd_get_segment(o) == g_tls_seg)
            uses_tls = 1;
        else if (opnd_is_base_disp(o))
            uses_app_mem = 1;
    }
    for (int k = 0; k < instr_num_dsts(in) && !uses_app_mem; k++) {
        opnd_t o = instr_get_dst(in, k);
        if (opnd_is_far_base_disp(o) && opnd_get_segment(o) == g_tls_seg)
            uses_tls = 1;
        else if (opnd_is_base_disp(o))
            uses_app_mem = 1;
    }
    if (uses_tls && !uses_app_mem)
        instr_set_meta(in);
    instrlist_preinsert(bb, at, in);
}

/* копия mem-операнда со сдвигом смещения (для hi-половин) */
opnd_t mem_off16(opnd_t mem, int disp_add, opnd_size_t sz)
{
    if (opnd_is_base_disp(mem)) {
        return opnd_create_base_disp(opnd_get_base(mem), opnd_get_index(mem),
                                     opnd_get_scale(mem),
                                     opnd_get_disp(mem) + disp_add, sz);
    }
    if (opnd_is_far_base_disp(mem)) {
        return opnd_create_far_base_disp(opnd_get_segment(mem), opnd_get_base(mem),
                                         opnd_get_index(mem), opnd_get_scale(mem),
                                         opnd_get_disp(mem) + disp_add, sz);
    }
    if (opnd_is_rel_addr(mem)) {
        return opnd_create_rel_addr((byte *)((char *)opnd_get_addr(mem) + disp_add), sz);
    }
    if (opnd_is_abs_addr(mem)) {
        return opnd_create_abs_addr((byte *)((char *)opnd_get_addr(mem) + disp_add), sz);
    }
    return mem;
}

/* [r10 + off] */
opnd_t buf_opnd(uint off, opnd_size_t sz)
{
    return opnd_create_base_disp(DR_REG_R10, DR_REG_NULL, 0, (int)off, sz);
}

/* [gs: g_tls_raw_off + off] */
opnd_t raw_opnd(uint off, opnd_size_t sz)
{
    return opnd_create_far_base_disp(g_tls_seg, DR_REG_NULL, DR_REG_NULL, 0,
                                     (int)(g_tls_raw_off + off), sz);
}

/* начало «носителя»: сохранить r10, загрузить базу BUF в r10 */
void carrier_begin(void *dc, instrlist_t *bb, instr_t *at)
{
    pass_ins(dc, bb, at, INSTR_CREATE_mov_st(
        dc, raw_opnd(RAW_SV10, OPSZ_8), opnd_create_reg(DR_REG_R10)));
    drmgr_insert_read_tls_field(dc, g_tls_idx_buf, bb, at, DR_REG_R10);
}

/* конец: восстановить r10 (если он не выходной регистр операции) */
void carrier_end(void *dc, instrlist_t *bb, instr_t *at, uint32_t gpr_dst)
{
    if (gpr_dst & (1u << gpr_idx(DR_REG_R10)))
        return;
    pass_ins(dc, bb, at, INSTR_CREATE_mov_ld(
        dc, opnd_create_reg(DR_REG_R10), raw_opnd(RAW_SV10, OPSZ_8)));
}

/* RAW_YHI[x] = 0 — два mov-imm, без регистров */
void zero_yhi(void *dc, instrlist_t *bb, instr_t *at, int xi)
{
    /* 4×mov dword [gs:off], 0 — гарантированно кодируемая форма */
    for (uint k = 0; k < 16; k += 4)
        pass_ins(dc, bb, at, INSTR_CREATE_mov_st(
            dc, raw_opnd(RAW_YHI(xi) + k, OPSZ_4), OPND_CREATE_INT32(0)));
}

/* ─────────────── таблица generic-опкодов ─────────────── */

static const int generic_opcs[] = {
    /* SSE4.1/4.2 векторные */
    OP_pcmpgtq, OP_pcmpeqq, OP_pmulld, OP_pmuldq, OP_packusdw, OP_phminposuw,
    OP_mpsadbw, OP_roundps, OP_roundpd, OP_roundss, OP_roundsd, OP_dpps,
    OP_dppd, OP_blendps, OP_blendpd, OP_blendvps, OP_blendvpd, OP_pblendvb,
    OP_vpblendd, OP_pminsb, OP_pmaxsb, OP_pminsd, OP_pmaxsd,
    OP_pminuw, OP_pmaxuw, OP_pminud, OP_pmaxud,
    OP_pmovsxbw, OP_pmovsxbd, OP_pmovsxbq, OP_pmovsxwd, OP_pmovsxwq,
    OP_pmovsxdq, OP_pmovzxbw, OP_pmovzxbd, OP_pmovzxbq, OP_pmovzxwd,
    OP_pmovzxwq, OP_pmovzxdq,
    OP_ptest, OP_vtestps, OP_vtestpd,
    OP_insertps, OP_extractps, OP_pextrb, OP_pextrw, OP_pextrd,
    OP_vpextrb, OP_vpextrw, OP_vpextrd,        /* VEX-алиасы (и vpextrq) */
    OP_pinsrb, OP_pinsrd,
    OP_vpinsrb, OP_vpinsrw, OP_vpinsrd,        /* VEX-алиасы (и vpinsrq) */
    /* AES-NI / PCLMUL */
    OP_aesenc, OP_aesdec, OP_aesenclast, OP_aesdeclast, OP_aesimc,
    OP_aeskeygenassist, OP_pclmulqdq, OP_vpclmulqdq,
    /* AVX2 */
    OP_vpsllvd, OP_vpsrlvd, OP_vpsravd, OP_vpsllvq, OP_vpsrlvq, OP_vpsravq,
    OP_vpermq, OP_vpermd, OP_vperm2f128, OP_vperm2i128,
    OP_vpmaskmovd, OP_vpmaskmovq,
    OP_vpgatherdd, OP_vpgatherdq, OP_vpgatherqd, OP_vpgatherqq,
    OP_vgatherdps, OP_vgatherdpd, OP_vgatherqps, OP_vgatherqpd,
    OP_vcvtps2pd, OP_vcvtpd2ps,
    /* GPR */
    OP_crc32, OP_pext, OP_pdep, OP_bzhi, OP_bextr, OP_mulx, OP_adcx,
    OP_adox, OP_popcnt, OP_lzcnt, OP_tzcnt,
    /* §6.22: BMI1/BMI2 целиком в generic — inline-путь (rewrite_special)
     * искажал флаги: CF у blsi/blsmsk, PF у всех логических BMI,
     * и портил флаги shl/shr-расширениями sarx/shlx/shrx (SHLX флаги
     * не пишет, а shl — пишет). bun.exe на этом уходил в бесконечный
     * спин; sarx с dst==rcx оставался нативным и ронял DR. */
    OP_andn, OP_blsi, OP_blsmsk, OP_blsr, OP_sarx, OP_shlx, OP_shrx,
    OP_rorx,
    0
};

static int is_generic_opc(int opc);

int pass_is_generic_opc(int opc) { return is_generic_opc(opc); }

static int is_generic_opc(int opc)
{
    for (int k = 0; generic_opcs[k] != 0; k++)
        if (generic_opcs[k] == opc) return 1;
    return 0;
}


/* ─────────────── build_desc ─────────────── */

static void build_desc(instr_t *i, emu_desc_t *d)
{
    memset(d, 0, sizeof(*d));
    d->opc = instr_get_opcode(i);
    d->is_vex = (uint8_t)instr_bytes_vex(i);
    d->ndst = instr_num_dsts(i); if (d->ndst > 2) d->ndst = 2;
    d->nsrc = instr_num_srcs(i); if (d->nsrc > 4) d->nsrc = 4;
    for (int k = 0; k < d->ndst; k++) d->dsts[k] = instr_get_dst(i, k);
    for (int k = 0; k < d->nsrc; k++) d->srcs[k] = instr_get_src(i, k);

    for (int k = 0; k < 2; k++) {
        if (k >= d->ndst) break;
        opnd_t o = d->dsts[k];
        if (opnd_is_reg(o)) {
            reg_id_t r = opnd_get_reg(o);
            if ((r >= DR_REG_XMM0 && r <= DR_REG_XMM15) ||
                (r >= DR_REG_YMM0 && r <= DR_REG_YMM15)) {
                d->xmm_dst |= 1u << xmm_idx(r);
                if (xmm_idx(r) == 0) d->uses_xmm0 = 1;
            } else if (is_gpr_reg(r)) {
                d->gpr_dst |= 1u << gpr_idx(r);
            }
        } else if (opnd_is_memory_reference(o)) {
            reg_id_t b = opnd_get_base(o), ix = opnd_get_index(o);
            if (is_gpr_reg(b)) d->gpr_src |= 1u << gpr_idx(b);
            if (is_gpr_reg(ix)) d->gpr_src |= 1u << gpr_idx(ix);
        }
        if (opnd_get_size(o) == OPSZ_32) d->is_ymm = 1;
    }
    for (int k = 0; k < 4; k++) {
        if (k >= d->nsrc) break;
        opnd_t o = d->srcs[k];
        if (opnd_is_reg(o)) {
            reg_id_t r = opnd_get_reg(o);
            if ((r >= DR_REG_XMM0 && r <= DR_REG_XMM15) ||
                (r >= DR_REG_YMM0 && r <= DR_REG_YMM15)) {
                d->xmm_src |= 1u << xmm_idx(r);
                if (xmm_idx(r) == 0) d->uses_xmm0 = 1;
            } else if (is_gpr_reg(r)) {
                d->gpr_src |= 1u << gpr_idx(r);
            }
        } else if (opnd_is_memory_reference(o)) {
            reg_id_t b = opnd_get_base(o), ix = opnd_get_index(o);
            if (is_gpr_reg(b)) d->gpr_src |= 1u << gpr_idx(b);
            if (is_gpr_reg(ix)) d->gpr_src |= 1u << gpr_idx(ix);
            if (ix >= DR_REG_XMM0 && ix <= DR_REG_XMM15)
                d->xmm_src |= 1u << xmm_idx(ix);      /* gather: индекс в xmm */
            if (opnd_get_size(o) == OPSZ_32) d->is_ymm = 1;
        }
        if (opnd_get_size(o) == OPSZ_32) d->is_ymm = 1;
    }

    /* implicit-операнды */
    if (d->opc == OP_blendvps || d->opc == OP_blendvpd ||
        d->opc == OP_pblendvb || d->opc == OP_pcmpestri || d->opc == OP_pcmpestrm)
        d->uses_xmm0 = 1;
    if (d->opc == OP_mulx)
        d->gpr_src |= 1u << gpr_idx(DR_REG_RDX);      /* неявный RDX */
    if (d->opc == OP_vzeroall)
        d->xmm_dst = 0xFFFF;                          /* восстановить все xmm */

    /* флаги */
    uint eflags = instr_get_eflags(i, DR_QUERY_DEFAULT);
    if (eflags & (EFLAGS_READ_CF | EFLAGS_READ_ZF | EFLAGS_READ_SF |
                  EFLAGS_READ_OF | EFLAGS_READ_PF | EFLAGS_READ_AF |
                  EFLAGS_READ_DF))
        d->flags_use = 1;
    if (eflags & (EFLAGS_WRITE_CF | EFLAGS_WRITE_ZF | EFLAGS_WRITE_SF |
                  EFLAGS_WRITE_OF | EFLAGS_WRITE_PF | EFLAGS_WRITE_AF |
                  EFLAGS_WRITE_DF))
        d->flags_def = 1;
}

/* ─────────────── generic-путь (clean call) ─────────────── */

int rewrite_generic_instr(void *dc, instrlist_t *bb, instr_t *i)
{
    if (g_cfg.verbose >= 2)
        dr_fprintf(STDERR, "[P.A.S.S.] generic: opc=%d\n", instr_get_opcode(i));

    emu_desc_t *d = (emu_desc_t *)dr_global_alloc(sizeof(emu_desc_t));
    build_desc(i, d);
    d->pc_next = instr_get_app_pc(i) + instr_length(dc, i);

    /* r10 в операндах поддерживается: carrier_begin спилит исходный r10
     * в RAW_SV10, а ниже (4b) значение переносится в BUF_GPR(10) —
     * эмулятор читает операнд оттуда же, откуда читает остальные GPR.
     * Раньше такие инструкции отклонялись и оставались на native-пути
     * (на реальном Core 2 — SIGILL и trap-fallback на каждый вылет). */

    /* (1) носитель r10 */
    carrier_begin(dc, bb, i);

    /* (2) захват флагов: при flags_use — как источники; при flags_def —
     * ОБЯЗАТЕЛЬНО даже если инструкция их не читает: эмулятор обновляет
     * только арифметические биты поверх захваченного значения (flags_setmask
     — read-modify-write), и popf вернёт приложению нетронутыми DF/IF/TF и
     * прочие неопределяемые биты. Без захвата popf записал бы мусор из
     * BUF_FLAGS: у node 8000+ эмуляций tzcnt/bzhi перевернули DF → rep movs/stos шли
     * назад → «malloc(): corrupted top size». */
    if (d->flags_use || d->flags_def) {
        pass_ins(dc, bb, i, INSTR_CREATE_pushf(dc));
        pass_ins(dc, bb, i, INSTR_CREATE_pop(
            dc, buf_opnd(BUF_FLAGS, OPSZ_8)));
    }

    /* (3) спилы xmm */
    uint32_t xmmall = d->xmm_src | d->xmm_dst;
    for (int k = 0; k < 16; k++) {
        if (!(xmmall & (1u << k))) continue;
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, buf_opnd(BUF_XMM(k), OPSZ_16),
            opnd_create_reg(DR_REG_XMM0 + k)));
    }
    /* (4) спилы GPR: RAX всегда — он носитель при перезагрузке флагов.
     * r10 в цикле исключён: его значение переносится отдельно в (4b). */
    uint32_t gprall = (d->gpr_src | d->gpr_dst | (1u << gpr_idx(DR_REG_RAX)))
                      & ~(1u << gpr_idx(DR_REG_R10));
    for (int k = 0; k < 16; k++) {
        if (!(gprall & (1u << k))) continue;
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, buf_opnd(BUF_GPR(k), OPSZ_8),
            opnd_create_reg(DR_REG_RAX + k)));
    }

    /* (4b) операнд r10: исходное значение лежит в RAW_SV10 (gs-относительно,
     * carrier_begin). Переносим в BUF_GPR(10) через rax: rax уже спилен
     * выше (k=0 — первый в цикле), так что его значение не теряется. */
    if ((d->gpr_src | d->gpr_dst) & (1u << gpr_idx(DR_REG_R10))) {
        pass_ins(dc, bb, i, INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), raw_opnd(RAW_SV10, OPSZ_8)));
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, buf_opnd(BUF_GPR(gpr_idx(DR_REG_R10)), OPSZ_8),
            opnd_create_reg(DR_REG_RAX)));
    }

    /* (5) clean call — save_fpstate только когда трогаем xmm/ymm:
     * эмулятор скомпилирован с -mno-sse и xmm не портит, а fxsave+
     * fninit+fxrstor на каждый чистый вызов съедали ~60% стоимости
     * (bun.exe на core2: спин в OLL-обёртке DR, §6.24). */
    bool fp = (d->xmm_src | d->xmm_dst) != 0;
    dr_insert_clean_call(dc, bb, i, (void *)emu_generic, fp, 1,
                         OPND_CREATE_INTPTR((ptr_int_t)d));

    /* (5b) §7.3: прокси фолта. Если эмуляция столкнулась с недоступной
     * памятью приложения (emu_safe_read/write: BUF_TG != 0), здесь, в
     * кэше, исполняется НАСТОЯЩИЙ 1-байтовый доступ по записанному
     * адресу: фолт в кэше DR транслирует в app-PC эмулируемой
     * инструкции — SEH (Windows) и сигналы (Linux) приложения работают
     * как при нативном исполнении. BUF_TG: bit0 = фолт, bit1 = запись
     * (r10-относительные слоты: в контексте доставки сигнала gs-база
     * ненадёжна — доступ 0x290 фолтил, r10-носитель жив до (7)).
     * ОГРАНИЧЕНИЯ: прокси стоит ПОСЛЕ clean call — вне DR-umbrella —
     * поэтому НИ СТЕКА (красная зона!), ни регистров, кроме rax
     * (безусловно восстанавливается в (6)), трогать нельзя. Флаги
     * прокси портит (test/shr) — их восстанавливает безусловный
     * (6)-рестор из BUF_FLAGS, куда обёртка emu_generic кладёт
     * app-флаги на каждом вызове. При фолте доставляются флаги после
     * test/shr — отличие от нативных pre-instruction флагов принято
     * как ограничение v1 (хендлеры флаги не читают).             */
    {
        instr_t *l_end = INSTR_CREATE_label(dc);
        instr_t *l_read = INSTR_CREATE_label(dc);
        /* фолт-путь восстанавливает КОНТЕКСТ ПРИЛОЖЕНИЯ (флаги, rax,
         * r10 из BUF — туда их кладёт обёртка) ДО настоящего доступа:
         * доставленный хендлеру контекст отличается от нативного одним
         * регистром-throwaway (r10 = адрес фолта / прочитанный байт). */
        instr_t *p_movc = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_TG, OPSZ_8));
        instr_t *p_t0 = INSTR_CREATE_test(
            dc, opnd_create_reg(DR_REG_RAX), opnd_create_reg(DR_REG_RAX));
        instr_t *p_jz = INSTR_CREATE_jcc(dc, OP_jz, opnd_create_instr(l_end));
        instr_t *p_shr = INSTR_CREATE_shr(
            dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT8(1));
        instr_t *p_jr = INSTR_CREATE_jcc(dc, OP_jz, opnd_create_instr(l_read));
        /* WRITE-путь */
        instr_t *p_wfa = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FAULT, OPSZ_8));
        instr_t *p_wpush = INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX));
        instr_t *p_wfl = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_wpushf = INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX));
        instr_t *p_wpopf = INSTR_CREATE_popf(dc);
        instr_t *p_wrax = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_GPR(0), OPSZ_8));
        instr_t *p_wr10 = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_R10), buf_opnd(BUF_R10, OPSZ_8));
        instr_t *p_wpop = INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_R10));
        instr_t *p_wacc = INSTR_CREATE_mov_st(
            dc, opnd_create_base_disp(DR_REG_R10, DR_REG_NULL, 0, 0, OPSZ_1),
            OPND_CREATE_INT8(0));
        instr_t *p_wjmp = INSTR_CREATE_jmp_short(dc, opnd_create_instr(l_end));
        /* READ-путь */
        instr_t *p_rfa = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FAULT, OPSZ_8));
        instr_t *p_rpush = INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX));
        instr_t *p_rfl = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_rpushf = INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX));
        instr_t *p_rpopf = INSTR_CREATE_popf(dc);
        instr_t *p_rrax = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_GPR(0), OPSZ_8));
        instr_t *p_rr10 = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_R10), buf_opnd(BUF_R10, OPSZ_8));
        instr_t *p_rpop = INSTR_CREATE_pop(dc, opnd_create_reg(DR_REG_R10));
        instr_t *p_racc = INSTR_CREATE_movzx(
            dc, opnd_create_reg(DR_REG_R10D), opnd_create_base_disp(
                DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_1));
        instr_t *proxy_ins[] = { p_movc, p_t0, p_jz, p_shr, p_jr,
                                 p_wfa, p_wpush, p_wfl, p_wpushf, p_wpopf,
                                 p_wrax, p_wr10, p_wpop, p_wacc, p_wjmp,
                                 p_rfa, p_rpush, p_rfl, p_rpushf, p_rpopf,
                                 p_rrax, p_rr10, p_rpop, p_racc };
        for (size_t pi = 0; pi < sizeof(proxy_ins)/sizeof(proxy_ins[0]); pi++)
            instr_set_translation(proxy_ins[pi], instr_get_app_pc(i));
        instrlist_preinsert(bb, i, p_movc);
        instrlist_preinsert(bb, i, p_t0);
        instrlist_preinsert(bb, i, p_jz);
        instrlist_preinsert(bb, i, p_shr);
        instrlist_preinsert(bb, i, p_jr);
        instrlist_preinsert(bb, i, p_wfa);
        instrlist_preinsert(bb, i, p_wpush);
        instrlist_preinsert(bb, i, p_wfl);
        instrlist_preinsert(bb, i, p_wpushf);
        instrlist_preinsert(bb, i, p_wpopf);
        instrlist_preinsert(bb, i, p_wrax);
        instrlist_preinsert(bb, i, p_wr10);
        instrlist_preinsert(bb, i, p_wpop);
        instrlist_preinsert(bb, i, p_wacc);
        instrlist_preinsert(bb, i, p_wjmp);
        instrlist_preinsert(bb, i, l_read);
        instrlist_preinsert(bb, i, p_rfa);
        instrlist_preinsert(bb, i, p_rpush);
        instrlist_preinsert(bb, i, p_rfl);
        instrlist_preinsert(bb, i, p_rpushf);
        instrlist_preinsert(bb, i, p_rpopf);
        instrlist_preinsert(bb, i, p_rrax);
        instrlist_preinsert(bb, i, p_rr10);
        instrlist_preinsert(bb, i, p_rpop);
        instrlist_preinsert(bb, i, p_racc);
        instrlist_preinsert(bb, i, l_end);
    }

    /* (6) восстановление: после call-последовательности, до i */
    instr_t *cur = instr_get_prev(i);
    uint32_t gdst = d->gpr_dst;
    /* xmm-результаты */
    for (int k = 0; k < 16; k++) {
        if (!(d->xmm_dst & (1u << k))) continue;
        instr_t *ni = INSTR_CREATE_movups(
            dc, opnd_create_reg(DR_REG_XMM0 + k), buf_opnd(BUF_XMM(k), OPSZ_16));
        instrlist_postinsert(bb, cur, ni);
        cur = ni;
    }
    /* GPR-результаты, кроме rax (flags-танец) и r10 (носитель) */
    for (int k = 0; k < 16; k++) {
        if (!(gdst & (1u << k))) continue;
        if (k == gpr_idx(DR_REG_RAX) || k == gpr_idx(DR_REG_R10)) continue;
        instr_t *ni = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX + k), buf_opnd(BUF_GPR(k), OPSZ_8));
        instrlist_postinsert(bb, cur, ni);
        cur = ni;
    }
    /* VEX.128-запись обнуляет старшие половины (инвариант shadow).
     * Проверяем по фактическому VEX-префиксу инструкции: легаси- и
     * VEX-формы SSE4.1 имеют ОДИН opcode в DR (pmovsxbw и т.п.),
     * табличный признак v0.1 ошибочно глушал обнуление для VEX-форм. */
    if (d->xmm_dst != 0 && !d->is_ymm && instr_bytes_vex(i)) {
        for (int k = 0; k < 16; k++) {
            if (!(d->xmm_dst & (1u << k))) continue;
            instr_t *z1 = INSTR_CREATE_mov_st(dc, raw_opnd(RAW_YHI(k), OPSZ_4),
                                              OPND_CREATE_INT32(0));
            instr_t *z2 = INSTR_CREATE_mov_st(dc, raw_opnd(RAW_YHI(k) + 4, OPSZ_4),
                                              OPND_CREATE_INT32(0));
            instr_t *z3 = INSTR_CREATE_mov_st(dc, raw_opnd(RAW_YHI(k) + 8, OPSZ_4),
                                              OPND_CREATE_INT32(0));
            instr_t *z4 = INSTR_CREATE_mov_st(dc, raw_opnd(RAW_YHI(k) + 12, OPSZ_4),
                                              OPND_CREATE_INT32(0));
            instr_set_meta(z1); instr_set_meta(z2);
            instr_set_meta(z3); instr_set_meta(z4);
            instrlist_postinsert(bb, cur, z1); cur = z1;
            instrlist_postinsert(bb, cur, z2); cur = z2;
            instrlist_postinsert(bb, cur, z3); cur = z3;
            instrlist_postinsert(bb, cur, z4); cur = z4;
        }
    }
    /* флаги: безусловное восстановление — прокси (5b) мог их портить,
     * а обёртка emu_generic кладёт app-флаги в BUF_FLAGS на каждом вызове */
    if (1) {
        instr_t *n1 = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                          buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *n2 = INSTR_CREATE_push(dc, opnd_create_reg(DR_REG_RAX));
        instr_t *n3 = INSTR_CREATE_popf(dc);
        instrlist_postinsert(bb, cur, n1); cur = n1;
        instrlist_postinsert(bb, cur, n2); cur = n2;
        instrlist_postinsert(bb, cur, n3); cur = n3;
    }
    /* rax — всегда последним: popf затирает его носителем;
       спил гарантирует исходное значение (rax — dst или источник) */
    {
        instr_t *ni = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                          buf_opnd(BUF_GPR(0), OPSZ_8));
        instrlist_postinsert(bb, cur, ni);
        cur = ni;
    }
    /* (7) r10: если он выходной — результат операции (из BUF_GPR(10));
     * иначе — исходное значение приложения из RAW_SV10 (gs-относительно,
     * после clean call r10 всё ещё держит базу BUF — любой buf_opnd выше
     * уже отработал, поэтому перезагрузка идёт последней). */
    {
        instr_t *ni = (gdst & (1u << gpr_idx(DR_REG_R10)))
            ? INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_R10),
                                  buf_opnd(BUF_GPR(gpr_idx(DR_REG_R10)), OPSZ_8))
            : INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_R10),
                                  raw_opnd(RAW_SV10, OPSZ_8));
        instr_set_meta(ni);
        instrlist_postinsert(bb, cur, ni);
        cur = ni;
    }
    (void)cur;

    /* (8) удалить исходную инструкцию */
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

/* ─────────────── главный диспетчер ─────────────── */
int g_disable_256 = 0, g_disable_128 = 0, g_disable_special = 0;

/* ── трассировка операндов: ловим вызовы get_dst/get_src по инструкциям
 * без операндов до того, как релизный DR уронит процесс (см. pass.h).
 * ra — адрес вызова в клиенте; вместе с печатью адреса самой обёртки
 * это позволяет вычислить файловый offset call site'а (addr2line).
 * Реальные функции доступны через asm-label на DR-экспорт (INSTR_GET_DST
 * — internal API, клиентам недоступен).                              */
/* Реальные функции: GCC/MinGW — asm-label на DR-экспорт; MSVC —
 * unresolved-символ, разрешаемый линкером через /alternatename
 * (INSTR_GET_DST — internal API, клиентам недоступен).             */
#ifdef _MSC_VER
opnd_t real_get_dst_fn(instr_t *instr, uint pos);
#pragma comment(linker, "/alternatename:real_get_dst_fn=instr_get_dst")
opnd_t real_get_src_fn(instr_t *instr, uint pos);
#pragma comment(linker, "/alternatename:real_get_src_fn=instr_get_src")
#else
extern opnd_t real_get_dst_fn(instr_t *instr, uint pos) __asm__("instr_get_dst");
extern opnd_t real_get_src_fn(instr_t *instr, uint pos) __asm__("instr_get_src");
#endif

opnd_t pass_traced_get_dst(instr_t *in, uint pos, void *ra)
{
    /* диагностика под env-гейтом PASS_OPND_TRACE=1: без неё — обычное
     * поведение релизного DR (краш на dsts==NULL), с ней — громкий
     * отчёт с адресом call site'а до краша (§7.3)                   */
    static int on = -1;
    if (on == -1)
        on = (getenv("PASS_OPND_TRACE") != NULL);
    if (on && (!instr_operands_valid(in) || pos >= (uint)instr_num_dsts(in))) {
        void *self = (void *)&pass_traced_get_dst;
        dr_fprintf(STDERR,
                   "[P.A.S.S.][OPND-TRAP] dst pos=%u ndst=%u nsrc=%u opc=%d "
                   "meta=%d valid=%d ra=" PIFX " self=" PIFX "\n",
                   pos, (uint)instr_num_dsts(in), (uint)instr_num_srcs(in),
                   instr_get_opcode(in), instr_is_meta(in) ? 1 : 0,
                   instr_operands_valid(in) ? 1 : 0,
                   (ptr_uint_t)ra, (ptr_uint_t)self);
        dr_flush_file(STDERR);
        dr_abort();
    }
    return real_get_dst_fn(in, pos);
}

opnd_t pass_traced_get_src(instr_t *in, uint pos, void *ra)
{
    static int on = -1;
    if (on == -1)
        on = (getenv("PASS_OPND_TRACE") != NULL);
    if (on && (!instr_operands_valid(in) || pos >= (uint)instr_num_srcs(in))) {
        void *self = (void *)&pass_traced_get_src;
        dr_fprintf(STDERR,
                   "[P.A.S.S.][OPND-TRAP] src pos=%u ndst=%u nsrc=%u opc=%d "
                   "meta=%d valid=%d ra=" PIFX " self=" PIFX "\n",
                   pos, (uint)instr_num_dsts(in), (uint)instr_num_srcs(in),
                   instr_get_opcode(in), instr_is_meta(in) ? 1 : 0,
                   instr_operands_valid(in) ? 1 : 0,
                   (ptr_uint_t)ra, (ptr_uint_t)self);
        dr_flush_file(STDERR);
        dr_abort();
    }
    return real_get_src_fn(in, pos);
}

static void report_unsupported(void *dc, instr_t *i)
{
    int opc = instr_get_opcode(i);
    if (opc < OPC_MAX)
        PASS_ATOMIC_INC32(&g_stat_unsupported[opc]);
    if (g_cfg.verbose) {
        dr_fprintf(STDERR, "[P.A.S.S.] нет эмуляции для opc=%d @ " PIFX
                           " — останется на trap-fallback\n", opc,
                   (ptr_uint_t)instr_get_app_pc(i));
    }
    (void)dc;
}

/* Обёртка переписывания: вокруг зоны вставки ставится sentinel-метка,
 * после переписывания всем вставленным app-инструкциям проставляется
 * translation-поле (требование DR для маппинга сигналов/стеков).    */
typedef int (*rewrite_fn_t)(void *dc, instrlist_t *bb, instr_t *i);

static int run_rewriter(void *dc, instrlist_t *bb, instr_t *i, rewrite_fn_t fn)
{
    instr_t *prev = instr_get_prev(i);
    int opc0 = instr_get_opcode(i);   /* до fn(): fn уничтожает i */
    app_pc pc = instr_get_app_pc(i);
    instr_t *sentinel = INSTR_CREATE_label(dc);
    instrlist_postinsert(bb, i, sentinel);
    int handled = fn(dc, bb, i);
    if (handled && opc0 < OPC_MAX)
        PASS_ATOMIC_INC32(&g_stat_static[opc0]);
    if (handled && g_cfg.verbose >= 3) {
        dr_fprintf(STDERR, "[P.A.S.S.] rewrite opc=%d @ " PIFX ":\n",
                   opc0, (ptr_uint_t)pc);
        instr_t *n = (prev != NULL) ? instr_get_next(prev) : instrlist_first(bb);
        while (n != sentinel && n != NULL) {
            dr_fprintf(STDERR, "    ");
            instr_disassemble(dc, n, STDERR);
            dr_fprintf(STDERR, "\n");
            n = instr_get_next(n);
        }
    }
    if (handled) {
        instr_t *n = (prev != NULL) ? instr_get_next(prev) : instrlist_first(bb);
        while (n != sentinel && n != NULL) {
            if (!instr_is_meta(n))
                instr_set_translation(n, pc);
            n = instr_get_next(n);
        }
    }
    instrlist_remove(bb, sentinel);
    instr_destroy(dc, sentinel);
    return handled;
}

/* void-обёртки для cpuid/xgetbv-хуков */
static int rewrite_cpuid_w(void *dc, instrlist_t *bb, instr_t *i)
{ rewrite_cpuid(dc, bb, i); return 1; }
static int rewrite_xgetbv_w(void *dc, instrlist_t *bb, instr_t *i)
{ rewrite_xgetbv(dc, bb, i); return 1; }
static int rewrite_nop_w(void *dc, instrlist_t *bb, instr_t *i)
{
    instr_t *nop = INSTR_CREATE_nop(dc);
    instrlist_preinsert(bb, i, nop);
    instrlist_remove(bb, i);
    instr_destroy(dc, i);
    return 1;
}

static void rewrite_instr(void *dc, instrlist_t *bb, instr_t *i)
{
    if (instr_is_meta(i)) return;
    int opc = instr_get_opcode(i);
    opnd_size_t dsz = (instr_num_dsts(i) > 0) ? opnd_get_size(instr_get_dst(i, 0))
                                              : OPSZ_NA;

    /* системные особые случаи */
    if (opc == OP_cpuid && !g_no_spoof) { run_rewriter(dc, bb, i, rewrite_cpuid_w); return; }
    if (opc == OP_xgetbv && !g_no_spoof) { run_rewriter(dc, bb, i, rewrite_xgetbv_w); return; }
    if (opc == OP_prefetchw && !opc_supported(opc, dsz)) {
        run_rewriter(dc, bb, i, rewrite_nop_w);
        return;
    }

    if (opc_supported(opc, dsz))
        return;                              /* passthrough */

    /* AVX-512/EVEX — не эмулируем: громкий trap вместо тихого passthrough */
    if (instr_bytes_evex(i)) {
        report_unsupported(dc, i);
        return;
    }
    for (int k = 0; k < instr_num_srcs(i); k++) {
        if (opnd_is_reg(instr_get_src(i, k)) &&
            opnd_get_reg(instr_get_src(i, k)) >= DR_REG_K0) {
            report_unsupported(dc, i);
            return;
        }
    }
    for (int k = 0; k < instr_num_dsts(i); k++) {
        if (opnd_is_reg(instr_get_dst(i, k)) &&
            opnd_get_reg(instr_get_dst(i, k)) >= DR_REG_K0) {
            report_unsupported(dc, i);
            return;
        }
    }

    if (is_generic_opc(opc)) {
        if (g_cfg.enable_generic &&
            run_rewriter(dc, bb, i, rewrite_generic_instr))
            return;
        report_unsupported(dc, i);
        return;
    }
    /* 256-битные упакованные FMA: inline-путь (try_fma) умеет только
     * 128-битные — 256-битные ведём в generic (или trap при -generic 0) */
    if (dsz == OPSZ_32 && g_cfg.enable_generic &&
        (opc == OP_vfmadd132ps || opc == OP_vfmadd132pd ||
         opc == OP_vfmadd213ps || opc == OP_vfmadd213pd ||
         opc == OP_vfmadd231ps || opc == OP_vfmadd231pd ||
         opc == OP_vfmsub132ps || opc == OP_vfmsub132pd ||
         opc == OP_vfmsub213ps || opc == OP_vfmsub213pd ||
         opc == OP_vfmsub231ps || opc == OP_vfmsub231pd ||
         opc == OP_vfnmadd132ps || opc == OP_vfnmadd132pd ||
         opc == OP_vfnmadd213ps || opc == OP_vfnmadd213pd ||
         opc == OP_vfnmadd231ps || opc == OP_vfnmadd231pd ||
         opc == OP_vfnmsub132ps || opc == OP_vfnmsub132pd ||
         opc == OP_vfnmsub213ps || opc == OP_vfnmsub213pd ||
         opc == OP_vfnmsub231ps || opc == OP_vfnmsub231pd ||
         opc == OP_vfmaddsub132ps || opc == OP_vfmaddsub132pd ||
         opc == OP_vfmaddsub213ps || opc == OP_vfmaddsub213pd ||
         opc == OP_vfmaddsub231ps || opc == OP_vfmaddsub231pd ||
         opc == OP_vfmsubadd132ps || opc == OP_vfmsubadd132pd ||
         opc == OP_vfmsubadd213ps || opc == OP_vfmsubadd213pd ||
         opc == OP_vfmsubadd231ps || opc == OP_vfmsubadd231pd) &&
        run_rewriter(dc, bb, i, rewrite_generic_instr))
        return;
    if (g_cfg.enable_inline && run_rewriter(dc, bb, i, try_rewrite_vex))
        return;
    if (g_cfg.enable_inline && !g_disable_special &&
        run_rewriter(dc, bb, i, try_rewrite_special))
        return;

    report_unsupported(dc, i);
}

dr_emit_flags_t pass_bb_event(void *dc, void *tag, instrlist_t *bb,
                              bool for_trace, bool translating)
{
    (void)tag; (void)for_trace;
    if (translating)
        return DR_EMIT_DEFAULT;      /* результат детерминирован */

    PASS_ATOMIC_INC32(&g_stat_bbs);

    instr_t *i = instrlist_first(bb);
    while (i != NULL) {
        instr_t *next = instr_get_next(i);
        rewrite_instr(dc, bb, i);
        i = next;
    }
    return DR_EMIT_DEFAULT;
}

void pass_rewrite_init(void)
{
    g_cfg.shadow_active = 1;   /* обнуление YHI безусловно, flush не нужен */
}
