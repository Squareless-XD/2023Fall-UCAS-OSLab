#include <os/irq.h>
// #include <os/task_tools.h>
#include <os/net.h>
// #include <e1000.h>
#include <plic.h>
#include <csr.h>

void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    // TODO: [p5-task3] external interrupt handler.
    // Note: plic_claim and plic_complete will be helpful ...
    WRITE_LOG("external interrupt.\n")

    // clear the interrupt signal in CSR register
    asm volatile("csrc sip, %0\n\t" ::"r"(SIE_SEIE));

    // claim the interrupt
    uint32_t irq = plic_claim();
    if (irq == 0)
        return;
    // handle the interrupt
    else if (irq == PLIC_E1000_QEMU_IRQ || irq == PLIC_E1000_PYNQ_IRQ)
    {
        // handle the e1000 interrupt
        net_handle_irq();
    }
    else
    {
        WRITE_LOG("unknown irq: %d\n", irq);
    }
    // complete the interrupt
    plic_complete(irq);
}
