// #include <os/list.h>
// #include <os/lock.h>
#include <os/sched.h>
// #include <os/time.h>
// #include <os/mm.h>
// #include <screen.h>
#include <printk.h>
#include <os/string.h>
// #include <assert.h>
// #include <os/procst.h>



// get app info address from memory
unsigned short info_off;
int task_num;

// Task info array
task_info_t tasks[TASK_MAXNUM];
int exists_in_mem[TASK_MAXNUM];

// prepare to the input of running tasks
int run_task_num = 0;
int run_task_idx[MAX_PROCESS_NUM]; // though I don't know is it OK to reach the max process num


int task_cmp(char* buf, int strlen)
{
    int task_idx;
    for (task_idx = 0; task_idx < task_num; ++task_idx)
    {
        if (strcmp(buf, tasks[task_idx].task_name) == 0) // find the task
        {
            printk("Task found!\n");
            return task_idx;
        }
    }
    printk("Task not found!\n");
    return -1;
}

bool task_exist_check(int *run_task_idx, int run_task_num, int task_idx)
{
    for (int i = 0; i < run_task_num; ++i)
    {
        if (run_task_idx[i] == task_idx)
        {
            return true;
        }
    }
    return false;
}

