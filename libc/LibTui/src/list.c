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

$define %func tui_list_clamp as procedure with args tui_list_t *
$define %func tui_list_init as procedure with args tui_list_t *, const tui_rect_t *, int
$define %func tui_list_count_set as procedure with args tui_list_t *, int
$define %func tui_list_select as procedure with args tui_list_t *, int
$define %func tui_list_draw as procedure with args tui_list_t *, tui_row_fn, void *, int
$define %func tui_list_key as function with args tui_list_t *, const tui_key_t *
$define %func tui_edit_init as procedure with args tui_edit_t *, char *, size_t
$define %func tui_edit_draw as procedure with args const tui_edit_t *, int, int, int
$define %func tui_edit_key as function with args tui_edit_t *, const tui_key_t *

*/

/* !SPACE!

$space %internal tui_list_clamp
$space %export tui_list_init, tui_list_count_set, tui_list_select
$space %export tui_list_draw, tui_list_key
$space %export tui_edit_init, tui_edit_draw, tui_edit_key

*/

#include <stddef.h>
#include <string.h>
#include "tui_int.h"


static void
tui_list_clamp(tui_list_t *list)
{
	int	rows;

	rows = list->rect.height;
	if (rows < 1) {
		rows = 1;
	}
	if (list->count <= 0) {
		list->count = 0;
		list->sel = 0;
		list->off = 0;
		return;
	}
	if (list->sel < 0) {
		list->sel = 0;
	}
	if (list->sel >= list->count) {
		list->sel = list->count - 1;
	}
	if (list->off > list->sel) {
		list->off = list->sel;
	}
	if (list->sel >= list->off + rows) {
		list->off = list->sel - rows + 1;
	}
	if (list->off > list->count - rows) {
		list->off = list->count - rows;
	}
	if (list->off < 0) {
		list->off = 0;
	}
}

void
tui_list_init(tui_list_t *list, const tui_rect_t *rect, int count)
{
	if (list == NULL || rect == NULL) {
		return;
	}
	memset(list, 0, sizeof(*list));
	list->rect = *rect;
	list->count = count;
	tui_list_clamp(list);
}

void
tui_list_count_set(tui_list_t *list, int count)
{
	if (list == NULL) {
		return;
	}
	list->count = count;
	tui_list_clamp(list);
}

void
tui_list_select(tui_list_t *list, int index)
{
	if (list == NULL) {
		return;
	}
	list->sel = index;
	tui_list_clamp(list);
}


void
tui_list_draw(tui_list_t *list, tui_row_fn row, void *ctx, int focused)
{
	const tui_theme_t	*th;
	int			index;
	int			sel;
	int			y;

	if (list == NULL) {
		return;
	}
	th = tui_theme();
	tui_list_clamp(list);

	for (y = 0; y < list->rect.height; y++) {
		index = list->off + y;
		sel = (index == list->sel && list->count > 0);
		tui_move(list->rect.row + y, list->rect.col);
		if (sel && focused) {
			tui_attr_bold(th->sel_fg, th->sel_bg);
		} else if (sel) {

			tui_attr(th->sel_idle_fg, th->sel_idle_bg);
		} else {
			tui_attr(th->text_fg, th->win_bg);
		}
		if (index >= list->count || row == NULL) {
			tui_pad(' ', list->rect.width);
			continue;
		}
		row(index, list->rect.width, sel, ctx);
	}
}


int
tui_list_key(tui_list_t *list, const tui_key_t *key)
{
	int	page;

	if (list == NULL || key == NULL) {
		return (0);
	}
	page = list->rect.height;
	if (page < 1) {
		page = 1;
	}
	switch (key->code) {
	case TUI_KEY_UP:
		list->sel--;
		break;
	case TUI_KEY_DOWN:
		list->sel++;
		break;
	case TUI_KEY_PAGEUP:
		list->sel -= page;
		break;
	case TUI_KEY_PAGEDOWN:
		list->sel += page;
		break;
	case TUI_KEY_HOME:
		list->sel = 0;
		break;
	case TUI_KEY_END:
		list->sel = list->count - 1;
		break;
	default:
		return (0);
	}
	tui_list_clamp(list);
	return (1);
}

void
tui_edit_init(tui_edit_t *ed, char *buf, size_t size)
{
	if (ed == NULL) {
		return;
	}
	memset(ed, 0, sizeof(*ed));
	if (buf == NULL || size == 0) {
		return;
	}
	ed->buf = buf;
	ed->size = size;
	buf[size - 1] = '\0';
	ed->len = strlen(buf);
	ed->caret = ed->len;
}


void
tui_edit_draw(const tui_edit_t *ed, int row, int col, int width)
{
	const tui_theme_t	*th;
	size_t			view;

	if (ed == NULL || ed->buf == NULL || width < 1) {
		return;
	}
	th = tui_theme();
	view = 0;
	if (ed->caret >= (size_t)width) {
		view = ed->caret - (size_t)width + 1;
	}
	tui_move(row, col);
	tui_attr(th->field_fg, th->field_bg);
	tui_field(ed->buf + view, width);
	tui_move(row, col + (int)(ed->caret - view));
}

int
tui_edit_key(tui_edit_t *ed, const tui_key_t *key)
{
	size_t	i;

	if (ed == NULL || ed->buf == NULL || key == NULL) {
		return (0);
	}
	switch (key->code) {
	case TUI_KEY_LEFT:
		if (ed->caret > 0) {
			ed->caret--;
		}
		return (1);
	case TUI_KEY_RIGHT:
		if (ed->caret < ed->len) {
			ed->caret++;
		}
		return (1);
	case TUI_KEY_HOME:
		ed->caret = 0;
		return (1);
	case TUI_KEY_END:
		ed->caret = ed->len;
		return (1);
	case TUI_KEY_BACKSPACE:
		if (ed->caret == 0) {
			return (1);
		}
		for (i = ed->caret - 1; i + 1 < ed->len; i++) {
			ed->buf[i] = ed->buf[i + 1];
		}
		ed->len--;
		ed->caret--;
		ed->buf[ed->len] = '\0';
		return (1);
	case TUI_KEY_DELETE:
		if (ed->caret >= ed->len) {
			return (1);
		}
		for (i = ed->caret; i + 1 < ed->len; i++) {
			ed->buf[i] = ed->buf[i + 1];
		}
		ed->len--;
		ed->buf[ed->len] = '\0';
		return (1);
	default:
		break;
	}

	if (key->ch < 32 || key->ch > 126) {
		return (0);
	}
	if (ed->len + 1 >= ed->size) {
		return (1);
	}
	for (i = ed->len; i > ed->caret; i--) {
		ed->buf[i] = ed->buf[i - 1];
	}
	ed->buf[ed->caret] = (char)key->ch;
	ed->len++;
	ed->caret++;
	ed->buf[ed->len] = '\0';
	return (1);
}
