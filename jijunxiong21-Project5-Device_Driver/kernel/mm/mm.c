#include <os/mm.h>
// #include <os/lock.h>
// #include <os/sched.h>

// 0xffffffc052004000 - 0xffffffc060000000
// static ptr_t kernMemCurr = FREEMEM_KERNEL;

// static spin_lock_t kernMemCurr_lk = {UNLOCKED};


void init_alloc(void)
{
    void zone_init(void);
    void kmalloc_init(void);
    void page_alloc_kern_init(void);
    void page_alloc_sd_init(void);

    zone_init();
    kmalloc_init();
    page_alloc_kern_init();
    page_alloc_sd_init();

    /* for order:14, assign 2 free pages
     * 0xffff_ffc0_5400_0000 - 0xffff_ffc0_5800_0000
     * 0xffff_ffc0_5800_0000 - 0xffff_ffc0_5c00_0000
     */
    zone.free_area[14].nr_free = 2;
    list_insert_2(&(zone.free_area[14].free_list),
                  (list_node_t *)0xffffffc054000000lu,
                  (list_node_t *)0xffffffc058000000lu);

    /* for order:13, assign 2 free pages
     * 0xffff_ffc0_5200_0000 - 0xffff_ffc0_5400_0000
     * 0xffff_ffc0_5c00_0000 - 0xffff_ffc0_5d00_0000
     */
    zone.free_area[13].nr_free = 2;
    list_insert_2(&(zone.free_area[13].free_list),
                  (list_node_t *)0xffffffc052000000lu,
                  (list_node_t *)0xffffffc05c000000lu);

    /* for order:12, assign 1 free pages
     * 0xffff_ffc0_5E00_0000 - 0xffff_ffc0_5F00_0000
     */
    zone.free_area[12].nr_free = 1;
    list_insert(&(zone.free_area[12].free_list),
                (list_node_t *)0xffffffc05e000000lu);

}

/* ------------------------------------
    page_alloc_kern() and page_free_kern()
    functions that provide or reclaim an aligned page in kernel space
    spaces for allocation: 0xffff_ffc0_5110_0000 - 0xffff_ffc0_51C0_0000
    every continuous "UNALLOCATED" page block has two boundary sturcture to record the size
    free page blocks are organized as a doubly linked list, with the head node is a global variable page_head
    used page are not in the linked list
   ------------------------------------ */

static pg_info_t page_head;
static pg_info_t *page_ptr;

spin_lock_t page_alloc_kern_lk = {UNLOCKED};

void page_alloc_kern_init(void)
{
    // initialize the head node
    page_head.prev = page_head.next = (pg_info_t *)USER_PGDIR_BASE;

    // initialize the head struct of first node
    pg_info_t *first_node_hd = (pg_info_t *)USER_PGDIR_BASE;
    first_node_hd->prev = first_node_hd->next = &page_head;
    first_node_hd->pg_num = USER_PGDIR_PG_NUM;

    // initialize the tail struct of first node
    pg_info_t *first_node_tl = (pg_info_t *)(USER_PGDIR_TOP - sizeof(pg_info_t));
    first_node_tl->prev = &page_head;

    // initialize the page pointer
    page_ptr = first_node_hd;
}

// allocate 1 page
ptr_t page_alloc_kern(void)
{
    WRITE_LOG("need a page for PTE\n")
    /* enter critical section */
    spin_lock_acquire(&page_alloc_kern_lk);

    // load the first node of the list
    pg_info_t *pg_info = page_ptr;

    // if reach the end (equal to head node), leave the loop
    if (pg_info == &page_head)
    {
        if (pg_info->next == &page_head) // if no node is found
            assert(0)
        pg_info = pg_info->next;
    }

    // if the memory block is large than 1 page, move the head struct of the memory block
    if (pg_info->pg_num > 1)
    {
        // create a new node by address calculation
        pg_info_t *new_info = (pg_info_t *)((ptr_t)pg_info + PAGE_SIZE);
        pg_info_t *node_tail = (pg_info_t *)((ptr_t)pg_info + (ptr_t)pg_info->pg_num * PAGE_SIZE - sizeof(pg_info_t));

        // insert the new node into the list
        new_info->next = pg_info->next;
        new_info->prev = pg_info->prev;
        pg_info->prev->next = new_info;
        pg_info->next->prev = new_info;
        node_tail->head = new_info;

        // change the size of the two nodes
        new_info->pg_num = pg_info->pg_num - 1;

        page_ptr = new_info;
    }
    // if the node is not so large, just use it.
    else
    {
        page_ptr = page_ptr->next;
    }

    spin_lock_release(&page_alloc_kern_lk);
    /* leave critical section */

    WRITE_LOG("allocated page base address: %lx\n", (ptr_t)pg_info)

    // return the address of the memory block
    return (ptr_t)pg_info;
}

// reclaim a page
void page_free_kern(ptr_t page_addr)
{
    WRITE_LOG("need to free page base address %lx\n", page_addr)

    /* enter critical section */
    spin_lock_acquire(&page_alloc_kern_lk);

    // get the node of the memory block
    pg_info_t *free_node = (pg_info_t *)page_addr;

    // check legality of the page address
    if (page_addr < USER_PGDIR_BASE || page_addr >= USER_PGDIR_TOP || page_addr % PAGE_SIZE != 0) {
        spin_lock_release(&page_alloc_kern_lk);
        /* leave critical section */
        return;
    }
    // check the validity of the page address (please keep the prev free area)
    pg_info_t *prev_node = &page_head;
    while (prev_node->next != &page_head && prev_node->next <= free_node)
        prev_node = prev_node->next;
    // check if the page address is allocated
    if (prev_node != &page_head && page_addr - (ptr_t)prev_node < (ptr_t)prev_node->pg_num * PAGE_SIZE) {
        spin_lock_release(&page_alloc_kern_lk);
        /* leave critical section */
        return;
    }
    // get a new memory block of 1 page, insert it into the list
    pg_info_t *new_info = (pg_info_t *)page_addr;
    new_info->next = prev_node->next;
    new_info->prev = prev_node;
    prev_node->next->prev = new_info;
    prev_node->next = new_info;

    // set up its head and tail info
    new_info->pg_num = 1;
    pg_info_t *node_tail = (pg_info_t *)((ptr_t)new_info + PAGE_SIZE - sizeof(pg_info_t));
    node_tail->head = new_info;

    // check the validity of the next node
    if (new_info->next != &page_head &&
        (ptr_t)new_info + PAGE_SIZE == (ptr_t)new_info->next)
    {
        // merge the two nodes
        pg_info_t *new_next = new_info->next;
        new_info->pg_num += new_next->pg_num;
        node_tail = (pg_info_t *)((ptr_t)new_next + new_next->pg_num * PAGE_SIZE - sizeof(pg_info_t));
        node_tail->head = new_info;
        new_next = new_next->next; // NOTE: modify new_next pointer
        new_next->prev = new_info;
        new_info->next = new_next;
    }

    // check the validity of the prev node (change new_info to prev_node)
    if (prev_node->next != &page_head &&
        (ptr_t)prev_node + PAGE_SIZE * prev_node->pg_num ==
            (ptr_t)prev_node->next)
    {
        // merge the two nodes
        pg_info_t *new_next = prev_node->next;
        prev_node->pg_num += new_next->pg_num;
        node_tail = (pg_info_t *)((ptr_t)new_next + (ptr_t)new_next->pg_num * PAGE_SIZE - sizeof(pg_info_t));
        node_tail->head = prev_node;
        new_next = new_next->next; // NOTE: modify new_next pointer
        new_next->prev = prev_node;
        prev_node->next = new_next;
    }

    spin_lock_release(&page_alloc_kern_lk);
    /* leave critical section */

    WRITE_LOG("kernel PTE page freed\n")
}


/* this is used for mapping kernel virtual address into user page table */
void share_pgtable(PTE *dest_pgdir, const PTE *src_pgdir) // with pud locked (maybe)
{
    // TODO [P4-task1] share_pgtable:
    memcpy((void *)dest_pgdir, (void *)src_pgdir, PAGE_SIZE);
}

/*
 * this is used for copy a whole page table
 * we open new page for 2 layers of directories, but not for the last layer of page table 
 */
void fork_copy_pgtable(PTE *dest_pgdir, const PTE *src_pgdir) // with pud locked (maybe)
{
    WRITE_LOG("fork, copy src_pgdir: %lx into dest_pgdir %lx\n", src_pgdir, dest_pgdir)

    clear_pgdir((ptr_t)dest_pgdir);

    // check validity in first-level page directory
    for (uint64_t vpn2 = 0; vpn2 < NUM_PTE_ENTRY; vpn2++)
    {
        // check whether the page is in TBE
        if (get_attribute(src_pgdir[vpn2], _PAGE_VALID)) // valid
        {
            // opne a new page for it
            WRITE_LOG("assign 2nd-level directory for va (vpn2<<30): %lx\n", vpn2 << (PPN_BITS + PPN_BITS + NORMAL_PAGE_SHIFT))
            dest_pgdir[vpn2] = src_pgdir[vpn2]; // copy attributte
            set_pfn(&dest_pgdir[vpn2], kva2pa(page_alloc_kern()) >> NORMAL_PAGE_SHIFT);
            clear_pgdir(get_kva(dest_pgdir[vpn2]));

            PTE *pmd_src = (PTE *)get_kva(src_pgdir[vpn2]); // middle directory: source
            PTE *pmd_des = (PTE *)get_kva(dest_pgdir[vpn2]); // middle directory: destination

            // check validity in second-level page directory
            for (uint64_t vpn1 = 0; vpn1 < NUM_PTE_ENTRY; vpn1++)
            {
                // check whether the page is large page
                uint64_t attri = get_attribute(pmd_src[vpn1], _PAGE_VALID | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC);
                if ((attri != _PAGE_VALID) && (attri & _PAGE_VALID)) // kernel large page
                {
                    WRITE_LOG("copy 3rd-level directory for va (vpn2<<30 | vpn1<<21): %lx\n", ((vpn2 << PPN_BITS) | vpn1) << (PPN_BITS + NORMAL_PAGE_SHIFT))
                    pmd_des[vpn1] = pmd_src[vpn1]; // copy PTE
                }

                // check whether it's a page directory
                else if (attri == _PAGE_VALID)
                {
                    WRITE_LOG("assign 3rd-level directory for va (vpn2<<30 | vpn1<<21): %lx\n", ((vpn2 << PPN_BITS) | vpn1) << (PPN_BITS + NORMAL_PAGE_SHIFT))
                    pmd_des[vpn1] = pmd_src[vpn1]; // copy attributte
                    set_pfn(&pmd_des[vpn1], kva2pa(page_alloc_kern()) >> NORMAL_PAGE_SHIFT);
                    clear_pgdir(get_kva(pmd_des[vpn1]));

                    PTE *pte_src = (PTE *)get_kva(pmd_src[vpn1]); // middle directory: source
                    PTE *pte_des = (PTE *)get_kva(pmd_des[vpn1]); // middle directory: destination
                    for (uint64_t vpn0 = 0; vpn0 < NUM_PTE_ENTRY; vpn0++)
                    {
                        if (get_attribute(pte_src[vpn0], _PAGE_VALID | _PAGE_READ | _PAGE_WRITE | _PAGE_EXEC))
                        {
                            WRITE_LOG("link for va (vpn2<<30 | vpmn<<21 | vpn0 << 12): %lx\n", ((((vpn2 << PPN_BITS) | vpn1) << PPN_BITS) | vpn0) << NORMAL_PAGE_SHIFT)
                            // add forked flag to the source page entry
                            set_attribute(&pte_src[vpn0], _PAGE_FORKED); // add forked flag
                            clear_attribute(&pte_src[vpn0], _PAGE_WRITE); // clear write flag
                            cow_create(get_kva(pte_src[vpn0])); // deref the page

                            // copy the page entry to the destination
                            pte_des[vpn0] = pte_src[vpn0]; // copy PTE
                        }
                    }
                }
                // else // invaild
                // {
                //     pmd_des[vpn1] = 0; // does not exist
                // }    
            }
        }
        // else // invalid
        // {
        //     dest_pgdir[vpn2] = 0; // does not exist
        // }
    }
}

void free_tlb_page(PTE *pgdir) // with pud locked
{
    // TODO [P4-task1] free_tlb_page:
    // get the first-level page directory: upper directory
    for (int vpn2 = 0; vpn2 < NUM_PTE_ENTRY; vpn2++)
    {
        // check whether the page is in TBE
        if (!get_attribute(pgdir[vpn2], _PAGE_VALID))
            continue;
        PTE *pmd = (PTE *)get_kva(pgdir[vpn2]); // middle directory
        for (int vpn1 = 0; vpn1 < NUM_PTE_ENTRY; vpn1++)
        {
            // check whether the page is in TBE. NOTE: do not free kernel pages
            if (get_attribute(pmd[vpn1], _PAGE_VALID | _PAGE_READ |
                                             _PAGE_WRITE | _PAGE_EXEC) !=
                _PAGE_VALID)
                continue;
            PTE *pte = (PTE *)get_kva(pmd[vpn1]); // page table entry
            memset(pte, 0, NUM_PTE_ENTRY * sizeof(PTE));
        }
    }
    local_flush_tlb_all();
    __sync_synchronize();
    local_flush_icache_all();
}

ptr_t shm_page_get(int key)
{
    // TODO [P4-task5] shm_page_get:
    return 0;
}

void shm_page_dt(ptr_t addr)
{
    // TODO [P4-task5] shm_page_dt:
}

