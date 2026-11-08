# t_lztz.S — дифференциальный тест LZCNT/TZCNT: матрица значений × ширины,
# хеш (результат, eflags) пишется в stdout 4 байтами. Native vs PASS —
# вывод обязан совпасть побайтно.
.text
.global _start
_start:
    lea msg(%rip), %rbx         /* буфер вывода */
    mov $0, %r15d               /* индекс строки вывода */

    /* регистры-пресеты: значения источника кладём в r8 */
    lea table(%rip), %r14
    xor %r13d, %r13d            /* индекс значения */
val_loop:
    mov (%r14,%r13,8), %r8      /* исходное 64-битное значение */

    /* ── 64-битные ── */
    mov %r8, %rax
    .byte 0xF3,0x48,0x0F,0xBC,0xC0          /* tzcnt %rax,%rax */
    call emit
    mov %r8, %rax
    .byte 0xF3,0x48,0x0F,0xBD,0xC0          /* lzcnt %rax,%rax */
    call emit

    /* ── 32-битные (источник = младшие 32) ── */
    mov %r8d, %eax
    .byte 0xF3,0x0F,0xBC,0xC0               /* tzcnt %eax,%eax */
    call emit
    mov %r8d, %eax
    .byte 0xF3,0x0F,0xBD,0xC0               /* lzcnt %eax,%eax */
    call emit

    /* ── 16-битные ── */
    mov %r8w, %ax
    .byte 0x66,0xF3,0x0F,0xBC,0xC0          /* tzcnt %ax,%ax */
    call emit
    mov %r8w, %ax
    .byte 0x66,0xF3,0x0F,0xBD,0xC0          /* lzcnt %ax,%ax */
    call emit

    inc %r13d
    cmp $NVAL, %r13d
    jb val_loop

    /* вывод: 132 байта */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov $132, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

/* emit: rax = результат; писать 3 байта: val[7:0], val[15:8], флаги */
emit:
    pushf
    pop %r11                    /* флаги после инструкции */
    mov %rax, %r10
    mov %r15d, %ecx
    mov %r10b, (%rbx,%rcx,1)
    inc %ecx
    mov %r10w, %dx
    mov %dl, (%rbx,%rcx,1)
    inc %ecx
    mov %r11b, %dl
    and $0xF5, %dl              /* SF,ZF,AF(0),PF,CF — AF у нас всегда 0 */
    mov %dl, (%rbx,%rcx,1)
    add $3, %r15d
    ret

.set NVAL, 11
.data
table:
    .quad 0x0
    .quad 0x1
    .quad 0xFFFFFFFFFFFFFFFF
    .quad 0x8000000000000000
    .quad 0x0000000000001000
    .quad 0x00000000FFFF0000
    .quad 0x8000000080000000
    .quad 0x00000000000FFFFF
    .quad 0x00000000FFFFFFFF
    .quad 0x7FFFFFFFFFFFFFFF
    .quad 0xFFFFFFFFFFFF0000
.bss
.lcomm msg, 132
