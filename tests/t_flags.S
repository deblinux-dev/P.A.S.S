# t_flags.S — сохранность EFLAGS вокруг хуков P.A.S.S.
#  T1/T2: cpuid не должен менять флаги (спецификация). Наш хук — тоже.
#  T3:    TZCNT: значение + CF=(src==0)
#  T4:    LZCNT: lzcnt(0)=width, lzcnt(0x80000000)=0
# Код выхода: 0 = успех; установленный бит = какой тест провален.
#   бит1: T1 ZF потерян         бит2: T1 CF изменился
#   бит3: T2 флаги потеряны     бит5: T3 значение     бит6: T3 CF
#   бит7: T3b значение          бит8: T3b CF
#   бит9: T4a                   бит10: T4b
.text
.global _start
_start:
    xor %r12d, %r12d        /* счётчик ошибок */

    /* ── T1: cpuid(0): ждём ZF=1, CF=0 ── */
    xor %eax, %eax
    xor %ecx, %ecx
    mov $0x4040, %ax        /* AH=0x40: SF=0 ZF=1 AF=0 PF=0 CF=0 */
    sahf
    cpuid
    lahf
    movzbl %ah, %esi
    and $0xC5, %esi         /* изолировать SF|ZF|PF|CF (биты 7,6,2,0) */
    cmp $0x40, %esi
    je 1f
    or  $6, %r12d           /* биты 1|2: ZF/CF искажены */
1:

    /* ── T2: cpuid(7,0): ждём ZF=1, CF=1 ── */
    mov $7, %eax
    xor %ecx, %ecx
    mov $0x4141, %ax        /* AH=0x41: ZF=1, CF=1 */
    sahf
    cpuid
    lahf
    movzbl %ah, %esi
    and $0xC5, %esi
    cmp $0x41, %esi
    je 2f
    or  $8, %r12d
2:

    /* ── T3: TZCNT 0x1000 → 12, CF=0 (было CF=1 от stc) ── */
    mov $0x1000, %eax
    stc
    .byte 0xF3, 0x0F, 0xBC, 0xC0   /* tzcnt %eax,%eax */
    mov %eax, %r14d        /* значение до lahf (lahf пишет в AH⊂EAX!) */
    lahf
    movzbl %ah, %esi
    cmp $12, %r14d
    je  3f
    or  $32, %r12d
3:  test $0x01, %esi        /* CF д.б. 0 */
    jz  4f
    or  $64, %r12d
4:

    /* ── T3b: TZCNT 0 → 32, CF=1 ── */
    xor %eax, %eax
    .byte 0xF3, 0x0F, 0xBC, 0xC0
    mov %eax, %r14d
    lahf
    movzbl %ah, %esi
    cmp $32, %r14d
    je  5f
    or  $128, %r12d
5:  test $0x01, %esi        /* CF д.б. 1 */
    jnz 6f
    or  $256, %r12d
6:

    /* ── T4a: LZCNT 0x80000000 → 0 ── */
    mov $0x80000000, %eax
    .byte 0xF3, 0x0F, 0xBD, 0xC0   /* lzcnt %eax,%eax */
    test %eax, %eax
    jz  7f
    or  $512, %r12d
7:

    /* ── T4b: LZCNT 0 → 32 ── */
    xor %eax, %eax
    .byte 0xF3, 0x0F, 0xBD, 0xC0
    cmp $32, %eax
    je  8f
    or  $1024, %r12d
8:

    mov $60, %eax
    mov %r12d, %edi
    syscall
