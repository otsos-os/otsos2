/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data
$define %type filter_ops_t as struct with filter callbacks vtable

$define %func filt_power_attach as function with args knote_t *
$define %func filt_power_detach as procedure with args knote_t *
$define %func filt_power_event as function with args knote_t *, u32
$define %func filt_power_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_power_attach, filt_power_detach
$space %internal filt_power_event, filt_power_touch
$space %export filter_power_ops

*/

#include <kernel/drivers/power/pbutton.h>
#include <kernel/event/event.h>

static int
filt_power_attach(knote_t *kn)
{
	kn->fpriv = power_button_event_sequence();
	return (0);
}

static void
filt_power_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_power_event(knote_t *kn, u32 nevents)
{
	u64	sequence, pending;

	sequence = power_button_event_sequence();
	pending = sequence - kn->fpriv;
	if (pending == 0) {
		return (0);
	}
	if (nevents == 0) {
		return (1);
	}

	kn->fpriv = sequence;
	kn->fflags = NOTE_POWER_BUTTON;
	kn->data = (s64)pending;
	return (1);
}

static void
filt_power_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_power_ops = {
	.filter	= EVFILT_POWER,
	.name	= "power",
	.attach	= filt_power_attach,
	.detach	= filt_power_detach,
	.event	= filt_power_event,
	.touch	= filt_power_touch,
};
