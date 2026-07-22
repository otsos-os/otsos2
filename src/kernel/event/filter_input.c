/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type api_input_event as struct with normalized input event

$define %func filt_input_attach as function with args knote_t *
$define %func filt_input_detach as procedure with args knote_t *
$define %func filt_input_event as function with args knote_t *, u32
$define %func filt_input_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_input_attach, filt_input_detach
$space %internal filt_input_event, filt_input_touch
$space %export filter_input_ops

*/

#include <kernel/drivers/input/input.h>
#include <kernel/event/event.h>
#include <mlibc/mlibc.h>

static int
filt_input_attach(knote_t *kn)
{
	kn->fpriv = input_event_next_seq();
	return (0);
}

static void
filt_input_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_input_event(knote_t *kn, u32 nevents)
{
	struct api_input_event	event;

	if (nevents == 0) {
		return (input_event_pending(kn->fpriv) > 0);
	}

	if (input_event_get_after(&kn->fpriv, &event) == 0) {
		return (0);
	}

	kn->data = (s64)event.seq;
	kn->fflags = event.flags;
	kn->input = event;

	return (input_event_pending(kn->fpriv) > 0 ? 2 : 1);
}

static void
filt_input_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_input_ops = {
	.filter	= EVFILT_INPUT,
	.name	= "input",
	.attach	= filt_input_attach,
	.detach	= filt_input_detach,
	.event	= filt_input_event,
	.touch	= filt_input_touch,
};
