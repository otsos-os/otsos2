#include <mlibc/mlibc.h>
#include <kernel/interrupts/idt.h>

extern void kernel_panic(registers_t *regs);
extern void pic_send_eoi(unsigned char irq);

void isr_handler(registers_t *regs) { kernel_panic(regs); }

void irq_handler(registers_t *regs) {
  if (regs->int_no == 33) {
    (void)inb(0x60);
  }

  pic_send_eoi(regs->int_no - 32);
}
