// #include <os/time.h> // no need
// #include <os/sched.h> // ignored
#include <os/syscall.h>

uint64_t time_elapsed = 0;
uint64_t time_base = 0;

uint64_t get_ticks()
{
    __asm__ __volatile__(
        "rdtime %0"
        : "=r"(time_elapsed));
    return time_elapsed;
}

uint64_t get_timer()
{
    return get_ticks() / time_base;
}

uint64_t get_time_base()
{
    return time_base;
}

void latency(uint64_t time)
{
    uint64_t begin_time = get_timer();

    while (get_timer() - begin_time < time);
    return;
}

void check_sleeping(void) // in pcb_lk critical section
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    // iter the sleep queue
    list_node_t *node = sleep_queue.next;
    list_node_t *next_node;

    while (node != &sleep_queue)
    {
        pcb_t *pcb_check = (pcb_t *)((uint64_t)node - LIST_NODE_OFF);
        next_node = node->next;
        if (pcb_check->wakeup_time <= get_timer()) // if the task should wake up
        {
            pcb_check->status = TASK_READY;
            list_del(node);
            list_add_tail(&ready_queue, node);
        }
        node = next_node;
    }
}

void check_killed(void) // might call exit()
{
    if (atomic_swap(false, (ptr_t)&my_cpu()->to_kill))
        do_exit();
}