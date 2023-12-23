#include <list.h>

// check if a list is empty
bool list_empty(list_head *head)
{
    return head->next == head;
}

// add a list node to the head of a list
void list_add_head(list_head *head ,list_node_t *node)
{
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

// add a list node to the tail of a list
void list_add_tail(list_head *head, list_node_t *node)
{
    node->prev = head->prev;
    node->next = head;
    head->prev->next = node;
    head->prev = node;
}

// delete a list node from a list
void list_del(list_node_t *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->prev = node->next = NULL;
}

// insert a nodes into a list, at given position
void list_insert(list_node_t *prior, list_node_t *node)
{
    node->next = prior->next;
    node->prev = prior;
    prior->next->prev = node;
    prior->next = node;
}

// insert 2 nodes into a list, at given position
void list_insert_2(list_node_t *prior, list_node_t *node1, list_node_t *node2)
{
    node1->next = node2;
    node1->prev = prior;
    node2->prev = node1;
    node2->next = prior->next;
    prior->next->prev = node2;
    prior->next = node1;
}
