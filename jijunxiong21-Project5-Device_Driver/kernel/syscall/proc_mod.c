// #include <os/task.h> // ignored
// #include <os/sched.h> // ignored
#include <os/task_tools.h>
#include <os/mm.h>
#include <os/irq.h>

void unblock_list(list_head *head)
{
    while (!list_empty(head))
    {
        // find the first task: is main thread, or is child thread of main thread
        pcb_t *pcb_to_free = find_PCB_addr(head->next);

        // free the blocked task from waitlist
        pcb_to_free->status = TASK_READY;
        list_del(&(pcb_to_free->list));
        list_add_tail(&ready_queue, &(pcb_to_free->list));
    }
}

static inline pcb_t *pcb_clear(pid_t pid, int pcb_idx) // in pcb_lk critical section
{
    pcb_t *pcb_to_kill;

    // pcb should has the same pid, and is not a zombie process; or find another one
    if (pcb[pcb_idx].pid != pid || pcb[pcb_idx].status == TASK_ZOMBIE)
        return NULL;

    // the "process to kill" is found
    pcb_to_kill = &pcb[pcb_idx]; // abreviation

    pcb_to_kill->status = TASK_ZOMBIE; // set status to zombie
    if (&pcb[pcb_idx] != my_cpu()) // remove from any queue
        list_del(&(pcb_to_kill->list));

    unblock_list(&(pcb_to_kill->wait_list));

    int mlock_ocp_tmp = pcb_to_kill->mlock_ocp;
    int mbox_ocp_tmp = pcb_to_kill->mbox_ocp;
    pcb_to_kill->mlock_ocp = 0;
    pcb_to_kill->mbox_ocp = 0;

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    // free the lock that this task holds
    for (int mlock_idx = 0; mlock_idx < LOCK_NUM; mlock_idx++)
        if ((mlock_ocp_tmp & ((uint64_t)1 << mlock_idx)) != 0)
            do_mutex_lock_free(mlock_idx, pid, 0);

    // free all the barrier that this task holds
    for (int barrier_idx = 0; barrier_idx < BARRIER_NUM; barrier_idx++)
        if (barriers[barrier_idx].pid == pid)
            do_barrier_destroy(barrier_idx);

    // free all the semaphores that this task holds
    for (int sema_idx = 0; sema_idx < SEMAPHORE_NUM; sema_idx++)
        if (semaphores[sema_idx].pid == pid)
            do_semaphore_destroy(sema_idx);

    // free all the message queues (mailbox) that this task holds
    for (int mailbox_idx = 0; mailbox_idx < MBOX_NUM; mailbox_idx++)
        if ((mbox_ocp_tmp & ((uint64_t)1 << mailbox_idx)) != 0)
            do_mbox_destroy(mailbox_idx);

    /* enter critical section */
    spin_lock_acquire(pcb_to_kill->pud_lk);

    // free the pages that this task holds
    for (int mem_idx = 0; mem_idx < pcb_to_kill->mem_num; mem_idx++)
        if (pcb_to_kill->mem_info[mem_idx].valid != 0)
            free_page_proc((ptr_t)pcb_to_kill->mem_info[mem_idx].pte_addr,
                           pcb_to_kill->mem_info);

    // free the whole page table
    free_tlb_page(pcb_to_kill->pud);

    spin_lock_release(pcb_to_kill->pud_lk);
    /* leave critical section */

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    return pcb_to_kill;
}

static bool leave_task(pid_t pid) // in pcb_lk critical section
{
    pcb_t *pcb_tmp;
    pcb_t *pcb_to_kill = NULL;

    // find all tasks with the same pid, and set their status to TASK_ZOMBIE
    int pcb_idx;
    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
        if ((pcb_tmp = pcb_clear(pid, pcb_idx)) != NULL)
            pcb_to_kill = pcb_tmp; // the "process to kill" exists

    // remove the task from run_tasks
    if (pcb_to_kill == NULL)
        return false;

    // else if the pcb exits
    int task_idx;

    for (task_idx = 0; task_idx < MAX_PROCESS_NUM; task_idx++)
        if (run_tasks[task_idx]->pid == pid)
            break;
    if (task_idx == MAX_PROCESS_NUM)
        return false; // the task is not in run_tasks

    for (int i = task_idx; i < run_task_num - 1; i++)
        run_tasks[i] = run_tasks[i + 1];
    run_tasks[run_task_num - 1] = NULL;
    run_task_num--;
    return true;
}

// syscall: exec
pid_t do_exec(char *name, int argc, char *argv[])
{
    int task_idx;
    // int reg_arg_num;
    pid_t pid;

    if ((task_idx = task_cmp(name, strlen(name))) == -1)
    {
        return 0;
    }
    // returning task index
    // if (task_exist_check(task_idx))
    // {
    //     printk("Task already exists!\n");
    //     return 0;
    // }

    if (argc >= MAX_ARGC)
    {
        printk("Too many arguments!\n");
        assert(0)
    }

    pid = init_pcb_args(task_idx, argc, argv);

    assert(pid != 0)

    WRITE_LOG("Execute pid: %d, task name: %s\n", pid, name)

    return pid;
}

// syscall: exit
void do_exit(void)
{
    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    pcb_t *prev_proc = my_cpu();
    pid_t pid = prev_proc->pid; // record the pid of this thread
    assert(pid != 0) // IDLE process can not exit

    // use kill to kill all the tasks with the same pid
    leave_task(pid);

    // switch to the next task (WILL CHANGE current_running!)
    exec_next_task();

    switch_to_wrap(prev_proc, my_cpu());

    // spin_lock_release(&pcb_lk);
    /* leave critical section */
}

// syscall: kill
int do_kill(pid_t pid)
{
    WRITE_LOG("kill pid: %d\n", pid)
    if (pid <= 1) // pid0_pcb and shell can not be killed
        return 0;

    pcb_t *another_cpu_proc;

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    if ((another_cpu_proc = another_cpu()) != NULL && another_cpu_proc->pid == pid) // here perhaps (current[?]->pid) need a lock
    {
        another_cpu_proc->to_kill = true;
        WRITE_LOG("kill another cpu, pid: %d, tid: %d\n", another_cpu_proc->pid, another_cpu_proc->tid)
        for (int i = 0; i < NUM_MAX_TASK; i++)
            if (pcb[i].pid == pid)
                pcb[i].to_kill = true;
    }
    else if (my_cpu()->pid == pid)
        do_exit();
    else if (!leave_task(pid))// record whther the pid exists
        pid = 0;

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    return pid;
}

// syscall: fork
pid_t do_fork(void)
{
    WRITE_LOG("fork pid: %d\n", my_cpu()->pid)
    /* 
     * fork():
     * parent process returning pid of child process
     * child process returning 0
     */

    pcb_t *cpu_proc = my_cpu();

    assert(cpu_proc->tid == 0) // NOTE: child thread are not allowed to build new threads

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);
    
    // find an available pcb
    int pcb_idx;
    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx) // alloc all tasks with corresponding PCB
        if (pcb[pcb_idx].status == TASK_ZOMBIE)
            break;
    if (pcb_idx == NUM_MAX_TASK)
    {
        printk("No more PCB!\n");
        assert(0)
    }

    // copy relevant information from parent process
    pcb_t *new_proc = &pcb[pcb_idx];

    // copy the page table
    new_proc->pud_lk = user_pud_lk_base + pcb_idx;
    
    /* enter critical section */
    spin_lock_acquire(new_proc->pud_lk);
    spin_lock_acquire(cpu_proc->pud_lk);

    new_proc->pud = (PTE *)(user_pud + pcb_idx * PAGE_SIZE);
    fork_copy_pgtable(new_proc->pud, cpu_proc->pud);

    // copy memory information
    new_proc->mem_num = cpu_proc->mem_num;
    new_proc->mem_info = user_mem_info_base + pcb_idx * MAX_MEM_SECTION;
    for (int mem_idx = 0; mem_idx < MAX_MEM_SECTION; mem_idx++)
        new_proc->mem_info[mem_idx] = cpu_proc->mem_info[mem_idx];

    // // setup virtual page copy-on-write flag
    // for (int mem_idx = 0; mem_idx < new_proc->mem_num; mem_idx++)
    //     if (new_proc->mem_info[mem_idx].valid != false)
    //     {
    //         PTE *pte = new_proc->mem_info[mem_idx].pte_addr;
    //         int page_num = new_proc->mem_info[mem_idx].page_num;
    //         for (int i = 0; i < page_num; i++) // for every page, set the FORKED flag, clear write flag
    //         {
    //             set_attribute(pte, _PAGE_FORKED);
    //             clear_attribute(pte, _PAGE_WRITE);
    //             pte += 1;
    //         }
    //     }

    // set up stack
    new_proc->kernel_stack_base = alloc_page_proc(new_proc->mem_info, KERNEL_STACK_PAGE, 0, 0) + PAGE_SIZE * KERNEL_STACK_PAGE; // alloc kernel stack
    new_proc->kernel_sp = new_proc->kernel_stack_base;

    spin_lock_release(new_proc->pud_lk);
    spin_lock_release(cpu_proc->pud_lk);
    /* leave critical section */

    new_proc->user_stack_base = cpu_proc->user_stack_base; // copy user va from old proc
    new_proc->user_sp = cpu_proc->user_sp; // copy user va from old proc

    // add the child process to ready queue
    new_proc->status = TASK_READY;
    list_add_tail(&ready_queue, &(new_proc->list));

    // change the a0 register in trapframe
    regs_context_t *cpu_trap = (regs_context_t *)(cpu_proc->kernel_stack_base - sizeof(regs_context_t));
    regs_context_t *new_trap = (regs_context_t *)(new_proc->kernel_stack_base - sizeof(regs_context_t));
    memcpy((void *)new_trap, (void *)cpu_trap, sizeof(regs_context_t));
    new_trap->regs[TRAP_A(0)] = 0; // child process return 0
    // handle_syscall() automatically add 4 to the sepc

    // copy the context
    // memcpy((void *)&new_proc->context, (void *)&cpu_proc->context, sizeof(switchto_context_t));

    new_proc->context.regs[SWITCH_RA] = (reg_t)ret_from_exception;
    new_proc->context.regs[SWITCH_SP] = (new_proc->kernel_stack_base - sizeof(regs_context_t) - CONTEXT_SIMU_BUFFER) & (reg_t)-16;

    // assign a pid to the child process
    new_proc->pid = process_id++;

    // copy cursor
    new_proc->cursor_x = cpu_proc->cursor_x;
    new_proc->cursor_y = cpu_proc->cursor_y;

    // copy task info
    new_proc->task_id = cpu_proc->task_id;
    strncpy(new_proc->name, cpu_proc->name, MAX_TASK_NAME_LEN);
    new_proc->name[MAX_TASK_NAME_LEN - 1] = '\0';

    // copy core info
    new_proc->core_id = cpu_proc->core_id;
    new_proc->core_mask = cpu_proc->core_mask;

    // copy thread infomation
    new_proc->child_t_num = 0;
    new_proc->tid = 0;

    // wakeup time
    new_proc->wakeup_time = cpu_proc->wakeup_time;

    // copy ipc signs
    new_proc->mlock_ocp = 0;
    new_proc->mbox_ocp = 0;

    // copy to_kill flag
    new_proc->to_kill = 0;

    // intialize wait_list
    new_proc->wait_list = (list_head){&(new_proc->wait_list), &(new_proc->wait_list)};

    run_tasks[run_task_num++] = new_proc;


    spin_lock_release(&pcb_lk);
    /* leave critical section */

    WRITE_LOG("Fork pid: %d to pid: %d, task name: %s\n", cpu_proc->pid, new_proc->pid, new_proc->name)

    return new_proc->pid;
}
