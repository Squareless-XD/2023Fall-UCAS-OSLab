#include <sys/syscall.h>

long (*syscall[NUM_SYSCALLS])();

void handle_syscall(regs_context_t *regs, uint64_t interrupt, uint64_t cause)
{
    /* TODO: [p2-task3] handle syscall exception */
    /**
     * HINT: call syscall function like syscall[fn](arg0, arg1, arg2),
     * and pay attention to the return value and sepc
     */

    // set the sepc to the next instruction
    regs->sepc += 4;

    // now we need to get the syscall number from a7
    reg_t syscall_num = regs->regs[TRAP_A(7)];

    // the arguments from a0, a1, a2, a3, a4
    reg_t arg0 = regs->regs[TRAP_A(0)];
    reg_t arg1 = regs->regs[TRAP_A(1)];
    reg_t arg2 = regs->regs[TRAP_A(2)];
    reg_t arg3 = regs->regs[TRAP_A(3)];
    reg_t arg4 = regs->regs[TRAP_A(4)];

    // now we can call the syscall function
    reg_t ret = syscall[syscall_num](arg0, arg1, arg2, arg3, arg4);

    // set the return value to a0
    regs->regs[TRAP_A(0)] = ret;
}
