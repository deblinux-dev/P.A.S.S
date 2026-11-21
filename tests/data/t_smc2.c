/* t_smc2.c — SSE2-вариант SMC-теста: код нативен для Penryn, клиент его
 * НЕ переписывает (passthrough). Изолирует механизм: если виснет — SMC-
 * механика DR/qemu сама по себе; если нет — проблема в связке
 * «SMC-flush × переписанные (эмулируемые) фрагменты».               */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <emmintrin.h>

__attribute__((noinline)) void fn_v1(unsigned char *out, const unsigned char *in, unsigned n)
{
    __m128i m = _mm_set1_epi8(0x55);
    __m128i t = _mm_set1_epi8(7);
    for (unsigned i = 0; i < n; i += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(in + i));
        v = _mm_or_si128(_mm_and_si128(_mm_cmpgt_epi8(v, t), m),
                         _mm_andnot_si128(_mm_cmpgt_epi8(v, t), v));
        _mm_storeu_si128((__m128i *)(out + i), v);
    }
}
__attribute__((noinline)) void fn_v2(unsigned char *out, const unsigned char *in, unsigned n)
{
    __m128i m = _mm_set1_epi8(0xAA);
    __m128i t = _mm_set1_epi8(7);
    for (unsigned i = 0; i < n; i += 16) {
        __m128i v = _mm_loadu_si128((const __m128i *)(in + i));
        v = _mm_or_si128(_mm_and_si128(_mm_cmpgt_epi8(v, t), m),
                         _mm_andnot_si128(_mm_cmpgt_epi8(v, t), v));
        _mm_storeu_si128((__m128i *)(out + i), v);
    }
}
__attribute__((noinline)) void fn_end(void) { }

typedef void (*fn_t)(unsigned char *, const unsigned char *, unsigned);

int main(void)
{
    long len = (long)((const char *)fn_v2 - (const char *)fn_v1);
    printf("fn_v1 len=%ld\n", len);
    if (len <= 0 || len > 4096) { printf("BAD-LAYOUT\n"); return 3; }

    unsigned char *code = mmap(NULL, 65536,
                               PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) { perror("mmap"); return 1; }

    memcpy(code, (const void *)fn_v1, (size_t)len);
    __builtin___clear_cache((char *)code, (char *)code + len);

    unsigned char in[64], out[64];
    for (int i = 0; i < 64; i++) in[i] = (unsigned char)i;

    fn_t f = (fn_t)code;
    memset(out, 0, sizeof(out));
    f(out, in, 64);
    printf("v1 out[3]=%02x out[9]=%02x\n", out[3], out[9]);

    memcpy(code, (const char *)fn_v2, 32);
    __builtin___clear_cache((char *)code, (char *)code + 32);

    memset(out, 0, sizeof(out));
    f(out, in, 64);
    printf("v2 out[3]=%02x out[9]=%02x\n", out[3], out[9]);
    printf("DONE\n");
    return 0;
}
