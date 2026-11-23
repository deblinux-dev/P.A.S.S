/* t_pinsv.S — B-2014: микро-батарея inline-семьи SSE4.1 (VF_EXTR / VF_INS /
 * VF_ND2): vpextrd/q/b/w (reg- и mem-dst), vpinsrd/q (4-операндные формы:
 * merge != dst и merge == dst), vptest (флаги), vroundps (imm-форма).
 * Каждый кейс пишет результат в msg; прогон native vs P.A.S.S. диффится
 * побайтно — любое расхождение = дефект inline-переписывания.
 *
 * Запуск: ./t_pinsv > out.bin ; drrun -c libpass_client.so -target core2
 *         -- ./t_pinsv > out2.bin ; cmp out.bin out2.bin
 */
.text
.global _start

_start:
    lea  msg(%rip), %rbx
    lea  c1(%rip), %rax
    movups (%rax), %xmm1          /* 11111111 22222222 33333333 44444444 */
    lea  c2(%rip), %rax
    movups (%rax), %xmm2          /* AAAA0001 BBBB0002 CCCC0003 DDDD0004 */
    movups (%rax), %xmm0
    movabs $0xCAFEBABE5A5A5A5A, %rax
    movabs $0x0F0F0F0F0F0F0F0F, %rcx
    sub  $64, %rsp

    /* 1: vpextrd $2, %xmm1, %eax → 0x33333333 */
    mov $0xDEADBEEF, %eax
    vpextrd $2, %xmm1, %eax
    mov %eax, 0(%rbx)

    /* 2: vpextrd $1, %xmm1, mem → 0x22222222 */
    vpextrd $1, %xmm1, 8(%rbx)

    /* 3: vpinsrd 4-форма merge=xmm2≠dst: lane1=0x5A5A5A5A, rest=xmm2 */
    mov $0x5A5A5A5A, %eax
    vpinsrd $1, %eax, %xmm2, %xmm3
    vpextrd $1, %xmm3, %eax
    mov %eax, 16(%rbx)            /* ожидаем 0x5A5A5A5A */

    /* 4: vpinsrd 4-форма merge=dst: lane1=0xCAFEBABE, rest=xmm4 */
    mov $0xCAFEBABE, %eax
    lea c2(%rip), %rsi
    movups (%rsi), %xmm4
    vpinsrd $1, %eax, %xmm4, %xmm4
    vpextrd $2, %xmm4, %eax
    mov %eax, 20(%rbx)            /* ожидаем 0xCCCC0003 (lane2 нетронут) */

    /* 5: vptest — флаги */
    vptest %xmm2, %xmm1
    sete %al
    setc %ah
    mov %ax, 24(%rbx)

    /* 6: vpextrq $1, %xmm1, %rcx → 0x3333333344444444 */
    vpextrq $1, %xmm1, %rcx
    mov %rcx, 32(%rbx)

    /* 7: vpextrb $11, %xmm1, %al → 0x33 (lane11 = byte2 of dword2) */
    vpextrb $11, %xmm1, %eax
    mov %eax, 40(%rbx)

    /* 8: vroundps $3, %xmm2, %xmm1 (truncate) → 4 dword floats */
    vroundps $3, %xmm2, %xmm1
    vpextrd $0, %xmm1, %eax
    mov %eax, 44(%rbx)

    add $64, %rsp
    mov $1, %eax
    mov $1, %edi
    lea msg(%rip), %rsi
    mov $48, %edx
    syscall
    mov $60, %eax
    xor %edi, %edi
    syscall

.data
msg: .space 64
c1:  .long 0x11111111, 0x22222222, 0x33333333, 0x44444444
c2:  .long 0xAAAA0001, 0xBBBB0002, 0xCCCC0003, 0xDDDD0004
