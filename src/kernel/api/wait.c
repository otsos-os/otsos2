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
#include <mm/vm/vm_map.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

extern void pmap_destroy_page_tables_only(u64 cr3);

int api_proc_wait(int *status) {
  process_t *current = process_current();
  if (!current) {
    return -API_ERR_NO_CHILD;
  }

  /* Try to reap a zombie child first (non-blocking) */
  for (int attempt = 0; attempt < 2; attempt++) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
      process_t *child = &process_table[i];
      if (child->pid == 0) {
        continue;
      }
      thread_t *child_td = child->main_thread;
      if (!child_td || child_td->state != PROC_STATE_ZOMBIE) {
        continue;
      }
      if (child->ppid != current->pid) {
        continue;
      }

      if (status && is_user_address(status, sizeof(int))) {
        *status = child->exit_code;
      }

      if (child->owns_address_space && child->cr3) {
        u64 old_cr3 = pmap_get_cr3();
        pmap_load(child->cr3);
        vm_map_free_all(child);
        pmap_load(old_cr3);
        pmap_destroy(child->cr3);
        child->cr3 = 0;
        child->owns_address_space = 0;
      }

      if (child_td) {
        thread_destroy(child_td);
      }

      int pid = (int)child->pid;
      memset(child, 0, sizeof(process_t));
      return pid;
    }

    /* No zombie child found — sleep and wait for a child to exit */
    if (attempt == 0) {
      /* Use the current process's PID as the wait channel.
       * process_exit() calls event_notify_proc_exit which calls
       * knote_notify_all, but we also need a direct wakeup.
       * The simplest approach: sleep on a global "child wait" channel
       * that process_exit wakes up. */
      extern void proc_sleep(void *channel);
      extern void proc_wakeup(void *channel);

      /* Use the parent's process structure as the wait channel */
      proc_sleep((void *)current);
    }
  }

  return -API_ERR_NO_CHILD;
}
