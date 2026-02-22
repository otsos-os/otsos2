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
#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <mlibc/memory.h>

int sys_fork(registers_t *regs) {
  process_t *parent = process_current();
  if (!parent || !regs) {
    return -EINVAL;
  }

  process_t *child = alloc_process();
  if (!child) {
    return -EAGAIN;
  }

  child->state = PROC_STATE_EMBRYO;

  u64 child_cr3 = mmu_clone_user_space(parent->cr3);
  if (!child_cr3) {
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    return -ENOMEM;
  }

  u8 *kstack = (u8 *)kmalloc_aligned(KERNEL_STACK_SIZE, 16);
  if (!kstack) {
    mmu_free_user_space(child_cr3);
    kfree((void *)(child_cr3 & PTE_ADDR_MASK));
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    return -ENOMEM;
  }
  memset(kstack, 0, KERNEL_STACK_SIZE);

  memset(child, 0, sizeof(process_t));

  child->pid = next_pid++;
  child->ppid = parent->pid;
  child->state = PROC_STATE_RUNNABLE;
  child->cr3 = child_cr3;
  child->entry_point = parent->entry_point;

  for (int i = 0; i < PROCESS_NAME_LEN - 1 && parent->name[i]; i++) {
    child->name[i] = parent->name[i];
  }
  child->name[PROCESS_NAME_LEN - 1] = '\0';

  child->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);
  child->user_stack = parent->user_stack;

  process_save_context(parent, regs);
  child->context = parent->context;
  child->context.rax = 0;

  child->exit_code = 0;
  child->owns_address_space = 1;
  child->mmap_base = parent->mmap_base;
  posix_copy_fds(child, parent);
  child->next = NULL;

  return (int)child->pid;
}
