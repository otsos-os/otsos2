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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s8 as 8 bit signed
$define %type s16 as 16 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type mouse_event as struct with normalized mouse input event

$define %func filt_mouse_attach as function with args knote_t *
$define %func filt_mouse_detach as procedure with args knote_t *
$define %func filt_mouse_event as function with args knote_t *, u32
$define %func filt_mouse_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_mouse_attach, filt_mouse_detach
$space %internal filt_mouse_event, filt_mouse_touch
$space %export filter_mouse_ops

*/

#include <kernel/event/event.h>
#include <kernel/drivers/mouse/mouse.h>
#include <mlibc/mlibc.h>

static int
filt_mouse_attach(knote_t *kn)
{
	(void)kn;
	return (0);
}

static void
filt_mouse_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_mouse_event(knote_t *kn, u32 nevents)
{
	struct mouse_event	ev;
	u64			data;

	if (nevents == 0) {
		return (mouse_event_count() > 0);
	}

	if (mouse_event_get(&ev) == 0) {
		return (0);
	}

	data = (u64)(u16)(s16)ev.dx;
	data |= (u64)(u16)(s16)ev.dy << 16;
	data |= (u64)(u8)(s8)ev.dz << 32;
	data |= (u64)(u8)ev.buttons << 40;
	data |= (u64)(u16)ev.flags << 48;

	kn->data = (s64)data;
	kn->fflags = ev.flags | (ev.buttons << 16);

	return (mouse_event_count() > 0 ? 2 : 1);
}

static void
filt_mouse_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_mouse_ops = {
	.filter	= EVFILT_MOUSE,
	.name	= "mouse",
	.attach	= filt_mouse_attach,
	.detach	= filt_mouse_detach,
	.event	= filt_mouse_event,
	.touch	= filt_mouse_touch,
};
