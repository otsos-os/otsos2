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

/* !DEFINES!

$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type kqueue_t as struct with event queue, knote pool, ready list
$define %type knote_t as struct with registered event state
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type process_t as struct with process control block
$define %type pipe_t as struct with pipe ring buffer
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type net_endpoint_t as native network endpoint state
$define %type ipc_endpoint_t as native IPC endpoint state

$define %func filter_index as function with args s16
$define %func filter_register as procedure with args const filter_ops_t *
$define %func filter_lookup as function with args s16
$define %func proc_sleep as procedure with args void *
$define %func proc_sleep_interlock as procedure with args void *, spin_t *
$define %func proc_sleep_wait as procedure with args thread_t *
$define %func proc_wakeup as procedure with args void *
$define %func proc_wakeup_one as procedure with args void *
$define %func event_init as procedure with args void
$define %func event_lock as procedure with args void
$define %func event_unlock as procedure with args void
$define %func kqueue_create as function with args void
$define %func kqueue_destroy as function with args int
$define %func kqueue_get as function with args int
$define %func knote_find as function with args kqueue_t *, u64, s16
$define %func knote_alloc as function with args kqueue_t *
$define %func knote_add_to_ready as procedure with args kqueue_t *, knote_t *
$define %func knote_remove_from_ready as procedure with args kqueue_t *, knote_t *
$define %func knote_ready as procedure with args knote_t *
$define %func kqueue_wakeup as procedure with args kqueue_t *
$define %func knote_notify_all as procedure with args s16, u64, u32, s64
$define %func process_change as function with args kqueue_t *, struct kevent *
$define %func collect_events as function with args kqueue_t *, struct kevent *, int
$define %func kevent_process as function with args int, struct kevent *, int, struct kevent *, int, s64
$define %func event_timer_tick as procedure with args void
$define %func event_cleanup_process as procedure with args struct process *
$define %func event_fork_process as procedure with args struct process *, struct process *
$define %func event_notify_pipe_change as procedure with args pipe_t *
$define %func event_notify_net_change as procedure with args net_endpoint_t *
$define %func event_notify_ipc_change as procedure with args ipc_endpoint_t *

*/

/* !SPACE!

$space %internal filter_index, knote_find, knote_alloc
$space %internal knote_add_to_ready, knote_remove_from_ready
$space %internal process_change, collect_events
$space %internal proc_sleep_wait, kqueue_destroy_locked
$space %export filter_register, filter_lookup
$space %export proc_sleep, proc_sleep_interlock
$space %export proc_wakeup, proc_wakeup_one
$space %export event_lock, event_unlock, event_spin
$space %export knote_ready_locked, kqueue_wakeup_locked
$space %export event_init, kqueue_create, kqueue_destroy
$space %export kqueue_get, knote_ready, kqueue_wakeup
$space %export knote_notify_all, kevent_process
$space %export event_timer_tick, event_cleanup_process
$space %export event_fork_process, event_notify_pipe_change
$space %export event_notify_net_change, event_notify_ipc_change

*/

#include <kernel/event/event.h>
#include <kernel/api/api.h>
#include <kernel/ipc/ipc.h>
#include <kernel/interrupts/irq.h>
#include <kernel/drivers/timer.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <kernel/panic.h>
#include <kernel/trace/trace.h>
#include <mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static kqueue_t			kqueue_pool[MAX_KQUEUES];
static int			event_initialized;
static const filter_ops_t	*filter_table[EVFILT_SYSCOUNT];
static spin_t			event_spin =
				    SPIN_INITIALIZER("event", LO_EVENT);

static int
filter_index(s16 filter)
{
	int	idx;

	idx = -filter - 1;
	if (idx < 0 || idx >= EVFILT_SYSCOUNT) {
		return (-1);
	}
	return (idx);
}

void
filter_register(const filter_ops_t *ops)
{
	int	idx;

	idx = filter_index(ops->filter);
	if (idx < 0) {
		printk("[EVENT] filter_register: invalid "
		    "filter %d\n", ops->filter);
		return;
	}
	filter_table[idx] = ops;
	printk("[EVENT] registered filter '%s' "
	    "(id=%d)\n", ops->name, ops->filter);
}

const filter_ops_t *
filter_lookup(s16 filter)
{
	int	idx;

	idx = filter_index(filter);
	if (idx < 0) {
		return (NULL);
	}
	return (filter_table[idx]);
}

void
event_lock(void)
{
	spin_lock(&event_spin);
}

void
event_unlock(void)
{
	spin_unlock(&event_spin);
}

static void
proc_sleep_wait(thread_t *td)
{
	while (__atomic_load_n(&td->wait_channel, __ATOMIC_ACQUIRE) != NULL) {
		__asm__ volatile("sti");
		__asm__ volatile("int %0" :: "i"(IRQ_VECTOR_YIELD)
		    : "memory");
		__asm__ volatile("cli");
		if (__atomic_load_n(&td->wait_channel,
		    __ATOMIC_ACQUIRE) == NULL) {
			break;
		}
		__asm__ volatile("sti; hlt; cli");
	}
}

void
proc_sleep_interlock(void *channel, spin_t *interlock)
{
	thread_t	*td;
	u64		flags;
	u32		depth;

	td = thread_current();
	if (!td) {
		if (interlock != NULL) {
			spin_unlock(interlock);
		}
		return;
	}

	__atomic_store_n(&td->wait_channel, channel, __ATOMIC_RELEASE);
	thread_state_set(td, PROC_STATE_SLEEPING);

	if (interlock != NULL) {
		flags = spin_unlock_nocli(interlock);
	} else {
		__asm__ volatile("pushfq; pop %0; cli"
		    : "=r"(flags) :: "memory");
	}

	depth = smp_lock_release_all();
	witness_check_sleep("proc_sleep");
	proc_sleep_wait(td);
	if (depth != 0) {
		smp_lock_acquire_depth(depth);
	}
	thread_state_set(td, PROC_STATE_RUNNING);
	spin_flags_restore(flags);
}

void
proc_sleep(void *channel)
{
	proc_sleep_interlock(channel, NULL);
}

void
proc_wakeup(void *channel)
{
	int		i;
	thread_t	*td;

	thread_lock();
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (td->used &&
		    thread_state_get(td) == PROC_STATE_SLEEPING &&
		    __atomic_load_n(&td->wait_channel,
		    __ATOMIC_ACQUIRE) == channel) {
			thread_state_set(td, PROC_STATE_RUNNABLE);
			__atomic_store_n(&td->wait_channel, NULL,
			    __ATOMIC_RELEASE);
		}
	}
	thread_unlock();
}

void
proc_wakeup_one(void *channel)
{
	int		i;
	thread_t	*td;

	thread_lock();
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (td->used &&
		    thread_state_get(td) == PROC_STATE_SLEEPING &&
		    __atomic_load_n(&td->wait_channel,
		    __ATOMIC_ACQUIRE) == channel) {
			thread_state_set(td, PROC_STATE_RUNNABLE);
			__atomic_store_n(&td->wait_channel, NULL,
			    __ATOMIC_RELEASE);
			break;
		}
	}
	thread_unlock();
}

void
event_init(void)
{
	printk("[EVENT] Initializing event subsystem...\n");

	memset(kqueue_pool, 0, sizeof(kqueue_pool));
	memset(filter_table, 0, sizeof(filter_table));

	{
		extern const filter_ops_t filter_read_ops;
		extern const filter_ops_t filter_write_ops;
		extern const filter_ops_t filter_timer_ops;
		extern const filter_ops_t filter_proc_ops;
		extern const filter_ops_t filter_signal_ops;
		extern const filter_ops_t filter_user_ops;
		extern const filter_ops_t filter_kbd_ops;
		extern const filter_ops_t filter_ipc_ops;
		extern const filter_ops_t filter_input_ops;
		extern const filter_ops_t filter_entity_ops;
		extern const filter_ops_t filter_power_ops;

		filter_register(&filter_read_ops);
		filter_register(&filter_write_ops);
		filter_register(&filter_timer_ops);
		filter_register(&filter_proc_ops);
		filter_register(&filter_signal_ops);
		filter_register(&filter_user_ops);
		filter_register(&filter_kbd_ops);
		filter_register(&filter_ipc_ops);
		filter_register(&filter_input_ops);
		filter_register(&filter_entity_ops);
		filter_register(&filter_power_ops);
	}

	entity_event_set_notify(event_notify_entity);
	event_initialized = 1;
	printk("[EVENT] Event subsystem initialized "
	    "(%d kqueue slots)\n", MAX_KQUEUES);
}

int
kqueue_create(void)
{
	process_t	*owner;
	entity_id_t	id;
	int		i, slot, handle;

	if (!event_initialized) {
		return (-1);
	}

	owner = process_current();
	slot = -1;
	spin_lock(&event_spin);
	for (i = 0; i < MAX_KQUEUES; i++) {
		if (!kqueue_pool[i].used) {
			memset(&kqueue_pool[i], 0, sizeof(kqueue_t));
			kqueue_pool[i].used = 1;
			kqueue_pool[i].owner = owner;
			kqueue_pool[i].wait_channel = &kqueue_pool[i];
			slot = i;
			break;
		}
	}
	spin_unlock(&event_spin);

	if (slot < 0) {
		printk("[EVENT] kqueue_create: no free slots\n");
		return (-1);
	}

	id = entity_io_create_raw(ENTITY_ARCH_KQUEUE, 0);
	if (id == 0) {
		spin_lock(&event_spin);
		kqueue_pool[slot].used = 0;
		spin_unlock(&event_spin);
		return (-1);
	}
	entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING, &kqueue_pool[slot]);
	handle = entity_io_attach(id,
	    ENTITY_ACCESS_READ | ENTITY_ACCESS_WRITE);
	if (handle < 0) {
		spin_lock(&event_spin);
		kqueue_pool[slot].used = 0;
		spin_unlock(&event_spin);
		entity_destroy(id);
		return (handle);
	}

	spin_lock(&event_spin);
	kqueue_pool[slot].entity = id;
	kqueue_pool[slot].entity_handle = handle;
	spin_unlock(&event_spin);

	printk("[EVENT] Created kqueue idx=%d owner_pid=%d\n", slot,
	    owner ? (int)owner->pid : 0);
	trace_kqueue_create(slot, owner ? owner->pid : 0);
	return (handle);
}

static int
kqueue_destroy_locked(int kq_idx, int force)
{
	kqueue_t		*kq;
	int			i;
	const filter_ops_t	*ops;
	process_t		*owner;

	if (kq_idx < 0 || kq_idx >= MAX_KQUEUES) {
		return (-1);
	}

	kq = &kqueue_pool[kq_idx];
	if (!kq->used) {
		return (-1);
	}
	owner = process_current();
	if (!force && owner && kq->owner != owner) {
		return (-API_ERR_PERM);
	}

	for (i = 0; i < MAX_KNOTES; i++) {
		knote_t	*kn;

		kn = &kq->knotes[i];
		if (!kn->used) {
			continue;
		}

		ops = filter_lookup(kn->filter);
		if (ops && ops->detach) {
			ops->detach(kn);
		}
		memset(kn, 0, sizeof(knote_t));
	}

	proc_wakeup(kq->wait_channel);
	trace_kqueue_destroy(kq_idx);

	memset(kq, 0, sizeof(kqueue_t));
	printk("[EVENT] Destroyed kqueue idx=%d\n", kq_idx);
	return (0);
}

int
kqueue_destroy(int kq_idx)
{
	process_t	*owner;
	kqueue_t	*kq;

	kq = kqueue_get(kq_idx);
	if (!kq) {
		return (-1);
	}
	owner = process_current();
	if (owner && kq->owner != owner) {
		return (-API_ERR_PERM);
	}
	return (entity_handle_free(owner, kq_idx));
}

kqueue_t *
kqueue_get(int kq_idx)
{
	process_t	*proc;
	entity_id_t	id;
	kqueue_t	*kq;
	u32		access;
	int		ret;

	proc = process_current();
	ret = entity_handle_lookup(proc, kq_idx, &id, &access);
	if (ret != 0) {
		return (NULL);
	}
	if (entity_arch(id) != ENTITY_ARCH_KQUEUE) {
		return (NULL);
	}
	kq = (kqueue_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (!kq || !kq->used) {
		return (NULL);
	}
	return (kq);
}

void
kqueue_entity_release(entity_id_t entity)
{
	kqueue_t	*kq;
	int		idx;

	kq = (kqueue_t *)entity_io_ptr(entity, ENTITY_IO_PTR_BACKING);
	if (!kq) {
		return;
	}
	idx = (int)(kq - kqueue_pool);
	if (idx < 0 || idx >= MAX_KQUEUES) {
		return;
	}
	spin_lock(&event_spin);
	if (kqueue_pool[idx].used) {
		kqueue_destroy_locked(idx, 1);
	}
	spin_unlock(&event_spin);
}

static knote_t *
knote_find(kqueue_t *kq, u64 ident, s16 filter)
{
	int	i;

	for (i = 0; i < MAX_KNOTES; i++) {
		knote_t	*kn;

		kn = &kq->knotes[i];
		if (kn->used && kn->ident == ident &&
		    kn->filter == filter) {
			return (kn);
		}
	}
	return (NULL);
}

static knote_t *
knote_alloc(kqueue_t *kq)
{
	int	i;

	for (i = 0; i < MAX_KNOTES; i++) {
		if (!kq->knotes[i].used) {
			return (&kq->knotes[i]);
		}
	}
	return (NULL);
}

static void
knote_add_to_ready(kqueue_t *kq, knote_t *kn)
{
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

static void
knote_remove_from_ready(kqueue_t *kq, knote_t *kn)
{
	knote_t	*prev, *cur;

	if (!kn->pending) {
		return;
	}

	prev = NULL;
	cur = kq->ready_head;
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

void
knote_ready_locked(knote_t *kn)
{
	kqueue_t	*kq;

	if (!kn || !kn->used || kn->disabled) {
		return;
	}

	kq = kn->kq;
	if (!kq || !kq->used) {
		return;
	}

	knote_add_to_ready(kq, kn);
	kqueue_wakeup_locked(kq);
	trace_knote_ready(kn->filter, kn->ident, kn->data);
}

void
knote_ready(knote_t *kn)
{
	spin_lock(&event_spin);
	knote_ready_locked(kn);
	spin_unlock(&event_spin);
}

void
kqueue_wakeup_locked(kqueue_t *kq)
{
	if (kq && kq->used) {
		proc_wakeup_one(kq->wait_channel);
	}
}

void
kqueue_wakeup(kqueue_t *kq)
{
	spin_lock(&event_spin);
	kqueue_wakeup_locked(kq);
	spin_unlock(&event_spin);
}

void
knote_notify_all(s16 filter, u64 ident, u32 fflags, s64 data)
{
	int		locked;
	int		i, j;

	locked = spin_owned(&event_spin);
	if (!locked) {
		spin_lock(&event_spin);
	}
	for (i = 0; i < MAX_KQUEUES; i++) {
		kqueue_t	*kq;
		knote_t		*kn;

		kq = &kqueue_pool[i];
		if (!kq->used) {
			continue;
		}

		for (j = 0; j < MAX_KNOTES; j++) {
			kn = &kq->knotes[j];
			if (!kn->used ||
			    kn->filter != filter ||
			    kn->ident != ident) {
				continue;
			}

			if (fflags) {
				if (kn->sfflags != 0 &&
				    (kn->sfflags & fflags) == 0) {
					continue;
				}
				kn->fflags |= (kn->sfflags != 0) ?
				    (fflags & kn->sfflags) : fflags;
			}
			if (data) {
				kn->data = data;
			}

			knote_ready_locked(kn);
		}
	}
	if (!locked) {
		spin_unlock(&event_spin);
	}
}

static int
process_change(kqueue_t *kq, struct kevent *kev)
{
	const filter_ops_t	*ops;
	knote_t			*kn;
	int			ret, pending;

	ops = filter_lookup(kev->filter);
	if (!ops) {
		printk("[EVENT] unknown filter %d\n",
		    kev->filter);
		return (-API_ERR_INVAL);
	}

	if (kev->flags & EV_DELETE) {
		kn = knote_find(kq, kev->ident, kev->filter);
		if (!kn) {
			return (-API_ERR_NOT_FOUND);
		}
		if (ops->detach) {
			ops->detach(kn);
		}
		knote_remove_from_ready(kq, kn);
		memset(kn, 0, sizeof(knote_t));
		return (0);
	}

	kn = knote_find(kq, kev->ident, kev->filter);

	if (kev->flags & EV_ADD) {
		if (kn) {
			kn->fflags = kev->fflags;
			kn->sfflags = kev->fflags;
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
			kn = knote_alloc(kq);
			if (!kn) {
				printk("[EVENT] no free knote "
				    "slots\n");
				return (-API_ERR_NO_MEMORY);
			}

			memset(kn, 0, sizeof(knote_t));
			kn->used = 1;
			kn->ident = kev->ident;
			kn->filter = kev->filter;
			kn->flags = kev->flags;
			kn->fflags = kev->fflags;
			kn->sfflags = kev->fflags;
			kn->data = kev->data;
			kn->udata = kev->udata;
			kn->kq = kq;

			if (kev->flags & EV_DISABLE) {
				kn->disabled = 1;
			}

			if (ops->attach) {
				ret = ops->attach(kn);
				if (ret != 0) {
					memset(kn, 0,
					    sizeof(knote_t));
					printk("[EVENT] filter "
					    "attach failed: %d\n",
					    ret);
					return (ret);
				}
			}

			/*printk("[EVENT] added knote "
			    "ident=%llu filter=%d\n",
			    kev->ident, kev->filter);*/

			if (!kn->disabled && ops->event) {
				pending = ops->event(kn, 0);
				if (pending > 0) {
					knote_ready_locked(kn);
				}
			}
		}
		return (0);
	}

	if (kev->flags & EV_ENABLE) {
		if (!kn) {
			return (-API_ERR_NOT_FOUND);
		}
		kn->disabled = 0;
		if (ops->event) {
			pending = ops->event(kn, 0);
			if (pending > 0) {
				knote_ready_locked(kn);
			}
		}
	}

	if (kev->flags & EV_DISABLE) {
		if (!kn) {
			return (-API_ERR_NOT_FOUND);
		}
		kn->disabled = 1;
		knote_remove_from_ready(kq, kn);
	}

	return (0);
}

static int
collect_events(kqueue_t *kq, struct kevent *eventlist, int nevents)
{
	int			count;
	knote_t			*kn;
	const filter_ops_t	*ops;
	int			result, requeue;

	count = 0;

	while (kq->ready_head && count < nevents) {
		kn = kq->ready_head;
		requeue = 0;

		ops = filter_lookup(kn->filter);
		if (ops && ops->event) {
			result = ops->event(kn, 1);
			if (result <= 0) {
				knote_remove_from_ready(kq, kn);
				continue;
			}
			if (result > 1) {
				requeue = 1;
			}
		}

		eventlist[count].ident = kn->ident;
		eventlist[count].filter = kn->filter;
		eventlist[count].flags = kn->flags;
		eventlist[count].fflags = kn->fflags;
		eventlist[count].data = kn->data;
		eventlist[count].udata = kn->udata;
		eventlist[count].input = kn->input;

		if (kn->filter == EVFILT_PROC && (kn->fflags & NOTE_EXIT)) {
			eventlist[count].flags |= EV_EOF;
		}

		count++;

		if (kn->flags & EV_ONESHOT) {
			if (ops && ops->detach) {
				ops->detach(kn);
			}
			knote_remove_from_ready(kq, kn);
			memset(kn, 0, sizeof(knote_t));
			continue;
		}

		if (kn->flags & EV_CLEAR) {
			kn->fflags = 0;
			kn->data = 0;
			memset(&kn->input, 0, sizeof(kn->input));
		}

		if (kn->flags & EV_DISPATCH) {
			kn->disabled = 1;
			requeue = 0;
		}

		knote_remove_from_ready(kq, kn);
		if (requeue && !kn->disabled) {
			knote_ready_locked(kn);
		}
	}

	return (count);
}

int
kevent_process(int kq_idx, struct kevent *changelist,
    int nchanges, struct kevent *eventlist, int nevents,
    s64 timeout_ms)
{
	kqueue_t	*kq;
	process_t	*owner;
	thread_t	*td;
	int		i, count, ret;
	u64		start_ticks, timeout_ticks, elapsed, now_ticks;
	u64		remaining_ticks;
	u32		frequency;

	if (!event_initialized) {
		return (-API_ERR_NOT_SUPPORTED);
	}

	kq = kqueue_get(kq_idx);
	if (!kq) {
		return (-API_ERR_BAD_HANDLE);
	}
	owner = process_current();
	if (owner && kq->owner != owner) {
		return (-API_ERR_PERM);
	}
	td = thread_current();
	trace_kevent_wait(kq_idx, nchanges, nevents, timeout_ms);

	spin_lock(&event_spin);
	for (i = 0; i < nchanges; i++) {
		struct kevent	*kev;

		kev = &changelist[i];
		ret = process_change(kq, kev);

		if (kev->flags & EV_RECEIPT) {
			if (nevents > 0 && eventlist) {
				eventlist[0].ident = kev->ident;
				eventlist[0].filter = kev->filter;
				eventlist[0].flags = EV_ERROR;
				eventlist[0].data = ret;
				eventlist++;
				nevents--;
			}
		} else if (ret != 0 && (kev->flags & EV_ADD)) {
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

	if (nevents <= 0) {
		spin_unlock(&event_spin);
		trace_kevent_return(kq_idx, 0, timeout_ms);
		return (0);
	}

	count = collect_events(kq, eventlist, nevents);
	if (count > 0) {
		spin_unlock(&event_spin);
		trace_kevent_return(kq_idx, count, timeout_ms);
		return (count);
	}

	if (timeout_ms == 0) {
		spin_unlock(&event_spin);
		trace_kevent_return(kq_idx, 0, timeout_ms);
		return (0);
	}

	start_ticks = timer_get_ticks();
	timeout_ticks = 0;
	if (timeout_ms > 0) {
		frequency = timer_get_frequency();
		if (frequency == 0) {
			frequency = 1000;
		}
		timeout_ticks = ((u64)timeout_ms * frequency + 999) /
		    1000;
		if (timeout_ticks == 0) {
			timeout_ticks = 1;
		}
	}

	while (1) {
		if (!kq->used) {
			spin_unlock(&event_spin);
			trace_kevent_return(kq_idx, 0, timeout_ms);
			return (-API_ERR_BAD_HANDLE);
		}

		if (kq->ready_count > 0) {
			count = collect_events(kq, eventlist,
			    nevents);
			spin_unlock(&event_spin);
			trace_kevent_return(kq_idx, count, timeout_ms);
			return (count);
		}

		if (timeout_ms > 0) {
			elapsed = timer_get_ticks() - start_ticks;
			if (elapsed >= timeout_ticks) {
				spin_unlock(&event_spin);
				trace_kevent_return(kq_idx, 0,
				    timeout_ms);
				return (0);
			}
		}

		if (timeout_ms > 0 && td != NULL) {
			now_ticks = timer_get_ticks();
			elapsed = now_ticks - start_ticks;
			if (elapsed >= timeout_ticks) {
				spin_unlock(&event_spin);
				trace_kevent_return(kq_idx, 0, timeout_ms);
				return (0);
			}
			remaining_ticks = timeout_ticks - elapsed;
			td->sleep_target_ticks = now_ticks + remaining_ticks;
		}
		proc_sleep_interlock(kq->wait_channel, &event_spin);
		if (td != NULL) {
			td->sleep_target_ticks = 0;
		}
		spin_lock(&event_spin);
	}
}

void
event_timer_tick(void)
{
	if (!event_initialized) {
		return;
	}
	trace_event_timer_tick();

	{
		extern void filter_timer_tick(void);
		filter_timer_tick();
	}
	ipc_timer_tick();
}

void
event_cleanup_process(struct process *proc)
{
	int		i, j;
	kqueue_t	*kq;

	if (!proc || !event_initialized) {
		return;
	}

	spin_lock(&event_spin);
	for (i = 0; i < MAX_KQUEUES; i++) {
		kq = &kqueue_pool[i];
		if (kq->used && kq->owner == proc) {
			kqueue_destroy_locked(i, 1);
		}
	}

	if (proc->pid != 0) {
		for (i = 0; i < MAX_KQUEUES; i++) {
			kq = &kqueue_pool[i];
			if (!kq->used) {
				continue;
			}
			for (j = 0; j < MAX_KNOTES; j++) {
				knote_t			*kn;
				const filter_ops_t	*ops;

				kn = &kq->knotes[j];
				if (!kn->used ||
				    kn->filter != EVFILT_PROC ||
				    kn->ident != proc->pid) {
					continue;
				}
				ops = filter_lookup(kn->filter);
				if (ops && ops->detach) {
					ops->detach(kn);
				}
			}
		}
	}
	spin_unlock(&event_spin);
}

void
event_fork_process(struct process *parent, struct process *child)
{
	if (!parent || !child || !event_initialized) {
		return;
	}

	(void)parent;
	(void)child;
}

struct event_entity_ctx {
	u16	arch;
	void	*ptr;
	s16	filter;
	u32	fflags;
};

static int
event_entity_notify_cb(int handle, entity_id_t id, u32 access, void *ctx)
{
	struct event_entity_ctx	*ec;

	(void)access;
	ec = (struct event_entity_ctx *)ctx;
	if (entity_arch(id) != ec->arch) {
		return (0);
	}
	if (entity_io_ptr(id, ENTITY_IO_PTR_BACKING) != ec->ptr) {
		return (0);
	}
	knote_notify_all(ec->filter, (u64)handle, ec->fflags, 0);
	return (0);
}

static void
event_entity_notify(u16 arch, void *ptr, s16 filter, u32 fflags)
{
	struct event_entity_ctx	ec;
	int			i;

	if (!event_initialized || !ptr) {
		return;
	}
	ec.arch = arch;
	ec.ptr = ptr;
	ec.filter = filter;
	ec.fflags = fflags;
	for (i = 0; i < MAX_PROCESSES; i++) {
		process_t	*proc;

		proc = &process_table[i];
		if (proc->pid == 0) {
			continue;
		}
		entity_handle_foreach(proc, event_entity_notify_cb, &ec);
	}
}

void
event_notify_pipe_change(pipe_t *p)
{
	event_entity_notify(ENTITY_ARCH_PIPE, p, EVFILT_READ, 0);
	event_entity_notify(ENTITY_ARCH_PIPE, p, EVFILT_WRITE, 0);
}

void
event_notify_net_change(struct net_endpoint *ep)
{
	event_entity_notify(ENTITY_ARCH_NET, ep, EVFILT_READ, 0);
	event_entity_notify(ENTITY_ARCH_NET, ep, EVFILT_WRITE, 0);
}

void
event_notify_ipc_change(struct ipc_endpoint *endpoint)
{
	event_entity_notify(ENTITY_ARCH_IPC, endpoint, EVFILT_IPC,
	    NOTE_IPC_ALL);
}

struct event_entity_id_ctx {
	entity_id_t	id;
	u32		fflags;
};

static int
event_entity_id_cb(int handle, entity_id_t id, u32 access, void *ctx)
{
	struct event_entity_id_ctx	*ec;

	(void)access;
	ec = (struct event_entity_id_ctx *)ctx;
	if (id != ec->id) {
		return (0);
	}
	knote_notify_all(EVFILT_ENTITY, (u64)handle, ec->fflags, 0);
	return (0);
}

void
event_notify_entity(u64 entity, u32 fflags)
{
	struct event_entity_id_ctx	ec;
	int				i;

	if (!event_initialized) {
		return;
	}
	ec.id = (entity_id_t)entity;
	ec.fflags = fflags;
	for (i = 0; i < MAX_PROCESSES; i++) {
		process_t	*proc;

		proc = &process_table[i];
		if (proc->pid == 0) {
			continue;
		}
		entity_handle_foreach(proc, event_entity_id_cb, &ec);
	}
}
