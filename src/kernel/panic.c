#include <kernel/interrupts/idt.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static const char *exception_messages[] = {"Division By Zero",
                                           "Debug",
                                           "Non Maskable Interrupt",
                                           "Breakpoint",
                                           "Into Detected Overflow",
                                           "Out of Bounds",
                                           "Invalid Opcode",
                                           "No Coprocessor",
                                           "Double Fault",
                                           "Coprocessor Segment Overrun",
                                           "Bad TSS",
                                           "Segment Not Present",
                                           "Stack Fault",
                                           "General Protection Fault",
                                           "Page Fault",
                                           "Unknown Interrupt",
                                           "Coprocessor Fault",
                                           "Alignment Check",
                                           "Machine Check",
                                           "SIMD Floating-Point",
                                           "Virtualization",
                                           "Control Protection",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Reserved",
                                           "Security",
                                           "Reserved"};

void kernel_panic(registers_t *regs) {

  if (regs->int_no < 32) {
  } else {
  }

  com1_printf("\n\r####################################################\n\r");
  if (regs->int_no < 32) {
    com1_printf("              KERNEL PANIC: %s\n\r",
                exception_messages[regs->int_no]);
  } else {
    com1_printf("              KERNEL PANIC: Unexpected Interrupt\n\r");
  }
  com1_printf("####################################################\n\r");
  com1_printf("Interrupt: %d (Error Code: %x)\n\r", (int)regs->int_no,
              (int)regs->err_code);
  com1_printf("RIP: %p  CS: %x  RFLAGS: %p\n\r", regs->rip, (int)regs->cs,
              regs->rflags);
  com1_printf("RSP: %p  SS: %x\n\r", regs->rsp, (int)regs->ss);
  com1_printf("\n\rRegisters:\n\r");
  com1_printf("RAX: %p  RBX: %p  RCX: %p\n\r", regs->rax, regs->rbx, regs->rcx);
  com1_printf("RDX: %p  RSI: %p  RDI: %p\n\r", regs->rdx, regs->rsi, regs->rdi);
  com1_printf("RBP: %p  R8:  %p  R9:  %p\n\r", regs->rbp, regs->r8, regs->r9);
  com1_printf("R10: %p  R11: %p  R12: %p\n\r", regs->r10, regs->r11, regs->r12);
  com1_printf("R13: %p  R14: %p  R15: %p\n\r", regs->r13, regs->r14, regs->r15);

  if (regs->int_no == 14) {
    unsigned long long cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    com1_printf("\n\rPage Fault Address (CR2): %p\n\r", cr2);
  }

  com1_printf("\n\rSystem Halted.\n\r");

  while (1) {
    __asm__ volatile("cli; hlt");
  }
}
