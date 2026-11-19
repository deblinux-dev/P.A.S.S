# t_bcast.S — дифференциальный тест broadcast-семьи (ловушка дыры
# generic-пути: под -nobcast исполнение должно совпадать с нативным).
# Семья: vbroadcastss/sd/f128/i128, vpbroadcastb/w/d/q — mem и reg
# источники, xmm/ymm приёмники, алиасинг dst==src. VEX.128-форма
# обязана обнулять верх YMM (инвариант shadow), VEX.256 — дублировать.
# Хеш lo+hi половин + eflags пишется в stdout 8 байтами на вызов.
# Native vs PASS — вывод обязан совпасть побайтно (и в базовом, и в
# -nobcast режиме).
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d

    /* 1. vbroadcastss m32 -> xmm1 (верх = 0) */
    vmovdqu tq0(%rip), %xmm1
    vbroadcastss tm32(%rip), %xmm1
    vmovdqu %xmm1, %xmm0
    call hash
    vextractf128 $1, %ymm1, %xmm0
    call hash

    /* 2. vbroadcastss m32 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vbroadcastss tm32(%rip), %ymm0
    call hashlo
    call hashhi

    /* 3. vbroadcastss xmm2 -> xmm1 (reg-источник: младший dword) */
    vmovdqu tq0(%rip), %xmm2
    vbroadcastss %xmm2, %xmm1
    vmovdqu %xmm1, %xmm0
    call hash
    vextractf128 $1, %ymm1, %xmm0
    call hash

    /* 4. vbroadcastss xmm2 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vbroadcastss %xmm2, %ymm0
    call hashlo
    call hashhi

    /* 5. vbroadcastsd m64 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vbroadcastsd tm64(%rip), %ymm0
    call hashlo
    call hashhi

    /* 6. vbroadcastsd xmm3 -> ymm0 (младший qword xmm3) */
    vmovdqu tq1(%rip), %xmm3
    vmovdqu t128(%rip), %ymm0
    vbroadcastsd %xmm3, %ymm0
    call hashlo
    call hashhi

    /* 7. vbroadcastf128 m128 -> ymm0 */
    vmovdqu tq0(%rip), %ymm0
    vbroadcastf128 t128(%rip), %ymm0
    call hashlo
    call hashhi

    /* 8. vbroadcasti128 m128 -> ymm0 */
    vmovdqu tq0(%rip), %ymm0
    vbroadcasti128 t128(%rip), %ymm0
    call hashlo
    call hashhi

    /* 9. vpbroadcastb m8 -> xmm1 (верх = 0) */
    vmovdqu tq0(%rip), %xmm1
    vpbroadcastb tm8(%rip), %xmm1
    vmovdqu %xmm1, %xmm0
    call hash
    vextractf128 $1, %ymm1, %xmm0
    call hash

    /* 10. vpbroadcastb xmm2 -> ymm0 (младший байт tq0 = 0x11) */
    vmovdqu t128(%rip), %ymm0
    vpbroadcastb %xmm2, %ymm0
    call hashlo
    call hashhi

    /* 11. vpbroadcastw m16 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vpbroadcastw tm16(%rip), %ymm0
    call hashlo
    call hashhi

    /* 12. vpbroadcastd m32 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vpbroadcastd tm32(%rip), %ymm0
    call hashlo
    call hashhi

    /* 13. vpbroadcastq m64 -> xmm1 (верх = 0) */
    vmovdqu tq0(%rip), %xmm1
    vpbroadcastq tm64(%rip), %xmm1
    vmovdqu %xmm1, %xmm0
    call hash
    vextractf128 $1, %ymm1, %xmm0
    call hash

    /* 14. vpbroadcastq xmm4 -> ymm0 */
    vmovdqu tq0(%rip), %xmm4
    vmovdqu t128(%rip), %ymm0
    vpbroadcastq %xmm4, %ymm0
    call hashlo
    call hashhi

    /* 15. алиасинг: vbroadcastss xmm4, xmm4 */
    vmovdqu tq1(%rip), %xmm4
    vbroadcastss %xmm4, %xmm4
    vmovdqu %xmm4, %xmm0
    call hash
    vextractf128 $1, %ymm4, %xmm0
    call hash

    /* 16. алиасинг: vpbroadcastd ymm5, xmm5 (верх ymm5 предзаполнен) */
    vmovdqu tq1(%rip), %xmm5
    vinsertf128 $1, t128(%rip), %ymm5, %ymm5
    vpbroadcastd %xmm5, %ymm5
    vmovdqu %ymm5, %ymm0
    call hashlo
    call hashhi

    /* 17. vpbroadcastb m8 -> ymm0 */
    vmovdqu t128(%rip), %ymm0
    vpbroadcastb tm8(%rip), %ymm0
    call hashlo
    call hashhi

    /* 18. алиасинг ymm: vbroadcastss xmm4 -> ymm4 (верх предзаполнен) */
    vmovdqu tq1(%rip), %xmm4
    vinsertf128 $1, t128(%rip), %ymm4, %ymm4
    vbroadcastss %xmm4, %ymm4
    vmovdqu %ymm4, %ymm0
    call hashlo
    call hashhi

    /* вывод: r15 байт */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

/* hash: eflags ^ low64(xmm0), 8 байт в msg */
hash:
hashlo:
hashhi:
    pushfq
    pop %rax
    movq %xmm0, %rcx
    xor %rax, %rcx
    mov %ecx, 0(%rbx,%r15,1)
    shr $32, %rcx
    mov %ecx, 4(%rbx,%r15,1)
    add $8, %r15d
    ret

.section .rodata
.align 32
tq0:    .quad 0x8877665544332211
tq1:    .quad 0xBBAAAAAA00000064
tm32:   .long 0xCAFEBABE
tm64:   .quad 0xDEADBEEF12345678
t128:   .quad 0x1122334455667788, 0x99AABBCCDDEEFF00
tm8:    .byte 0x5A
tm16:   .word 0x3C3C
.section .bss
msg:
    .space 512
