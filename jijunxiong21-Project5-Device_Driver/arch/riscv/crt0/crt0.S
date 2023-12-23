#include <asm.h>

.section ".entry_function","ax"
ENTRY(_start)

    /* TODO: [p1-task3] setup C runtime environment for the user program */
    // clear bss
    la  t0, __bss_start         // t0 = start address of bss section
    la  t1, __BSS_END__         // t1 = end address of bss section
    andi    t2, t1, -8          // t1 = t1 & 0xfffffff8 (align to 8 bytes)
clear_double_crt0:
    bge t0, t2, clear_byte_crt0 // if t0 >= t2, go to byte-level clearing
    sd  zero,   0(t0)           // clear 8 bytes
    addi    t0, t0, 8           // t0 += 8
    j   clear_double_crt0       // jump back
clear_byte_crt0:
    bge t0, t1, clear_done_crt0 // if t0 >= t1, finish
    sb  zero,   0(t0)           // clear 1 bytes
    addi    t0, t0, 1           // t0 += 1
    j   clear_byte_crt0         // jump back
clear_done_crt0:

    /* TODO: [p1-task3] enter main function */
    call main

    /************************************************************/
    /* TODO: [P3] call sys_exit */
    li a7, 1 /* NOTE: PLEASE MAKE SURE THAT SYSCALL_EXIT=1 */
    ecall
    /************************************************************/
// while(1) loop, unreachable here
loop:
    wfi
    j loop

END(_start)
