#include <asm.h>
#include <csr.h>
#include <asm/regs.h>

.macro SAVE_CONTEXT
  /* TODO: [p2-task3] save all general purpose registers here! */
  /* HINT: Pay attention to the function of tp and sp, and save them carefully! */

  // get the address of current pcb from tp
  # addi sp, sp, -8
  # sd t0, 0(sp)
  csrw sscratch, t0
  ld t0, PCB_KERNEL_SP(tp)

  # // open a space in kernel stack
  # addi t0, t0, -OFFSET_SIZE

  // store registers into kernel stack
  # sd zero, OFFSET_REG_ZERO(t0)
  sd ra, OFFSET_REG_RA(t0)
  sd sp, OFFSET_REG_SP(t0)
  sd gp, OFFSET_REG_GP(t0)
  sd tp, OFFSET_REG_TP(t0)

  # sd t0, OFFSET_REG_T0(t0)
  sd t1, OFFSET_REG_T1(t0)
  sd t2, OFFSET_REG_T2(t0)

  sd s0, OFFSET_REG_S0(t0)
  sd s1, OFFSET_REG_S1(t0)

  sd a0, OFFSET_REG_A0(t0)
  sd a1, OFFSET_REG_A1(t0)
  sd a2, OFFSET_REG_A2(t0)
  sd a3, OFFSET_REG_A3(t0)
  sd a4, OFFSET_REG_A4(t0)
  sd a5, OFFSET_REG_A5(t0)
  sd a6, OFFSET_REG_A6(t0)
  sd a7, OFFSET_REG_A7(t0)

  sd s2, OFFSET_REG_S2(t0)
  sd s3, OFFSET_REG_S3(t0)
  sd s4, OFFSET_REG_S4(t0)
  sd s5, OFFSET_REG_S5(t0)
  sd s6, OFFSET_REG_S6(t0)
  sd s7, OFFSET_REG_S7(t0)
  sd s8, OFFSET_REG_S8(t0)
  sd s9, OFFSET_REG_S9(t0)
  sd s10, OFFSET_REG_S10(t0)
  sd s11, OFFSET_REG_S11(t0)

  sd t3, OFFSET_REG_T3(t0)
  sd t4, OFFSET_REG_T4(t0)
  sd t5, OFFSET_REG_T5(t0)
  sd t6, OFFSET_REG_T6(t0)

  // get old t0
  mv sp, t0
  # ld t0, 0(sp)
  # addi sp, sp, 8
  csrr t0, sscratch

  // store sp and t0 into kernel stack
  sd t0, OFFSET_REG_T0(sp)

  // sp is now at kernel stack


  /*
   * Disable user-mode memory access as it should only be set in the
   * actual user copy routines.
   *
   * Disable the FPU to detect illegal usage of floating point in kernel
   * space.
   */
  li t0, SR_SUM | SR_FS
  csrc CSR_SSTATUS, t0

  /* TODO: [p2-task3] save sstatus, sepc, stval and scause on kernel stack */

  csrr t2, sstatus
  csrr t3, sepc
  csrr t4, stval
  csrr t5, scause
  sd t2, OFFSET_REG_SSTATUS(sp)
  sd t3, OFFSET_REG_SEPC(sp)
  sd t4, OFFSET_REG_SBADADDR(sp)
  sd t5, OFFSET_REG_SCAUSE(sp)

.endm

.macro RESTORE_CONTEXT
  /* TODO: Restore all general purpose registers and sepc, sstatus */
  /* HINT: Pay attention to sp again! */

  // get the address of current pcb from tp
  ld t0, PCB_KERNEL_SP(tp)

  // load sstatus, sepc from kernel stack, and restore them
  ld t1, OFFSET_REG_SSTATUS(t0)
  ld t2, OFFSET_REG_SEPC(t0)
  csrw sstatus, t1
  # addi t2, t2, 4 // store the next instruction address (sepc + 4) back to sepc
  csrw sepc, t2

  // load registers from kernel stack
  # ld zero, OFFSET_REG_ZERO(t0)
  ld ra, OFFSET_REG_RA(t0)
  ld sp, OFFSET_REG_SP(t0)
  ld gp, OFFSET_REG_GP(t0)
  ld tp, OFFSET_REG_TP(t0) // defaultly keep tp the same as current_running

  # ld t0, OFFSET_REG_T0(t0)
  ld t1, OFFSET_REG_T1(t0)
  ld t2, OFFSET_REG_T2(t0)

  ld s0, OFFSET_REG_S0(t0)
  ld s1, OFFSET_REG_S1(t0)

  ld a0, OFFSET_REG_A0(t0)
  ld a1, OFFSET_REG_A1(t0)
  ld a2, OFFSET_REG_A2(t0)
  ld a3, OFFSET_REG_A3(t0)
  ld a4, OFFSET_REG_A4(t0)
  ld a5, OFFSET_REG_A5(t0)
  ld a6, OFFSET_REG_A6(t0)
  ld a7, OFFSET_REG_A7(t0)

  ld s2, OFFSET_REG_S2(t0)
  ld s3, OFFSET_REG_S3(t0)
  ld s4, OFFSET_REG_S4(t0)
  ld s5, OFFSET_REG_S5(t0)
  ld s6, OFFSET_REG_S6(t0)
  ld s7, OFFSET_REG_S7(t0)
  ld s8, OFFSET_REG_S8(t0)
  ld s9, OFFSET_REG_S9(t0)
  ld s10, OFFSET_REG_S10(t0)
  ld s11, OFFSET_REG_S11(t0)

  ld t3, OFFSET_REG_T3(t0)
  ld t4, OFFSET_REG_T4(t0)
  ld t5, OFFSET_REG_T5(t0)
  ld t6, OFFSET_REG_T6(t0)

  // store t0
  ld t0, OFFSET_REG_T0(t0)

.endm

ENTRY(enable_preempt)
  not t0, x0
  csrs CSR_SIE, t0
  jr ra
ENDPROC(enable_preempt)

ENTRY(disable_preempt)
  csrw CSR_SIE, zero
  jr ra
ENDPROC(disable_preempt)

ENTRY(enable_interrupt)
  li t0, SR_SIE
  csrs CSR_SSTATUS, t0
  jr ra
ENDPROC(enable_interrupt)

ENTRY(disable_interrupt)
  li t0, SR_SIE
  csrc CSR_SSTATUS, t0
  jr ra
ENDPROC(disable_interrupt)

// NOTE: the address of previous pcb in a0
// NOTE: the address of next pcb in a1
ENTRY(switch_to)
  # addi sp, sp, -(SWITCH_TO_SIZE)

  /* TODO: [p2-task1] save all callee save registers on kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  // get the kernel & user stack address of previous pcb
  # ld t0, PCB_KERNEL_SP(a0)
  # ld t1, PCB_USER_SP(a0)

  // get the context address of PREVIOUS PCB
  li  t0, PCB_CONTEXT
  add  t1, a0, t0

  // get the context address of NEXT PCB
  add  t2, a1, t0

  // store registers into kernel stack
  sd ra, SWITCH_TO_RA(t1)
  sd sp, SWITCH_TO_SP(t1)
  sd s0, SWITCH_TO_S0(t1)
  sd s1, SWITCH_TO_S1(t1)
  sd s2, SWITCH_TO_S2(t1)
  sd s3, SWITCH_TO_S3(t1)
  sd s4, SWITCH_TO_S4(t1)
  sd s5, SWITCH_TO_S5(t1)
  sd s6, SWITCH_TO_S6(t1)
  sd s7, SWITCH_TO_S7(t1)
  sd s8, SWITCH_TO_S8(t1)
  sd s9, SWITCH_TO_S9(t1)
  sd s10, SWITCH_TO_S10(t1)
  sd s11, SWITCH_TO_S11(t1)


  /* TODO: [p2-task1] restore all callee save registers from kernel stack,
   * see the definition of `struct switchto_context` in sched.h*/

  // keep tp the same as current_running
  mv tp, a1


  // load registers from kernel stack
  ld ra, SWITCH_TO_RA(t2)
  ld sp, SWITCH_TO_SP(t2)
  ld s0, SWITCH_TO_S0(t2)
  ld s1, SWITCH_TO_S1(t2)
  ld s2, SWITCH_TO_S2(t2)
  ld s3, SWITCH_TO_S3(t2)
  ld s4, SWITCH_TO_S4(t2)
  ld s5, SWITCH_TO_S5(t2)
  ld s6, SWITCH_TO_S6(t2)
  ld s7, SWITCH_TO_S7(t2)
  ld s8, SWITCH_TO_S8(t2)
  ld s9, SWITCH_TO_S9(t2)
  ld s10, SWITCH_TO_S10(t2)
  ld s11, SWITCH_TO_S11(t2)

  # addi sp, sp, SWITCH_TO_SIZE
  ret
ENDPROC(switch_to)

ENTRY(ret_from_exception)
  /* TODO: [p2-task3] restore context via provided macro and return to sepc */
  /* HINT: remember to check your sp, does it point to the right address? */
  RESTORE_CONTEXT
  sret
ENDPROC(ret_from_exception)

ENTRY(exception_handler_entry)

  /* TODO: [p2-task3] save context via the provided macro */
  SAVE_CONTEXT


  /* TODO: [p2-task3] load ret_from_exception into $ra so that we can return to
   * ret_from_exception when interrupt_help complete.
   */
  la ra, ret_from_exception


  /* TODO: [p2-task3] call interrupt_helper
   * NOTE: don't forget to pass parameters for it.
   */
# void interrupt_helper(regs_context_t *regs, uint64_t stval, uint64_t scause)
  ld a0, PCB_KERNEL_SP(tp)
  ld a1, OFFSET_REG_SBADADDR(a0)
  ld a2, OFFSET_REG_SCAUSE(a0)
  tail  interrupt_helper

ENDPROC(exception_handler_entry)
