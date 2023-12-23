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

#define NUM_MAX_TASK 32
#define LIST_NODE_OFF 16 // need to be modified as the size of list_node_t changes


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
// // compatible with switch_to()
// #define SW_COMP_RA 0
// #define SW_COMP_SP 1
// #define SW_COMP_S(num) (num + 2) // 2-13
// #define SW_COMP_ZERO 14
// #define SW_COMP_GP 15
// #define SW_COMP_TP 16
// #define SW_COMP_T(num) (num + 17) // 17-23
// #define SW_COMP_A(num) (num + 24) // 24-31

// registers to be saved in switch_to()
#define SWITCH_RA 0
#define SWITCH_SP 1
#define SWITCH_S(num) (num + 2) // 2-13

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
    TASK_EXITED,    // exited
    TASK_SLEEPING,  // sleeping
} task_status_t;

/* Process Control Block */
typedef struct pcb
{
    /* register context */
    // NOTE: this order must be preserved, which is defined in regs.h!!
    reg_t kernel_sp;    // kernel stack pointer
    reg_t user_sp;      // user stack pointer

    /* previous, next pointer */
    list_node_t list;   // used to link pcb in ready_queue or sleep_queue

    // use PCB to store the context of the process
    switchto_context_t context;

    /*  OFFSET
        kernel_sp: 0
        user_sp: 8
        list: 16
        context: 32
    */

    /* process id */
    pid_t pid;

    /* BLOCK | READY | RUNNING */
    task_status_t status; // task status

    /* cursor position */
    int cursor_x;
    int cursor_y;

    // used to store the time when the process is created   
    uint64_t created_time;

    /* time(seconds) to wake up sleeping PCB */
    uint64_t wakeup_time;

    // store the name of the process
    char name[MAX_TASK_NAME_LEN];

    // thread id
    tid_t tid;

    // child thread number
    int child_t_num;
} pcb_t;

/* ready queue to run */
extern list_head ready_queue;

/* sleep queue to be blocked in */
extern list_head sleep_queue;

/* current running task PCB */
extern pcb_t * volatile current_running;
extern pid_t process_id;

extern pcb_t pcb[NUM_MAX_TASK]; // all PCBs
extern pcb_t pid0_pcb;          // pid0 PCB
extern const ptr_t pid0_stack;  // pid0 stack


void switch_to(pcb_t *prev, pcb_t *next);
void do_scheduler(void);
void do_sleep(uint32_t);

void do_block(list_node_t *, list_head *queue);
void do_unblock(list_node_t *);

/************************************************************/
/* Do not touch this comment. Reserved for future projects. */
/************************************************************/

#endif
