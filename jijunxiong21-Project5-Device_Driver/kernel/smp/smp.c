// #include <os/smp.h> // ignored
// #include <os/lock.h> // ignored 
// #include <os/sched.h> // ignored
#include <os/irq.h>

spin_lock_t giant_lock = {UNLOCKED};
spin_lock_t giant_lock_s = {UNLOCKED};
int giant_lock_owner;

spin_lock_t log_lock = {UNLOCKED};

void smp_init()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_init(&giant_lock);

    // setup default process for hart 1
    current_running[1] = &pid0_pcb[1];
    strncpy(current_running[1]->name, "idle_1", MAX_TASK_NAME_LEN - 1);
    asm volatile (
        "mv tp, %0\n\t"
        :
        :"r"(current_running[1])
    ); // move current_running to $tp

    /* defaultly we consider two CPU cores have the same time_base, so no need to read it again */

    // Init interrupt (only needs to setup related registers)
    setup_exception();
}

void wakeup_other_hart()
{
    /* TODO: P3-TASK3 multicore*/
    // wake up another core
    const unsigned long mask = 0x1 << (1 - get_current_cpu_id()); // wake up all cores except core 0
    send_ipi(&mask);
}

void lock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_acquire(&giant_lock);
    giant_lock_owner = get_current_cpu_id(); // for debug
}

void unlock_kernel()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_release(&giant_lock);
}

void lock_kernel_s()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_acquire(&giant_lock_s);
    giant_lock_owner = get_current_cpu_id(); // for debug
}

void unlock_kernel_s()
{
    /* TODO: P3-TASK3 multicore*/
    spin_lock_release(&giant_lock_s);
}
