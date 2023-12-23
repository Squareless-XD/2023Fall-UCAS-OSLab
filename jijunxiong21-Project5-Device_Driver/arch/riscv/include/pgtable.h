#ifndef PGTABLE_H
#define PGTABLE_H

#include <type.h>
#include <os/addr.h>

#define SATP_MODE_SV39 8 // Sv39 mode in SATP register
#define SATP_MODE_SV48 9 // Sv48 mode in SATP register

#define SATP_ASID_SHIFT 44lu // ASID area start bit in SATP register
#define SATP_MODE_SHIFT 60lu // mode area start bit in SATP register

#define NORMAL_PAGE_SHIFT 12lu // normal page shift: page frame number shift left 12 bits to get physical addr
#define NORMAL_PAGE_SIZE (1lu << NORMAL_PAGE_SHIFT) // normal page size: 4KB
#define LARGE_PAGE_SHIFT 21lu // large page shift: page frame number shift left 21 bits to get physical addr
#define LARGE_PAGE_SIZE (1lu << LARGE_PAGE_SHIFT) // large page size: 2MB

/*
 * Flush entire local TLB.  'sfence.vma' implicitly fences with the instruction
 * cache as well, so a 'fence.i' is not necessary.
 */
static inline void local_flush_tlb_all(void)
{
    __asm__ __volatile__ ("sfence.vma" : : : "memory");
}

/* Flush one page from local TLB */
static inline void local_flush_tlb_page(unsigned long addr)
{
    __asm__ __volatile__ ("sfence.vma %0" : : "r" (addr) : "memory");
}

/* Flush entire local icache ??? */
static inline void local_flush_icache_all(void)
{
    asm volatile ("fence.i" ::: "memory");
}

/* set satp register */
static inline void set_satp(
    unsigned mode, unsigned asid, unsigned long ppn)
{
    unsigned long __v =
        (unsigned long)(((unsigned long)mode << SATP_MODE_SHIFT) | ((unsigned long)asid << SATP_ASID_SHIFT) | ppn);
    __asm__ __volatile__("sfence.vma\ncsrw satp, %0" : : "rK"(__v) : "memory");
}

#define PGDIR_PA 0x51000000lu  // use 51000000 page as PGDIR

/*
 * PTE format:
 * | XLEN-1  10 | 9             8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0
 *       PFN      reserved for SW   D   A   G   U   X   W   R   V
 */

#define _PAGE_ACCESSED_OFFSET 6 // accessed bit offset in PTE

#define _PAGE_VALID (1 << 0)    /* Valid */
#define _PAGE_READ (1 << 1)     /* Readable */
#define _PAGE_WRITE (1 << 2)    /* Writable */
#define _PAGE_EXEC (1 << 3)     /* Executable */
#define _PAGE_USER (1 << 4)     /* User */
#define _PAGE_GLOBAL (1 << 5)   /* Global */
#define _PAGE_ACCESSED (1 << 6) /* Set by hardware on any access */
#define _PAGE_DIRTY (1 << 7)    /* Set by hardware on any write */
#define _PAGE_FORKED (1 << 8)   /* Copy-on-Write Page */
#define _PAGE_PIN (1 << 9)      /* Pinned in Memory */

#define _PAGE_PFN_SHIFT 10lu // page frame number shift in PTE

#define VA_MASK ((1lu << 39) - 1) // virtual addr mask

#define PPN_BITS 9lu // page frame number bits in PTE
#define NUM_PTE_ENTRY (1 << PPN_BITS) // number of PTE entries in one page table

typedef uint64_t PTE; // page table entry

/* Translation between physical addr and kernel virtual addr */
static inline ptr_t kva2pa(ptr_t kva) // kernel virtual addr to physical addr
{
    /* TODO: [P4-task1] */
    return kva - KERNEL_ADDR_BASE;
}

static inline ptr_t pa2kva(ptr_t pa) // physical addr to kernel virtual addr
{
    /* TODO: [P4-task1] */
    return pa + KERNEL_ADDR_BASE;
}

/* get physical page addr from PTE 'entry' */
static inline uint64_t get_pa(const PTE entry) // get physical page addr
{
    /* TODO: [P4-task1] */
    return (entry >> _PAGE_PFN_SHIFT << NORMAL_PAGE_SHIFT) & VA_MASK;
}

/* get kernel virtual addr from PTE 'entry' */
static inline uint64_t get_kva(const PTE entry) // get kernal virtual addr
{
    /* TODO: [P4-task1] */
    return pa2kva(get_pa(entry));
}

/* Get/Set page frame number of the `entry` */
static inline long get_pfn(const PTE entry) // get page frame number
{
    /* TODO: [P4-task1] */
    return (entry >> _PAGE_PFN_SHIFT) &
           ((1lu << (PPN_BITS + PPN_BITS + PPN_BITS)) - 1);
}
static inline void set_pfn(PTE *entry, uint64_t pfn) // set page frame number
{
    /* TODO: [P4-task1] */
    // clear pfn bits and set new pfn
    *entry = ((pfn & ((1lu << (PPN_BITS + PPN_BITS + PPN_BITS)) - 1))
               << _PAGE_PFN_SHIFT) |
             (*entry & ((1lu << _PAGE_PFN_SHIFT) - 1));
}

/* Get/Set attribute(s) of the `entry` */
static inline long get_attribute(const PTE entry, uint64_t mask) // get attribute(s)
{
    /* TODO: [P4-task1] */
    return entry & mask;
}
static inline void set_attribute(PTE *entry, uint64_t bits) // set attribute(s)
{
    /* TODO: [P4-task1] */
    *entry |= bits & ((1lu << _PAGE_PFN_SHIFT) - 1);
}
static inline void clear_attribute(PTE *entry, uint64_t bits) // set attribute(s)
{
    /* TODO: [P4-task1] */
    *entry &= ~(bits & ((1lu << _PAGE_PFN_SHIFT) - 1));
}

static inline void clear_pgdir(ptr_t pgdir_addr) // clear page directory
{
    /* TODO: [P4-task1] */
    for (int i = 0; i < NUM_PTE_ENTRY; ++i) {
        ((PTE *)pgdir_addr)[i] = 0;
    }
}

#endif  // PGTABLE_H
