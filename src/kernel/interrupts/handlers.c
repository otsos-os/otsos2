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

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/console/console.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/power/pbutton.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/event/event.h>
#include <kernel/interrupts/idt.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_map.h>
#include <kernel/scheduler.h>
#include <kernel/smp/smp.h>
#include <kernel/thread.h>
#include <mlibc/mlibc.h>

extern void kernel_panic(registers_t *regs);
extern void pic_send_eoi(unsigned char irq);

#include <kernel/syscall.h>

#include <kernel/process.h>
#include <mlibc/stdio.h>

void isr_handler(registers_t *regs) {
  if (regs->int_no == 128) {
    syscall_handler(regs);
  } else {
    if ((regs->cs & 3) == 3) {
      process_t *proc = process_current();

    if (regs->int_no == 14) {
        u64 cr2 = 0;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        if (proc && vm_map_fault(proc, cr2, regs->err_code) == 0) {
            return;
        }
        if (vm_cow_fault(cr2, regs->err_code) == 0) {
            return;
        }
        printk("\n[KERNEL] Page Fault in process %d (%s) RIP=%p CR2=%p ERR=0x%x\n",
                    proc ? (int)proc->pid : -1,
                    proc ? proc->name : "???", (void *)regs->rip,
                    (void *)cr2, (unsigned)regs->err_code);
        printf("\033[31m[KERNEL] Segmentation Fault\033[0m\n");
    } else if (regs->int_no == 13) {
        printf("\033[31m[KERNEL] General Protection Fault ERR=0x%x\033[0m\n",
               (unsigned)regs->err_code);
        printk("[KERNEL] General Protection Fault ERR=0x%x\n",
                    (unsigned)regs->err_code);
    } else if (regs->int_no == 6) {
        printf("\033[31m[KERNEL] Invalid Opcode\033[0m\n");
        printk("[KERNEL] Invalid Opcode\n");
    } else if (regs->int_no == 0) {
        printf("\033[31m[KERNEL] Division by Zero\033[0m\n");
        printk("[KERNEL] Division by Zero\n");
    } else {
        printk("\n[KERNEL] Exception %d in process %d (%s) RIP=%p\n",
                    regs->int_no, proc ? (int)proc->pid : -1,
                    proc ? proc->name : "???", (void *)regs->rip);
    }
    if (smp_lock_held()) {
      smp_unlock();
    }
    process_exit(-1);
    } else {
      __asm__ volatile("sti");
      kernel_panic(regs);
    }
  }
}

void irq_handler(registers_t *regs) {
  if (regs->int_no == 32) {
    eventtimer_dispatch();
    power_button_poll();
    watchdog_tick();
    event_timer_tick();
    crypto_rng_tick();
    keyboard_poll();
    scheduler_tick(regs);
    terminal_update();
  } else if (regs->int_no == 33) {
    keyboard_common_handler();
  } else if (regs->int_no == 48) {
    eventtimer_dispatch();
    power_button_poll();
    watchdog_tick();
    event_timer_tick();
    crypto_rng_tick();
    keyboard_poll();
    scheduler_tick(regs);
    terminal_update();
    lapic_eoi();
    return;
  } else if (regs->int_no == 255) {
    return;
  }

  if (regs->int_no >= 32 && regs->int_no < 48)
    pic_send_eoi(regs->int_no - 32);
}
