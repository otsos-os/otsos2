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

#include <kernel/mmu.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>

static void load_context(process_t *proc, registers_t *regs) {
  regs->r15 = proc->context.r15;
  regs->r14 = proc->context.r14;
  regs->r13 = proc->context.r13;
  regs->r12 = proc->context.r12;
  regs->r11 = proc->context.r11;
  regs->r10 = proc->context.r10;
  regs->r9 = proc->context.r9;
  regs->r8 = proc->context.r8;
  regs->rbp = proc->context.rbp;
  regs->rdi = proc->context.rdi;
  regs->rsi = proc->context.rsi;
  regs->rdx = proc->context.rdx;
  regs->rcx = proc->context.rcx;
  regs->rbx = proc->context.rbx;
  regs->rax = proc->context.rax;
  regs->rip = proc->context.rip;
  regs->cs = proc->context.cs;
  regs->rflags = proc->context.rflags;
  regs->rsp = proc->context.rsp;
  regs->ss = proc->context.ss;
}

static process_t *pick_next(process_t *current) {
  int start = 0;
  if (current) {
    start = (int)(current - process_table) + 1;
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    int idx = (start + i) % MAX_PROCESSES;
    process_t *cand = &process_table[idx];
    if (cand->state == PROC_STATE_RUNNABLE) {
      return cand;
    }
  }

  return current;
}

void scheduler_tick(registers_t *regs) {
  if (!regs) {
    return;
  }

  process_t *current = process_current();
  if (!current) {
    return;
  }

  if ((regs->cs & 3) == 0) {
    return;
  }

  process_save_context(current, regs);

  if (current->state == PROC_STATE_RUNNING) {
    current->state = PROC_STATE_RUNNABLE;
  }

  process_t *next = pick_next(current);
  if (!next || next == current) {
    current->state = PROC_STATE_RUNNING;
    return;
  }

  process_set_current(next);
  if (next->cr3 != current->cr3) {
    mmu_write_cr3(next->cr3);
  }

  load_context(next, regs);
}
