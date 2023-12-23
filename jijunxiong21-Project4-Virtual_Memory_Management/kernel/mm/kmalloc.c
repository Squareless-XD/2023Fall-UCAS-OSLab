#include <os/mm.h>
// #include <os/lock.h>
// #include <os/sched.h>

/* ------------------------------------
    kmalloc() and kfree()
    functions that provide or reclaim continous memory blocks in kernel space
    spaces for allocation: 0xffff_ffc0_51C0_0000 - 0xffff_ffc0_5200_0000
    every memory block has a meta-data structure kmmdata_t of 16 bytes
    it is a linked list, with the head node is a global variable km_head
   ------------------------------------ */

km_mdata_t km_head;

spin_lock_t kmm_lk = {UNLOCKED};

void kmalloc_init(void)
{
    // initialize kmmdata_t at KERN_MEM_BASE
    km_mdata_t *km_mdata = (km_mdata_t *)KERN_MEM_BASE;
    km_mdata->next = &km_head;
    km_mdata->size = KERN_MEM_SIZE;
    km_mdata->used = false;

    // initialize km_head
    km_head.next = km_mdata;
}

void *kmalloc(size_t size)
{
    /* enter critical section */
    spin_lock_acquire(&kmm_lk);

    // TODO [P4-task1] (design you 'kmalloc' here if you need):
    WRITE_LOG("request size = %lx\n", size)
    // load the first node of the list
    km_mdata_t *km_mdata = km_head.next;

    /* find the first node with enough space
     * if reach the end (equal to head node), leave the loop
     * if the node is used, or the size is not enough, continue
     */
    while (km_mdata != &km_head && (km_mdata->used || km_mdata->size < size))
        km_mdata = km_mdata->next;
    if (km_mdata == &km_head) // if no node is found
    {
        spin_lock_release(&kmm_lk);
        return NULL;
    }

    // if the node is large enough, split it into two nodes
    if (km_mdata->size > size + sizeof(km_mdata_t))
    {
        // create a new node by address calculation
        km_mdata_t *new_mdata = (km_mdata_t *)((ptr_t)km_mdata + sizeof(km_mdata_t) + size);

        // insert the new node into the list
        new_mdata->next = km_mdata->next;
        km_mdata->next = new_mdata;

        // change the size of the two nodes
        new_mdata->size = km_mdata->size - size - sizeof(km_mdata_t);
        new_mdata->used = false;
        km_mdata->size = size;
    }

    // if the node is not so large, just use it. so together we set a used flag
    km_mdata->used = true;

    WRITE_LOG("assigned memery address = %lx\n", (void *)((ptr_t)km_mdata + sizeof(km_mdata_t)))

    spin_lock_release(&kmm_lk);
    /* leave critical section */

    // return the address of the memory block
    return (void *)((ptr_t)km_mdata + sizeof(km_mdata_t));
}

void kfree(void *ptr)
{
    /* enter critical section */
    spin_lock_acquire(&kmm_lk);

    // TODO [P4-task1] (design you 'kfree' here if you need):
    WRITE_LOG("address = %lx\n", ptr)
    // get the node of the memory block
    km_mdata_t *free_node = (km_mdata_t *)((ptr_t)ptr - sizeof(km_mdata_t));

    // check the validity of the node (please keep the prev node of the node to be freed)
    km_mdata_t *prev_node = &km_head;
    while (prev_node->next != &km_head && prev_node->next != free_node)
        prev_node = prev_node->next;
    if (prev_node->next == &km_head)
    {
        spin_lock_release(&kmm_lk); /* leave critical section */
        return;
    }

    // check the validity of the next node
    if (free_node->used == true)
    {
        spin_lock_release(&kmm_lk); /* leave critical section */
        return;
    }

    // free the memory block
    free_node->used = false;

    // check the validity of the next node
    if (free_node->next != &km_head && free_node->next->used == false)
    {
        // merge the two nodes
        free_node->size += free_node->next->size + sizeof(km_mdata_t);
        free_node->next = free_node->next->next;
    }

    // check the validity of the prev node (prev_node should not be the head node)
    if (prev_node != &km_head && prev_node->used == false)
    {
        // merge the two nodes
        prev_node->size += free_node->size + sizeof(km_mdata_t);
        prev_node->next = free_node->next;
    }

    WRITE_LOG("memery successfully freeed\n")

    spin_lock_release(&kmm_lk);
    /* leave critical section */
}
