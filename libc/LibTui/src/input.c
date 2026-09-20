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

$define %type api_key_event as native keyboard event

$define %func tui_key_from as procedure with args tui_key_t *, const api_key_event *
$define %func tui_key_read as procedure with args tui_key_t *
$define %func tui_key_poll as function with args tui_key_t *

*/

/* !SPACE!

$space %internal tui_key_from
$space %export tui_key_read, tui_key_poll

*/

#include <native.h>
#include <stddef.h>
#include <string.h>
#include "tui_int.h"

static void
tui_key_from(tui_key_t *key, const struct api_key_event *ev)
{
	memset(key, 0, sizeof(*key));
	key->code = (int)ev->key;
	key->ch = ev->ch;
	key->mods = ev->mods;
}


void
tui_key_read(tui_key_t *key)
{
	struct api_key_event	ev;
	int			ret;

	if (key == NULL) {
		return;
	}
	for (;;) {
		memset(&ev, 0, sizeof(ev));
		ret = inputRead(&ev, 1, 0);
		if (ret < 0) {
			memset(key, 0, sizeof(*key));
			key->code = TUI_KEY_ESC;
			return;
		}
		if (ret != 1) {
			continue;
		}
		if ((ev.flags & TUI_EVENT_PRESS) == 0) {
			continue;
		}
		tui_key_from(key, &ev);
		return;
	}
}


int
tui_key_poll(tui_key_t *key)
{
	struct api_key_event	ev;
	int			ret;

	if (key == NULL) {
		return (-1);
	}
	for (;;) {
		memset(&ev, 0, sizeof(ev));
		ret = inputRead(&ev, 1, API_INPUT_NONBLOCK);
		if (ret < 0) {
			return (-1);
		}
		if (ret != 1) {
			return (0);
		}
		if ((ev.flags & TUI_EVENT_PRESS) == 0) {
			continue;
		}
		tui_key_from(key, &ev);
		return (1);
	}
}
