#include <os/mm.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;
static spin_lock_t kernMemCurr_lk = {UNLOCKED};
static spin_lock_t userMemCurr_lk = {UNLOCKED};

ptr_t allocKernelPage(int numPage)
{
    spin_lock_acquire(&kernMemCurr_lk); /* enter critical section */
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    spin_lock_release(&kernMemCurr_lk); /* leave critical section */
    return ret;
}

ptr_t allocUserPage(int numPage)
{
    spin_lock_acquire(&userMemCurr_lk); /* enter critical section */
    // align PAGE_SIZE
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    spin_lock_release(&userMemCurr_lk); /* leave critical section */
    return ret;
}
