# t_istr.S — дифференциальный тест строковой SSE4.2 (§7.4 раунд #111).
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
    /* case: pcmpestri imm=0x00 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=1 edx=1 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $1, %eax
    mov $1, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=15 edx=2 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $15, %eax
    mov $2, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=16 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $3, %edx
    pcmpestri $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x00 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x10 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x40 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x50 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x04 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x14 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x44 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestri imm=0x54 eax=3 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $3, %edx
    pcmpestri $0x54, %xmm2, %xmm1
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
    /* case: pcmpestrm imm=0x04 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x14 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x44 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x54 eax=0 edx=0 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $0, %eax
    mov $0, %edx
    pcmpestrm $0x54, %xmm2, %xmm1
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
    /* case: pcmpestrm imm=0x04 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x14 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x44 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x54 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    pcmpestrm $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x00 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x10 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x40 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x50 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x04 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x14 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x44 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x54 eax=16 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $16, %eax
    mov $16, %edx
    pcmpestrm $0x54, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x00 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x10 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x40 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x50 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x04 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x04, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x14 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x14, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x44 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x44, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: pcmpestrm imm=0x54 eax=3 edx=16 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $3, %eax
    mov $16, %edx
    pcmpestrm $0x54, %xmm2, %xmm1
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
    /* case: vpcmpestri imm=0x10 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x10, %xmm2, %xmm1
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
    /* case: vpcmpestri imm=0x50 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestri $0x50, %xmm2, %xmm1
    pushfq
    pop %rax
    movl %ecx, (%rbx,%r14,1)
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x00 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x00, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x10 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x10, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x40 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x40, %xmm2, %xmm1
    pushfq
    pop %rax
    movl $0x5A5A5A5A, (%rbx,%r14,1)   /* ECX undefined у strm */
    mov %rax, 4(%rbx,%r14,1)
    add $12, %r14
    movdqu %xmm0, (%rbx,%r14,1)
    add $16, %r14
    /* case: vpcmpistrm imm=0x50 eax=7 edx=9 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $7, %eax
    mov $9, %edx
    vpcmpistrm $0x50, %xmm2, %xmm1
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
    /* case: vpcmpestrm imm=0x10 eax=5 edx=3 byte */
    mov $0xA5A5A5A5, %edi
    movd %edi, %xmm0
    vpinsrd $1, %edi, %xmm0, %xmm0
    vpinsrd $2, %edi, %xmm0, %xmm0
    vpinsrd $3, %edi, %xmm0, %xmm0
    movdqa ab(%rip), %xmm1
    movdqa bb(%rip), %xmm2
    mov $5, %eax
    mov $3, %edx
    vpcmpestrm $0x10, %xmm2, %xmm1
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
out: .space 5056
