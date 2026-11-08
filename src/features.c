/* P.A.S.S. — features.c
 * Сканирование хостового CPU, профили «старых» процессоров
 * и таблица соответствия «опкод → требуемая фича».            */
#include "pass.h"


pass_config_t g_cfg;
int      g_tls_idx_raw = -1;
int      g_tls_idx_buf = -1;
reg_id_t g_tls_seg     = DR_REG_NULL;
uint     g_tls_raw_off = 0;

uint32_t g_stat_static[OPC_MAX];
uint32_t g_stat_generic[OPC_MAX];
uint32_t g_stat_unsupported[OPC_MAX];
uint32_t g_stat_trap;
uint32_t g_stat_bbs;
uint64_t g_dyn_generic;
uint64_t g_dyn_trap;

static const char *feat_names[F_COUNT] = {
    "sse3","ssse3","sse4.1","sse4.2","popcnt","aes","pclmulqdq",
    "avx","avx2","fma","bmi1","bmi2","movbe","lzcnt","f16c",
    "xsave","osxsave","prefetchw"
};

const char *feat_name(int f)
{
    return (f >= 0 && f < F_COUNT) ? feat_names[f] : "?";
}

featset_t profile_from_name(const char *name)
{
    /* Профили «целевых» машин. Всё, что не перечислено — нет. */
    featset_t p = 0;
    if (name == NULL) name = "core2";
    if (strcmp(name, "native") == 0) {
        return g_cfg.host;                  /* passthrough без эмуляции */
    } else if (strcmp(name, "core2") == 0) {
        /* Merom/Penryn: SSE3, SSSE3. Нет SSE4.x/POPCNT/AVX. */
        p = FEAT(F_SSE3) | FEAT(F_SSSE3);
    } else if (strcmp(name, "nehalem") == 0) {
        p = FEAT(F_SSE3) | FEAT(F_SSSE3) | FEAT(F_SSE41) |
            FEAT(F_SSE42) | FEAT(F_POPCNT) | FEAT(F_LZCNT);
    } else if (strcmp(name, "westmere") == 0) {
        p = FEAT(F_SSE3) | FEAT(F_SSSE3) | FEAT(F_SSE41) |
            FEAT(F_SSE42) | FEAT(F_POPCNT) | FEAT(F_LZCNT) |
            FEAT(F_AES) | FEAT(F_PCLMUL);
    } else if (strcmp(name, "sandybridge") == 0) {
        p = FEAT(F_SSE3) | FEAT(F_SSSE3) | FEAT(F_SSE41) |
            FEAT(F_SSE42) | FEAT(F_POPCNT) | FEAT(F_LZCNT) |
            FEAT(F_AES) | FEAT(F_PCLMUL) | FEAT(F_AVX) | FEAT(F_XSAVE) |
            FEAT(F_OSXSAVE);
    } else if (strcmp(name, "haswell") == 0) {
        p = FEAT(F_SSE3) | FEAT(F_SSSE3) | FEAT(F_SSE41) |
            FEAT(F_SSE42) | FEAT(F_POPCNT) | FEAT(F_LZCNT) |
            FEAT(F_AES) | FEAT(F_PCLMUL) | FEAT(F_AVX) | FEAT(F_XSAVE) |
            FEAT(F_OSXSAVE) | FEAT(F_AVX2) | FEAT(F_FMA) | FEAT(F_BMI1) |
            FEAT(F_BMI2) | FEAT(F_MOVBE);
    } else {
        /* неизвестное имя — максимум Core 2 */
        p = FEAT(F_SSE3) | FEAT(F_SSSE3);
    }
    return p;
}

void features_init(void)
{
    uint32_t a, b, c, d;
    featset_t host = 0;

    /* leaf 1 */
    if (pass_cpuid(1, &a, &b, &c, &d)) {
        if (c & (1u << 0))  host |= FEAT(F_SSE3);
        if (c & (1u << 9))  host |= FEAT(F_SSSE3);
        if (c & (1u << 19)) host |= FEAT(F_SSE41);
        if (c & (1u << 20)) host |= FEAT(F_SSE42);
        if (c & (1u << 22)) host |= FEAT(F_MOVBE);
        if (c & (1u << 23)) host |= FEAT(F_POPCNT);
        if (c & (1u << 25)) host |= FEAT(F_AES);
        if (c & (1u << 26)) host |= FEAT(F_XSAVE);
        if (c & (1u << 27)) host |= FEAT(F_OSXSAVE);
        if (c & (1u << 28)) host |= FEAT(F_AVX);
        if (c & (1u << 29)) host |= FEAT(F_F16C);
        if (c & (1u << 12)) host |= FEAT(F_FMA);
        if (c & (1u << 1))  host |= FEAT(F_PCLMUL);
        if (d & (1u << 25)) host |= FEAT(F_SSE3);   /* (SSE) база */
    }
    /* leaf 7 subleaf 0 */
    if (pass_cpuid_count(7, 0, &a, &b, &c, &d)) {
        if (b & (1u << 3))  host |= FEAT(F_BMI1);
        if (b & (1u << 5))  host |= FEAT(F_AVX2);
        if (b & (1u << 8))  host |= FEAT(F_BMI2);
    }
    /* leaf 0x80000001: LZCNT (ecx.5), PREFETCHW (ecx.8) */
    if (pass_cpuid(0x80000001, &a, &b, &c, &d)) {
        if (c & (1u << 5))  host |= FEAT(F_LZCNT);
        if (c & (1u << 8))  host |= FEAT(F_PREFW);
    }
    g_cfg.host = host;
}

/* ─────────── опкод → фича (для решения passthrough/эмуляция) ─────────── */

#define CASES(opcs) case opcs

int opc_supported(int opc, opnd_size_t dsz)
{
    /* AVX-512/маски не эмулируем никогда */
    if (dsz == OPSZ_64 || dsz == OPSZ_32_of_64)
        return 0;

    switch (opc) {
    /* ── SSE3 ── */
    case OP_addsubpd: case OP_addsubps: case OP_haddpd: case OP_haddps:
    case OP_hsubpd: case OP_hsubps: case OP_lddqu: case OP_monitor:
    case OP_mwait: case OP_movddup: case OP_movshdup: case OP_movsldup:
    case OP_fisttp:
        return has(g_cfg.profile, F_SSE3);
    /* ── SSSE3 ── */
    case OP_pshufb: case OP_phaddw: case OP_phaddd: case OP_phaddsw:
    case OP_phsubw: case OP_phsubd: case OP_phsubsw: case OP_pmaddubsw:
    case OP_pmulhrsw: case OP_psignb: case OP_psignw: case OP_psignd:
    case OP_pabsb: case OP_pabsw: case OP_pabsd: case OP_palignr:
        return has(g_cfg.profile, F_SSSE3);
    /* ── SSE4.1 ── */
    case OP_blendpd: case OP_blendps: case OP_blendvpd: case OP_blendvps:
    case OP_dppd: case OP_dpps: case OP_extractps: case OP_insertps:
    case OP_movntdqa: case OP_mpsadbw: case OP_packusdw: case OP_pblendvb:
    case OP_pcmpeqq: case OP_pextrb: case OP_pextrd:
    case OP_pextrw: case OP_phminposuw: case OP_pinsrb: case OP_pinsrd:
    case OP_pmaxsb: case OP_pmaxsd: case OP_pmaxud:
    case OP_pmaxuw: case OP_pminsb: case OP_pminsd: case OP_pminud:
    case OP_pminuw: case OP_pmovsxbd: case OP_pmovsxbq: case OP_pmovsxbw:
    case OP_pmovsxdq: case OP_pmovsxwd: case OP_pmovsxwq:
    case OP_pmovzxbd: case OP_pmovzxbq: case OP_pmovzxbw:
    case OP_pmovzxdq: case OP_pmovzxwd: case OP_pmovzxwq:
    case OP_pmulld: case OP_pmuldq: case OP_ptest:
    case OP_roundpd: case OP_roundps: case OP_roundsd: case OP_roundss:
    case OP_pblendw:
        return has(g_cfg.profile, F_SSE41);
    /* ── SSE4.2 ── */
    case OP_crc32: case OP_pcmpestri: case OP_pcmpestrm:
    case OP_pcmpgtq: case OP_pcmpistri: case OP_pcmpistrm:
        return has(g_cfg.profile, F_SSE42);
    /* ── скалярные новые ── */
    case OP_popcnt: return has(g_cfg.profile, F_POPCNT);
    case OP_lzcnt:  return has(g_cfg.profile, F_LZCNT);
    case OP_aesdec: case OP_aesdeclast: case OP_aesenc: case OP_aesenclast:
    case OP_aesimc: case OP_aeskeygenassist:
        return has(g_cfg.profile, F_AES);
    case OP_pclmulqdq:
        return has(g_cfg.profile, F_PCLMUL);
    case OP_movbe:
        return has(g_cfg.profile, F_MOVBE);
    case OP_rdrand: case OP_rdseed:
        return 0; /* энтропию не эмулируем — trap сообщит */
    /* ── BMI1 ── */
    case OP_andn: case OP_bextr: case OP_blsi: case OP_blsmsk:
    case OP_blsr: case OP_tzcnt: case OP_tzmsk:
        return has(g_cfg.profile, F_BMI1);
    /* ── BMI2 ── */
    case OP_bzhi: case OP_mulx: case OP_pdep: case OP_pext: case OP_rorx:
    case OP_sarx: case OP_shlx: case OP_shrx:
        return has(g_cfg.profile, F_BMI2);
    /* ── AVX2-специфика ── */
    case OP_vbroadcasti128: case OP_vpbroadcastb: case OP_vpbroadcastw:
    case OP_vpbroadcastd: case OP_vpbroadcastq: case OP_vperm2i128:
    case OP_vpermd: case OP_vpermq: case OP_vpblendd:
    case OP_vpmaskmovd: case OP_vpmaskmovq:
    case OP_vpsllvd: case OP_vpsllvq: case OP_vpsrlvd: case OP_vpsrlvq:
    case OP_vpsravd: case OP_vpsravq:
    case OP_vpgatherdd: case OP_vpgatherdq: case OP_vpgatherqd:
    case OP_vpgatherqq: case OP_vgatherdps: case OP_vgatherdpd:
    case OP_vgatherqps: case OP_vgatherqpd:
    case OP_vpsllvw: case OP_vpsrlvw: case OP_vpsravw:
        return has(g_cfg.profile, F_AVX2);
    /* ── FMA ── */
    case OP_vfmadd132ps: case OP_vfmadd132pd: case OP_vfmadd132ss:
    case OP_vfmadd132sd: case OP_vfmadd213ps: case OP_vfmadd213pd:
    case OP_vfmadd213ss: case OP_vfmadd213sd: case OP_vfmadd231ps:
    case OP_vfmadd231pd: case OP_vfmadd231ss: case OP_vfmadd231sd:
    case OP_vfmsub132ps: case OP_vfmsub132pd: case OP_vfmsub132ss:
    case OP_vfmsub132sd: case OP_vfmsub213ps: case OP_vfmsub213pd:
    case OP_vfmsub213ss: case OP_vfmsub213sd: case OP_vfmsub231ps:
    case OP_vfmsub231pd: case OP_vfmsub231ss: case OP_vfmsub231sd:
    case OP_vfnmadd132ps: case OP_vfnmadd132pd: case OP_vfnmadd132ss:
    case OP_vfnmadd132sd: case OP_vfnmadd213ps: case OP_vfnmadd213pd:
    case OP_vfnmadd213ss: case OP_vfnmadd213sd: case OP_vfnmadd231ps:
    case OP_vfnmadd231pd: case OP_vfnmadd231ss: case OP_vfnmadd231sd:
    case OP_vfnmsub132ps: case OP_vfnmsub132pd: case OP_vfnmsub132ss:
    case OP_vfnmsub132sd: case OP_vfnmsub213ps: case OP_vfnmsub213pd:
    case OP_vfnmsub213ss: case OP_vfnmsub213sd: case OP_vfnmsub231ps:
    case OP_vfnmsub231pd: case OP_vfnmsub231ss: case OP_vfnmsub231sd:
    case OP_vfmaddsub132ps: case OP_vfmaddsub132pd:
    case OP_vfmaddsub213ps: case OP_vfmaddsub213pd:
    case OP_vfmaddsub231ps: case OP_vfmaddsub231pd:
    case OP_vfmsubadd132ps: case OP_vfmsubadd132pd:
    case OP_vfmsubadd213ps: case OP_vfmsubadd213pd:
    case OP_vfmsubadd231ps: case OP_vfmsubadd231pd:
        return has(g_cfg.profile, F_FMA);
    case OP_vzeroupper: case OP_vzeroall:
        return has(g_cfg.profile, F_AVX);
    /* ── AVX1 xsave-семейство ── */
    case OP_xsave64: case OP_xrstor64: case OP_xsetbv: case OP_xsaveopt64:
    case OP_xsavec64: case OP_xsaves64: case OP_xrstors64:
    case OP_xsave32: case OP_xrstor32: case OP_xsaveopt32: case OP_xsavec32:
        return has(g_cfg.profile, F_XSAVE);
    /* ── прочие VEX → спросить таблицы ──
     * ВАЖНО: неизвестное НЕ должно passthrough-ить молча: всё, что мы
     * умеем эмулировать, обязано возвращать 0 (иначе на целевом CPU —
     * SIGILL, который trap посчитает «легальной» инструкцией, а
     * 256-битные store испортят память). */
    default: {
        const vex_map_ent_t *e = vex_map_lookup(opc);
        if (e != NULL)
            return has(g_cfg.profile, e->req);
        if (special_req_ok(opc))
            return 0;   /* есть обработчик → эмуляция */
        if (pass_is_generic_opc(opc))
            return 0;   /* generic clean-call → эмуляция */
        /* действительно неизвестное — passthrough (поймает trap) */
        return 1;
    }
    }
}
