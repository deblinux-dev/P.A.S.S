/* t_bmi2.c — дифференциальный тест BMI2 (+movbe/crc32/popcnt): матрица
 * опкод × энкодинг (reg/mem, 32/64, алиасы dst==vvvv==rm) × краевые входы.
 * Вывод: строка "id res0 res1 flags" на каждый случай; native vs PASS —
 * вывод обязан совпасть побайтно. Флаги маскируются до определённых
 * Intel-битов (CF|PF|ZF|SF|OF = 0x8C5): AF у BMI2 undefined.
 *
 * Сборка: gcc -O2 -static -mbmi -mbmi2 -mlzcnt -mmovbe -msse4.2 t_bmi2.c -o t_bmi2
 */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

static char out[1 << 22];
static size_t olen;

static void emit(uint32_t id, uint64_t r0, uint64_t r1, uint64_t fl)
{
    olen += (size_t)sprintf(out + olen, "%04x %016llx %016llx %03llx\n",
                            (unsigned)id, (unsigned long long)r0,
                            (unsigned long long)r1, (unsigned long long)fl);
}

static inline uint64_t getfl(void)
{
    uint64_t f;
    __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f) : : "cc");
    return f;
}

/* mem-переменные для mem-форм */
static volatile uint64_t m64[4];
static volatile uint32_t m32[4];

static const uint64_t vals[] = {
    0x0000000000000000ULL, 0x0000000000000001ULL, 0x000000000000003fULL,
    0x0000000000000040ULL, 0x00000000000000ffULL, 0x00000000ffffffffULL,
    0x0000000080000000ULL, 0x7fffffffffffffffULL, 0x8000000000000000ULL,
    0xffffffffffffffffULL, 0xffffffff00000001ULL, 0x0123456789abcdefULL,
    0xfedcba9876543210ULL, 0x5555555555555555ULL, 0xaaaaaaaaaaaaaaaaULL,
    0x00000000deadbeefULL,
};
#define NVALS ((int)(sizeof(vals) / sizeof(vals[0])))

static const uint64_t idxs[] = { 0, 1, 7, 8, 15, 16, 31, 32, 33, 63, 64, 255 };
#define NIDX ((int)(sizeof(idxs) / sizeof(idxs[0])))

int main(void)
{
    uint32_t id = 0;
    uint64_t r, f, hi;

    /* ═══ TZCNT / LZCNT: reg и mem, 64 и 32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v; m32[1] = (uint32_t)v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\ttzcnt %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\ttzcnt %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\ttzcntl %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\ttzcntq %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tlzcnt %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tlzcnt %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tlzcntl %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());
    }

    /* ═══ BLSI / BLSR / BLSMSK: reg + mem, 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v; m32[1] = (uint32_t)v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsi %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsi %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsil %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsr %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsr %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsrl %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsmsk %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsmsk %1, %0" : "=r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tblsmskl %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());
    }

    /* ═══ ANDN: reg,reg и reg,mem; 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v; m32[1] = (uint32_t)v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tandn %1, %2, %0" : "=r"(r) : "r"(v), "r"(~v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tandn %1, %2, %0" : "=r"(r) : "m"(m64[1]), "r"(~v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tandnl %k1, %k2, %k0"
                         : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)~v) : "cc");
        emit(id++, r, 0, getfl());
    }

    /* ═══ BZHI: reg,reg + reg,mem; 64/32; idx из idxs ═══ */
    for (int k = 0; k < NVALS; k++) {
        for (int j = 0; j < NIDX; j++) {
            uint64_t v = vals[k], ix = idxs[j];
            m64[1] = v; m32[1] = (uint32_t)v;

            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbzhi %2, %1, %0" : "=r"(r) : "r"(v), "r"(ix) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbzhi %2, %1, %0" : "=r"(r) : "m"(m64[1]), "r"(ix) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbzhil %k2, %k1, %k0"
                             : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)ix) : "cc");
            emit(id++, r, 0, getfl());
        }
    }

    /* ═══ BEXTR: control=reg/mem; 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        for (int j = 0; j < NIDX; j++) {
            uint64_t v = vals[k];
            uint64_t ctrl = (idxs[j] & 0xff) | ((idxs[(j + 3) % NIDX] & 0xff) << 8);
            m64[1] = v; m32[1] = (uint32_t)v;

            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbextr %2, %1, %0" : "=r"(r) : "r"(v), "r"(ctrl) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbextr %2, %1, %0" : "=r"(r) : "m"(m64[1]), "r"(ctrl) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tbextrl %k2, %k1, %k0"
                             : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)ctrl) : "cc");
            emit(id++, r, 0, getfl());
        }
    }

    /* ═══ SHLX / SHRX / SARX: reg,reg + reg,mem; 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        for (int j = 0; j < NIDX; j++) {
            uint64_t v = vals[k], c = idxs[j];
            m64[1] = v; m32[1] = (uint32_t)v;

            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tshlx %2, %1, %0" : "=r"(r) : "r"(v), "r"(c) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tshlx %2, %1, %0" : "=r"(r) : "m"(m64[1]), "r"(c) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tshlxl %k2, %k1, %k0"
                             : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)c) : "cc");
            emit(id++, r, 0, getfl());

            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tshrx %2, %1, %0" : "=r"(r) : "r"(v), "r"(c) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tshrx %2, %1, %0" : "=r"(r) : "m"(m64[1]), "r"(c) : "cc");
            emit(id++, r, 0, getfl());

            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tsarx %2, %1, %0" : "=r"(r) : "r"(v), "r"(c) : "cc");
            emit(id++, r, 0, getfl());
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tsarxl %k2, %k1, %k0"
                             : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)c) : "cc");
            emit(id++, r, 0, getfl());
        }
    }

    /* ═══ RORX: imm-форма, 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\trorx $1, %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\trorx $13, %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\trorx $63, %1, %0" : "=r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\trorxl $5, %k1, %k0" : "=r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());
    }

    /* ═══ MULX: dst_hi=reg, dst_lo=vvvv, src=rm; алиасы ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v; m32[1] = (uint32_t)v;

        /* базовая: lo=rcx(маркер 0x11..), hi=rbx, src=rdx */
        {
            register uint64_t l __asm__("rcx") = 0x1111111111111111ULL;
            register uint64_t h __asm__("rbx") = 0x2222222222222222ULL;
            register uint64_t s __asm__("rdx") = v;
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmulx %%rdx, %%rcx, %%rbx"
                             : "+r"(l), "+r"(h), "+r"(s) : : "cc");
            __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f));
            emit(id++, l, h, f);
        }
        /* 32-бит */
        {
            register uint32_t l __asm__("ecx") = 0x11111111u;
            register uint32_t h __asm__("ebx") = 0x22222222u;
            register uint32_t s __asm__("edx") = (uint32_t)v;
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmulxl %%edx, %%ecx, %%ebx"
                             : "+r"(l), "+r"(h), "+r"(s) : : "cc");
            uint64_t lr = l, hr = h;
            __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f));
            emit(id++, lr, hr, f);
        }
        /* mem-форма src=[m64+8] */
        {
            register uint64_t l __asm__("rcx") = 0x1111111111111111ULL;
            register uint64_t h __asm__("rbx") = 0x2222222222222222ULL;
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmulx m64+8(%%rip), %%rcx, %%rbx"
                             : "+r"(l), "+r"(h) : : "cc");
            __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f));
            emit(id++, l, h, f);
        }
        /* алиас dst_hi == dst_lo (rcx,rcx) */
        {
            register uint64_t l __asm__("rcx") = 0x1111111111111111ULL;
            register uint64_t s __asm__("rdx") = v;
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmulx %%rdx, %%rcx, %%rcx" : "+r"(l), "+r"(s) : : "cc");
            __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f));
            emit(id++, l, 0, f);
        }
        /* алиас dst_lo == src (rdx,rbx) */
        {
            register uint64_t s __asm__("rdx") = v;
            register uint64_t h __asm__("rbx") = 0x2222222222222222ULL;
            __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmulx %%rdx, %%rdx, %%rbx" : "+r"(s), "+r"(h) : : "cc");
            __asm__ volatile("pushfq\n\tpopq %0\n\tandq $0x8C5, %0" : "=r"(f));
            emit(id++, s, h, f);
        }
    }

    /* ═══ PDEP / PEXT: mask=vvvv, src=rm; reg/mem; 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        uint64_t mask = 0x00000000f0f0f0f0ULL;
        m64[1] = v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpdep %2, %1, %0" : "=r"(r) : "r"(v), "r"(mask) : "cc");
        emit(id++, r, 0, getfl());
        /* mem-формы: rm-позиция у PDEP/P_EXT — это MASK (vvvv=src — всегда
         * reg по энкодингу VEX.NDS), поэтому в память кладём mask */
        m64[1] = mask;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpdepq %1, %2, %0" : "=r"(r) : "m"(m64[1]), "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpextq %1, %2, %0" : "=r"(r) : "m"(m64[1]), "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpextl %k2, %k1, %k0"
                         : "=r"(r) : "r"((uint32_t)v), "r"((uint32_t)mask) : "cc");
        emit(id++, r, 0, getfl());
    }

    /* ═══ MOVBE: reg<->mem, 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v; m32[1] = (uint32_t)v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmovbe m64+8(%%rip), %0" : "=r"(r));
        emit(id++, r, 0, 0);
        m64[1] = v;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmovbe %0, m64+8(%%rip)" : : "r"(v) : "memory");
        emit(id++, m64[1], 0, 0);
        m64[1] = v;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tmovbel m32+8(%%rip), %k0" : "=r"(r));
        emit(id++, r, 0, 0);
    }

    /* ═══ CRC32: reg/mem; l/l, q/q ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        uint64_t acc = (uint32_t)v;
        m64[1] = v; m32[1] = (uint32_t)v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tcrc32l %k1, %k0" : "+r"(acc) : "r"((uint32_t)v) : "cc");
        emit(id++, acc, 0, getfl());
        acc = (uint32_t)v;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tcrc32l %k1, %k0" : "+r"(acc) : "m"(m32[1]) : "cc");
        emit(id++, acc, 0, getfl());
        acc = (uint32_t)v;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tcrc32q %1, %0" : "+r"(acc) : "r"(v) : "cc");
        emit(id++, acc, 0, getfl());
        acc = (uint32_t)v;
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tcrc32q %1, %0" : "+r"(acc) : "m"(m64[1]) : "cc");
        emit(id++, acc, 0, getfl());
    }

    /* ═══ POPCNT: reg/mem, 64/32 ═══ */
    for (int k = 0; k < NVALS; k++) {
        uint64_t v = vals[k];
        m64[1] = v;

        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpopcnt %1, %0" : "=&r"(r) : "r"(v) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpopcnt %1, %0" : "=&r"(r) : "m"(m64[1]) : "cc");
        emit(id++, r, 0, getfl());
        __asm__ volatile("pushfq\n\tandq $-0x8D6, (%%rsp)\n\torq $0x44, (%%rsp)\n\tpopfq\n\tpopcntl %k1, %k0" : "=&r"(r) : "r"((uint32_t)v) : "cc");
        emit(id++, r, 0, getfl());
    }

    (void)hi;
    ssize_t wr = write(1, out, olen);
    (void)wr;
    _exit(0);
}
