/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

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
static int idt_loaded = 0;

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
extern void isr_stub_128();

void idt_set_gate(int n, unsigned long long handler, u8 type_attr) {
  idt[n].low_offset = handler & 0xFFFF;
  idt[n].selector = 0x08;
  idt[n].ist = 0;
  idt[n].type_attr = type_attr;
  idt[n].mid_offset = (handler >> 16) & 0xFFFF;
  idt[n].high_offset = (handler >> 32) & 0xFFFFFFFF;
  idt[n].reserved = 0;
}

extern void pic_remap(int offset1, int offset2);

void init_idt() {
  idt_ptr.limit = sizeof(idt_entry_t) * 256 - 1;
  idt_ptr.base = (unsigned long long)&idt;

  pic_remap(0x20, 0x28);

  idt_set_gate(0, (unsigned long long)isr_stub_0, 0x8E);
  idt_set_gate(1, (unsigned long long)isr_stub_1, 0x8E);
  idt_set_gate(2, (unsigned long long)isr_stub_2, 0x8E);
  idt_set_gate(3, (unsigned long long)isr_stub_3, 0x8E);
  idt_set_gate(4, (unsigned long long)isr_stub_4, 0x8E);
  idt_set_gate(5, (unsigned long long)isr_stub_5, 0x8E);
  idt_set_gate(6, (unsigned long long)isr_stub_6, 0x8E);
  idt_set_gate(7, (unsigned long long)isr_stub_7, 0x8E);
  idt_set_gate(8, (unsigned long long)isr_stub_8, 0x8E);
  idt_set_gate(9, (unsigned long long)isr_stub_9, 0x8E);
  idt_set_gate(10, (unsigned long long)isr_stub_10, 0x8E);
  idt_set_gate(11, (unsigned long long)isr_stub_11, 0x8E);
  idt_set_gate(12, (unsigned long long)isr_stub_12, 0x8E);
  idt_set_gate(13, (unsigned long long)isr_stub_13, 0x8E);
  idt_set_gate(14, (unsigned long long)isr_stub_14, 0x8E);
  idt_set_gate(15, (unsigned long long)isr_stub_15, 0x8E);
  idt_set_gate(16, (unsigned long long)isr_stub_16, 0x8E);
  idt_set_gate(17, (unsigned long long)isr_stub_17, 0x8E);
  idt_set_gate(18, (unsigned long long)isr_stub_18, 0x8E);
  idt_set_gate(19, (unsigned long long)isr_stub_19, 0x8E);
  idt_set_gate(20, (unsigned long long)isr_stub_20, 0x8E);
  idt_set_gate(21, (unsigned long long)isr_stub_21, 0x8E);
  idt_set_gate(22, (unsigned long long)isr_stub_22, 0x8E);
  idt_set_gate(23, (unsigned long long)isr_stub_23, 0x8E);
  idt_set_gate(24, (unsigned long long)isr_stub_24, 0x8E);
  idt_set_gate(25, (unsigned long long)isr_stub_25, 0x8E);
  idt_set_gate(26, (unsigned long long)isr_stub_26, 0x8E);
  idt_set_gate(27, (unsigned long long)isr_stub_27, 0x8E);
  idt_set_gate(28, (unsigned long long)isr_stub_28, 0x8E);
  idt_set_gate(29, (unsigned long long)isr_stub_29, 0x8E);
  idt_set_gate(30, (unsigned long long)isr_stub_30, 0x8E);
  idt_set_gate(31, (unsigned long long)isr_stub_31, 0x8E);

  idt_set_gate(32, (unsigned long long)irq_stub_0, 0x8E);
  idt_set_gate(33, (unsigned long long)irq_stub_1, 0x8E);
  idt_set_gate(34, (unsigned long long)irq_stub_2, 0x8E);
  idt_set_gate(35, (unsigned long long)irq_stub_3, 0x8E);
  idt_set_gate(36, (unsigned long long)irq_stub_4, 0x8E);
  idt_set_gate(37, (unsigned long long)irq_stub_5, 0x8E);
  idt_set_gate(38, (unsigned long long)irq_stub_6, 0x8E);
  idt_set_gate(39, (unsigned long long)irq_stub_7, 0x8E);
  idt_set_gate(40, (unsigned long long)irq_stub_8, 0x8E);
  idt_set_gate(41, (unsigned long long)irq_stub_9, 0x8E);
  idt_set_gate(42, (unsigned long long)irq_stub_10, 0x8E);
  idt_set_gate(43, (unsigned long long)irq_stub_11, 0x8E);
  idt_set_gate(44, (unsigned long long)irq_stub_12, 0x8E);
  idt_set_gate(45, (unsigned long long)irq_stub_13, 0x8E);
  idt_set_gate(46, (unsigned long long)irq_stub_14, 0x8E);
  idt_set_gate(47, (unsigned long long)irq_stub_15, 0x8E);

  // Syscall 0x80 (128), DPL=3 (User Mode), Type=0xE (Interrupt Gate)
  // 0xE | DPL<<5 | Present<<7 = 0xE | 0x60 | 0x80 = 0xEE
  idt_set_gate(128, (unsigned long long)isr_stub_128, 0xEE);

  load_idt(&idt_ptr);
  idt_loaded = 1;
}

int idt_is_loaded(void) {
  idt_ptr_t current;
  __asm__ volatile("sidt %0" : "=m"(current));
  u64 expected_base = (u64)&idt;
  u16 expected_limit = (u16)(sizeof(idt_entry_t) * 256 - 1);
  if (current.base != expected_base) {
    return 0;
  }
  if (current.limit != expected_limit) {
    return 0;
  }
  return idt_loaded;
}
