.text
.global _start
_start:
    mov $0x1234567890ABCDEF, %rax
    vmovq %rax, %xmm0           /* GPR → XMM */
    vmovq %xmm0, %rdx           /* XMM → GPR */
    cmp %rax, %rdx
    je 1f
    mov $1, %edi
    jmp 2f
1:  xor %edi, %edi
2:  mov $60, %eax
    syscall
