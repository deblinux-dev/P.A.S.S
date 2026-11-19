/* t_vx41.c — дифференциальный тест VEX-алиасов SSE4.1/4.2/AES + AVX1
 * cvt/permil/maskmov (§7.4 раунд #111). Входы — runtime-массив
 * (без constant-folding); вывод побайтно сравнивается native vs pass.
 * Аргумент: stage 1..6 — сколько блоков выполнить (бисекция).
 * Сборка: gcc -O1 -mavx2 -maes t_vx41.c */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

static uint32_t seed_data[64];
static void init_data(void) {
    uint32_t s = 0x12345678;
    for (int i = 0; i < 64; i++) {
        s = s * 1103515245u + 12345u;
        seed_data[i] = s;
    }
}

static void pr128(const char *name, __m128i v) {
    uint32_t u[4]; memcpy(u, &v, 16);
    printf("%-22s %08x %08x %08x %08x\n", name, u[0], u[1], u[2], u[3]);
}
static void pr256(const char *name, __m256i v) {
    uint32_t u[8]; memcpy(u, &v, 32);
    printf("%-22s %08x %08x %08x %08x %08x %08x %08x %08x\n", name,
           u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7]);
}
static void pr2d(const char *name, void *p, size_t n) {
    uint8_t *u = p;
    printf("%-22s", name);
    for (size_t i = 0; i < n; i++) printf(" %02x", u[i]);
    printf("\n");
}
static unsigned long rdflags(void) {
    unsigned long f;
    __asm__ volatile ("pushfq\n\tpopq %0" : "=r"(f));
    return (unsigned long)f;
}

#define A256 (*(__m256i *)seed_data)
#define B256 (*(__m256i *)(seed_data + 8))
#define C256 (*(__m256i *)(seed_data + 16))
#define A128 (*(__m128i *)seed_data)
#define B128 (*(__m128i *)(seed_data + 4))
#define C128 (*(__m128i *)(seed_data + 8))
#define D128 (*(__m128i *)(seed_data + 12))

int main(int argc, char **argv) {
    int stage = (argc > 1) ? atoi(argv[1]) : 6;
    init_data();
    __m256i a = A256, b = B256, c = C256;
    __m128i a1 = A128, b1 = B128, c1 = C128, d1 = D128;

    if (stage >= 1) {
        pr256("vpcmpeqq", _mm256_cmpeq_epi64(a, b));
        pr256("vpcmpgtq", _mm256_cmpgt_epi64(a, b));
        pr256("vpmulld", _mm256_mullo_epi32(a, b));
        pr256("vpmuldq", _mm256_mul_epi32(a, b));
        pr256("vpackusdw", _mm256_packus_epi32(a, b));
        pr128("vphminposuw", _mm_minpos_epu16(a1));
        pr256("vmpsadbw-0", _mm256_mpsadbw_epu8(a, b, 0));
        pr256("vmpsadbw-45", _mm256_mpsadbw_epu8(a, b, 0x45));
        pr256("vroundps-9", _mm256_castps_si256(_mm256_round_ps(_mm256_castsi256_ps(a), 0x9)));
        pr256("vroundpd-4", _mm256_castpd_si256(_mm256_round_pd(_mm256_castsi256_pd(a), 0x4)));
        pr128("vroundss-8", _mm_castps_si128(_mm_round_ss(_mm_castsi128_ps(a1), _mm_castsi128_ps(b1), 0x8)));
        pr128("vroundsd-3", _mm_castpd_si128(_mm_round_sd(_mm_castsi128_pd(a1), _mm_castsi128_pd(b1), 0x3)));
        pr256("vdpps-ff", _mm256_castps_si256(_mm256_dp_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), 0xFF)));
        pr128("vdppd-77", _mm_castpd_si128(_mm_dp_pd(_mm_castsi128_pd(a1), _mm_castsi128_pd(b1), 0x77)));
        pr256("vblendps-a5", _mm256_castps_si256(_mm256_blend_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), 0xA5)));
        pr128("vblendpd-2", _mm_castpd_si128(_mm_blend_pd(_mm_castsi128_pd(a1), _mm_castsi128_pd(b1), 0x2)));
        pr256("vblendvps", _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), _mm256_castsi256_ps(c))));
        pr128("vpblendvb", _mm_blendv_epi8(a1, b1, c1));
        pr256("vpminsb", _mm256_min_epi8(a, b));
        pr256("vpmaxsb", _mm256_max_epi8(a, b));
        pr256("vpminuw", _mm256_min_epu16(a, b));
        pr256("vpmaxuw", _mm256_max_epu16(a, b));
        pr256("vpminsd", _mm256_min_epi32(a, b));
        pr256("vpmaxsd", _mm256_max_epi32(a, b));
        pr256("vpminud", _mm256_min_epu32(a, b));
        pr256("vpmaxud", _mm256_max_epu32(a, b));
        pr256("vpmovsxbw", _mm256_cvtepi8_epi16(a1));
        pr256("vpmovsxbd", _mm256_cvtepi8_epi32(a1));
        pr256("vpmovsxbq", _mm256_cvtepi8_epi64(a1));
        pr256("vpmovsxwd", _mm256_cvtepi16_epi32(a1));
        pr256("vpmovsxwq", _mm256_cvtepi16_epi64(a1));
        pr256("vpmovsxdq", _mm256_cvtepi32_epi64(a1));
        pr256("vpmovzxbw", _mm256_cvtepu8_epi16(a1));
        pr256("vpmovzxbd", _mm256_cvtepu8_epi32(a1));
        pr256("vpmovzxbq", _mm256_cvtepu8_epi64(a1));
        pr256("vpmovzxwd", _mm256_cvtepu16_epi32(a1));
        pr256("vpmovzxwq", _mm256_cvtepu16_epi64(a1));
        pr256("vpmovzxdq", _mm256_cvtepu32_epi64(a1));
    }

    if (stage >= 2) {
        pr128("vinsertps-27", _mm_castps_si128(_mm_insert_ps(_mm_castsi128_ps(a1), _mm_castsi128_ps(b1), 0x27)));
        {
            int ext = _mm_extract_ps(_mm_castsi128_ps(a1), 2);
            int exti = _mm_extract_epi32(a1, 3);
            /* флаги НЕ печатаем: vextractps их не пишет, а фон вокруг
             * (libc-мусор, ASLR) недетерминирован — ловили ложный дифф */
            printf("%-22s %08x %08x\n", "vextractps", (uint32_t)ext,
                   (uint32_t)exti);
        }
        {
            int pt = _mm_testz_si128(a1, b1);
            int pt2 = _mm_testc_si128(a1, b1);
            int pt3 = _mm_testnzc_si128(a1, b1);
            printf("%-22s %d %d %d f=%016lx\n", "vptest", pt, pt2, pt3, rdflags());
        }
    }

    if (stage >= 3) {
        __m128i key = _mm_set1_epi32((int)seed_data[20]);
        pr128("vaesenc", _mm_aesenc_si128(a1, key));
        pr128("vaesdec", _mm_aesdec_si128(a1, key));
        pr128("vaesenclast", _mm_aesenclast_si128(a1, key));
        pr128("vaesdeclast", _mm_aesdeclast_si128(a1, key));
        pr128("vaesimc", _mm_aesimc_si128(a1));
        pr128("vaeskeygen", _mm_aeskeygenassist_si128(a1, 0x3B));
    }

    if (stage >= 4) {
        pr256("vcvtdq2pd-ymm", _mm256_castpd_si256(_mm256_cvtepi32_pd(a1)));
        pr128("vcvtdq2pd-xmm", _mm_castpd_si128(_mm_cvtepi32_pd(a1)));
        pr128("vcvtpd2dq-ymm", _mm256_cvtpd_epi32(_mm256_castsi256_pd(a)));
        pr128("vcvttpd2dq-ymm", _mm256_cvttpd_epi32(_mm256_castsi256_pd(a)));
        pr128("vcvtpd2dq-xmm", _mm_cvtpd_epi32(_mm_castsi128_pd(a1)));
        pr256("vpermilps-imm", _mm256_castps_si256(_mm256_permute_ps(_mm256_castsi256_ps(a), 0x1B)));
        pr128("vpermilps-imm128", _mm_castps_si128(_mm_permute_ps(_mm_castsi128_ps(a1), 0x1B)));
        pr256("vpermilps-reg", _mm256_castps_si256(_mm256_permutevar_ps(_mm256_castsi256_ps(a), b)));
        pr128("vpermilps-reg128", _mm_castps_si128(_mm_permutevar_ps(_mm_castsi128_ps(a1), b1)));
        pr256("vpermilpd-imm", _mm256_castpd_si256(_mm256_permute_pd(_mm256_castsi256_pd(a), 0x5)));
        pr128("vpermilpd-imm128", _mm_castpd_si128(_mm_permute_pd(_mm_castsi128_pd(a1), 0x5)));
        pr256("vpermilpd-reg", _mm256_castpd_si256(_mm256_permutevar_pd(_mm256_castsi256_pd(a), b)));
        pr128("vpermilpd-reg128", _mm_castpd_si128(_mm_permutevar_pd(_mm_castsi128_pd(a1), b1)));
    }

    if (stage >= 5) {
        __m256i mask = _mm256_setr_epi32(-1, 0, -1, (int)0x80000000, 0, -1, 0x7fffffff, -1);
        int buf[16];
        for (int i = 0; i < 16; i++) buf[i] = (int)(0xC0DE0000u + (uint32_t)i);
        pr256("vpmaskmovd-load", _mm256_maskload_epi32(buf + 2, mask));
        _mm256_maskstore_epi32(buf + 4, mask, a);
        pr2d("vpmaskmovd-store", buf, 64);
        float fbuf[16];
        for (int i = 0; i < 16; i++) fbuf[i] = (float)(0xC1DE0000 + i);
        pr256("vmaskmovps-load", _mm256_castps_si256(_mm256_maskload_ps(fbuf + 1, mask)));
        _mm256_maskstore_ps(fbuf + 3, mask, _mm256_castsi256_ps(b));
        pr2d("vmaskmovps-store", fbuf, 64);
        double dbuf[8];
        for (int i = 0; i < 8; i++) dbuf[i] = -1.5 - (double)i;
        pr256("vmaskmovpd-load", _mm256_castpd_si256(_mm256_maskload_pd(dbuf + 1, mask)));
        _mm256_maskstore_pd(dbuf + 2, mask, _mm256_castsi256_pd(c));
        pr2d("vmaskmovpd-store", dbuf, 64);
        {
            __m128i m1 = _mm_setr_epi32(-1, 0, (int)0x80000000, -1);
            int buf2[8];
            for (int i = 0; i < 8; i++) buf2[i] = (int)(0xBEEF0000u + (uint32_t)i);
            pr128("vpmaskmovd-l128", _mm_maskload_epi32(buf2 + 1, m1));
            _mm_maskstore_epi32(buf2 + 2, m1, c1);
            pr2d("vpmaskmovd-s128", buf2, 32);
        }
    }

    if (stage >= 6) {
        pr128("pcmpeqq", _mm_cmpeq_epi64(a1, b1));
        pr128("pcmpgtq", _mm_cmpgt_epi64(a1, b1));
        pr128("pmulld", _mm_mullo_epi32(a1, b1));
        pr128("pmuldq", _mm_mul_epi32(a1, b1));
        pr128("packusdw", _mm_packus_epi32(a1, b1));
        pr128("mpsadbw-0", _mm_mpsadbw_epu8(a1, b1, 0));
        pr128("roundps-9", _mm_castps_si128(_mm_round_ps(_mm_castsi128_ps(a1), 0x9)));
        pr128("dpps-ff", _mm_castps_si128(_mm_dp_ps(_mm_castsi128_ps(a1), _mm_castsi128_ps(b1), 0xFF)));
        pr128("pminsb", _mm_min_epi8(a1, b1));
        pr128("pmaxsd", _mm_max_epi32(a1, b1));
        pr128("pmovsxbw", _mm_cvtepi8_epi16(a1));
        pr128("pmovzxwq", _mm_cvtepu16_epi64(a1));
        pr128("insertps-27", _mm_castps_si128(_mm_insert_ps(_mm_castsi128_ps(a1), _mm_castsi128_ps(b1), 0x27)));
        {
            __m128i key = _mm_set1_epi32((int)seed_data[20]);
            pr128("aesenc", _mm_aesenc_si128(a1, key));
            pr128("aesimc", _mm_aesimc_si128(a1));
        }
    }
    (void)d1;
    return 0;
}
