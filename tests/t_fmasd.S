# t_fmasd.S — дифференциальный тест двух форм, пойманных ловушкой
# UNENCODABLE на bun (run #93, fault#1):
#   1) vfmadd231sd %xmm2, %xmm0, %xmm0  — VF_NDS, sd-форма, rm==dst
#      (стадия алиасинга: 8-байтовый xmm в RAW_T0; был movups[16] — некодируемо)
#   2) vpsrlq $imm, %xmm0 -> %xmm3      — VF_SHIFT, imm в src[0]: порядок
#      операндов DR (dst, imm, src); был movaps xmm3, $imm — некодируемо
# Результат хешируется (байты результата + eflags) и выводится в stdout
# 4 байтами на кейс: native vs PASS — вывод обязан совпасть побайтно.
.text
.global _start
_start:
    lea msg(%rip), %rbx         /* буфер вывода */
    mov $0, %r15d               /* индекс вывода */

    /* ── группа 1: vfmadd231sd, rm == dst (sd-алиасинг) ──
     * семантика 231: dst = src2*dst + src1 → res = a*a + b */
    lea fma_tab(%rip), %r14
    xor %r13d, %r13d
fma_loop:
    movq (%r14,%r13,8), %rax    /* a → dst и src2 (алиасинг!) */
    movq 8(%r14,%r13,8), %rcx   /* b → src1 */
    movq %rax, %xmm0
    movq %rcx, %xmm2
    vfmadd231sd %xmm0, %xmm2, %xmm0
    call hash_xmm               /* хеш xmm0 + eflags */
    inc %r13d
    cmp $4, %r13d
    jb fma_loop

    /* ── группа 2: vpsrlq $imm — imm в src[0], результат в dst ──
     * развёрнуто: imm — статические байты (5, 64, 1) */
    movq shl_tab+0(%rip), %rax
    movq %rax, %xmm0
    vpsrlq $5, %xmm0, %xmm3
    vmovaps %xmm3, %xmm0
    call hash_xmm

    movq shl_tab+16(%rip), %rax
    movq %rax, %xmm0
    vpsrlq $64, %xmm0, %xmm3    /* полный сдвиг → 0 */
    vmovaps %xmm3, %xmm0
    call hash_xmm

    movq shl_tab+32(%rip), %rax
    movq %rax, %xmm0
    vpsrlq $1, %xmm0, %xmm3
    vmovaps %xmm3, %xmm0
    call hash_xmm

    /* ── вывод ── */
    mov $1, %eax                /* write */
    mov $1, %edi                /* stdout */
    lea msg(%rip), %rsi
    mov %r15d, %edx             /* длина */
    syscall

    mov $60, %eax               /* exit */
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
    xor %ecx, %r8d              /* свёртка 64→32 */
    mov %r8d, 0(%rbx,%r15,1)
    add $4, %r15d
    ret

.section .rodata
.align 8
fma_tab:
    .quad 0x4008000000000000    /* a = 3.0 */
    .quad 0x4010000000000000    /* b = 4.0 → 13.0 */
    .quad 0xC008000000000000    /* a = -3.0 */
    .quad 0x4022000000000000    /* b = 9.0 → 18.0 */
    .quad 0x3FF0000000000000    /* a = 1.0 */
    .quad 0x4000000000000000    /* b = 2.0 → 3.0 */
    .quad 0x7FF8000000000000    /* a = NaN */
    .quad 0x3FF0000000000000    /* b = 1.0 → NaN (+флаги) */
shl_tab:
    .quad 0x89ABCDEF01234567
    .byte 5
    .quad 0xFFFFFFFFFFFFFFFF
    .byte 64                    /* полный сдвиг → 0 */
    .quad 0x0000000080000000
    .byte 1
.section .bss
msg:
    .space 256
