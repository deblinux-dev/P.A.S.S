/* t_sigill.S — B-2013: юнит-тест доставки SIGILL в app-обработчик.
 *
 * Паттерн Zig (bun): обработчик ставится RAW-сисколлом rt_sigaction
 * (в обход libc) — проверяем, что DR корректно записывает его в свою
 * таблицу и доставляет SIGILL при ud2. Обработчик продвигает RIP на 2
 * (скип ud2) и считает попадания; после 3 ud2 печатаем маркер и
 * выходим. Эталон (native host и qemu без клиента) обязан совпасть
 * с прогоном под P.A.S.S. байт в байт: "SIGILL-OK n=3\n".
 *
 * Замечание: ud2 — легальный опкод; trap-путь клиента обязан вернуть
 * «инструкция легальна» и доставить сигнал в app-обработчик (B-2011). */
    .text
    .globl _start

/* struct kernel_sigaction (x86_64, raw ABI):
 *   +0 handler, +8 flags, +16 restorer, +24 mask[8] */
    .data
    .align 8
sa:     .quad handler
        .quad 0x04000000            /* SA_RESTORER */
        .quad restorer
        .quad 0, 0                  /* mask = пусто */
sigcnt: .long 0
msg_ok:  .ascii "SIGILL-OK n=3\n"
msg_bad: .ascii "SIGILL-FAIL\n"

    .text
/* обработчик: (%rsp) = restorer, сохранённый RIP — в sigframe:
 * %rsp+8 = ucontext, +40 = uc_mcontext, +128 = gregs[REG_RIP]
 * → %rsp+176. Скипаем ud2 (+2), считаем попадания. */
handler:
    addw $2, 176(%rsp)
    incl sigcnt(%rip)
    ret

restorer:
    mov $15, %eax                   /* rt_sigreturn */
    syscall

_start:
    /* raw rt_sigaction(SIGILL=4, &sa, NULL, 8) — syscall 13 */
    mov $13, %eax
    mov $4, %edi
    lea sa(%rip), %rsi
    xor %edx, %edx
    xor %r10d, %r10d
    mov $8, %r10d                   /* sigsetsize */
    syscall
    test %eax, %eax
    jnz fail

    /* три ud2 в разных местах — каждый ловится обработчиком */
    ud2
    ud2

    mov sigcnt(%rip), %eax
    cmp $2, %eax
    jne fail

    ud2

    mov sigcnt(%rip), %eax
    cmp $3, %eax
    jne fail

    /* write(1, msg_ok, 14) */
    mov $1, %eax
    mov $1, %edi
    lea msg_ok(%rip), %rsi
    mov $14, %edx
    syscall

    /* exit_group(0) */
    mov $231, %eax
    xor %edi, %edi
    syscall

fail:
    /* write(1, msg_bad, 12) */
    mov $1, %eax
    mov $1, %edi
    lea msg_bad(%rip), %rsi
    mov $12, %edx
    syscall
    mov $231, %eax
    mov $1, %edi
    syscall
