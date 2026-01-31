#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

extern void kernel_panic(registers_t *regs);
extern void pic_send_eoi(unsigned char irq);

void isr_handler(registers_t *regs) { kernel_panic(regs); }

void irq_handler(registers_t *regs) {
  if (regs->int_no == 33) {
    keyboard_common_handler();
  }

  pic_send_eoi(regs->int_no - 32);
}
