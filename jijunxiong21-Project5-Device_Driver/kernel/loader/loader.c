#include <os/loader.h>
#include <os/kernel.h>
#include <os/mm.h>
// #include <os/sched.h> // ignored
#include <os/task_tools.h>

uint64_t load_task_img(int taskid, pcb_t *pcb_alloc) // with pcb_lk locked
{
    /**
     * TODO:
     * 1. [p1-task3] load task from image via task id, and return its entrypoint
     * 2. [p1-task4] load task via task name, thus the arg should be 'char *taskname'
     */

    // read the task program from SD card to buffer
    task_info_t task = tasks[taskid];
    uint64_t task_entry = task.entry; // task program address in memory

    // if (atomic_swap(true, (ptr_t)(exists_in_mem + taskid)) == true)
    //     return task_entry;
    // if (exists_in_mem[taskid] == true)
    //     return task_entry;
    // exists_in_mem[taskid] = true;

    uint32_t task_size = task.filesz; // task program size in SD card
    uint32_t mem_size = task.memsz; // task program size in memory
    uint32_t task_off = task.phyaddr; // task program offset in SD card

    int task_sec_begin = task_off / SECTOR_SIZE; // where the task begins (sector)
    int task_sec_end = NBYTES2SEC(task_off + task_size); // where the task ends (sector)
    int task_sec_num = task_sec_end - task_sec_begin; // number of sectors
    bios_sd_read(kva2pa(INITIAL_BUF), task_sec_num, task_sec_begin); // read the task from SD card
    __sync_synchronize();
    local_flush_icache_all(); // flush the cache

    WRITE_LOG("task %d: entry = %lx, task_size = %d, mem_size = %d, task_off = %d\n",
              taskid, task_entry, task_size, mem_size, task_off)

    // get new pages for the task
    inst_alloc(mem_size, task_entry, pcb_alloc, INITIAL_BUF + (uint64_t)task_off % SECTOR_SIZE);

    return task_entry;
}


ptr_t inst_alloc(uint32_t mem_size, uint64_t task_entry, pcb_t *pcb_alloc, ptr_t buf_base)
{
    // TODO: assert()
    ptr_t va = task_entry; // alignize task_entry
    assert(va % PAGE_SIZE == 0);
    ptr_t va_end = va + mem_size; // get the end of the task
    ptr_t va_assign;
    ptr_t buf_ptr = buf_base;

    while (va < va_end) // allocate the rest pages in a while loop
    {
        // call alloc_page_helper to allocate a page
        va_assign = alloc_page_helper(va, 0, pcb_alloc, PINNED);

        // copy the task from buffer to its own memory space
        memcpy((void *)va_assign, (void *)buf_ptr, PAGE_SIZE);

        va += PAGE_SIZE;
        buf_ptr += PAGE_SIZE;
    }
    // flush the cache
    __sync_synchronize();
    local_flush_icache_all();

    return va_end - mem_size; // get a vaddr that kernel can operate (1st page)
}

