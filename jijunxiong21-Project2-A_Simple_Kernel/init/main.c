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

// #include <os/sched.h>
#include <os/procst.h>
#include <os/io.h>
#include <os/task_tools.h>
#include <os/thread.h>

extern void ret_from_exception();



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
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first

    // get app info from memory
    info_off = *(unsigned short *)APP_INFO_OFF_ADDR;
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
    pcb_t *pcb)
{
    /* TODO: [p2-task3] initialization of registers on kernel stack
     * HINT: sp, ra, sepc, sstatus
     * NOTE: To run the task in user mode, you should set corresponding bits
     *     of sstatus(SPP, SPIE, etc.).
     */
    regs_context_t *pt_trap = (regs_context_t *)(kernel_stack);

    // simulate a trapframe
    asm volatile(
        "csrr %0, sstatus"
        : "=r"(pt_trap->sstatus)
    ); // read sstatus register using assembly
    // pt_trap->sstatus = SR_SPIE; // set SPIE to 1
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
    pt_switchto->regs[SWITCH_SP] = kernel_stack;
}

void init_pcb(int task_idx)
{
    /* TODO: [p2-task1] load needed tasks and init their corresponding PCB */
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

    pcb_avail->kernel_sp = allocKernelPage(ALLOC_KERNEL_PAGE_NUM) - sizeof(regs_context_t) + 1 * PAGE_SIZE; // alloc kernel stack
    pcb_avail->user_sp = allocUserPage(ALLOC_USER_PAGE_NUM) + 1 * PAGE_SIZE; // alloc user stack
    // printk("kernel_sp: %x, user_sp: %x\n", pcb_avail->kernel_sp + sizeof(regs_context_t), pcb_avail->user_sp);
    list_add_tail(&ready_queue, &(pcb_avail->list)); // add to ready queue
    pcb_avail->pid = process_id++; // assign pid, and increase process_id automatically
    pcb_avail->status = TASK_READY; // set status to ready
    pcb_avail->cursor_x = 0; // set cursor to (0, 0)
    pcb_avail->cursor_y = 0;
    strncpy(pcb_avail->name, tasks[task_idx].task_name, MAX_TASK_NAME_LEN - 1); // copy task name
    pcb_avail->name[MAX_TASK_NAME_LEN - 1] = '\0';
    pcb_avail->tid = 0; // set tid to 0
    pcb_avail->child_t_num = 0; // set child thread number to 0

    init_pcb_stack(pcb_avail->kernel_sp, pcb_avail->user_sp, tasks[task_idx].entry, &pcb[pcb_idx]);
}

static void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    syscall[SYSCALL_SLEEP]         = (long (*)())do_sleep;
    syscall[SYSCALL_YIELD]         = (long (*)())do_scheduler;
    syscall[SYSCALL_WRITE]         = (long (*)())screen_write;
    syscall[SYSCALL_CURSOR]        = (long (*)())screen_move_cursor;
    syscall[SYSCALL_REFLUSH]       = (long (*)())screen_reflush;
    syscall[SYSCALL_GET_TIMEBASE]  = (long (*)())get_time_base;
    syscall[SYSCALL_GET_TICK]      = (long (*)())get_ticks;
    syscall[SYSCALL_LOCK_INIT]     = (long (*)())do_mutex_lock_init;
    syscall[SYSCALL_LOCK_ACQ]      = (long (*)())do_mutex_lock_acquire;
    syscall[SYSCALL_LOCK_RELEASE]  = (long (*)())do_mutex_lock_release;
    syscall[SYSCALL_THREAD_CREATE] = (long (*)())thread_create;
    syscall[SYSCALL_THREAD_YIELD]  = (long (*)())thread_yield;
}
/************************************************************/


int main(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    allocKernelPage(1);
    for (int i = 0; i < NUM_MAX_TASK; ++i)
        pcb[i].status = TASK_EXITED;
    current_running = &pid0_pcb;
    current_running->status = TASK_RUNNING;
    strcpy(current_running->name, "idle");
    current_running->tid = 0;
    current_running->child_t_num = 0;
    asm volatile (
        "mv tp, %0\n\t"
        :
        :"r"(current_running)
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

    // Init screen (QAQ)
    init_screen();
    printk("> [INIT] SCREEN initialization succeeded.\n");


    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    void (*entry_point)(void);


    while (true)
    {
        int input_len;
        int task_idx;

        input_len = getline(buf, INPUT_BUF_SIZE);
        if (input_len <= 0)
            continue;

        if (strcmp(buf, "OS, start!") == 0)
            break;

        task_idx = task_cmp(buf, input_len+ 1);
        if (task_idx == -1) // task not found
            continue;

        // check whether the task has already been ordered to execute
        if (task_exist_check(run_task_idx, run_task_num, task_idx))
        {
            printk("Task already been put into ready queue!\n");
            continue;
        }
        
        entry_point = (void (*)())load_task_img(task_idx); // load the task
        // entry_point(); // execute the task
        init_pcb(task_idx);
        run_task_idx[run_task_num++] = task_idx; // record the task index into ready queue
    }

    // init screen again
    init_screen();

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
        /* set the first time interrupt */

    // reset the timer by the current time added by the interval
    bios_set_timer(get_ticks() + TIMER_INTERVAL);
 
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
loadbootd
print1
print2
lock1
lock2
sleep
timer
thread_test
dinosaur
OS, start!
*/
