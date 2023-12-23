#include <os/mm.h>
// #include <os/lock.h>
// #include <os/sched.h>

/* ------------------------------------
    umalloc() and ufree()
    functions that provide or reclaim memory pages for user programs
    spaces for allocation: 0xffff_ffc0_5200_0000 - 0xffff_ffc0_5f00_0000
    use buddy system to manage the memory
    store the linked lists of buddy system in the kernel space
    use kmalloc() and kfree() to manage the linked lists
    and use page_alloc_kern() and page_free_kern() to manage the physical pages
    pages that is allocated by umalloc() is mapped into user space, and recorded in PCB
   ------------------------------------ */

zone_t zone;
bs_pg_info_t bs_alloc_head = {
    .list = {
        .prev = &bs_alloc_head.list,
        .next = &bs_alloc_head.list,
    },
    .kva = 0,
    .ref_num = 0,
};

spin_lock_t alloc_page_buddy_lk = {UNLOCKED};

bs_pg_info_t *bs_next(bs_pg_info_t *node)
{
    return (bs_pg_info_t *)node->list.next;
}

bs_pg_info_t *bs_prev(bs_pg_info_t *node)
{
    return (bs_pg_info_t *)node->list.prev;
}

void bs_delete(bs_pg_info_t *node)
{
    list_del(&node->list);
    kfree(node);
}

bs_pg_info_t *bs_add_after(bs_pg_info_t *node, ptr_t kva)
{
    // now node is the node after which should be inserted
    bs_pg_info_t *new_info = kmalloc(sizeof(bs_pg_info_t));
    new_info->kva = kva;
    new_info->ref_num = 1;
    list_insert(&node->list, &new_info->list);

    return new_info;
}

// add a new node with corresponding kva. assert if it already exists
bs_pg_info_t *bs_add_new(ptr_t kva)
{
    bs_pg_info_t *node = bs_next(&bs_alloc_head);

    while (node != &bs_alloc_head && node->kva < kva)
        node = bs_next(node);
    assert(node == &bs_alloc_head || node->kva != kva)

    // now that add a new node before "node"
    bs_pg_info_t *new_info = bs_add_after(bs_prev(node), kva);

    return new_info;
}

// if no node in the linked list is found, return NULL
bs_pg_info_t *bs_find(ptr_t kva)
{
    bs_pg_info_t *node = bs_next(&bs_alloc_head);

    while (node != &bs_alloc_head && node->kva < kva)
        node = bs_next(node);
    if (node == &bs_alloc_head || node->kva != kva)
        return NULL; // note found
    return node; // found
}

// reference a physical page. if already exists, ref_num++; else, allocate a new node
void bs_ref_ppage(ptr_t kva)
{
    bs_pg_info_t *node = bs_find(kva);
    if (node != NULL) // if already exists
        node->ref_num++;
    else // if not exists, we can help
        bs_add_new(kva);
}

// when a process wants to write something into this page, ref_num-- or remove it
int bs_deref_ppage(ptr_t kva)
{
    bs_pg_info_t *node = bs_find(kva);
    assert(node != NULL) // it must exist
    node->ref_num--;
    if (node->ref_num == 0) {
        bs_delete(node);
        return 0;
    }
    return 1;
}

// when a process wants to write something into this page, ref_num-- or remove it
int bs_try_deref_ppage(ptr_t kva)
{
    bs_pg_info_t *node = bs_find(kva);
    assert(node != NULL) // it must exist
    if (node->ref_num <= 1) {
        // bs_delete(node);
        return 0;
    } else {
        node->ref_num--;
        return 1;
    }
}

void zone_init(void)
{
    // initialize the zone
    for (int i = 0; i < MAX_ORDER; i++)
    {
        zone.free_area[i].nr_free = 0;
        zone.free_area[i].free_list.prev = zone.free_area[i].free_list.next = &zone.free_area[i].free_list;
    }
}


// the address of the page directory of kernel (2MB large page mapping)
PTE *kern_pgdir = (PTE *)(PGDIR_PA + KERNEL_ADDR_BASE);

// delete a node from buddy system
void bs_deletet(int order, list_node_t *node)
{
    list_del(node);
    zone.free_area[order].nr_free--;
}

// insert a node into buddy system, sorted by address (from low to high)
void bs_insert(int order, list_node_t *node)
{
    list_node_t *head = &zone.free_area[order].free_list;
    list_node_t *prior = head;

    // find the prior node to insert after
    while (prior->next != head && prior->next < node) // not reached the end, and not reached the address
        prior = prior->next;

    // check legality
    assert(prior->next != node)

    // insert the node
    list_insert(prior, node);
    zone.free_area[order].nr_free++;
}

// recursively split a large memory block into two smaller blocks, and try split into order/2 blocks
void split_block(int page_num, int order, list_node_t *node)
{
    // if the node is too large, recursively split it into several nodes
    ptr_t node_pg_num = 1 << order;
    assert(page_num <= node_pg_num)
    assert(page_num > 0)
    int order_low = order - 1;
    ptr_t half_page_num = 1 << order_low;

    // split the node into two nodes
    list_node_t *new_node = (list_node_t *)((ptr_t)node + (ptr_t)half_page_num * PAGE_SIZE);
    bs_deletet(order, node);
    if (page_num == node_pg_num) // exactly the same size
        return;

    // some pages left, recursively split it
    bs_insert(order_low, new_node);
    if (page_num <= half_page_num)
    {
        bs_insert(order_low, node);
        split_block(page_num, order_low, node);
    }
    else
        split_block(page_num - half_page_num, order_low, new_node);
}

// automatically add a new node into the buddy system
void merge_into_block(int order, list_node_t *node)
{
    // if the node is too large, recursively split it into several nodes
    int order_high = order + 1;
    ptr_t double_page_num = 1 << order_high;

    assert(node != NULL)

    bs_insert(order, node);
    assert(order < MAX_ORDER)

    // check whether the node can be merged with its buddy
    if (((ptr_t)(&node->prev) ^ (ptr_t)(&node)) == (ptr_t)double_page_num * PAGE_SIZE)
    {
        // merge the two nodes
        list_node_t *buddy = node->prev;
        bs_deletet(order, buddy);
        bs_deletet(order, node);
        merge_into_block(order_high, buddy);
    }
    else if (((ptr_t)(&node->next) ^ (ptr_t)(&node)) == (ptr_t)double_page_num * PAGE_SIZE)
    {
        // merge the two nodes
        list_node_t *buddy = node->next;
        bs_deletet(order, node);
        bs_deletet(order, buddy);
        merge_into_block(order_high, node);
    }
}

ptr_t alloc_page_proc(user_page_info_t *mem_info, int page_num, ptr_t va, PTE *pte_addr) // with pud locked
{
    // check whether the process could own more pages
    int mem_idx;
    for (mem_idx = 0; mem_idx < MAX_MEM_SECTION; mem_idx++)
        if (mem_info[mem_idx].valid == false)
            break;
    assert(mem_idx < MAX_MEM_SECTION)
    
    // allocate a page
    ptr_t kva = alloc_page(page_num);

    // set the size of this base address
    mem_info[mem_idx].page_num = page_num;
    mem_info[mem_idx].pte_addr = pte_addr;
    mem_info[mem_idx].valid = true;
    mem_info[mem_idx].va = va & ~(PAGE_SIZE - 1);

    return kva;
}

// allocate the space > 0xffff_ffc0_5200_0000
ptr_t alloc_page(int page_num) // with pud locked
{
    WRITE_LOG("allocate user page for number %d\n", page_num)

    /* enter critical section */
    spin_lock_acquire(&alloc_page_buddy_lk);

    // find the first node with enough space
    // get the order of the memory block
    int order = 0;
    while ((1 << order) < page_num) // space not enough
        order++;
    while (order < MAX_ORDER && zone.free_area[order].nr_free == 0) // no free space at this order
        order++;
    if (order == MAX_ORDER) { // no free space for the whole zone
        spin_lock_release(&alloc_page_buddy_lk);
        /* leave critical section */
        return (ptr_t)NULL;
    }
    // get the first node of the list
    list_node_t *node = zone.free_area[order].free_list.next;

    // if the node is too large, recursively split it into several nodes
    split_block(page_num, order, node);


    /* now we begin to deal with the bs_pg_info_t
     * ref one node to this linked list for any single page
     */

    ptr_t kva = (ptr_t)node;
    assert((kva & (PAGE_SIZE - 1)) == 0)
    ptr_t kva_end = kva + page_num * PAGE_SIZE;
    while (kva < kva_end)
    {
        bs_ref_ppage(kva);
        kva += PAGE_SIZE;
    }

    spin_lock_release(&alloc_page_buddy_lk);
    /* leave critical section */

    WRITE_LOG("allocate user page at %lx for number %d\n", (ptr_t)node, page_num)

    return (ptr_t)node;
}

// NOTE: Only need for S-core to alloc 2MB large page
#ifdef S_CORE
static ptr_t largePageMemCurr = LARGE_PAGE_FREEMEM;
ptr_t allocLargePage(int page_num)
{
    // align LARGE_PAGE_SIZE
    ptr_t ret = ROUND(largePageMemCurr, LARGE_PAGE_SIZE);
    largePageMemCurr = ret + page_num * LARGE_PAGE_SIZE;
    return ret;    
}
#endif

void free_page_proc(ptr_t pte_addr, user_page_info_t *mem_info) // with pud locked
{
    // TODO [P4-task1] (design you 'free_page' here if you need):

    // get the number of pages of this base address
    int mem_idx, page_num;

    for (mem_idx = 0; mem_idx < MAX_MEM_SECTION; mem_idx++)
        if (mem_info[mem_idx].valid == true &&
            (ptr_t)mem_info[mem_idx].pte_addr == pte_addr)
            break;
    if (mem_idx == MAX_MEM_SECTION) // no memory period is found
        assert(0)

    // get the size of this base address
    page_num = mem_info[mem_idx].page_num;

    /* Reclaim those pages back to buddy system, and merge them
     * from low order to higher.
     * Put all the pages into the list of the lowest order, then
     * recursively merge them into higher order
     */

    free_page(pte_addr, page_num);

    mem_info[mem_idx].valid = false;
}

void free_page(ptr_t pte_addr, int page_num) // with pud locked
{
    // TODO [P4-task1] (design you 'free_page' here if you need):

    /* enter critical section */
    spin_lock_acquire(&alloc_page_buddy_lk);

    assert(pte_addr != 0)
    if (page_num == 0) {
        spin_lock_release(&alloc_page_buddy_lk);
        /* leave critical section */
        return;
    }
    /* Reclaim those pages back to buddy system, and merge them
     * from low order to higher.
     * Put all the pages into the list of the lowest order, then
     * recursively merge them into higher order
     */
    // int order = 0;
    // ptr_t step_offset = PAGE_SIZE;
    ptr_t base_addr = get_kva(*(PTE *)pte_addr); // base of allocated pages
    // ptr_t top_addr = base_addr + (ptr_t)page_num * PAGE_SIZE; // top of allocated pages

    // judge the lowest bit of the address
    // while ((base_addr & step_offset) == 0 && (top_addr & step_offset) == 0 && order < MAX_ORDER)
    // {
    //     order++;
    //     step_offset <<= 1;
    // }

    // if (order == MAX_ORDER) // no address matched
    //     assert(0)

    // insert them into the free list with corresponding order
    PTE *pte = (PTE *)pte_addr;
    ptr_t reclaim_addr;
    for (int i = 0; i < page_num; i++)
    {
        if (!get_attribute(*pte, _PAGE_VALID))
            swap_from_sd((ptr_t)pte, reclaim_addr);
        reclaim_addr = get_kva(*pte);
        list_node_t *node = (list_node_t *)reclaim_addr;
        merge_into_block(0, node);
        pte_addr += 1;
    }
    // ptr_t reclaim_addr = base_addr;
    // while (reclaim_addr < top_addr)
    // {
    //     list_node_t *node = (list_node_t *)reclaim_addr;
    //     merge_into_block(order, node);
    //     reclaim_addr += step_offset;
    // }


    /* now we begin to deal with the bs_pg_info_t
     * deref one node to this linked list for any single page
     */

    ptr_t kva = base_addr;
    assert((kva & (PAGE_SIZE - 1)) == 0)
    ptr_t kva_end = kva + page_num * PAGE_SIZE;
    while (kva < kva_end)
    {
        bs_deref_ppage(kva);
        kva += PAGE_SIZE;
    }

    spin_lock_release(&alloc_page_buddy_lk);
    /* leave critical section */

    WRITE_LOG("free user page at %lx for number %d\n", base_addr, page_num)
}

void cow_create(ptr_t kva)
{
    spin_lock_acquire(&alloc_page_buddy_lk);
    bs_ref_ppage(kva);
    spin_lock_release(&alloc_page_buddy_lk);
}

int cow_try_eliminate(ptr_t kva)
{
    spin_lock_acquire(&alloc_page_buddy_lk);
    int ret = bs_try_deref_ppage(kva);
    spin_lock_release(&alloc_page_buddy_lk);
    return ret;
}
