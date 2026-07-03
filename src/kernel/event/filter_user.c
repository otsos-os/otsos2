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
$define %type int as 32 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable

$define %func filt_user_attach as function with args knote_t *
$define %func filt_user_detach as procedure with args knote_t *
$define %func filt_user_event as function with args knote_t *, u32
$define %func filt_user_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_user_attach, filt_user_detach
$space %internal filt_user_event, filt_user_touch
$space %export filter_user_ops

*/

#include <kernel/event/event.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
filt_user_attach(knote_t *kn)
{
	printk("[EVFILT_USER] attach: ident=%llu\n",
	    kn->ident);
	return (0);
}

static void
filt_user_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_user_event(knote_t *kn, u32 nevents)
{
	if (kn->fpriv & NOTE_TRIGGER) {
		if (kn->flags & EV_CLEAR) {
			kn->fpriv &= ~NOTE_TRIGGER;
		}
		return (1);
	}
	return (0);
}

static void
filt_user_touch(knote_t *kn, struct kevent *kev)
{
	u32	ctrl, user_fflags;

	if (kev->fflags & NOTE_TRIGGER) {
		ctrl = kev->fflags & NOTE_FFCTRLMASK;
		user_fflags = kev->fflags & NOTE_FFLAGSMASK;

		switch (ctrl) {
		case NOTE_FFNOP:
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

		kn->fpriv |= NOTE_TRIGGER;
		knote_ready(kn);
	} else {
		kn->fflags = kev->fflags;
	}
}

const filter_ops_t filter_user_ops = {
	.filter	= EVFILT_USER,
	.name	= "user",
	.attach	= filt_user_attach,
	.detach	= filt_user_detach,
	.event	= filt_user_event,
	.touch	= filt_user_touch,
};
