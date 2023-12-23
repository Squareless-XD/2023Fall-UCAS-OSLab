#include <asm.h>
#include <csr.h>

ENTRY(setup_exception)
  /* TODO: [p2-task3] save exception_handler_entry into STVEC */
  la t0, exception_handler_entry
  # andi t0, t0, -4
  csrw stvec, t0

  /* TODO: [p2-task4] enable interrupts globally */
  li t0, SR_SIE
  csrs sstatus, t0

  ret
ENDPROC(setup_exception)
