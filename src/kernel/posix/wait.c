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
#include <kernel/useraddr.h>
#include <mlibc/memory.h>

int sys_wait(int *status) {
  process_t *current = process_current();
  if (!current) {
    return -ECHILD;
  }

  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_t *child = &process_table[i];
    if (child->state != PROC_STATE_ZOMBIE) {
      continue;
    }
    if (child->ppid != current->pid) {
      continue;
    }

    if (status && is_user_address(status, sizeof(int))) {
      *status = child->exit_code;
    }

    if (child->owns_address_space && child->cr3) {
      mmu_free_user_space(child->cr3);
      kfree((void *)(child->cr3 & PTE_ADDR_MASK));
      child->cr3 = 0;
      child->owns_address_space = 0;
    }

    if (child->kernel_stack) {
      u64 kstack_base = child->kernel_stack - KERNEL_STACK_SIZE;
      kfree((void *)kstack_base);
    }

    int pid = (int)child->pid;
    memset(child, 0, sizeof(process_t));
    child->state = PROC_STATE_UNUSED;
    return pid;
  }

  return -ECHILD;
}
