// This file defines the data structure and functions for process stack management
// The process stack is allocated in the user memory space
// NOTE: this file is added by the student

#ifndef INCLUDE_PROC_ST_H_
#define INCLUDE_PROC_ST_H_

#include <type.h>
#include <printk.h>
#include <os/lock.h>

#define MAX_PROCESS_NUM 0x80
#define PROC_PAGE_NUM 0x80
#define MAX_PAGE_NUM 0xDB00 // MAX_PROCESS_NUM * PROC_PAGE_NUM
#define MAX_PAGE_NUM_ST 0x4000
#define MAX_PAGE_NUM_HP 0x9B00 // MAX_PAGE_NUM - MAX_PAGE_NUM_ST
#define MAX_ST_DEPTH 0x80000 // PAGE_SIZE * PROC_PAGE_NUM
// 0xDB in decimal is 219, which is 1/256 of the number of pages in user memory
// 0x6D in decimal is 109, half of 219
// 0x49 in decimal is 73, 219/3 = 73
// actually one process can get the stack of 0x30 pages if we want

#define USER_STACK_BASE 0x60000000

typedef struct block
{
    uint64_t base;
    // uint32_t size;
    bool valid;
} proc_st_info;

// page size: 4KB (0x1000)
// BBL mem:                 0x5000 0000 - 0x5020 0000  0x200 pages
// Kernel data & program:   0x5020 0000 - 0x5050 0000  0x300 pages
// Kernel memory:           0x5050 0000 - 0x5200 0000  0x1B00 pages
// User data & program:     0x5200 0000 - 0x5250 0000  0x500 pages
// User memory:             0x5250 0000 - 0x6000 0000  0xDB00 pages

// void init_mem();
int available_mem();
void *prc_st_alloc();
void prc_st_free(void *ptr);

#endif
