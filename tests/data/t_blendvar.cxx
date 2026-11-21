// P.A.S.S. diff-test: варианты аллокации/форм vpblendvb (VEX.256) в
// haswell-ядре simdutf + margin-8 структура (bun 1.2.14: 2 векторных
// итерации на строке из 42 юнитов).
//
// Варианты:
//   A: dst == W (регистр `in`)  — вероятная аллокация MSVC
//   B: dst == V (регистр t4)    — вероятная аллокация gcc
//   C: dst == mask
//   D: W = память (YMMWORD PTR [in])
//   K: полное ядро с safety_margin=8 (42 юнита → 2 векторных итерации)
//
// exit 0 = все варианты совпали с эталоном (нативно и под pass).
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <immintrin.h>

#include "simdutf_u16u8_tables.h"
using simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes;

static void expected_blend(const uint8_t* V, const uint8_t* W,
                           const uint8_t* M, uint8_t* out) {
    for (int i = 0; i < 32; i++)
        out[i] = (M[i] & 0x80) ? W[i] : V[i];
}

static int check(const char* name, const uint8_t* got, const uint8_t* exp) {
    if (memcmp(got, exp, 32) != 0) {
        printf("FAIL %s\n  got:", name);
        for (int i = 0; i < 32; i++) printf(" %02x", got[i]);
        printf("\n  exp:");
        for (int i = 0; i < 32; i++) printf(" %02x", exp[i]);
        printf("\n");
        return 1;
    }
    return 0;
}

int main() {
    // 16 слов: смесь ASCII-пробелов и кириллицы (как в строке пользователя)
    static const uint16_t words[16] = {
        0x041F, 0x0420, 0x041E, 0x0412, 0x0415, 0x0420, 0x041A, 0x0410,
        0x0020, 0x0420, 0x0410, 0x0417, 0x041D, 0x041E, 0x0419, 0x0020,
    };
    alignas(32) uint8_t W[32], V[32], M[32], got[32], exp[32];
    memcpy(W, words, 32);                                   // сырой ввод
    for (int i = 0; i < 16; i++) {                          // t4: кодировка
        uint16_t w = words[i];
        uint16_t enc = (uint16_t)(((w << 2) & 0x1F00) | (w & 0x3F) | 0xC080);
        if ((w & 0xFF80) == 0) enc = w;                     // ASCII: raw
        memcpy(V + i * 2, &enc, 2);
    }
    for (int i = 0; i < 16; i++) {                          // маска
        uint16_t m = ((words[i] & 0xFF80) == 0) ? 0xFFFF : 0x0000;
        memcpy(M + i * 2, &m, 2);
    }
    expected_blend(V, W, M, exp);

    int fails = 0;

    {   // A: dst == W
        __asm__ volatile(
            ".intel_syntax noprefix \n"
            "vmovdqu ymm0, [%0] \n"
            "vmovdqu ymm1, [%1] \n"
            "vmovdqu ymm2, [%2] \n"
            "vpxor ymm3, ymm3, ymm3 \n"
            "vpblendvb ymm0, ymm1, ymm0, ymm2 \n"
            "vmovdqu [%3], ymm0 \n"
            ".att_syntax prefix \n"
            :
            : "r"(W), "r"(V), "r"(M), "r"(got)
            : "memory");
        fails += check("A dst==W", got, exp);
    }
    {   // B: dst == V
        __asm__ volatile(
            ".intel_syntax noprefix \n"
            "vmovdqu ymm0, [%0] \n"
            "vmovdqu ymm1, [%1] \n"
            "vmovdqu ymm2, [%2] \n"
            "vpblendvb ymm1, ymm1, ymm0, ymm2 \n"
            "vmovdqu [%3], ymm1 \n"
            ".att_syntax prefix \n"
            :
            : "r"(W), "r"(V), "r"(M), "r"(got)
            : "memory");
        fails += check("B dst==V", got, exp);
    }
    {   // C: dst == mask
        __asm__ volatile(
            ".intel_syntax noprefix \n"
            "vmovdqu ymm0, [%0] \n"
            "vmovdqu ymm1, [%1] \n"
            "vmovdqu ymm2, [%2] \n"
            "vpblendvb ymm2, ymm1, ymm0, ymm2 \n"
            "vmovdqu [%3], ymm2 \n"
            ".att_syntax prefix \n"
            :
            : "r"(W), "r"(V), "r"(M), "r"(got)
            : "memory");
        fails += check("C dst==mask", got, exp);
    }
    {   // D: dst = независимый ymm4 (без алиасинга)
        __asm__ volatile(
            ".intel_syntax noprefix \n"
            "vmovdqu ymm0, [%0] \n"
            "vmovdqu ymm1, [%1] \n"
            "vmovdqu ymm2, [%2] \n"
            "vpblendvb ymm4, ymm1, ymm0, ymm2 \n"
            "vmovdqu [%3], ymm4 \n"
            ".att_syntax prefix \n"
            :
            : "r"(W), "r"(V), "r"(M), "r"(got)
            : "memory");
        fails += check("D dst=fresh", got, exp);
    }

    // K: ядро с safety_margin=8 — 42 юнита → 2 векторных итерации
    // + скалярный хвост; сверка с эталонным UTF-8
    {
        static const char16_t in42[] =
            u"ПРОВЕРКА РАЗНОЙ КИРИЛЛИЦЫ И ПРОЧЕГО ВЫВОДА";
        const size_t n = sizeof(in42) / sizeof(in42[0]) - 1;
        static char out[256];
        memset(out, 0xAA, sizeof(out));
        const char16_t* buf = in42;
        const char16_t* end = in42 + n;
        char* op = out;
        const __m256i v_0000 = _mm256_setzero_si256();
        const __m256i v_f800 = _mm256_set1_epi16((int16_t)0xf800);
        const __m256i v_c080 = _mm256_set1_epi16((int16_t)0xc080);
        const size_t safety_margin = 8;                     // ← 8, не 12
        while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
            __m256i in = _mm256_loadu_si256((__m256i*)buf);
            const __m256i v_ff80 = _mm256_set1_epi16((int16_t)0xff80);
            if (_mm256_testz_si256(in, v_ff80)) {
                const __m128i pk = _mm_packus_epi16(
                    _mm256_castsi256_si128(in), _mm256_extractf128_si256(in, 1));
                _mm_storeu_si128((__m128i*)op, pk);
                buf += 16; op += 16;
                continue;
            }
            const __m256i obm =
                _mm256_cmpeq_epi16(_mm256_and_si256(in, v_ff80), v_0000);
            const uint32_t obmask = (uint32_t)_mm256_movemask_epi8(obm);
            const __m256i otbm =
                _mm256_cmpeq_epi16(_mm256_and_si256(in, v_f800), v_0000);
            if ((uint32_t)_mm256_movemask_epi8(otbm) == 0xffffffff) {
                const __m256i v_1f00 = _mm256_set1_epi16((int16_t)0x1f00);
                const __m256i v_003f = _mm256_set1_epi16((int16_t)0x003f);
                const __m256i t0 = _mm256_slli_epi16(in, 2);
                const __m256i t1 = _mm256_and_si256(t0, v_1f00);
                const __m256i t2 = _mm256_and_si256(in, v_003f);
                const __m256i t3 = _mm256_or_si256(t1, t2);
                const __m256i t4 = _mm256_or_si256(t3, v_c080);
                const __m256i uu = _mm256_blendv_epi8(t4, in, obm);
                const uint32_t M0 = obmask & 0x55555555;
                const uint32_t M2 = ((M0 >> 7) | M0) & 0x00ff00ff;
                const uint8_t* row = &pack_1_2_utf8_bytes[uint8_t(M2)][0];
                const uint8_t* row2 = &pack_1_2_utf8_bytes[uint8_t(M2 >> 16)][0];
                const __m128i sh = _mm_loadu_si128((__m128i*)(row + 1));
                const __m128i sh2 = _mm_loadu_si128((__m128i*)(row2 + 1));
                const __m256i pk = _mm256_shuffle_epi8(
                    uu, _mm256_setr_m128i(sh, sh2));
                _mm_storeu_si128((__m128i*)op, _mm256_castsi256_si128(pk));
                op += row[0];
                _mm_storeu_si128((__m128i*)op, _mm256_extractf128_si256(pk, 1));
                op += row2[0];
                buf += 16;
                continue;
            }
            break; // не должно случиться на этих данных
        }
        for (; buf < end; buf++) {                          // скалярный хвост
            uint16_t w = (uint16_t)*buf;
            if ((w & 0xFF80) == 0) *op++ = (char)w;
            else { *op++ = (char)((w >> 6) | 0xC0); *op++ = (char)((w & 0x3F) | 0x80); }
        }
        // эталон
        static char ref[256];
        char* rp = ref;
        for (size_t i = 0; i < n; i++) {
            uint16_t w = (uint16_t)in42[i];
            if ((w & 0xFF80) == 0) *rp++ = (char)w;
            else { *rp++ = (char)((w >> 6) | 0xC0); *rp++ = (char)((w & 0x3F) | 0x80); }
        }
        size_t olen = (size_t)(op - out), rlen = (size_t)(rp - ref);
        if (olen != rlen || memcmp(out, ref, rlen) != 0) {
            printf("FAIL K margin8 len out=%zu ref=%zu\n  out:", olen, rlen);
            for (size_t i = 0; i < olen; i++) printf(" %02x", (uint8_t)out[i]);
            printf("\n  ref:");
            for (size_t i = 0; i < rlen; i++) printf(" %02x", (uint8_t)ref[i]);
            printf("\n");
            fails++;
        }
    }

    if (fails == 0) printf("OK all variants\n");
    return fails ? 1 : 0;
}
