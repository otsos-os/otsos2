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
#include <kernel/posix/posix.h>
#include <kernel/syscall.h>
#include <lib/com1.h>

void syscall_handler(registers_t *regs) {
  u64 syscall_number = regs->rax;
  u64 arg1 = regs->rdi;
  u64 arg2 = regs->rsi;
  u64 arg3 = regs->rdx;

  // com1_printf("SYSCALL: #%d, args: %p, %p, %p\n", syscall_number, arg1, arg2,
  // arg3);

  switch (syscall_number) {
  case SYS_WRITE:
    regs->rax = (u64)sys_write((int)arg1, (const void *)arg2, (u32)arg3);
    break;
  case SYS_EXIT:
    com1_printf("Process exited with code %d\n", arg1);
    // TODO: SCHEDULER
    while (1)
      __asm__ volatile("hlt");
    break;
  default:
    com1_printf("Unknown syscall: %d\n", syscall_number);
    regs->rax = -1;
    break;
  }
}
