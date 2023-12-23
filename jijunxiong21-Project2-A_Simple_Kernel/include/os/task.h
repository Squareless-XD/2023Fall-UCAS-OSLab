#ifndef __INCLUDE_TASK_H__
#define __INCLUDE_TASK_H__

#include <type.h>

#define TASK_MEM_BASE    0x52000000
#define TASK_MAXNUM      16
#define TASK_SIZE        0x10000

#define BOOT_LOADER_ADDR 0x50200000
#define APP_INFO_OFF_ADDR 0x502001fe
#define INITIAL_BUF 0x53000000

#define SECTOR_SIZE 512
#define NBYTES2SEC(nbytes) (((nbytes) / SECTOR_SIZE) + ((nbytes) % SECTOR_SIZE != 0))

#define MAX_TASK_NAME_LEN 32 // max length of task name

/* TODO: [p1-task4] implement your own task_info_t! */
typedef struct {
    uint64_t entry;    // entry point
    uint32_t filesz;   // actual size of program
    uint32_t phyaddr;  // start address in SD card (read as file)
    char task_name[MAX_TASK_NAME_LEN];
} task_info_t;

extern task_info_t tasks[TASK_MAXNUM];
extern int exists_in_mem[TASK_MAXNUM];

#endif