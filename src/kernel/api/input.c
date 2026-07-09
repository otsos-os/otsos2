/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
$define %type int as 32 bit signed
$define %type api_key_event as struct with normalized keyboard input event
$define %type kbd_event as struct with kernel keyboard input event

$define %func input_poll_hardware as procedure with args void
$define %func input_copy_event as procedure with args api_key_event *, event *
$define %func api_input_read as function with args api_key_event *, u32, u32
$define %func api_input_poll as function with args void
$define %func api_input_flush as function with args void

*/

/* !SPACE!

$space %internal input_poll_hardware, input_copy_event
$space %export api_input_read, api_input_poll, api_input_flush

*/

#include <kernel/api/api.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static void
input_poll_hardware(void)
{
	keyboard_start_direct_input();
	keyboard_poll();
	keyboard_stop_direct_input();
	keyboard_flush_chars();
}

static void
input_copy_event(struct api_key_event *dst, struct kbd_event *src)
{
	dst->timestamp = src->timestamp;
	dst->key = src->key;
	dst->raw = src->raw;
	dst->flags = src->flags;
	dst->mods = src->mods;
	dst->ch = src->ch;
}

int
api_input_read(struct api_key_event *buf, u32 count, u32 flags)
{
	struct kbd_event	ev;
	process_t		*proc;
	void			*channel;
	u32			n;

	if (count == 0) {
		return (0);
	}
	if (!buf || !is_user_address(buf, count * sizeof(*buf))) {
		return (-API_ERR_BAD_ADDR);
	}

	n = 0;
	while (n < count) {
		input_poll_hardware();
		if (kbd_event_get(&ev) == 1) {
			input_copy_event(&buf[n], &ev);
			n++;
			continue;
		}
		if (n > 0) {
			break;
		}
		if (flags & API_INPUT_NONBLOCK) {
			break;
		}

		proc = process_current();
		if (proc && (proc->sigpending & ~proc->sigmask)) {
			return (-API_ERR_INTR);
		}
		channel = terminal_get_input_channel();
		if (channel) {
			proc_sleep(channel);
		} else {
			process_yield();
		}
	}

	return ((int)n);
}

int
api_input_poll(void)
{
	input_poll_hardware();
	return (kbd_event_count());
}

int
api_input_flush(void)
{
	keyboard_flush_input();
	terminal_flush_input_active();
	return (0);
}
