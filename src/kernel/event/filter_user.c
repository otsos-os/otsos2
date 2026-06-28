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
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_USER — user-triggered events.
 *
 * ident:  arbitrary user-chosen identifier
 * fflags: lower 24 bits are user-defined flags
 *
 * Triggered by setting NOTE_TRIGGER in fflags via kevent() change.
 * On return, fflags contains the user-defined flags in the lower 24 bits.
 */

static int filt_user_attach(knote_t *kn) {
  /* EVFILT_USER is always valid — any ident is acceptable */
  com1_printf("[EVFILT_USER] attach: ident=%llu\n", kn->ident);
  return 0;
}

static void filt_user_detach(knote_t *kn) {
  (void)kn;
}

static int filt_user_event(knote_t *kn, u32 nevents) {
  /* The event is pending if the triggered flag is set */
  if (kn->fpriv & NOTE_TRIGGER) {
    /* Clear the trigger after delivery (unless EV_CLEAR is not set) */
    if (kn->flags & EV_CLEAR) {
      kn->fpriv &= ~NOTE_TRIGGER;
    }
    return 1;
  }
  return 0;
}

static void filt_user_touch(knote_t *kn, struct kevent *kev) {
  if (kev->fflags & NOTE_TRIGGER) {
    /* Apply fflags manipulation */
    u32 ctrl = kev->fflags & NOTE_FFCTRLMASK;
    u32 user_fflags = kev->fflags & NOTE_FFLAGSMASK;

    switch (ctrl) {
    case NOTE_FFNOP:
      /* No change to existing fflags */
      break;
    case NOTE_FFAND:
      kn->fpriv &= user_fflags;
      break;
    case NOTE_FFOR:
      kn->fpriv |= user_fflags;
      break;
    case NOTE_FFCOPY:
      kn->fpriv = user_fflags;
      break;
    }

    /* Set trigger bit */
    kn->fpriv |= NOTE_TRIGGER;
    knote_ready(kn);
  } else {
    /* Just update fflags without triggering */
    kn->fflags = kev->fflags;
  }
}

const filter_ops_t filter_user_ops = {
  .filter = EVFILT_USER,
  .name   = "user",
  .attach = filt_user_attach,
  .detach = filt_user_detach,
  .event  = filt_user_event,
  .touch  = filt_user_touch,
};
