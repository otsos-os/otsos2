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
#include <kernel/signal.h>
#include <kernel/process.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_SIGNAL — returns when a signal is delivered to the process.
 *
 * ident:  signal number to monitor
 * data:   number of times the signal has occurred since last kevent()
 *
 * This filter automatically sets EV_CLEAR internally.
 */

/* Per-process signal counts (indexed by pid, then signal number) */
#define MAX_SIGNAL_SLOTS 64  /* matches MAX_PROCESSES */
#define MAX_SIGNALS      32

static u32 signal_counts[MAX_SIGNAL_SLOTS][MAX_SIGNALS];

static int filt_signal_attach(knote_t *kn) {
  int sig = (int)kn->ident;
  if (sig < 0 || sig >= MAX_SIGNALS) {
    com1_printf("[EVFILT_SIGNAL] attach: invalid signal %d\n", sig);
    return -API_ERR_INVAL;
  }

  /* EVFILT_SIGNAL auto-sets EV_CLEAR */
  kn->flags |= EV_CLEAR;
  return 0;
}

static void filt_signal_detach(knote_t *kn) {
  (void)kn;
}

static int filt_signal_event(knote_t *kn, u32 nevents) {
  int sig = (int)kn->ident;
  process_t *proc = process_current();
  if (!proc) {
    return 0;
  }

  int slot = (int)proc->pid % MAX_SIGNAL_SLOTS;
  if (sig < 0 || sig >= MAX_SIGNALS) {
    return 0;
  }

  u32 count = signal_counts[slot][sig];
  if (count > 0) {
    kn->data = (s64)count;
    /* EV_CLEAR: reset count after retrieval */
    signal_counts[slot][sig] = 0;
    return 1;
  }

  return 0;
}

static void filt_signal_touch(knote_t *kn, struct kevent *kev) {
  (void)kn;
  (void)kev;
}

/*
 * Called by process_send_signal() to record a signal delivery.
 * This is the kernel-to-event-system hook for signals.
 */
void event_notify_signal(u32 pid, int sig) {
  if (sig < 0 || sig >= MAX_SIGNALS) {
    return;
  }

  int slot = (int)(pid % MAX_SIGNAL_SLOTS);
  signal_counts[slot][sig]++;

  /* Notify all knotes watching this signal for this process */
  knote_notify_all(EVFILT_SIGNAL, (u64)sig, 0, 0);
}

const filter_ops_t filter_signal_ops = {
  .filter = EVFILT_SIGNAL,
  .name   = "signal",
  .attach = filt_signal_attach,
  .detach = filt_signal_detach,
  .event  = filt_signal_event,
  .touch  = filt_signal_touch,
};
