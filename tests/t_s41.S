# t_s41.S — дифференциальный тест ЛЕГАСИ-форм SSE4.1/4.2/AES/PCLMUL emu
# (§7.3 run #106: фиксы (A) ptest 0dst_2src, (C) legacy-стейджинг —
# неявный первый источник V = DST, (C-2) blendv-индексация).
# Запуск: нативно (AVX-хост = эталон) vs pass -target core2 — побайтно.
# Все кейсы — легаси-формы: на профиле core2 идут через emu (inline-стаб
# или generic), нативно на AVX-хосте исполняются железом.
# Вывод: на каждый кейс 16Б xmm0 + 4Б rflags. Хвост не сравнивается —
# только файл целиком (cmp).
#
# Покрытие по классам фикса:
#  legacy_vw (S1=D, S2=srcs[0]): pcmpgtq pcmpeqq pmulld pmuldq packusdw
#    pmin/pmax-семья aesenc/dec/enclast/declast blendps/pd palignr
#    dpps/dppd mpsadbw roundss/sd insertps pclmulqdq
#  W-класс (S1=srcs[0]): roundps/pd phminposuw pmovsx/zx aeskeygenassist
#    aesimc
#  blendv (маска XMM0): pblendvb blendvps blendvpd
#  0dst_2src (флаги): ptest (reg и mem-формы)
.text
.global _start
_start:
    lea buf(%rip), %rbx
    mov $0, %r15d

    /* пресеты */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    movups msk(%rip), %xmm0

    /* ── 1-2: ptest (0dst_2src — флаги и есть результат) ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    ptest  %xmm2, %xmm1
    call dump
    movups t0(%rip), %xmm1
    ptest  msk(%rip), %xmm1          /* mem-форма (rip-относительная) */
    call dump

    /* ── 3-4: pcmpgtq (destructive [W,V] — порядок источников!) ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pcmpgtq %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pcmpgtq msk(%rip), %xmm1         /* mem-W */
    movups %xmm1, %xmm0
    call dump

    /* ── 5-6: pcmpeqq ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pcmpeqq %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pcmpeqq msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 7-8: pmulld / pmuldq ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pmulld %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pmuldq %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 9-10: packusdw (половины от разных источников!) ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    packusdw %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    packusdw msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 11-18: pmin/pmax семья (symmetric, но проверяем оба порядка) ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pminub %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmaxub %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pminuw %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmaxuw %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pminud %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmaxud %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pminsb %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmaxsb %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 19-20: pminsd/pmaxsd ── */
    movups t0(%rip), %xmm1
    pminsd %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmaxsd msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 21-22: blendps/pd (imm-форма [W,imm,V] — V=DST!) ── */
    movups t0(%rip), %xmm1
    blendps $0xA5, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    blendpd $0x0A, msk(%rip), %xmm1  /* mem-W */
    movups %xmm1, %xmm0
    call dump

    /* ── 23-24: palignr (конкатенация V:W >> imm — порядок критичен) ── */
    movups t0(%rip), %xmm1
    palignr $0x08, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    palignr $0x20, msk(%rip), %xmm1  /* imm > 16 — весь результат из W */
    movups %xmm1, %xmm0
    call dump

    /* ── 25-26: dpps/dppd ── */
    movups t0(%rip), %xmm1
    dpps $0x71, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    dppd $0x31, msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 27-28: mpsadbw (imm-смещения по V и W раздельно!) ── */
    movups t0(%rip), %xmm1
    mpsadbw $0x44, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    mpsadbw $0x04, msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 29-32: roundps/pd/ss/sd (roundss/sd: merge верхней части из DST) ── */
    movups t0(%rip), %xmm1
    roundps $0x2, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    roundpd $0x1, msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1          /* верх xmm1 обязан сохраниться */
    roundss $0x2, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    roundsd $0x3, msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 33: insertps (imm-форма, V=DST) ── */
    movups t0(%rip), %xmm1
    insertps $0x13, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 34-36: blendv-семья (маска = XMM0!) ── */
    movups msk(%rip), %xmm0         /* маска — знак-элементы */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pblendvb %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups msk(%rip), %xmm0
    movups t0(%rip), %xmm1
    blendvps %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups msk(%rip), %xmm0
    movups t0(%rip), %xmm1
    blendvpd msk(%rip), %xmm1       /* mem-W + маска XMM0 */
    movups %xmm1, %xmm0
    call dump

    /* ── 37-39: phminposuw / pmovsx / pmovzx (W-класс) ── */
    movups t0(%rip), %xmm1
    phminposuw %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups msk(%rip), %xmm1
    pmovsxbw %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pmovzxbd msk(%rip), %xmm0       /* mem-W */
    call dump

    /* ── 40-44: AES (состояние в DST, ключ W — SWAP-класс) ── */
    movups kstate(%rip), %xmm1
    movups kkey(%rip), %xmm2
    aesenc %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups kstate(%rip), %xmm1
    aesdec kkey(%rip), %xmm1        /* mem-ключ */
    movups %xmm1, %xmm0
    call dump
    movups kstate(%rip), %xmm1
    aesenclast %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups kstate(%rip), %xmm1
    aesdeclast kkey(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump
    movups kkey(%rip), %xmm1
    aesimc %xmm2, %xmm1             /* unary: W-класс */
    movups %xmm1, %xmm0
    call dump

    /* ── 45: aeskeygenassist ── */
    movups kkey(%rip), %xmm1
    aeskeygenassist $0x3, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── 46-49: pclmulqdq (imm выбирает половины V и W!) ── */
    movups t0(%rip), %xmm1
    movups t1(%rip), %xmm2
    pclmulqdq $0x00, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pclmulqdq $0x11, %xmm2, %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pclmulqdq $0x01, kkey(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump
    movups t0(%rip), %xmm1
    pclmulqdq $0x10, msk(%rip), %xmm1
    movups %xmm1, %xmm0
    call dump

    /* ── вывод ── */
    mov $1, %eax
    mov $1, %edi
    lea buf(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

/* dump: 16Б xmm0 + 4Б rflags */
dump:
    movups %xmm0, 0(%rbx,%r15,1)
    add $16, %r15d
    pushfq
    pop %rax
    mov %eax, 0(%rbx,%r15,1)
    add $4, %r15d
    ret

.section .rodata
.align 16
t0:  .quad 0x8877665544332211, 0xDEADBEEFCAFEBABE, 0x0123456789ABCDEF, 0x80000000FFFFFF7F
t1:  .quad 0x1122334455667788, 0x1234567890ABCDEF, 0xFE00000000000001, 0x00000000FF807FFF
msk: .quad 0x8001800280048008, 0x8000C00000008000, 0x0000FFFF0000FFFF, 0x8000000080000000
kstate: .quad 0x0011223344556677, 0x8899AABBCCDDEEFF, 0x0102030405060708, 0x090A0B0C0D0E0F10
kkey:   .quad 0x2B7E151628AED2A6, 0xABF7158809CF4F3C, 0xF0E1D2C3B4A59687, 0x0011223344556677

.section .bss
.align 16
buf: .space 4096
