#include <os/irq.h>
#include <os/syscall.h>
#include <os/task_tools.h>
#include <os/mm.h>
#include <screen.h>

#include <os/thread.h>
#include <os/loader.h>
#include <os/kernel.h>
#include <os/time.h>

#include <asm/unistd.h>
#include <csr.h>
#include <e1000.h>
#include <plic.h>
#include <os/ioremap.h>
#include <os/net.h>

bool master_core_arrive = false;
bool sec_core_arrive = false;

// this function is used to unmap the boot table, will only be called once
static inline void unmap_boot_table()
{
    // clear middle-level page directory
    for (uint64_t va = 0x50000000lu; va < 0x51000000lu; va += 0x200000lu)
    {
        va &= VA_MASK;
        uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
        uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
        PTE *pmd = (PTE *)get_kva(kern_pgdir[vpn2]);
        pmd[vpn1] = 0;
    }
    // clear upper-level page directory
    uint64_t va = 0x50000000lu & VA_MASK;
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    kern_pgdir[vpn2] = 0;
}

static inline void init_jmptab(void)
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

static inline void init_task_info(void)
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
    bios_sd_read(kva2pa(INITIAL_BUF), info_sec_num, info_sec_begin);
    __sync_synchronize();
    local_flush_icache_all();
    
    // load task_num
    int *task_num_addr = (int *)(INITIAL_BUF + (uint64_t)info_off % SECTOR_SIZE);
    task_num = *task_num_addr;

    // check number of tasks
    assert(task_num < TASK_MAXNUM)

    // get app info from sd card
    /* one bug here: too much trust for "plus" */
    task_info_t *info_addr = (task_info_t *)((void *)task_num_addr + sizeof(int));
    for (int task_idx = 0; task_idx < TASK_MAXNUM; ++task_idx)
    {
        tasks[task_idx] = info_addr[task_idx];
    }

    // get the base of swap partition in sd card
    sd_swap_base = ROUND(tasks[task_num - 1].phyaddr + tasks[task_num - 1].filesz, PAGE_SIZE);
}

// NUM_MAX_TASK * PAGE_SIZE == 0x20*0x1000 == 0x20000
// NUM_MAX_TASK * sizeof(spin_lock_t) == 0x20 * 0x4 == 0x80
// NUM_MAX_TASK * MAX_MEM_SECTION * sizeof(user_page_info_t) == 0x20 * 0x40 * 0x20 == 0x10000
// in total: 0x30080
ptr_t user_pud = USER_PUD_ADDR;
spin_lock_t *user_pud_lk_base = (spin_lock_t *)(USER_PUD_ADDR + NUM_MAX_TASK * PAGE_SIZE);
user_page_info_t *user_mem_info_base = (user_page_info_t *)(USER_PUD_ADDR + NUM_MAX_TASK * PAGE_SIZE + NUM_MAX_TASK * sizeof(spin_lock_t));

static inline void init_tasks_pud(void)
{
    for (int pcb_idx = 0; pcb_idx < NUM_MAX_TASK; ++pcb_idx)
    {
        pcb[pcb_idx].pud = (PTE *)(user_pud + pcb_idx * PAGE_SIZE);

        pcb[pcb_idx].pud_lk = user_pud_lk_base + pcb_idx;
        spin_lock_init(pcb[pcb_idx].pud_lk);

        pcb[pcb_idx].mem_info = user_mem_info_base + pcb_idx * MAX_MEM_SECTION;
    }
    // [MAX_MEM_SECTION]
}

/************************************************************/
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb, reg_t arg0, reg_t arg1, reg_t ra)
{
    // simulate a trapframe
    regs_context_t *pt_trap = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));
    pt_trap->sstatus = SR_SUM | SR_SPIE; // set SPIE to 1
    pt_trap->sepc = entry_point;
    pt_trap->regs[TRAP_SP] = user_stack;
    pt_trap->regs[TRAP_TP] = (reg_t)pcb;
    pt_trap->regs[TRAP_RA] = ra;
    pt_trap->regs[TRAP_A(0)] = arg0;
    pt_trap->regs[TRAP_A(1)] = arg1;

    // simulate a switch context
    switchto_context_t *pt_switchto = &(pcb->context);
    pt_switchto->regs[SWITCH_RA] = (reg_t)ret_from_exception;
    pt_switchto->regs[SWITCH_SP] = (kernel_stack - sizeof(regs_context_t) - CONTEXT_SIMU_BUFFER) & (reg_t)-16;
}

// void init_pcb_stack_main(
//     ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
//     pcb_t *pcb)
// {
//     /* TODO: [p2-task3] initialization of registers on kernel stack
//      * HINT: sp, ra, sepc, sstatus
//      * NOTE: To run the task in user mode, you should set corresponding bits
//      *     of sstatus(SPP, SPIE, etc.).
//      */
//     regs_context_t *pt_trap = (regs_context_t *)(kernel_stack - sizeof(regs_context_t));

//     // simulate a trapframe
//     pt_trap->sstatus = SR_SUM | SR_SPIE; // set SPIE to 1
//     pt_trap->sepc = entry_point;
//     pt_trap->regs[TRAP_SP] = user_stack;
//     pt_trap->regs[TRAP_TP] = (reg_t)pcb;

//     /* TODO: [p2-task1] set sp to simulate just returning from switch_to
//      * NOTE: you should prepare a stack, and push some values to
//      * simulate a callee-saved context.
//      */
//     switchto_context_t *pt_switchto = &(pcb->context);

//     // simulate a switch context
//     pt_switchto->regs[SWITCH_RA] = (reg_t)ret_from_exception;
//     pt_switchto->regs[SWITCH_SP] = (kernel_stack - sizeof(regs_context_t) - CONTEXT_SIMU_BUFFER) & (reg_t)-16;
// }

pid_t init_pcb_args(int task_idx, int argc, char **argv)
{
    pcb_t *pcb_assigned;
    void (*entry_point)(void);

    /* enter critical section */
    spin_lock_acquire(&pcb_lk);

    // task can be executed.
    pcb_assigned = init_pcb(task_idx);
    entry_point = (void (*)())load_task_img(task_idx, pcb_assigned); // load the task
    run_tasks[run_task_num++] = pcb_assigned; // record the task index into ready queue
    if (run_task_num > NUM_MAX_TASK)
    {
        printk("No more tasks!\n");
        assert(0)
    }

    // set up argv pointer
    char **st_argv;
    ptr_t user_sp = pcb_assigned->user_sp - (argc + 1) * sizeof(char *); // vaddr in the new pcb

    // get the "va in kernel PTE" of the argv pointer array's pa
    // now the page of user stack is not allocated, so we can alloc it and get the pa
    st_argv = (char **)user_sp;
    ptr_t usp_kern_va;
    // ptr_t usp_kern_va = alloc_page_helper(user_sp, 0, pcb_assigned->pud, UNPINNED);
    // use a for loop to copy the arguments, change sp, setup argv in stack
    ptr_t argv_kern_va = alloc_page_helper((ptr_t)(st_argv + argc), 0, pcb_assigned, UNPINNED);
    *(char **)argv_kern_va = NULL;
    for (int i = 0; i < argc; ++i)
    {
        int strlen_arg = strlen(argv[i]) + 1;
        int pg_left_sz;
        while (strlen_arg > (pg_left_sz = (user_sp - 1) % PAGE_SIZE + 1))
        {
            user_sp -= pg_left_sz;
            strlen_arg -= pg_left_sz;
            usp_kern_va = alloc_page_helper(user_sp, 0, pcb_assigned, UNPINNED);
            memcpy((void *)usp_kern_va, (void *)(argv[i]) + strlen_arg, pg_left_sz);
        }
        user_sp -= strlen_arg;
        usp_kern_va = alloc_page_helper(user_sp, 0, pcb_assigned, UNPINNED);
        memcpy((void *)usp_kern_va, (void *)(argv[i]), strlen_arg);
        argv_kern_va = alloc_page_helper((ptr_t)(st_argv + i), 0, pcb_assigned, UNPINNED);
        *(char **)argv_kern_va = (char *)user_sp;
    }

    // get an aligned sp
    user_sp &= (reg_t)-16;

    // reveal user stack
    pcb_assigned->user_sp = user_sp;

    init_pcb_stack(
        pcb_assigned->kernel_sp, user_sp, (ptr_t)entry_point,
        pcb_assigned, (reg_t)argc, (reg_t)st_argv, 0ul);

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
        assert(0)
    }

    WRITE_LOG("newly assigned pcb_idx: %d\n", pcb_idx)

    pcb_avail = &pcb[pcb_idx];

    pcb_avail->pud = (PTE *)(user_pud + pcb_idx * PAGE_SIZE);
    pcb_avail->pud_lk = user_pud_lk_base + pcb_idx;

    /* enter critical section */
    spin_lock_acquire(pcb_avail->pud_lk);

    share_pgtable(pcb_avail->pud, kern_pgdir); // copy kernel PTE into process
    pcb_avail->mem_num = 0; // have no memory section now
    pcb_avail->mem_info = user_mem_info_base + pcb_idx * MAX_MEM_SECTION;
    for (int i = 0; i < MAX_MEM_SECTION; i++) // all sections are not used
        pcb_avail->mem_info[i].valid = false;

    pcb_avail->kernel_stack_base = alloc_page_proc(pcb_avail->mem_info, KERNEL_STACK_PAGE, 0, 0) + PAGE_SIZE * KERNEL_STACK_PAGE; // alloc kernel stack

    spin_lock_release(pcb_avail->pud_lk);
    /* leave critical section */

    pcb_avail->kernel_sp = pcb_avail->kernel_stack_base; // alloc kernel stack
    pcb_avail->user_stack_base = USER_STACK_ADDR; // setup user stack
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
    pcb_avail->core_mask = my_cpu()->core_mask; // the core mask that the process can run on
    pcb_avail->to_kill = false; // when a process is to be killed but running on another core

    WRITE_LOG("kernel stack base: %lx, user stack base: %lx, pid: %d, name: %s\n", pcb_avail->kernel_stack_base, pcb_avail->user_sp, pcb_avail->pid, pcb_avail->name)

    return pcb_avail;
}

static inline void init_syscall(void)
{
    // TODO: [p2-task3] initialize system call table.
    // for (int i = 0; i < NUM_SYSCALLS; ++i)
    //     syscall[i] = (long (*)())NULL;
    syscall[SYSCALL_EXEC]            = (long (*)())do_exec;
    syscall[SYSCALL_EXIT]            = (long (*)())do_exit;
    syscall[SYSCALL_SLEEP]           = (long (*)())do_sleep; // old syscall
    syscall[SYSCALL_KILL]            = (long (*)())do_kill;
    syscall[SYSCALL_WAITPID]         = (long (*)())do_waitpid;
    syscall[SYSCALL_PS]              = (long (*)())do_process_show;
    syscall[SYSCALL_GETPID]          = (long (*)())do_getpid;
    syscall[SYSCALL_YIELD]           = (long (*)())do_scheduler; // old syscall
    syscall[SYSCALL_FORK]            = (long (*)())do_fork;
    syscall[SYSCALL_THREAD_CREATE]   = (long (*)())do_thread_create; // old syscall, personnaly defined
    syscall[SYSCALL_THREAD_YIELD]    = (long (*)())do_thread_yield; // old syscall, personnaly defined
    syscall[SYSCALL_THREAD_JOIN]     = (long (*)())do_thread_join; // old syscall, personnaly defined
    syscall[SYSCALL_THREAD_EXIT]     = (long (*)())do_thread_exit; // old syscall, personnaly defined
    syscall[SYSCALL_MOVEUP]          = (long (*)())shell_moveup; // personnaly defined
    syscall[SYSCALL_GET_CPUID]       = (long (*)())get_current_cpu_id; // personnaly defined
    syscall[SYSCALL_TASKSET]         = (long (*)())do_taskset; // personnaly defined
    syscall[SYSCALL_WRITE]           = (long (*)())screen_write; // old syscall
    syscall[SYSCALL_READCH]          = (long (*)())bios_getchar;
    syscall[SYSCALL_CURSOR]          = (long (*)())screen_move_cursor; // old syscall
    syscall[SYSCALL_REFLUSH]         = (long (*)())screen_reflush; // old syscall
    syscall[SYSCALL_CLEAR]           = (long (*)())screen_clear;
    syscall[SYSCALL_GET_TIMEBASE]    = (long (*)())get_time_base; // old syscall
    syscall[SYSCALL_GET_TICK]        = (long (*)())get_ticks; // old syscall
    syscall[SYSCALL_SEMA_INIT]       = (long (*)())do_semaphore_init;
    syscall[SYSCALL_SEMA_UP]         = (long (*)())do_semaphore_up;
    syscall[SYSCALL_SEMA_DOWN]       = (long (*)())do_semaphore_down;
    syscall[SYSCALL_SEMA_DESTROY]    = (long (*)())do_semaphore_destroy;
    syscall[SYSCALL_LOCK_INIT]       = (long (*)())do_mutex_lock_init; // old syscall
    syscall[SYSCALL_LOCK_ACQ]        = (long (*)())do_mutex_lock_acquire; // old syscall
    syscall[SYSCALL_LOCK_RELEASE]    = (long (*)())do_mutex_lock_release; // old syscall
    syscall[SYSCALL_SHOW_TASK]       = (long (*)())NULL;
    syscall[SYSCALL_BARR_INIT]       = (long (*)())do_barrier_init;
    syscall[SYSCALL_BARR_WAIT]       = (long (*)())do_barrier_wait;
    syscall[SYSCALL_BARR_DESTROY]    = (long (*)())do_barrier_destroy;
    syscall[SYSCALL_COND_INIT]       = (long (*)())NULL;
    syscall[SYSCALL_COND_WAIT]       = (long (*)())NULL;
    syscall[SYSCALL_COND_SIGNAL]     = (long (*)())NULL;
    syscall[SYSCALL_COND_BROADCAST]  = (long (*)())NULL;
    syscall[SYSCALL_COND_DESTROY]    = (long (*)())NULL;
    syscall[SYSCALL_MBOX_OPEN]       = (long (*)())do_mbox_open;
    syscall[SYSCALL_MBOX_CLOSE]      = (long (*)())do_mbox_close;
    syscall[SYSCALL_MBOX_SEND]       = (long (*)())do_mbox_send;
    syscall[SYSCALL_MBOX_RECV]       = (long (*)())do_mbox_recv;
    syscall[SYSCALL_SHM_GET]         = (long (*)())shm_page_get; // not finished
    syscall[SYSCALL_SHM_DT]          = (long (*)())shm_page_dt; // not finished
    syscall[SYSCALL_NET_SEND]        = (long (*)())do_net_send;
    syscall[SYSCALL_NET_RECV]        = (long (*)())do_net_recv;
    syscall[SYSCALL_NET_RECV_STREAM] = (long (*)())do_net_recv_stream;

}
/************************************************************/

void init_first_core(void)
{
    // Init jump table provided by kernel and bios(ΦωΦ)
    init_jmptab();

    // Init task information (〃'▽'〃)
    init_task_info();
    
    // Init allocation functions
    init_alloc(); // need a global variable in init_task_info

    // Init page upper directories for tasks (｡•̀ᴗ-)✧
    init_tasks_pud();

    /* TODO: [p2-task1] remember to initialize 'current_running' */
    for (int i = 0; i < NUM_MAX_TASK; ++i)
        pcb[i].status = TASK_ZOMBIE;

    current_running[0] = &pid0_pcb[0];
    strncpy(current_running[0]->name, "idle", MAX_TASK_NAME_LEN - 1);
    asm volatile (
        "mv tp, %0\n\t"
        :
        :"r"(current_running[0])
    ); // move current_running to $tp

    // Read Flatten Device Tree (｡•ᴗ-)_
    time_base = bios_read_fdt(TIMEBASE);
    e1000 = (volatile uint8_t *)bios_read_fdt(ETHERNET_ADDR);
    uint64_t plic_addr = bios_read_fdt(PLIC_ADDR);
    uint32_t nr_irqs = (uint32_t)bios_read_fdt(NR_IRQS);
    printk("> [INIT] e1000: %lx, plic_addr: %lx, nr_irqs: %lx.\n", e1000, plic_addr, nr_irqs);

    // IOremap
    plic_addr = (uintptr_t)ioremap((uint64_t)plic_addr, 0x4000 * NORMAL_PAGE_SIZE);
    e1000 = (uint8_t *)ioremap((uint64_t)e1000, 8 * NORMAL_PAGE_SIZE);
    printk("> [INIT] IOremap initialization succeeded.\n");

    // Init lock mechanism o(´^｀)o
    init_locks();
    printk("> [INIT] Lock mechanism initialization succeeded.\n");

    // Init interrupt (^_^)
    init_exception();
    printk("> [INIT] Interrupt processing initialization succeeded.\n");

    // TODO: [p5-task3] Init plic
    plic_init(plic_addr, nr_irqs);
    printk("> [INIT] PLIC initialized successfully. addr = 0x%lx, nr_irqs=0x%x\n", plic_addr, nr_irqs);

    // Init network device (after e1000)
    e1000_init();
    printk("> [INIT] E1000 device initialized successfully.\n");

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
}

int main(void)
{
    __sync_synchronize();
    local_flush_icache_all(); // flush the cache

    if (get_current_cpu_id() == 0)
    {
        init_first_core();
        WRITE_LOG("initialize cpu:0\n")

        // wait for core 1 needs to clear TBE's temporary mapping
        wakeup_other_hart(); // NOTE: commented for single core debugging
        // sec_core_arrive = true; // NOTE: for single core debugging
        while (sec_core_arrive == false)
            ;

        // Remove temporary mapping entries from boot loader or BIOS
        unmap_boot_table(); // PGDIR_PA

        __sync_synchronize();
        local_flush_icache_all();
        local_flush_tlb_all();

        WRITE_LOG("boot table unmapped\n")

        // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
        //   and then execute them.
        void (*entry_point)(void);

        // use lock to protect the PCB
        spin_lock_acquire(&pcb_lk);

        // launch shell
        int task_idx = task_cmp("shell", strlen("shell") + 1);
        if (task_idx == -1) // task not found
            assert(0)

        pcb_t *pcb_shell = init_pcb(task_idx);
        entry_point = (void (*)())load_task_img(task_idx, pcb_shell); // load the task
        init_pcb_stack(pcb_shell->kernel_sp, pcb_shell->user_sp,
                       (ptr_t)entry_point, pcb_shell, 0ul, 0ul, 0ul);
        WRITE_LOG("shell loaded\n")

        run_tasks[run_task_num++] = pcb_shell; // record the task

        screen_move_cursor(0, 20);

        // unlock the PCB
        spin_lock_release(&pcb_lk);

        printk("core id: %d\n", get_current_cpu_id());

        // reset the timer by the current time added by the interval
        bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);

        master_core_arrive = true;
    }
    else if (get_current_cpu_id() == 1) // hart 1
    {
        // initialize the symmetric multi-processor
        smp_init();

        // reset the timer by the current time added by the interval
        bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);

        screen_move_cursor(0, 21);

        printk("core id: %d\n", get_current_cpu_id());

        sec_core_arrive = true;
    }

    // TODO: [p2-task4] Setup timer interrupt and enable all interrupt globally
    // NOTE: The function of sstatus.sie is different from sie's
        /* set the first time interrupt */

    // // reset the timer by the current time added by the interval
    // bios_set_timer(get_ticks() + TIMER_INTERVAL_LARGE);

    while ((master_core_arrive && sec_core_arrive) == false)
        ;

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        // If you do preemptive scheduling, they're used to enable CSR_SIE and wfi
        // check_killed();
        // spin_lock_acquire(&pcb_lk);
        // check_sleeping();
        // spin_lock_release(&pcb_lk);
        enable_preempt();
        asm volatile("wfi");
    }

    return 0;
}

/* test case
./network.sh

loadbootm
clear && exec fly &

exec send

exec recv

*/
