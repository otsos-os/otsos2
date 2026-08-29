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

$define %func filt_signal_attach as function with args knote_t *
$define %func filt_signal_detach as procedure with args knote_t *
$define %func filt_signal_event as function with args knote_t *, u32
$define %func filt_signal_touch as procedure with args knote_t *, struct kevent *
$define %func event_notify_signal as procedure with args u32, int

*/

/* !SPACE!

$space %internal filt_signal_attach, filt_signal_detach
$space %internal filt_signal_event, filt_signal_touch
$space %export filter_signal_ops, event_notify_signal

*/

#include <kernel/event/event.h>
#include <kernel/signal.h>
#include <kernel/process.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	MAX_SIGNAL_SLOTS	64
#define	MAX_SIGNALS		32

static u32	signal_counts[MAX_SIGNAL_SLOTS][MAX_SIGNALS];

static int
filt_signal_attach(knote_t *kn)
{
	int	sig;

	sig = (int)kn->ident;
	if (sig < 0 || sig >= MAX_SIGNALS) {
		printk("[EVFILT_SIGNAL] attach: invalid "
		    "signal %d\n", sig);
		return (-API_ERR_INVAL);
	}

	kn->flags |= EV_CLEAR;
	return (0);
}

static void
filt_signal_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_signal_event(knote_t *kn, u32 nevents)
{
	int		sig, slot;
	process_t	*proc;
	u32		count;

	sig = (int)kn->ident;
	proc = process_current();
	if (!proc) {
		return (0);
	}

	slot = (int)proc->pid % MAX_SIGNAL_SLOTS;
	if (sig < 0 || sig >= MAX_SIGNALS) {
		return (0);
	}

	count = __atomic_exchange_n(&signal_counts[slot][sig], 0,
	    __ATOMIC_ACQ_REL);
	if (count > 0) {
		kn->data = (s64)count;
		return (1);
	}

	return (0);
}

static void
filt_signal_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

void
event_notify_signal(u32 pid, int sig)
{
	int	slot;

	if (sig < 0 || sig >= MAX_SIGNALS) {
		return;
	}

	slot = (int)(pid % MAX_SIGNAL_SLOTS);
	__atomic_fetch_add(&signal_counts[slot][sig], 1, __ATOMIC_ACQ_REL);

	knote_notify_all(EVFILT_SIGNAL, (u64)sig, 0, 0);
}

const filter_ops_t filter_signal_ops = {
	.filter	= EVFILT_SIGNAL,
	.name	= "signal",
	.attach	= filt_signal_attach,
	.detach	= filt_signal_detach,
	.event	= filt_signal_event,
	.touch	= filt_signal_touch,
};
