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

$define %func compute_timer_ticks as function with args s64, u32
$define %func filt_timer_attach as function with args knote_t *
$define %func filt_timer_detach as procedure with args knote_t *
$define %func filt_timer_event as function with args knote_t *, u32
$define %func filt_timer_touch as procedure with args knote_t *, struct kevent *
$define %func filter_timer_tick as procedure with args void

*/

/* !SPACE!

$space %internal compute_timer_ticks, filt_timer_attach
$space %internal filt_timer_detach, filt_timer_event
$space %internal filt_timer_touch
$space %export filter_timer_ops, filter_timer_tick

*/

#include <kernel/event/event.h>
#include <kernel/drivers/timer.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static knote_t	*timer_list[MAX_KQUEUES * MAX_KNOTES];
static int	timer_count;

static u64
compute_timer_ticks(s64 data, u32 fflags)
{
	u64	freq;

	freq = timer_get_frequency();

	if (fflags & NOTE_SECONDS) {
		return ((u64)data * freq);
	} else if (fflags & NOTE_USECONDS) {
		return ((u64)data * freq / 1000000);
	} else if (fflags & NOTE_NSECONDS) {
		return ((u64)data * freq / 1000000000);
	} else {
		return ((u64)data * freq / 1000);
	}
}

static int
filt_timer_attach(knote_t *kn)
{
	u64	period_ticks;

	if (kn->data <= 0) {
		kn->data = 1;
	}

	period_ticks = compute_timer_ticks(kn->data, kn->fflags);
	if (period_ticks == 0) {
		period_ticks = 1;
	}

	kn->fpriv = timer_get_ticks() + period_ticks;

	if (timer_count < (int)(MAX_KQUEUES * MAX_KNOTES)) {
		timer_list[timer_count++] = kn;
	}

	printk("[EVFILT_TIMER] attach: ident=%llu "
	    "period=%llu ticks\n", kn->ident, period_ticks);

	return (0);
}

static void
filt_timer_detach(knote_t *kn)
{
	int	i;

	for (i = 0; i < timer_count; i++) {
		if (timer_list[i] == kn) {
			timer_list[i] =
			    timer_list[--timer_count];
			return;
		}
	}
}

static int
filt_timer_event(knote_t *kn, u32 nevents)
{
	u64	now, period, elapsed, expirations;

	if (kn->pending) {
		return (1);
	}

	if (timer_get_ticks() >= kn->fpriv) {
		now = timer_get_ticks();
		period = compute_timer_ticks(kn->data, kn->fflags);
		if (period == 0) {
			period = 1;
		}

		elapsed = now - (kn->fpriv - period);
		expirations = elapsed / period;
		if (expirations < 1) {
			expirations = 1;
		}
		kn->data = (s64)expirations;

		if (!(kn->flags & EV_ONESHOT)) {
			kn->fpriv = now + period;
		}

		return (1);
	}

	return (0);
}

static void
filt_timer_touch(knote_t *kn, struct kevent *kev)
{
	u64	period_ticks;

	kn->data = kev->data;
	kn->fflags = kev->fflags;

	period_ticks = compute_timer_ticks(kn->data, kn->fflags);
	if (period_ticks == 0) {
		period_ticks = 1;
	}
	kn->fpriv = timer_get_ticks() + period_ticks;
}

void
filter_timer_tick(void)
{
	u64	now, period, elapsed, expirations;
	int	i;
	knote_t	*kn;

	now = timer_get_ticks();

	for (i = 0; i < timer_count; i++) {
		kn = timer_list[i];
		if (!kn || !kn->used || kn->disabled) {
			continue;
		}

		if (now >= kn->fpriv) {
			period = compute_timer_ticks(kn->data,
			    kn->fflags);
			if (period == 0) {
				period = 1;
			}

			elapsed = now - (kn->fpriv - period);
			expirations = elapsed / period;
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
	.filter	= EVFILT_TIMER,
	.name	= "timer",
	.attach	= filt_timer_attach,
	.detach	= filt_timer_detach,
	.event	= filt_timer_event,
	.touch	= filt_timer_touch,
};
