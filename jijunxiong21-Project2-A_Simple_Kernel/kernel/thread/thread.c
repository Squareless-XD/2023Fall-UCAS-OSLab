#include <os/list.h>
// #include <os/lock.h>
// #include <os/sched.h>
// #include <os/time.h>
#include <os/mm.h>
// #include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/procst.h>
#include <os/loader.h>
#include <os/task_tools.h>
#include <os/string.h>
#include <os/thread.h>



int thread_create(void func_p(), int args_num, ...)
{
    // kernel process cannot create thread
    if (current_running->pid == 0)
    {
        printk("Kernel process cannot create thread!\n");
        return 0;
    }
    if (current_running->tid != 0)
    {
        printk("Child thread cannot create another thread!\n");
        return 0;
    }
    if (current_running->child_t_num >= MAX_THREAD_NUM)
    {
        printk("Too many threads!\n");
        return 0;
    }

    // create a new pcb
    int pcb_idx;
    pcb_t *pcb_avail;
    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx) // alloc all tasks with corresponding PCB
        if (pcb[pcb_idx].status == TASK_EXITED)
            break;
    if (pcb_idx == NUM_MAX_TASK)
    {
        printk("No more PCB!\n");
        assert(1);
    }

    pcb_avail = &pcb[pcb_idx];

    int tid = current_running->child_t_num + 1;

    /* NOTE: ALLOCATION OF THREAD STACK SHOULD BE MODIFIED WHEN EXITION OF THREAD IS SUPPORTED */
    pcb_avail->kernel_sp = current_running->kernel_sp - (ALLOC_KERNEL_PAGE_NUM - tid) * PAGE_SIZE; // alloc kernel stack
    pcb_avail->user_sp = current_running->user_sp - (ALLOC_USER_PAGE_NUM - tid) * PAGE_SIZE; // alloc user stack
    /* NOTE: ALLOCATION OF THREAD STACK SHOULD BE MODIFIED WHEN EXITION OF THREAD IS SUPPORTED */
    // printk("kernel_sp: %x, user_sp: %x\n", pcb_avail->kernel_sp + sizeof(regs_context_t), pcb_avail->user_sp);

    list_add_tail(&ready_queue, &(pcb_avail->list)); // add to ready queue
    pcb_avail->pid = current_running->pid; // assign pid, and increase process_id automatically
    pcb_avail->status = TASK_READY; // set status to ready
    pcb_avail->cursor_x = 0; // set cursor to (0, 0)
    pcb_avail->cursor_y = 0;
    strncpy(pcb_avail->name, current_running->name, MAX_TASK_NAME_LEN - 1); // copy task name
    // char str_thr_idx[6] = "_thr";
    // str_thr_idx[5] = tid + '0';
    // strcat(pcb_avail->name, str_thr_idx); // add "_thr?" to the end of the name
    pcb_avail->name[MAX_TASK_NAME_LEN - 1] = '\0';
    pcb_avail->tid = tid; // set tid to 0

    /* NOTE: SHOULD BE CHANGED WHEN EXITION OF THREAD IS SUPPORTED */
    (current_running->child_t_num)++; // set child thread number to tid

    // pcb_avail->child_t_num = current_running->pid;

    init_pcb_stack(pcb_avail->kernel_sp, pcb_avail->user_sp, (uint64_t)func_p, &pcb[pcb_idx]);

    // RA and return value not supported now
    regs_context_t *trap_frame = (regs_context_t *)(pcb_avail->kernel_sp);
    va_list valist; // valist is a variable type
    va_start(valist, args_num); // init valist
    for (int i = 0; i < args_num; i++)
    {
        trap_frame->regs[TRAP_A(i)] = va_arg(valist, reg_t); // set the value of the its arguments
    }
    va_end(valist); // end valist

    return tid; // new thread's tid
}


void thread_yield()
{
    // kernel process cannot be yielded
    if (current_running->pid == 0)
    {
        printk("Kernel process cannot be yielded to other thread!\n");
        return;
    }

    // if this is a main thread, and has no child thread, then nothing should be done
    if (current_running->tid == 0 && current_running->child_t_num == 0)
    {
        printk("No thread to yield to!!\n");
        return;
    }

    // store current_running before scheduling
    pcb_t *prev_running = current_running;

    // record the pid of this thread
    pid_t pid = prev_running->pid;
    tid_t tid = prev_running->tid;

    // switch to the next task
    if (!list_empty(&ready_queue))
    {
        pcb_t *pcb_next;
        list_node_t *list_node = ready_queue.next;
        while (list_node != &ready_queue) // until no more task in the ready queue
        {
            // find the first task: is main thread, or is child thread of main thread
            pcb_next = (pcb_t *)find_PCB_addr(list_node);

            // if find any task with the same pid but different tid, then this might be the thread-yield target
            if (pcb_next->pid == pid && pcb_next->tid != tid)
                break;

            // if not, try to match the next task, and continue
            list_node = list_node->next;
        }
        // test if no task is found
        if (list_node == &ready_queue)
        {
            printk("No thread to yield to!\n");
            return;
        }
        // if the task found is not prev_running itself, then switch to it
        else
        {
            current_running = pcb_next;
            list_del(list_node);
        }
    }
    else
    {
        printk("No thread to yield to!\n");
        return;
    }

    prev_running->status = TASK_READY;
    // add prev_running to the ready queue
    list_add_tail(&ready_queue, &(prev_running->list));

    current_running->status = TASK_RUNNING;

    // switch_to current_running
    switch_to(prev_running, current_running);
}
