/* t_xps3 — детерминированный: каждый вектор загружается из памяти
 * непосредственно перед asm с привязкой "x" — без привязанных к истории
 * регистровых остатков. */
#include <stdio.h>
#include <stdint.h>
#include <immintrin.h>
static uint64_t g_hash = 0x12345678;
static void acc(uint64_t v){ g_hash = g_hash*6364136223846793005ull + v + 1; }
static uint32_t mem_slot[64];
int main(void) {
    uint32_t pats[16][4];
    for (int r = 0; r < 16; r++)
        for (int k = 0; k < 4; k++)
            pats[r][k] = 0x01000000u*r + 0x111u*(k+1);
    for (int r = 0; r < 16; r++) {
        __m128i p = _mm_loadu_si128((__m128i*)pats[r]);
        for (int lane = 0; lane < 4; lane++) {
            uint32_t v;
            switch (lane) {
            case 0: __asm__ volatile("vextractps %2, %1, %0" : "=r"(v) : "x"(p), "i"(0)); break;
            case 1: __asm__ volatile("vextractps %2, %1, %0" : "=r"(v) : "x"(p), "i"(1)); break;
            case 2: __asm__ volatile("vextractps %2, %1, %0" : "=r"(v) : "x"(p), "i"(2)); break;
            default: __asm__ volatile("vextractps %2, %1, %0" : "=r"(v) : "x"(p), "i"(3)); break;
            }
            acc(v);
        }
    }
    /* mem-приёмник, все лейны */
    for (int lane = 0; lane < 4; lane++) {
        __m128i p = _mm_loadu_si128((__m128i*)pats[0]);
        switch (lane) {
        case 0: __asm__ volatile("vextractps %2, %1, %0" : "=m"(mem_slot[0]) : "x"(p), "i"(0)); break;
        case 1: __asm__ volatile("vextractps %2, %1, %0" : "=m"(mem_slot[1]) : "x"(p), "i"(1)); break;
        case 2: __asm__ volatile("vextractps %2, %1, %0" : "=m"(mem_slot[2]) : "x"(p), "i"(2)); break;
        default: __asm__ volatile("vextractps %2, %1, %0" : "=m"(mem_slot[3]) : "x"(p), "i"(3)); break;
        }
    }
    for (int k = 0; k < 4; k++) acc(mem_slot[k]);
    /* pinsr/pextr семья — детерминированно из памяти */
    __m128i b4 = _mm_loadu_si128((__m128i*)pats[1]);
    for (int lane = 0; lane < 4; lane++) {
        __m128i x;
        switch (lane) {
        case 0: x = _mm_insert_epi32(b4, 0xDEADBEEF, 0); break;
        case 1: x = _mm_insert_epi32(b4, 0xDEADBEEF, 1); break;
        case 2: x = _mm_insert_epi32(b4, 0xDEADBEEF, 2); break;
        default: x = _mm_insert_epi32(b4, 0xDEADBEEF, 3); break;
        }
        uint32_t lanes[4]; _mm_storeu_si128((__m128i*)lanes, x);
        for (int k = 0; k < 4; k++) acc(lanes[k]);
    }
    printf("hash=%016llx\n", (unsigned long long)g_hash);
    return 0;
}
