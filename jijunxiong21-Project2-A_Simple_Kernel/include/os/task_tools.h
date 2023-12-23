#ifndef INCLUDE_TASK_TOOLS_H_
#define INCLUDE_TASK_TOOLS_H_

#include <os/sched.h>

#define MAX_THREAD_NUM 4
#define ALLOC_KERNEL_PAGE_NUM 5
#define ALLOC_USER_PAGE_NUM 5

// get app info address from memory
extern unsigned short info_off;
extern int task_num;

// Task info array
extern task_info_t tasks[TASK_MAXNUM];
extern int exists_in_mem[TASK_MAXNUM];

// prepare to the input of running tasks
extern int run_task_num;
extern int run_task_idx[MAX_PROCESS_NUM]; // though I don't know is it OK to reach the max process num

int task_cmp(char* buf, int strlen);
bool task_exist_check(int *run_task_idx, int run_task_num, int task_idx);
uint64_t find_PCB_addr(list_node_t *list_node);
void exec_next_task(void);
void init_pcb_stack(
    ptr_t kernel_stack, ptr_t user_stack, ptr_t entry_point,
    pcb_t *pcb);
void init_pcb(int task_idx); // initialize PCB in main.c

#endif
