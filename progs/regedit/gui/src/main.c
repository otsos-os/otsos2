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

$define %type rg_state as regedit gui global state
$define %type sprot_event as one compositor event

$define %func rg_start_path as procedure with args rg_state *, const char *
$define %func rg_run as procedure with args rg_state *
$define %func rg_main as function with args const char *

*/

/* !SPACE!

$space %internal rg_start_path, rg_run
$space %export rg_main

*/

#include <errno.h>
#include <regedit/frontend.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

static rg_state_t	rg_state;
static void
rg_start_path(rg_state_t *st, const char *path)
{
	re_path_reset(&st->path);
	if (path == NULL || path[0] == '\0') {
		return;
	}
	errno = 0;
	if (re_path_parse(&st->path, path) != 0) {
		re_path_reset(&st->path);
		rg_status_fmt(st, "%s: %s", path, re_error(errno));
	}
}

static void
rg_run(rg_state_t *st)
{
	sprot_event_t	event;
	int		ret, burst;

	burst = 0;
	while (st->running) {
		ret = sprot_poll_event(st->conn, &event,
		    st->dirty ? 0 : RG_IDLE_MS);
		if (ret < 0) {
			st->running = 0;
			break;
		}
		if (ret > 0) {
			rg_dispatch(st, &event);
			burst++;
			if (burst < RG_BURST_MAX) {
				continue;
			}
		}
		burst = 0;
		if (!st->dirty) {
			continue;
		}
		st->dirty = 0;
		rg_draw(st);
		if (rg_window_present(st) != 0) {
			st->running = 0;
			break;
		}
	}
}

int
rg_main(const char *path)
{
	rg_state_t	*st;
	int		ret;
	st = &rg_state;
	memset(st, 0, sizeof(*st));
	st->click_pane = -1;
	st->click_row = -1;
	st->mouse_x = -1;
	st->mouse_y = -1;
	st->focus = RG_FOCUS_KEYS;
	st->width = RG_DEFAULT_W;
	st->height = RG_DEFAULT_H;
	rg_start_path(st, path);
	ret = rg_window_open(st, RG_TITLE);
	if (ret != RG_OK) {
		rg_window_close(st);
		if (ret == RG_UNAVAILABLE) {
			return (RG_UNAVAILABLE);
		}
		fprintf(stderr, "regedit: cannot create a window: %s\n",
		    sprot_last_error());
		return (RG_FAILED);
	}

	st->running = 1;
	rg_reload(st);
	rg_run(st);
	rg_window_close(st);
	return (RG_OK);
}
