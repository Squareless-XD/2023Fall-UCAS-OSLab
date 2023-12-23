// #include <os/lock.h> // ignored
#include <os/sched.h>

mutex_lock_t mlocks[LOCK_NUM];

void init_locks(void)
{
    /* TODO: [p2-task2] initialize mlocks */
    for (int i = 0; i < LOCK_NUM; i++)
    {
        mlocks[i].apply_times = 0;
        mlocks[i].block_queue.prev = mlocks[i].block_queue.next = &(mlocks[i].block_queue);
        mlocks[i].key = 0;
        mlocks[i].lock.status = UNLOCKED;
        mlocks[i].pid = 0;
        mlocks[i].status = UNLOCKED;
    }
}

int do_mutex_lock_init(int key)
{
    /* TODO: [p2-task2] initialize mutex lock */
    int mlock_idx;

    for (mlock_idx = 0; mlock_idx < LOCK_NUM; mlock_idx++)
    {
        spin_lock_acquire(&(mlocks[mlock_idx].lock)); /* into critical section */
        if (mlocks[mlock_idx].apply_times != 0 && mlocks[mlock_idx].key == key)
            break;
        else
            spin_lock_release(&(mlocks[mlock_idx].lock)); /* out of critical section */
    }
    if (mlock_idx == LOCK_NUM) // if no matched key is found
    {
        for (mlock_idx = 0; mlock_idx < LOCK_NUM; mlock_idx++)
        {
            spin_lock_acquire(&(mlocks[mlock_idx].lock)); /* into critical section */
            if (mlocks[mlock_idx].apply_times == 0)
                break;
            else
                spin_lock_release(&(mlocks[mlock_idx].lock)); /* out of critical section */
        }
    }
    if (mlock_idx == LOCK_NUM) // if no empty lock is found
    {
        printk("mutex locks not enough!\n");
        assert(mlock_idx == LOCK_NUM)
    }

    mutex_lock_t *mlock = &mlocks[mlock_idx]; // abbreviated mlocks[mlock_idx]

    // mlock->status = UNLOCKED;
    mlock->apply_times += 1;
    mlock->key = key;

    uint64_t mlock_ocp_tmp = (uint64_t)1 << mlock_idx;
    atomic_or_d(mlock_ocp_tmp, (ptr_t)&(my_cpu()->mlock_ocp));

    spin_lock_release(&(mlocks[mlock_idx].lock)); /* out of critical section */

    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t *mlock = &mlocks[mlock_idx];

    spin_lock_acquire(&(mlock->lock)); /* into critical section */

    while (mlock->status == LOCKED)
    {
        do_block(&(my_cpu()->list), &(mlock->block_queue), &(mlock->lock)); /* inside critical section change */
    }
    mlock->pid = my_cpu()->pid; // store the pid of the process that holds the lock
    mlock->status = LOCKED;            // set the lock status to locked

    spin_lock_release(&(mlock->lock)); /* out of critical section */
}

void do_mutex_lock_release(int mlock_idx)
{
    /* TODO: [p2-task2] release mutex lock */
    mutex_lock_t *mlock = &mlocks[mlock_idx];

    spin_lock_acquire(&(mlock->lock)); /* into critical section */

    mlock->status = UNLOCKED;
    if (!list_empty(&(mlock->block_queue)))
        do_unblock(mlock->block_queue.next);

    spin_lock_release(&(mlock->lock)); /* out of critical section */
}

void do_mutex_lock_free(int mlock_idx, pid_t pid, pthread_t tid)
{
    mutex_lock_t *mlock = &mlocks[mlock_idx]; // abbreviated mlocks[mlock_idx]

    spin_lock_acquire(&(mlock->lock)); /* into critical section */

    // if the lock is locked by the current process, release it
    if (mlock->status == LOCKED && mlock->pid == pid && (tid == 0 || mlock->tid == tid))
    {
        mlock->status = UNLOCKED;
        if (!list_empty(&(mlock->block_queue)))
            do_unblock(mlock->block_queue.next);
    }

    mlock->apply_times -= 1;
    if (mlock->apply_times == 0)
    {
        mlock->apply_times = 0;
        mlock->block_queue.prev = mlock->block_queue.next = &(mlock->block_queue);
        mlock->key = 0;
        mlock->pid = 0;
    }

    spin_lock_release(&(mlock->lock)); /* out of critical section */
}
