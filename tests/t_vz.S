.text
.global _start
_start:
    /* заполняем ymm3 данными через нашу модель (vpbroadcastb и т.п.) */
    lea buf(%rip), %rax
    movl $0, (%rax)
    movl $0, 4(%rax)
    movl $0, 8(%rax)
    movl $0, 12(%rax)
    movb $0x5A, (%rax)
    vmovdqu (%rax), %xmm0
    vpxor %ymm1, %ymm1, %ymm1
    vinserti128 $0, %xmm0, %ymm1, %ymm3
    /* hi-половина через vinserti128 $1 */
    movl $0x77, 16(%rax)
    vmovdqu 16(%rax), %xmm0
    vinserti128 $1, %xmm0, %ymm3, %ymm3
    /* vzeroupper — эмулируемая */
    vzeroupper
    /* запись результата: buf был перезаписан? нет — просто exit */
    mov $60, %eax
    xor %edi, %edi
    syscall
.bss
.lcomm buf, 32
