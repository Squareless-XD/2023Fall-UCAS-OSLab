#include <asm/biosdef.h>
#include <csr.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ os_size_loc, 0x502001f8

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
	// fence on all memory and I/O
	fence
    // a0 is mhartid
	bnez a0, secondary

    // TODO: [p1-task1] call BIOS to print string "It's bootblock!"
    li  a7, BIOS_PUTSTR     // a7 = BIOS_PUTSTR
    la  a0, msg             // load msg
    jal bios_func_entry     // jump to BIOS function entry. BIOS will print msg

    // TODO: [p1-task2] call BIOS to read kernel in SD card
    li  a7, BIOS_SDREAD     // a7 = BIOS_SDREAD
    li  a0, kernel          // a0 = kernel
    li  t0, os_size_loc     // t0 = os_size_loc
    lw  a1, 0(t0)           // a1 = os_size_loc, the size of kernel
    li  a2, 1               // the second block of disk
    jal bios_func_entry     // jump to BIOS function entry, BIOS will read kernel from SD card into memory


    // TODO: [p1-task4] load task-related arguments and pass them to kernel
    # lh  a0, 2(t0)           // a0 = os_size_loc + 2, the sector id of APP Info
    # sll a0, a0, 9           // a0 = a0 * 512, the address of APP Info

    // TODO: [p1-task2] jump to kernel to start UCAS-OS
    j   kernel              // jump to kernel (kernel is loaded into memory)


	/************************************************************/
secondary:
	/* TODO [P3-task3]: 
	 * 1. Mask all interrupts
	 * 2. let stvec pointer to kernel_main
	 * 3. enable software interrupt for ipi
	 */

    /* TODO: [p2-task4] enable interrupts globally */
    csrw sstatus, zero // disable interrupts

    li t1, SIE_SSIE
    csrw sie, t1 // enable software interrupt for ipi

    la t2, kernel
    # andi t2, t2, -4 // align to 4-byte (change to default mode)
    csrw stvec, t2 // load exception handler entry

    /* TODO: [p2-task4] enable interrupts globally */
    li t0, SR_SIE
    csrw sstatus, t0

wait_for_wakeup:
	wfi
	j wait_for_wakeup

	/************************************************************/
// while(1) --> stop here
stop:
    j stop
    nop

.data

msg: .string "It's Boot Loader!\n"

