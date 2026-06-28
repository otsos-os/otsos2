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

#ifndef KERNEL_EVENT_H
#define KERNEL_EVENT_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

/*
 * Event system — FreeBSD kqueue/kevent-inspired kernel event notification.
 *
 * A kqueue is a kernel object that collects events from registered filters.
 * Each registered event is a knote (kernel note) identified by the
 * (ident, filter) pair. When a filter detects that its condition is true,
 * the knote is marked as pending and placed on the kqueue's ready list.
 * The user retrieves pending events via kevent().
 */

/* ── Filters ──────────────────────────────────────────────────────── */

#define EVFILT_READ    (-1)   /* file descriptor readable              */
#define EVFILT_WRITE   (-2)   /* file descriptor writable              */
#define EVFILT_TIMER   (-3)   /* periodic / one-shot timer             */
#define EVFILT_PROC    (-4)   /* process exit / fork / exec            */
#define EVFILT_SIGNAL  (-5)   /* signal delivered to process           */
#define EVFILT_USER    (-6)   /* user-triggered event                  */

#define EVFILT_SYSCOUNT 6     /* number of system filters              */

/* ── Action flags (kevent.flags) ──────────────────────────────────── */

#define EV_ADD       0x0001   /* add event to kqueue                   */
#define EV_DELETE    0x0002   /* remove event from kqueue              */
#define EV_ENABLE    0x0004   /* permit event to be returned           */
#define EV_DISABLE   0x0008   /* disable event (not returned)          */
#define EV_ONESHOT   0x0010   /* delete after first delivery           */
#define EV_CLEAR     0x0020   /* reset state after retrieval           */
#define EV_RECEIPT   0x0040   /* force EV_ERROR on every change        */
#define EV_DISPATCH  0x0080   /* disable after delivery                */
#define EV_EOF       0x8000   /* filter-specific EOF condition         */
#define EV_ERROR     0x4000   /* error occurred (returned in eventlist)*/
#define EV_KEEPUDATA 0x2000   /* preserve existing udata on modify     */

/* ── NOTE_* flags for EVFILT_PROC ─────────────────────────────────── */

#define NOTE_EXIT    0x80000000   /* process exited                  */
#define NOTE_FORK    0x40000000   /* process called fork             */
#define NOTE_EXEC    0x20000000   /* process called exec             */
#define NOTE_TRACK   0x00000001   /* follow process across fork      */
#define NOTE_CHILD   0x00000002   /* child event (returned to parent)*/
#define NOTE_TRACKERR 0x00000004  /* tracking error                  */

/* ── NOTE_* flags for EVFILT_TIMER ────────────────────────────────── */

#define NOTE_SECONDS  0x00000001   /* data in seconds               */
#define NOTE_MSECONDS 0x00000002   /* data in milliseconds          */
#define NOTE_USECONDS 0x00000004   /* data in microseconds          */
#define NOTE_NSECONDS 0x00000008   /* data in nanoseconds           */

/* ── NOTE_* flags for EVFILT_USER ─────────────────────────────────── */

#define NOTE_FFNOP      0x00000000   /* ignore input fflags         */
#define NOTE_FFAND      0x40000000   /* bitwise AND fflags          */
#define NOTE_FFOR       0x80000000   /* bitwise OR  fflags          */
#define NOTE_FFCOPY     0xC0000000   /* copy fflags                 */
#define NOTE_FFCTRLMASK 0xC0000000   /* control mask                */
#define NOTE_FFLAGSMASK 0x00FFFFFF   /* user-defined flag mask      */
#define NOTE_TRIGGER    0x01000000   /* trigger the event           */

/* ── NOTE_* flags for EVFILT_READ / EVFILT_WRITE ──────────────────── */

#define NOTE_LOWAT 0x00000001   /* low-water mark for read/write     */

/* ── Limits ───────────────────────────────────────────────────────── */

#define MAX_KQUEUES    32       /* max kqueue objects system-wide    */
#define MAX_KNOTES     64       /* max knotes per kqueue             */
#define MAX_KEVENTS    64       /* max events returned per kevent()  */

/* ── Userspace kevent structure ───────────────────────────────────── */

struct kevent {
  u64  ident;     /* identifier (fd, pid, timer id, etc.)            */
  s16  filter;    /* filter type (EVFILT_*)                           */
  u16  flags;     /* action flags (EV_*)                              */
  u32  fflags;    /* filter-specific flags (NOTE_*)                   */
  s64  data;      /* filter-specific data value                       */
  u64  udata;     /* opaque user-defined value                        */
};

#define EV_SET(kevp, id, filt, fl, ffl, d, ud)                       \
  do {                                                                \
    (kevp)->ident  = (u64)(id);                                       \
    (kevp)->filter = (s16)(filt);                                     \
    (kevp)->flags  = (u16)(fl);                                       \
    (kevp)->fflags = (u32)(ffl);                                      \
    (kevp)->data   = (s64)(d);                                        \
    (kevp)->udata  = (u64)(ud);                                       \
  } while (0)

/* ── Kernel-internal types ────────────────────────────────────────── */

struct kqueue;
struct knote;
struct process;

/*
 * Filter operations vtable — each filter implements these callbacks.
 * The kernel calls them to check conditions, attach, detach, and
 * process event delivery.
 */
typedef int (*filter_attach_fn)(struct knote *kn);
typedef void (*filter_detach_fn)(struct knote *kn);
typedef int (*filter_event_fn)(struct knote *kn, u32 nevents);
typedef void (*filter_touch_fn)(struct knote *kn, struct kevent *kev);

typedef struct {
  s16              filter;     /* EVFILT_* this ops implements          */
  const char      *name;       /* human-readable name                   */
  filter_attach_fn  attach;     /* called when knote is added            */
  filter_detach_fn  detach;     /* called when knote is removed           */
  filter_event_fn   event;      /* called to check if event is pending    */
  filter_touch_fn   touch;      /* called when knote is modified          */
} filter_ops_t;

/*
 * knote — a registered event within a kqueue.
 * This is the kernel-side representation of a kevent.
 */
typedef struct knote {
  int               used;        /* 1 = slot occupied                     */
  int               pending;     /* 1 = event is ready for delivery       */
  int               disabled;    /* 1 = event disabled (not returned)     */
  u64               ident;       /* identifier value                      */
  s16               filter;      /* filter type                           */
  u16               flags;       /* kevent flags                          */
  u32               fflags;      /* filter-specific flags                 */
  s64               data;        /* filter-specific data                  */
  u64               udata;       /* user-defined opaque value             */

  /* Filter-private state */
  u64               fpriv;       /* filter-private data (timer deadline,
                                   pipe bytes, proc pid, etc.)           */

  /* Back-pointer to owning kqueue */
  struct kqueue    *kq;

  /* Linkage in the kqueue's knote list */
  struct knote     *next;
} knote_t;

/*
 * kqueue — a kernel event queue. Allocated from a static pool.
 * Contains an array of knote slots and a ready-list of pending knotes.
 */
typedef struct kqueue {
  int               used;        /* 1 = slot occupied                     */
  struct process   *owner;       /* owning process (for cleanup on exit)  */

  /* Knote storage */
  knote_t           knotes[MAX_KNOTES];

  /* Ready list — singly-linked list of pending knotes */
  knote_t          *ready_head;
  knote_t          *ready_tail;
  int               ready_count;

  /* Sleep channel for blocking kevent() calls */
  void             *wait_channel;
} kqueue_t;

/* ── Public API ───────────────────────────────────────────────────── */

/* Initialize the event subsystem (called once during boot) */
void event_init(void);

/* Create a new kqueue — returns index into kqueue pool, or -1 on error */
int  kqueue_create(void);

/* Destroy a kqueue by index */
int  kqueue_destroy(int kq_idx);

/* Get kqueue by index (returns NULL if invalid) */
kqueue_t *kqueue_get(int kq_idx);

/* Register or modify events on a kqueue.
 * Processes the changelist (up to nchanges entries), then returns
 * up to nevents pending events into eventlist.
 * timeout_ms: -1 = block forever, 0 = poll, >0 = max wait in ms.
 * Returns number of events placed in eventlist, or -1 on error. */
int  kevent_process(int kq_idx, struct kevent *changelist, int nchanges,
                    struct kevent *eventlist, int nevents,
                    s64 timeout_ms);

/* Notify a kqueue that a knote may be ready (called by filters) */
void knote_ready(knote_t *kn);

/* Notify all knotes on all kqueues that match a given filter+ident */
void knote_notify_all(s16 filter, u64 ident, u32 fflags, s64 data);

/* Wake a process blocked in kevent() — called on event delivery */
void kqueue_wakeup(kqueue_t *kq);

/* Register a filter implementation */
void filter_register(const filter_ops_t *ops);

/* Get filter ops by filter id */
const filter_ops_t *filter_lookup(s16 filter);

/* Timer tick handler — called from IRQ0 to process timer filters */
void event_timer_tick(void);

/* Cleanup all kqueues owned by a process (called on process exit) */
void event_cleanup_process(struct process *proc);

/* Copy kqueues on fork (currently shares kq objects via refcount) */
void event_fork_process(struct process *parent, struct process *child);

/* ── Kernel-to-event notification hooks ───────────────────────────── */

/* Called by process_exit() to notify EVFILT_PROC watchers */
void event_notify_proc_exit(u32 pid, int exit_code);

/* Called by process_create/clone/spawn to notify NOTE_FORK watchers */
void event_notify_proc_fork(u32 parent_pid, u32 child_pid);

/* Called by process_send_signal() to notify EVFILT_SIGNAL watchers */
void event_notify_signal(u32 pid, int sig);

/* Called by pipe_read/pipe_write to notify EVFILT_READ/EVFILT_WRITE */
struct pipe;
void event_notify_pipe_change(struct pipe *p);

#endif /* KERNEL_EVENT_H */
