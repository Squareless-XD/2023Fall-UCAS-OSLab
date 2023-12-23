#ifndef SMP_H
#define SMP_H

#include <lib.h>

#define NR_CPUS 2

extern void smp_init();
extern void wakeup_other_hart();
extern void lock_kernel();
extern void unlock_kernel();
extern void lock_kernel_s();
extern void unlock_kernel_s();
static inline uint64_t get_current_cpu_id()
{
    uint64_t ret;
    __asm__ __volatile__ ("csrr %0, mhartid\n" : "=r"(ret) ::);
    return ret;
}

#endif /* SMP_H */
