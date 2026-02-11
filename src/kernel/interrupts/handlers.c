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
#include <kernel/drivers/tty.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/interrupts/idt.h>
#include <kernel/mmu.h>
#include <kernel/scheduler.h>
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
      printf("\n\033[31m[KERNEL] Exception %d detected in userspace process %d "
             "(%s) at RIP=%p\033[0m\n",
             regs->int_no, proc ? (int)proc->pid : -1,
             proc ? proc->name : "???", (void *)regs->rip);

      com1_printf("\n[KERNEL] Exception %d detected in userspace process %d "
                  "(%s) at RIP=%p\n",
                  regs->int_no, proc ? (int)proc->pid : -1,
                  proc ? proc->name : "???", (void *)regs->rip);
      com1_printf("[KERNEL] CS=0x%x SS=0x%x RSP=%p\n", (unsigned)regs->cs,
                  (unsigned)regs->ss, (void *)regs->rsp);

      if (regs->int_no == 13) {
        printf("\033[31m[KERNEL] General Protection Fault ERR=0x%x\033[0m\n",
               (unsigned)regs->err_code);
        com1_printf("[KERNEL] General Protection Fault ERR=0x%x\n",
                    (unsigned)regs->err_code);
      } else if (regs->int_no == 14) {
        u64 cr2 = 0;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        printf("\033[31m[KERNEL] Segmentation Fault (Page Fault) CR2=%p "
               "ERR=0x%x\033[0m\n",
               (void *)cr2, (unsigned)regs->err_code);

        com1_printf("[KERNEL] Segmentation Fault\n");
        com1_printf("[KERNEL] Page Fault: CR2=%p ERR=0x%x\n", (void *)cr2,
                    (unsigned)regs->err_code);
        u64 pte_flags = mmu_get_pte_flags(cr2);
        com1_printf("[KERNEL] CR2 PTE flags: %p\n", (void *)pte_flags);
      } else if (regs->int_no == 6) {
        printf("\033[31m[KERNEL] Invalid Opcode\033[0m\n");
        com1_printf("[KERNEL] Invalid Opcode\n");
      } else if (regs->int_no == 0) {
        printf("\033[31m[KERNEL] Division by Zero\033[0m\n");
        com1_printf("[KERNEL] Division by Zero\n");
      }

      __asm__ volatile("sti");
      process_exit(-1);
    } else {
      __asm__ volatile("sti");
      kernel_panic(regs);
    }
  }
}

void irq_handler(registers_t *regs) {
  if (regs->int_no == 32) {
    timer_handler();
    watchdog_tick();
    scheduler_tick(regs);
    keyboard_poll();
    tty_update();
  } else if (regs->int_no == 33) {
    keyboard_common_handler();
  }

  pic_send_eoi(regs->int_no - 32);
}
