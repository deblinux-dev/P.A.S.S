/* Минимальный репро + тест: инструкции с r10 в операндах.
 * 1) pinsrd с источником r10d  (раньше: отклонение -> native -> SIGILL на Core 2)
 * 2) pextrd с приёмником r10d  (запись в r10 через BUF_GPR(10))
 * 3) исходный r10 обязан пережить эмулированную операцию без изменений
 */
#include <stdio.h>
#include <stdint.h>

int main(void)
{
    uint32_t v = 0xDEADBEEF;
    uint32_t out1[4] = {0, 0, 0, 0};
    uint64_t r10_before = 0, r10_after = 0, r10_dst = 0;

    __asm__ volatile(
        "movabs $0x1122334455667788, %%r10\n\t"
        "movq %%r10, %1\n\t"
        "movl %4, %%ecx\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "pinsrd $1, %%r10d, %%xmm0\n\t"      /* r10d — источник */
        "pinsrd $3, %%ecx, %%xmm0\n\t"
        "movdqu %%xmm0, %0\n\t"
        "movq %%r10, %2\n\t"                 /* r10 должен выжить */
        "pextrd $1, %%xmm0, %%r10d\n\t"      /* r10d — приёмник */
        "movq %%r10, %3\n\t"
        : "=m"(out1), "=m"(r10_before), "=m"(r10_after), "=m"(r10_dst)
        : "r"(v)
        : "rcx", "r10", "xmm0", "memory");

    printf("out1   = %08x %08x %08x %08x (ожид: 00000000 55667788 00000000 deadbeef)\n",
           out1[0], out1[1], out1[2], out1[3]);
    printf("r10 до = %016llx (ожид: 1122334455667788)\n",
           (unsigned long long)r10_before);
    printf("r10 по = %016llx (ожид: 1122334455667788 — без изменений!)\n",
           (unsigned long long)r10_after);
    printf("pextrd -> r10 = %016llx (ожид: 0000000055667788)\n",
           (unsigned long long)r10_dst);

    int ok = (out1[1] == 0x55667788 && out1[3] == 0xDEADBEEF &&
              r10_before == 0x1122334455667788ull &&
              r10_after  == 0x1122334455667788ull &&
              r10_dst    == 0x0000000055667788ull);
    printf("%s\n", ok ? "OK: все проверки r10 пройдены"
                      : "FAIL: есть расхождения");
    return ok ? 0 : 1;
}
