#ifndef SMP_H
#define SMP_H

#include <type.h>
#include <os/lock.h>

#define NR_CPUS 2

/* gaint lock for OS kernel, for supporting p3-task3 symetric multi-processor */
extern spin_lock_t giant_lock;
extern int giant_lock_owner;

extern void smp_init();
extern void wakeup_other_hart();
extern uint64_t get_current_cpu_id();
extern void lock_kernel();
extern void unlock_kernel();

#endif /* SMP_H */
