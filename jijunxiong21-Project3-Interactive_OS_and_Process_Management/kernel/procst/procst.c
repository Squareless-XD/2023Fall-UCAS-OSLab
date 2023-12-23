#include <os/procst.h>

static proc_st_info proc_list[MAX_PROCESS_NUM];
static bool mem_occupied[MAX_PROCESS_NUM];

static spin_lock_t mem_spinlock = {UNLOCKED};

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
    spin_lock_acquire(&mem_spinlock); /* enter critical section */
    for (int info_idx = 0; info_idx < MAX_PROCESS_NUM; info_idx++)
    {
        if (proc_list[info_idx].valid == false)
        {
            proc_list[info_idx].valid = true;
            // proc_list[info_idx].size = MAX_ST_DEPTH;
            int mem_idx = available_mem();
            mem_occupied[mem_idx] = true;
            proc_list[info_idx].base = USER_STACK_BASE - mem_idx * MAX_ST_DEPTH;
            spin_lock_release(&mem_spinlock); /* leave critical section */

            return (void *)proc_list[info_idx].base;
        }
    }
    spin_lock_release(&mem_spinlock); /* leave critical section */

    printk("No enough memory for process!\n");
    return NULL;
}

void prc_st_free(void *ptr)
{
    spin_lock_acquire(&mem_spinlock); /* enter critical section */
    for (int info_idx = 0; info_idx < MAX_PROCESS_NUM; info_idx++)
    {
        if (proc_list[info_idx].base == (uint64_t)ptr)
        {
            proc_list[info_idx].valid = false;
            int mem_idx = (USER_STACK_BASE - proc_list[info_idx].base) / MAX_ST_DEPTH;
            mem_occupied[mem_idx] = false;
            spin_lock_release(&mem_spinlock); /* leave critical section */

            return;
        }
    }
    spin_lock_release(&mem_spinlock); /* leave critical section */

    printk("No such process!\n");
    return;
}
