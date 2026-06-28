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
#include <kernel/drivers/timer.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_TIMER — periodic or one-shot timer events.
 *
 * ident: arbitrary user-chosen timer ID (unique per kqueue)
 * data:  timeout period (in units determined by fflags)
 * fflags: NOTE_SECONDS, NOTE_MSECONDS, NOTE_USECONDS, NOTE_NSECONDS
 *         (default: milliseconds)
 *
 * On return, data contains the number of times the timer has expired
 * since the last kevent() call.
 *
 * If EV_ONESHOT is set, the timer fires once and is deleted.
 * Otherwise it fires periodically.
 *
 * fpriv stores the next-fire deadline in timer ticks.
 */

/* Track all timer knotes for the tick handler */
static knote_t *timer_list[MAX_KQUEUES * MAX_KNOTES];
static int timer_count = 0;

static u64 compute_timer_ticks(s64 data, u32 fflags) {
  u64 freq = timer_get_frequency(); /* Hz */

  if (fflags & NOTE_SECONDS) {
    return (u64)data * freq;
  } else if (fflags & NOTE_USECONDS) {
    return (u64)data * freq / 1000000;
  } else if (fflags & NOTE_NSECONDS) {
    return (u64)data * freq / 1000000000;
  } else {
    /* Default: milliseconds (NOTE_MSECONDS or no flag) */
    return (u64)data * freq / 1000;
  }
}

static int filt_timer_attach(knote_t *kn) {
  if (kn->data <= 0) {
    kn->data = 1; /* Minimum 1 unit */
  }

  u64 period_ticks = compute_timer_ticks(kn->data, kn->fflags);
  if (period_ticks == 0) {
    period_ticks = 1;
  }

  kn->fpriv = timer_get_ticks() + period_ticks;

  /* Add to global timer list */
  if (timer_count < (int)(MAX_KQUEUES * MAX_KNOTES)) {
    timer_list[timer_count++] = kn;
  }

  com1_printf("[EVFILT_TIMER] attach: ident=%llu period=%llu ticks\n",
              kn->ident, period_ticks);

  return 0;
}

static void filt_timer_detach(knote_t *kn) {
  /* Remove from timer list */
  for (int i = 0; i < timer_count; i++) {
    if (timer_list[i] == kn) {
      timer_list[i] = timer_list[--timer_count];
      return;
    }
  }
}

static int filt_timer_event(knote_t *kn, u32 nevents) {
  /* If we're on the ready list, the timer has fired.
   * data = number of expirations since last check. */
  if (kn->pending) {
    return 1;
  }

  /* Check if deadline has passed */
  if (timer_get_ticks() >= kn->fpriv) {
    /* Count expirations */
    u64 now = timer_get_ticks();
    u64 period = compute_timer_ticks(kn->data, kn->fflags);
    if (period == 0) {
      period = 1;
    }

    u64 elapsed = now - (kn->fpriv - period);
    u64 expirations = elapsed / period;
    if (expirations < 1) {
      expirations = 1;
    }
    kn->data = (s64)expirations;

    /* Schedule next fire (for periodic timers) */
    if (!(kn->flags & EV_ONESHOT)) {
      kn->fpriv = now + period;
    }

    return 1;
  }

  return 0;
}

static void filt_timer_touch(knote_t *kn, struct kevent *kev) {
  /* Re-arm with new parameters */
  kn->data = kev->data;
  kn->fflags = kev->fflags;

  u64 period_ticks = compute_timer_ticks(kn->data, kn->fflags);
  if (period_ticks == 0) {
    period_ticks = 1;
  }
  kn->fpriv = timer_get_ticks() + period_ticks;
}

/*
 * Called from event_timer_tick() on every IRQ0.
 * Checks all timer knotes and fires expired ones.
 */
void filter_timer_tick(void) {
  u64 now = timer_get_ticks();

  for (int i = 0; i < timer_count; i++) {
    knote_t *kn = timer_list[i];
    if (!kn || !kn->used || kn->disabled) {
      continue;
    }

    if (now >= kn->fpriv) {
      /* Timer expired */
      u64 period = compute_timer_ticks(kn->data, kn->fflags);
      if (period == 0) {
        period = 1;
      }

      u64 elapsed = now - (kn->fpriv - period);
      u64 expirations = elapsed / period;
      if (expirations < 1) {
        expirations = 1;
      }
      kn->data = (s64)expirations;

      if (!(kn->flags & EV_ONESHOT)) {
        kn->fpriv = now + period;
      }

      knote_ready(kn);
    }
  }
}

const filter_ops_t filter_timer_ops = {
  .filter = EVFILT_TIMER,
  .name   = "timer",
  .attach = filt_timer_attach,
  .detach = filt_timer_detach,
  .event  = filt_timer_event,
  .touch  = filt_timer_touch,
};
