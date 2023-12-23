#include <os/task_tools.h>
// #include <os/sched.h> // ignored

// get app info address from memory
int task_num;
long sd_swap_base;

// Task info array
task_info_t tasks[TASK_MAXNUM];
int exists_in_mem[TASK_MAXNUM];

// prepare to the input of running tasks
int run_task_num = 0;
pcb_t *run_tasks[MAX_PROCESS_NUM]; // though I don't know is it OK to reach the max process num

int task_cmp(const char* buf, int strlen)
{
    int task_idx;
    for (task_idx = 0; task_idx < task_num; ++task_idx)
        if (tasks[task_idx].task_name[0] != '\0' && strcmp(buf, tasks[task_idx].task_name) == 0) // find the task
            return task_idx;

    return -1;
}

// bool task_exist_check(int task_idx)
// {
//     for (int i = 0; i < run_task_num; ++i)
//         if (run_tasks[i] != NULL && run_tasks[i]->task_id == task_idx)
//             return true;

//     return false;
// }

