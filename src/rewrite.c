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
    /* §7.3 fault#1: КОСОСИНУС контракта DR-клиента — translation НИ
     * на одной вставленной инструкции не выставлялся (кроме прокси).
     * Фолт/релокация на app-инструкции стаба (staged-загрузки mem-
     * операндов, финальная legacy-инструкция) давала мусорный app-PC;
     * на Windows это кончилось encode-to-NULL в instr_encode_arch.
     * Каждая инструкция стаба реализует заменяемую `at` — её app-PC
     * и есть правильная трансляция. */
    instr_set_translation(in, instr_get_app_pc(at));
    /* B-2009: маркер наших мета-инструкций для restore_state (рекреация
     * app-контекста при SMC-flush/deletion-redirect — см. restore_state.c) */
    instr_set_note(in, PASS_META_NOTE);
    /* §7.3 fault#1 КОРЕНЬ: emit.c делает instr_encode_to_copy и
     * ASSERT(pc != NULL) — в релизе проверка ВЫРЕЗАНА. Некодируемая
     * инструкция в стабе (2-я семья бага #10 после vcmpsd) оставляет
     * pc=NULL, и СЛЕДУЮЩАЯ инструкция стаба — наша gs-TLS staging —
     * кодируется по NULL (crash: write-AV @0, «movb $0x65,(%rsi)» =
     * GS-префикс, rsi=0 — первый байт gs-инструкции). Контроль на
     * месте создания: некодируемая инструкция = ГРОМКАЯ остановка
     * с именем опкода (фиксится таблицей, как vcmpsd в #10). */
    if (!instr_is_label(in) && !instr_raw_bits_valid(in)) {
        byte encbuf[32];
        /* rip-rel операнды пропускаем: DR манглит их в абсолют (mangle_rel_addr)
         * до эмиссии, энкодер их не видит; честный тест даст ложный фолт
         * (disp32 из кэша не достаёт до app-данных — нормально). */
        int has_rip_rel = 0;
        for (uint q = 0; q < instr_num_dsts(in) && !has_rip_rel; q++)
            has_rip_rel = opnd_is_near_rel_addr(instr_get_dst(in, q));
        for (uint q = 0; q < instr_num_srcs(in) && !has_rip_rel; q++)
            has_rip_rel = opnd_is_near_rel_addr(instr_get_src(in, q));
        if (!has_rip_rel && instr_encode(dc, in, encbuf) == NULL) {
            /* однострочный формат: хвост 200Б в CI должен поймать всё */
            dr_fprintf(STDERR, "[P.A.S.S.] UNENCODABLE new-opc=%d(0x%x) <<< ",
                       instr_get_opcode(in), instr_get_opcode(in));
            opnd_disassemble(dc, instr_get_dst(in, 0), STDERR);
            dr_fprintf(STDERR, " , ");
            if (instr_num_srcs(in) > 0)
                opnd_disassemble(dc, instr_get_src(in, 0), STDERR);
            dr_fprintf(STDERR, " <<< orig-opc=%d app=" PIFX " <<< ",
                       instr_get_opcode(at), (ptr_uint_t)instr_get_app_pc(at));
            instr_disassemble(dc, at, STDERR);
            dr_fprintf(STDERR, " >>> END\n");
            dr_flush_file(STDERR);
            dr_abort();
        }
    }
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
/* §7.3: r10 в кэше = носитель BUF; inline-рерайты инструкций с r10 в
 * операндах (рег или mem base/index) исполнятся с НЕВЕРНЫМ r10 —
 * такие инструкции отклоняются в generic-путь (он спилит r10 в
 * BUF_GPR(10) через (4b) и прочитает корректно).                  */
int opnd_uses_r10(opnd_t o)
{
    if (opnd_is_reg(o) && opnd_get_reg(o) == DR_REG_R10)
        return 1;
    if (opnd_is_memory_reference(o)) {
        /* B-2009: opnd_get_base валиден ТОЛЬКО для base-disp — rel/abs-адреса
         * (vmovdqa <abs>[32byte] после rip-rel decode, call %rbx rsp-write)
         * тоже memory_reference, но get_base на них — CLIENT_ASSERT в
         * debug-DR и мусор в release. */
        if (opnd_is_base_disp(o) || opnd_is_far_base_disp(o)) {
            reg_id_t b = opnd_get_base(o), ix = opnd_get_index(o);
            if (b == DR_REG_R10 || ix == DR_REG_R10)
                return 1;
        }
    }
    return 0;
}

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
    /* §7.4 раунд #111: строковая семья — маршрутизация из left-native
     * в generic (README: «маршрутизация в emu — следующий раунд»).
     * На реальном Core 2 каждый вызов был SIGILL + trap-fallback
     * (bun/simdutf звает pcmpistri на каждый поиск), теперь — один
     * clean call; семантика выверена натурной пробой (t_istr.S). */
    OP_pcmpestri, OP_pcmpestrm, OP_pcmpistri, OP_pcmpistrm,
    OP_vpcmpestri, OP_vpcmpestrm, OP_vpcmpistri, OP_vpcmpistrm,
    /* §7.4 раунд #111: VEX-алиасы уже-эмулируемых SSE4.1/4.2/AES —
     * тот же класс, что vpsubusb (t_iemu 19/20/23): без маршрутизации —
     * left-native (мусор тени на дев-хосте, SIGILL на Core 2) */
    OP_vpcmpeqq, OP_vpcmpgtq, OP_vpmulld, OP_vpmuldq,
    OP_vpackusdw, OP_vphminposuw, OP_vmpsadbw,
    OP_vroundps, OP_vroundpd, OP_vroundss, OP_vroundsd,
    OP_vdpps, OP_vdppd, OP_vblendps, OP_vblendpd,
    OP_vblendvps, OP_vblendvpd, OP_vpblendvb,
    OP_vpminuw, OP_vpmaxuw, OP_vpminud, OP_vpmaxud,
    OP_vpminsb, OP_vpmaxsb, OP_vpminsd, OP_vpmaxsd,
    OP_vpmovsxbw, OP_vpmovsxbd, OP_vpmovsxbq,
    OP_vpmovsxwd, OP_vpmovsxwq, OP_vpmovsxdq,
    OP_vpmovzxbw, OP_vpmovzxbd, OP_vpmovzxbq,
    OP_vpmovzxwd, OP_vpmovzxwq, OP_vpmovzxdq,
    OP_vinsertps, OP_vextractps, OP_vptest,
    OP_vaesenc, OP_vaesdec, OP_vaesenclast, OP_vaesdeclast,
    OP_vaesimc, OP_vaeskeygenassist,
    /* AVX1: vmaskmov (load/store — один opc, ветвление в emu по dst),
     * cvt-ширины (256-битные формы читают младшую половину источника —
     * механическое расщепление inline-карты неприменимо), permil */
    OP_vmaskmovps, OP_vmaskmovpd,
    OP_vcvtdq2pd, OP_vcvtpd2dq, OP_vcvttpd2dq,
    OP_vpermilps, OP_vpermilpd,
    /* полуходы: load-форма = merge с m64 (reg,reg legacy-формы нет),
     * store-форма читает верх результата из RAW_YHI-тени */
    OP_vmovlps, OP_vmovhps, OP_vmovlpd, OP_vmovhpd,
    /* §7.5: скалярные сравнения vcmpss/vcmpsd — из inline-карты убраны
     * в §7.3 (legacy-alias OP_cmpsd некодируем), в generic не попадали:
     * left-native → на CPU без AVX #UD, bun-рантайм возвращает
     * управление на ту же инструкцию → замкнутый trap-цикл, exit 86
     * (натурный репродуктор на Windows, bun и bun-baseline одинаково).
     * Все 32 предиката SDM Table 4-10 — vcmp_result в emu.c (t_vcmp). */
    OP_vcmpss, OP_vcmpsd,
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
    /* diag: -allgeneric — принудительно вести все special-опкоды в generic */
    {
        extern int g_all_generic;
        if (g_all_generic && special_req_ok(opc)) return 1;
    }
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
    /* §7.4 #111: лимит 5 — pcmpestri/strm несут srcs[4]=EDX (длина) */
    d->nsrc = instr_num_srcs(i); if (d->nsrc > 5) d->nsrc = 5;
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
            /* B-2009: base/index только для base-disp (см. opnd_uses_r10) */
            if (opnd_is_base_disp(o) || opnd_is_far_base_disp(o)) {
                reg_id_t b = opnd_get_base(o), ix = opnd_get_index(o);
                if (is_gpr_reg(b)) d->gpr_src |= 1u << gpr_idx(b);
                if (is_gpr_reg(ix)) d->gpr_src |= 1u << gpr_idx(ix);
            }
        }
        if (opnd_get_size(o) == OPSZ_32) d->is_ymm = 1;
    }
    /* §7.4 #111: лимит 5 — pcmpestri/strm несут srcs[3]=EAX, srcs[4]=EDX
     * (длины строк); при жёстком 4 эти источники не попадали в gpr_src,
     * не спиллись в BUF_GPR, и emu читал длины как нули (86/180 кейсов
     * t_istr расходились: ECX=16, CF=0 — как у пустых строк). */
    for (int k = 0; k < 5; k++) {
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
            /* B-2009: base/index только для base-disp (см. opnd_uses_r10) */
            if (opnd_is_base_disp(o) || opnd_is_far_base_disp(o)) {
                reg_id_t b = opnd_get_base(o), ix = opnd_get_index(o);
                if (is_gpr_reg(b)) d->gpr_src |= 1u << gpr_idx(b);
                if (is_gpr_reg(ix)) d->gpr_src |= 1u << gpr_idx(ix);
                if (ix >= DR_REG_XMM0 && ix <= DR_REG_XMM15)
                    d->xmm_src |= 1u << xmm_idx(ix);      /* gather: индекс в xmm */
            }
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
    /* B-2009: регистрация переписанного pc → маски dst/флагов для
     * restore_state (рекреация SMC-flush/deletion-redirect). */
    pass_rw_record((void *)instr_get_app_pc(i), d->gpr_dst, d->xmm_dst,
                   (d->flags_use || d->flags_def) ? 1 : 0);

    /* r10 в операндах поддерживается: carrier_begin спилит исходный r10
     * в RAW_SV10, а ниже (4b) значение переносится в BUF_GPR(10) —
     * эмулятор читает операнд оттуда же, откуда читает остальные GPR.
     * Раньше такие инструкции отклонялись и оставались на native-пути
     * (на реальном Core 2 — SIGILL и trap-fallback на каждый вылет). */

    /* (1) носитель r10 */
    carrier_begin(dc, bb, i);

    /* (2) B-2011: стековый захват флагов УДАЛЁН. Раньше: pushf→BUF_FLAGS
     * ×2 (BUF_FLAGS0) — записи в [app_rsp-8], т.е. В КРАСНУЮ ЗОНУ x86-64
     * (128 байт под rsp — живые данные листовых функций; t_redzone:
     * 0x44 вместо 0x45, bun: Math.max → undefined, console.log молчит).
     * Потребность в стеке отпала целиком:
     *  — аппаратные флаги через всю пре-последовательность НЕ трогаются
     *    (mov/movups/lahf/seto — флаг-сохраняющие), поэтому mc.xflags в
     *    точке clean call = ИСТИННЫЙ пред-оп образ;
     *  — обёртка emu_generic кладёт его в BUF_FLAGS (вход эмулятора) и
     *    BUF_FLAGS0 (копия restore_state) — обе роли бывшего захвата;
     *  — DF/IF/TF не восстанавливаются popf'ом, а сохраняются ТРАНЗИТИВНО:
     *    их не пишет ни одна инструкция последовательности (в отличие от
     *    popf мусорного образа — баг node 8000+). */

    /* (3) спилы xmm.
     * §7.3 run #106 (B): uses_xmm0 (неявная маска XMM0 у pcmpestri/
     * pcmpestrm/blendv-семьи) обязан попадать в спил — раньше в xmmall
     * входили только ЯВНЫЕ операнды, а у pcmpestri XMM0 в IR-модели
     * НЕ ПРИСУТСТВУЕТ (dst=ECX, srcs=[A,B,imm,EAX,EDX]) → emu читал
     * мусор из BUF_XMM(0) вместо маски → порча строковых результатов
     * в bun (simdutf/JSC). Для legacy blendv/pblendvb XMM0 в IR явный
     * (srcs[1]) — там спил и раньше работал, uses_xmm0 закрывает
     * остальные случаи и trap-путь.                                    */
    uint32_t xmmall = d->xmm_src | d->xmm_dst | (d->uses_xmm0 ? 1u : 0u);
    for (int k = 0; k < 16; k++) {
        if (!(xmmall & (1u << k))) continue;
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, buf_opnd(BUF_XMM(k), OPSZ_16),
            opnd_create_reg(DR_REG_XMM0 + k)));
    }
    /* B-2009: пред-оп спил xmm-dst в BUF_XMMD — BUF_XMM(k) перезаписывается
     * результатом emu; при рекреации SMC-flush нужен ИСХОДНЫЙ dst
     * (реисполнение dst∧src — vpblendvb ymm с dst==src). */
    for (int k = 0; k < 16; k++) {
        if (!(d->xmm_dst & (1u << k))) continue;
        pass_ins(dc, bb, i, INSTR_CREATE_movups(
            dc, buf_opnd(BUF_XMMD(k), OPSZ_16),
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

    /* B-2009: пред-оп спил GPR-dst + rax в BUF_GPRD — BUF_GPR(k) для dst
     * перезаписывается результатом emu; restore_state восстанавливает
     * исходные dst для реисполнения (crc32/popcnt/adcx с dst==rax и
     * прочие dst∧src-опы). */
    for (int k = 0; k < 16; k++) {
        if (!(((d->gpr_dst | (1u << gpr_idx(DR_REG_RAX)))) & (1u << k))) continue;
        pass_ins(dc, bb, i, INSTR_CREATE_mov_st(
            dc, buf_opnd(BUF_GPRD(k), OPSZ_8),
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

    /* (5) clean call — fp=TRUE ВСЕГДА (см. комментарий ниже).
     * История: раньше fp был условным «(xmm_src|xmm_dst)!=0» с мотивом
     * «fxsave+fninit+fxrstor на каждый вызов съедали ~60% стоимости
     * (bun.exe на core2: спин в OLL-обёртке DR, §6.24)». Оптимизация
     * несостоятельна — см. §7.3 run #103: bun -e падал SyntaxError
     * (порча xmm0-5 приложения каллби).                             */
    /* §7.3 run #103: fp = TRUE ВСЕГДА. Условие (xmm_src|xmm_dst)!=0 —
     * несостоятельная оптимизация: каллби (emu_generic) компилируется
     * с -msse2 (GNU) / без ограничений (MSVC) — memcpy/структурные
     * копии инлайнятся в movups xmm0-5, и при fp=false DR НЕ спасает
     * xmm вокруг вызова → ПОРЧА векторов приложения. На Linux везло
     * (кодеген не вкладывал SSE именно в fp=false-путях), на MSVC —
     * закономерно: bun -e → SyntaxError в builtin (порча строк-данных
     * в xmm0-5, векторные буферы JSC/bun). Бисекция #103: чистый DR →
     * OK, -stage 1 → OK, -inline 0 -generic 0 (спуф без стабов) → OK,
     * -inline 0 (generic-стабы) → FAIL, default → FAIL. Батарея
     * локально зелёная и с fp=true (ценой fxsave на вызов — до
     * отдельной xmm-чистой сборки emu, см. v0.3).                 */
    bool fp = true;
    /* B-2014: КРАСНАЯ ЗОНА x86-64 вокруг clean call. DR-кодеген clean
     * call пушит все GPR (+fpstate fxsave) на app-стек ПОД rsp — прямо
     * в [rsp-128, rsp): bun (clang -O2) держит там локальные данные
     * leaf-функций. Симптом: require('fs') ломался во ВСЕХ режимах
     * (спец-танец → SIGSEGV rsi=0xd в классификаторе модулей 0x2ecdc54
     * [ptr-поле структуры = 13 = длина соседнего поля]; trap-путь →
     * ошибка лексера «Expected ";" but found " "» и «Maximum call
     * stack size exceeded») — tzcnt 5473×/pmovsxbd 994× через generic
     * стаб между записью в red zone и её чтением. Батареи не ловили:
     * асмовый тестовый код red zone не использует. Фикс: sub rsp,0x80
     * перед call — пушы DR уходят НИЖЕ red zone приложения; add rsp,0x80
     * после (DR балансирует rsp сам, сдвиг чисто смещает базу).      */
    /* B-2014 ПОПЫТКА red-zone-сдвига (lea -0x80(%rsp),rsp вокруг call)
     * ОТМЕНЕНА: хвост стаба (7b) «возврат на app-стек» держит в rsp
     * вершину scratch-стека и ссылается на него относительно — сдвиг
     * ломает 16/33 батарей. Красная зона здесь НЕ защищается сдвигом;
     * гипотеза о её роли в bun-require не подтвердилась фиксацией
     * (краш сместился, но не исчез). Вопрос — в B-2015.             */
    dr_insert_clean_call(dc, bb, i, (void *)emu_generic, fp, 1,
                         OPND_CREATE_INTPTR((ptr_int_t)d));

    /* (5*) §7.3: гард «тихого пропуска». Исходная инструкция УДАЛЕНА
     * (8), поэтому emu_generic == 0 (необработанный опкод добрался до
     * стаба) = операция молча пропущена: последующие чтения видят
     * мусор shadow → паника аллокатора (класс -nobcast). ud2 здесь
     * нельзя: SIGILL из середины стаба уйдёт в trap-fallback с
     * КОНТЕКСТОМ НОСИТЕЛЯ (r10 = база BUF, хвост стаба пропущен).
     * Громкая остановка с именем опкода — та же философия, что у
     * ловушки UNENCODABLE в pass_ins.                              */
    {
        /* rax после clean call с fp=1 затёрт DR-эпилогом — читаем
         * BUF_RC (кладёт обёртка emu_generic); r10 ещё держит базу
         * BUF — (5b)/(6)/(7) идут ниже.                                 */
        instr_t *s_mov = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_RC, OPSZ_8));
        instr_t *s_tst = INSTR_CREATE_test(
            dc, opnd_create_reg(DR_REG_RAX), opnd_create_reg(DR_REG_RAX));
        instr_t *s_lab = INSTR_CREATE_label(dc);
        instr_t *s_jnz = INSTR_CREATE_jcc(dc, OP_jnz, opnd_create_instr(s_lab));
        instr_set_translation(s_mov, instr_get_app_pc(i));
        instr_set_translation(s_tst, instr_get_app_pc(i));
        instr_set_translation(s_jnz, instr_get_app_pc(i));
        instrlist_preinsert(bb, i, s_mov);
        instrlist_preinsert(bb, i, s_tst);
        instrlist_preinsert(bb, i, s_jnz);
        dr_insert_clean_call(dc, bb, i, (void *)emu_stub_unsupported,
                             false, 1, OPND_CREATE_INTPTR((ptr_int_t)d));
        instrlist_preinsert(bb, i, s_lab);
    }

    /* (5b) §7.3: прокси фолта. Если эмуляция столкнулась с недоступной
     * памятью приложения (emu_safe_read/write: BUF_TG != 0), здесь, в
     * кэше, исполняется НАСТОЯЩИЙ 1-байтовый доступ по записанному
     * адресу: фолт в кэше DR транслирует в app-PC эмулируемой
     * инструкции — SEH (Windows) и сигналы (Linux) приложения работают
     * как при нативном исполнении. BUF_TG: bit0 = фолт, bit1 = запись
     * (r10-относительные слоты: gs-база в контексте доставки сигнала
     * ненадёжен). Фолт-путь восстанавливает КОНТЕКСТ ПРИЛОЖЕНИЯ
     * (флаги/rax/r10 из BUF_FLAGS/BUF_GPR(0)/BUF_R10 — их кладёт
     * обёртка emu_generic) ДО настоящего доступа; доставленный
     * контекст отличается от нативного одним регистром-throwaway
     * (r10 = адрес фолта / прочитанный байт). Прокси идёт ПОСЛЕ
     * clean call — вне DR-umbrella — rax кlobber'ится осознанно
     * (восстанавливается в (6)), флаги чинит безусловный (6)-рестор. */
    if (!g_no_proxy)
    {
        instr_t *l_end = INSTR_CREATE_label(dc);
        instr_t *l_read = INSTR_CREATE_label(dc);
        instr_t *p_movc = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_TG, OPSZ_8));
        instr_t *p_t0 = INSTR_CREATE_test(
            dc, opnd_create_reg(DR_REG_RAX), opnd_create_reg(DR_REG_RAX));
        instr_t *p_jz = INSTR_CREATE_jcc(dc, OP_jz, opnd_create_instr(l_end));
        instr_t *p_shr = INSTR_CREATE_shr(
            dc, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INT8(1));
        instr_t *p_jr = INSTR_CREATE_jcc(dc, OP_jz, opnd_create_instr(l_read));
        /* WRITE-путь. B-2011: стековый транспорт (push адреса / pushf-popf)
         * заменён на staging BUF_STG. Раскладка как в (6): sahf читает AH,
         * а BUF_FLAGS — сырой EFLAGS — обязателен shl $8 (AH := байт 0);
         * OF — add-трюк последним флаг-пишущим опом; перезагрузка AH-образа
         * из staging (mov не пишет флаги); sahf. DF/IF/TF — транзитивно.
         * Адрес фолта — из BUF_FAULT напрямую. БАГ B-2010-отката: мёртвая
         * загрузка BUF_R10 в r10 — удалена вместе со стековым транспортом. */
        instr_t *p_wfa = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_wshl8 = INSTR_CREATE_shl(
            dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(8));
        instr_t *p_wst = INSTR_CREATE_mov_st(
            dc, buf_opnd(BUF_STG, OPSZ_8), opnd_create_reg(DR_REG_RAX));
        instr_t *p_wld = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_wshl = INSTR_CREATE_shl(
            dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(20));
        instr_t *p_wadd = INSTR_CREATE_add(
            dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EAX));
        instr_t *p_wrld = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_STG, OPSZ_8));
        instr_t *p_wsahf = INSTR_CREATE_sahf(dc);
        instr_t *p_wrax = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_GPR(0), OPSZ_8));
        instr_t *p_wr10 = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_R10), buf_opnd(BUF_FAULT, OPSZ_8));
        instr_t *p_wacc = INSTR_CREATE_mov_st(
            dc, opnd_create_base_disp(DR_REG_R10, DR_REG_NULL, 0, 0, OPSZ_1),
            OPND_CREATE_INT8(0));
        instr_t *p_wjmp = INSTR_CREATE_jmp_short(dc, opnd_create_instr(l_end));
        /* READ-путь. БАГ (тянется с отбрасывания alt-стека B-2010): доступ
         * шёл по [rax] ПОСЛЕ перезагрузки app-rax (p_rrax) — т.е. по адресу
         * ПРИЛОЖЕНИЯ, а не фолта; теперь — по настоящему адресу фолта
         * (rax = BUF_FAULT), r10 = прочитанный байт (throwaway, см. шапку
         * (5b)). app-rax осознанно кlobber'ится — восстанавливается в (6). */
        instr_t *p_rfa = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_rshl8 = INSTR_CREATE_shl(
            dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(8));
        instr_t *p_rst = INSTR_CREATE_mov_st(
            dc, buf_opnd(BUF_STG, OPSZ_8), opnd_create_reg(DR_REG_RAX));
        instr_t *p_rld = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *p_rshl = INSTR_CREATE_shl(
            dc, opnd_create_reg(DR_REG_EAX), OPND_CREATE_INT8(20));
        instr_t *p_radd = INSTR_CREATE_add(
            dc, opnd_create_reg(DR_REG_EAX), opnd_create_reg(DR_REG_EAX));
        instr_t *p_rrld = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_STG, OPSZ_8));
        instr_t *p_rsahf = INSTR_CREATE_sahf(dc);
        instr_t *p_rfa2 = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX), buf_opnd(BUF_FAULT, OPSZ_8));
        instr_t *p_rr10 = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_R10), buf_opnd(BUF_FAULT, OPSZ_8));
        instr_t *p_racc = INSTR_CREATE_movzx(
            dc, opnd_create_reg(DR_REG_R10D), opnd_create_base_disp(
                DR_REG_RAX, DR_REG_NULL, 0, 0, OPSZ_1));
        instr_t *proxy_ins[] = { p_movc, p_t0, p_jz, p_shr, p_jr,
                                 p_wfa, p_wshl8, p_wst, p_wld, p_wshl,
                                 p_wadd, p_wrld, p_wsahf, p_wrax, p_wr10,
                                 p_wacc, p_wjmp,
                                 p_rfa, p_rshl8, p_rst, p_rld, p_rshl,
                                 p_radd, p_rrld, p_rsahf, p_rfa2, p_rr10,
                                 p_racc };
        for (size_t pi = 0; pi < sizeof(proxy_ins)/sizeof(proxy_ins[0]); pi++)
            instr_set_translation(proxy_ins[pi], instr_get_app_pc(i));
        instrlist_preinsert(bb, i, p_movc);
        instrlist_preinsert(bb, i, p_t0);
        instrlist_preinsert(bb, i, p_jz);
        instrlist_preinsert(bb, i, p_shr);
        instrlist_preinsert(bb, i, p_jr);
        instrlist_preinsert(bb, i, p_wfa);
        instrlist_preinsert(bb, i, p_wshl8);
        instrlist_preinsert(bb, i, p_wst);
        instrlist_preinsert(bb, i, p_wld);
        instrlist_preinsert(bb, i, p_wshl);
        instrlist_preinsert(bb, i, p_wadd);
        instrlist_preinsert(bb, i, p_wrld);
        instrlist_preinsert(bb, i, p_wsahf);
        instrlist_preinsert(bb, i, p_wrax);
        instrlist_preinsert(bb, i, p_wr10);
        instrlist_preinsert(bb, i, p_wacc);
        instrlist_preinsert(bb, i, p_wjmp);
        instrlist_preinsert(bb, i, l_read);
        instrlist_preinsert(bb, i, p_rfa);
        instrlist_preinsert(bb, i, p_rshl8);
        instrlist_preinsert(bb, i, p_rst);
        instrlist_preinsert(bb, i, p_rld);
        instrlist_preinsert(bb, i, p_rshl);
        instrlist_preinsert(bb, i, p_radd);
        instrlist_preinsert(bb, i, p_rrld);
        instrlist_preinsert(bb, i, p_rsahf);
        instrlist_preinsert(bb, i, p_rfa2);
        instrlist_preinsert(bb, i, p_rr10);
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
        instr_set_translation(ni, instr_get_app_pc(i));
        instrlist_postinsert(bb, cur, ni);
        cur = ni;
    }
    /* GPR-результаты, кроме rax (flags-танец) и r10 (носитель) */
    for (int k = 0; k < 16; k++) {
        if (!(gdst & (1u << k))) continue;
        if (k == gpr_idx(DR_REG_RAX) || k == gpr_idx(DR_REG_R10)) continue;
        instr_t *ni = INSTR_CREATE_mov_ld(
            dc, opnd_create_reg(DR_REG_RAX + k), buf_opnd(BUF_GPR(k), OPSZ_8));
        instr_set_translation(ni, instr_get_app_pc(i));
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
     * а обёртка emu_generic кладёт app-флаги в BUF_FLAGS на каждом вызове.
     * B-2011: СТЕКЛЕСНЫЙ рестор вместо push rax/popf (красная зона).
     * РАСКЛАДКА КРИТИЧНА (баг v1 трюка, пойман t_lztz: 28/44 строк):
     * sahf читает AH = биты 8..15 rax, а BUF_FLAGS — СЫРОЙ образ EFLAGS
     * (арифметический байт в AL, битах 0..7) — без сдвига CF брался из
     * бита 8 (TF!), PF из бита 10 (DF) — вся пятёрка со сдвигом 8.
     * Поэтому: (1) AH := байт 0 образа через shl $8; (2) OF — add-трюк
     * ПОСЛЕДНИМ флаг-пишущим опом (shl $20/add, shl $8 его бы убил);
     * (3) перезагрузка AH-образа из staging (mov не пишет флаги — OF жив);
     * (4) sahf возвращает CF/PF/AF/ZF/SF. Итог: все 6 арифметических
     * битов = образ; DF/IF/TF — транзитивно нетронуты (см. (2)). */
    if (1) {
        instr_t *n1 = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                          buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *n1b = INSTR_CREATE_shl(dc, opnd_create_reg(DR_REG_EAX),
                                        OPND_CREATE_INT8(8));
        instr_t *n1c = INSTR_CREATE_mov_st(dc, buf_opnd(BUF_STG, OPSZ_8),
                                           opnd_create_reg(DR_REG_RAX));
        instr_t *n1d = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                           buf_opnd(BUF_FLAGS, OPSZ_8));
        instr_t *n1e = INSTR_CREATE_shl(dc, opnd_create_reg(DR_REG_EAX),
                                        OPND_CREATE_INT8(20));
        instr_t *n1f = INSTR_CREATE_add(dc, opnd_create_reg(DR_REG_EAX),
                                        opnd_create_reg(DR_REG_EAX));
        instr_t *n1g = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                           buf_opnd(BUF_STG, OPSZ_8));
        instr_t *n3 = INSTR_CREATE_sahf(dc);
        instr_set_translation(n1, instr_get_app_pc(i));
        instr_set_translation(n1b, instr_get_app_pc(i));
        instr_set_translation(n1c, instr_get_app_pc(i));
        instr_set_translation(n1d, instr_get_app_pc(i));
        instr_set_translation(n1e, instr_get_app_pc(i));
        instr_set_translation(n1f, instr_get_app_pc(i));
        instr_set_translation(n1g, instr_get_app_pc(i));
        instr_set_translation(n3, instr_get_app_pc(i));
        instrlist_postinsert(bb, cur, n1); cur = n1;
        instrlist_postinsert(bb, cur, n1b); cur = n1b;
        instrlist_postinsert(bb, cur, n1c); cur = n1c;
        instrlist_postinsert(bb, cur, n1d); cur = n1d;
        instrlist_postinsert(bb, cur, n1e); cur = n1e;
        instrlist_postinsert(bb, cur, n1f); cur = n1f;
        instrlist_postinsert(bb, cur, n1g); cur = n1g;
        instrlist_postinsert(bb, cur, n3); cur = n3;
    }
    /* rax — всегда последним: рестор флагов использует его как носитель;
       спил гарантирует исходное значение (rax — dst или источник) */
    {
        instr_t *ni = INSTR_CREATE_mov_ld(dc, opnd_create_reg(DR_REG_RAX),
                                          buf_opnd(BUF_GPR(0), OPSZ_8));
        instr_set_translation(ni, instr_get_app_pc(i));
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

    /* (7b) B-2010: возврат на app-стек. rsp держит вершину scratch-стека;
     * app rsp — в RAW_TRSP; rax к этому моменту уже восстановлен из BUF —
     * танец через RAW_TRAX возвращает и его. Порядок важен: (6) уже
     * восстановил ВСЕ результаты, включая rsp-dst (их у нас нет —
     * alt_switch=0 для таких опов). */
    /* B-2009: пометить note'ом инструкции, вставленные МИМО pass_ins
     * (гард (5*), прокси (5b), (6)) — walk назад до первой app-инструкции
     * с ЧУЖИМ app-pc (соседние переписанные последовательности продолжают
     * walk — безвредно: restore_state ключуется по pc-якорю). Инструкции
     * DR-обёртки clean call остаются без note — их рекреацию ведёт DR. */
    {
        app_pc mypc = instr_get_app_pc(i);
        for (instr_t *w = instr_get_prev(i); w != NULL; w = instr_get_prev(w)) {
            app_pc wpc = instr_get_app_pc(w);
            if (wpc != NULL && wpc != mypc)
                break;                       /* чужая app-инструкция */
            if (!instr_is_label(w) && instr_get_note(w) == NULL)
                instr_set_note(w, PASS_META_NOTE);
        }
    }

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
    /* B-2009: счётчик переписываний для pass_bb_event — блок с
     * переписанными инструкциями обязан вернуть DR_EMIT_STORE_TRANSLATIONS:
     * recreate DR пересобирает ilist прогоном bb-события с translating=true,
     * и если клиент его НЕ меняет, раскладка recreate не совпадает с
     * кэш-раскладкой → recreate_app не находит точку прерывания
     * (translate.c:1242 ASSERT_NOT_REACHED в debug; в release —
     * RECREATE_FAILURE → искажённый контекст → заморозка bun на
     * JIT-холодном патчировании, симптом B-2008). */
    if (handled)
        PASS_ATOMIC_INC32(&g_bb_rwseq);
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
    instr_set_translation(nop, instr_get_app_pc(i));
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
    /* B-2012: -skipopc — опкод исключён из перезаписи, остаётся нативным.
     * cpuid/xgetbv-хуки важнее (спуфинг обязан работать всегда).       */
    if (g_skip_opc[opc]) return;
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
        /* B-2014: вернул проверку -specialonly (флаг парсился с B-2012,
         * но проверка потерялась при рефакторах) — special-путь только
         * для одного опкода, остальное уходит в generic/trap. */
        (g_special_only < 0 || g_special_only == opc) &&
        run_rewriter(dc, bb, i, try_rewrite_special))
        return;
    /* B-2010: vinsertf128/vinserti128, отклонённые inline-путём
     * (dst==xmm15 / s1==xmm15 / s2==xmm15 — конфликт с xmm15-носителем),
     * обязаны уходить в generic-стаб: там операнды читаются из теневого
     * файла, носитель не участвует. Раньше такие случаи проваливались
     * до report_unsupported → нативный no-op на Penryn → молчаливая
     * потеря hi-половины результата (t_ins128 case 7; у bun 1.2.14 —
     * пустой console.log и undefined у JSC-builtin'ов).            */
    if ((opc == OP_vinsertf128 || opc == OP_vinserti128) &&
        g_cfg.enable_generic &&
        run_rewriter(dc, bb, i, rewrite_generic_instr))
        return;

    /* §7.3: r10 в операндах — inline-пути отклонили (носитель BUF),
     * generic спилит app-значение в BUF_GPR(10) и прочитает корректно */
    {
        int uses_r10 = 0;
        for (int k = 0; k < instr_num_dsts(i) && !uses_r10; k++)
            uses_r10 = opnd_uses_r10(instr_get_dst(i, k));
        for (int k = 0; k < instr_num_srcs(i) && !uses_r10; k++)
            uses_r10 = opnd_uses_r10(instr_get_src(i, k));
        if (uses_r10 && g_cfg.enable_generic &&
            run_rewriter(dc, bb, i, rewrite_generic_instr))
            return;
    }

    /* §7.3: broadcast-семья при отказе inline-путей (-nobcast и др.) —
     * в generic-стаб, НЕ left-native: нативное исполнение пишёт
     * старшую половину результата в РЕАЛЬНЫЙ ymm, минуя RAW_YHI-shadow
     * (инвариант клиента: hi живёт в TLS) — последующие стабы читают
     * мусор (паника аллокатора bun под -nobcast, run #96/#97).      */
    if ((opc == OP_vbroadcastss || opc == OP_vbroadcastsd ||
         opc == OP_vbroadcastf128 || opc == OP_vbroadcasti128 ||
         opc == OP_vpbroadcastb || opc == OP_vpbroadcastw ||
         opc == OP_vpbroadcastd || opc == OP_vpbroadcastq) &&
        g_cfg.enable_generic &&
        run_rewriter(dc, bb, i, rewrite_generic_instr))
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

    /* B-2009: блок с переписанными инструкциями → DR_EMIT_STORE_TRANSLATIONS.
     * recreate DR для SMC-flush/deletion-redirect пересобирает ilist
     * прогоном bb-события с translating=true; клиент его не меняет, поэтому
     * раскладка recreate (чистый app-код) не совпадает с кэш-раскладкой
     * (вставленные стабы) → recreate_app не находит точку прерывания
     * (debug: translate.c:1242; release: RECREATE_FAILURE → искажённый
     * app-контекст → заморозка bun на JIT-холодном патчировании).
     * STORE_TRANSLATIONS велит DR сохранить ilist целиком (с нашими
     * стабами и note'ами) — recreate идёт по нему точно и вызывает
     * restore_state (restore_state.c) на наших мета-инструкциях.      */
    {
        uint seq0 = g_bb_rwseq;
        instr_t *i = instrlist_first(bb);
        while (i != NULL) {
            instr_t *next = instr_get_next(i);
            rewrite_instr(dc, bb, i);
            i = next;
        }
        return (g_bb_rwseq != seq0) ? DR_EMIT_STORE_TRANSLATIONS
                                    : DR_EMIT_DEFAULT;
    }
}

void pass_rewrite_init(void)
{
    g_cfg.shadow_active = 1;   /* обнуление YHI безусловно, flush не нужен */
}
