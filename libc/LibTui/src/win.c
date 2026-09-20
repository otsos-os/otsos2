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

$define %func tui_win_open as procedure with args tui_win_t *, const char *, int, int
$define %func tui_win_at as procedure with args tui_win_t *, const char *, const tui_rect_t *
$define %func tui_win_draw as procedure with args tui_win_t *
$define %func tui_buttons_init as procedure with args tui_buttons_t *, const char *const *, int, int
$define %func tui_buttons_width as function with args const tui_buttons_t *
$define %func tui_buttons_draw as procedure with args const tui_buttons_t *, const tui_win_t *, int
$define %func tui_buttons_key as function with args tui_buttons_t *, const tui_key_t *
$define %func tui_screen_begin as procedure with args const char *, const char *
$define %func tui_screen_body as procedure with args tui_rect_t *

*/

/* !SPACE!

$space %internal tui_win_geom, tui_buttons_width
$space %export tui_win_open, tui_win_at, tui_win_draw
$space %export tui_buttons_init, tui_buttons_draw, tui_buttons_key
$space %export tui_screen_begin, tui_screen_body

*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "tui_int.h"

#define TUI_BTN_GAP		2
#define TUI_BTN_PAD		1
#define TUI_SCREEN_CHROME	2


static void
tui_win_geom(tui_win_t *win)
{
	win->body.row = win->frame.row + 1;
	win->body.col = win->frame.col + 1;
	win->body.width = win->frame.width - 2;
	win->body.height = win->frame.height - 2;
	if (win->body.width < 0) {
		win->body.width = 0;
	}
	if (win->body.height < 0) {
		win->body.height = 0;
	}
}

void
tui_win_open(tui_win_t *win, const char *title, int width, int height)
{
	if (win == NULL) {
		return;
	}
	memset(win, 0, sizeof(*win));
	tui_center(&win->frame, width, height);
	tui_win_geom(win);
	if (title != NULL) {
		(void)snprintf(win->title, sizeof(win->title), "%s", title);
	}
}

void
tui_win_at(tui_win_t *win, const char *title, const tui_rect_t *frame)
{
	if (win == NULL || frame == NULL) {
		return;
	}
	memset(win, 0, sizeof(*win));
	win->frame = *frame;
	tui_win_geom(win);
	if (title != NULL) {
		(void)snprintf(win->title, sizeof(win->title), "%s", title);
	}
}

void
tui_win_draw(tui_win_t *win)
{
	const tui_theme_t	*th;

	if (win == NULL) {
		return;
	}
	th = tui_theme();
	tui_fill(&win->body, th->win_fg, th->win_bg);
	tui_border(&win->frame, win->title, 1);
}

void
tui_buttons_init(tui_buttons_t *row, const char *const *labels, int count,
    int focus)
{
	if (row == NULL) {
		return;
	}
	memset(row, 0, sizeof(*row));
	if (labels == NULL || count <= 0) {
		return;
	}
	if (count > TUI_BUTTONS_MAX) {
		count = TUI_BUTTONS_MAX;
	}
	row->labels = labels;
	row->count = count;
	row->focus = focus;
	if (row->focus < 0 || row->focus >= count) {
		row->focus = 0;
	}
}

static int
tui_buttons_width(const tui_buttons_t *row)
{
	int	total;
	int	i;

	total = 0;
	for (i = 0; i < row->count; i++) {
		if (i > 0) {
			total += TUI_BTN_GAP;
		}
		total += (int)strlen(row->labels[i]) + 2 + (2 * TUI_BTN_PAD);
	}
	return (total);
}

void
tui_buttons_draw(const tui_buttons_t *row, const tui_win_t *win, int focused)
{
	const tui_theme_t	*th;
	int			col;
	int			hot;
	int			i;

	if (row == NULL || win == NULL || row->count == 0) {
		return;
	}
	th = tui_theme();
	col = win->frame.col + ((win->frame.width - tui_buttons_width(row)) / 2);
	if (col < win->frame.col + 1) {
		col = win->frame.col + 1;
	}
	tui_move(win->frame.row + win->frame.height - 1, col);

	for (i = 0; i < row->count; i++) {
		if (i > 0) {
			tui_attr(th->border_fg, th->win_bg);
			tui_pad(TUI_BOX_H, TUI_BTN_GAP);
		}
		hot = (focused && i == row->focus);
		if (hot) {
			tui_attr_bold(th->button_focus_fg, th->button_focus_bg);
		} else {
			tui_attr(th->button_fg, th->button_bg);
		}
		tui_put("[");
		tui_pad(' ', TUI_BTN_PAD);
		tui_put(row->labels[i]);
		tui_pad(' ', TUI_BTN_PAD);
		tui_put("]");
	}
}


int
tui_buttons_key(tui_buttons_t *row, const tui_key_t *key)
{
	if (row == NULL || key == NULL || row->count == 0) {
		return (-2);
	}
	switch (key->code) {
	case TUI_KEY_LEFT:
		if (row->focus > 0) {
			row->focus--;
		}
		return (-2);
	case TUI_KEY_RIGHT:
		if (row->focus + 1 < row->count) {
			row->focus++;
		}
		return (-2);
	case TUI_KEY_TAB:
		row->focus = (row->focus + 1) % row->count;
		return (-2);
	case TUI_KEY_ENTER:
	case TUI_KEY_KP_ENTER:
	case TUI_KEY_SPACE:
		return (row->focus);
	case TUI_KEY_ESC:
		return (-1);
	default:
		return (-2);
	}
}

void
tui_screen_begin(const char *title, const char *status)
{
	const tui_theme_t	*th;
	tui_rect_t		bar;

	th = tui_theme();
	tui_backdrop(title);

	bar.row = tui_rows();
	bar.col = 1;
	bar.width = tui_cols();
	bar.height = 1;
	tui_fill(&bar, th->status_fg, th->status_bg);
	if (status != NULL && status[0] != '\0') {
		tui_move(bar.row, 2);
		tui_attr(th->status_fg, th->status_bg);
		tui_field(status, bar.width - 2);
	}
}

void
tui_screen_body(tui_rect_t *rect)
{
	if (rect == NULL) {
		return;
	}
	rect->row = 2;
	rect->col = 1;
	rect->width = tui_cols();
	rect->height = tui_rows() - TUI_SCREEN_CHROME;
	if (rect->height < 1) {
		rect->height = 1;
	}
}
