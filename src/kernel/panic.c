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
    com1_printf("  [%d] %p\n\r", i, (void *)rip);
    printf("  [%d] %p\n", i, (void *)rip);
    frame = (unsigned long long *)frame[0];
  }
}

void kernel_panic(registers_t *regs) {
  __asm__ volatile("cli");

  vga_set_color(0x1F);
  clear_scr();

  const char *msg = (regs->int_no < 32) ? exception_messages[regs->int_no]
                                        : "Unexpected Interrupt";

  com1_printf(
      "\n\r:::::::::::::::::::::::: KERNEL PANIC ::::::::::::::::::::::::\n\r");
  printf(":::::::::::::::::::::::: KERNEL PANIC ::::::::::::::::::::::::\n");

  com1_printf("Exception: %s\n\r", msg);
  printf("Exception: %s\n", msg);

  com1_printf("RIP: %p  CS: %x  RFLAGS: %p  ERR: %x\n\r", (void *)regs->rip,
              (int)regs->cs, (void *)regs->rflags, (int)regs->err_code);
  printf("RIP: %p  CS: %x  RFLAGS: %p  ERR: %x\n", (void *)regs->rip,
         (int)regs->cs, (void *)regs->rflags, (int)regs->err_code);

  com1_printf("RAX: %p RBX: %p RCX: %p RDX: %p\n\r", (void *)regs->rax,
              (void *)regs->rbx, (void *)regs->rcx, (void *)regs->rdx);
  printf("RAX: %p RBX: %p RCX: %p RDX: %p\n", (void *)regs->rax,
         (void *)regs->rbx, (void *)regs->rcx, (void *)regs->rdx);

  com1_printf("RSI: %p RDI: %p RBP: %p RSP: %p\n\r", (void *)regs->rsi,
              (void *)regs->rdi, (void *)regs->rbp, (void *)regs->rsp);
  printf("RSI: %p RDI: %p RBP: %p RSP: %p\n", (void *)regs->rsi,
         (void *)regs->rdi, (void *)regs->rbp, (void *)regs->rsp);

  unsigned long long cr0, cr2, cr3, cr4;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));

  com1_printf("CR0: %p  CR2: %p  CR3: %p  CR4: %p\n\r", (void *)cr0,
              (void *)cr2, (void *)cr3, (void *)cr4);
  printf("CR0: %p  CR2: %p  CR3: %p  CR4: %p\n", (void *)cr0, (void *)cr2,
         (void *)cr3, (void *)cr4);

  if (regs->int_no == 14) {
    com1_printf("PF Details: ");
    if (!(regs->err_code & 1))
      com1_printf("Not-Present ");
    else
      com1_printf("Protection-Violation ");
    if (regs->err_code & 2)
      com1_printf("Write ");
    if (regs->err_code & 16)
      com1_printf("Instruction-Fetch ");
    com1_printf("\n\r");

    printf("PF Details: ");
    if (!(regs->err_code & 1))
      printf("Not-Present ");
    else
      printf("Protection-Violation ");
    if (regs->err_code & 2)
      printf("Write ");
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
  com1_printf("GDTR: %p (Limit: %x)  IDTR: %p (Limit: %x)\n\r",
              (void *)gdtr.base, gdtr.limit, (void *)idtr.base, idtr.limit);
  printf("GDTR: %p (Limit: %x)  IDTR: %p (Limit: %x)\n", (void *)gdtr.base,
         gdtr.limit, (void *)idtr.base, idtr.limit);

  com1_printf("\n\rCode dump at RIP:\n\r");
  printf("\nCode dump at RIP:\n");
  dump_memory(regs->rip, 16);

  com1_printf("\n\rStack dump at RSP:\n\r");
  printf("\nStack dump at RSP:\n");
  dump_memory(regs->rsp, 32);

  print_stack_trace(regs->rbp);

  com1_printf("\n\rSystem Halted.\n\r");
  printf("\nSystem Halted.");

  while (1) {
    __asm__ volatile("hlt");
  }
}

void panic(const char *format, ...) {
  __asm__ volatile("cli");

  vga_set_color(0x1F);
  clear_scr();

  com1_printf(
      "\n\r:::::::::::::::::::::::: KERNEL PANIC ::::::::::::::::::::::::\n\r");
  printf(":::::::::::::::::::::::: KERNEL PANIC ::::::::::::::::::::::::\n");

  com1_printf("Message: ");
  printf("Message: ");

  __builtin_va_list args;
  __builtin_va_start(args, format);

  char buffer[512];
  int i = 0;
  const char *p = format;

  while (*p && i < 511) {
    if (*p == '%') {
      p++;
      switch (*p) {
      case 's': {
        const char *str = __builtin_va_arg(args, const char *);
        while (*str && i < 511)
          buffer[i++] = *str++;
        break;
      }
      case 'd': {
        int val = __builtin_va_arg(args, int);
        if (val < 0) {
          buffer[i++] = '-';
          val = -val;
        }
        char tmp[16];
        int j = 0;
        do {
          tmp[j++] = '0' + (val % 10);
          val /= 10;
        } while (val && j < 16);
        while (j > 0 && i < 511)
          buffer[i++] = tmp[--j];
        break;
      }
      case 'x': {
        unsigned int val = __builtin_va_arg(args, unsigned int);
        const char hex[] = "0123456789ABCDEF";
        char tmp[16];
        int j = 0;
        do {
          tmp[j++] = hex[val % 16];
          val /= 16;
        } while (val && j < 16);
        while (j > 0 && i < 511)
          buffer[i++] = tmp[--j];
        break;
      }
      case '%':
        buffer[i++] = '%';
        break;
      default:
        buffer[i++] = '%';
        if (i < 511)
          buffer[i++] = *p;
        break;
      }
    } else {
      buffer[i++] = *p;
    }
    p++;
  }
  buffer[i] = '\0';

  __builtin_va_end(args);

  com1_printf("%s\n\r", buffer);
  printf("%s\n", buffer);

  com1_printf("\n\rSystem Halted.\n\r");
  printf("\nSystem Halted.");

  while (1) {
    __asm__ volatile("hlt");
  }
}
