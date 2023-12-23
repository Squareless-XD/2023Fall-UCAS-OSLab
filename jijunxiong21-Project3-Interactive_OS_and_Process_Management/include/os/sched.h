/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *        Process scheduling related content, such as: scheduler, process blocking,
 *                 process wakeup, process creation, process kill, etc.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_SCHEDULER_H_
#define INCLUDE_SCHEDULER_H_

#include <type.h>
#include <os/list.h>
#include <os/task.h>
#include <os/procst.h>
#include <os/lock.h>
#include <os/time.h>
#include <os/mm.h>
#include <screen.h>
#include <printk.h>
#include <assert.h>
#include <os/smp.h>

#define NUM_MAX_TASK 32
#define LIST_NODE_OFF 16 // need to be modified as the size of list_node_t changes

#define SEC_CORE_PID0_PAGE_NUM 2

// registers as the sequence defined by RI5C-V
#define TRAP_ZERO 0
#define TRAP_RA 1
#define TRAP_SP 2
#define TRAP_GP 3
#define TRAP_TP 4
#define TRAP_T(num) ((num <= 2) ? (num + 5) : (num + 25)) // 5-7, 28-31
#define TRAP_S(num) ((num <= 1) ? (num + 8) : (num + 16)) // 8-9, 18-27
#define TRAP_A(num) (num + 10) // 10-17
#define TRAP_SSTATUS  32
#define TRAP_SEPC     33
#define TRAP_SBADADDR 34
#define TRAP_SCAUSE   35

// registers to be saved in switch_to()
#define SWITCH_RA 0
#define SWITCH_SP 1
#define SWITCH_S(num) (num + 2) // 2-13
#define CONTEXT_SIMU_BUFFER 0x200 // page

/* used to save register infomation */
typedef struct regs_context
{
    /* Saved main processor registers.*/
    reg_t regs[32]; // 32 general registers

    /* Saved special registers. */
    reg_t sstatus;  // Special status register
    reg_t sepc;     // Special exception program counter
    reg_t sbadaddr; // Special bad address
    reg_t scause;   // Special trap cause
} regs_context_t;

/* used to save register infomation in switch_to */
typedef struct switchto_context
{
    /* Callee saved registers.*/
    reg_t regs[14];
} switchto_context_t;

typedef enum {
    TASK_BLOCKED,   // blocked
    TASK_RUNNING,   // running
    TASK_READY,     // ready
    TASK_ZOMBIE,    // zombie
    TASK_SLEEPING,  // sleeping
    TASK_WAITING,   // waiting
    TASK_STAT_NUM
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!

    /* next pointer */
    reg_t kernel_sp;    // kernel stack pointer
    reg_t user_sp;      // user stack pointer

    list_node_t list;   // used to link pcb in ready_queue or sleep_queue
    // use PCB to store the context of the process
    switchto_context_t context;
    /*  OFFSET
        kernel_sp: 0
        user_sp: 8
        list: 16
        context: 32
    */

    /* previous pointer */
    ptr_t kernel_stack_base;
    ptr_t user_stack_base;

    list_head wait_list;

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status; // task status

    /* cursor position */
    int cursor_x;
    int cursor_y;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    char name[MAX_TASK_NAME_LEN]; // store the name of the process
    int task_id; // store the id of the process in tasks (in task_info_t)

    // store the mutex locks (or mailboxes) that the process has initialized
    uint64_t mlock_ocp; // 64 mutex locks at most
    uint64_t mbox_ocp; // 64 mail boxes at most

    tid_t tid; // thread id
    int child_t_num; // child thread number

    int core_id; // the core id that the process is running on (if running)
    int core_mask; // the core mask that the process can run on
    bool to_kill; // when a process is to be killed but running on another core
} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running[NR_CPUS];
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK]; // all PCBs
extern spin_lock_t pcb_lk;
extern pcb_t pid0_pcb[NR_CPUS]; // pid0 PCB

void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue, spin_lock_t *lock);
void do_unblock(list_node_t *);

/************************************************************/
/* TODO [P3-TASK1] exec exit kill waitpid ps*/
#ifdef S_CORE
extern pid_t do_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
extern pid_t do_exec(char *name, int argc, char *argv[]);
#endif
extern void do_exit(void);
extern int do_kill(pid_t pid);
extern int do_waitpid(pid_t pid);
extern void do_process_show();
extern pid_t do_getpid();
extern int do_taskset(int mask, pid_t pid, char *task_name, int argc, char **argv);
/************************************************************/

#endif
