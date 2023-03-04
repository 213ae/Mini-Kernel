#include "defs.h"
    .section .text.entry
    .align 2
    .globl _traps 
_traps:
    # 如果是用户进程，切换栈，根据sscratch的值判断
    csrrw sp, sscratch, sp
    bne sp, zero, user_thread
    csrrw sp, sscratch, sp

user_thread:
    # 1. save 32 registers and sepc to stack
    sd x0, -8(sp)
    sd x1, -16(sp)
    sd x2, -24(sp)
    sd x3, -32(sp)
    sd x4, -40(sp)
    sd x5, -48(sp)
    sd x6, -56(sp)
    sd x7, -64(sp)
    sd x8, -72(sp)
    sd x9, -80(sp)
    sd x10, -88(sp)
    sd x11, -96(sp)
    sd x12, -104(sp)
    sd x13, -112(sp)
    sd x14, -120(sp)
    sd x15, -128(sp)
    sd x16, -136(sp)
    sd x17, -144(sp)
    sd x18, -152(sp)
    sd x19, -160(sp)
    sd x20, -168(sp)
    sd x21, -176(sp)
    sd x22, -184(sp)
    sd x23, -192(sp)
    sd x24, -200(sp)
    sd x25, -208(sp)
    sd x26, -216(sp)
    sd x27, -224(sp)
    sd x28, -232(sp)
    sd x29, -240(sp)
    sd x30, -248(sp)
    sd x31, -256(sp)

    # 之所以将sepc，sstatus保存在栈上是因为trap_handler可能要修改他们的值
    csrr t0, sepc
    sd t0, -264(sp)
    csrr t0, sstatus
    sd t0, -272(sp)

    addi sp, sp, -272

    # 2. call trap_handler
    csrr a0, scause
    csrr a1, sepc
    mv a2, sp
    call trap_handler

    # 3. restore sepc and 32 registers (x2(sp) should be restore last) from stack 
    addi sp, sp, 272

    ld t0, -272(sp)
    csrw sstatus, t0
    ld t0, -264(sp)
    csrw sepc, t0

    ld x0, -8(sp)
    ld x1, -16(sp)
    ld x2, -24(sp)
    ld x3, -32(sp)
    ld x4, -40(sp)
    ld x5, -48(sp)
    ld x6, -56(sp)
    ld x7, -64(sp)
    ld x8, -72(sp)
    ld x9, -80(sp)
    ld x10, -88(sp)
    ld x11, -96(sp)
    ld x12, -104(sp)
    ld x13, -112(sp)
    ld x14, -120(sp)
    ld x15, -128(sp)
    ld x16, -136(sp)
    ld x17, -144(sp)
    ld x18, -152(sp)
    ld x19, -160(sp)
    ld x20, -168(sp)
    ld x21, -176(sp)
    ld x22, -184(sp)
    ld x23, -192(sp)
    ld x24, -200(sp)
    ld x25, -208(sp)
    ld x26, -216(sp)
    ld x27, -224(sp)
    ld x28, -232(sp)
    ld x29, -240(sp)
    ld x30, -248(sp)
    ld x31, -256(sp)

    # todo 如果是用户进程，切换栈，根据sscratch的值判断
    csrrw sp, sscratch, sp
    bne sp, zero, trap_end
    csrrw sp, sscratch, sp

trap_end:
    # 4. return from trap
    sret

    .global __dummy
__dummy:
    # 切换栈
    csrrw sp, sscratch, sp
    # sd t0, -8(sp)
    # la t0, dummy
    # csrw sepc, t0
    # ld t0, -8(sp)
    sret

    .globl __switch_to
    .equ WORD_LEN, 8
    .equ RA_OFFSET, 5 * WORD_LEN
    .equ SP_OFFSET, 6 * WORD_LEN
    .equ S0_OFFSET, 7 * WORD_LEN
    .equ S1_OFFSET, 8 * WORD_LEN
    .equ S2_OFFSET, 9 * WORD_LEN
    .equ S3_OFFSET, 10 * WORD_LEN
    .equ S4_OFFSET, 11 * WORD_LEN
    .equ S5_OFFSET, 12 * WORD_LEN
    .equ S6_OFFSET, 13 * WORD_LEN
    .equ S7_OFFSET, 14 * WORD_LEN
    .equ S8_OFFSET, 15 * WORD_LEN
    .equ S9_OFFSET, 16 * WORD_LEN
    .equ S10_OFFSET, 17 * WORD_LEN
    .equ S11_OFFSET, 18 * WORD_LEN
    .equ SEPC_OFFSET, 19 * WORD_LEN
    .equ SSTATUS_OFFSET, 20 * WORD_LEN
    .equ SSCRATCH_OFFSET, 21 * WORD_LEN
    .equ PGT_OFFSET, 22 * WORD_LEN
__switch_to:
    # save state to prev process
    sd ra, RA_OFFSET(a0)
    sd sp, SP_OFFSET(a0)
    sd s0, S0_OFFSET(a0)
    sd s1, S1_OFFSET(a0)
    sd s2, S2_OFFSET(a0)
    sd s3, S3_OFFSET(a0)
    sd s4, S4_OFFSET(a0)
    sd s5, S5_OFFSET(a0)
    sd s6, S6_OFFSET(a0)
    sd s7, S7_OFFSET(a0)
    sd s8, S8_OFFSET(a0)
    sd s9, S9_OFFSET(a0)
    sd s10, S10_OFFSET(a0)
    sd s11, S11_OFFSET(a0)

    csrr t0, sepc 
    sd t0, SEPC_OFFSET(a0)
    csrr t0, sstatus 
    sd t0, SSTATUS_OFFSET(a0)
    csrr t0, sscratch 
    sd t0, SSCRATCH_OFFSET(a0)

    # restore state from next process
    ld ra, RA_OFFSET(a1)
    ld sp, SP_OFFSET(a1)
    ld s0, S0_OFFSET(a1)
    ld s1, S1_OFFSET(a1)
    ld s2, S2_OFFSET(a1)
    ld s3, S3_OFFSET(a1)
    ld s4, S4_OFFSET(a1)
    ld s5, S5_OFFSET(a1)
    ld s6, S6_OFFSET(a1)
    ld s7, S7_OFFSET(a1)
    ld s8, S8_OFFSET(a1)
    ld s9, S9_OFFSET(a1)
    ld s10, S10_OFFSET(a1)
    ld s11, S11_OFFSET(a1)

    ld t0, SEPC_OFFSET(a1)
    csrw sepc, t0
    ld t0, SSTATUS_OFFSET(a1)
    csrw sstatus ,t0
    ld t0, SSCRATCH_OFFSET(a1)
    csrw sscratch ,t0
    ld t0, PGT_OFFSET(a1)
    srli t0, t0, PAGE_SHIFT
    li t1, SV39_MODE
    or t0, t0, t1
    csrw satp, t0

    sfence.vma zero, zero

    ret

    .global ret_from_fork
ret_from_fork:
    addi a0, a0, 272
    
    ld x0, -8(a0)
    ld x1, -16(a0)
    ld x2, -24(a0)
    ld x3, -32(a0)
    ld x4, -40(a0)
    ld x5, -48(a0)
    ld x6, -56(a0)
    ld x7, -64(a0)
    ld x8, -72(a0)
    ld x9, -80(a0)
    ld x11, -96(a0)
    ld x12, -104(a0)
    ld x13, -112(a0)
    ld x14, -120(a0)
    ld x15, -128(a0)
    ld x16, -136(a0)
    ld x17, -144(a0)
    ld x18, -152(a0)
    ld x19, -160(a0)
    ld x20, -168(a0)
    ld x21, -176(a0)
    ld x22, -184(a0)
    ld x23, -192(a0)
    ld x24, -200(a0)
    ld x25, -208(a0)
    ld x26, -216(a0)
    ld x27, -224(a0)
    ld x28, -232(a0)
    ld x29, -240(a0)
    ld x30, -248(a0)
    ld x31, -256(a0)
    ld x10, -88(a0)

    csrrw sp, sscratch, sp

    sret