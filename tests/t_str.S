# t_str.S — точная реплика цикла strchrnul_avx2_rtm.
# Ищем '%' (0x25) в "a%b..." — ожидаемый индекс 1.
# Также верифицируем каждый шаг: pcmp/por/pmovmskb/tzcnt.
# Код выхода: бит0 — индекс != 1; бит1 — маска != 0x000A (как у native).
.text
.global _start
_start:
    lea buf(%rip), %rdi
    movb $'a', (%rdi)
    movb $'%', 1(%rdi)
    movb $'b', 2(%rdi)
    movb $0,   3(%rdi)
    /* заполним весь блок 32 детерминированным хвостом */
    mov $4, %ecx
1:  movb $'z', (%rdi,%rcx,1)
    inc %ecx
    cmp $32, %ecx
    jb 1b

    mov $0x25, %esi
    vmovd %esi, %xmm0
    vpbroadcastb %xmm0, %ymm0
    vpxor %xmm1, %xmm1, %xmm1

    xor %r12d, %r12d

    vmovdqu (%rdi), %ymm2
    vpcmpeqb %ymm2, %ymm0, %ymm3      /* char-match */
    vpcmpeqb %ymm2, %ymm1, %ymm2      /* zero-match */
    vpor %ymm3, %ymm2, %ymm3
    vpmovmskb %ymm3, %eax
    cmp $0x000A, %eax                 /* биты 1 и 3: '%' и '\0' */
    je 2f
    or  $2, %r12d
2:  tzcnt %eax, %eax
    cmp $1, %eax
    je 3f
    or  $1, %r12d
3:
    mov $60, %eax
    mov %r12d, %edi
    syscall
.bss
.lcomm buf, 64
