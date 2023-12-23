#include <os/task.h>
#include <os/string.h>
#include <os/kernel.h>
#include <type.h>

uint64_t load_task_img(int taskid)
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // read the task program from SD card to buffer
    task_info_t task = tasks[taskid];
    int task_entry = task.entry; // task program address in memory

    if (exists_in_mem[taskid] == true)
        return task_entry;

    int task_size = task.filesz; // task program size in SD card
    int task_off = task.phyaddr; // task program offset in SD card

    int task_sec_begin = task_off / SECTOR_SIZE; // where the task begins (sector)
    int task_sec_end = NBYTES2SEC(task_off + task_size); // where the task ends (sector)
    int task_sec_num = task_sec_end - task_sec_begin; // number of sectors
    bios_sd_read(INITIAL_BUF, task_sec_num, task_sec_begin); // read the task from SD card

    int mem_size;
    
    // copy the task from buffer to its own memory space
    // memcpy((void *)task_entry, (void *)(INITIAL_BUF + task_off % SECTOR_SIZE), task_size);
    decompress(INITIAL_BUF + task_off % SECTOR_SIZE, task_size, task_entry, &mem_size);

    return task_entry;
}