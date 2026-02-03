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

#include <kernel/gdt.h>
#include <kernel/interrupts/idt.h>
#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <kernel/syscall.h>
#include <lib/com1.h>

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

extern void syscall_entry(void);

static inline void wrmsr(u32 msr, u64 value) {
  u32 low = value & 0xFFFFFFFF;
  u32 high = value >> 32;
  __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline u64 rdmsr(u32 msr) {
  u32 low, high;
  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
  return ((u64)high << 32) | low;
}

void syscall_init(void) {
  /* 1. Enable SCE (System Call Extensions) in EFER */
  u64 efer = rdmsr(MSR_EFER);
  wrmsr(MSR_EFER, efer | 1);

  /* 2. Configure STAR register
   * STAR[47:32] = Kernel CS/SS base  (0x08 -> KCODE=0x08, KDATA=0x10)
   * STAR[63:48] = User CS/SS base    (0x10 -> UDATA=0x18, UCODE=0x20)
   */
  u64 star = ((u64)GDT_KERNEL_CODE << 32) | ((u64)GDT_KERNEL_DATA << 48);
  wrmsr(MSR_STAR, star);

  /* 3. Set LSTAR to our entry point */
  wrmsr(MSR_LSTAR, (u64)syscall_entry);

  /* 4. Configure SFMASK (RFLAGS mask)
   * Mask IF (interrupts), TF (trap), etc.
   */
  wrmsr(MSR_SFMASK, 0x200); /* Mask interrupts (IF) */

  com1_printf("[SYSCALL] syscall/sysret initialized\n");
}

void syscall_handler(registers_t *regs) {
  u64 syscall_number = regs->rax;
  u64 arg1 = regs->rdi;
  u64 arg2 = regs->rsi;
  u64 arg3 = regs->rdx;

  switch (syscall_number) {
  case SYS_READ:
    regs->rax = (u64)sys_read((int)arg1, (void *)arg2, (u32)arg3);
    break;
  case SYS_WRITE:
    regs->rax = (u64)sys_write((int)arg1, (const void *)arg2, (u32)arg3);
    break;
  case SYS_OPEN:
    regs->rax = (u64)sys_open((const char *)arg1, (int)arg2);
    break;
  case SYS_CLOSE:
    regs->rax = (u64)sys_close((int)arg1);
    break;
  case SYS_EXIT:
    process_exit((int)arg1);
    break;
  case SYS_KILL:
    regs->rax = process_kill((u32)arg1);
    break;
  default:
    com1_printf("Unknown syscall: %d\n", syscall_number);
    regs->rax = -1;
    break;
  }
}
