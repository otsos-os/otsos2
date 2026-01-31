#include <kernel/interrupts/idt.h>

typedef struct {
  unsigned short low_offset;
  unsigned short selector;
  unsigned char ist;
  unsigned char type_attr;
  unsigned short mid_offset;
  unsigned int high_offset;
  unsigned int reserved;
} __attribute__((packed)) idt_entry_t;

typedef struct {
  unsigned short limit;
  unsigned long long base;
} __attribute__((packed)) idt_ptr_t;

idt_entry_t idt[256];
idt_ptr_t idt_ptr;

extern void load_idt(idt_ptr_t *);

extern void isr_stub_0();
extern void isr_stub_1();
extern void isr_stub_2();
extern void isr_stub_3();
extern void isr_stub_4();
extern void isr_stub_5();
extern void isr_stub_6();
extern void isr_stub_7();
extern void isr_stub_8();
extern void isr_stub_9();
extern void isr_stub_10();
extern void isr_stub_11();
extern void isr_stub_12();
extern void isr_stub_13();
extern void isr_stub_14();
extern void isr_stub_15();
extern void isr_stub_16();
extern void isr_stub_17();
extern void isr_stub_18();
extern void isr_stub_19();
extern void isr_stub_20();
extern void isr_stub_21();
extern void isr_stub_22();
extern void isr_stub_23();
extern void isr_stub_24();
extern void isr_stub_25();
extern void isr_stub_26();
extern void isr_stub_27();
extern void isr_stub_28();
extern void isr_stub_29();
extern void isr_stub_30();
extern void isr_stub_31();

extern void irq_stub_0();
extern void irq_stub_1();
extern void irq_stub_2();
extern void irq_stub_3();
extern void irq_stub_4();
extern void irq_stub_5();
extern void irq_stub_6();
extern void irq_stub_7();
extern void irq_stub_8();
extern void irq_stub_9();
extern void irq_stub_10();
extern void irq_stub_11();
extern void irq_stub_12();
extern void irq_stub_13();
extern void irq_stub_14();
extern void irq_stub_15();

void idt_set_gate(int n, unsigned long long handler) {
  idt[n].low_offset = handler & 0xFFFF;
  idt[n].selector = 0x08;
  idt[n].ist = 0;
  idt[n].type_attr = 0x8E;
  idt[n].mid_offset = (handler >> 16) & 0xFFFF;
  idt[n].high_offset = (handler >> 32) & 0xFFFFFFFF;
  idt[n].reserved = 0;
}

extern void pic_remap(int offset1, int offset2);

void init_idt() {
  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base = (unsigned long long)&idt;

  pic_remap(0x20, 0x28);

  idt_set_gate(0, (unsigned long long)isr_stub_0);
  idt_set_gate(1, (unsigned long long)isr_stub_1);
  idt_set_gate(2, (unsigned long long)isr_stub_2);
  idt_set_gate(3, (unsigned long long)isr_stub_3);
  idt_set_gate(4, (unsigned long long)isr_stub_4);
  idt_set_gate(5, (unsigned long long)isr_stub_5);
  idt_set_gate(6, (unsigned long long)isr_stub_6);
  idt_set_gate(7, (unsigned long long)isr_stub_7);
  idt_set_gate(8, (unsigned long long)isr_stub_8);
  idt_set_gate(9, (unsigned long long)isr_stub_9);
  idt_set_gate(10, (unsigned long long)isr_stub_10);
  idt_set_gate(11, (unsigned long long)isr_stub_11);
  idt_set_gate(12, (unsigned long long)isr_stub_12);
  idt_set_gate(13, (unsigned long long)isr_stub_13);
  idt_set_gate(14, (unsigned long long)isr_stub_14);
  idt_set_gate(15, (unsigned long long)isr_stub_15);
  idt_set_gate(16, (unsigned long long)isr_stub_16);
  idt_set_gate(17, (unsigned long long)isr_stub_17);
  idt_set_gate(18, (unsigned long long)isr_stub_18);
  idt_set_gate(19, (unsigned long long)isr_stub_19);
  idt_set_gate(20, (unsigned long long)isr_stub_20);
  idt_set_gate(21, (unsigned long long)isr_stub_21);
  idt_set_gate(22, (unsigned long long)isr_stub_22);
  idt_set_gate(23, (unsigned long long)isr_stub_23);
  idt_set_gate(24, (unsigned long long)isr_stub_24);
  idt_set_gate(25, (unsigned long long)isr_stub_25);
  idt_set_gate(26, (unsigned long long)isr_stub_26);
  idt_set_gate(27, (unsigned long long)isr_stub_27);
  idt_set_gate(28, (unsigned long long)isr_stub_28);
  idt_set_gate(29, (unsigned long long)isr_stub_29);
  idt_set_gate(30, (unsigned long long)isr_stub_30);
  idt_set_gate(31, (unsigned long long)isr_stub_31);

  idt_set_gate(32, (unsigned long long)irq_stub_0);
  idt_set_gate(33, (unsigned long long)irq_stub_1);
  idt_set_gate(34, (unsigned long long)irq_stub_2);
  idt_set_gate(35, (unsigned long long)irq_stub_3);
  idt_set_gate(36, (unsigned long long)irq_stub_4);
  idt_set_gate(37, (unsigned long long)irq_stub_5);
  idt_set_gate(38, (unsigned long long)irq_stub_6);
  idt_set_gate(39, (unsigned long long)irq_stub_7);
  idt_set_gate(40, (unsigned long long)irq_stub_8);
  idt_set_gate(41, (unsigned long long)irq_stub_9);
  idt_set_gate(42, (unsigned long long)irq_stub_10);
  idt_set_gate(43, (unsigned long long)irq_stub_11);
  idt_set_gate(44, (unsigned long long)irq_stub_12);
  idt_set_gate(45, (unsigned long long)irq_stub_13);
  idt_set_gate(46, (unsigned long long)irq_stub_14);
  idt_set_gate(47, (unsigned long long)irq_stub_15);

  load_idt(&idt_ptr);

  __asm__ volatile("sti");
}
