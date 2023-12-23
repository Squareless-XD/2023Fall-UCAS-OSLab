// #include <os/lock.h> // ignored
#include <os/sched.h>

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
//         printk("spin lock not available! address: %lx, ra: %lx\n", lock, ra);
//         assert(0)
//     }
// }

void spin_lock_acquire(spin_lock_t *lock)
{
    // WRITE_LOG("acquire lock addr: %lx\n", lock)
    /* TODO: [p2-task2] acquire spin lock */
    while (atomic_swap(LOCKED, (ptr_t)&(lock->status)) == LOCKED)
        ;
    // WRITE_LOG("lock get\n")
}

void spin_lock_release(spin_lock_t *lock)
{
    // WRITE_LOG("release lock addr: %lx\n", lock)
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}

void spin_lock_acquire_no_log(spin_lock_t *lock)
{
    /* TODO: [p2-task2] acquire spin lock */
    while (atomic_swap(LOCKED, (ptr_t)&(lock->status)) == LOCKED)
        ;
}

void spin_lock_release_no_log(spin_lock_t *lock)
{
    /* TODO: [p2-task2] release spin lock */
    lock->status = UNLOCKED;
}
