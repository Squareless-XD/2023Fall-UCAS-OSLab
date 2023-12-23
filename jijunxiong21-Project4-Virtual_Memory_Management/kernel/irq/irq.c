#include <os/irq.h>
#include <os/time.h>
#include <os/kernel.h>

handler_t irq_table[IRQC_COUNT]; // only modified in init_exception
handler_t exc_table[EXCC_COUNT]; // only modified in init_exception

const char *exc_name_table[EXCC_COUNT] = {
    [EXCC_INST_MISALIGNED]  = "Instruction Address Misaligned",
    [EXCC_INST_ACCESS]      = "Instruction Access Fault",
    [EXCC_ILLEGAL_INST]     = "Illegal Instruction",
    [EXCC_BREAKPOINT]       = "Breakpoint",
    [EXCC_LOAD_MISALIGNED]  = "Load Address Misaligned",
    [EXCC_LOAD_ACCESS]      = "Load Access Fault",
    [EXCC_STORE_MISALIGNED] = "Store/AMO Address Misaligned",
    [EXCC_STORE_ACCESS]     = "Store/AMO Access Fault",
    [EXCC_SYSCALL]          = "Environment Call from U-mode",
    [EXCC_SYSCALL_S]        = "Environment Call from S-mode",
    [EXCC_INST_PAGE_FAULT]  = "Instruction Page Fault",
    [EXCC_LOAD_PAGE_FAULT]  = "Load Page Fault",
    [EXCC_STORE_PAGE_FAULT] = "Store/AMO Page Fault",
};

void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    
    // WRITE_LOG("stval = %lx, scause = %lx\n", stval, scause);

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
}

void interrupt_helper_s(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task3] & [p2-task4] interrupt handler.
    // call corresponding handler by the value of `scause`
    
    // WRITE_LOG("stval = %lx, scause = %lx\n", stval, scause);

    if (my_cpu() == my_idle_proc())
    {
        lock_kernel();
    }

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

    if (my_cpu() == my_idle_proc())
    {
        unlock_kernel();
    }
}

void handle_irq_timer(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p2-task4] clock interrupt handler.
    // Note: use bios_set_timer to reset the timer and remember to reschedule
    
    // WRITE_LOG("timer interrupt.\n")
    
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
    // exc_table[EXCC_INST_MISALIGNED]  = handle_other;         // instruction address misaligned
    // exc_table[EXCC_INST_ACCESS]      = handle_other;         // instruction access fault
    // exc_table[EXCC_ILLEGAL_INST]     = handle_other;         // instruction access fault
    // exc_table[EXCC_BREAKPOINT]       = handle_other;         // instruction access fault
    // exc_table[EXCC_LOAD_MISALIGNED]  = handle_other;         // breakpoint
    // exc_table[EXCC_LOAD_ACCESS]      = handle_other;         // breakpoint
    // exc_table[EXCC_STORE_MISALIGNED] = handle_other;         // load address fault
    // exc_table[EXCC_STORE_ACCESS]     = handle_other;         // store/AMO address fault
    exc_table[EXCC_SYSCALL]          = handle_syscall;       // environment call from U-mode
    // exc_table[EXCC_SYSCALL_S]        = handle_other;         // instruction page fault
    // exc_table[EXCC_INST_PAGE_FAULT]  = handle_other;         // instruction page fault
    exc_table[EXCC_LOAD_PAGE_FAULT]  = handle_page_fault_rd; // load page fault
    exc_table[EXCC_STORE_PAGE_FAULT] = handle_page_fault_wr; // store/AMO page fault

    /* TODO: [p2-task4] initialize irq_table */
    /* NOTE: handle_int, handle_other, etc.*/
    for(int i = 0; i < IRQC_COUNT; ++i)
        irq_table[i] = handle_other;
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
    WRITE_LOG("ERROR: core %d encountered %s\n", get_current_cpu_id(), exc_name_table[scause]);
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
            PRINTKL("%s : %016lx ",reg_name[i+j], regs->regs[i+j]);
        }
        PRINTKL("\n\r");
    }
    PRINTKL("sstatus: 0x%lx sbadaddr: 0x%lx scause: %lu\n\r",
           regs->sstatus, regs->sbadaddr, regs->scause);
    PRINTKL("sepc: 0x%lx\n\r", regs->sepc);
    PRINTKL("tval: 0x%lx cause: 0x%lx\n", stval, scause);
    PRINTKL("exception cause: %s\n", exc_name_table[scause]);
    assert(0)
}
