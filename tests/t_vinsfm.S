# t_vinsfm.S — дифференциальный тест VINSERTF128/VINSERTI128 с алиасингом
# s2 == младший регистр приёмника (B-2027: натурный корень спина bun 1.4.5).
#
# bun-овский memset (bun+0x5245730) материализует fill-значение цепочкой
#   mov $val,%rcx; movabs $0x0101..,%rax; imul %rcx,%rax   (броадкаст байта)
#   movq %rax,%xmm0; punpcklqdq %xmm0,%xmm0                (FF×16 в lo)
#   vinsertf128 $1, %xmm0, %ymm0, %ymm0                    (FF×32)
# B-2012 guard s2i != di ПРОПУСКАЛ запись YHI при алиасе s2==dst.lo —
# hi оставался stale (0 после vzeroupper / 128-битной записи) → fill
# писал (FF,0) в старшие 16Б каждого 32Б-стора индекса ArrayHashMap →
# SIMD group-scan не находил ни одного 0xFF → вечный спин
# (bun+0x1756040 / bun+0x2c40070 / repopulate bun+0x52dcc09).
#
# Кейсы: (1) точная bun-цепочка + 4×32B стора @0/32/64/96;
# (2) s2==dst.lo при s1!=dst (вариант с кlobber'ом s2 записью lo);
# (3) все три операнда алиасированы; (4) imm=0 алиас (регрессия:
# guard s1i != di там семантически корректен — hi не меняется);
# (5) mem-источник s2 (носитель xmm15) + целостность xmm15;
# (6) vinserti128 алиас; (7) fill 48Б (len=32: main 32 + зеркало 16)
# в порядке хвост→голова, как реальный цикл.
# Native vs PASS — вывод совпадает побайтно.
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d

    /* ── 1. ТОЧНАЯ bun-цепочка fill-значения + 4 стора 32Б ── */
    mov $0xFF, %ecx
    movabs $0x0101010101010101, %rax
    imul %rcx, %rax                    # rax = FF×8
    movq %rax, %xmm0                   # lo = FF×8 (128-бит запись)
    punpcklqdq %xmm0, %xmm0            # xmm0 = FF×16
    vzeroupper                         # YHI(0) := 0 (инвариант PASS)
    vinsertf128 $1, %xmm0, %ymm0, %ymm0   # Intel: ymm0,ymm0,xmm0,1
    vmovdqu %ymm0, buf(%rip)
    vmovdqu %ymm0, buf+32(%rip)
    vmovdqu %ymm0, buf+64(%rip)
    vmovdqu %ymm0, buf+96(%rip)
    vmovdqu buf(%rip), %xmm0           # хеш @0    (FF×16)
    call hash
    vmovdqu buf+16(%rip), %xmm0        # хеш @16   (pre-fix: НОЛИ — дамп SPINW)
    call hash
    vmovdqu buf+48(%rip), %xmm0        # хеш @48   (второй стор)
    call hash
    vmovdqu buf+96(%rip), %xmm0        # хеш @96   (четвёртый стор)
    call hash

    /* ── 2. s2==dst.lo при s1!=dst: vinsertf128 ymm5,ymm3,xmm5,1 ── */
    vmovdqu patB(%rip), %ymm5          # YHI(5) := patB.hi (детерминированный stale)
    vmovdqu patA(%rip), %ymm3          # s1
    vmovdqu patS(%rip), %xmm5          # s2 == dst.lo
    vinsertf128 $1, %xmm5, %ymm3, %ymm5
    vmovdqu %ymm5, buf(%rip)
    vmovdqu buf(%rip), %xmm0           # lo = patA.lo
    call hash
    vmovdqu buf+16(%rip), %xmm0        # hi = patS (pre-fix: stale patB.hi/0)
    call hash

    /* ── 3. все три алиасированы: vinsertf128 ymm2,ymm2,xmm2,1 ── */
    vmovdqu patC(%rip), %ymm2
    movabs $0xABABABABABABABAB, %rax
    movq %rax, %xmm2                   # xmm2 = AB×8
    vinsertf128 $1, %xmm2, %ymm2, %ymm2
    vmovdqu %ymm2, buf(%rip)
    vmovdqu buf(%rip), %xmm0           # lo = AB×16
    call hash
    vmovdqu buf+16(%rip), %xmm0        # hi = AB×16 (pre-fix: stale)
    call hash

    /* ── 4. imm=0 алиас: vinsertf128 ymm4,ymm4,xmm4,0 (регрессия) ── */
    vmovdqu patD(%rip), %ymm4
    vmovdqu patS2(%rip), %xmm4
    vinsertf128 $0, %xmm4, %ymm4, %ymm4   # lo := xmm4, hi := ymm4.hi (не меняется)
    vmovdqu %ymm4, buf(%rip)
    vmovdqu buf(%rip), %xmm0           # lo = patS2
    call hash
    vmovdqu buf+16(%rip), %xmm0        # hi = patD.hi
    call hash

    /* ── 5. mem-источник: vinsertf128 ymm6,ymm6,m128,1 (носитель xmm15) ── */
    vmovdqu patE(%rip), %ymm6
    vzeroupper
    vinsertf128 $1, m128(%rip), %ymm6, %ymm6
    vmovdqu %ymm6, buf(%rip)
    vmovdqu buf(%rip), %xmm0           # lo = patE.lo
    call hash
    vmovdqu buf+16(%rip), %xmm0        # hi = m128
    call hash
    vmovdqu %xmm15, %xmm0              # целостность xmm15 (носитель обязан спилиться)
    call hash

    /* ── 6. vinserti128 алиас: vinserti128 ymm7,ymm7,xmm7,1 ── */
    vmovdqu patF(%rip), %ymm7
    movabs $0xCDCDCDCDCDCDCDCD, %rax
    movq %rax, %xmm7
    vinserti128 $1, %xmm7, %ymm7, %ymm7
    vmovdqu %ymm7, buf(%rip)
    vmovdqu buf(%rip), %xmm0           # lo = CD×16
    call hash
    vmovdqu buf+16(%rip), %xmm0        # hi = CD×16
    call hash

    /* ── 7. fill 48Б (len=32: main 32 + зеркало 16), хвост→голова ── */
    mov $0xFF, %ecx
    movabs $0x0101010101010101, %rax
    imul %rcx, %rax
    movq %rax, %xmm0
    punpcklqdq %xmm0, %xmm0
    vinsertf128 $1, %xmm0, %ymm0, %ymm0
    vmovdqu %ymm0, buf+32(%rip)        # хвост (32Б стор @32)
    vmovdqu %ymm0, buf(%rip)           # голова
    vmovdqu buf(%rip), %xmm0           # @0    = FF×16
    call hash
    vmovdqu buf+16(%rip), %xmm0        # @16   = FF×16 (pre-fix: НОЛИ)
    call hash
    vmovdqu buf+32(%rip), %xmm0        # @32   = FF×16
    call hash
    vmovdqu buf+48(%rip), %xmm0        # @48   = FF×16 (зеркало)
    call hash

    /* вывод: r15 байт */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

/* hash: eflags ^ low64(xmm0), 8 байт в msg */
hash:
    pushfq
    pop %rax
    movq %xmm0, %rcx
    xor %rax, %rcx
    mov %ecx, 0(%rbx,%r15,1)
    shr $32, %rcx
    mov %ecx, 4(%rbx,%r15,1)
    add $8, %r15d
    ret

.section .rodata
.align 32
patA:   .quad 0x1111111111111111, 0x2222222222222222
        .quad 0x3333333333333333, 0x4444444444444444
patB:   .quad 0x9E3779B97F4A7C15, 0xBF58476D1CE4E5B9
        .quad 0x94D049BB133111EB, 0xCAFEBABEDEADBEEF
patC:   .quad 0x0123456789ABCDEF, 0xFEDCBA9876543210
        .quad 0xA5A5A5A5A5A5A5A5, 0x5C5C5C5C5C5C5C5C
patD:   .quad 0x88CCDDEEFF001234, 0x5555555555555555
        .quad 0x6666666666666666, 0x7777777777777777
patS:   .quad 0xDEADBEEFCAFEBABE, 0x0123456789ABCDEF
patS2:  .quad 0x13579BDF2468ACE0, 0xFFFF0000AAAA5555
patE:   .quad 0xE44E31ACD5F02B77, 0x8BADF00DD15EA5E5
        .quad 0xC0FFEE1234567890, 0xFEEDFACE0DDC0FFE
patF:   .quad 0x7F1D1537760816EB, 0x2545F1A91F5B51DB
        .quad 0xD3AEB15024C7E291, 0x3BC6E9E63E33D8B4
.align 16
m128:   .quad 0xB16B00B5DEADC0DE, 0x8BADF00DBAADF00D
.section .bss
.align 32
buf:    .space 128
msg:
    .space 512
