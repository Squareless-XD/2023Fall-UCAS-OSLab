#include <os/irq.h>
#include <os/mm.h>

/* page fault handler:
 * task2 assign a new page for the fault address
 * task3 add disk check, load data from swap partition
 */
void handle_page_fault_rd(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    WRITE_LOG("read page fault. stval: %lx\n", stval)
    // spin_lock_acquire(&pcb_lk); /* enter critical section */
    alloc_page_helper(stval, 0, my_cpu(), UNPINNED);
    // spin_lock_release(&pcb_lk); /* leave critical section */
}

void handle_page_fault_wr(regs_context_t *regs, uint64_t stval, uint64_t cause)
{
    WRITE_LOG("write page fault. stval: %lx\n", stval)
    // spin_lock_acquire(&pcb_lk); /* enter critical section */
    alloc_page_helper(stval, 0, my_cpu(), UNPINNED);
    cow_check(stval, my_cpu());
    // spin_lock_release(&pcb_lk); /* leave critical section */
}


/* allocate physical page for `va`, mapping it into `pgdir`,
 * return the kernel virtual address for the page
 * note that when pa==0, we allocate a new page
 * when pa!=0, we directly use pa itself as physical address
 */

// spin_lock_t pagedir_lk;

ptr_t alloc_page_helper(ptr_t va, ptr_t pa, pcb_t *pcb_alloc, int pin)
{
    WRITE_LOG("va: %lx, pa: %lx, pcb_idx: %d, pin: %d\n", va, pa, pcb_alloc - pcb, pin)

    // if (va < 0x10000)
    // {
    //     handle_other(regs, stval, scause);
    // }

    /* enter critical section */
    spin_lock_acquire(pcb_alloc->pud_lk);

    // TODO [P4-task1] alloc_page_helper:
    PTE *pgdir = pcb_alloc->pud; // get the page directory of the process
    va &= VA_MASK; // only use lowest 39 bits

    // set up flags
    uint64_t pa_given_flag = (pa == 0) ? 1 : 0;
    uint64_t pin_flag = pin ? _PAGE_PIN : 0;

    // calculate three virtual page numbers
    uint64_t vpn2 = va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS);
    uint64_t vpn1 = (vpn2 << PPN_BITS) ^ (va >> (NORMAL_PAGE_SHIFT + PPN_BITS));
    uint64_t vpn0 = ((1lu << PPN_BITS) - 1) & (va >> NORMAL_PAGE_SHIFT);

    // get the first-level page directory: upper directory
    WRITE_LOG("1st-level page kva: %lx\n", &pgdir[vpn2])
    if (!get_attribute(pgdir[vpn2], _PAGE_VALID))
    {
        assert(!get_attribute(pgdir[vpn2], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC))
        // alloc a new second-level page directory
        set_pfn(&pgdir[vpn2], kva2pa(page_alloc_kern()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pgdir[vpn2], _PAGE_VALID | _PAGE_USER);
        clear_pgdir(get_kva(pgdir[vpn2]));
        WRITE_LOG("add a new 2nd-level page table\n")
    }

    // get the second-level page directory: middle directory
    PTE *pmd = (PTE *)get_kva(pgdir[vpn2]); // middle directory
    WRITE_LOG("2nd-level page kva: %lx\n", pmd)
    if (!get_attribute(pmd[vpn1], _PAGE_VALID)) // NOTE: check whether the page is in TBE
    {
        assert(!get_attribute(pmd[vpn1], _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC))
        // alloc a new page table
        set_pfn(&pmd[vpn1], kva2pa(page_alloc_kern()) >> NORMAL_PAGE_SHIFT);
        set_attribute(&pmd[vpn1], _PAGE_VALID | _PAGE_USER);
        clear_pgdir(get_kva(pmd[vpn1]));
        WRITE_LOG("add a new 3rd-level page table\n")
    }

    // get the third-level page directory: page table entry
    ptr_t pfn;
    PTE *pte = (PTE *)get_kva(pmd[vpn1]); // page table entry
    WRITE_LOG("3rd-level page kva: %lx\n", pte)
    // not in memory: not assigned or in SD card
    if (!get_attribute(pte[vpn0], _PAGE_VALID))
    {
        // this page might be in SD card now, so we need to load it into memory
        // check whether the page is in SD card
        int idx;
        for (idx = 0; idx < pcb_alloc->mem_num; idx++)
        {
            ptr_t va_base = pcb_alloc->mem_info[idx].va & VA_MASK;
            ptr_t va_end = va_base + pcb_alloc->mem_info[idx].page_num * PAGE_SIZE;
            if (va >= va_base && va < va_end) // the page is in SD card
            {
                // load it into memory
                check_page_full(pcb_alloc);
                assert(swap_from_sd(pte[vpn0], va) == 0)
                break;
            }
        }
        if (idx == pcb_alloc->mem_num) // the page is not in SD card. not assigned
        {
            // alloc a new page if (pa == 0)
            if (pa_given_flag)
            {
                check_page_full(pcb_alloc);
                pfn = kva2pa(alloc_page_proc(pcb_alloc->mem_info, 1, va,
                                             &pte[vpn0])) >>
                      NORMAL_PAGE_SHIFT;
            }
            else
                pfn = pa >> NORMAL_PAGE_SHIFT;
            set_pfn(&pte[vpn0], pfn);
            set_attribute(&pte[vpn0], _PAGE_VALID | _PAGE_READ | _PAGE_WRITE |
                                          _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED |
                                          _PAGE_DIRTY | pin_flag);
        }
    }
    // else // in memory
    // {
    // }
    WRITE_LOG("va: %lx, pa: %lx\n", va, get_pa(pte[vpn0]))

    spin_lock_release(pcb_alloc->pud_lk);
    /* leave critical section */

    // local_flush_tlb_page(va);
    return get_kva(pte[vpn0]) + va % PAGE_SIZE;
}

void cow_check(ptr_t va, pcb_t *pcb_alloc)
{
    /* enter critical section */
    spin_lock_acquire(pcb_alloc->pud_lk);

    PTE *pgdir = pcb_alloc->pud; // get the page directory of the process
    va &= VA_MASK; // only use lowest 39 bits

    // calculate three virtual page numbers
    uint64_t mask = (1lu << PPN_BITS) - 1;
    uint64_t vpn2 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS + PPN_BITS)) & mask;
    uint64_t vpn1 = (va >> (NORMAL_PAGE_SHIFT + PPN_BITS)) & mask;
    uint64_t vpn0 = (va >> (NORMAL_PAGE_SHIFT)) & mask;

    // get the first-level page directory: upper directory
    assert(get_attribute(pgdir[vpn2], _PAGE_VALID))

    // get the second-level page directory: middle directory
    PTE *pmd = (PTE *)get_kva(pgdir[vpn2]); // middle directory
    if (!get_attribute(pmd[vpn1], _PAGE_VALID | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC)) // a large page
        return;
    assert(get_attribute(pmd[vpn1], _PAGE_VALID))

    // get the third-level page directory: page table entry
    PTE *pte = (PTE *)get_kva(pmd[vpn1]); // page table entry
    assert(get_attribute(pte[vpn0], _PAGE_VALID))
    if (get_attribute(pte[vpn0], _PAGE_FORKED))
    {
        if (cow_try_eliminate(get_kva(pte[vpn0])) != 0) // deref the page
        {
            WRITE_LOG("dereference page with pa: %lx\n", get_pa(pte[vpn0]))

            // alloc a new page table
            ptr_t old_kva = get_kva(pte[vpn0]);
            ptr_t new_kva = alloc_page(1);
            set_pfn(&pte[vpn0], kva2pa(new_kva) >> NORMAL_PAGE_SHIFT);
            // clear_pgdir(get_kva(pte[vpn0]));
            memcpy((void *)new_kva, (void *)old_kva, PAGE_SIZE);

            WRITE_LOG("dereference old pa: %lx to new pa: %lx\n", kva2pa(old_kva), get_pa(pte[vpn0]))
        }
        set_attribute(&pte[vpn0], _PAGE_WRITE);
        clear_attribute(&pte[vpn0], _PAGE_FORKED);
    }

    spin_lock_release(pcb_alloc->pud_lk);
    /* leave critical section */

    // local_flush_tlb_page(va);
}
