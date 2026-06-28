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
#include <kernel/api/api.h>
#include <kernel/drivers/timer.h>
#include <kernel/process.h>
#include <kernel/panic.h>
#include <mm/kmem.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/* ── Static pool of kqueue objects ────────────────────────────────── */

static kqueue_t kqueue_pool[MAX_KQUEUES];
static int event_initialized = 0;

/* ── Filter registry ──────────────────────────────────────────────── */

static const filter_ops_t *filter_table[EVFILT_SYSCOUNT];

/* Convert negative filter id to array index: EVFILT_READ(-1) -> 0 */
static int filter_index(s16 filter) {
  int idx = -filter - 1;
  if (idx < 0 || idx >= EVFILT_SYSCOUNT) {
    return -1;
  }
  return idx;
}

void filter_register(const filter_ops_t *ops) {
  int idx = filter_index(ops->filter);
  if (idx < 0) {
    com1_printf("[EVENT] filter_register: invalid filter %d\n", ops->filter);
    return;
  }
  filter_table[idx] = ops;
  com1_printf("[EVENT] registered filter '%s' (id=%d)\n", ops->name,
              ops->filter);
}

const filter_ops_t *filter_lookup(s16 filter) {
  int idx = filter_index(filter);
  if (idx < 0) {
    return NULL;
  }
  return filter_table[idx];
}

/* ── Sleep / wake infrastructure ──────────────────────────────────── */

/*
 * Put the current process to sleep on a wait channel.
 * The process will not run until wakeup() is called on the same channel.
 * Must be called with interrupts enabled (or about to be enabled).
 */
void proc_sleep(void *channel) {
  process_t *proc = process_current();
  if (!proc) {
    return;
  }

  proc->wait_channel = channel;
  proc->state = PROC_STATE_SLEEPING;
 // com1_printf("[EVENT] proc_sleep: pid=%d ch=%p\n", proc->pid, channel); commented because it spam to the uart

  /*
   * Halt with interrupts enabled. hlt wakes on ANY interrupt
   * (timer, keyboard, etc). After waking, check if we were
   * actually woken by proc_wakeup (wait_channel cleared) or
   * just by a spurious timer tick. If not woken, go back to sleep.
   */
  __asm__ volatile("sti");
  while (proc->wait_channel != NULL) {
    __asm__ volatile("hlt");
  }
  __asm__ volatile("cli");

  proc->state = PROC_STATE_RUNNING;
}

/*
 * Wake all processes sleeping on the given wait channel.
 */
void proc_wakeup(void *channel) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_t *p = &process_table[i];
    if (p->state == PROC_STATE_SLEEPING && p->wait_channel == channel) {
      p->state = PROC_STATE_RUNNABLE;
      p->wait_channel = NULL;
     // com1_printf("[EVENT] proc_wakeup: pid=%d\n", p->pid); commented because it spam to the uart
    }
  }
}

/*
 * Wake one process sleeping on the given wait channel (highest priority:
 * first found in process table order).
 */
void proc_wakeup_one(void *channel) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    process_t *p = &process_table[i];
    if (p->state == PROC_STATE_SLEEPING && p->wait_channel == channel) {
      p->state = PROC_STATE_RUNNABLE;
      p->wait_channel = NULL;
      com1_printf("[EVENT] proc_wakeup_one: pid=%d\n", p->pid);
      return;
    }
  }
}

/* ── kqueue lifecycle ─────────────────────────────────────────────── */

void event_init(void) {
  com1_printf("[EVENT] Initializing event subsystem...\n");

  memset(kqueue_pool, 0, sizeof(kqueue_pool));
  memset(filter_table, 0, sizeof(filter_table));

  /* Register all built-in filters */
  extern const filter_ops_t filter_read_ops;
  extern const filter_ops_t filter_write_ops;
  extern const filter_ops_t filter_timer_ops;
  extern const filter_ops_t filter_proc_ops;
  extern const filter_ops_t filter_signal_ops;
  extern const filter_ops_t filter_user_ops;

  filter_register(&filter_read_ops);
  filter_register(&filter_write_ops);
  filter_register(&filter_timer_ops);
  filter_register(&filter_proc_ops);
  filter_register(&filter_signal_ops);
  filter_register(&filter_user_ops);

  event_initialized = 1;
  com1_printf("[EVENT] Event subsystem initialized (%d kqueue slots)\n",
              MAX_KQUEUES);
}

int kqueue_create(void) {
  if (!event_initialized) {
    return -1;
  }

  for (int i = 0; i < MAX_KQUEUES; i++) {
    if (!kqueue_pool[i].used) {
      memset(&kqueue_pool[i], 0, sizeof(kqueue_t));
      kqueue_pool[i].used = 1;
      kqueue_pool[i].owner = process_current();
      kqueue_pool[i].wait_channel = &kqueue_pool[i];
      com1_printf("[EVENT] Created kqueue idx=%d owner_pid=%d\n", i,
                  process_current() ? (int)process_current()->pid : 0);
      return i;
    }
  }

  com1_printf("[EVENT] kqueue_create: no free slots\n");
  return -1;
}

int kqueue_destroy(int kq_idx) {
  if (kq_idx < 0 || kq_idx >= MAX_KQUEUES) {
    return -1;
  }

  kqueue_t *kq = &kqueue_pool[kq_idx];
  if (!kq->used) {
    return -1;
  }

  /* Detach all knotes */
  for (int i = 0; i < MAX_KNOTES; i++) {
    knote_t *kn = &kq->knotes[i];
    if (!kn->used) {
      continue;
    }

    const filter_ops_t *ops = filter_lookup(kn->filter);
    if (ops && ops->detach) {
      ops->detach(kn);
    }

    memset(kn, 0, sizeof(knote_t));
  }

  /* Wake anyone blocked on this kqueue */
  proc_wakeup(kq->wait_channel);

  memset(kq, 0, sizeof(kqueue_t));
  com1_printf("[EVENT] Destroyed kqueue idx=%d\n", kq_idx);
  return 0;
}

kqueue_t *kqueue_get(int kq_idx) {
  if (kq_idx < 0 || kq_idx >= MAX_KQUEUES) {
    return NULL;
  }
  if (!kqueue_pool[kq_idx].used) {
    return NULL;
  }
  return &kqueue_pool[kq_idx];
}

/* ── knote management ─────────────────────────────────────────────── */

static knote_t *knote_find(kqueue_t *kq, u64 ident, s16 filter) {
  for (int i = 0; i < MAX_KNOTES; i++) {
    knote_t *kn = &kq->knotes[i];
    if (kn->used && kn->ident == ident && kn->filter == filter) {
      return kn;
    }
  }
  return NULL;
}

static knote_t *knote_alloc(kqueue_t *kq) {
  for (int i = 0; i < MAX_KNOTES; i++) {
    if (!kq->knotes[i].used) {
      return &kq->knotes[i];
    }
  }
  return NULL;
}

static void knote_add_to_ready(kqueue_t *kq, knote_t *kn) {
  if (kn->pending) {
    return;
  }

  kn->pending = 1;
  kn->next = NULL;

  if (kq->ready_tail) {
    kq->ready_tail->next = kn;
    kq->ready_tail = kn;
  } else {
    kq->ready_head = kn;
    kq->ready_tail = kn;
  }
  kq->ready_count++;
}

static void knote_remove_from_ready(kqueue_t *kq, knote_t *kn) {
  if (!kn->pending) {
    return;
  }

  /* Linear scan to unlink */
  knote_t *prev = NULL;
  knote_t *cur = kq->ready_head;
  while (cur) {
    if (cur == kn) {
      if (prev) {
        prev->next = kn->next;
      } else {
        kq->ready_head = kn->next;
      }
      if (kq->ready_tail == kn) {
        kq->ready_tail = prev;
      }
      kn->next = NULL;
      kn->pending = 0;
      kq->ready_count--;
      return;
    }
    prev = cur;
    cur = cur->next;
  }
}

void knote_ready(knote_t *kn) {
  if (!kn || !kn->used || kn->disabled) {
    return;
  }

  kqueue_t *kq = kn->kq;
  if (!kq || !kq->used) {
    return;
  }

  knote_add_to_ready(kq, kn);
  kqueue_wakeup(kq);
}

void kqueue_wakeup(kqueue_t *kq) {
  if (kq && kq->used) {
    proc_wakeup_one(kq->wait_channel);
  }
}

/*
 * Notify all knotes across all kqueues that match (filter, ident).
 * This is called by kernel subsystems (pipe, tty, proc, etc.) when
 * a state change occurs.
 */
void knote_notify_all(s16 filter, u64 ident, u32 fflags, s64 data) {
  for (int i = 0; i < MAX_KQUEUES; i++) {
    kqueue_t *kq = &kqueue_pool[i];
    if (!kq->used) {
      continue;
    }

    for (int j = 0; j < MAX_KNOTES; j++) {
      knote_t *kn = &kq->knotes[j];
      if (!kn->used || kn->filter != filter || kn->ident != ident) {
        continue;
      }

      /* Update knote data from the notification */
      if (fflags) {
        kn->fflags |= fflags;
      }
      if (data) {
        kn->data = data;
      }

      knote_ready(kn);
    }
  }
}

/* ── kevent processing ────────────────────────────────────────────── */

static int process_change(kqueue_t *kq, struct kevent *kev) {
  const filter_ops_t *ops = filter_lookup(kev->filter);
  if (!ops) {
    com1_printf("[EVENT] unknown filter %d\n", kev->filter);
    return -API_ERR_INVAL;
  }

  /* Handle EV_DELETE */
  if (kev->flags & EV_DELETE) {
    knote_t *kn = knote_find(kq, kev->ident, kev->filter);
    if (!kn) {
      return -API_ERR_NOT_FOUND;
    }

    if (ops->detach) {
      ops->detach(kn);
    }
    knote_remove_from_ready(kq, kn);
    memset(kn, 0, sizeof(knote_t));
    return 0;
  }

  /* Handle EV_ADD or modify existing */
  knote_t *kn = knote_find(kq, kev->ident, kev->filter);

  if (kev->flags & EV_ADD) {
    if (kn) {
      /* Re-add: modify existing knote */
      kn->fflags = kev->fflags;
      kn->data = kev->data;
      if (!(kev->flags & EV_KEEPUDATA)) {
        kn->udata = kev->udata;
      }
      if (ops->touch) {
        ops->touch(kn, kev);
      }
      if (kev->flags & EV_DISABLE) {
        kn->disabled = 1;
        knote_remove_from_ready(kq, kn);
      } else if (kev->flags & EV_ENABLE) {
        kn->disabled = 0;
      }
    } else {
      /* Allocate new knote */
      kn = knote_alloc(kq);
      if (!kn) {
        com1_printf("[EVENT] no free knote slots\n");
        return -API_ERR_NO_MEMORY;
      }

      memset(kn, 0, sizeof(knote_t));
      kn->used = 1;
      kn->ident = kev->ident;
      kn->filter = kev->filter;
      kn->flags = kev->flags;
      kn->fflags = kev->fflags;
      kn->data = kev->data;
      kn->udata = kev->udata;
      kn->kq = kq;

      if (kev->flags & EV_DISABLE) {
        kn->disabled = 1;
      }

      /* Call filter attach */
      if (ops->attach) {
        int ret = ops->attach(kn);
        if (ret != 0) {
          memset(kn, 0, sizeof(knote_t));
          com1_printf("[EVENT] filter attach failed: %d\n", ret);
          return ret;
        }
      }

      com1_printf("[EVENT] added knote ident=%llu filter=%d\n",
                  kev->ident, kev->filter);

      /* Check if condition is already true */
      if (!kn->disabled && ops->event) {
        int pending = ops->event(kn, 0);
        if (pending > 0) {
          knote_ready(kn);
        }
      }
    }
    return 0;
  }

  /* Handle EV_ENABLE / EV_DISABLE on existing knote */
  if (kev->flags & EV_ENABLE) {
    if (!kn) {
      return -API_ERR_NOT_FOUND;
    }
    kn->disabled = 0;
    if (ops->event) {
      int pending = ops->event(kn, 0);
      if (pending > 0) {
        knote_ready(kn);
      }
    }
  }

  if (kev->flags & EV_DISABLE) {
    if (!kn) {
      return -API_ERR_NOT_FOUND;
    }
    kn->disabled = 1;
    knote_remove_from_ready(kq, kn);
  }

  return 0;
}

static int collect_events(kqueue_t *kq, struct kevent *eventlist,
                          int nevents) {
  int count = 0;

  while (kq->ready_head && count < nevents) {
    knote_t *kn = kq->ready_head;

    const filter_ops_t *ops = filter_lookup(kn->filter);
    if (ops && ops->event) {
      /* Ask filter if the condition still holds */
      int result = ops->event(kn, 1);
      if (result <= 0) {
        /* Condition no longer true — remove from ready list */
        knote_remove_from_ready(kq, kn);
        continue;
      }
    }

    /* Fill the eventlist entry */
    eventlist[count].ident = kn->ident;
    eventlist[count].filter = kn->filter;
    eventlist[count].flags = kn->flags;
    eventlist[count].fflags = kn->fflags;
    eventlist[count].data = kn->data;
    eventlist[count].udata = kn->udata;

    /* Preserve EOF flag if set by filter */
    if (kn->fflags & 0x80000000) {
      eventlist[count].flags |= EV_EOF;
    }

    count++;

    /* Handle EV_ONESHOT — delete after delivery */
    if (kn->flags & EV_ONESHOT) {
      if (ops && ops->detach) {
        ops->detach(kn);
      }
      knote_remove_from_ready(kq, kn);
      memset(kn, 0, sizeof(knote_t));
      continue;
    }

    /* Handle EV_CLEAR — reset state after retrieval */
    if (kn->flags & EV_CLEAR) {
      kn->fflags = 0;
      kn->data = 0;
    }

    /* Handle EV_DISPATCH — disable after delivery */
    if (kn->flags & EV_DISPATCH) {
      kn->disabled = 1;
    }

    knote_remove_from_ready(kq, kn);
  }

  return count;
}

int kevent_process(int kq_idx, struct kevent *changelist, int nchanges,
                   struct kevent *eventlist, int nevents, s64 timeout_ms) {
  if (!event_initialized) {
    return -API_ERR_NOT_SUPPORTED;
  }

  kqueue_t *kq = kqueue_get(kq_idx);
  if (!kq) {
    return -API_ERR_BAD_HANDLE;
  }

  /* Process changelist first */
  for (int i = 0; i < nchanges; i++) {
    struct kevent *kev = &changelist[i];
    int ret = process_change(kq, kev);

    if (kev->flags & EV_RECEIPT) {
      /* EV_RECEIPT: put result in eventlist if space */
      if (nevents > 0 && eventlist) {
        eventlist[0].ident = kev->ident;
        eventlist[0].filter = kev->filter;
        eventlist[0].flags = EV_ERROR;
        eventlist[0].data = ret;
        eventlist++;
        nevents--;
      }
    } else if (ret != 0 && (kev->flags & EV_ADD)) {
      /* Error on add: report it in eventlist if space */
      if (nevents > 0 && eventlist) {
        eventlist[0].ident = kev->ident;
        eventlist[0].filter = kev->filter;
        eventlist[0].flags = EV_ERROR;
        eventlist[0].data = ret;
        eventlist++;
        nevents--;
      }
    }
  }

  /* If no events requested, just return 0 */
  if (nevents <= 0) {
    return 0;
  }

  /* Try to collect pending events */
  int count = collect_events(kq, eventlist, nevents);
  if (count > 0) {
    return count;
  }

  /* No pending events — block if timeout allows */
  if (timeout_ms == 0) {
    /* Poll mode — return immediately */
    return 0;
  }

  /* Block until events arrive or timeout */
  u64 start_ticks = timer_get_ticks();
  u64 timeout_ticks = 0;
  if (timeout_ms > 0) {
    timeout_ticks = (u64)timeout_ms * timer_get_frequency() / 1000;
  }

  while (1) {
    /* Check if we have events now */
    if (kq->ready_count > 0) {
      return collect_events(kq, eventlist, nevents);
    }

    /* Check timeout */
    if (timeout_ms > 0) {
      u64 elapsed = timer_get_ticks() - start_ticks;
      if (elapsed >= timeout_ticks) {
        return 0; /* timeout expired */
      }
    }

    /* Sleep on the kqueue's wait channel */
    proc_sleep(kq->wait_channel);
  }

  return 0; /* unreachable */
}

/* ── Timer tick handler ───────────────────────────────────────────── */

void event_timer_tick(void) {
  if (!event_initialized) {
    return;
  }

  /* Let the timer filter check all timer knotes */
  extern void filter_timer_tick(void);
  filter_timer_tick();
}

/* ── Process cleanup ──────────────────────────────────────────────── */

void event_cleanup_process(struct process *proc) {
  if (!proc || !event_initialized) {
    return;
  }

  for (int i = 0; i < MAX_KQUEUES; i++) {
    kqueue_t *kq = &kqueue_pool[i];
    if (kq->used && kq->owner == proc) {
      kqueue_destroy(i);
    }
  }

  /* Also remove any knotes on other kqueues that reference this PID */
  if (proc->pid != 0) {
    for (int i = 0; i < MAX_KQUEUES; i++) {
      kqueue_t *kq = &kqueue_pool[i];
      if (!kq->used) {
        continue;
      }
      for (int j = 0; j < MAX_KNOTES; j++) {
        knote_t *kn = &kq->knotes[j];
        if (kn->used && kn->filter == EVFILT_PROC && kn->ident == proc->pid) {
          const filter_ops_t *ops = filter_lookup(kn->filter);
          if (ops && ops->detach) {
            ops->detach(kn);
          }
          knote_remove_from_ready(kq, kn);
          memset(kn, 0, sizeof(knote_t));
        }
      }
    }
  }
}

void event_fork_process(struct process *parent, struct process *child) {
  if (!parent || !child || !event_initialized) {
    return;
  }

  /*
   * FreeBSD behaviour: kqueues are NOT inherited by fork by default.
   * The child gets an empty set. The parent retains all kqueues.
   * This matches the default (no KQUEUE_CPONFORK) behavior.
   */
  (void)parent;
  (void)child;
}

/* ── Pipe change notification ─────────────────────────────────────── */

/*
 * Called when a pipe's state changes (data written or read).
 * Finds all EVFILT_READ/EVFILT_WRITE knotes whose ident is a handle
 * pointing to the given pipe, and marks them as ready.
 */
void event_notify_pipe_change(pipe_t *p) {
  if (!event_initialized || !p) {
    return;
  }

  api_object_t *objects = api_get_object_table();
  if (!objects) {
    return;
  }

  /* Find object indices that reference this pipe */
  for (int obj_idx = 0; obj_idx < MAX_DATA_OBJECTS; obj_idx++) {
    if (!objects[obj_idx].used || objects[obj_idx].type != API_OBJECT_PIPE) {
      continue;
    }
    if (objects[obj_idx].pipe != p) {
      continue;
    }

    /* Find handles pointing to this object */
    for (int pid_slot = 0; pid_slot < MAX_PROCESSES; pid_slot++) {
      process_t *proc = &process_table[pid_slot];
      if (proc->state == PROC_STATE_UNUSED) {
        continue;
      }

      for (int fd = 0; fd < MAX_HANDLES; fd++) {
        if (!proc->handles[fd].used ||
            proc->handles[fd].object_index != obj_idx) {
          continue;
        }

        /* Notify both READ and WRITE filters for this fd */
        knote_notify_all(EVFILT_READ, (u64)fd, 0, 0);
        knote_notify_all(EVFILT_WRITE, (u64)fd, 0, 0);
      }
    }
  }
}
