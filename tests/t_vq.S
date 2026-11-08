.text
.global _start
_start:
    vpcmpeqb %xmm0, %xmm0, %xmm0
    vmovq %xmm0, %rax
    mov $0xFFFFFFFFFFFFFFFF, %r11
    cmp %r11, %rax
    je 1f
    mov $1, %edi
    jmp 2f
1:  xor %edi, %edi
2:  mov $60, %eax
    syscall
