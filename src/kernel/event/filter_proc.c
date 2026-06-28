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

#include <kernel/event/event.h>
#include <kernel/process.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_PROC — monitor process events (exit, fork, exec).
 *
 * ident:  process ID to monitor
 * fflags: NOTE_EXIT, NOTE_FORK, NOTE_EXEC, NOTE_TRACK
 *
 * On return:
 *   fflags contains the events that triggered
 *   data contains the exit status (for NOTE_EXIT) or child PID (for NOTE_CHILD)
 */

static int filt_proc_attach(knote_t *kn) {
  u32 pid = (u32)kn->ident;

  process_t *proc = process_get(pid);
  if (!proc) {
    com1_printf("[EVFILT_PROC] attach: pid %d not found\n", pid);
    return -API_ERR_NO_PROC;
  }

  com1_printf("[EVFILT_PROC] attach: monitoring pid=%d\n", pid);
  return 0;
}

static void filt_proc_detach(knote_t *kn) {
  (void)kn;
}

static int filt_proc_event(knote_t *kn, u32 nevents) {
  u32 pid = (u32)kn->ident;

  process_t *proc = process_get(pid);
  if (!proc) {
    /* Process no longer exists — report exit with NOTE_EXIT */
    kn->fflags |= NOTE_EXIT;
    return 1;
  }

  /* Check if process has exited (zombie state) */
  if (proc->state == PROC_STATE_ZOMBIE) {
    if (kn->fflags & NOTE_EXIT) {
      /* Already reported — check if still pending */
      return kn->pending ? 1 : 0;
    }
    kn->fflags |= NOTE_EXIT;
    kn->data = proc->exit_code;
    return 1;
  }

  return 0;
}

static void filt_proc_touch(knote_t *kn, struct kevent *kev) {
  kn->fflags = kev->fflags;
}

/*
 * Called by process_exit() to notify all EVFILT_PROC watchers.
 * This is the kernel-to-event-system hook.
 */
void event_notify_proc_exit(u32 pid, int exit_code) {
  knote_notify_all(EVFILT_PROC, (u64)pid, NOTE_EXIT, (s64)exit_code);
}

/*
 * Called by process_create/clone/spawn to notify NOTE_FORK watchers.
 */
void event_notify_proc_fork(u32 parent_pid, u32 child_pid) {
  knote_notify_all(EVFILT_PROC, (u64)parent_pid, NOTE_FORK, (s64)child_pid);
}

const filter_ops_t filter_proc_ops = {
  .filter = EVFILT_PROC,
  .name   = "proc",
  .attach = filt_proc_attach,
  .detach = filt_proc_detach,
  .event  = filt_proc_event,
  .touch  = filt_proc_touch,
};
