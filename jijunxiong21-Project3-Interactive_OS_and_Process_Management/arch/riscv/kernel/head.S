/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#include <asm.h>
#include <csr.h>

#define KERNEL_STACK		0x50500000
#define KERNEL_STACK_2		0x50502000

.section ".entry_function","ax"
ENTRY(_start)
    /* Mask all interrupts */
    csrw CSR_SIE, zero
    csrw CSR_SIP, zero

    // a0 is mharid (passed from bootblock.S)
    csrr a0, CSR_MHARTID
	bnez a0, hart_1_entry     // if the CPU is hart 1, do not clear bss

    /* TODO: [p1-task2] clear BSS for flat non-ELF images */

    // clear bss
    la  t0, __bss_start     // t0 = start address of bss section
    la  t1, __BSS_END__     // t1 = end address of bss section
clear_loop:
    bge t0, t1, clear_done  // if t0 >= t1, jump to clear_done
    sb  zero,   0(t0)       // clear 1 bytes
    addi    t0, t0, 1       // t0 += 1
    j   clear_loop          // jump to clear_loop
clear_done:

    /* TODO: [p1-task2] setup C environment */

    // set stack pointer
    li  sp, KERNEL_STACK

    // jump to main
    j   main


loop:
    wfi
    j loop


hart_1_entry:
    // set stack pointer
    li  sp, KERNEL_STACK_2

    // jump to main
    j   main


loop2:
    wfi
    j loop2

END(_start)

