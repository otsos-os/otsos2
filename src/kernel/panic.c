#include <kernel/drivers/vga.h>
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
                                           "Reserved",
                                           "Security",
                                           "Reserved"};

void dump_memory(unsigned long long addr, int count) {
  unsigned char *ptr = (unsigned char *)addr;
  for (int i = 0; i < count; i++) {
    if (i > 0 && i % 16 == 0) {
      com1_printf("\n\r");
      printf("\n");
    }

    char buf[3];
    itoa(ptr[i], buf, 16);

    if (ptr[i] < 16) {
      com1_printf("0");
      printf("0");
    }
    com1_printf("%s ", buf);
    printf("%s ", buf);
  }
  com1_printf("\n\r");
  printf("\n");
}

void print_stack_trace(unsigned long long rbp) {
  com1_printf("\n\rStack Trace:\n\r");
  printf("\nStack Trace:\n");

  unsigned long long *frame = (unsigned long long *)rbp;
  for (int i = 0; i < 8 && frame; i++) {
    if ((unsigned long long)frame < 0x100000 ||
        (unsigned long long)frame > 0x800000)
      break;
    unsigned long long rip = frame[1];
    com1_printf("  [%d] %p\n\r", i, rip);
    printf("  [%d] %p\n", i, rip);
    frame = (unsigned long long *)frame[0];
  }
}

void kernel_panic(registers_t *regs) {
  __asm__ volatile("cli");

  vga_set_color(0x1F); 
  clear_scr();

  const char *msg = (regs->int_no < 32) ? exception_messages[regs->int_no]
                                        : "Unexpected Interrupt";

  printf(":::::::::::::::::::::::: KERNEL PANIC: %s ::::::::::::::::::::::::\n",
         msg);
  com1_printf("\n\r!!!!!!!!!!!!!!!! KERNEL PANIC: %s !!!!!!!!!!!!!!!!\n\r",
              msg);

  printf("RIP: %p  CS: %x  RFLAGS: %p  ERR: %x\n", regs->rip, (int)regs->cs,
         regs->rflags, (int)regs->err_code);
  printf("RAX: %p RBX: %p RCX: %p RDX: %p\n", regs->rax, regs->rbx, regs->rcx,
         regs->rdx);
  printf("RSI: %p RDI: %p RBP: %p RSP: %p\n", regs->rsi, regs->rdi, regs->rbp,
         regs->rsp);

  unsigned long long cr0, cr2, cr3, cr4;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

  printf("CR0: %p  CR2: %p (PF Address)\n", cr0, cr2);
  printf("CR3: %p (PML4)  CR4: %p\n", cr3, cr4);

  if (regs->int_no == 14) {
    printf("PF Details: ");
    if (!(regs->err_code & 1))
      printf("Not-Present ");
    else
      printf("Protection-Violation ");
    if (regs->err_code & 2)
      printf("Write ");
    else
      printf("Read ");
    if (regs->err_code & 4)
      printf("User ");
    if (regs->err_code & 16)
      printf("Instruction-Fetch ");
    printf("\n");
  }

  struct {
    u16 limit;
    u64 base;
  } __attribute__((packed)) gdtr, idtr;
  __asm__ volatile("sgdt %0" : "=m"(gdtr));
  __asm__ volatile("sidt %0" : "=m"(idtr));
  printf("GDTR: %p (Limit: %x)  IDTR: %p (Limit: %x)\n", gdtr.base, gdtr.limit,
         idtr.base, idtr.limit);

  printf("\nCode dump at RIP:\n");
  dump_memory(regs->rip, 16);

  printf("\nStack dump at RSP:\n");
  dump_memory(regs->rsp, 32);

  print_stack_trace(regs->rbp);

  com1_printf("\n\rSystem Halted.\n\r");
  printf("\nSystem Halted.");

  while (1) {
    __asm__ volatile("hlt");
  }
}
