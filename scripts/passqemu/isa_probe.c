/* isa_probe.c — проверка, реально ли qemu-user TCG гейтит ISA по -cpu MODEL:
   печатает, что видит CPUID, и что происходит при ИСПОЛНЕНИИ
   SSE2 / SSE4.1 / SSE4.2 / AVX / AVX2 (SIGILL = гейтинг работает). */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <immintrin.h>
#include <cpuid.h>

static sigjmp_buf jb;

static void ill(int s, siginfo_t *si, void *u) { (void)s; (void)si; (void)u; siglongjmp(jb, 1); }

#define PROBE(name, fn) do { \
    volatile int out = -1; \
    if (sigsetjmp(jb, 1) == 0) { fn(&out); printf("%-8s EXEC-OK   out=%d\n", name, out); } \
    else printf("%-8s SIGILL    (нет на этом CPU)\n", name); \
} while (0)

__attribute__((target("sse2")))  static void p_sse2(volatile int *out) {
    volatile int x[4] = {1,2,3,4}; __m128i a = _mm_loadu_si128((const __m128i*)x);
    __m128i b = _mm_add_epi32(a, a); int r[4]; _mm_storeu_si128((__m128i*)r, b); *out = r[3];
}
__attribute__((target("sse4.1"))) static void p_sse41(volatile int *out) {
    volatile int x[4] = {100,200,300,400}; __m128i a = _mm_loadu_si128((const __m128i*)x);
    __m128i b = _mm_blend_epi16(a, _mm_set1_epi32(7), 0x33); int r[4];
    _mm_storeu_si128((__m128i*)r, b); *out = r[0];
}
__attribute__((target("sse4.2"))) static void p_sse42(volatile int *out) {
    volatile char a[16], b[16]; memset((void*)a, 'A', 16); memset((void*)b, 'B', 16);
    __m128i va = _mm_loadu_si128((const __m128i*)a), vb = _mm_loadu_si128((const __m128i*)b);
    *out = _mm_cmpestrc(va, 16, vb, 16, 0);
}
__attribute__((target("avx")))  static void p_avx(volatile int *out) {
    volatile float f[8] = {1,2,3,4,5,6,7,8}; __m256 a = _mm256_loadu_ps((const float*)f);
    __m256 b = _mm256_add_ps(a, a); float r[8]; _mm256_storeu_ps(r, b); *out = (int)r[7];
}
__attribute__((target("avx2"))) static void p_avx2(volatile int *out) {
    volatile int x[8] = {1,2,3,4,5,6,7,8}; __m256i a = _mm256_loadu_si256((const __m256i*)x);
    __m256i b = _mm256_add_epi32(a, a); int r[8]; _mm256_storeu_si256((__m256i*)r, b); *out = r[7];
}

static void cpuid_dump(void) {
    unsigned a,b,c,d;
    __cpuid_count(1, 0, a,b,c,d);
    printf("cpuid#1   : sse4.1=%d sse4.2=%d avx=%d\n", (c>>19)&1, (c>>20)&1, (c>>28)&1);
    __cpuid_count(7, 0, a,b,c,d);
    printf("cpuid#7.0 : bmi1=%d avx2=%d bmi2=%d\n", (b>>3)&1, (b>>5)&1, (b>>8)&1);
}

int main(void) {
    struct sigaction sa; memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = ill; sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);
    cpuid_dump();
    PROBE("sse2",  p_sse2);
    PROBE("sse41", p_sse41);
    PROBE("sse42", p_sse42);
    PROBE("avx",   p_avx);
    PROBE("avx2",  p_avx2);
    return 0;
}
