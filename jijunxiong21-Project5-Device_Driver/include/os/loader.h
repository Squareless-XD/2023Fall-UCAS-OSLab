#ifndef __INCLUDE_LOADER_H__
#define __INCLUDE_LOADER_H__

#include <lib.h>
#include <os/sched.h>

ptr_t inst_alloc(uint32_t mem_size, uint64_t task_entry, pcb_t *pcb_alloc, ptr_t buf_base);
uint64_t load_task_img(int taskid, pcb_t *pcb_alloc);

#endif