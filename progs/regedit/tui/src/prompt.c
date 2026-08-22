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

$define %type size_t as native object size
$define %type api_key_event as native keyboard event
$define %type rt_state as regedit tui global state

$define %func rt_prompt_line as procedure with args rt_state *, label, text
$define %func rt_prompt as function with args rt_state *, label, char *, size_t
$define %func rt_confirm as function with args rt_state *, const char *

*/

/* !SPACE!

$space %internal rt_prompt_line
$space %export rt_prompt, rt_confirm

*/

#include <native.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

#define RT_PROMPT_COLOR		93

static void
rt_prompt_line(rt_state_t *st, const char *label, const char *text)
{
	char	line[RT_STATUS_MAX];
	int	room, len, skip;

	rt_screen_move(st->rows, 1);
	rt_screen_color(RT_PROMPT_COLOR);
	room = st->cols - (int)strlen(label) - 3;
	if (room < 1) {
		room = 1;
	}
	len = (int)strlen(text);
	skip = len > room ? len - room : 0;
	snprintf(line, sizeof(line), " %s%s_", label, text + skip);
	rt_screen_field(line, st->cols);
	rt_screen_reset();
}

int
rt_prompt(rt_state_t *st, const char *label, char *out, size_t size)
{
	struct api_key_event	ev;
	size_t			len;

	if (!out || size == 0) {
		return (0);
	}
	out[size - 1] = '\0';
	len = strnlen(out, size - 1);
	out[len] = '\0';
	for (;;) {
		rt_draw(st);
		rt_prompt_line(st, label, out);
		rt_key_read(&ev);
		if (ev.key == RT_KEY_ESC) {
			rt_status(st, "Cancelled");
			return (0);
		}
		if (ev.key == RT_KEY_ENTER || ev.key == RT_KEY_KP_ENTER) {
			return (1);
		}
		if (ev.key == RT_KEY_BACKSPACE) {
			if (len > 0) {
				len--;
				out[len] = '\0';
			}
			continue;
		}
		if (ev.ch >= 32 && ev.ch < 127 && len + 1 < size) {
			out[len] = (char)ev.ch;
			len++;
			out[len] = '\0';
		}
	}
}

int
rt_confirm(rt_state_t *st, const char *question)
{
	struct api_key_event	ev;
	char			label[RT_STATUS_MAX];

	snprintf(label, sizeof(label), "%s [y/n] ", question);
	for (;;) {
		rt_draw(st);
		rt_prompt_line(st, label, "");
		rt_key_read(&ev);
		if (ev.key == RT_KEY_ESC) {
			rt_status(st, "Cancelled");
			return (0);
		}
		if (ev.ch == 'y' || ev.ch == 'Y') {
			return (1);
		}
		if (ev.ch == 'n' || ev.ch == 'N') {
			rt_status(st, "Cancelled");
			return (0);
		}
	}
}
