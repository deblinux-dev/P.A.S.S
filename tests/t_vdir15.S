# t_vdir15.S — дифференциальный тест VF_DIR (vmovdqa/vmovdqu) во всех
# комбинациях с xmm15/ymm15: носитель клиента обязан сохранять app-значение
# и переносить lo+hi без искажений (B-2006/B-2007 regression).
# src==ymm15 (B-2007: lo читается ДО затирания носителем), dst==ymm15,
# обычные reg-reg, no-op, store ymm15->mem, load mem->ymm15.
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d

    /* 1. src==ymm15: ymm2 <- ymm15 (верх ymm2 предзаполнен мусором) */
    vmovdqu tB(%rip), %ymm2
    vmovdqu tA(%rip), %ymm15
    vmovdqa %ymm15, %ymm2
    vmovdqu %xmm2, %xmm0
    call hash                     # lo ymm2
    vextractf128 $1, %ymm2, %xmm0
    call hash                     # hi ymm2
    vmovdqu %xmm15, %xmm0
    call hash                     # xmm15 инвариант (= tA.lo)

    /* 2. dst==ymm15: ymm15 <- ymm1 */
    vmovdqu tB(%rip), %ymm1
    vmovdqa %ymm1, %ymm15
    vmovdqu %xmm15, %xmm0
    call hash                     # lo ymm15 = tB.lo
    vextractf128 $1, %ymm15, %xmm0
    call hash                     # hi ymm15 = tB.hi

    /* 3. обычный reg-reg: ymm4 <- ymm3 (верх ymm4 предзаполнен) */
    vmovdqu tB(%rip), %ymm4
    vmovdqu tC(%rip), %ymm3
    vmovdqa %ymm3, %ymm4
    vmovdqu %xmm4, %xmm0
    call hash
    vextractf128 $1, %ymm4, %xmm0
    call hash
    vmovdqu %xmm15, %xmm0
    call hash                     # инвариант

    /* 4. no-op: ymm2 <- ymm2 */
    vmovdqa %ymm2, %ymm2
    vmovdqu %xmm2, %xmm0
    call hash

    /* 5. store ymm15 -> mem: буфер == tB, xmm15 не тронут */
    vmovdqu tB(%rip), %ymm15
    vmovdqu %ymm15, buf(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash
    vmovdqu buf+16(%rip), %xmm0
    call hash
    vmovdqu %xmm15, %xmm0
    call hash

    /* 6. load mem -> ymm15 */
    vmovdqu tC(%rip), %ymm5
    vmovdqu %ymm5, buf(%rip)
    vmovdqu buf(%rip), %ymm15
    vmovdqu %xmm15, %xmm0
    call hash
    vextractf128 $1, %ymm15, %xmm0
    call hash

    /* вывод */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

hash:
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
tA:     .quad 0x11AA11AA11AA11AA, 0x22BB22BB22BB22BB
        .quad 0x33CC33CC33CC33CC, 0x44DD44DD44DD44DD
        .quad 0x55EE55EE55EE55EE, 0x66FF66FF66FF66FF
        .quad 0x7700770077007700, 0x8811881188118811
tB:     .quad 0x9E3779B97F4A7C15, 0xBF58476D1CE4E5B9
        .quad 0x94D049BB133111EB, 0xCAFEBABEDEADBEEF
        .quad 0x0123456789ABCDEF, 0xFEDCBA9876543210
        .quad 0xA5A5A5A5A5A5A5A5, 0x5C5C5C5C5C5C5C5C
tC:     .quad 0x123456789ABCDEF0, 0x0FEDCBA987654321
        .quad 0xDEADBEEFCAFEBABE, 0xAAAAAAAA55555555
        .quad 0x1111222233334444, 0x5555666677778888
        .quad 0x9999AAAABBBBCCCC, 0xDDDDEEEEFFFF0000
.section .bss
.align 32
buf:    .space 64
msg:
    .space 512
