#include <os/irq.h>

handler_t irq_table[IRQC_COUNT]; // only modified in init_exception
handler_t exc_table[EXCC_COUNT]; // only modified in init_exception

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`

    // acquire the gaint lock
    // lock_kernel();
    
    // check the highest bit of scause
    if (scause & INTERRUPT_SIGN) // interrupt
    {
        if ((scause & ~INTERRUPT_SIGN) >= IRQC_COUNT)
            irq_table[0](regs, stval, scause);
        else
            irq_table[scause & ~INTERRUPT_SIGN](regs, stval, scause);
    }
    else // exception
    {
        if (scause >= EXCC_COUNT)
            exc_table[0](regs, stval, scause);
        else
            exc_table[scause](regs, stval, scause);
    }

    // release the gaint lock
    // unlock_kernel();
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule

    // reset the timer by the current time added by the interval
    bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);

    // reschedule
    do_scheduler();
}

void init_exception()
{
    // printk("into init_exception()\n");
    /* TODO: [p2-task3] initialize exc_table */
    /* NOTE: handle_syscall, handle_other, etc.*/
    for(int i = 0; i < EXCC_COUNT; ++i)
        exc_table[i] = handle_other;
    // exc_table[EXCC_INST_MISALIGNED]  = handle_other;    // instruction address misaligned
    // exc_table[EXCC_INST_ACCESS]      = handle_other;    // instruction access fault
    // exc_table[EXCC_BREAKPOINT]       = handle_other;    // breakpoint
    // exc_table[EXCC_LOAD_ACCESS]      = handle_other;    // load address fault
    // exc_table[EXCC_STORE_ACCESS]     = handle_other;    // store/AMO address fault
    exc_table[EXCC_SYSCALL]          = handle_syscall;  // environment call from U-mode
    // exc_table[EXCC_INST_PAGE_FAULT]  = handle_other;    // instruction page fault
    // exc_table[EXCC_LOAD_PAGE_FAULT]  = handle_other;    // load page fault
    // exc_table[EXCC_STORE_PAGE_FAULT] = handle_other;    // store/AMO page fault

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for(int i = 0; i < IRQC_COUNT; ++i)
        irq_table[i] = handle_irq_timer;
    // irq_table[IRQC_U_SOFT]  = handle_other;     //
    // irq_table[IRQC_S_SOFT]  = handle_other;     // supervisor software interrupt
    // irq_table[IRQC_M_SOFT]  = handle_other;     //
    // irq_table[IRQC_U_TIMER] = handle_other;     //
    irq_table[IRQC_S_TIMER] = handle_irq_timer; // supervisor timer interrupt
    // irq_table[IRQC_M_TIMER] = handle_other;     //
    // irq_table[IRQC_U_EXT]   = handle_other;     //
    // irq_table[IRQC_S_EXT]   = handle_other;     // supervisor external interrupt
    // irq_table[IRQC_M_EXT]   = handle_other;     //

    /* TODO: [p2-task3] set up the entrypoint of exceptions */
    setup_exception();
}

void handle_other(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    char* reg_name[] = {
        "zero "," ra  "," sp  "," gp  "," tp  ",
        " t0  "," t1  "," t2  ","s0/fp"," s1  ",
        " a0  "," a1  "," a2  "," a3  "," a4  ",
        " a5  "," a6  "," a7  "," s2  "," s3  ",
        " s4  "," s5  "," s6  "," s7  "," s8  ",
        " s9  "," s10 "," s11 "," t3  "," t4  ",
        " t5  "," t6  "
    };
    for (int i = 0; i < 32; i += 3) {
        for (int j = 0; j < 3 && i + j < 32; ++j) {
            printk("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        printk("\n\r");
    }
    printk("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    printk("sepc: 0x%lx\n\r", regs->sepc);
    printk("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    assert(0);
}
