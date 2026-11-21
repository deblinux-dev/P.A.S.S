/* t_smc.c — минимальный репрод B-2009: JIT-паттерн JSC.
 * Копируем функцию с AVX2 в RWX-страницу («JIT»), вызываем, затем
 * холодно-патчим середину через 32-байтный memcpy (rdx=0x20 — как в
 * замороженном состоянии bun), вызываем снова. Под DR запись в
 * исполняемую страницу = SMC-detect → flush; флаш идёт по фрагментам,
 * содержащим мета-инструкции P.A.S.S. с translation-полем.          */
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <immintrin.h>

__attribute__((noinline)) void fn_v1(unsigned char *out, const unsigned char *in, unsigned n)
{
    __m256i m = _mm256_set1_epi8(0x55);
    __m256i t = _mm256_set1_epi8(7);
    for (unsigned i = 0; i < n; i += 32) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(in + i));
        v = _mm256_blendv_epi8(v, m, _mm256_cmpgt_epi8(v, t));
        _mm256_storeu_si256((__m256i *)(out + i), v);
    }
}
__attribute__((noinline)) void fn_v2(unsigned char *out, const unsigned char *in, unsigned n)
{
    __m256i m = _mm256_set1_epi8(0xAA);
    __m256i t = _mm256_set1_epi8(7);
    for (unsigned i = 0; i < n; i += 32) {
        __m256i v = _mm256_loadu_si256((const __m256i *)(in + i));
        v = _mm256_blendv_epi8(v, m, _mm256_cmpgt_epi8(v, t));
        _mm256_storeu_si256((__m256i *)(out + i), v);
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

    memcpy(code, (const void *)fn_v1, (size_t)len);   /* «JIT»-копия fn_v1 */
    __builtin___clear_cache((char *)code, (char *)code + len);

    unsigned char in[64], out[64];
    for (int i = 0; i < 64; i++) in[i] = (unsigned char)i;

    fn_t f = (fn_t)code;
    memset(out, 0, sizeof(out));
    f(out, in, 64);                                   /* вызов #1: fn_v1 */
    printf("v1 out[3]=%02x out[9]=%02x\n", out[3], out[9]);

    /* холодный патч: первые 32 байта тела fn_v2 поверх начала JIT-копии
     * (offset 0 = граница инструкций — как настоящие JIT-патчи JSC) */
    long off = 0;
    memcpy(code + off, (const char *)fn_v2 + off, 32);
    __builtin___clear_cache((char *)code + off, (char *)code + off + 32);

    memset(out, 0, sizeof(out));
    f(out, in, 64);                                   /* вызов #2: патч */
    printf("v2 out[3]=%02x out[9]=%02x\n", out[3], out[9]);
    printf("DONE\n");
    return 0;
}
