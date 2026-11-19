# t_iemu.S — дифференциальный тест целочисленной VEX-семьи emu
# (§7.3 run #101: bun/JSC-строки портятся — ищем несовпадение с
# нативом). Запуск: нативно vs pass (default) vs pass -inline 0 —
# все три вывода обязаны совпасть побайтно. Хеш lo+hi+eflags.
# Кейсы покрывают: логику, арифметику, насыщение, умножения,
# сравнения, imm-сдвиги (в т.ч. count >= ширины), упаковки,
# пересечения, pshufb/psadbw/palignr/blendw, vmovd/vmovq.
.text
.global _start
_start:
    lea msg(%rip), %rbx
    mov $0, %r15d

    /* пресеты: ymm1 = t0 (32Б), ymm2 = t1, ymm3 = t2 */
    vmovdqu t0(%rip), %ymm1
    vmovdqu t1(%rip), %ymm2
    vmovdqu t2(%rip), %ymm3

    /* 1-4: логика */
    vpand  %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpor   %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpxor  %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpandn %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 5-12: арифметика b/w/d/q */
    vpaddb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddd %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddq %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubb %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubw %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubd %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubq %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 13-20: насыщение */
    vpaddsb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddsw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubsb %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubsw %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddusb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpaddusw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubusb %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsubusw %ymm3, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 21-24: умножения */
    vpmullw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpmulhw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpmulhuw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpmaddwd %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 25-26: средние */
    vpavgb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpavgw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 27-32: сравнения */
    vpcmpeqb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpcmpeqw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpcmpeqd %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpcmpgtb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpcmpgtw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpcmpgtd %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 33-36: imm-сдвиги в границах ширины */
    vmovdqu t0(%rip), %ymm1
    vpsllw $5, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsrlw $5, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpslld $11, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsrlq $33, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 37-40: imm-сдвиги ЗА границей ширины (SDM: ноль) */
    vpsllw $16, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsrld $32, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpslldq $16, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsrldq $17, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 41-43: упаковки (128-бит кейсы — источники xmm) */
    vmovdqu t0(%rip), %xmm1
    vmovdqu t1(%rip), %xmm2
    vpacksswb %xmm2, %xmm1, %xmm0
    call hashlo
    vextractf128 $1, %ymm0, %xmm0
    call hashhi2
    vpackuswb %xmm2, %xmm1, %xmm4
    vmovdqu %xmm4, %xmm0
    call hashlo
    vpackssdw %xmm2, %xmm1, %xmm4
    vmovdqu %xmm4, %xmm0
    call hashlo

    /* 44-47: пересечения байты/слова/дворды/кворды */
    vmovdqu t0(%rip), %ymm1
    vmovdqu t1(%rip), %ymm2
    vpunpcklbw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpunpckhbw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpunpcklwd %ymm2, %ymm1, %ymm0
    call hashlo
    vpunpckhwd %ymm2, %ymm1, %ymm4
    vmovdqu %ymm4, %ymm0
    call hashlo
    vpunpckldq %ymm2, %ymm1, %ymm0
    call hashlo
    vpunpckhqdq %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 48-51: pshufb, psadbw, palignr, blendw */
    vpshufb %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpsadbw %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpalignr $7, %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi
    vpblendw $0xA5, %ymm2, %ymm1, %ymm0
    call hashlo
    call hashhi

    /* 52-54: vmovd/vmovq GPR<->xmm */
    vmovdqu t0(%rip), %xmm1
    vmovd  %xmm1, %r8d
    mov    $0x5A5AA5A5, %r9d
    vmovd  %r9d, %xmm5
    vmovdqu %xmm5, %xmm0
    call hashlo
    vmovdqu t0(%rip), %xmm1
    vmovq  %xmm1, %r11
    movq   %r11, %xmm0
    call hashlo
    mov    $0x1122334455667788, %r8
    vmovq  %r8, %xmm0
    call hashlo

    /* 55: vmovdqa 256 загрузка/сохранение (через память) */
    vmovdqa t0(%rip), %ymm4
    vmovdqa %ymm4, scratch(%rip)
    vmovdqa scratch(%rip), %ymm0
    call hashlo
    call hashhi

    /* 40b: imm за границей ещё раз (некратные) */
    vpsllw $17, %ymm1, %ymm0
    call hashlo
    vpsrld $33, %ymm1, %ymm0
    call hashlo

    /* вывод */
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov %r15d, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

/* hash: eflags ^ low64(xmm0) — 8 байт */
hashlo:
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
/* hashhi2: хеш текущего xmm0 (после vextractf128) */
hashhi2:
    pushfq
    pop %rax
    movq %xmm0, %rcx
    xor %rax, %rcx
    mov %ecx, 0(%rbx,%r15,1)
    shr $32, %rcx
    mov %ecx, 4(%rbx,%r15,1)
    add $8, %r15d
    ret
/* hashhi: хеш старшей половины ymm0 */
hashhi:
    pushfq
    pop %rax
    vextractf128 $1, %ymm0, %xmm5
    movq %xmm5, %rcx
    xor %rax, %rcx
    mov %ecx, 0(%rbx,%r15,1)
    shr $32, %rcx
    mov %ecx, 4(%rbx,%r15,1)
    add $8, %r15d
    ret

.section .rodata
.align 32
t0: .quad 0x8877665544332211, 0x7F80FF7F0180FE01, 0xDEADBEEFCAFEBABE, 0x8001000200007FFF
t1: .quad 0x1122334455667788, 0x0180FF808000FFFF, 0x1234567890ABCDEF, 0x00010002FF807FFF
t2: .quad 0xFFFFFFFF00000000, 0x00000000FFFFFFFF, 0x8000000000000001, 0x00000000000000FF
.align 16
r11_tmp: .space 16
.section .bss
.align 32
scratch: .space 32
msg:
    .space 2048
