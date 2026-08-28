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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type entity_id_t as 64 bit generation tagged object id

$define %func filt_proc_attach as function with args knote_t *
$define %func filt_proc_detach as procedure with args knote_t *
$define %func filt_proc_event as function with args knote_t *, u32
$define %func filt_proc_touch as procedure with args knote_t *, struct kevent *
$define %func event_notify_proc_exit as procedure with args u32, int
$define %func event_notify_proc_fork as procedure with args u32, u32
$define %func event_notify_proc_reap as procedure with args u32

*/

/* !SPACE!

$space %internal filt_proc_attach, filt_proc_detach
$space %internal filt_proc_event, filt_proc_touch
$space %export filter_proc_ops
$space %export event_notify_proc_exit, event_notify_proc_fork
$space %export event_notify_proc_reap

*/

#include <kernel/entity/entity.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
filt_proc_attach(knote_t *kn)
{
	entity_id_t	id;
	u32		pid;
	int		code, flags;

	pid = (u32)kn->ident;

	id = process_entity_of_pid(pid);
	if (id == 0) {
		printk("[EVFILT_PROC] attach: pid %d not "
		    "found\n", pid);
		return (-API_ERR_NO_PROC);
	}
	if (entity_retain_checked(id) != 0) {
		return (-API_ERR_NO_MEMORY);
	}
	kn->fpriv = (u64)id;

	printk("[EVFILT_PROC] attach: monitoring "
	    "pid=%d\n", pid);

	if (process_record_read(id, &code, &flags, NULL, NULL) == 0 &&
	    (flags & PROC_EXIT_EXITED)) {
		kn->fflags |= NOTE_EXIT;
		kn->data = code;
		knote_ready(kn);
	}

	return (0);
}

static void
filt_proc_detach(knote_t *kn)
{
	entity_id_t	id;

	if (kn->fpriv == 0) {
		return;
	}
	id = (entity_id_t)kn->fpriv;
	kn->fpriv = 0;
	entity_release(id);
}

static int
filt_proc_event(knote_t *kn, u32 nevents)
{
	entity_id_t	id;
	int		code, flags;

	if ((kn->fflags & NOTE_REAP) && kn->pending) {
		return (1);
	}

	id = (entity_id_t)kn->fpriv;
	if (id == 0) {
		kn->fflags |= NOTE_EXIT;
		return (1);
	}

	if (process_record_read(id, &code, &flags, NULL, NULL) != 0) {
		kn->fflags |= NOTE_EXIT;
		return (1);
	}

	if (flags & PROC_EXIT_EXITED) {
		if (kn->fflags & NOTE_EXIT) {
			return (kn->pending ? 1 : 0);
		}
		kn->fflags |= NOTE_EXIT;
		kn->data = code;
		return (1);
	}

	return (0);
}

static void
filt_proc_touch(knote_t *kn, struct kevent *kev)
{
	kn->fflags = kev->fflags;
}

void
event_notify_proc_exit(u32 pid, int exit_code)
{
	knote_notify_all(EVFILT_PROC, (u64)pid, NOTE_EXIT,
	    (s64)exit_code);
}

void
event_notify_proc_fork(u32 parent_pid, u32 child_pid)
{
	knote_notify_all(EVFILT_PROC, (u64)parent_pid,
	    NOTE_FORK, (s64)child_pid);
}

void
event_notify_proc_reap(u32 parent_pid)
{
	if (parent_pid == 0) {
		return;
	}

	knote_notify_all(EVFILT_PROC, (u64)parent_pid, NOTE_REAP, 0);
}

const filter_ops_t filter_proc_ops = {
	.filter	= EVFILT_PROC,
	.name	= "proc",
	.attach	= filt_proc_attach,
	.detach	= filt_proc_detach,
	.event	= filt_proc_event,
	.touch	= filt_proc_touch,
};
