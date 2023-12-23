#include <os/mm.h>
#include <os/kernel.h>
#include <os/task_tools.h>

/* ------------------------------------
    swap_to_sd() and swap_from_sd()
    functions that provide for swapping page frames to and from SD card
    user should provide a page table entry
   ------------------------------------ */

static pg_info_sd_t page_head = {
    {
        (list_node_t *)&page_head,
        (list_node_t *)&page_head,
    },
    0,
};
static pg_info_sd_t *page_ptr;

spin_lock_t alloc_page_sd_lk = {UNLOCKED};

pg_info_sd_t *pg_next(pg_info_sd_t *node)
{
    return (pg_info_sd_t *)node->list.next;
}

pg_info_sd_t *pg_add_head(pg_info_sd_t *node)
{
    list_add_head(&page_head.list, &node->list);
    return node;
}

pg_info_sd_t *pg_insert_after(pg_info_sd_t *node, uint32_t page_addr)
{
    pg_info_sd_t *new_info = kmalloc(sizeof(pg_info_sd_t));
    new_info->sd_addr = page_addr;
    new_info->pg_num = 1;
    list_insert(&node->list, &new_info->list);
    return new_info;
}

void pg_merge_with_next(pg_info_sd_t *node)
{
    pg_info_sd_t *next_node = pg_next(node);
    node->pg_num += next_node->pg_num;
    list_del(&next_node->list);
    kfree(next_node);
}

// initialize the page allocator of SD card
void page_alloc_sd_init(void)
{
    // initialize the head node
    pg_info_sd_t *free_node = kmalloc(sizeof(pg_info_sd_t));
    pg_add_head(free_node);

    // initialize the first node
    free_node->sd_addr = sd_swap_base;
    free_node->pg_num = SD_MAX_PG_NUM;

    // initialize the page pointer
    page_ptr = free_node;
}

// allocate 1 page
uint32_t page_alloc_sd(void)
{
    /* enter critical section */
    spin_lock_acquire(&alloc_page_sd_lk);

    // load the first node of the list
    pg_info_sd_t *node = page_ptr;

    // if reach the end (equal to head node), leave the loop
    if (node == &page_head)
    {
        if (node->list.next == &page_head.list) // if no node is found
            assert(0)
        node = pg_next(node);
    }
    uint32_t sd_base_get = node->sd_addr; // please make sure it is aligned

    // if the sd node is large than 1 page, move the sd_addr and change the size
    if (node->pg_num > 1)
    {
        // change the number of pages
        node->pg_num -= 1;
        node->sd_addr += PAGE_SIZE;
        page_ptr = node;
    }
    // if the node is not so large, just use it.
    else
    {
        page_ptr = pg_next(node);
        list_del(&node->list);
        kfree(node); // this node has been used out, free it
    }

    spin_lock_release(&alloc_page_sd_lk);
    /* leave critical section */

    // return the address of the SD page frame
    return sd_base_get;
}


// reclaim a page
void page_free_sd(uint32_t page_addr)
{
    /* enter critical section */
    spin_lock_acquire(&alloc_page_sd_lk);

    page_addr &= ~(PAGE_SIZE - 1); // alignize the address

    // get the node of the SD page frame
    pg_info_sd_t *node = &page_head;
    while (pg_next(node) != &page_head && pg_next(node)->sd_addr < page_addr)
        node = pg_next(node);
    // if the node is not found, return
    if (pg_next(node) != &page_head && pg_next(node)->sd_addr == page_addr)
        assert(0)
    if (node != &page_head && node->sd_addr + node->pg_num * PAGE_SIZE > page_addr)
        assert(0)

    // get a new SD pagem and insert the new node into the list
    pg_info_sd_t *new_info = pg_insert_after(node, page_addr);

    // check the validity of the next node
    pg_info_sd_t *next_node = pg_next(new_info);
    if (next_node != &page_head &&
        next_node->sd_addr == page_addr + PAGE_SIZE)
    {
        // merge the two nodes
        pg_merge_with_next(new_info);
    }

    // check the validity of the prev node (change new_info to prev_node)
    if (node != &page_head &&
        node->sd_addr + PAGE_SIZE * node->pg_num == page_addr)
    {
        // merge the two nodes
        pg_merge_with_next(node);
    }

    spin_lock_release(&alloc_page_sd_lk);
    /* leave critical section */
}


// use 512MB space in SD card as swap space
// base address is "long sd_swap_base" in task_tools.c
// 512MB = 512 * 1024 * 1024 = 536870912 = 0x20000000

// entry: page table entry, pointing to the page table entry of the page
// va: virtual address of the page
int swap_to_sd(ptr_t entry, ptr_t va) // enter with corresponding pud locked
{
    // 0. check situations
    // 1. get the page frame address
    // 2. find a space in SD card
    // 3. write the page frame to SD card
    // 4. update the page table entry
    // 5. flush the cache

    // 0. check situations
    if (entry == 0) // kernel stack page
        return -1;
    if (!get_attribute(*(PTE *)entry, _PAGE_VALID)) // page is already in SD card
        return -1;
    if (get_attribute(*(PTE *)entry, _PAGE_PIN)) // page is pinned
        return -1;

    // 1. get the page frame address
    ptr_t pa = get_pa(*(PTE *)entry);
    assert(pa != 0)

    // 2. find a space in SD card
    uint32_t sd_addr = page_alloc_sd();

    // 3. write the page frame to SD card
    bios_sd_write(pa, SEC_PER_PAGE, NBYTES2SEC(sd_addr));
    free_page(entry, 1); // free the physical page frame after writing

    // 4. update the page table entry
    clear_attribute((PTE *)entry, _PAGE_VALID);
    set_pfn((PTE *)entry, sd_addr >> NORMAL_PAGE_SHIFT);

    // 5. flush TLB
    __sync_synchronize();
    local_flush_icache_all();
    local_flush_tlb_page(va);

    WRITE_LOG("swap from pa: %lx, to sd_addr: %lx\n", pa, sd_addr)

    return 0;
}

int swap_from_sd(ptr_t entry, ptr_t va) // enter with corresponding pud locked
{
    // 0. check situations
    // 1. allocate a page frame address
    // 2. find the corresponding SD address
    // 3. read the page frame from SD card
    // 4. update the page table entry
    // 5. flush the cache

    // 0. check situations
    if (entry == 0) // kernel stack page
        return -1;
    if (get_attribute(*(PTE *)entry, _PAGE_VALID)) // page is already in memory
        return -1;
    // if (get_attribute(*(PTE *)entry, _PAGE_PIN)) // page is pinned
    //     return -1;

    // 1. allocate a page frame address
    ptr_t pa = kva2pa(alloc_page(1));

    // 2. find the corresponding SD address
    uint32_t sd_addr = get_pa(*(PTE *)entry);

    // 3. read the page frame from SD card
    bios_sd_read(pa, SEC_PER_PAGE, NBYTES2SEC(sd_addr));

    // 4. update the page table entry
    set_attribute((PTE *)entry, _PAGE_VALID | _PAGE_READ | _PAGE_WRITE |
                                    _PAGE_EXEC | _PAGE_USER | _PAGE_ACCESSED |
                                    _PAGE_DIRTY);

    // 5. flush TLB
    __sync_synchronize();
    local_flush_icache_all();
    local_flush_tlb_page(va);

    WRITE_LOG("swap to pa: %lx, from sd_addr: %lx\n", pa, sd_addr)

    return 0;
}

#define PROC_MAX_PAGE_NUM 16

// get the number of pages that the pcb owns and in memory now (not in SD card)
int get_pcb_page_num(user_page_info_t *mem_info) // enter with corresponding pud locked
{
    int page_num = 0;
    for (int mem_idx = 0; mem_idx < MAX_MEM_SECTION; mem_idx++) {
        if (mem_info[mem_idx].valid == false || mem_info[mem_idx].pte_addr == 0)
            continue;
        int page_num_tmp = 0;

        // check whether the page is in SD card
        for (int j = 0;  j < mem_info[mem_idx].page_num; j++)
            if (get_attribute(*(PTE *)((ptr_t)mem_info[mem_idx].pte_addr + j * PAGE_SIZE), _PAGE_VALID) == false)
                page_num_tmp++;

        // add the number of pages to the total number
        page_num += page_num_tmp;
    }
    return page_num;
}

static long holdrand = 1L;

int rand() {
    return (((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff);
}

bool check_page_full(pcb_t *pcb_check) // enter with corresponding pud locked
{
    user_page_info_t *mem_info = pcb_check->mem_info;
    // check whether the pcb owns too many page. if so, swap 1 page to SD card
    int has_page_num = get_pcb_page_num(mem_info);
    if (has_page_num <= PROC_MAX_PAGE_NUM)
        return false;
    WRITE_LOG("The page is full\n")
    // find a page table entry that is not used
    int pte_idx;
    // for (pte_idx = 0; pte_idx < MAX_MEM_SECTION; pte_idx++)
    //     if (mem_info[pte_idx].valid == 0)
    //         break;
    // if (pte_idx == MAX_MEM_NUM)
    //     assert(0)

    PTE *pte;
    ptr_t va;
    int page_num;
    int count = has_page_num - PROC_MAX_PAGE_NUM;
    while (count > 0) { // if find a page is swapped to SD card (count < page_num), quit the loop
        // wait until a true was found
        while (mem_info[pte_idx = rand() % MAX_MEM_SECTION].valid == false) // NOTE: temporary parameter
            ;
        // swap the page to SD card
        pte = mem_info[pte_idx].pte_addr;
        va = mem_info[pte_idx].va;
        page_num = mem_info[pte_idx].page_num;
        // pick one page that is in memory to swap
        for (int i = 0; i < page_num; i++)
        {
            if (swap_to_sd((ptr_t)pte, va) != 0)
                count--;
            pte += 1;
            va += PAGE_SIZE;
        }
        // while (swap_to_sd((ptr_t)pte, va) != 0 && ++count < page_num)
        // {
        //     pte += 1;
        //     va += PAGE_SIZE;
        // }
    }
    return true;
}
