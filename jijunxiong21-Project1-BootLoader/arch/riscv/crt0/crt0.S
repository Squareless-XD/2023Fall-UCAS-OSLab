#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    // clear bss
    la  t0, __bss_start         // t0 = start address of bss section
    la  t1, __BSS_END__         // t1 = end address of bss section
clear_loop_crt0:
    bge t0, t1, clear_done_crt0 // if t0 >= t1, jump to clear_done_crt0
    sb  zero,   0(t0)           // clear 1 bytes
    addi    t0, t0, 1           // t0 += 1
    j   clear_loop_crt0         // jump to clear_loop_crt0
clear_done_crt0:

    // save registers
    addi    sp, sp, -16          // sp -= 16
    sw  ra, 0(sp)               // save ra

    /* TODO: [p1-task3] enter main function */
    call main


    /* TODO: [p1-task3] finish task and return to the kernel, replace this in p3-task2! */
    // restore registers
    lw  ra, 0(sp)               // restore ra
    addi    sp, sp, 16           // sp += 16

    ret

    /************************************************************/
	/* Do not touch this comment. Reserved for future projects. */
	/************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
