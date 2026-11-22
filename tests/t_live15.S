/* t_live15.S — B-2012: батарея «живые регистры через перезаписанные опы».
 *
 * Все 16 ymm + 12 GPR залиты различными паттернами; каждый кейс исполняет
 * один (семантически осмысленный) переписываемый оп, затем dump() пишет
 * flags + ymm0..15 + GPR в выходной буфер. Нативный прогон vs P.A.S.S.
 * диффится побайтно: любое расхождение = утечка носителя/тени/контекста.
 *
 * Особо: xmm15/ymm15 — наш носитель; кейсы с dst==xmm15 и живым xmm15
 * ловят пропущенный спил (класс «builtin-возвраты undefined» в bun).
 *
 * Запуск: ./t_live15 > out.bin ; drrun -c libpass_client.so -target core2
 *         -- ./t_live15 > out2.bin ; cmp out.bin out2.bin
 */
.text
.global _start

#define DUMPSZ 640

_start:
    lea  msg(%rip), %rbx          /* курсор вывода                      */
    lea  cases(%rip), %r12        /* таблица .quad-указателей           */
    lea  cases_end(%rip), %r13
next_case:
    cmp  %r13, %r12
    je   done
    call reload
    mov  (%r12), %rax
    call *%rax
    add  $8, %r12
    call dump
    /* по-кейсовая запись (diag): write(2, rbx-640, 640) */
    mov  $1, %eax
    mov  $2, %edi
    mov  %rbx, %rsi
    sub  $640, %rsi
    mov  $640, %edx
    syscall
    jmp  next_case
done:
    /* длина = число кейсов * DUMPSZ */
    mov  $CASEN * 640, %edx
    mov  $1, %eax
    mov  $1, %edi
    lea  msg(%rip), %rsi
    syscall
    mov  $60, %eax
    xor  %edi, %edi
    syscall

/* reload: залить ymm0..15 и GPR паттернами */
reload:
    vmovdqu const0(%rip),  %ymm0
    vmovdqu const1(%rip),  %ymm1
    vmovdqu const2(%rip),  %ymm2
    vmovdqu const3(%rip),  %ymm3
    vmovdqu const4(%rip),  %ymm4
    vmovdqu const5(%rip),  %ymm5
    vmovdqu const6(%rip),  %ymm6
    vmovdqu const7(%rip),  %ymm7
    vmovdqu const8(%rip),  %ymm8
    vmovdqu const9(%rip),  %ymm9
    vmovdqu const10(%rip), %ymm10
    vmovdqu const11(%rip), %ymm11
    vmovdqu const12(%rip), %ymm12
    vmovdqu const13(%rip), %ymm13
    vmovdqu const14(%rip), %ymm14
    vmovdqu const15(%rip), %ymm15
    movabs $0x1111111111111111, %rax
    movabs $0x2222222222222222, %rcx
    movabs $0x0000000000000004, %rdx   /* счётчик сдвига для shlx       */
    movabs $0x4444444444444444, %rsi
    movabs $0x0000000000000020, %rdi   /* индекс для bzhi               */
    movabs $0x5555555555555555, %r8
    movabs $0x6666666666666666, %r9
    movabs $0x00000000FFFFFFFF, %r10
    movabs $0x7777777777777777, %r11
    movabs $0x9999999999999999, %r14
    movabs $0xAAAAAAAAAAAAAAAA, %r15
    movabs $0x7F7F7F7F7F7F7F7F, %rbp
    ret

/* dump: flags + ymm0..15 + GPR в (%rbx), rbx += DUMPSZ */
dump:
    pushfq
    pop  %rax
    mov  %rax, 0(%rbx)
    vmovdqu %ymm0,  8(%rbx)
    vmovdqu %ymm1,  40(%rbx)
    vmovdqu %ymm2,  72(%rbx)
    vmovdqu %ymm3,  104(%rbx)
    vmovdqu %ymm4,  136(%rbx)
    vmovdqu %ymm5,  168(%rbx)
    vmovdqu %ymm6,  200(%rbx)
    vmovdqu %ymm7,  232(%rbx)
    vmovdqu %ymm8,  264(%rbx)
    vmovdqu %ymm9,  296(%rbx)
    vmovdqu %ymm10, 328(%rbx)
    vmovdqu %ymm11, 360(%rbx)
    vmovdqu %ymm12, 392(%rbx)
    vmovdqu %ymm13, 424(%rbx)
    vmovdqu %ymm14, 456(%rbx)
    vmovdqu %ymm15, 488(%rbx)
    mov  %rax, 520(%rbx)
    mov  %rcx, 528(%rbx)
    mov  %rdx, 536(%rbx)
    mov  %rsi, 544(%rbx)
    mov  %rdi, 552(%rbx)
    mov  %r8,  560(%rbx)
    mov  %r9,  568(%rbx)
    mov  %r10, 576(%rbx)
    mov  %r11, 584(%rbx)
    mov  %r14, 592(%rbx)
    mov  %r15, 600(%rbx)
    mov  %rbp, 608(%rbx)
    add  $640, %rbx
    ret

/* ── таблица кейсов ── */
.section .rodata
.align 8
cases:
    .quad .c1, .c2, .c3, .c4, .c5, .c6, .c7, .c8, .c9, .c10
    .quad .c11, .c12, .c13, .c14, .c15, .c16, .c17, .c18, .c19, .c20
    .quad .c21, .c22, .c23, .c24, .c25, .c26, .c27, .c28, .c29, .c30
    .quad .c31, .c32, .c33, .c34, .c35, .c36, .c37, .c38, .c39, .c40
    .quad .c41, .c42, .c43, .c44, .c45, .c46, .c47, .c48, .c49, .c50
    .quad .c51, .c52, .c53, .c54, .c55, .c56, .c57, .c58, .c59, .c60
    .quad .c61, .c62, .c63, .c64, .c65, .c66, .c67, .c68, .c69, .c70
    .quad .c71, .c72, .c73, .c74, .c75, .c76, .c77, .c78, .c79, .c80
    .quad .c81, .c82, .c83, .c84, .c85
    .quad .c86, .c87, .c88
cases_end:

.set CASEN, (cases_end-cases)/8

/* ── кейсы: каждый — одна инструкция поверх залитых паттернов ── */
.text
.c1:   vmovups %ymm2, %ymm1
       ret
.c2:   vmovups tmem(%rip), %ymm3
       ret
.c3:   vmovups %ymm4, wmem(%rip)
       vmovups wmem(%rip), %ymm5
       ret
.c4:   vmovdqu %ymm6, %ymm7
       ret
.c5:   vmovdqa %ymm8, %ymm9
       ret
.c6:   vmovaps tmem(%rip), %ymm10
       ret
.c7:   vzeroupper
       ret
.c8:   vxorps %ymm11, %ymm12, %ymm13
       ret
.c9:   vpxor  %ymm14, %ymm15, %ymm0        /* dst=c0, живой ymm15         */
       ret
.c10:  vpcmpeqb %ymm1, %ymm15, %ymm2      /* dst=c2, живой ymm15         */
       ret
.c11:  vpmovmskb %ymm15, %eax             /* GPR-приёмник из ymm15       */
       ret
.c12:  vpinsrb $3, %eax, %xmm1, %xmm2
       ret
.c13:  vpextrd $1, %xmm3, %edx
       ret
.c14:  vpshufb %ymm4, %ymm5, %ymm6
       ret
.c15:  vpshufd $0x1B, %ymm7, %ymm8
       ret
.c16:  vpermilps %ymm9, %ymm10, %ymm11
       ret
.c17:  vinserti128 $1, %xmm12, %ymm13, %ymm14
       ret
.c18:  vextracti128 $1, %ymm15, %xmm0
       ret
.c19:  vbroadcastss %xmm1, %ymm2
       ret
.c20:  vblendvps %ymm3, %ymm4, %ymm5, %ymm6
       ret
.c21:  vfmadd231ps %ymm7, %ymm8, %ymm9
       ret
.c22:  vaddps %ymm10, %ymm11, %ymm12
       ret
.c23:  vmulps %ymm13, %ymm14, %ymm15
       ret
.c24:  vpsubd %ymm0, %ymm1, %ymm2
       ret
.c25:  vpalignr $5, %ymm3, %ymm4, %ymm5
       ret
.c26:  vpslldq $3, %ymm6, %ymm7
       ret
.c27:  vpsrldq $9, %ymm8, %ymm9
       ret
.c28:  vpmovsxbd %xmm10, %ymm11
       ret
.c29:  vpackssdw %ymm12, %ymm13, %ymm14
       ret
.c30:  vmovq %rax, %xmm15
       ret
.c31:  vmovq %xmm15, %rax
       ret
.c32:  vroundsd $9, %xmm1, %xmm2, %xmm3
       ret
.c33:  vmovlps tmem(%rip), %xmm4, %xmm5
       ret
.c34:  vpbroadcastb %xmm6, %ymm7
       ret
.c35:  vptest %ymm8, %ymm9
       ret
.c36:  vpsraw $2, %ymm10, %ymm11
       ret
.c37:  vunpcklpd %ymm12, %ymm13, %ymm14
       ret
.c38:  tzcnt %r9d, %r10d
       ret
.c39:  mulx  %r8d, %r9d, %r10d           /* r9:r10 = rdx * r8d (rdx=4)   */
       ret
.c40:  shlx  %edx, %r14d, %r15d          /* r15 = r14 << (rdx&31), rdx=4 */
       ret
.c41:  bzhi  %edi, %esi, %ebp
       ret
.c42:  pdep  %ebp, %edi, %esi
       ret
.c43:  pext  %esi, %edi, %ebp
       ret
.c44:  andn  %r10d, %r11d, %r14d
       ret
.c45:  popcnt %r15d, %r8d
       ret
.c46:  lzcnt %r9d, %r15d
       ret
.c47:  crc32 %ebx, %eax
       ret
.c48:  movbe tmem(%rip), %edx
       ret
.c49:  vpermq $0x4E, %ymm0, %ymm1
       ret
.c50:  vpor   %ymm2, %ymm3, %ymm4
       ret
.c51:  vinserti128 $0, tmem(%rip), %ymm13, %ymm14
       ret
.c52:  vextracti128 $0, %ymm1, wmem(%rip)
       vmovdqu wmem(%rip), %ymm2
       ret
.c53:  vinsertf128 $1, tmem(%rip), %ymm5, %ymm6
       ret
.c54:  vextractf128 $1, %ymm7, wmem(%rip)
       vmovdqu wmem(%rip), %ymm8
       ret
.c55:  vinserti128 $1, %xmm9, %ymm15, %ymm0
       ret
.c56:  vbroadcastss tmem(%rip), %ymm10
       ret
.c57:  vbroadcastsd tmem(%rip), %ymm11
       ret
.c58:  vpbroadcastd %xmm12, %ymm13
       ret
.c59:  vpermilpd $5, %ymm14, %ymm15
       ret
.c60:  vpermilps %ymm0, %ymm1, %ymm2
       ret
.c61:  vpmovmskb %ymm3, %esi
       ret
.c62:  vmovmskps %ymm4, %edi
       ret
.c63:  vmovmskpd %ymm5, %r8d
       ret
.c64:  vpmovmskb %ymm6, %r9d
       ret
.c65:  vfmadd132ps %ymm7, %ymm8, %ymm9
       ret
.c66:  vfnmadd213ps %ymm10, %ymm11, %ymm12
       ret
.c67:  vfmsub231ps %ymm13, %ymm14, %ymm15
       ret
.c68:  vblendvps %ymm0, %ymm1, %ymm2, %ymm3
       ret
.c69:  vpsubb %ymm4, %ymm5, %ymm6
       ret
.c70:  vpand %ymm7, %ymm8, %ymm9
       ret
.c71:  vpsrld $7, %ymm10, %ymm11
       ret
.c72:  vmovddup %ymm12, %ymm13
       ret
.c73:  vmovhlps %xmm14, %xmm15, %xmm0
       ret
.c74:  vpcmpeqd %ymm1, %ymm2, %ymm3
       ret
.c75:  vpcmpgtb %ymm4, %ymm5, %ymm6
       ret
.c76:  vshufps $0x44, %ymm7, %ymm8, %ymm9
       ret
.c77:  vunpcklps %ymm10, %ymm11, %ymm12
       ret
.c78:  vcvtsi2sd %r13, %xmm14, %xmm15
       ret
.c79:  vcvtdq2ps %ymm0, %ymm1
       ret
.c80:  vmovapd %ymm2, %ymm3
       ret
.c81:  vpmovmskb %ymm7, %rax
       ret
.c82:  vmovmskps %ymm8, %rax
       ret
.c83:  vpmovmskb %ymm15, %rax
       ret
.c84:  vmovmskps %ymm15, %rax
       ret
.c85:  vpmovmskb %ymm9, %rdi
       ret
.c86:  vpmovmskb %ymm2, %rax
       ret
.c87:  vmovmskps %ymm2, %rax
       ret
.c88:  vmovmskpd %ymm3, %rax
       ret

.section .rodata
.align 32
const0:  .quad 0xC0DEC0DEC0DEC0DE, 0xFEEDFACEFEEDFACE
         .quad 0x0BADC0DE0BADC0DE, 0x123456789ABCDEF0
const1:  .quad 0x1111111111111111, 0x2222222222222222
         .quad 0x3333333333333333, 0x4444444444444444
const2:  .quad 0x5555555555555555, 0x6666666666666666
         .quad 0x7777777777777777, 0x8888888888888888
const3:  .quad 0x9999999999999999, 0xAAAAAAAAAAAAAAAA
         .quad 0xBBBBBBBBBBBBBBBB, 0xCCCCCCCCCCCCCCCC
const4:  .quad 0xDDDDDDDDDDDDDDDD, 0xEEEEEEEEEEEEEEEE
         .quad 0xFFFFFFFFFFFFFFFF, 0x0000000000000000
const5:  .quad 0x0102030405060708, 0x090A0B0C0D0E0F10
         .quad 0x1112131415161718, 0x191A1B1C1D1E1F20
const6:  .quad 0x2122232425262728, 0x292A2B2C2D2E2F30
         .quad 0x3132333435363738, 0x393A3B3C3D3E3F40
const7:  .quad 0x4142434445464748, 0x494A4B4C4D4E4F50
         .quad 0x5152535455565758, 0x595A5B5C5D5E5F60
const8:  .quad 0x6162636465666768, 0x696A6B6C6D6E6F70
         .quad 0x7172737475767778, 0x797A7B7C7D7E7F80
const9:  .quad 0x8182838485868788, 0x898A8B8C8D8E8F90
         .quad 0x9192939495969798, 0x999A9B9C9D9E9FA0
const10: .quad 0xA1A2A3A4A5A6A7A8, 0xA9AAABACADAEAFB0
         .quad 0xB1B2B3B4B5B6B7B8, 0xB9BABBBEBFC0C1C2
const11: .quad 0xC1C2C3C4C5C6C7C8, 0xC9CACBCCCDCECFD0
         .quad 0xD1D2D3D4D5D6D7D8, 0xD9DADBDCDDDEDFE0
const12: .quad 0xE1E2E3E4E5E6E7E8, 0xE9EAEBECEDEEEFF0
         .quad 0xF1F2F3F4F5F6F7F8, 0xF9FAFBFCFDFEFF00
const13: .quad 0x7FF0000000000001, 0xFFF0000000000002
         .quad 0x0000000000000003, 0x8000000000000004
const14: .quad 0x4000000000000005, 0xC000000000000006
         .quad 0x3FF0000000000007, 0xBFF0000000000008
const15: .quad 0xA5A5A5A5A5A5A5A5, 0x5C5C5C5C5C5C5C5C
         .quad 0x9E3779B97F4A7C15, 0xBF58476D1CE4E5B9
tmem:    .quad 0xDEADBEEFDEADBEEF, 0xCAFEBABECAFEBABE
         .quad 0x0123456789ABCDEF, 0xFEDCBA9876543210

.section .bss
.align 64
wmem:    .space 64
msg:     .space 65536
