#include <os/task_tools.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // read the task program from SD card to buffer
    task_info_t task = tasks[taskid];
    uint64_t task_entry = task.entry; // task program address in memory

    if (atomic_swap(true, (ptr_t)(exists_in_mem + taskid)) == true)
        return task_entry;
    // if (exists_in_mem[taskid] == true)
    //     return task_entry;
    // exists_in_mem[taskid] = true;

    uint32_t task_size = task.filesz; // task program size in SD card
    uint32_t task_off = task.phyaddr; // task program offset in SD card

    int task_sec_begin = task_off / SECTOR_SIZE; // where the task begins (sector)
    int task_sec_end = NBYTES2SEC(task_off + task_size); // where the task ends (sector)
    int task_sec_num = task_sec_end - task_sec_begin; // number of sectors
    bios_sd_read(INITIAL_BUF, task_sec_num, task_sec_begin); // read the task from SD card

    // copy the task from buffer to its own memory space
    memcpy((void *)task_entry, (void *)(INITIAL_BUF + (uint64_t)task_off % SECTOR_SIZE), task_size);

    return task_entry;
}