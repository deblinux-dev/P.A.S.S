.text
.global _start
_start:
    vpxor %ymm1, %ymm1, %ymm1
    /* память: 0x80 в байтах 3 и 20 */
    lea buf(%rip), %rax
    movl $0, (%rax)
    movl $0, 4(%rax)
    movl $0, 8(%rax)
    movl $0, 12(%rax)
    movl $0, 16(%rax)
    movl $0, 20(%rax)
    movl $0, 24(%rax)
    movl $0, 28(%rax)
    movb $0x80, 3(%rax)
    movb $0x80, 20(%rax)
    vmovdqu (%rax), %xmm0
    vinserti128 $0, %xmm0, %ymm1, %ymm1
    vmovdqu 16(%rax), %xmm0
    vinserti128 $1, %xmm0, %ymm1, %ymm1
    stc
    vpmovmskb %ymm1, %edi
    /* edi = 0x00100008 ожидается */
    mov $8, %ecx
    lea out(%rip), %rsi
    add $7, %rsi
1:  mov %edi, %edx
    and $0xF, %edx
    movb hex(%rdx), %al
    movb %al, (%rsi)
    shr $4, %edi
    dec %rsi
    loop 1b
    mov $1, %eax
    mov $1, %edi
    lea out(%rip), %rsi
    mov $8, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.bss
.lcomm buf, 32
.lcomm out, 16
.data
hex: .ascii "0123456789abcdef"
