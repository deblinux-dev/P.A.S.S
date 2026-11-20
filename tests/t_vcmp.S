# t_vcmp.S — §7.5: дифференциальный тест скалярных сравнений
# VCMPSS/VCMPSD. Матрица: 12 пар значений × 32 предиката imm8 ×
# форма reg/mem, отдельно для double и float.
# Вывод: после каждой операции полный DEST (16Б) — маска в lane0 и
# верх из SRC1 (маркер 0xC3 в SRC1[127:64]; старый DEST забит мусором
# 0x77 — проверка, что верх берётся из SRC1, а не из старого DEST).
# Native vs PASS — вывод обязан совпасть побайтно.
# История: vcmpss/sd отсутствовали в эмуляции (§7.3 убрал из
# inline-карты, generic-маршрут не был создан) — left-native на CPU
# без AVX давал #UD и trap-цикл с exit 86 (bun.exe на Windows).
.text
.global _start
_start:
    lea msg(%rip), %rbx
    xor %r15d, %r15d                 /* байтовый offset вывода */

    /* ── double: 12 пар × 32 предиката × (reg-форма + mem-форма m64) ──
     * r13d = индекс ЭЛЕМЕНТА таблицы (шаг 2 на пару: масштаб SIB ≤ 8) */
    lea pdtab(%rip), %r14
    xor %r13d, %r13d
pd_loop:
    movq (%r14,%r13,8), %xmm0        /* SRC1 = a */
    movhps mark(%rip), %xmm0         /* SRC1[127:64] = маркер 0xC3 */
    movq 8(%r14,%r13,8), %xmm1       /* SRC2 = b (reg-форма) */
    movhps mark2(%rip), %xmm1        /* верх SRC2 — не должен протечь */
    .irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    movdqu garb(%rip), %xmm2         /* мусор в старом DEST */
    vcmpsd $\i, %xmm1, %xmm0, %xmm2  /* DEST = cmp(SRC1=xmm0, SRC2=xmm1) */
    movdqu %xmm2, (%rbx,%r15,1)
    add $16, %r15d
    movdqu garb(%rip), %xmm2
    vcmpsd $\i, 8(%r14,%r13,8), %xmm0, %xmm2   /* mem-форма: SRC2 = m64 */
    movdqu %xmm2, (%rbx,%r15,1)
    add $16, %r15d
    .endr
    add $2, %r13d
    cmp $NPD, %r13d
    jb pd_loop

    /* ── float: те же 12 пар в 32-битной точности (m32-форма) ── */
    lea pstab(%rip), %r14
    xor %r13d, %r13d
ps_loop:
    movd (%r14,%r13,8), %xmm0        /* SRC1 = a (float) */
    movhps mark(%rip), %xmm0         /* SRC1[127:32] = маркер */
    movd 4(%r14,%r13,8), %xmm1       /* SRC2 = b */
    movhps mark2(%rip), %xmm1
    .irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    movdqu garb(%rip), %xmm2
    vcmpss $\i, %xmm1, %xmm0, %xmm2
    movdqu %xmm2, (%rbx,%r15,1)
    add $16, %r15d
    movdqu garb(%rip), %xmm2
    vcmpss $\i, 4(%r14,%r13,8), %xmm0, %xmm2   /* mem-форма: SRC2 = m32 */
    movdqu %xmm2, (%rbx,%r15,1)
    add $16, %r15d
    .endr
    inc %r13d
    cmp $NP, %r13d
    jb ps_loop

    /* вывод: r15d байт; выход 0 */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15, %rdx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

.section .rodata
.set NP, 12
.set NPD, 24                      /* NP*2: double-индекс элемента */
.align 16
mark:  .quad 0xC3C3C3C3C3C3C3C3       /* верх SRC1 */
mark2: .quad 0x6969696969696969       /* верх SRC2 (в DEST не попадает) */
garb:  .quad 0x7777777777777777, 0x7777777777777777   /* прежний DEST */

/* пары double: eq / lt / gt / ±0 / QNaN / SNaN / ±inf / denorm */
pdtab:
    .quad 0x3FF8000000000000, 0x3FF8000000000000   /* 1.5, 1.5 */
    .quad 0x3FF8000000000000, 0x4004000000000000   /* 1.5, 2.5 */
    .quad 0x4004000000000000, 0x3FF8000000000000   /* 2.5, 1.5 */
    .quad 0x0000000000000000, 0x8000000000000000   /* +0, -0 */
    .quad 0x8000000000000000, 0x0000000000000000   /* -0, +0 */
    .quad 0x7FF8000000000000, 0x3FF0000000000000   /* QNaN, 1.0 */
    .quad 0x3FF0000000000000, 0x7FF8000000000000   /* 1.0, QNaN */
    .quad 0x7FF0000000000001, 0x3FF0000000000000   /* SNaN, 1.0 */
    .quad 0x7FF0000000000000, 0x3FF0000000000000   /* +inf, 1.0 */
    .quad 0xFFF0000000000000, 0x3FF0000000000000   /* -inf, 1.0 */
    .quad 0x7FF0000000000000, 0xFFF0000000000000   /* +inf, -inf */
    .quad 0x0000000000000001, 0x0000000000000000   /* denorm, 0.0 */

/* те же пары в float */
pstab:
    .long 0x3FC00000, 0x3FC00000                   /* 1.5f, 1.5f */
    .long 0x3FC00000, 0x40200000                   /* 1.5f, 2.5f */
    .long 0x40200000, 0x3FC00000
    .long 0x00000000, 0x80000000                   /* +0, -0 */
    .long 0x80000000, 0x00000000
    .long 0x7FC00000, 0x3F800000                   /* QNaN, 1.0f */
    .long 0x3F800000, 0x7FC00000
    .long 0x7F800001, 0x3F800000                   /* SNaN, 1.0f */
    .long 0x7F800000, 0x3F800000                   /* +inf */
    .long 0xFF800000, 0x3F800000                   /* -inf */
    .long 0x7F800000, 0xFF800000
    .long 0x00000001, 0x00000000                   /* denorm, 0.0 */

.section .bss
.align 32
msg:  .space 32768
