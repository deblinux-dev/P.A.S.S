// P.A.S.S. diff-test: точная копия simdutf haswell (AVX2) ядра
// convert_utf16_to_utf8 (LE) — тот же путь, что bun 1.2.14 зовёт из
// string_immutable.zig: copyUTF16IntoUTF8WithBuffer ->
// bun.simdutf.convert.utf16.to.utf8.with_errors.le (use_simdutf=1).
//
// Воспроизводит порчу кириллицы: единицы U+04xx, попавшие в выхлоп
// как [c>>8, c&0xFF] (сырые байты юнита) вместо [0xC0|(c>>6), 0x80|(c&0x3F)].
//
// Выход: печатает hex транскода; exit 0 если байт-в-байт совпало с
// эталонным UTF-8, 1 если нет. Батарея сравнивает native vs pass.
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <utility>
#include <immintrin.h>

#include "simdutf_u16u8_tables.h"

using simdutf::tables::utf16_to_utf8::pack_1_2_utf8_bytes;
using simdutf::tables::utf16_to_utf8::pack_1_2_3_utf8_bytes;

// ── скалярный хвост (как simdutf scalar::utf16::utf16_to_utf8_convert) ──
static char* scalar_tail(const char16_t* buf, const char16_t* end, char* out) {
    while (buf < end) {
        uint16_t word = (uint16_t)*buf++;
        if ((word & 0xFF80) == 0) {
            *out++ = (char)word;
        } else if ((word & 0xF800) == 0) {
            *out++ = (char)((word >> 6) | 0xC0);
            *out++ = (char)((word & 0x3F) | 0x80);
        } else if ((word & 0xF800) != 0xD800) {
            *out++ = (char)((word >> 12) | 0xE0);
            *out++ = (char)(((word >> 6) & 0x3F) | 0x80);
            *out++ = (char)((word & 0x3F) | 0x80);
        } else {
            // surrogate pair (в наших данных нет; для полноты)
            uint16_t diff = (uint16_t)(word - 0xD800);
            uint16_t next = (uint16_t)(buf < end ? *buf : 0);
            buf++;
            uint16_t diff2 = (uint16_t)(next - 0xDC00);
            if ((diff | diff2) > 0x3FF) return out;
            uint32_t value = ((uint32_t)diff << 10) + diff2 + 0x10000;
            *out++ = (char)((value >> 18) | 0xF0);
            *out++ = (char)(((value >> 12) & 0x3F) | 0x80);
            *out++ = (char)(((value >> 6) & 0x3F) | 0x80);
            *out++ = (char)((value & 0x3F) | 0x80);
        }
    }
    return out;
}

// ── векторное ядро: дословно avx2_convert_utf16_to_utf8 (LE) ──
static std::pair<const char16_t*, char*>
avx2_convert_utf16_to_utf8(const char16_t* buf, size_t len, char* utf8_output) {
    const char16_t* end = buf + len;

    const __m256i v_0000 = _mm256_setzero_si256();
    const __m256i v_f800 = _mm256_set1_epi16((int16_t)0xf800);
    const __m256i v_d800 = _mm256_set1_epi16((int16_t)0xd800);
    const __m256i v_c080 = _mm256_set1_epi16((int16_t)0xc080);
    const size_t safety_margin = 12;

    while (end - buf >= std::ptrdiff_t(16 + safety_margin)) {
        __m256i in = _mm256_loadu_si256((__m256i*)buf);
        // big_endian=false: swap-путь не нужен
        const __m256i v_ff80 = _mm256_set1_epi16((int16_t)0xff80);
        if (_mm256_testz_si256(in, v_ff80)) { // ASCII fast path
            const __m128i utf8_packed = _mm_packus_epi16(
                _mm256_castsi256_si128(in), _mm256_extractf128_si256(in, 1));
            _mm_storeu_si128((__m128i*)utf8_output, utf8_packed);
            buf += 16;
            utf8_output += 16;
            continue;
        }
        const __m256i one_byte_bytemask =
            _mm256_cmpeq_epi16(_mm256_and_si256(in, v_ff80), v_0000);
        const uint32_t one_byte_bitmask =
            (uint32_t)_mm256_movemask_epi8(one_byte_bytemask);

        const __m256i one_or_two_bytes_bytemask =
            _mm256_cmpeq_epi16(_mm256_and_si256(in, v_f800), v_0000);
        const uint32_t one_or_two_bytes_bitmask =
            (uint32_t)_mm256_movemask_epi8(one_or_two_bytes_bytemask);

        if (one_or_two_bytes_bitmask == 0xffffffff) {
            const __m256i v_1f00 = _mm256_set1_epi16((int16_t)0x1f00);
            const __m256i v_003f = _mm256_set1_epi16((int16_t)0x003f);

            const __m256i t0 = _mm256_slli_epi16(in, 2);
            const __m256i t1 = _mm256_and_si256(t0, v_1f00);
            const __m256i t2 = _mm256_and_si256(in, v_003f);
            const __m256i t3 = _mm256_or_si256(t1, t2);
            const __m256i t4 = _mm256_or_si256(t3, v_c080);

            const __m256i utf8_unpacked =
                _mm256_blendv_epi8(t4, in, one_byte_bytemask);

            const uint32_t M0 = one_byte_bitmask & 0x55555555;
            const uint32_t M1 = M0 >> 7;
            const uint32_t M2 = (M1 | M0) & 0x00ff00ff;

            const uint8_t* row =
                &pack_1_2_utf8_bytes[uint8_t(M2)][0];
            const uint8_t* row_2 =
                &pack_1_2_utf8_bytes[uint8_t(M2 >> 16)][0];

            const __m128i shuffle = _mm_loadu_si128((__m128i*)(row + 1));
            const __m128i shuffle_2 = _mm_loadu_si128((__m128i*)(row_2 + 1));

            const __m256i utf8_packed = _mm256_shuffle_epi8(
                utf8_unpacked, _mm256_setr_m128i(shuffle, shuffle_2));
            _mm_storeu_si128((__m128i*)utf8_output,
                             _mm256_castsi256_si128(utf8_packed));
            utf8_output += row[0];
            _mm_storeu_si128((__m128i*)utf8_output,
                             _mm256_extractf128_si256(utf8_packed, 1));
            utf8_output += row_2[0];

            buf += 16;
            continue;
        }
        // проверка суррогатов (в наших данных суррогатов нет, но путь
        // нужен для полноты — как в оригинале)
        const __m256i surrogates_bytemask =
            _mm256_cmpeq_epi16(_mm256_and_si256(in, v_f800), v_d800);
        const uint32_t surrogates_bitmask =
            (uint32_t)_mm256_movemask_epi8(surrogates_bytemask);
        if (surrogates_bitmask == 0x00000000) {
            // 1..3 байта — дословно как в simdutf
            const __m256i dup_even = _mm256_setr_epi16(
                0x0000, 0x0202, 0x0404, 0x0606, 0x0808, 0x0a0a, 0x0c0c, 0x0e0e,
                0x0000, 0x0202, 0x0404, 0x0606, 0x0808, 0x0a0a, 0x0c0c, 0x0e0e);
#define simdutf_vec(x) _mm256_set1_epi16(static_cast<uint16_t>(x))
            const __m256i t0 = _mm256_shuffle_epi8(in, dup_even);
            const __m256i t1 = _mm256_and_si256(t0, simdutf_vec(0b0011111101111111));
            const __m256i t2 = _mm256_or_si256(t1, simdutf_vec(0b1000000000000000));
            const __m256i s0 = _mm256_srli_epi16(in, 4);
            const __m256i s1 = _mm256_and_si256(s0, simdutf_vec(0b0000111111111100));
            const __m256i s2 = _mm256_maddubs_epi16(s1, simdutf_vec(0x0140));
            const __m256i s3 = _mm256_or_si256(s2, simdutf_vec(0b1100000011100000));
            const __m256i m0 = _mm256_andnot_si256(one_or_two_bytes_bytemask,
                                                   simdutf_vec(0b0100000000000000));
            const __m256i s4 = _mm256_xor_si256(s3, m0);
#undef simdutf_vec
            const __m256i out0 = _mm256_unpacklo_epi16(t2, s4);
            const __m256i out1 = _mm256_unpackhi_epi16(t2, s4);
            const uint32_t mask = (one_byte_bitmask & 0x55555555) |
                                  (one_or_two_bytes_bitmask & 0xaaaaaaaa);
            const uint8_t mask0 = uint8_t(mask);
            const uint8_t* row0 = &pack_1_2_3_utf8_bytes[mask0][0];
            const __m128i shuffle0 = _mm_loadu_si128((__m128i*)(row0 + 1));
            const __m128i utf8_0 =
                _mm_shuffle_epi8(_mm256_castsi256_si128(out0), shuffle0);
            const uint8_t mask1 = uint8_t(mask >> 8);
            const uint8_t* row1 = &pack_1_2_3_utf8_bytes[mask1][0];
            const __m128i shuffle1 = _mm_loadu_si128((__m128i*)(row1 + 1));
            const __m128i utf8_1 =
                _mm_shuffle_epi8(_mm256_castsi256_si128(out1), shuffle1);
            const uint8_t mask2 = uint8_t(mask >> 16);
            const uint8_t* row2 = &pack_1_2_3_utf8_bytes[mask2][0];
            const __m128i shuffle2 = _mm_loadu_si128((__m128i*)(row2 + 1));
            const __m128i utf8_2 =
                _mm_shuffle_epi8(_mm256_extractf128_si256(out0, 1), shuffle2);
            const uint8_t mask3 = uint8_t(mask >> 24);
            const uint8_t* row3 = &pack_1_2_3_utf8_bytes[mask3][0];
            const __m128i shuffle3 = _mm_loadu_si128((__m128i*)(row3 + 1));
            const __m128i utf8_3 =
                _mm_shuffle_epi8(_mm256_extractf128_si256(out1, 1), shuffle3);
            _mm_storeu_si128((__m128i*)utf8_output, utf8_0);
            utf8_output += row0[0];
            _mm_storeu_si128((__m128i*)utf8_output, utf8_1);
            utf8_output += row1[0];
            _mm_storeu_si128((__m128i*)utf8_output, utf8_2);
            utf8_output += row2[0];
            _mm_storeu_si128((__m128i*)utf8_output, utf8_3);
            utf8_output += row3[0];
            buf += 16;
        } else {
            // суррогаты: скалярный fallback (как в оригинале)
            size_t forward = 15;
            size_t k = 0;
            if (size_t(end - buf) < forward + 1) forward = size_t(end - buf - 1);
            for (; k < forward; k++) {
                uint16_t word = (uint16_t)buf[k];
                if ((word & 0xFF80) == 0) {
                    *utf8_output++ = (char)word;
                } else if ((word & 0xF800) == 0) {
                    *utf8_output++ = (char)((word >> 6) | 0xC0);
                    *utf8_output++ = (char)((word & 0x3F) | 0x80);
                } else if ((word & 0xF800) != 0xD800) {
                    *utf8_output++ = (char)((word >> 12) | 0xE0);
                    *utf8_output++ = (char)(((word >> 6) & 0x3F) | 0x80);
                    *utf8_output++ = (char)((word & 0x3F) | 0x80);
                } else {
                    uint16_t diff = (uint16_t)(word - 0xD800);
                    uint16_t next_word = (uint16_t)buf[k + 1];
                    k++;
                    uint16_t diff2 = (uint16_t)(next_word - 0xDC00);
                    if ((diff | diff2) > 0x3FF) {
                        return std::make_pair(nullptr, utf8_output);
                    }
                    uint32_t value = ((uint32_t)diff << 10) + diff2 + 0x10000;
                    *utf8_output++ = (char)((value >> 18) | 0xF0);
                    *utf8_output++ = (char)(((value >> 12) & 0x3F) | 0x80);
                    *utf8_output++ = (char)(((value >> 6) & 0x3F) | 0x80);
                    *utf8_output++ = (char)((value & 0x3F) | 0x80);
                }
            }
            buf += k;
        }
    }
    // скалярный хвост (как в вызывающем коде simdutf)
    utf8_output = scalar_tail(buf, end, utf8_output);
    return std::make_pair(end, utf8_output);
}

int main() {
    // строка пользователя, ×3 (126 юнитов) — 7+ векторных итераций
    static const char16_t in[] =
        u"ПРОВЕРКА РАЗНОЙ КИРИЛЛИЦЫ И ПРОЧЕГО ВЫВОДА"
        u"ПРОВЕРКА РАЗНОЙ КИРИЛЛИЦЫ И ПРОЧЕГО ВЫВОДА"
        u"ПРОВЕРКА РАЗНОЙ КИРИЛЛИЦЫ И ПРОЧЕГО ВЫВОДА";
    const size_t n = sizeof(in) / sizeof(in[0]) - 1;

    // эталонный UTF-8 (скалярно, заранее)
    static char ref[1024];
    char* ref_end = scalar_tail(in, in + n, ref);
    const size_t ref_len = (size_t)(ref_end - ref);

    static char out[1024];
    memset(out, 0xAA, sizeof(out));
    std::pair<const char16_t*, char*> r =
        avx2_convert_utf16_to_utf8(in, n, out);
    const size_t out_len = (size_t)(r.second - out);

    if (out_len != ref_len || memcmp(out, ref, ref_len) != 0) {
        printf("MISMATCH len out=%zu ref=%zu consumed=%d\n",
               out_len, ref_len, (int)(r.first - in));
        printf("out : ");
        for (size_t i = 0; i < out_len && i < 400; i++) printf("%02x ", (uint8_t)out[i]);
        printf("\nref : ");
        for (size_t i = 0; i < ref_len && i < 400; i++) printf("%02x ", (uint8_t)ref[i]);
        printf("\n");
        return 1;
    }
    printf("OK len=%zu\n", out_len);
    return 0;
}
