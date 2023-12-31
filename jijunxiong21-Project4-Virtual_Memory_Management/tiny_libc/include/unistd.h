#ifndef __UNISTD_H__
#define __UNISTD_H__

#include <stdint.h>
typedef int32_t pid_t;
typedef pid_t pthread_t;

void sys_sleep(uint32_t time);
void sys_yield(void);
void sys_write(char *buff);
void sys_move_cursor(int x, int y);
void sys_reflush(void);
void sys_clear(void);
long sys_get_timebase(void);
long sys_get_tick(void);
int sys_mutex_init(int key);
void sys_mutex_acquire(int mutex_idx);
void sys_mutex_release(int mutex_idx);
int sys_thread_create(void (*start_routine)(void*), ptr_t mem_top, ptr_t ra, void* arg);
void sys_thread_yield(void);
int sys_thread_join(pthread_t thread);
void sys_thread_exit(void);

/************************************************************/
/* TODO: [P3 task1] ps, getchar */
void sys_ps(void);
int  sys_getchar(void);

/* TODO: [P3 task1] exec, exit, kill waitpid */
#ifdef S_CORE
// S-core
pid_t  sys_exec(int id, int argc, uint64_t arg0, uint64_t arg1, uint64_t arg2);
#else
// A/C-core
pid_t  sys_exec(char *name, int argc, char **argv);
#endif

void sys_exit(void);
int  sys_kill(pid_t pid);
int  sys_waitpid(pid_t pid);
pid_t sys_getpid();


/* TODO: [P3 task2] barrier */ 
int  sys_barrier_init(int key, int goal);
void sys_barrier_wait(int bar_idx);
void sys_barrier_destroy(int bar_idx);

/* TODO: [P3 task2] condition */ 
int sys_condition_init(int key);
void sys_condition_wait(int cond_idx, int mutex_idx);
void sys_condition_signal(int cond_idx);
void sys_condition_broadcast(int cond_idx);
void sys_condition_destroy(int cond_idx);

/* TODO: [P3 task2] semaphore */ 
int sys_semaphore_init(int key, int init);
void sys_semaphore_up(int sema_idx);
void sys_semaphore_down(int sema_idx);
void sys_semaphore_destroy(int sema_idx);

/* TODO: [P3 task2] mailbox */ 
int sys_mbox_open(char * name);
void sys_mbox_close(int mbox_id);
int sys_mbox_send(int mbox_idx, void *msg, int msg_length);
int sys_mbox_recv(int mbox_idx, void *msg, int msg_length);

/* TODO: [P4-task5] shmpageget/dt */
/* shmpageget/dt */
void* sys_shmpageget(int key);
void sys_shmpagedt(void *addr);
/************************************************************/

// personnally defined
void sys_shell_moveup(char *shell_buf_ptr[]);
int sys_get_cpuid(void);
int sys_taskset(int mask, pid_t pid, char *task_name, int argc, char **argv);
pid_t sys_fork(void);

#endif
