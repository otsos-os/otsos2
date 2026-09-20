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

$define %func tui_dialog_width as function with args const char *, const char *
$define %func tui_run_modal as function with args tui_win_t *, const char *, tui_buttons_t *
$define %func tui_msgbox as procedure with args const char *, const char *
$define %func tui_yesno as function with args const char *, const char *, int
$define %func tui_buttonbox as function with args const char *, const char *, const char *const *, int, int
$define %func tui_inputbox as function with args const char *, const char *, char *, size_t
$define %func tui_textbox as procedure with args const char *, const char *

*/

/* !SPACE!

$space %internal tui_dialog_width, tui_run_modal
$space %export tui_msgbox, tui_yesno, tui_buttonbox, tui_inputbox
$space %export tui_textbox

*/

#include <stddef.h>
#include <string.h>
#include "tui_int.h"


int
tui_dialog_width(const char *title, const char *text)
{
	int	want;
	int	room;
	int	len;

	want = TUI_DLG_WIDTH_MIN;
	if (title != NULL) {
		len = (int)strlen(title) + 6;
		if (len > want) {
			want = len;
		}
	}
	if (text != NULL) {
		len = (int)strlen(text);
		len = (len / 3) + 4;
		if (len > want) {
			want = len;
		}
	}
	if (want > TUI_DLG_WIDTH_MAX) {
		want = TUI_DLG_WIDTH_MAX;
	}
	room = tui_cols() - TUI_DLG_MARGIN;
	if (want > room) {
		want = room;
	}
	if (want < TUI_DLG_WIDTH_MIN && room >= TUI_DLG_WIDTH_MIN) {
		want = TUI_DLG_WIDTH_MIN;
	}
	return (want);
}


int
tui_run_modal(tui_win_t *win, const char *text, tui_buttons_t *row)
{
	tui_key_t	key;
	int		pick;

	for (;;) {
		tui_win_draw(win);
		(void)tui_text_draw(&win->body, text, 0);
		tui_buttons_draw(row, win, 1);
		tui_flush();

		tui_key_read(&key);
		pick = tui_buttons_key(row, &key);
		if (pick == -2) {
			continue;
		}
		return (pick < 0 ? TUI_ESC : pick);
	}
}

void
tui_msgbox(const char *title, const char *text)
{
	static const char *const	labels[] = { "OK" };
	tui_buttons_t			row;
	tui_win_t			win;
	int				width;
	int				rows;

	width = tui_dialog_width(title, text);
	rows = tui_text_rows(text, width - 2);
	if (rows > TUI_DLG_TEXT_MAX) {
		rows = TUI_DLG_TEXT_MAX;
	}
	if (rows < 1) {
		rows = 1;
	}
	tui_win_open(&win, title, width, rows + 3);
	tui_buttons_init(&row, labels, 1, 0);
	(void)tui_run_modal(&win, text, &row);
}

int
tui_yesno(const char *title, const char *text, int def_yes)
{
	static const char *const	labels[] = { "Yes", "No" };
	tui_buttons_t			row;
	tui_win_t			win;
	int				width;
	int				pick;
	int				rows;

	width = tui_dialog_width(title, text);
	rows = tui_text_rows(text, width - 2);
	if (rows > TUI_DLG_TEXT_MAX) {
		rows = TUI_DLG_TEXT_MAX;
	}
	if (rows < 1) {
		rows = 1;
	}
	tui_win_open(&win, title, width, rows + 3);
	tui_buttons_init(&row, labels, 2, def_yes ? 0 : 1);
	pick = tui_run_modal(&win, text, &row);
	return (pick == 0 ? TUI_OK : TUI_CANCEL);
}

int
tui_buttonbox(const char *title, const char *text, const char *const *labels,
    int count, int initial)
{
	tui_buttons_t	row;
	tui_win_t	win;
	int		width;
	int		rows;

	if (labels == NULL || count <= 0) {
		return (TUI_ESC);
	}
	width = tui_dialog_width(title, text);
	rows = tui_text_rows(text, width - 2);
	if (rows > TUI_DLG_TEXT_MAX) {
		rows = TUI_DLG_TEXT_MAX;
	}
	if (rows < 1) {
		rows = 1;
	}
	tui_win_open(&win, title, width, rows + 3);
	tui_buttons_init(&row, labels, count, initial);
	return (tui_run_modal(&win, text, &row));
}


int
tui_inputbox(const char *title, const char *label, char *out, size_t size)
{
	static const char *const	labels[] = { "OK", "Cancel" };
	char				work[TUI_TEXT_MAX];
	tui_buttons_t			row;
	tui_edit_t			ed;
	tui_win_t			win;
	tui_key_t			key;
	tui_rect_t			text;
	int				width;
	int				field;
	int				rows;
	int				pick;

	if (out == NULL || size == 0) {
		return (TUI_ESC);
	}
	if (size > sizeof(work)) {
		size = sizeof(work);
	}
	memset(work, 0, sizeof(work));
	memcpy(work, out, strnlen(out, size - 1));

	width = tui_dialog_width(title, label);
	rows = tui_text_rows(label, width - 2);
	if (rows > TUI_DLG_TEXT_MAX) {
		rows = TUI_DLG_TEXT_MAX;
	}
	if (rows < 1) {
		rows = 1;
	}
	tui_win_open(&win, title, width, rows + 5);
	tui_buttons_init(&row, labels, 2, 0);
	tui_edit_init(&ed, work, size);

	text = win.body;
	text.height = rows;
	field = win.body.width - 2;
	if (field < 1) {
		field = 1;
	}

	for (;;) {
		tui_win_draw(&win);
		(void)tui_text_draw(&text, label, 0);
		tui_edit_draw(&ed, win.body.row + rows + 1, win.body.col + 1,
		    field);
		tui_buttons_draw(&row, &win, 1);
		tui_edit_draw(&ed, win.body.row + rows + 1, win.body.col + 1,
		    field);
		tui_cursor(1);
		tui_flush();

		tui_key_read(&key);
		tui_cursor(0);

		if (key.code == TUI_KEY_ENTER || key.code == TUI_KEY_KP_ENTER) {
			if (row.focus != 0) {
				return (TUI_CANCEL);
			}
			memcpy(out, work, strlen(work) + 1);
			return (TUI_OK);
		}
		if (key.code == TUI_KEY_ESC) {
			return (TUI_ESC);
		}
		if (key.code == TUI_KEY_TAB) {
			row.focus = (row.focus + 1) % row.count;
			continue;
		}
		if (tui_edit_key(&ed, &key)) {
			continue;
		}
		pick = tui_buttons_key(&row, &key);
		if (pick == -1) {
			return (TUI_ESC);
		}
		if (pick >= 0) {
			if (pick != 0) {
				return (TUI_CANCEL);
			}
			memcpy(out, work, strlen(work) + 1);
			return (TUI_OK);
		}
	}
}


void
tui_textbox(const char *title, const char *text)
{
	static const char *const	labels[] = { "OK" };
	tui_buttons_t			row;
	tui_win_t			win;
	tui_key_t			key;
	int				total;
	int				width;
	int				page;
	int				top;
	int				pick;

	width = tui_cols() - TUI_DLG_MARGIN;
	if (width > TUI_DLG_WIDTH_MAX + 12) {
		width = TUI_DLG_WIDTH_MAX + 12;
	}
	page = tui_rows() - 8;
	if (page < 3) {
		page = 3;
	}
	tui_win_open(&win, title, width, page + 2);
	tui_buttons_init(&row, labels, 1, 0);

	total = tui_text_rows(text, win.body.width);
	top = 0;
	for (;;) {
		tui_win_draw(&win);
		(void)tui_text_draw(&win.body, text, top);
		tui_buttons_draw(&row, &win, 1);
		tui_flush();

		tui_key_read(&key);
		switch (key.code) {
		case TUI_KEY_UP:
			if (top > 0) {
				top--;
			}
			continue;
		case TUI_KEY_DOWN:
			if (top + win.body.height < total) {
				top++;
			}
			continue;
		case TUI_KEY_PAGEUP:
			top -= win.body.height;
			if (top < 0) {
				top = 0;
			}
			continue;
		case TUI_KEY_PAGEDOWN:
			if (top + win.body.height < total) {
				top += win.body.height;
				if (top + win.body.height > total) {
					top = total - win.body.height;
				}
			}
			continue;
		case TUI_KEY_HOME:
			top = 0;
			continue;
		case TUI_KEY_END:
			top = total - win.body.height;
			if (top < 0) {
				top = 0;
			}
			continue;
		default:
			break;
		}
		pick = tui_buttons_key(&row, &key);
		if (pick != -2) {
			return;
		}
	}
}
