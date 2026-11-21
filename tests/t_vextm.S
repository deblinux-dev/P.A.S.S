# t_vextm.S — дифференциальный тест VEXTRACTF128/VEXTRACTI128 с ПАМЯТЬЮ-
# приёмником (B-2006: bun 1.2.14 -e — JIT-спил ymm в память падал в
# ловушку UNENCODABLE: спил/восстановление xmm15 кодировался как OP_mov
# с xmm-операндом вместо movups).
# Формы: vextracti128 imm=1/imm=0, vextractf128 imm=1, mem-приёмник,
# источник ymm15 (ветка s_is_15 — спил не выполняется). После каждой
# операции хешируем буфер-приёмник И xmm15 (спил/восстановление обязан
# сохранять xmm15 побайтно). Native vs PASS — вывод совпадает побайтно.
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d

    /* 1. vextracti128 $1, %ymm1 -> mem (жирная форма из bun-JIT) */
    vmovdqu t256(%rip), %ymm1
    vextracti128 $1, %ymm1, buf(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash
    vmovdqu %xmm15, %xmm0
    call hash

    /* 2. vextracti128 $0, %ymm1 -> mem (младшая половина) */
    vmovdqu t256b(%rip), %ymm1
    vextracti128 $0, %ymm1, buf(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash

    /* 3. vextractf128 $1, %ymm2 -> mem */
    vmovdqu t256(%rip), %ymm2
    vextractf128 $1, %ymm2, buf(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash

    /* 4. источник ymm15: спил не нужен, но результат тот же */
    vmovdqu t256b(%rip), %ymm15
    vextracti128 $1, %ymm15, buf(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash
    vmovdqu %xmm15, %xmm0
    call hash

    /* 5. повторный вызов на ту же память (перезапись) */
    vmovdqu t256(%rip), %ymm3
    vextracti128 $1, %ymm3, buf(%rip)
    vextractf128 $0, %ymm3, buf+16(%rip)
    vmovdqu buf(%rip), %xmm0
    call hash
    vmovdqu buf+16(%rip), %xmm0
    call hash

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
t256:   .quad 0x1111111111111111, 0x2222222222222222
        .quad 0x3333333333333333, 0x4444444444444444
        .quad 0x5555555555555555, 0x6666666666666666
        .quad 0x7777777777777777, 0x88CCDDEEFF001234
t256b:  .quad 0x9E3779B97F4A7C15, 0xBF58476D1CE4E5B9
        .quad 0x94D049BB133111EB, 0xCAFEBABEDEADBEEF
        .quad 0x0123456789ABCDEF, 0xFEDCBA9876543210
        .quad 0xA5A5A5A5A5A5A5A5, 0x5C5C5C5C5C5C5C5C
.section .bss
.align 32
buf:    .space 64
msg:
    .space 512
