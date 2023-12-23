#include <os/procst.h>
#include <printk.h>

proc_st_info proc_list[MAX_PROCESS_NUM];
bool mem_occupied[MAX_PROCESS_NUM];

// void init_mem()
// {
//     for (int i = 0; i < MAX_PROCESS_NUM; i++)
//     {
//         proc_list[i].valid = false;
//     }
// }

int available_mem()
{
    int mem_idx = 0;
    for (mem_idx = 0; mem_idx < MAX_PROCESS_NUM; mem_idx++)
    {
        if (mem_occupied[mem_idx] == false)
        {
            break;
        }
    }
    if (mem_idx == MAX_PROCESS_NUM)
    {
        printk("No enough memory for process!\n");
        return -1;
    }
    return mem_idx;
}

void *prc_st_alloc()
{
    for (int info_idx = 0; info_idx < MAX_PROCESS_NUM; info_idx++)
    {
        if (proc_list[info_idx].valid == false)
        {
            proc_list[info_idx].valid = true;
            // proc_list[info_idx].size = MAX_ST_DEPTH;
            int mem_idx = available_mem();
            mem_occupied[mem_idx] = true;
            proc_list[info_idx].base = USER_STACK_BASE - mem_idx * MAX_ST_DEPTH;
            return (void *)proc_list[info_idx].base;
        }
    }
    printk("No enough memory for process!\n");
    return NULL;
}

void prc_st_free(void *ptr)
{
    for (int info_idx = 0; info_idx < MAX_PROCESS_NUM; info_idx++)
    {
        if (proc_list[info_idx].base == (uint64_t)ptr)
        {
            proc_list[info_idx].valid = false;
            int mem_idx = (USER_STACK_BASE - proc_list[info_idx].base) / MAX_ST_DEPTH;
            mem_occupied[mem_idx] = false;
            return;
        }
    }
    printk("No such process!\n");
    return;
}
