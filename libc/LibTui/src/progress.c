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

$define %func tui_gauge_paint as procedure with args tui_gauge_t *
$define %func tui_gauge_open as procedure with args tui_gauge_t *, const char *, uint64_t
$define %func tui_gauge_step as procedure with args tui_gauge_t *, uint64_t, const char *
$define %func tui_gauge_close as procedure with args tui_gauge_t *

*/

/* !SPACE!

$space %internal tui_gauge_paint
$space %export tui_gauge_open, tui_gauge_step, tui_gauge_close

*/

#include <stddef.h>
#include <stdio.h>
#include "tui_int.h"

#define TUI_GAUGE_MARGIN	8
#define TUI_GAUGE_WIDTH_MIN	40
#define TUI_GAUGE_BODY		3

static void
tui_gauge_paint(tui_gauge_t *gauge)
{
	const tui_theme_t	*theme;
	char			pct[16];
	uint64_t		scaled;
	int			bar_width;
	int			filled;
	int			percent;
	int			len;

	theme = tui_theme();
	tui_win_draw(&gauge->win);

	tui_move(gauge->win.body.row, gauge->win.body.col);
	tui_attr(theme->text_fg, theme->win_bg);
	tui_field(gauge->label, gauge->win.body.width);


	if (gauge->total == 0) {
		percent = 0;
	} else {
		scaled = gauge->done;
		if (scaled > gauge->total) {
			scaled = gauge->total;
		}
		percent = (int)((scaled * 100u) / gauge->total);
	}

	bar_width = gauge->win.body.width - 2;
	if (bar_width < 1) {
		bar_width = 1;
	}
	filled = (bar_width * percent) / 100;

	tui_move(gauge->win.body.row + 2, gauge->win.body.col + 1);
	tui_attr(theme->gauge_fg, theme->gauge_bg);
	tui_pad(' ', filled);
	tui_attr(theme->win_fg, theme->win_bg);
	tui_pad(' ', bar_width - filled);

	len = snprintf(pct, sizeof(pct), " %d%% ", percent);
	if (len < 0 || len > bar_width) {
		tui_flush();
		return;
	}
	tui_move(gauge->win.body.row + 2,
	    gauge->win.body.col + 1 + (bar_width - len) / 2);
	if ((bar_width - len) / 2 < filled) {
		tui_attr(theme->gauge_bg, theme->gauge_fg);
	} else {
		tui_attr(theme->text_fg, theme->win_bg);
	}
	tui_put(pct);
	tui_attr_reset();
	tui_cursor(0);
	tui_flush();
}


void
tui_gauge_open(tui_gauge_t *gauge, const char *title, uint64_t total)
{
	int	width;

	if (gauge == NULL) {
		return;
	}
	width = tui_cols() - TUI_GAUGE_MARGIN;
	if (width > TUI_DLG_WIDTH_MAX) {
		width = TUI_DLG_WIDTH_MAX;
	}
	if (width < TUI_GAUGE_WIDTH_MIN) {
		width = TUI_GAUGE_WIDTH_MIN;
	}

	gauge->total = total;
	gauge->done = 0;
	gauge->label[0] = '\0';
	tui_win_open(&gauge->win, title, width, TUI_GAUGE_BODY + 2);
	tui_gauge_paint(gauge);
}


void
tui_gauge_step(tui_gauge_t *gauge, uint64_t done, const char *label)
{
	if (gauge == NULL) {
		return;
	}
	gauge->done = done;
	if (label != NULL) {
		(void)snprintf(gauge->label, sizeof(gauge->label), "%s", label);
	}
	tui_gauge_paint(gauge);
}


void
tui_gauge_close(tui_gauge_t *gauge)
{
	if (gauge == NULL) {
		return;
	}
	gauge->total = 0;
	gauge->done = 0;
	gauge->label[0] = '\0';
	tui_attr_reset();
	tui_flush();
}
