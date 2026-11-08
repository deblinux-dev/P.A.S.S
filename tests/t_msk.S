# t_msk.S — vpmovmskb ymm: значение, сохранность GPR/флагов, dst≠rax.
# Код выхода: бит0 — T1 значение; бит1 — T1 rax; бит2 — T1 CF;
#             бит3 — T2 значение; бит4 — rdx испорчен.
.text
.global _start
_start:
    xor %r12d, %r12d

    /* ── T1: lo=0xFF, hi=0x00 ── */
    vpcmpeqb %ymm6, %ymm6, %ymm6
    vpxor %xmm7, %xmm7, %xmm7
    vinserti128 $1, %xmm7, %ymm6, %ymm6
    mov $0x1111111111111111, %rax
    mov $0x2222222222222222, %rcx
    mov $0x4444444444444444, %rdx
    stc                                      /* CF=1 */
    vpmovmskb %ymm6, %ecx
    pushf
    pop %r13                                 /* флаги сразу после vpmovmskb */
    cmp $0xFFFF, %ecx
    je 1f
    or  $1, %r12d
1:  movabs $0x1111111111111111, %r14
    cmp %r14, %rax
    je 2f
    or  $2, %r12d                            /* rax испорчен */
2:  test $1, %r13d                           /* CF д.б. 1 */
    jnz 3f
    or  $4, %r12d
3:

    /* ── T2: lo=hi=0xFF ── */
    vpcmpeqb %ymm6, %ymm6, %ymm6
    vpmovmskb %ymm6, %ecx
    movabs $0x4444444444444444, %r14
    cmp %r14, %rdx
    je 4f
    or  $16, %r12d
4:
    mov $60, %eax
    mov %r12d, %edi
    syscall
