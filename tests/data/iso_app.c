/* iso_app.c — листовая функция с живыми данными в красной зоне x86-64.
 * gcc для листовой функции без вызовов кладёт локальные volatile-переменные
 * в [rsp-N..rsp) (N<=128) и НЕ двигает rsp — это и есть красная зона.
 * Если кто-то (стаб clean call) пишет ниже rsp — данные портятся.
 */
#include <stdio.h>

__attribute__((noinline)) static long leaf(void)
{
    volatile long a = 0x41;
    volatile long b = 0x42;
    volatile long c = 0x43;
    volatile long d = 0x44;
    volatile long e = 0x45;
    long s = a + b + c + d + e;
    return s;
}

int main(void)
{
    printf("%lx\n", leaf());
    return 0;
}
