# t_store.S — проверка пути VEX.256 store/load (VF_DIR) и vpbroadcastb.
# ymm0 = 0xFF×32 → store в buf; ymm1 = 0 → store в buf+32;
# load из buf → ymm2 → store в buf+64. Байтовая сверка.
# Код выхода: бит0 — байты 0..31 != 0xFF; бит1 — 32..63 != 0;
#             бит2 — 64..95 != 0xFF (сквозной load/store);
#             бит3 — 96..127 != 0.
.text
.global _start
_start:
    lea buf(%rip), %rdi
    vpxor %xmm0, %xmm0, %xmm0
    vpcmpeqb %xmm0, %xmm0, %xmm0    /* xmm0 = 0xFF×16 */
    vpbroadcastb %xmm0, %ymm0       /* ymm0 = 0xFF×32 (bcast) */
    vmovdqu %ymm0, (%rdi)           /* store 256 (VF_DIR store) */
    vpxor %ymm1, %ymm1, %ymm1
    vmovdqu %ymm1, 32(%rdi)
    vmovdqu (%rdi), %ymm2           /* load 256 (VF_DIR load) */
    vmovdqu %ymm2, 64(%rdi)
    vpxor %xmm3, %xmm3, %xmm3
    vmovdqu %xmm3, 96(%rdi)

    xor %r12d, %r12d
    xor %ecx, %ecx
1:  movzbl (%rdi,%rcx,1), %eax
    cmp $32, %ecx
    jl 2f
    cmp $64, %ecx
    jl 3f
    cmp $96, %ecx
    jl 2f
    /* 96..127: д.б. 0 */
    test %eax, %eax
    jz 3f
    or  $8, %r12d
    jmp 3f
2:  /* 0..31 и 64..95: д.б. 0xFF */
    cmp $0xFF, %eax
    je 3f
    or  $1, %r12d
3:  inc %ecx
    cmp $128, %ecx
    jb 1b

    mov $60, %eax
    mov %r12d, %edi
    syscall
.bss
.lcomm buf, 128
