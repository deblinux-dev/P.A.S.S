# t_pmk4.S — B-2014: широкая батарея паттернов bun (маски/сторы/тень).
# Кейсы: mem-источники vpcmpeqb, алиасинг, rbp-негативные disp32,
# disp8→disp32 граница, ymm8-15, перемежение с tzcnt/popcnt (clean call),
# vzeroupper. Побайтный дифф native vs pass.
.text
.global _start
_start:
    lea data(%rip), %rbx
    lea out(%rip), %r8

    push %rbp
    mov %rsp, %rbp
    sub $0x800, %rsp

    /* ── C1: mem-источник vpcmpeqb (rip-rel) + pmovmskb eax ── */
    vpcmpeqb (%rbx), %ymm1, %ymm0
    vpmovmskb %ymm0, %eax
    mov %rax, (%r8)
    add $8, %r8

    /* ── C2: mem-источник (rbp-neg disp32) + алиас dst==s1 ── */
    vmovdqu 32(%rbx), %ymm2
    vmovdqu %ymm2, -0x5b0(%rbp)
    vmovdqu %ymm2, -0x590(%rbp)
    vpcmpeqb %ymm2, %ymm2, %ymm3          /* алиас: dst==s1==s2 */
    vpmovmskb %ymm3, %ecx
    mov %rcx, (%r8)
    add $8, %r8

    /* ── C3: возврат из стека (rbp-neg) → pmovmskb → store в стек ── */
    vmovdqu -0x5b0(%rbp), %ymm4
    vmovdqu -0x590(%rbp), %ymm5
    vpcmpeqb %ymm5, %ymm4, %ymm6
    vpmovmskb %ymm6, %edx
    mov %rdx, (%r8)
    add $8, %r8
    vmovdqu %ymm6, -0x570(%rbp)

    /* ── C4: disp8→disp32 граница (store на 0x70(%rbp) → hi на 0x80) ── */
    vmovdqu 64(%rbx), %ymm7
    vmovdqu %ymm7, -0x70(%rbp)            /* hi уйдёт на -0x60 */
    vmovdqu -0x70(%rbp), %ymm8            /* ymm8-15! */
    vpmovmskb %ymm8, %r9d
    mov %r9, (%r8)
    add $8, %r8

    /* ── C5: ymm8-15 полная цепочка + tzcnt/popcnt между (clean call) ── */
    vmovdqu 96(%rbx), %ymm9
    vmovdqu 128(%rbx), %ymm10
    vpcmpeqb %ymm10, %ymm9, %ymm11
    mov $0x12345678, %r12d
    tzcnt %r12d, %r13d
    popcnt %r12d, %r14d
    vpmovmskb %ymm11, %r11d
    vmovdqu %ymm11, -0x550(%rbp)
    vmovdqu -0x550(%rbp), %ymm12
    vpmovmskb %ymm12, %r15d
    mov %r11, (%r8)
    add $8, %r8
    mov %r15, (%r8)
    add $8, %r8
    mov %r13, (%r8)
    add $8, %r8
    mov %r14, (%r8)
    add $8, %r8

    /* ── C6: vzeroupper между store и load (hi-инвариант) ── */
    vmovdqu 160(%rbx), %ymm13
    vmovdqu %ymm13, -0x530(%rbp)
    vzeroupper
    vmovdqu -0x530(%rbp), %ymm14
    vpmovmskb %ymm14, %eax
    vmovdqu -0x530(%rbp), %xmm15          /* только lo */
    vpmovmskb %xmm15, %ecx
    mov %rax, (%r8)
    add $8, %r8
    mov %rcx, (%r8)
    add $8, %r8

    /* ── C7: перекрытие store→load по смещению 8 (unaligned) ── */
    vmovdqu 192(%rbx), %ymm0
    vmovdqu %ymm0, -0x510(%rbp)
    vmovdqu -0x508(%rbp), %ymm1           /* +8 от store */
    vpcmpeqb %ymm1, %ymm0, %ymm2
    vpmovmskb %ymm2, %esi
    mov %rsi, (%r8)
    add $8, %r8

    /* ── C8: pmovmskb r64-имя приёмника (rdi) + повторные маски ── */
    vpcmpeqb %ymm2, %ymm0, %ymm3
    vpmovmskb %ymm3, %rdi
    vpmovmskb %ymm3, %eax
    mov %rdi, (%r8)
    add $8, %r8
    mov %rax, (%r8)
    add $8, %r8

    /* ── C9: алиас dst==s1 c mem-источником ── */
    vpcmpeqb 224(%rbx), %ymm4, %ymm4
    vpmovmskb %ymm4, %r10d
    mov %r10, (%r8)
    add $8, %r8

    add $0x800, %rsp
    pop %rbp

    /* печать out (14 quad'ов = 112 байт → 224 hex) */
    lea out(%rip), %r8
    lea hexbuf(%rip), %r9
    mov $112, %r10
hloop:
    movzbl (%r8), %eax
    lea hextab(%rip), %rcx
    add %rax, %rax
    add %rcx, %rax
    movzbl (%rax), %edx
    movzbl 1(%rax), %esi
    mov %dl, (%r9)
    mov %sil, 1(%r9)
    add $1, %r8
    add $2, %r9
    dec %r10
    jnz hloop
    mov $1, %eax
    mov $1, %edi
    lea hexbuf(%rip), %rsi
    mov $224, %edx
    syscall

    mov $60, %eax
    xor %edi, %edi
    syscall

.bss
.lcomm out, 160
.lcomm hexbuf, 256
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
    .quad 0x0102030405060708, 0x090a0b0c0d0e0f10, 0x1112131415161718, 0x191a1b1c1d1e1f20
    .quad 0x2122232425262728, 0x292a2b2c2d2e2f30, 0x3132333435363738, 0x393a3b3c3d3e3f40
    .quad 0x4142434445464748, 0x494a4b4c4d4e4f50, 0x5152535455565758, 0x595a5b5c5d5e5f60
    .quad 0x6162636465666768, 0x696a6b6c6d6e6f70, 0x7172737475767778, 0x797a7b7c7d7e7f80
    .quad 0x8182838485868788, 0x898a8b8c8d8e8f90, 0x9192939495969798, 0x999a9b9c9d9e9fa0
    .quad 0xa1a2a3a4a5a6a7a8, 0xa9aaabacadaeafb0, 0xb1b2b3b4b5b6b7b8, 0xb9babbbcbdbebfc0
    .quad 0xc1c2c3c4c5c6c7c8, 0xc9cacbcccdcecfd0, 0xd1d2d3d4d5d6d7d8, 0xd9dadbdcdddedfe0
    .quad 0xe1e2e3e4e5e6e7e8, 0xe9eaebecedeeeff0, 0xf1f2f3f4f5f6f7f8, 0xf9fafbfcfdfeff00
