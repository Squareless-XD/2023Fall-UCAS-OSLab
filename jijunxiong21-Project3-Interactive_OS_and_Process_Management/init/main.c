#include <common.h>
#include <asm.h>
#include <asm/unistd.h>
#include <os/loader.h>
#include <os/irq.h>
#include <os/list.h>
#include <os/sched.h>
#include <os/lock.h>
#include <os/kernel.h>
#include <os/task.h>
#include <os/string.h>
#include <os/mm.h>
#include <os/time.h>
#include <sys/syscall.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <type.h>
#include <csr.h>

#include <os/procst.h>
#include <os/io.h>
#include <os/task_tools.h>
#include <os/thread.h>
#include <os/tasks.h>
#include <os/smp.h>

static void init_jmptab(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())KERNEL_JMPTAB_BASE;
    // jmptab is a pointer to a pointer to a function. 
    // actually, it is an array of pointers to functions.

    jmptab[CONSOLE_PUTSTR]  = (volatile long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (volatile long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (volatile long (*)())port_read_ch;
    jmptab[SD_READ]         = (volatile long (*)())sd_read;
    jmptab[SD_WRITE]        = (volatile long (*)())sd_write;
    jmptab[QEMU_LOGGING]    = (volatile long (*)())qemu_logging;
    jmptab[SET_TIMER]       = (volatile long (*)())set_timer;
    jmptab[READ_FDT]        = (volatile long (*)())read_fdt;
    jmptab[MOVE_CURSOR]     = (volatile long (*)())screen_move_cursor;
    jmptab[PRINT]           = (volatile long (*)())printk;
    jmptab[YIELD]           = (volatile long (*)())do_scheduler;
    jmptab[MUTEX_INIT]      = (volatile long (*)())do_mutex_lock_init;
    jmptab[MUTEX_ACQ]       = (volatile long (*)())do_mutex_lock_acquire;
    jmptab[MUTEX_RELEASE]   = (volatile long (*)())do_mutex_lock_release;
    jmptab[WRITE]           = (volatile long (*)())screen_write;
    jmptab[CLEAR]           = (volatile long (*)())screen_clear;
    jmptab[REFLUSH]         = (volatile long (*)())screen_reflush;
    jmptab[EXEC]            = (volatile long (*)())do_exec;
    jmptab[EXIT]            = (volatile long (*)())do_exit;
    jmptab[KILL]            = (volatile long (*)())do_kill;
    jmptab[WAITPID]         = (volatile long (*)())do_waitpid;
    jmptab[PS]              = (volatile long (*)())do_process_show;
    jmptab[GETPID]          = (volatile long (*)())do_getpid;
    jmptab[BARRIER_INIT]    = (volatile long (*)())NULL;
    jmptab[BARRIER_WAIT]    = (volatile long (*)())NULL;
    jmptab[BARRIER_DESTROY] = (volatile long (*)())NULL;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
    unsigned short info_off;

    // get app info from memory
    info_off = *(int *)APP_INFO_OFF_ADDR;
    int info_size = sizeof(task_info_t) * TASK_MAXNUM + sizeof(int); // tasks & task_num

    // NOTE: task_info_t is 4-byte aligned?
    int info_sec_begin = info_off / SECTOR_SIZE;
    int info_sec_end = NBYTES2SEC(info_off + info_size);
    int info_sec_num = info_sec_end - info_sec_begin;
    bios_sd_read(INITIAL_BUF, info_sec_num, info_sec_begin);

    // load task_num
    int *task_num_addr = (int *)(INITIAL_BUF + (uint64_t)info_off % SECTOR_SIZE);
    task_num = *task_num_addr;

    // get app info from sd card
    /* one bug here: too much trust for "plus" */
    task_info_t *info_addr = (task_info_t *)((void *)task_num_addr + sizeof(int));
    for (int task_idx = 0; task_idx < TASK_MAXNUM; ++task_idx)
    {
        tasks[task_idx] = info_addr[task_idx];
    }
}

/************************************************************/
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, int argc, char **argv)
{
    // simulate a trapframe
    regs_context_t *pt_trap = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_trap->sstatus = SR_SPIE; // set SPIE to 1
    pt_trap->sepc = entry_point;
    pt_trap->regs[TRAP_TP] = (reg_t)pcb;
    pt_trap->regs[TRAP_A(0)] = argc;
    pt_trap->regs[TRAP_A(1)] = (reg_t)argv;
    pt_trap->regs[TRAP_SP] = user_stack;

    // simulate a switch context
    switchto_context_t *pt_switchto = &(pcb->context);
    pt_switchto->regs[SWITCH_RA] = (reg_t)ret_from_exception;
    pt_switchto->regs[SWITCH_SP] = (kernel_stack - sizeof(regs_context_t) - CONTEXT_SIMU_BUFFER) & (reg_t)-16;
}

void init_pcb_stack_main(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t *pt_trap = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

    // simulate a trapframe
    pt_trap->sstatus = SR_SPIE; // set SPIE to 1
    pt_trap->sepc = entry_point;
    pt_trap->regs[TRAP_SP] = user_stack;
    pt_trap->regs[TRAP_TP] = (reg_t)pcb;

    /* TODO: [p2-task1] set sp to simulate just returning from switch_to
     * NOTE: you should prepare a stack, and push some values to
     * simulate a callee-saved context.
     */
    switchto_context_t *pt_switchto = &(pcb->context);

    // simulate a switch context
    pt_switchto->regs[SWITCH_RA] = (reg_t)ret_from_exception;
    pt_switchto->regs[SWITCH_SP] = (kernel_stack - sizeof(regs_context_t) - CONTEXT_SIMU_BUFFER) & (reg_t)-16;
}

pid_t init_pcb_args(int task_idx, int argc, char **argv)
{
    pcb_t *pcb_assigned;
    void (*entry_point)(void);

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // task can be executed.
    entry_point = (void (*)())load_task_img(task_idx); // load the task
    pcb_assigned = init_pcb(task_idx);
    run_tasks[run_task_num++] = pcb_assigned; // record the task index into ready queue
    if (run_task_num > NUM_MAX_TASK)
    {
        printk("No more tasks!\n");
        assert(1);
    }

    // set up argv pointer
    char **st_argv;
    reg_t user_sp;
    user_sp = pcb_assigned->user_sp - (argc + 1) * sizeof(char *);
    st_argv = (char **)user_sp;

    // use a for loop to copy the arguments, change sp, setup argv in stack
    *(st_argv + argc) = NULL;
    for (int i = 0; i < argc; ++i)
    {
        user_sp -= strlen(argv[i]) + 1;
        strncpy((void *)user_sp, argv[i], strlen(argv[i]) + 1);
        st_argv[i] = (char *)user_sp;
    }

    // get an aligned sp
    user_sp &= (reg_t)-16;

    // reveal user stack
    pcb_assigned->user_sp = user_sp;

    init_pcb_stack(
        pcb_assigned->kernel_sp, user_sp, (ptr_t)entry_point,
        pcb_assigned, argc, st_argv);

    spin_lock_release(&pcb_lk);
    /* leave  critical section */

    return pcb_assigned->pid;
}

pcb_t *init_pcb(int task_idx)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
    int pcb_idx;
    pcb_t *pcb_avail;

    for (pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx) // alloc all tasks with corresponding PCB
        if (pcb[pcb_idx].status == TASK_ZOMBIE)
            break;
    if (pcb_idx == NUM_MAX_TASK)
    {
        printk("No more PCB!\n");
        assert(1);
    }

    pcb_avail = &pcb[pcb_idx];

    pcb_avail->kernel_stack_base = allocKernelPage(ALLOC_KERNEL_PAGE_NUM) + ALLOC_KERNEL_PAGE_NUM * PAGE_SIZE; // alloc kernel stack
    pcb_avail->kernel_sp = pcb_avail->kernel_stack_base; // alloc kernel stack
    pcb_avail->user_stack_base = allocUserPage(ALLOC_USER_PAGE_NUM) + ALLOC_USER_PAGE_NUM * PAGE_SIZE; // alloc user stack
    pcb_avail->user_sp = pcb_avail->user_stack_base; // alloc user stack
    // printk("kernel_sp: %x, user_sp: %x\n", pcb_avail->kernel_sp + sizeof(regs_context_t), pcb_avail->user_sp);

    list_add_tail(&ready_queue, &(pcb_avail->list)); // add to ready queue
    pcb_avail->wait_list.prev = pcb_avail->wait_list.next = &(pcb_avail->wait_list); // init wait list

    pcb_avail->pid = process_id++; // assign pid, and increase process_id automatically
    pcb_avail->status = TASK_READY; // set status to ready
    pcb_avail->cursor_x = 0; // set cursor to (0, 0)
    pcb_avail->cursor_y = 0;
    strncpy(pcb_avail->name, tasks[task_idx].task_name, MAX_TASK_NAME_LEN - 1); // copy task name
    pcb_avail->name[MAX_TASK_NAME_LEN - 1] = '\0';
    pcb_avail->task_id = task_idx; // set task id

    pcb_avail->tid = 0; // set tid to 0
    pcb_avail->child_t_num = 0; // set child thread number to 0
    pcb_avail->core_id = NR_CPUS;
    int core_id = get_current_cpu_id();
    pcb_avail->core_mask = current_running[core_id]->core_mask; // the core mask that the process can run on
    pcb_avail->to_kill = false; // when a process is to be killed but running on another core

    return pcb_avail;
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    // for (int i = 0; i < NUM_SYSCALLS; ++i)
    //     syscall[i] = (long (*)())NULL;
    syscall[SYSCALL_EXEC]           = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]           = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP]          = (long (*)())do_sleep; // old syscall
    syscall[SYSCALL_KILL]           = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]        = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]             = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]         = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]          = (long (*)())do_scheduler; // old syscall
    syscall[SYSCALL_THREAD_CREATE]  = (long (*)())thread_create; // old syscall, personnaly defined
    syscall[SYSCALL_THREAD_YIELD]   = (long (*)())thread_yield; // old syscall, personnaly defined
    syscall[SYSCALL_MOVEUP]         = (long (*)())shell_moveup; // personnaly defined
    syscall[SYSCALL_GET_CPUID]      = (long (*)())get_current_cpu_id; // personnaly defined
    syscall[SYSCALL_TASKSET]        = (long (*)())do_taskset; // personnaly defined
    syscall[SYSCALL_WRITE]          = (long (*)())screen_write; // old syscall
    syscall[SYSCALL_READCH]         = (long (*)())bios_getchar;
    syscall[SYSCALL_CURSOR]         = (long (*)())screen_move_cursor; // old syscall
    syscall[SYSCALL_REFLUSH]        = (long (*)())screen_reflush; // old syscall
    syscall[SYSCALL_CLEAR]          = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE]   = (long (*)())get_time_base; // old syscall
    syscall[SYSCALL_GET_TICK]       = (long (*)())get_ticks; // old syscall
    syscall[SYSCALL_SEMA_INIT]      = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]        = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]      = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]   = (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_LOCK_INIT]      = (long (*)())do_mutex_lock_init; // old syscall
    syscall[SYSCALL_LOCK_ACQ]       = (long (*)())do_mutex_lock_acquire; // old syscall
    syscall[SYSCALL_LOCK_RELEASE]   = (long (*)())do_mutex_lock_release; // old syscall
    syscall[SYSCALL_SHOW_TASK]      = (long (*)())NULL;
    syscall[SYSCALL_BARR_INIT]      = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]      = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]   = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]      = (long (*)())NULL;
    syscall[SYSCALL_COND_WAIT]      = (long (*)())NULL;
    syscall[SYSCALL_COND_SIGNAL]    = (long (*)())NULL;
    syscall[SYSCALL_COND_BROADCAST] = (long (*)())NULL;
    syscall[SYSCALL_COND_DESTROY]   = (long (*)())NULL;
    syscall[SYSCALL_MBOX_OPEN]      = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]     = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]      = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]      = (long (*)())do_mbox_recv;
}
/************************************************************/


int main(void)
{
    int core_id = get_current_cpu_id();
    if (core_id == 0)
    {
        // Init jump table provided by kernel and bios(ΦωΦ)
        init_jmptab();

        // Init task information (〃'▽'〃)
        init_task_info();

        /* TODO: [p2-task1] remember to initialize 'current_running' */
        allocKernelPage(SEC_CORE_PID0_PAGE_NUM); //
        for (int i = 0; i < NUM_MAX_TASK; ++i)
            pcb[i].status = TASK_ZOMBIE;

        current_running[core_id] = &pid0_pcb[core_id];
        strncpy(current_running[core_id]->name, "idle", MAX_TASK_NAME_LEN - 1);
        asm volatile (
            "mv tp, %0\n\t"
            :
            :"r"(current_running[core_id])
        ); // move current_running to $tp

        // // Init Process Control Blocks |•'-'•) ✧
        // init_pcb();
        // printk("> [INIT] PCB initialization succeeded.\n");

        // Read CPU frequency (｡•ᴗ-)_
        time_base = bios_read_fdt(TIMEBASE);

        // Init lock mechanism o(´^｀)o
        init_locks();
        printk("> [INIT] Lock mechanism initialization succeeded.\n");

        // Init interrupt (^_^)
        init_exception();
        printk("> [INIT] Interrupt processing initialization succeeded.\n");

        // Init system call table (0_0)
        init_syscall();
        printk("> [INIT] System call initialized successfully.\n");

        // init barriers, semaphores, mailboxes [p3-task2]
        init_barriers();
        init_semaphores();
        init_mbox();

        // Init screen (QAQ)
        init_screen();
        printk("> [INIT] SCREEN initialization succeeded.\n");


        // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
        //   and then execute them.
        int task_idx;
        void (*entry_point)(void);

        // launch shell
        task_idx = task_cmp("shell", strlen("shell") + 1);
        if (task_idx == -1) // task not found
            assert(0);
        entry_point = (void (*)())load_task_img(task_idx); // load the task
        
        pcb_t *pcb_shell = init_pcb(task_idx);
        init_pcb_stack_main(pcb_shell->kernel_sp, pcb_shell->user_sp,
                            (ptr_t)entry_point, pcb_shell);

        run_tasks[run_task_num++] = pcb_shell; // record the task

        screen_move_cursor(0, 20);
        printk("core id: %d\n", core_id);

        // reset the timer by the current time added by the interval
        bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);
        wakeup_other_hart();
    }
    else if (core_id == 1) // hart 1
    {
        // initialize the symmetric multi-processor
        smp_init();

        // reset the timer by the current time added by the interval
        bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);
        screen_move_cursor(0, 21);
        printk("core id: %d\n", core_id);
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
        /* set the first time interrupt */

    // // reset the timer by the current time added by the interval
    // bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}
/* test case
./launch.sh
loadbootd
exec multicore&
*/

/* test case
./multicore.sh
loadbootm
clear && exec multicore

clear && exec mbox_server &
exec mbox_client &
exec mbox_client &
exec mbox_client &

clear && exec barrier &
taskset 0x1 test_barrier
taskset 0x2 test_barrier

clear && exec genshin_impact start!&

clear && exec semaphore &

exec waitpid&
*/
