/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Memory Management
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */
#ifndef MM_H
#define MM_H

#include <os/task.h> // for NR_CPUS
#include <os/smp.h> // for NR_CPUS
#include <os/sched.h>

#define MAP_KERNEL 1
#define MAP_USER 2
#define MEM_SIZE 32
#define PAGE_SIZE 0x1000 // 4K
// #define FREEMEM_KERNEL (INIT_KERNEL_STACK + PID0_PAGE_NUM * NR_CPUS * PAGE_SIZE)

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))
#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define MAX_ORDER 16 // max order of buddy system

#define KERNEL_STACK_PAGE 2

// types defination

typedef struct free_area
{
    list_head free_list;
    int nr_free;
} free_area_t;

typedef struct zone
{
    /* free areas of different sizes */
    free_area_t free_area[MAX_ORDER];
} zone_t;

typedef struct _km_mdata {
    struct _km_mdata *next;
    uint32_t size;
    bool used;
} km_mdata_t;

typedef struct pg_info {
    union {
        struct pg_info *prev;
        struct pg_info *head;
    };
    struct pg_info *next;
    uint32_t pg_num;
} pg_info_t;

typedef struct {
    list_node_t list; // free list
    ptr_t kva; // kernal virtual address
    int ref_num; // reference number, for copy-on-write
} bs_pg_info_t; // buddy system allocated page information

// global variables
extern zone_t zone;
extern PTE *kern_pgdir; // kernel page table entry
extern km_mdata_t km_head;

// functions 
void cow_create(ptr_t kva);
int cow_try_eliminate(ptr_t kva);
void init_alloc(void);
ptr_t alloc_page_proc(user_page_info_t *mem_info, int numPage, ptr_t va, PTE *pte_addr);
ptr_t alloc_page(int numPage);

// TODO [P4-task1] */
void free_page_proc(ptr_t pte_addr, user_page_info_t *mem_info);
void free_page(ptr_t pte_addr, int page_num);

// #define S_CORE
// NOTE: only need for S-core to alloc 2MB large page
#ifdef S_CORE
ptr_t allocLargePage(int numPage);
#endif

// TODO [P4-task1] */
void* kmalloc(size_t size);
void kfree(void *ptr);
void share_pgtable(PTE *dest_pgdir, const PTE *src_pgdir);
void fork_copy_pgtable(PTE *dest_pgdir, const PTE *src_pgdir);

ptr_t alloc_page_helper(ptr_t va, ptr_t pa, pcb_t *pcb_alloc, int pin);
void cow_check(ptr_t va, pcb_t *pcb_alloc);

void kern_map_large_pg(ptr_t va, PTE *pgdir);
void free_tlb_page(PTE *pgdir);

ptr_t page_alloc_kern(void);
void page_free_kern(ptr_t page_addr);

// TODO [P4-task4]: shm_page_get/dt */
ptr_t shm_page_get(int key);
void shm_page_dt(ptr_t addr);



#define SD_MAX_PG_NUM (0x20000000 / PAGE_SIZE)
#define SEC_PER_PAGE (PAGE_SIZE / SECTOR_SIZE)
enum {
    UNPINNED = 0,
    PINNED = 1,
};

typedef struct pg_info_sd {
    list_node_t list;
    uint32_t sd_addr;
    uint32_t pg_num;
} pg_info_sd_t;

int swap_to_sd(ptr_t pte, ptr_t va);
int swap_from_sd(ptr_t pte, ptr_t va);
bool check_page_full(pcb_t *pcb_check);


#endif /* MM_H */
