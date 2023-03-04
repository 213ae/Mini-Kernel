#include "defs.h"
.extern mm.init
.extern task_init
.extern start_kernel

    .section .text.init
    .globl _start
_start:
    la sp, boot_stack_top
    li a0, PA2KVA_OFFSET
    sub sp, sp, a0

    call setup_vm
    call relocate

    # set stvec = _traps
    la t0, _traps
    li t1, 0xfffffffffffffffc
    and t0, t0, t1
    csrw stvec, t0

    # set sie[STIE] = 1
    csrr t0, sie
    ori t0, t0, 0x020
    csrw sie, t0

    # set first time interrupt
    # rdtime a0
    # li a1, 0x1000000
    # add a0, a1, a0
    # mv a1, zero
    # mv a2, zero
    # mv a3, zero
    # mv a4, zero
    # mv a5, zero
    # mv a6, zero
    # mv a7, zero
    # ecall

    call mm_init
    call setup_vm_final
    call task_init
    
    # set sstatus[SIE] = 1
    # csrrsi x0, sstatus, 0x002
    
    j start_kernel

relocate:
    # set ra = ra + PA2VA_OFFSET
    # set sp = sp + PA2VA_OFFSET (If you have set the sp before)
    
    li a0, PA2KVA_OFFSET
    add ra, ra, a0
    add sp, sp, a0
    #add sp, sp, a0
    # set satp with early_pgtbl
    
    la a1, early_pgtbl
    sub a1, a1, a0
    srli a1, a1, PAGE_SHIFT
    li a0, SV39_MODE
    or a1, a1, a0

    csrw satp, a1
    
    # flush tlb
    sfence.vma zero, zero

    ret

    .section .bss.stack
    .globl boot_stack
boot_stack:
    .space 4096# <-- change to your stack size

    .globl boot_stack_top
boot_stack_top: