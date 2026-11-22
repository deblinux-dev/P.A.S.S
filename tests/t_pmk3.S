# t_pmk3.S — B-2014: воспроизведение паттерна bun require():
#   vmovdqu (данные) → vpcmpeqb ymm (inline VF_NDS) → vpmovmskb (special-танец)
# Формы bun: si=0..3, dst=eax/ecx/edx/r8d/r9d, VEX.128, повторные вызовы.
# Вывод: 56 байт quad'ов → 112 hex-символов; побайтный дифф native vs pass.
.text
.global _start
_start:
    lea data(%rip), %rbx
    lea out(%rip), %r8

    /* ── форма A: si=1 dst=eax (доминирующая у bun, 31 повтор) ── */
    vmovdqu (%rbx), %ymm0
    vmovdqu 32(%rbx), %ymm2
    vpcmpeqb %ymm2, %ymm0, %ymm1
    vpmovmskb %ymm1, %eax
    mov %rax, (%r8)
    add $8, %r8

    /* повтор сразу — повторное исполнение танца в одном bb */
    vpcmpeqb %ymm2, %ymm0, %ymm1
    vpmovmskb %ymm1, %eax
    mov %rax, (%r8)
    add $8, %r8

    /* ── форма B: si=3 dst=ecx, stc перед pmovmskb ── */
    vmovdqu 64(%rbx), %ymm3
    vpcmpeqb %ymm2, %ymm3, %ymm3
    stc
    vpmovmskb %ymm3, %ecx
    mov %rcx, (%r8)
    add $8, %r8

    /* ── форма C: si=2 dst=edx; результат vpcmpeqb ПИШЕТ ymm2 (needle),
        затем vzeroupper, затем pmovmskb по «свежему» ymm2 ── */
    vmovdqu 96(%rbx), %ymm4
    vpcmpeqb %ymm2, %ymm4, %ymm2
    vzeroupper
    vpmovmskb %ymm2, %edx
    mov %rdx, (%r8)
    add $8, %r8

    /* ── форма D: si=0 dst=eax, rax нагружен флаговым образом (pushf/pop) ── */
    vmovdqu 128(%rbx), %ymm0
    vpcmpeqb %ymm2, %ymm0, %ymm0
    pushf
    pop %rax
    vpmovmskb %ymm0, %eax
    mov %rax, (%r8)
    add $8, %r8

    /* ── форма E: si=4/5 dst=r8d/r9d (редкие dst, у bun r8d) ── */
    vmovdqu 160(%rbx), %ymm5
    vpcmpeqb %ymm5, %ymm3, %ymm6
    vpmovmskb %ymm6, %r10d
    mov %r10, (%r8)
    add $8, %r8
    vpcmpeqb %ymm5, %ymm3, %ymm1
    vpmovmskb %ymm1, %r9d
    mov %r9, (%r8)
    add $8, %r8

    /* ── форма F: VEX.128 pmovmskb (xmm) после 256-битной работы ── */
    vpcmpeqb %xmm2, %xmm0, %xmm6
    vpmovmskb %xmm6, %esi
    mov %rsi, (%r8)
    add $8, %r8

    /* ── форма G: pmovmskb ПОСЛЕ vmovdqu-загрузки без сравнения (hi-инвариант) */
    vmovdqu 192(%rbx), %ymm7
    vpmovmskb %ymm7, %eax
    mov %rax, (%r8)
    add $8, %r8

    /* ── печать out (56 байт → 112 hex) ── */
    lea out(%rip), %r8
    lea hexbuf(%rip), %r9
    mov $72, %r10
hloop:
    movzbl (%r8), %eax
    lea hextab(%rip), %rcx
    add %rax, %rax             /* смещение = байт*2 */
    add %rcx, %rax
    movzwl (%rax), %eax
    mov %ax, (%r9)
    add $1, %r8
    add $2, %r9
    dec %r10
    jnz hloop
    mov $1, %eax
    mov $1, %edi
    lea hexbuf(%rip), %rsi
    mov $144, %edx
    syscall

    mov $60, %eax
    xor %edi, %edi
    syscall

.bss
.lcomm out, 128
.lcomm hexbuf, 128
.data
hextab:
    .ascii "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
    .ascii "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f"
    .ascii "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f"
    .ascii "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f"
    .ascii "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
    .ascii "a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
    .ascii "c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
    .ascii "e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff"
data:
    /* блок 0 (ymm0, A/D): 0xff на позициях 1,5,17,25 */
    .byte 0x00,0xff,0x00,0x00, 0x00,0xff,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0xff,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0xff,0x00,0x00, 0x00,0x00,0x00,0x00
    /* блок 1 (needle, ymm2): 0xff на 1,5,17,29 → маска A ≠ all-ones */
    .byte 0x00,0xff,0x00,0x00, 0x00,0xff,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0xff,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0xff,0x00,0x00
    /* блок 2 (форма B, ymm3): 0xff на 0,2,28 → маска B = 0x10000005 */
    .byte 0xff,0x00,0xff,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0xff, 0x00,0x00,0x00,0x00
    /* блок 3 (форма C, ymm4): 0xff на 4,19 → маска C = 0x00080010 */
    .byte 0x00,0x00,0x00,0x00, 0xff,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0xff, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    /* блок 4 (форма E, ymm5): 0xff на 8,31 */
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0xff,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0xff
    /* блок 5 (форма G, ymm7): 0xff на 3,20 */
    .byte 0x00,0x00,0x00,0xff, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0xff, 0x00,0x00,0x00,0x00
    .byte 0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00
