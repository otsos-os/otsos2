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
#include <kernel/api/posix/posix.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>
#include <mlibc/stdio.h>
#include <mm/vm/vm_map.h>
#include <mm/kmem.h>

long api_proc_clone(u64 flags, u64 child_stack, u64 ptid, registers_t *regs) {
  (void)ptid;
  process_t *parent = process_current();
  if (!parent || !regs) {
    return -API_ERR_BAD_VALUE;
  }

  thread_t *parent_td = thread_current();
  if (!parent_td) {
    return -API_ERR_BAD_VALUE;
  }

  /* Thread creation: CLONE_VM | CLONE_THREAD — shares address space */
  if ((flags & API_CLONE_THREAD) && (flags & API_CLONE_VM)) {
    if (!child_stack) {
      return -API_ERR_BAD_VALUE;
    }

    thread_t *new_td = thread_create(parent,
        parent_td->context.rip,
        child_stack & ~0xFULL,
        USER_CS, USER_DS);
    if (!new_td) {
      return -API_ERR_NO_MEMORY;
    }

    /* Copy parent's context, set return value to 0 */
    thread_save_context(parent_td, regs);
    new_td->context = parent_td->context;
    thread_copy_fpu_context(new_td, parent_td);
    new_td->context.rax = 0;
    new_td->context.rsp = child_stack & ~0xFULL;

    printk("[CLONE] new thread tid=%d in PID %d\n",
        new_td->tid, parent->pid);

    return (long)new_td->tid;
  }

  /* Full process fork: no CLONE_VM, no CLONE_THREAD */
  if (flags & (API_CLONE_VM | API_CLONE_THREAD)) {
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
  child->mmap_base = parent->mmap_base;
  child->kusr_auth = parent->kusr_auth;
  child->uid = parent->uid;
  child->gid = parent->gid;
  child->euid = parent->euid;
  child->egid = parent->egid;
  child->suid = parent->suid;
  child->sgid = parent->sgid;
  vm_map_fork(parent, child);
  api_copy_handles(child, parent);
  posix_copy_fds(child, parent);
  api_session_fork(parent, child);
  if (process_entity_attach(child) != 0) {
    pmap_destroy(child_cr3);
    process_creation_abort(child);
    return -API_ERR_NO_MEMORY;
  }
  scheduler_assign_process(child);

  /* Create thread for child process */
  thread_t *td = thread_create(child, parent->entry_point,
      child_stack ? (child_stack & ~0xFULL) : parent->user_stack,
      USER_CS, USER_DS);
  if (!td) {
    pmap_destroy(child_cr3);
    process_creation_abort(child);
    return -API_ERR_NO_MEMORY;
  }

  child->main_thread = td;
  child->cur_thread = td;

  /* Copy parent's context, set return value to 0 */
  thread_save_context(parent_td, regs);
  td->context = parent_td->context;
  thread_copy_fpu_context(td, parent_td);
  td->context.rax = 0;

  if (child_stack) {
    td->context.rsp = child_stack & ~0xFULL;
  }

  return (long)child->pid;
}
