#include <screen.h>
// #include <os/sched.h> // ignored
#include <os/task_tools.h>
#include <math.h>

static const char *task_stat_str[TASK_STAT_NUM] = {
    "TASK_BLOCKED ", // blocked
    "TASK_RUNNING ", // running
    "TASK_READY   ", // ready
    "TASK_ZOMBIE  ", // zombie
    "TASK_SLEEPING", // sleeping
    "TASK_WAITING ", // waiting
    "TASK_EXITED ", // exited
};

static const char blank_line[SCREEN_WIDTH + 1] = {
    [0 ... SCREEN_WIDTH - 1] = ' ',
    [SCREEN_WIDTH] = '\0'
};


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
    proc_schedule(&pcb[pcb_idx].wait_list, TASK_WAITING, NULL);

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
            printk("%s%c[%d] pid: %d, core: %d, taskset: 0x%x status: %s, name: %s\n",
                   blank_line, '\r', task_idx + 1, run_tasks[task_idx]->pid,
                   run_tasks[task_idx]->core_id, run_tasks[task_idx]->core_mask,
                   task_stat_str[run_tasks[task_idx]->status],
                   run_tasks[task_idx]->name);

        for (task_idx = print_max; task_idx < i * PS_LINES + PS_LINES; ++task_idx)
            printk("%s%c", blank_line, '\n');
    }

    spin_lock_release(&pcb_lk);
    /* leave critical section */

    printk("%s%c", blank_line, '\n');
}

pid_t do_getpid()
{
    return current_running[get_current_cpu_id()]->pid;
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