/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                                   Thread Lock
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#ifndef INCLUDE_LOCK_H_
#define INCLUDE_LOCK_H_

#include <lib.h>

#define LOCK_NUM 16 // max number of locks, no more than 64 (for the structrue in pcb)

typedef enum {
    UNLOCKED,
    LOCKED,
} lock_status_t;

typedef struct spin_lock
{
    volatile lock_status_t status; // status of the spinlock. UNLOCKED or LOCKED
} spin_lock_t;

/* gaint lock for OS kernel, for supporting p3-task3 symetric multi-processor */
extern spin_lock_t giant_lock;
extern int giant_lock_owner;

// a lock for the log
extern spin_lock_t log_lock;

typedef struct mutex_lock
{
    int apply_times; // times of applying for the lock
    list_node_t block_queue; // block queue
    int key; // key of the mutex lock
    spin_lock_t lock; // use a spinlock to protect the mutex lock
    pid_t pid; // the pid of the process that holds the lock
    pid_t tid; // the tid of the process that holds the lock
    volatile lock_status_t status; // the mutexlock.
} mutex_lock_t;

void init_locks(void);

void spin_lock_init(spin_lock_t *lock);
int spin_lock_try_acquire(spin_lock_t *lock);
void spin_lock_acquire(spin_lock_t *lock);
void spin_lock_release(spin_lock_t *lock);
void spin_lock_acquire_no_log(spin_lock_t *lock);
void spin_lock_release_no_log(spin_lock_t *lock);

int do_mutex_lock_init(int key);
void do_mutex_lock_acquire(int mlock_idx);
void do_mutex_lock_release(int mlock_idx);
void do_mutex_lock_free(int mlock_idx, pid_t pid, pthread_t tid);

/************************************************************/
typedef struct barrier
{
    // TODO [P3-TASK2 barrier]
    list_node_t block_queue; // block queue
    int goal; // goal of the barrier
    int key; // key of the barrier
    spin_lock_t lock; // use a spinlock to protect the barrier
    pid_t pid; // the pid of the process that holds the barrier
    int value; // remainning number of processes to wait
} barrier_t;

#define BARRIER_NUM 16

extern barrier_t barriers[BARRIER_NUM];

void init_barriers(void);
int do_barrier_init(int key, int goal);
void do_barrier_wait(int bar_idx);
void do_barrier_destroy(int bar_idx);

typedef struct condition
{
    // TODO [P3-TASK2 condition]
} condition_t;

#define CONDITION_NUM 16

void init_conditions(void);
int do_condition_init(int key);
void do_condition_wait(int cond_idx, int mutex_idx);
void do_condition_signal(int cond_idx);
void do_condition_broadcast(int cond_idx);
void do_condition_destroy(int cond_idx);

typedef struct semaphore
{
    // TODO [P3-TASK2 semaphore]
    list_node_t block_queue; // block queue
    int key; // key of the semaphore
    spin_lock_t lock; // use a spinlock to protect the semaphore
    pid_t pid; // the pid of the process that holds the semaphore
    int value; // value of the semaphore
} semaphore_t;

#define SEMAPHORE_NUM 16

extern semaphore_t semaphores[SEMAPHORE_NUM];

void init_semaphores(void);
int do_semaphore_init(int key, int init);
void do_semaphore_up(int sema_idx);
void do_semaphore_down(int sema_idx);
void do_semaphore_destroy(int sema_idx);

#define MAX_MBOX_LENGTH (256)
#define MAX_MBOX_NAME_LENGTH (32)

typedef struct mailbox
{
    // TODO [P3-TASK2 mailbox]
    int apply_times; // times of applying for the mailbox
    list_node_t recv_queue; // block queue of receiving requests 
    list_node_t send_queue; // block queue of sending reqeusts
    char data[MAX_MBOX_LENGTH]; // data in the mailbox
    int begin; // begin position of data in the mailbox
    int end; // end position of data in the mailbox
    spin_lock_t lock; // use a spinlock to protect the mailbox
    char name[MAX_MBOX_NAME_LENGTH]; // name of the mailbox
} mailbox_t;

#define MBOX_NUM 16

extern mailbox_t mailboxes[MBOX_NUM];

void init_mbox();
int do_mbox_open(char *name);
void do_mbox_close(int mbox_idx);
int do_mbox_send(int mbox_idx, void * msg, int msg_length);
int do_mbox_recv(int mbox_idx, void * msg, int msg_length);
void do_mbox_destroy(int mbox_idx);

/************************************************************/

#endif
