# t_pmk5.S — B-2014: ГОРЯЧИЙ цикл (DR-трейс) с полным паттерном bun:
# rbp-стек, vmovdqu load/store, vpcmpeqb, vpmovmskb, tzcnt — 100k повторов.
.text
.global _start
_start:
    lea data(%rip), %rbx
    lea out(%rip), %r8
    push %rbp
    mov %rsp, %rbp
    sub $0x800, %rsp
    xor %r9d, %r9d                  # счётчик
    mov $100000, %r9d
loopk:
    vmovdqu (%rbx), %ymm0           # данные
    vmovdqu 32(%rbx), %ymm2         # needle
    vpcmpeqb %ymm2, %ymm0, %ymm1
    vpmovmskb %ymm1, %eax
    mov %eax, -0x5b0(%rbp)          # маска в red-zone-глубину стека
    tzcnt %eax, %ecx
    mov %ecx, -0x5a8(%rbp)
    vmovdqu -0x5b0(%rbp), %xmm3     # чтение обратно (lo)
    vpmovmskb %xmm3, %edx
    add %edx, %esi                  # аккумулятор
    vzeroupper
    dec %r9d
    jnz loopk
    mov %rsi, (%r8)
    mov %rcx, 8(%r8)
    add $0x800, %rsp
    pop %rbp
    mov $1, %eax
    mov $1, %edi
    lea out(%rip), %rsi
    mov $16, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall
.bss
.lcomm out, 32
.data
data:
.quad 0x0102030405060708, 0x090a0b0c0d0e0f10, 0x1112131415161718, 0x191a1b1c1d1e1f20
.quad 0x2122232425262728, 0x292a2b2c2d2e2f30, 0x3132333435363738, 0x393a3b3c3d3e3f40
