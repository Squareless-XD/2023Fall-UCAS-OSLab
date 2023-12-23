#ifndef INCLUDE_THREAD_H_
#define INCLUDE_THREAD_H_

#include <lib.h>

int do_thread_create(void (*start_routine)(void*), ptr_t mem_top, ptr_t ra, void* arg);
void do_thread_yield(void);
int do_thread_join(pthread_t thread);
void do_thread_exit(void);

#endif