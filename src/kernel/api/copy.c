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

#include <mm/vm/pmap.h>
#include <kernel/gdt.h>
#include <kernel/api/api.h>
#include <kernel/api/session.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <mm/vm/vm_map.h>
#include <mm/kmem.h>

int api_proc_copy(registers_t *regs) {
  process_t *parent = process_current();
  if (!parent || !regs) {
    return -API_ERR_BAD_VALUE;
  }

  process_t *child = alloc_process();
  if (!child) {
    return -API_ERR_RETRY;
  }

  memset(child, 0, sizeof(process_t));

  u64 child_cr3 = pmap_clone(parent->cr3);
  if (!child_cr3) {
    memset(child, 0, sizeof(process_t));
    return -API_ERR_NO_MEMORY;
  }

  child->pid = next_pid++;
  child->ppid = parent->pid;
  child->cr3 = child_cr3;
  child->entry_point = parent->entry_point;

  for (int i = 0; i < PROCESS_NAME_LEN - 1 && parent->name[i]; i++) {
    child->name[i] = parent->name[i];
  }
  child->name[PROCESS_NAME_LEN - 1] = '\0';

  child->user_stack = parent->user_stack;
  child->owns_address_space = 1;
  child->preferred_cpu = -1;
  child->last_cpu = -1;
  child->kusr_auth = parent->kusr_auth;
  child->uid = parent->uid;
  child->gid = parent->gid;
  child->euid = parent->euid;
  child->egid = parent->egid;
  child->suid = parent->suid;
  child->sgid = parent->sgid;
  if (vm_map_init(&child->vm_map, 0, VM_MAP_STACK_END) != 0 ||
      vm_map_fork(&parent->vm_map, &child->vm_map) != 0) {
    pmap_destroy(child_cr3);
    memset(child, 0, sizeof(process_t));
    return (-API_ERR_NO_MEMORY);
  }
  api_copy_handles(child, parent);
  api_session_fork(parent, child);
  if (process_entity_attach(child) != 0) {
    pmap_destroy(child_cr3);
    process_creation_abort(child);
    return -API_ERR_NO_MEMORY;
  }
  scheduler_assign_process(child);

  /* Create thread for child process */
  thread_t *td = thread_create(child, parent->entry_point,
      parent->user_stack, USER_CS, USER_DS);
  if (!td) {
    pmap_destroy(child_cr3);
    process_creation_abort(child);
    return -API_ERR_NO_MEMORY;
  }

  child->main_thread = td;
  child->cur_thread = td;

  /* Copy parent's context, set return value to 0 */
  process_save_context(parent, regs);
  td->context = parent->cur_thread->context;
  thread_copy_fpu_context(td, parent->cur_thread);
  td->context.rax = 0;

  return (int)child->pid;
}
