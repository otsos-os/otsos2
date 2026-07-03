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
$define %type process_t as struct with process control block

$define %func filt_proc_attach as function with args knote_t *
$define %func filt_proc_detach as procedure with args knote_t *
$define %func filt_proc_event as function with args knote_t *, u32
$define %func filt_proc_touch as procedure with args knote_t *, struct kevent *
$define %func event_notify_proc_exit as procedure with args u32, int
$define %func event_notify_proc_fork as procedure with args u32, u32

*/

/* !SPACE!

$space %internal filt_proc_attach, filt_proc_detach
$space %internal filt_proc_event, filt_proc_touch
$space %export filter_proc_ops
$space %export event_notify_proc_exit, event_notify_proc_fork

*/

#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
filt_proc_attach(knote_t *kn)
{
	u32		pid;
	process_t	*proc;

	pid = (u32)kn->ident;

	proc = process_get(pid);
	if (!proc) {
		printk("[EVFILT_PROC] attach: pid %d not "
		    "found\n", pid);
		return (-API_ERR_NO_PROC);
	}

	printk("[EVFILT_PROC] attach: monitoring "
	    "pid=%d\n", pid);

	if (proc->main_thread &&
	    proc->main_thread->state == PROC_STATE_ZOMBIE) {
		kn->fflags |= NOTE_EXIT;
		kn->data = proc->exit_code;
		knote_ready(kn);
	}

	return (0);
}

static void
filt_proc_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_proc_event(knote_t *kn, u32 nevents)
{
	u32		pid;
	process_t	*proc;

	pid = (u32)kn->ident;

	proc = process_get(pid);
	if (!proc) {
		kn->fflags |= NOTE_EXIT;
		return (1);
	}

	if (proc->main_thread &&
	    proc->main_thread->state == PROC_STATE_ZOMBIE) {
		if (kn->fflags & NOTE_EXIT) {
			return (kn->pending ? 1 : 0);
		}
		kn->fflags |= NOTE_EXIT;
		kn->data = proc->exit_code;
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

const filter_ops_t filter_proc_ops = {
	.filter	= EVFILT_PROC,
	.name	= "proc",
	.attach	= filt_proc_attach,
	.detach	= filt_proc_detach,
	.event	= filt_proc_event,
	.touch	= filt_proc_touch,
};
