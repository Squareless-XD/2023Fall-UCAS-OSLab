#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>

pcb_t pcb[NUM_MAX_TASK];
spin_lock_t pcb_lk = {UNLOCKED};

pcb_t pid0_pcb[NR_CPUS] = {
    {
        .pid = 0,
        .kernel_sp = PID0_STACK_CORE0,
        // .user_sp = PID0_STACK_CORE0,
        // .kernel_stack_base = PID0_STACK_CORE0,
        // .user_stack_base = PID0_STACK_CORE0,
        .status = TASK_RUNNING,
        .tid = 0,
        .child_t_num = 0,
        .core_mask = (1 << NR_CPUS) - 1,
        .pud = (PTE *)(PGDIR_PA + KERNEL_ADDR_BASE),
        .pud_lk = (spin_lock_t *)PID0_PUD_LK_ADDR
    },
    {
        .pid = 0,
        .kernel_sp = PID0_STACK_CORE1,
        // .user_sp = PID0_STACK_CORE1,
        // .kernel_stack_base = PID0_STACK_CORE1,
        // .user_stack_base = PID0_STACK_CORE1,
        .status = TASK_RUNNING,
        .tid = 0,
        .child_t_num = 0,
        .core_mask = (1 << NR_CPUS) - 1,
        .pud = (PTE *)(PGDIR_PA + KERNEL_ADDR_BASE),
        .pud_lk = (spin_lock_t *)PID0_PUD_LK_ADDR + sizeof(spin_lock_t)
    }
};

// initialize status queues
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);
LIST_HEAD(thread_exit_queue);

/* current running task PCB */
pcb_t * volatile current_running[NR_CPUS];

/* global process id */
pid_t process_id = 1; // protected by pcb_lk (only occured in one place)

// given an address of a "list_node_t" variable, to find the address of the corresponding PCB
pcb_t *find_PCB_addr(list_node_t *list_node)
{
    // pcb_t test_pcb;
    // uint64_t list_node_off = (uint64_t)(&test_pcb.list) - (uint64_t)(&test_pcb);
    return (pcb_t *)((ptr_t)list_node - LIST_NODE_OFF);
}

/*
 * fetch a ready task from the ready queue, and change it to the running state
 *
 * NOTE: this function will change my_cpu() result
 */
void exec_next_task(void) // in pcb_lk critical section
{
    list_node_t *next_node = ready_queue.next;
    pcb_t *pcb_next;
    int cpu_id = get_current_cpu_id();

    // search for a ready task THAT CAN run on this core
    while (next_node != &ready_queue) // a precess in the queue
    {
        pcb_next = find_PCB_addr(next_node);
        if ((pcb_next->core_mask & (1 << cpu_id)) && pcb_next->to_kill == false)
            break;
        next_node = next_node->next;
    }

    // check if there is no ready task
    if (next_node == &ready_queue)
        current_running[cpu_id] = my_idle_proc();
    else
    {
        // TODO: [p2-task1] Modify the current_running pointer.
        current_running[cpu_id] = pcb_next;
        list_del(&(pcb_next->list));
    }

    pcb_t *cpu_proc = my_cpu();
    cpu_proc->status = TASK_RUNNING;
    cpu_proc->core_id = cpu_id;
}

void switch_to_wrap(pcb_t *prev_proc, pcb_t *next_proc)
{
    // WRITE_LOG("switch from pid: %d to pid: %d\n", prev_proc->pid, next_proc->pid);

    set_satp(SATP_MODE_SV39, next_proc->pid, kva2pa((ptr_t)next_proc->pud) >> NORMAL_PAGE_SHIFT);

    // WRITE_LOG("successfully set satp\n");

    __sync_synchronize();
    local_flush_icache_all();
    local_flush_tlb_all();

    // WRITE_LOG("ready to call switch_to()\n");

    switch_to(prev_proc, next_proc);

    // WRITE_LOG("return from pid: %d to pid: %d\n", prev_proc->pid, next_proc->pid);
}

// lock the pcb_lk before calling this function, and unlock it after switch_to inside this function
void proc_schedule(list_node_t *dest_list_head, task_status_e task_status, spin_lock_t *lock)
{
    // store current_running before scheduling
    pcb_t *prev_proc = my_cpu();
    prev_proc->status = task_status;

    // add prev_proc to the ready queue
    if (prev_proc != my_idle_proc())
        list_add_tail(dest_list_head, &(prev_proc->list));

    // switch to the next task
    exec_next_task();

    if (lock != NULL)
        spin_lock_release(lock);

    switch_to_wrap(prev_proc, my_cpu());

    // spin_lock_release(&pcb_lk);
    /* leave critical section */
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_killed();
    check_sleeping();
    /************************************************************/
    // TODO: [p5-task3] Check send/recv queue to unblock PCBs
    /************************************************************/

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    proc_schedule(&ready_queue, TASK_READY, NULL);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.

    // set the wakeup time
    my_cpu()->wakeup_time = get_timer() + sleep_time;

    check_killed();
    check_sleeping();

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    proc_schedule(&sleep_queue, TASK_SLEEPING, NULL);
}

// add proc into
void do_block(list_head *queue, spin_lock_t *lock)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    proc_schedule(queue, TASK_BLOCKED, lock);

    // acquire the lock back from switching to this process
    if (lock != NULL)
        spin_lock_acquire(lock);
}

void do_unblock(list_node_t *pcb_node) // pcb_node must truly exists
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    // delete pcb_node from the blocked queue, and add it to the ready queue

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    list_del(pcb_node);
    list_add_tail(&ready_queue, pcb_node);
    pcb_t *proc = find_PCB_addr(pcb_node);
    proc->status = TASK_READY;

    spin_lock_release(&pcb_lk);
    /* leave critical section */
}

pcb_t *my_cpu(void) // no need to lock pcb_lk, since current proc will not change
{
    return current_running[get_current_cpu_id()];
}

pcb_t *another_cpu(void) // must be in critical section of pcb_lk
{
    return current_running[1 - get_current_cpu_id()]; // 1 == NR_CPUS-1
}

pcb_t *my_idle_proc(void) // no need to lock pcb_lk, since current proc will not change
{
    return pid0_pcb + get_current_cpu_id();
}
