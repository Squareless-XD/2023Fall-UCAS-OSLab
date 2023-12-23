#include <os/list.h>

// check if a PCB list is empty
bool list_empty(list_head *head)
{
    return head->next == head;
}

// add a PCB to the head of a list
void list_add_head(list_head *head ,list_node_t *node)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

// add a PCB to the tail of a list
void list_add_tail(list_head *head, list_node_t *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

// delete a PCB from a list
void list_del(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
}
