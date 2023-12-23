#include <os/list.h>
#include <os/lock.h>
#include <os/sched.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/procst.h>
#include <os/task_tools.h>

pcb_t pcb[NUM_MAX_TASK];
const ptr_t pid0_stack = INIT_KERNEL_STACK + PAGE_SIZE;
pcb_t pid0_pcb = {
    .pid = 0,
    .kernel_sp = (ptr_t)pid0_stack,
    .user_sp = (ptr_t)pid0_stack
};

// initialize status queues
LIST_HEAD(ready_queue);
LIST_HEAD(sleep_queue);

/* current running task PCB */
pcb_t * volatile current_running;

/* global process id */
pid_t process_id = 1;

// given an address of a "list_node_t" variable, to find the address of the corresponding PCB
uint64_t find_PCB_addr(list_node_t *list_node)
{
    // pcb_t test_pcb;
    // uint64_t list_node_off = (uint64_t)(&test_pcb.list) - (uint64_t)(&test_pcb);
    return (uint64_t)list_node - LIST_NODE_OFF;
}

// fetch a ready task from the ready queue, and change it to the running state
void exec_next_task(void)
{
    if (!list_empty(&ready_queue))
    {
        current_running = (pcb_t *)find_PCB_addr((&ready_queue)->next);
        list_del(&(current_running->list));
    }
    else
        current_running = &pid0_pcb;

    current_running->status = TASK_RUNNING;
}

void do_scheduler(void)
{
    // TODO: [p2-task3] Check sleep queue to wake up PCBs
    check_sleeping();

    /************************************************************/
    /* Do not touch this comment. Reserved for future projects. */
    /************************************************************/

    // TODO: [p2-task1] Modify the current_running pointer.

    // store current_running before scheduling
    pcb_t *prev_running = current_running;
    prev_running->status = TASK_READY;

    // add prev_running to the ready queue
    if (prev_running != &pid0_pcb)
        list_add_tail(&ready_queue, &(prev_running->list));

    // switch to the next task
    exec_next_task();

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_running, current_running);
}

void do_sleep(uint32_t sleep_time)
{
    // TODO: [p2-task3] sleep(seconds)
    // NOTE: you can assume: 1 second = 1 `timebase` ticks
    // 1. block the current_running
    // 2. set the wake up time for the blocked task
    // 3. reschedule because the current_running is blocked.

    check_sleeping();

    // set the wakeup time
    current_running->created_time = get_timer();
    current_running->wakeup_time = sleep_time;

    // store current_running before scheduling
    pcb_t *prev_running = current_running;
    prev_running->status = TASK_SLEEPING;

    // add current_running to the sleep queue
    if (prev_running != &pid0_pcb)
        list_add_tail(&sleep_queue, &(prev_running->list));

    // switch to the next task
    exec_next_task();

    switch_to(prev_running, current_running);
}

void do_block(list_node_t *pcb_node, list_head *queue)
{
    // TODO: [p2-task2] block the pcb task into the block queue

    // store current_running before scheduling
    pcb_t *prev_running = current_running;
    prev_running->status = TASK_BLOCKED;

    // add the node (current_running actually) into the block queue, and reschedule
    if (prev_running != &pid0_pcb)
        list_add_tail(queue, pcb_node);

    // switch to the next task
    exec_next_task();

    switch_to(prev_running, current_running);
}

void do_unblock(list_node_t *pcb_node)
{
    // TODO: [p2-task2] unblock the `pcb` from the block queue
    // delete pcb_node from the blocked queue, and add it to the ready queue
    list_del(pcb_node);
    list_add_tail(&ready_queue, pcb_node);
    pcb_t *pcb_now = (pcb_t *)find_PCB_addr(pcb_node);
    pcb_now->status = TASK_READY;
}
