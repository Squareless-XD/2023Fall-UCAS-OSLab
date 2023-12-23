#ifndef ATOMIC_H
#define ATOMIC_H

/* from linux/arch/riscv/include/asm/cmpxchg.h */

#include <type.h>

// atomicly store val to *mem_addr, and return the old value of *mem_addr
static inline uint32_t atomic_swap(uint32_t val, ptr_t mem_addr)
{
    uint32_t ret;
    __asm__ __volatile__ (
        "amoswap.w.aqrl %0, %2, %1\n" // atomic swap, ret = *mem_addr, *mem_addr = val
        : "=r"(ret), "+A" (*(uint32_t*)mem_addr)
        : "r"(val)
        : "memory");
    return ret;
}

static inline uint64_t atomic_swap_d(uint64_t val, ptr_t mem_addr)
{
    uint64_t ret;
    __asm__ __volatile__ (
        "amoswap.d.aqrl %0, %2, %1\n"
        : "=r"(ret), "+A" (*(uint64_t*)mem_addr)
        : "r"(val)
        : "memory");
    return ret;
}

static inline uint32_t atomic_add(uint32_t val, ptr_t mem_addr)
{
    uint32_t ret;
    __asm__ __volatile__ (
        "amoadd.w.aqrl %0, %2, %1\n"
        : "=r"(ret), "+A" (*(uint32_t*)mem_addr)
        : "r"(val)
        : "memory");
    return ret;
}

static inline uint64_t atomic_add_d(uint64_t val, ptr_t mem_addr)
{
    uint64_t ret;
    __asm__ __volatile__ (
        "amoadd.d.aqrl %0, %2, %1\n"
        : "=r"(ret), "+A" (*(uint64_t*)mem_addr)
        : "r"(val)
        : "memory");
    return ret;
}

static inline uint64_t atomic_or_d(uint64_t val, ptr_t mem_addr)
{
    uint64_t ret;
    __asm__ __volatile__ (
        "amoor.d.aqrl %0, %2, %1\n"
        : "=r"(ret), "+A" (*(uint64_t*)mem_addr)
        : "r"(val)
        : "memory");
    return ret;
}

// /* if *mem_addr == old_val, then *mem_addr = new_val, else return *mem_addr */
// static inline uint32_t atomic_cmpxchg(uint32_t old_val, uint32_t new_val, ptr_t mem_addr)
// {
//     uint32_t ret;
//     register unsigned int __rc;
//     __asm__ __volatile__ (
//           "0:	lr.w %0, %2\n"     // load reserved, ret = *mem_addr
//           "	bne  %0, %z3, 1f\n"    // if ret != old_val, goto 1 (z means 0)
//           "	sc.w.rl %1, %z4, %2\n" // store conditional, *mem_addr = new_val if *mem_addr == ret. __rc = 0 if store success
//           "	bnez %1, 0b\n"         // if store failed, goto 0
//           "	fence rw, rw\n"        // fence
//           "1:\n"
//           : "=&r" (ret), "=&r" (__rc), "+A" (*(uint32_t*)mem_addr)
//           : "rJ" (old_val), "rJ" (new_val)
//           : "memory");
//     return ret; // return *mem_addr
// }

// /* if *mem_addr == old_val, then *mem_addr = new_val, else return *mem_addr */
// static inline uint64_t atomic_cmpxchg_d(uint64_t old_val, uint64_t new_val, ptr_t mem_addr)
// {
//     uint64_t ret;
//     register unsigned int __rc;
//     __asm__ __volatile__ (
//           "0:	lr.d %0, %2\n"
//           "	bne  %0, %z3, 1f\n"
//           "	sc.d.rl %1, %z4, %2\n"
//           "	bnez %1, 0b\n"
//           "	fence rw, rw\n"
//           "1:\n"
//           : "=&r" (ret), "=&r" (__rc), "+A" (*(uint64_t*)mem_addr)
//           : "rJ" (old_val), "rJ" (new_val)
//           : "memory");
//     return ret;
// }

#endif /* ATOMIC_H */
