# t_sh256.S — дифференциальный тест 256-бит imm-сдвигов (ловушка
# UNENCODABLE run #96, bun-старт: vpsrld $0x10 %ymm0 -> %ymm0).
# DR моделирует VEX imm-сдвиги как (dst, imm, src) — вектор в s2;
# прежний 256-скелет грузил hi(s1) = hi(imm) → movups xmm, $imm.
# Фикс: ранний SHIFT-imm путь (hi/lo из s2, алиасинг dst==src —
# стадия обеих половин до записей). Кейсы: vpsrld/vpsrldq/vpslldq/
# vpsrlw imm, алиасинг и без. Хеши lo+hi половин + eflags.
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d
    /* vpsrld $0x10 ymm0 -> ymm0 (точно кейс из run #96) */
    vmovdqu ytab(%rip), %ymm0
    vpsrld $0x10, %ymm0, %ymm0
    vmovdqu %ymm0, %ymm0
    call hash_ylo
    call hash_yhi
    /* vpsrld $0x10 ymm1 -> ymm3 (без алиасинга) */
    vmovdqu ytab(%rip), %ymm1
    vpsrld $0x10, %ymm1, %ymm3
    vmovdqu %ymm3, %ymm0
    call hash_ylo
    call hash_yhi
    /* vpsrldq $1 ymm0 -> ymm2 */
    vmovdqu ytab+32(%rip), %ymm0
    vpsrldq $1, %ymm0, %ymm2
    vmovdqu %ymm2, %ymm0
    call hash_ylo
    call hash_yhi
    /* vpslldq $8 ymm0 -> ymm0 (алиасинг dst) */
    vmovdqu ytab+64(%rip), %ymm0
    vpslldq $8, %ymm0, %ymm0
    call hash_ylo
    call hash_yhi
    /* vpsrlw $4 ymm1 -> ymm4 (w-сдвиг) */
    vmovdqu ytab(%rip), %ymm1
    vpsrlw $4, %ymm1, %ymm4
    vmovdqu %ymm4, %ymm0
    call hash_ylo
    call hash_yhi
    /* вывод */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
/* hash младшей половины (xmm0) */
hash_ylo:
    pushfq
    pop %rax
    movq %xmm0, %rcx
    xor %rax, %rcx
    mov %ecx, %r9d
    shr $32, %rcx
    xor %ecx, %r9d
    mov %r9d, 0(%rbx,%r15,1)
    add $4, %r15d
    ret
/* hash старшей половины: vextractf128 $1, ymm0 -> xmm0, затем как ylo */
hash_yhi:
    pushfq
    pop %rax
    vextractf128 $1, %ymm0, %xmm5
    movq %xmm5, %rcx
    xor %rax, %rcx
    mov %ecx, %r9d
    shr $32, %rcx
    xor %ecx, %r9d
    mov %r9d, 0(%rbx,%r15,1)
    add $4, %r15d
    ret
.section .rodata
.align 32
ytab:
    .quad 0x1122334455667788, 0x99AABBCCDDEEFF00, 0x0102030405060708, 0x090A0B0C0D0E0F10
    .quad 0xDEADBEEFCAFEBABE, 0x1234567890ABCDEF, 0xFEEDFACE0DDC0FFE, 0x7F1D3B9A2C5E7086
    .quad 0xFFFFFFFF00000000, 0x00000000FFFFFFFF, 0x8000000000000001, 0x00000000000000FF
.section .bss
msg:
    .space 256
