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

#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/timer.h>
#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

extern void kernel_panic(registers_t *regs);
extern void pic_send_eoi(unsigned char irq);

#include <kernel/syscall.h>

#include <kernel/process.h>
#include <lib/com1.h>

void isr_handler(registers_t *regs) {
  if (regs->int_no == 128) {
    syscall_handler(regs);
  } else {
    if ((regs->cs & 3) == 3) {
      process_t *proc = process_current();
      com1_printf("\n[KERNEL] Exception %d detected in userspace process %d "
                  "(%s) at RIP=%p\n",
                  regs->int_no, proc ? (int)proc->pid : -1,
                  proc ? proc->name : "???", (void *)regs->rip);

      
      if (regs->int_no == 13) {
        com1_printf("[KERNEL] General Protection Fault\n");
      } else if (regs->int_no == 14) {
        com1_printf("[KERNEL] Segmentation Fault\n");
      } else if (regs->int_no == 6) {
        com1_printf("[KERNEL] Invalid Opcode\n");
      }

      process_exit(-1);
    } else {
      kernel_panic(regs);
    }
  }
}

void irq_handler(registers_t *regs) {
  if (regs->int_no == 32) {
    timer_handler();
    keyboard_poll();
  } else if (regs->int_no == 33) {
    keyboard_common_handler();
  }

  pic_send_eoi(regs->int_no - 32);
}
