#ifndef __KERN_ADDR_H__
#define __KERN_ADDR_H__

// kernel virtual addr base (used by kva2pa and pa2kva)
#define KERNEL_ADDR_BASE 0xffffffc000000000lu

#ifdef S_CORE
#define LARGE_PAGE_FREEMEM 0xffffffc056000000lu
#define USER_STACK_ADDR 0x400000
#else
#define USER_STACK_ADDR     0xf00010000
#endif

#define BOOT_LOADER_ADDR    0xffffffc050200000lu
#define APP_INFO_OFF_ADDR   0xffffffc0502001fclu

#define USER_PUD_ADDR       0xffffffc051090000lu // user's page upper directory address
// 0x30080 used for many purposes
// #define USER_PUD_NUM        (USER_PGDIR_BASE - USER_PUD_ADDR) / sizeof(PTE)

// Total page num in boot_kernel 0x1 + 0x80 + 0x8 = 0x89
// page_alloc_kern() and page_free_kern(), allocate and free an aligned page
// used for page table memory management
#define USER_PGDIR_BASE     0xffffffc051300000lu
#define USER_PGDIR_TOP      0xffffffc051c00000lu
#define USER_PGDIR_PG_NUM   ((USER_PGDIR_TOP - USER_PGDIR_BASE) / PAGE_SIZE)

// for kernel memory management: kmalloc() and kfree()
#define KERN_MEM_BASE       0xffffffc051c00000lu
#define KERN_MEM_TOP        0xffffffc052000000lu
#define KERN_MEM_SIZE       (KERN_MEM_TOP - KERN_MEM_BASE - sizeof(km_mdata_t))

// kernel stack addr, above are pid0_pcbs (two processes)
// #define INIT_KERNEL_STACK   0xffffffc052000000lu
#define INITIAL_BUF         0xffffffc05f000000lu
#define PID0_STACK_CORE0    0xffffffc05ff04000lu
#define PID0_STACK_CORE1    0xffffffc05ff08000lu
#define PID0_PUD_LK_ADDR    0xffffffc05ff08100lu

#endif  // __KERN_ADDR_H__