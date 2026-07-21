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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type kevent as struct with ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type knote_t as struct with registered event state
$define %type kqueue_t as struct with event queue, knote pool, ready list
$define %type process as struct with process control block
$define %type pipe as struct with pipe ring buffer
$define %type net_endpoint_t as native network endpoint state
$define %type ipc_endpoint_t as native IPC endpoint state

$define %func event_init as procedure with args void
$define %func kqueue_create as function with args void
$define %func kqueue_destroy as function with args int
$define %func kqueue_get as function with args int
$define %func kevent_process as function with args int, struct kevent *, int, struct kevent *, int, s64
$define %func knote_ready as procedure with args knote_t *
$define %func knote_notify_all as procedure with args s16, u64, u32, s64
$define %func kqueue_wakeup as procedure with args kqueue_t *
$define %func filter_register as procedure with args const filter_ops_t *
$define %func filter_lookup as function with args s16
$define %func event_timer_tick as procedure with args void
$define %func event_cleanup_process as procedure with args struct process *
$define %func event_fork_process as procedure with args struct process *, struct process *
$define %func event_notify_proc_exit as procedure with args u32, int
$define %func event_notify_proc_fork as procedure with args u32, u32
$define %func event_notify_signal as procedure with args u32, int
$define %func event_notify_pipe_change as procedure with args struct pipe *
$define %func event_notify_net_change as procedure with args net_endpoint_t *
$define %func event_notify_ipc_change as procedure with args ipc_endpoint_t *

*/

/* !SPACE!

$space %export event_init, kqueue_create, kqueue_destroy
$space %export kqueue_get, kevent_process, knote_ready
$space %export knote_notify_all, kqueue_wakeup
$space %export filter_register, filter_lookup
$space %export event_timer_tick, event_cleanup_process
$space %export event_fork_process
$space %export event_notify_proc_exit, event_notify_proc_fork
$space %export event_notify_signal, event_notify_pipe_change
$space %export event_notify_net_change, event_notify_ipc_change

*/

#ifndef KERNEL_EVENT_H
#define KERNEL_EVENT_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define	EVFILT_READ	(-1)
#define	EVFILT_WRITE	(-2)
#define	EVFILT_TIMER	(-3)
#define	EVFILT_PROC	(-4)
#define	EVFILT_SIGNAL	(-5)
#define	EVFILT_USER	(-6)
#define	EVFILT_KBD	(-7)
#define	EVFILT_MOUSE	(-8)
#define	EVFILT_IPC	(-9)

#define	EVFILT_SYSCOUNT	9

#define	EV_ADD		0x0001
#define	EV_DELETE	0x0002
#define	EV_ENABLE	0x0004
#define	EV_DISABLE	0x0008
#define	EV_ONESHOT	0x0010
#define	EV_CLEAR	0x0020
#define	EV_RECEIPT	0x0040
#define	EV_DISPATCH	0x0080
#define	EV_EOF		0x8000
#define	EV_ERROR	0x4000
#define	EV_KEEPUDATA	0x2000

#define	NOTE_EXIT	0x80000000
#define	NOTE_FORK	0x40000000
#define	NOTE_EXEC	0x20000000
#define	NOTE_TRACK	0x00000001
#define	NOTE_CHILD	0x00000002
#define	NOTE_TRACKERR	0x00000004

#define	NOTE_SECONDS	0x00000001
#define	NOTE_MSECONDS	0x00000002
#define	NOTE_USECONDS	0x00000004
#define	NOTE_NSECONDS	0x00000008

#define	NOTE_FFNOP	0x00000000
#define	NOTE_FFAND	0x40000000
#define	NOTE_FFOR	0x80000000
#define	NOTE_FFCOPY	0xC0000000
#define	NOTE_FFCTRLMASK	0xC0000000
#define	NOTE_FFLAGSMASK	0x00FFFFFF
#define	NOTE_TRIGGER	0x01000000

#define	NOTE_LOWAT	0x00000001

#define	NOTE_IPC_READ	0x00000001
#define	NOTE_IPC_WRITE	0x00000002
#define	NOTE_IPC_HUP	0x00000004
#define	NOTE_IPC_PEER	0x00000008
#define	NOTE_IPC_ALL	0x0000000F

#define	MAX_KQUEUES	32
#define	MAX_KNOTES	64
#define	MAX_KEVENTS	64

struct kevent {
	u64	ident;
	s16	filter;
	u16	flags;
	u32	fflags;
	s64	data;
	u64	udata;
};

#define	EV_SET(kevp, id, filt, fl, ffl, d, ud)			\
	do {							\
		(kevp)->ident  = (u64)(id);			\
		(kevp)->filter = (s16)(filt);			\
		(kevp)->flags  = (u16)(fl);			\
		(kevp)->fflags = (u32)(ffl);			\
		(kevp)->data   = (s64)(d);			\
		(kevp)->udata  = (u64)(ud);			\
	} while (0)

struct kqueue;
struct knote;
struct process;
struct thread;
struct pipe;
struct net_endpoint;
struct ipc_endpoint;

typedef int	(*filter_attach_fn)(struct knote *kn);
typedef void	(*filter_detach_fn)(struct knote *kn);
typedef int	(*filter_event_fn)(struct knote *kn, u32 nevents);
typedef void	(*filter_touch_fn)(struct knote *kn,
    struct kevent *kev);

typedef struct {
	s16			filter;
	const char		*name;
	filter_attach_fn	 attach;
	filter_detach_fn	 detach;
	filter_event_fn		 event;
	filter_touch_fn		 touch;
} filter_ops_t;

typedef struct knote {
	int			used;
	int			pending;
	int			disabled;
	u64			ident;
	s16			filter;
	u16			flags;
	u32			fflags;
	s64			data;
	u64			udata;
	u64			fpriv;
	struct kqueue		*kq;
	struct knote		*next;
} knote_t;

typedef struct kqueue {
	int			used;
	struct process		*owner;
	knote_t			knotes[MAX_KNOTES];
	knote_t			*ready_head;
	knote_t			*ready_tail;
	int			ready_count;
	void			*wait_channel;
} kqueue_t;

void	event_init(void);
int	kqueue_create(void);
int	kqueue_destroy(int kq_idx);
kqueue_t *kqueue_get(int kq_idx);
int	kevent_process(int kq_idx, struct kevent *changelist,
    int nchanges, struct kevent *eventlist, int nevents,
    s64 timeout_ms);
void	knote_ready(knote_t *kn);
void	knote_notify_all(s16 filter, u64 ident, u32 fflags,
    s64 data);
void	kqueue_wakeup(kqueue_t *kq);
void	filter_register(const filter_ops_t *ops);
const filter_ops_t *filter_lookup(s16 filter);
void	event_timer_tick(void);
void	event_cleanup_process(struct process *proc);
void	event_fork_process(struct process *parent,
    struct process *child);

void	event_notify_proc_exit(u32 pid, int exit_code);
void	event_notify_proc_fork(u32 parent_pid, u32 child_pid);
void	event_notify_signal(u32 pid, int sig);
void	event_notify_pipe_change(struct pipe *p);
void	event_notify_net_change(struct net_endpoint *ep);
void	event_notify_ipc_change(struct ipc_endpoint *endpoint);

void	proc_sleep(void *channel);
void	proc_wakeup(void *channel);
void	proc_wakeup_one(void *channel);

#endif
