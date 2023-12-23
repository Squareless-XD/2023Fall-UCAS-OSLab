#ifndef INCLUDE_TASK_TOOLS_H_
#define INCLUDE_TASK_TOOLS_H_

// #include <os/task.h> // ignored
#include <os/sched.h>

#define MAX_THREAD_NUM 32
#define ALLOC_KERNEL_PAGE_NUM 5
#define ALLOC_USER_PAGE_NUM 5
#define MAX_PROCESS_NUM 0x80

// get app info address from memory
extern int task_num;
extern long sd_swap_base;

// Task info array
extern task_info_t tasks[TASK_MAXNUM];
extern int exists_in_mem[TASK_MAXNUM];

// prepare to the input of running tasks
extern int run_task_num;
extern pcb_t *run_tasks[MAX_PROCESS_NUM]; // though I don't know is it OK to reach the max process num

int task_cmp(const char* buf, int strlen);
// extern bool task_exist_check(int task_idx);
pcb_t *find_PCB_addr(list_node_t *list_node);
void exec_next_task(void);
void switch_to_wrap(pcb_t *prev_proc, pcb_t *cpu_proc);
void init_pcb_stack(ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
                    pcb_t *pcb, reg_t arg0, reg_t arg1, reg_t ra);
// void init_pcb_stack_main(ptr_t kernel_stack, ptr_t user_stack,
//                          ptr_t entry_point, pcb_t *pcb);
pid_t init_pcb_args(int task_idx, int argc, char **argv);
pcb_t *init_pcb(int task_idx); // initialize PCB in main.c

#endif
