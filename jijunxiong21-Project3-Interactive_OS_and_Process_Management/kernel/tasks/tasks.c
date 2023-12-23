#include <os/tasks.h>

static const char *task_stat_str[TASK_STAT_NUM] = {
    "TASK_BLOCKED",   // blocked
    "TASK_RUNNING",   // running
    "TASK_READY",     // ready
    "TASK_ZOMBIE",    // zombie
    "TASK_SLEEPING",  // sleeping
    "TASK_WAITING",   // waiting
};

static const char blank_line[SCREEN_WIDTH + 1] = {
    [0 ... SCREEN_WIDTH - 1] = ' ',
    [SCREEN_WIDTH] = '\0'
};

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
        assert(0);
    }

    pid = init_pcb_args(task_idx, argc, argv);

    assert(pid != 0);

    return pid;
}

static inline pcb_t *pcb_clear(pid_t pid, int pcb_idx)
{
    pcb_t *pcb_to_kill;

    // pcb should has the same pid, and is not a zombie process; or find another one
    if (pcb[pcb_idx].pid != pid || pcb[pcb_idx].status == TASK_ZOMBIE)
        return NULL;

    // the "process to kill" is found
    pcb_to_kill = &pcb[pcb_idx]; // abreviation

    pcb_to_kill->status = TASK_ZOMBIE; // set status to zombie
    int core_id = get_current_cpu_id();
    if (&pcb[pcb_idx] != current_running[core_id]) // remove from any queue
        list_del(&(pcb_to_kill->list));

    // release all the nodes in the corresponding waitlist
    pcb_t *pcb_to_free;
    while (!list_empty(&(pcb_to_kill->wait_list)))
    {
        // find the first task: is main thread, or is child thread of main thread
        pcb_to_free = (pcb_t *)find_PCB_addr(pcb_to_kill->wait_list.next);

        // free the blocked task from waitlist
        pcb_to_free->status = TASK_READY;
        list_del(&(pcb_to_free->list));
        list_add_tail(&ready_queue, &(pcb_to_free->list));
    }

    int mlock_ocp_tmp = pcb_to_kill->mlock_ocp;
    int mbox_ocp_tmp = pcb_to_kill->mbox_ocp;
    pcb_to_kill->mlock_ocp = 0;
    pcb_to_kill->mbox_ocp = 0;

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    // free the lock that this task holds
    for (int mlock_idx = 0; mlock_idx < LOCK_NUM; mlock_idx++)
        if ((mlock_ocp_tmp & ((uint64_t)1 << mlock_idx)) != 0)
            do_mutex_lock_free(mlock_idx, pid);

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
    spin_lock_acquire(&pcb_lk);

    return pcb_to_kill;
}

static bool leave_task(pid_t pid) // in pcb_lk critical section
{
    pcb_t *pcb_tmp;
    pcb_t *pcb_to_kill;

    // find all tasks with the same pid, and set their status to TASK_ZOMBIE
    int pcb_idx;
    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
        if ((pcb_tmp = pcb_clear(pid, pcb_idx)) != NULL)
            pcb_to_kill = pcb_tmp; // the "process to kill" exists

    // remove the task from run_tasks
    if (pcb_to_kill != NULL)
    {
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
    return false;
}

void do_exit(void)
{
    // store current_running before scheduling
    int core_id = get_current_cpu_id();

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    pcb_t *prev_running = current_running[core_id];

    // record the pid of this thread
    pid_t pid = current_running[core_id]->pid;

    assert(pid != 0);

    // use kill to kill all the tasks with the same pid
    leave_task(pid);

    // switch to the next task (WILL CHANGE current_running!)
    exec_next_task();

    // unlock_kernel();

    switch_to(prev_running, current_running[core_id]);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */

    // lock_kernel();
}

int do_kill(pid_t pid)
{
    // if (pid == 0) // this will reflect on leave_task cannot find the pid0_pcb
    //     return 0;
    int core_id = get_current_cpu_id();
    int pid_ret = pid;

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    if (current_running[core_id]->pid == pid)
        do_exit();
    else if (current_running[1 - core_id]->pid == pid) // here perhaps (current[?]->pid) need a lock
        current_running[1 - core_id]->to_kill = true;
    else
        if (!leave_task(pid))// record whther the pid exists
            pid_ret = 0;

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    return pid_ret;
}

int do_waitpid(pid_t pid)
{
    // find if the pid exists
    int pcb_idx;

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
        // pcb is valid, has the same pid, is main process
        if (pcb[pcb_idx].status != TASK_ZOMBIE && pid == pcb[pcb_idx].pid && pcb[pcb_idx].tid == 0)
            break;

    if (pcb_idx == NUM_MAX_TASK) // no pcb matches the pid
    {
        spin_lock_release(&pcb_lk); /* leave critical section */
        return 0;
    }

    // put the current task into waiting queue of this process, and change to other task
    int core_id = get_current_cpu_id();

    // store current_running before scheduling
    pcb_t *prev_running = current_running[core_id];
    prev_running->status = TASK_WAITING;

    // add prev_running to the ready queue
    if (prev_running != &(pid0_pcb[core_id]))
        list_add_tail(&(pcb[pcb_idx].wait_list), &(prev_running->list));

    // switch to the next task
    exec_next_task();

    // unlock_kernel();

    // TODO: [p2-task1] switch_to current_running
    switch_to(prev_running, current_running[core_id]);

    // spin_lock_release(&pcb_lk);
    /* leave critical section */

    // lock_kernel();

    return pid;
}

void do_process_show(void)
{
    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    int print_times = round_up_div(run_task_num, PS_LINES);

    for (int i = 0; i < print_times; ++i)
    {
        // wait for permission of printing the next page
        if (i != 0)
        {
            spin_lock_release(&pcb_lk); /* leave critical section */

            printk("Press enter to continue, \"q\" to quit...\n");
            int key;
            while ((key = port_read_ch()) != '\n' && key != '\r' && key != 'q' && key != 'Q')
                ;
            if (key == 'q' || key == 'Q')
                break;

            spin_lock_acquire(&pcb_lk); /* enter critical section */
        }
        screen_move_cursor(0, SHELL_BEGIN + SHELL_HEIGHT + 1);

        // print the next page
        
        // the number of tasks to print
        int print_max = min(i * PS_LINES + PS_LINES, run_task_num);
        int task_idx;

        // print the processes by task_idx
        for (task_idx = i * PS_LINES; task_idx < print_max; ++task_idx)
            // if the task exists (not zombie), then print it
            // if (run_tasks[task_idx] != NULL && run_tasks[task_idx]->status != TASK_ZOMBIE)
            printk("%s%c[%d] pid: %d, core: %d, taskset: 0x%x name: %s, status: %s\n",
                   blank_line, '\r', task_idx + 1, run_tasks[task_idx]->pid,
                   run_tasks[task_idx]->core_id, run_tasks[task_idx]->core_mask,
                   run_tasks[task_idx]->name,
                   task_stat_str[run_tasks[task_idx]->status]);

        for (task_idx = print_max; task_idx < i * PS_LINES + PS_LINES; ++task_idx)
            printk("%s%c", blank_line, '\n');
    }

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    printk("%s%c", blank_line, '\n');
}

pid_t do_getpid()
{
    int core_id = get_current_cpu_id();
    return current_running[core_id]->pid;
}

int do_taskset(int mask, pid_t pid, char *task_name, int argc, char **argv)
{
    if (mask < 1 || mask > 3)
        return -1; // invalid mask
    if (!pid && task_name == NULL)
        return -2; // invalid task name

    pid_t pid_get;

    if (!pid) // execute a new process by task_name
    {
        pid_get = do_exec(task_name, argc, argv);
        if(pid_get == 0)
            return -3; // invalid task name
    }
    else
        pid_get = pid;

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // find correspoding pcb by pid, and set up its core_mask
    for (int pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
    {
        if (pcb[pcb_idx].pid == pid_get && pcb[pcb_idx].status != TASK_ZOMBIE)
        {
            pcb_t *pcb_find = &pcb[pcb_idx];
            pcb_find->core_mask = mask;
        }
    }

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    return 0;
}