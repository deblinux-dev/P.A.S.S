# t_istr.S — дифференциальный тест строковой SSE4.2 (§7.4 раунд #112).
# Сгенерирован scripts/gen_t_istr.py — НЕ править руками.
# native vs pass побайтно: ECX + EFLAGS + XMM0 на каждый кейс.
# XMM0 засеен 0xA5-паттерном: stri не трогает, strm перезаписывает.
.text
.global _start
_start:
    lea out(%rip), %rbx
    xor %r14d, %r14d              /* байтовое смещение вывода */
    /* пресет: XMM0 = мусор (детектор лишних чтений/записей XMM0) */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    /* case: pcmpistri imm=0x00 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x10 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x40 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x50 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x04 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x14 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x44 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x54 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x08 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x08, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x18 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x18, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x48 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x48, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x58 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x58, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x0c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x0c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x1c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x1c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x4c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x4c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x5c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x5c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x20 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x20, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x24 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x24, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x28 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x28, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x2c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x2c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x30 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x34 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x38 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x38, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x3c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x60 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x60, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x64 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x64, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x68 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x68, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x6c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x6c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x70 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x74 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x78 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x78, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x7c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x1a eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x1a, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x3a eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x3a, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x12 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x12, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x01 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x01, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x11 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x11, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x41 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x41, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x51 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x51, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x05 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x05, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x15 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x15, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x45 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x45, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x55 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x55, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x09 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x09, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x19 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x19, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x49 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x49, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x59 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x59, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x0d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x0d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x1d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x1d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x4d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x4d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x5d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x5d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x21 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x21, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x25 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x25, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x29 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x29, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x2d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x2d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x31 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x31, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x35 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x35, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x39 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x39, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x3d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x3d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x61 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x61, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x65 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x65, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x69 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x69, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x6d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x6d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x71 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x71, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x75 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x75, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x79 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x79, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x7d eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x7d, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x1b eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x1b, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x3b eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x3b, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x13 eax=7 edx=9 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x13, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x00 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x10 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x40 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x50 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x04 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x14 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x44 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x54 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x08 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x08, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x18 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x18, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x48 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x48, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x58 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x58, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x0c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x0c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x1c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x1c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x4c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x4c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x5c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x5c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x20 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x20, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x24 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x24, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x28 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x28, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x2c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x2c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x30 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x34 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x38 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x38, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x3c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x60 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x60, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x64 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x64, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x68 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x68, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x6c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x6c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x70 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x74 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x78 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x78, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x7c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x1a eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x1a, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x3a eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x3a, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistrm imm=0x12 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistrm $0x12, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x30 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x34 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x3c eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x70 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x74 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x7c eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x30 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x34 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x3c eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x70 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x74 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x7c eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x30 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x34 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x3c eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x70 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x74 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x7c eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x30 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x34 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x3c eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x70 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x70, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x74 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x74, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x7c eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x7c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x00 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x10 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x40 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x50 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x30 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x34 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x38 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x38, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x3c eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x00 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x10 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x40 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x50 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x30 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x34 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x38 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x38, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x3c eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x01 eax=0 edx=0 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x01, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x11 eax=0 edx=0 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x11, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x41 eax=0 edx=0 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x41, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x51 eax=0 edx=0 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestri $0x51, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x01 eax=5 edx=3 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x01, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x11 eax=5 edx=3 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x11, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x41 eax=5 edx=3 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x41, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x51 eax=5 edx=3 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestri $0x51, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x01 eax=16 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x01, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x11 eax=16 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x11, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x41 eax=16 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x41, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x51 eax=16 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestri $0x51, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x01 eax=3 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x01, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x11 eax=3 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x11, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x41 eax=3 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x41, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x51 eax=3 edx=16 word */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa aw(%rip), %xmm1
    movdqa bw(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestri $0x51, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x06 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x06, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x16 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x16, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x46 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x46, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x26 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x26, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x36 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x36, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x66 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x66, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x76 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x76, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x07 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x07, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x17 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x17, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpistri imm=0x47 eax=7 edx=9 byte neg */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa an(%rip), %xmm1
    movdqa bn(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    pcmpistri $0x47, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x00 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x10 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x40 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x50 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x04 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x14 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x44 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x34 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x3a eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x3a, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x08 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x08, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x18 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x18, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x48 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x48, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x38 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x38, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x0c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x0c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x1c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x1c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistri imm=0x3c eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestri imm=0x00 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestri imm=0x40 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestri imm=0x04 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestri imm=0x34 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestri imm=0x3c eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x3c, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x04 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x44 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x30 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x34 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x34, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestrm imm=0x00 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpestrm imm=0x30 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestrm $0x30, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* вывод */
    mov $1, %eax
    mov $1, %edi
    lea out(%rip), %rsi
    mov %r14d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

.section .rodata
.align 16
.align 16
ab: .ascii "ABCDEZYXWVUTSRQ0"
.align 16
bb: .ascii "CDE\0FGHIJKLMNOP"
.align 16
aw: .word 65, 66, 67, 68, 69, 90, 89, 88
bw: .word 67, 68, 69, 0, 70, 71, 72, 73
.align 16
an: .byte 0x80, 0x01, 0xfe, 0x7f, 0x00, 0x40, 0xc0, 0x20, 0x81, 0x7f, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06
bn: .byte 0xfe, 0xff, 0x00, 0x7f, 0x80, 0x01, 0x7f, 0x00, 0xfe, 0xff, 0x02, 0x80, 0x10, 0x20, 0x30, 0x40
.section .bss
.align 16
out: .space 6456
