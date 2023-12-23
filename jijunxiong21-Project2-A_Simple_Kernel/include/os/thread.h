#ifndef INCLUDE_THREAD_H_
#define INCLUDE_THREAD_H_

int thread_create(void func_p(), int args_num, ...);
void thread_yield(void);

#endif