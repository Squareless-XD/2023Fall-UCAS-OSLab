#include <os/list.h>
#include <os/sched.h>
#include <type.h>

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

void check_sleeping(void)
{
    // TODO: [p2-task3] Pick out tasks that should wake up from the sleep queue
    // iter the sleep queue
    list_node_t *node = sleep_queue.next;
    list_node_t *next_node;
    while (node != &sleep_queue)
    {
        pcb_t *pcb = (pcb_t *)((uint64_t)node - LIST_NODE_OFF);
        next_node = node->next;
        if (pcb->created_time + pcb->wakeup_time <= get_timer()) // if the task should wake up
        {
            pcb->status = TASK_READY;
            list_del(node);
            list_add_tail(&ready_queue, node);
        }
        node = next_node;
    }
}