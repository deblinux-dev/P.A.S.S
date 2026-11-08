/* t_deadloop.c — проверка предохранителя trap-цикла (ARCHITECTURE §6.21).
 * Сценарий бага bun.exe v0.2: приложение перехватывает SIGILL, а обработчик
 * ВОЗВРАЩАЕТСЯ на ту же инструкцию — без предохранителя это бесшумное
 * вечное зависание со 100% CPU.
 * Нативно: виснет бесконечно (не запускать без timeout!).
 * Под P.A.S.S.: громкий диагноз на stderr и exit(86) после 64 trap-срабатываний.
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sigh(int s) { (void)s; }   /* вернуться на ту же инструкцию */

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigh;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGILL, &sa, NULL) != 0) { perror("sigaction"); return 2; }
    printf("до ud2\n");
    fflush(stdout);
    __asm__ volatile("ud2");
    printf("после ud2 — сюда попадать не должны\n");
    return 0;
}
