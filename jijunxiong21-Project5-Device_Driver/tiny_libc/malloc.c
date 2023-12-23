#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct _mem_info {
    struct _mem_info *next;
    uint64_t size;
    bool used;
} mem_info_t;

const static ptr_t HEAP_BASE = 0x10000000;
static ptr_t HEAP_TOP = 0x30000000;

#define HEAP_SIZE (HEAP_TOP - HEAP_BASE - sizeof(mem_info_t))

mem_info_t mem_head = {
    .next = NULL,
};

void *my_malloc(size_t size)
{
    // TODO [P4-task1] (design you 'kmalloc' here if you need):

    if (mem_head.next == NULL)
    {
        // initialize the list
        mem_head.next = (mem_info_t *)HEAP_BASE;
        mem_head.next->next = &mem_head;
        mem_head.next->size = HEAP_SIZE;
        mem_head.next->used = false;
    }

    // load the first node of the list
    mem_info_t *mem_info = mem_head.next;

    /* find the first node with enough space
     * if reach the end (equal to head node), leave the loop
     * if the node is used, or the size is not enough, continue
     */
    while (mem_info != &mem_head && (mem_info->used || mem_info->size < size))
        mem_info = mem_info->next;
    if (mem_info == &mem_head) // if no node is found
        return NULL;

    // if the node is large enough, split it into two nodes
    if (mem_info->size > size + sizeof(mem_info_t))
    {
        // create a new node by address calculation
        mem_info_t *new_mdata = (mem_info_t *)((ptr_t)mem_info + sizeof(mem_info_t) + size);

        // insert the new node into the list
        new_mdata->next = mem_info->next;
        mem_info->next = new_mdata;

        // change the size of the two nodes
        new_mdata->size = mem_info->size - size - sizeof(mem_info_t);
        new_mdata->used = false;
        mem_info->size = size;
    }

    // if the node is not so large, just use it. so together we set a used flag
    mem_info->used = true;

    // return the address of the memory block
    return (void *)((ptr_t)mem_info + sizeof(mem_info_t));
}

void my_free(void *ptr)
{
    // TODO [P4-task1] (design you 'kfree' here if you need):

    if (mem_head.next == NULL)
    {
        // initialize the list
        mem_head.next = (mem_info_t *)HEAP_BASE;
        mem_head.next->next = &mem_head;
        mem_head.next->size = HEAP_SIZE;
        mem_head.next->used = false;
    }

    // get the node of the memory block
    mem_info_t *free_node = (mem_info_t *)((ptr_t)ptr - sizeof(mem_info_t));

    // check the validity of the node (please keep the prev node of the node to be freed)
    mem_info_t *prev_node = &mem_head;
    while (prev_node->next != &mem_head && prev_node->next != free_node)
        prev_node = prev_node->next;
    if (prev_node->next == &mem_head)
        return;

    // check the validity of the next node
    if (free_node->used == true)
        return;

    // free the memory block
    free_node->used = false;

    // check the validity of the next node
    if (free_node->next != &mem_head && free_node->next->used == false)
    {
        // merge the two nodes
        free_node->size += free_node->next->size + sizeof(mem_info_t);
        free_node->next = free_node->next->next;
    }

    // check the validity of the prev node (prev_node should not be the head node)
    if (prev_node != &mem_head && prev_node->used == false)
    {
        // merge the two nodes
        prev_node->size += free_node->size + sizeof(mem_info_t);
        prev_node->next = free_node->next;
    }
}
