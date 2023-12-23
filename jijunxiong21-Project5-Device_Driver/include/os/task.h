#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <lib.h>

#define TASK_MAXNUM      32
// #define TASK_SIZE        0x10000

// #define PID0_PAGE_NUM 2

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))
#define NBYTES2PG(nbytes) (((nbytes) / PAGE_SIZE) + ((nbytes) % PAGE_SIZE != 0))

#define MAX_TASK_NAME_LEN 32 // max length of task name

#define ARGV_MAX_SIZE  256 // max size of argv stack (in bytes) (consists of pointer and memory space)
#define MAX_ARGC 20 // max number of arguments

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    uint64_t entry;    // entry point
    uint32_t filesz;   // actual size of program
    uint32_t memsz;    // size of memory space
    uint32_t phyaddr;  // start address in SD card (read as file)
    char task_name[MAX_TASK_NAME_LEN];
} task_info_t;

typedef struct
{
    PTE *pte_addr; // in kernel, get_pa(*(PTE *)pte_addr) == pa
    ptr_t va; // user virtual address
    uint32_t page_num; // number of pages allocated
    bool valid; // whether the page is valid
} user_page_info_t;

#endif