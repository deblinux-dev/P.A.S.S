# t_fam.S — дифференциальный тест семей, починенных фаззером карт
# рерайтера (§7.3, fault#1 — некодируемые стабы inline-пути):
#   1) vunpcklps/pd с mem-источником  — DR-канон mem-операнда unpckl* —
#      OPSZ_8 (ISA читает младшие 8 байт); gs16-слот клампается
#   2) vpblendw / vpalignr / vcmpps   — legacy = 1dst_3src(d, src, imm, dst)
#      (было 1d2s — некодируемо; run #94 ловил vpblendw 256)
#   3) vsqrtsd / vcvtsi2sd / vcvtsd2ss / vcvtss2sd — скалярная семья:
#      legacy = 1dst_1src; была нечаянно выброшена из карты
#      незакрытым комментарием (bd0b846), восстановлена
#   4) vcvtss2si 3-форма (AVX merge)  — конвертируемый операнд = src[1]
#   5) vunpcklps алиасинг (s2 == dst) — стадия в RAW_T0 + кламп
# Результат каждого кейса хешируется (байты + eflags) и выводится;
# native vs PASS — побайтное совпадение (см. run_all.sh).
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d               /* индекс вывода */

    /* ── 1: vunpcklpd %xmm2, mem[16] -> %xmm1 (mem-SRC, фаззер op644) ── */
    lea pd_tab(%rip), %rax
    movdqu (%rax), %xmm1        /* dst-вход (верх читается из входа) */
    movdqu 16(%rax), %xmm2      /* s1-вход */
    vunpcklpd 32(%rax), %xmm2, %xmm1   /* mem[16] = m_tab */
    movaps %xmm1, %xmm0
    call hash_xmm

    /* ── 2: vunpcklps %xmm2, mem[16] -> %xmm1 ── */
    lea ps_tab(%rip), %rax
    movdqu (%rax), %xmm1
    movdqu 16(%rax), %xmm2
    vunpcklps 32(%rax), %xmm2, %xmm1
    movaps %xmm1, %xmm0
    call hash_xmm

    /* ── 3: vunpcklps алиасинг s2 == dst ──
     * семантика: dst = interleave(dst, s1): после — s1-dword'ы в нечётных */
    lea ps_tab(%rip), %rax
    movdqu (%rax), %xmm1        /* dst == s2 (алиасинг) */
    movdqu 16(%rax), %xmm2
    vunpcklps %xmm1, %xmm2, %xmm1
    movaps %xmm1, %xmm0
    call hash_xmm

    /* ── 4: vpblendw $imm — 1dst_3src семья ── */
    lea ps_tab(%rip), %rax
    movdqu (%rax), %xmm1
    movdqu 16(%rax), %xmm2
    vpblendw $0xF0, %xmm2, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 5: vpalignr $imm — concat(src:dst) >> imm, 1d3s ── */
    lea ps_tab(%rip), %rax
    movdqu (%rax), %xmm1        /* младшая часть concat (dst-вход) */
    movdqu 16(%rax), %xmm2      /* старшая часть */
    vpalignr $4, %xmm2, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 6: vcmpps $imm, 2-форма (merge: dst = cmp(dst, src)) ── */
    lea ps_tab(%rip), %rax
    movdqu (%rax), %xmm1
    movdqu 16(%rax), %xmm2
    vcmpps $1, %xmm2, %xmm1, %xmm1     /* LT: dst < src ? all-ones : 0 */
    movaps %xmm1, %xmm0
    call hash_xmm

    /* ── 7: vcmpps $imm, 3-форма (dst = cmp(s1, s2)) ──
     * NB: hash_xmm затирает %rax (pushfq/pop) — каждый кейс со
     * ссылкой через %rax обязан перезагружать lea.                */
    lea ps_tab(%rip), %rax
    movdqu 16(%rax), %xmm1
    movdqu (%rax), %xmm2
    vcmpps $7, %xmm1, %xmm2, %xmm3     /* ord_q: TRUE ONLY if ordered */
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 8: vsqrtsd merge: верх от s1, низ = sqrt(s2) ── */
    lea pd_tab(%rip), %rax
    movdqu (%rax), %xmm1
    movq 48(%rax), %xmm2        /* s2 = 4.0 → sqrt = 2.0 */
    vsqrtsd %xmm2, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 9: vcvtsi2sd %eax (32-бит gpr → double, merge от s1) ── */
    movdqu pd_tab(%rip), %xmm1
    mov $-12345, %eax
    vcvtsi2sd %eax, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 10: vcvtsd2ss (double → float, merge от s1) ── */
    movdqu pd_tab(%rip), %xmm1
    movq pd_tab+32(%rip), %xmm2
    vcvtsd2ss %xmm2, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 11: vcvtss2sd (float → double, merge от dst, 2-форма VEX) ── */
    movdqu ps_tab(%rip), %xmm1
    vmovss ps_tab+32(%rip), %xmm2
    vcvtss2sd %xmm2, %xmm3, %xmm3      /* dst=xmm3, merge=xmm3, src=xmm2 */
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── 12: vcvtss2si VEX 2-форма (vvvv = merge src, конвертируем src) ──
     * GAS: vcvtss2si %xmm3, %r8d → VEX c5 7a 2d: dst=r8d, s0=xmm0 (merge),
     * s1=xmm3 (конвертируем). Прежний ND2-код брал s1=src[0]=merge —
     * конвертировал НЕ ТОТ операнд; фикс: операнд = src[1].           */
    movdqu pd_tab(%rip), %xmm0  /* merge-мусор в xmm0 (отличим от src) */
    vmovss ps_tab+36(%rip), %xmm3      /* src = 10.0f → cvt → 10 */
    vcvtss2si %xmm3, %r8d       /* → 32-бит int (round-to-nearest) */
    movq %r8, %xmm0
    call hash_xmm

    /* ── 13: vpsrldq $imm (Shift imm-семья: (dst, imm, src)) ── */
    movdqu ps_tab(%rip), %xmm1
    vpsrldq $6, %xmm1, %xmm3
    movaps %xmm3, %xmm0
    call hash_xmm

    /* ── вывод ── */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall

    mov $60, %eax
    xor %edi, %edi
    syscall

/* hash_xmm: 8 байт xmm0 + eflags → 4 байта в буфер rbx[r15d..] */
hash_xmm:
    pushfq
    pop %rax
    movq %xmm0, %rcx
    xor %rax, %rcx
    mov %ecx, %r8d
    shr $32, %rcx
    xor %ecx, %r8d
    mov %r8d, 0(%rbx,%r15,1)
    add $4, %r15d
    ret

.section .rodata
.align 16
pd_tab:
    .quad 0x3FF0000000000000    /* 1.0 — dst/s1-входы */
    .quad 0x4008000000000000    /* 3.0 */
    .quad 0x4010000000000000    /* 4.0 */
    .quad 0x4000000000000000    /* 2.0 */
    .quad 0x4022000000000000    /* 9.0 — mem-источник для unpcklpd */
    .quad 0xC00C000000000000    /* -3.5 */
    .quad 0x4010000000000000    /* 4.0 — sqrt-аргумент */
    .quad 0xC060000000000000    /* -128.0 */
.align 16
ps_tab:
    .long 0x3FC00000            /* 1.5f — dst-вход */
    .long 0x4048F5C3            /* 3.14f */
    .long 0xBF800000            /* -1.0f */
    .long 0x41200000            /* 10.0f */
    .long 0x42200000            /* 40.0f — mem-источник */
    .long 0xC1700000            /* -15.0f */
    .long 0x41200000            /* 10.0f — vcvtss2si src[1] → 10 */
    .long 0xBF4CCCCD            /* -0.8f */
.section .bss
msg:
    .space 256
