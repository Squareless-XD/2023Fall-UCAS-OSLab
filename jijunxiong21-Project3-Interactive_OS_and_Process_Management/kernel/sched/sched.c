#include <os/sched.h>

pcb_t pcb[NUM_MAX_TASK];
spin_lock_t pcb_lk = {UNLOCKED};

static const ptr_t pid0_stack_core0 = INIT_KERNEL_STACK;
static const ptr_t pid0_stack_core1 = INIT_KERNEL_STACK + SEC_CORE_PID0_PAGE_NUM * PAGE_SIZE;

pcb_t pid0_pcb[NR_CPUS] = {
    {
        .pid = 0,
        .kernel_sp = (ptr_t)pid0_stack_core0,
        .user_sp = (ptr_t)pid0_stack_core0,
        .status = TASK_RUNNING,
        .tid = 0,
        .child_t_num = 0,
        .core_mask = 0x3,
    },
    {
        .pid = 0,
        .kernel_sp = (ptr_t)pid0_stack_core1,
        .user_sp = (ptr_t)pid0_stack_core1,
        .status = TASK_RUNNING,
        .tid = 0,
        .child_t_num = 0,
        .core_mask = 0x3,
    }
};

// initialize status queues
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

// spin_lock_t ready_queue_lk = {UNLOCKED};
// spin_lock_t sleep_queue_lk = {UNLOCKED};

/* current running task PCB */
pcb_t * volatile current_running[NR_CPUS];

/* global process id */
pid_t process_id = 1; // protected by pcb_lk (only occured in one place)

// given an address of a "list_node_t" variable, to find the address of the corresponding PCB
uint64_t find_PCB_addr(list_node_t *list_node)
{
    // pcb_t test_pcb;
    // uint64_t list_node_off = (uint64_t)(&test_pcb.list) - (uint64_t)(&test_pcb);
    return (uint64_t)list_node - LIST_NODE_OFF;
}

// fetch a ready task from the ready queue, and change it to the running state
void exec_next_task(void) // in pcb_lk critical section
{
    int core_id = get_current_cpu_id();
    
    list_node_t *next_node = &ready_queue;
    pcb_t *pcb_next;

    // search for a ready task THAT CAN run on this core
    while (next_node->next != &ready_queue) // a precess in the queue
    {
        next_node = next_node->next;
        pcb_next = (pcb_t *)find_PCB_addr(next_node);
        if (pcb_next->core_mask & (1 << core_id))
        {
            current_running[core_id] = pcb_next;
            list_del(&(pcb_next->list));
            break;
        }
    }

    // check if there is no ready task
    if (next_node->next == &ready_queue)
        current_running[core_id] = &pid0_pcb[core_id];

    current_running[core_id]->status = TASK_RUNNING;
    current_running[core_id]->core_id = core_id;
    // if (core_id)
    // {
    //     screen_move_cursor(0, 10);
    //     printk("Core 1: some process going in.\n");
    // }
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    int core_id = get_current_cpu_id();
    check_killed();
    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    check_sleeping();

    // store current_running before scheduling
    pcb_t *prev_running = current_running[core_id];
    prev_running->status = TASK_READY;

    // add prev_running to the ready queue
    if (prev_running != &pid0_pcb[core_id])
        list_add_tail(&ready_queue, &(prev_running->list));

    // switch to the next task
    exec_next_task();

    // unlock_kernel();

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_running, current_running[core_id]);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */

    // lock_kernel();
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.
    int core_id = get_current_cpu_id();
    check_killed();

    // set the wakeup time
    current_running[core_id]->wakeup_time = get_timer() + sleep_time;

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    check_sleeping();

    // store current_running before scheduling
    pcb_t *prev_running = current_running[core_id];
    prev_running->status = TASK_SLEEPING;

    // add current_running to the sleep queue
    if (prev_running != &pid0_pcb[core_id])
        list_add_tail(&sleep_queue, &(prev_running->list));

    // switch to the next task
    exec_next_task();

    // unlock_kernel();

    switch_to(prev_running, current_running[core_id]);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */

    // lock_kernel();
}

void do_block(list_node_t *pcb_node, list_head *queue, spin_lock_t *lock)
{
    // TODO: [p2-task2] block the pcb task into the block queue
    int core_id = get_current_cpu_id();

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // store current_running before scheduling
    pcb_t *prev_running = current_running[core_id];
    prev_running->status = TASK_BLOCKED;

    // add the node (current_running actually) into the block queue, and reschedule
    if (prev_running != &pid0_pcb[core_id])
        list_add_tail(queue, pcb_node);

    // switch to the next task
    exec_next_task();

    // release the lock
    spin_lock_release(lock);

    // unlock_kernel();

    // switch to the next task
    switch_to(prev_running, current_running[core_id]);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */

    // lock_kernel();

    // acquire the lock back from switching to this process
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
    pcb_t *pcb_now = (pcb_t *)find_PCB_addr(pcb_node);
    pcb_now->status = TASK_READY;

    spin_lock_release(&pcb_lk);
    /* leave critical section */
}
