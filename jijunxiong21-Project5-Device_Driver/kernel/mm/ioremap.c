#include <os/ioremap.h>
#include <os/mm.h>
// #include <pgtable.h>
// #include <type.h>

#define LARGE_PAGE_MASK 0xffffffffffe00000
#define ROUND_UP_LARGE_PAGE(x) (((x) + LARGE_PAGE_SIZE - 1) & LARGE_PAGE_MASK)
#define ROUND_DOWN_LARGE_PAGE(x) ((x) & LARGE_PAGE_MASK)

// maybe you can map it to IO_ADDR_START ?
static uintptr_t io_base = IO_ADDR_START;

// using 2MB large page
static void map_page_kern(uint64_t va, uint64_t pa, PTE *pgdir)
{
    va &= VA_MASK; // only use lowest 39 bits
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));

    if (!get_attribute(pgdir[vpn2], _PAGE_VALID)) // if second-level page directory is not allocated
    {
        // alloc a new second-level page directory
        set_pfn(&pgdir[vpn2], kva2pa(page_alloc_kern()) >> NORMAL_PAGE_SHIFT); // set physical page number
        set_attribute(&pgdir[vpn2], _PAGE_VALID); // set attribute
        clear_pgdir(get_kva(pgdir[vpn2])); // clear the new second-level page directory
    }

    PTE *pmd = (PTE *)get_kva(pgdir[vpn2]); // get the second-level page directory
    if (!get_attribute(pmd[vpn1], _PAGE_VALID)) // if second-level page directory is not allocated
    {
        set_pfn(&pmd[vpn1], pa >> NORMAL_PAGE_SHIFT); // set physical page number
        set_attribute(&pmd[vpn1], _PAGE_VALID | _PAGE_READ | _PAGE_WRITE |
                                      _PAGE_EXEC | _PAGE_ACCESSED |
                                      _PAGE_DIRTY | _PAGE_PIN);
    }
}

// naively map given physical region to linear increasing virtual address
void *ioremap(unsigned long phys_addr, unsigned long size)
{
    // TODO: [p5-task1] map one specific physical region to virtual address
    ptr_t va_base = io_base;
    io_base += size;
   
    ptr_t va = ROUND_DOWN_LARGE_PAGE(va_base); // beginning address of the first page
    ptr_t va_end = ROUND_UP_LARGE_PAGE(va_base + size); // ending address of the last page
    ptr_t pa = ROUND_DOWN_LARGE_PAGE(phys_addr); // beginning address of the first page
    while (va < va_end) {
        map_page_kern(va, pa, kern_pgdir);
        va += LARGE_PAGE_SIZE;
        pa += LARGE_PAGE_SIZE;
    }
    local_flush_tlb_all();
    return (void *)va_base;
}

// naively unmap by doing nothing
void iounmap(void *io_addr)
{
    // TODO: [p5-task1] a very naive iounmap() is OK
    // maybe no one would call this function?
}
