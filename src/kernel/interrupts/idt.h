#ifndef INTERRUPTS_IDT_H
#define INTERRUPTS_IDT_H

void init_idt();

typedef struct {
  unsigned long long r15, r14, r13, r12, r11, r10, r9, r8;
  unsigned long long rbp, rdi, rsi, rdx, rcx, rbx, rax;
  unsigned long long int_no, err_code;
  unsigned long long rip, cs, rflags, rsp, ss;
} registers_t;

#endif
