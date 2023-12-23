#include <pthread.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>

#define PTHREAD_SIZE 0x100000

typedef struct _pthread_info {
    struct _pthread_info *next;
    ptr_t base;
    pthread_t tid;
    // bool exited;
    // long return_value;
} pthread_info_t;

pthread_info_t pthread_head = {
    .next = NULL,
};

/* TODO:[P4-task4] pthread_create/wait */
void pthread_create(pthread_t *thread, void (*start_routine)(void*), void *arg)
{
    /* TODO: [p4-task4] implement pthread_create */

    // create a thread info struct
    pthread_info_t *new_thread = my_malloc(sizeof(pthread_info_t));

    // malloc a stack space for the thread
    new_thread->base = (ptr_t)my_malloc(PTHREAD_SIZE);

    // insert the thread info struct into the linked list
    new_thread->next = pthread_head.next;
    pthread_head.next = new_thread;

    // set up the stack
    ptr_t mem_top = new_thread->base + PTHREAD_SIZE; // system call will automatically alignate stack point

    // set the thread as unexited
    // new_thread->exited = false;

    // set up the stack frame
    new_thread->tid = sys_thread_create(start_routine, mem_top, (ptr_t)pthread_exit, arg);

    if (new_thread->tid != 0)
    {
        *thread = new_thread->tid;
    }
    else
    {
        printf("pthread_create failed\n");
        assert(0)
    }
}

int pthread_join(pthread_t thread)
{
    /* TODO: [p4-task4] implement pthread_join */
    
    if (thread == 0)
        assert(0);

    // find whther the thread exists
    pthread_info_t *prev = &pthread_head;
    while (prev->next != NULL && prev->next->tid != thread) {
        prev = prev->next;
    }
    if (prev->next == NULL)
        assert(0);
    // assert(0)
    // pthread_info_t *join_thread = prev->next;
    // assert(0)
    // check whether the task is finished
    // while (join_thread->exited == false)
    //     ;

    // stop the task
    if (sys_thread_join(thread) != 0)
        assert(0);

    // free the stack space
    my_free((void *)prev->next->base);

    // remove the thread info struct from the linked list
    pthread_info_t *cur = prev->next;
    prev->next = cur->next;
    my_free(cur);
    
    return 0;
}

void pthread_exit(void)
{
    sys_thread_exit();
}
