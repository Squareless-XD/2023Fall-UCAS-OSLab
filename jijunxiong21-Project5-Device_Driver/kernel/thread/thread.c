#include <os/thread.h>
#include <os/mm.h>
#include <screen.h>
// #include <os/procst.h>
// #include <os/loader.h>
#include <os/task_tools.h>

int do_thread_create(void (*start_routine)(void*), ptr_t mem_top, ptr_t ra, void* arg)
{
    pcb_t *cpu_proc = my_cpu();
    // kernel process cannot create thread
    if (cpu_proc->pid == 0)
    {
        printk("Kernel process cannot create thread!\n");
        assert(0)
    }
    if (cpu_proc->tid != 0)
    {
        printk("Child thread cannot create another thread!\n");
        assert(0)
    }
    if (cpu_proc->child_t_num >= MAX_THREAD_NUM)
    {
        printk("Too many threads!\n");
        assert(0)
    }

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // find a available pcb
    int pcb_idx;
    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx) // alloc all tasks with corresponding PCB
        if (pcb[pcb_idx].status == TASK_ZOMBIE)
            break;
    // if (pcb_idx == NUM_MAX_TASK)
    // {
    //     printk("No more PCB!\n");
    //     return 0;
    // }c
    assert(pcb_idx < NUM_MAX_TASK)
    pcb_t *new_proc = &pcb[pcb_idx];

    int tid = ++(cpu_proc->child_t_num); // increase the number of child threads

    /* enter critical section */
    spin_lock_acquire(cpu_proc->pud_lk);

    new_proc->pud = cpu_proc->pud; // copy kernel PTE into process
    new_proc->pud_lk = cpu_proc->pud_lk; // NOTE: sharing the same pud_lk with main thread
    new_proc->mem_num = cpu_proc->mem_num; // have no memory section now

    new_proc->mem_info = cpu_proc->mem_info; // copy mem_info into process

    /* NOTE: ALLOCATION OF THREAD STACK SHOULD BE MODIFIED WHEN EXITION OF THREAD IS SUPPORTED */
    new_proc->kernel_stack_base = alloc_page_proc(new_proc->mem_info, KERNEL_STACK_PAGE, 0, 0) + PAGE_SIZE * KERNEL_STACK_PAGE; // alloc kernel stack
    new_proc->kernel_sp = new_proc->kernel_stack_base; // alloc kernel stack

    spin_lock_release(cpu_proc->pud_lk);
    /* leave critical section */

    new_proc->user_stack_base = mem_top & ~(PAGE_SIZE - 1); // alloc user stack
    new_proc->user_sp = new_proc->user_stack_base; // alloc user stack
    /* NOTE: ALLOCATION OF THREAD STACK SHOULD BE MODIFIED WHEN EXITION OF THREAD IS SUPPORTED */
    // printk("kernel_sp: %x, user_sp: %x\n", new_proc->kernel_sp + sizeof(regs_context_t), new_proc->user_sp);

    list_add_tail(&ready_queue, &(new_proc->list)); // add to ready queue
    new_proc->wait_list.prev = new_proc->wait_list.next = &(new_proc->wait_list); // init wait list
    new_proc->pid = cpu_proc->pid;
    new_proc->status = TASK_READY; // set status to ready
    new_proc->cursor_x = 0; // set cursor to (0, 0)
    new_proc->cursor_y = 0;
    strncpy(new_proc->name, cpu_proc->name, MAX_TASK_NAME_LEN - 1); // copy task name
    new_proc->name[MAX_TASK_NAME_LEN - 1] = '\0';
    new_proc->task_id = cpu_proc->task_id; // assign task_id
    new_proc->tid = tid; // after main thread's child thread number hase increased

    /* NOTE: SHOULD BE CHANGED WHEN EXITION OF THREAD IS SUPPORTED */
    new_proc->core_id = NR_CPUS;
    new_proc->core_mask = cpu_proc->core_mask; // the core mask that the process can run on
    new_proc->to_kill = false; // when a process is to be killed but running on another core


    init_pcb_stack(new_proc->kernel_stack_base, new_proc->user_stack_base, (uint64_t)start_routine, new_proc, (reg_t)arg, 0ul, ra);


    // screen_move_cursor(0, 6 + cpu_proc->pid);
    // printk(">[task] kenel: pid: %d, tid %d", cpu_proc->pid, new_proc->tid);

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    return new_proc->tid; // new thread's tid
}


void do_thread_yield()
{
    pcb_t *cpu_proc = my_cpu();
    // kernel process cannot be yielded
    if (cpu_proc->pid == 0)
    {
        printk("Kernel process cannot be yielded to other thread!\n");
        return;
    }

    // if this is a main thread, and has no child thread, then nothing should be done
    if (cpu_proc->tid == 0 && cpu_proc->child_t_num == 0)
    {
        printk("No thread to yield to!!\n");
        return;
    }

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // store current_running before scheduling
    pcb_t *prev_proc = cpu_proc;

    // record the pid of this thread
    pid_t pid = prev_proc->pid;
    tid_t tid = prev_proc->tid;

    // switch to the next task
    if (!list_empty(&ready_queue))
    {
        pcb_t *pcb_next;
        list_node_t *list_node = ready_queue.next;
        while (list_node != &ready_queue) // until no more task in the ready queue
        {
            // find the first task: is main thread, or is child thread of main thread
            pcb_next = find_PCB_addr(list_node);

            // if find any task with the same pid but different tid, then this might be the thread-yield target
            if (pcb_next->pid == pid && pcb_next->tid != tid
                && pcb_next->core_mask & (1 << get_current_cpu_id()))
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
        // if the task found is not prev_proc itself, then switch to it
        else
        {
            current_running[get_current_cpu_id()] = pcb_next;
            list_del(list_node);
        }
    }
    else
    {
        printk("No thread to yield to!\n");
        return;
    }

    prev_proc->status = TASK_READY;
    // add prev_proc to the ready queue
    list_add_tail(&ready_queue, &(prev_proc->list));

    cpu_proc->status = TASK_RUNNING;
    cpu_proc->core_id = get_current_cpu_id();

    switch_to_wrap(prev_proc, cpu_proc);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */
}

int do_thread_join(pthread_t thread)
{
    // assert(0)
    pcb_t *cpu_proc = my_cpu();
    // kernel process cannot be yielded
    if (cpu_proc->pid == 0)
    {
        printk("Kernel process cannot be yielded to other thread!\n");
        return -1;
    }

    // screen_move_cursor(0, 6 + cpu_proc->pid);
    // printk(">[task] kenel: pid: %d, tid %d", cpu_proc->pid, cpu_proc->tid);

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // find the thread to join
    int pcb_idx = NUM_MAX_TASK;
    while (pcb_idx == NUM_MAX_TASK)
    {
        for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
            if (pcb[pcb_idx].status != TASK_ZOMBIE &&
                pcb[pcb_idx].pid == cpu_proc->pid && pcb[pcb_idx].tid == thread)
                break;
        if (pcb_idx == NUM_MAX_TASK) // no thread found (or thread had already exited)
        {
            proc_schedule(&ready_queue, TASK_READY, NULL);
            /* enter critical section */
            spin_lock_acquire(&pcb_lk);
        }
    }

    pcb_t *pcb_join = &pcb[pcb_idx];

    // wait until the thread is exited
    if (pcb_join->status != TASK_EXITED)
    {
        // put the current task into waiting queue of this process, and change to other task
        proc_schedule(&pcb_join->wait_list, TASK_WAITING, NULL);
        /* enter critical section */
        spin_lock_acquire(&pcb_lk);
    }

    unblock_list(&pcb_join->wait_list);

    // clear the pcb of the exited thread
    pcb_join->mlock_ocp = 0;
    pcb_join->mbox_ocp = 0;
    pcb_join->status = TASK_ZOMBIE;

    list_del(&pcb_join->list);

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    return 0;
}

static void add_mem_info(pcb_t *main_proc, pcb_t *proc_clear) // in pcb_lk critical section
{
    // copy mem_info into main_proc (with what the task now don't have)
    // NOTE: needs double for-loop

    /* enter critial section */
    spin_lock_acquire(main_proc->pud_lk);

    for (int to_mvoe_idx = 0; to_mvoe_idx < MAX_MEM_SECTION; to_mvoe_idx++)
    {
        if (proc_clear->mem_info[to_mvoe_idx].valid == false) // if not valid
            continue;
        int main_mem_idx;
        for (main_mem_idx = 0; main_mem_idx < MAX_MEM_SECTION; main_mem_idx++)
            if (main_proc->mem_info[main_mem_idx].valid == true &&
                main_proc->mem_info[main_mem_idx].va == proc_clear->mem_info[to_mvoe_idx].va)
              break;
        if (main_mem_idx < MAX_MEM_SECTION) // if found
            continue;

        // if not found
        for (main_mem_idx = 0; main_mem_idx < MAX_MEM_SECTION; main_mem_idx++)
            if (main_proc->mem_info[main_mem_idx].valid == false)
                break;
        if (main_mem_idx == MAX_MEM_SECTION) // if not found
            assert(0) // no more mem_info space
        // if found space
        main_proc->mem_info[main_mem_idx] = proc_clear->mem_info[to_mvoe_idx];
        main_proc->mem_num++;
    }

    spin_lock_release(main_proc->pud_lk);
    /* leave critial section */
}

static void clear_pcb_thread(pcb_t *proc_kill) // in pcb_lk critical section
{
    // proc_kill->status = TASK_EXITED; // set status to zombie
    // if (proc_kill != my_cpu()) // remove from any queue
    //     list_del(&proc_kill->list);
    // list_add_head(&thread_exit_queue, &proc_kill->list);

    // release all the nodes in the corresponding waitlist
    unblock_list(&(proc_kill->wait_list));

    int mlock_ocp_tmp = proc_kill->mlock_ocp;
    // int mbox_ocp_tmp = proc_kill->mbox_ocp;
    proc_kill->mlock_ocp = 0;
    proc_kill->mbox_ocp = 0;

    pid_t pid = proc_kill->pid;
    pthread_t tid = proc_kill->tid;

    // free the lock that this task holds
    for (int mlock_idx = 0; mlock_idx < LOCK_NUM; mlock_idx++)
        if ((mlock_ocp_tmp & ((uint64_t)1 << mlock_idx)) != 0)
            do_mutex_lock_free(mlock_idx, pid, tid);

    // barrier do not needs to be released
    // semaphores do not needs to be released
    // mailbox do not needs to be released

    // add mem_info to main thread
    int main_thread_idx;
    for (main_thread_idx = 0; main_thread_idx < NUM_MAX_TASK; main_thread_idx++)
        if (pcb[main_thread_idx].status != TASK_ZOMBIE &&
            pcb[main_thread_idx].pid == pid && pcb[main_thread_idx].tid == 0)
            break;
    if (main_thread_idx == NUM_MAX_TASK) // if not found
        assert(0) // no main thread
    add_mem_info(pcb + main_thread_idx, proc_kill);
}

void do_thread_exit() // NOTE: must be called by the thread itself
{
    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    pcb_t *cpu_proc = my_cpu();

    // only child thread itself could call this function
    assert(cpu_proc->tid != 0)

    clear_pcb_thread(cpu_proc); // record whther the pid exists

    // screen_move_cursor(0, 11 + cpu_proc->pid);
    // printk(">[task] thread_exit(): pid: %d, tid %d", cpu_proc->pid, cpu_proc->tid);

    // wake up all the tasks in the waitlist
    unblock_list(&cpu_proc->wait_list);

    proc_schedule(&thread_exit_queue, TASK_EXITED, NULL);
}
