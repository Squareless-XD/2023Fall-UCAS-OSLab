#include <asm/biosdef.h>

// os size location (os_size could be sectors [p1-task3] or bytes [p1-task4])
.equ kernel_sz, 0x502001f4
.equ decmp_sec_off, 0x502001f6
.equ decmp_start_loc, 0x502001f8
.equ decmp_size_loc, 0x502001fa
.equ os_size_loc, 0x502001fc
.equ app_info_loc, 0x502001fe

// kernel address (move kernel to here, and jmp here to start kernel)
.equ kernel, 0x50201000
.equ kernel_tmp, 0x50250000
.equ decompress, 0x50210000

// BIOS function entry (jump here to use BIOS APIs)
.equ bios_func_entry, 0x50150000

.text
.global main

main:
    // fence on all memory and I/O
    fence

    // TODO: [p1-task1] call BIOS to print string "It's bootblock!"
    li  a7, BIOS_PUTSTR     // a7 = BIOS_PUTSTR
    la  a0, msg             // load msg
    jal bios_func_entry     // jump to BIOS function entry. BIOS will print msg

    // TODO: [p1-task2] call BIOS to read kernel in SD card
    li  a7, BIOS_SDREAD     // a7 = BIOS_SDREAD
    li  a0, decompress      // a0 = decompress
    li  t0, decmp_size_loc  // t0 = decmp_size_loc
    lh  a1, 0(t0)           // a1 = decmp_size_loc, the size of kernel (by sector)
    li  t0, decmp_start_loc // t0 = decmp_start_loc
    lh  a2, 0(t0)           // start at which sector
    jal bios_func_entry     // jump to BIOS function entry, BIOS will read kernel from SD card into memory

    li  t0, decmp_sec_off
    lh  t0, 0(t0)
    add t0, t0, a0          // t0 = t0 + a0

loop:
    slli    t1, a1, 9       // t1 = sector_size * 512
    add t2, a0, t1
    lw  t1, (t0)            // t1 = $(t0)
    sw  t1, (a0)            // $(a0) = t1
    addi    t0, t0, 4       // t0 = t0 + 4
    addi    a0, a0, 4       // a0 = a0 + 4
    blt a0, t2, loop        // if a0 < t2 then loop
    
    

    li  a7, BIOS_SDREAD     // a7 = BIOS_SDREAD
    li  a0, kernel_tmp      // a0 = kernel_tmp
    li  t0, os_size_loc     // t0 = os_size_loc
    lh  a1, 0(t0)           // a1 = os_size_loc, the size of kernel
    li  a2, 1               // the second block of disk
    jal bios_func_entry     // jump to BIOS function entry, BIOS will read kernel from SD card into memory


    // TODO: [p1-task4] load task-related arguments and pass them to kernel
    # lh  a0, 2(t0)           // a0 = os_size_loc + 2, the sector id of APP Info
    # sll a0, a0, 9           // a0 = a0 * 512, the address of APP Info

    li  a0, kernel_tmp
    li  t0, kernel_sz
    lh  a1, 0(t0)
    li  a2, kernel
    jal decompress

    // TODO: [p1-task2] jump to kernel to start UCAS-OS
    j   kernel              // jump to kernel (kernel is loaded into memory)


    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/
// while(1) --> stop here
stop:
    j stop
    nop

.data

msg: .string "It's Boot Loader!\n"

