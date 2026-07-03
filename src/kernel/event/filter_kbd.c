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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type knote_t as struct with registered event state
$define %type kevent as struct with event ident, filter, flags, fflags, data, udata
$define %type filter_ops_t as struct with filter callbacks vtable
$define %type kbd_event as struct with raw keyboard event

$define %func filt_kbd_attach as function with args knote_t *
$define %func filt_kbd_detach as procedure with args knote_t *
$define %func filt_kbd_event as function with args knote_t *, u32
$define %func filt_kbd_touch as procedure with args knote_t *, struct kevent *

*/

/* !SPACE!

$space %internal filt_kbd_attach, filt_kbd_detach
$space %internal filt_kbd_event, filt_kbd_touch
$space %export filter_kbd_ops

*/

#include <kernel/event/event.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <mlibc/mlibc.h>

/*
 * EVFILT_KBD returns raw keyboard events through the kevent data field.
 *
 * The 64-bit event data is packed as:
 *   bits  0..15  scancode
 *   bit  16      released (1 = key up, 0 = key down)
 *   bit  17      extended (E0 prefix)
 *   bits 24..31  ASCII translation (0 if none)
 */

static int
filt_kbd_attach(knote_t *kn)
{
	(void)kn;
	return (0);
}

static void
filt_kbd_detach(knote_t *kn)
{
	(void)kn;
}

static int
filt_kbd_event(knote_t *kn, u32 nevents)
{
	struct kbd_event	ev;
	u64			data;

	(void)nevents;

	if (kbd_event_get(&ev) == 0) {
		return (0);
	}

	data = (u64)ev.scancode;
	data |= (u64)(ev.released ? 1 : 0) << 16;
	data |= (u64)(ev.extended ? 1 : 0) << 17;
	data |= (u64)(u8)ev.ascii << 24;

	kn->data = (s64)data;

	/*
	 * If more events remain in the ring, re-mark this knote ready so
	 * the next kevent() call can pick them up without waiting for a new
	 * hardware interrupt.
	 */
	if (kbd_event_count() > 0) {
		knote_ready(kn);
	}

	return (1);
}

static void
filt_kbd_touch(knote_t *kn, struct kevent *kev)
{
	(void)kn;
	(void)kev;
}

const filter_ops_t filter_kbd_ops = {
	.filter	= EVFILT_KBD,
	.name	= "kbd",
	.attach	= filt_kbd_attach,
	.detach	= filt_kbd_detach,
	.event	= filt_kbd_event,
	.touch	= filt_kbd_touch,
};
