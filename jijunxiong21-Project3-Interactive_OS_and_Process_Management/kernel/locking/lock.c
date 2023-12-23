#include <os/lock.h>
#include <os/sched.h>
// #include <os/list.h>
#include <atomic.h>
#include <assert.h>

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

void spin_lock_init(spin_lock_t *lock) // seems that this function is useless XD
{
    /* TODO: [p2-task2] initialize spin lock */
    lock->status = UNLOCKED;
}

int spin_lock_try_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] try to acquire spin lock */
    if (atomic_swap(LOCKED, (ptr_t)&(lock->status)) == UNLOCKED)
        return true; // lock successfully
    return false;
}

// void spin_lock_acquire(spin_lock_t *lock) // for test wrong spinlock
// {
//     /* TODO: [p2-task2] acquire spin lock */
//     if (lock->status == UNLOCKED)
//         lock->status = LOCKED;
//     else // use assembly to print the ra register
//     {
//         reg_t ra;
//         asm volatile("mv %0, ra\n\t" : "=r"(ra));
//         printk("spin lock not available! address: %x, ra: %x\n", lock, ra);
//         assert(0);
//     }
// }

void spin_lock_acquire(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (atomic_swap(LOCKED, (ptr_t)&(lock->status)) == LOCKED)
        ;
}

void spin_lock_release(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
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
        assert(mlock_idx == LOCK_NUM);
    }

    mutex_lock_t *mlock = &mlocks[mlock_idx]; // abbreviated mlocks[mlock_idx]

    // mlock->status = UNLOCKED;
    mlock->apply_times += 1;
    mlock->key = key;

    // spin_lock_acquire(&pcb_lk); /* enter critical section */
    uint64_t mlock_ocp_tmp = (uint64_t)1 << mlock_idx;
    atomic_or_d(mlock_ocp_tmp, (ptr_t)&(current_running[get_current_cpu_id()]->mlock_ocp));
    // current_running[get_current_cpu_id()]->mlock_ocp |= ((uint64_t)1 << mlock_idx);
    // spin_lock_release(&pcb_lk); /* leave critical section */

    spin_lock_release(&(mlocks[mlock_idx].lock)); /* out of critical section */

    return mlock_idx;
}

void do_mutex_lock_acquire(int mlock_idx)
{
    /* TODO: [p2-task2] acquire mutex lock */
    mutex_lock_t *mlock = &mlocks[mlock_idx];
    int core_id = get_current_cpu_id();

    spin_lock_acquire(&(mlock->lock)); /* into critical section */

    while (mlock->status == LOCKED)
    {
        do_block(&(current_running[core_id]->list), &(mlock->block_queue), &(mlock->lock)); /* inside critical section change */
        core_id = get_current_cpu_id(); // refresh the core id
    }
    mlock->pid = current_running[core_id]->pid; // store the pid of the process that holds the lock
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

void do_mutex_lock_free(int mlock_idx, pid_t pid)
{
    mutex_lock_t *mlock = &mlocks[mlock_idx]; // abbreviated mlocks[mlock_idx]

    spin_lock_acquire(&(mlock->lock)); /* into critical section */

    // if the lock is locked by the current process, release it
    if (mlock->status == LOCKED && mlock->pid == pid)
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
