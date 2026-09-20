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

$define %type tui_menu_ctx_t as struct with the items and the label column width

$define %func tui_menu_row as procedure with args int, int, int, void *
$define %func tui_check_row as procedure with args int, int, int, void *
$define %func tui_menu_layout as procedure with args tui_win_t *, tui_rect_t *, const char *, const char *, int
$define %func tui_menu as function with args const char *, const char *, const tui_item_t *, int, int *
$define %func tui_checklist as function with args const char *, const char *, tui_item_t *, int

*/

/* !SPACE!

$space %internal tui_menu_ctx_t, tui_menu_row, tui_check_row, tui_menu_layout
$space %export tui_menu, tui_checklist

*/

#include <stddef.h>
#include <string.h>
#include "tui_int.h"

#define TUI_MENU_MARGIN		6
#define TUI_MENU_ROWS_MAX	12
#define TUI_MENU_ROWS_MIN	3
#define TUI_MENU_WIDTH_MIN	40
#define TUI_CHECK_MARK		4

typedef struct tui_menu_ctx {
	const tui_item_t	*items;
	int			label_width;
} tui_menu_ctx_t;


static void
tui_menu_row(int index, int width, int selected, void *ctx)
{
	const tui_menu_ctx_t	*mc;
	const tui_item_t	*item;
	int			used;

	(void)selected;
	mc = (const tui_menu_ctx_t *)ctx;
	item = &mc->items[index];

	tui_put(" ");
	used = 1;
	if (mc->label_width > 0 && used + mc->label_width <= width) {
		tui_field(item->label != NULL ? item->label : "",
		    mc->label_width);
		used += mc->label_width;
	}
	if (item->detail != NULL && used + 2 < width) {
		tui_put("  ");
		used += 2;
		tui_field(item->detail, width - used);
		return;
	}
	tui_pad(' ', width - used);
}

static void
tui_check_row(int index, int width, int selected, void *ctx)
{
	const tui_menu_ctx_t	*mc;
	const tui_item_t	*item;
	int			used;

	(void)selected;
	mc = (const tui_menu_ctx_t *)ctx;
	item = &mc->items[index];

	tui_put(item->selected ? " [x] " : " [ ] ");
	used = 5;
	if (mc->label_width > 0 && used + mc->label_width <= width) {
		tui_field(item->label != NULL ? item->label : "",
		    mc->label_width);
		used += mc->label_width;
	}
	if (item->detail != NULL && used + 2 < width) {
		tui_put("  ");
		used += 2;
		tui_field(item->detail, width - used);
		return;
	}
	tui_pad(' ', width - used);
}


static void
tui_menu_layout(tui_win_t *win, tui_rect_t *list, const char *title,
    const char *prompt, int count)
{
	tui_rect_t	text;
	int		width;
	int		rows;
	int		head;

	width = tui_cols() - TUI_MENU_MARGIN;
	if (width > TUI_DLG_WIDTH_MAX + 16) {
		width = TUI_DLG_WIDTH_MAX + 16;
	}
	if (width < TUI_MENU_WIDTH_MIN) {
		width = TUI_MENU_WIDTH_MIN;
	}

	head = tui_text_rows(prompt, width - 2);
	if (head > 4) {
		head = 4;
	}

	rows = count;
	if (rows > TUI_MENU_ROWS_MAX) {
		rows = TUI_MENU_ROWS_MAX;
	}
	if (rows < TUI_MENU_ROWS_MIN) {
		rows = TUI_MENU_ROWS_MIN;
	}
	if (rows + head + 4 > tui_rows()) {
		rows = tui_rows() - head - 4;
		if (rows < 1) {
			rows = 1;
		}
	}

	tui_win_open(win, title, width, rows + head + 3);
	text = win->body;
	text.height = head;

	list->row = win->body.row + head;
	list->col = win->body.col;
	list->width = win->body.width;
	list->height = rows;
}


int
tui_menu(const char *title, const char *prompt, const tui_item_t *items,
    int count, int *sel)
{
	static const char *const	labels[] = { "Select", "Cancel" };
	tui_menu_ctx_t			mc;
	tui_buttons_t			row;
	tui_list_t			list;
	tui_rect_t			rect;
	tui_rect_t			text;
	tui_win_t			win;
	tui_key_t			key;
	int				widest;
	int				pick;
	int				len;
	int				i;

	if (items == NULL || count <= 0 || sel == NULL) {
		return (TUI_ESC);
	}
	widest = 0;
	for (i = 0; i < count; i++) {
		len = (items[i].label != NULL) ? (int)strlen(items[i].label) : 0;
		if (len > widest) {
			widest = len;
		}
	}

	tui_menu_layout(&win, &rect, title, prompt, count);
	text = win.body;
	text.height = rect.row - win.body.row;

	if (widest > rect.width - 6) {
		widest = rect.width - 6;
	}
	if (widest < 1) {
		widest = 1;
	}
	mc.items = items;
	mc.label_width = widest;

	tui_list_init(&list, &rect, count);
	tui_list_select(&list, *sel);
	tui_buttons_init(&row, labels, 2, 0);

	for (;;) {
		tui_win_draw(&win);
		(void)tui_text_draw(&text, prompt, 0);
		tui_list_draw(&list, tui_menu_row, &mc, 1);
		tui_buttons_draw(&row, &win, 1);
		tui_flush();

		tui_key_read(&key);
		if (tui_list_key(&list, &key)) {
			continue;
		}
		if (key.code == TUI_KEY_ENTER || key.code == TUI_KEY_KP_ENTER) {
			if (row.focus != 0) {
				return (TUI_CANCEL);
			}
			*sel = list.sel;
			return (TUI_OK);
		}
		pick = tui_buttons_key(&row, &key);
		if (pick == -1) {
			return (TUI_ESC);
		}
		if (pick >= 0) {
			if (pick != 0) {
				return (TUI_CANCEL);
			}
			*sel = list.sel;
			return (TUI_OK);
		}
	}
}


int
tui_checklist(const char *title, const char *prompt, tui_item_t *items,
    int count)
{
	static const char *const	labels[] = { "OK", "Cancel" };
	int				state[TUI_TEXT_MAX / 8];
	tui_menu_ctx_t			mc;
	tui_buttons_t			row;
	tui_list_t			list;
	tui_rect_t			rect;
	tui_rect_t			text;
	tui_win_t			win;
	tui_key_t			key;
	int				widest;
	int				pick;
	int				len;
	int				i;

	if (items == NULL || count <= 0) {
		return (TUI_ESC);
	}
	if (count > (int)(sizeof(state) / sizeof(state[0]))) {
		return (TUI_ESC);
	}
	widest = 0;
	for (i = 0; i < count; i++) {
		state[i] = items[i].selected;
		len = (items[i].label != NULL) ? (int)strlen(items[i].label) : 0;
		if (len > widest) {
			widest = len;
		}
	}

	tui_menu_layout(&win, &rect, title, prompt, count);
	text = win.body;
	text.height = rect.row - win.body.row;

	if (widest > rect.width - TUI_CHECK_MARK - 6) {
		widest = rect.width - TUI_CHECK_MARK - 6;
	}
	if (widest < 1) {
		widest = 1;
	}
	mc.items = items;
	mc.label_width = widest;

	tui_list_init(&list, &rect, count);
	tui_buttons_init(&row, labels, 2, 0);

	for (;;) {
		tui_win_draw(&win);
		(void)tui_text_draw(&text, prompt, 0);
		tui_list_draw(&list, tui_check_row, &mc, 1);
		tui_buttons_draw(&row, &win, 1);
		tui_flush();

		tui_key_read(&key);
		if (tui_list_key(&list, &key)) {
			continue;
		}
		if (key.code == TUI_KEY_SPACE) {
			items[list.sel].selected = !items[list.sel].selected;
			continue;
		}
		if (key.code == TUI_KEY_ENTER || key.code == TUI_KEY_KP_ENTER) {
			if (row.focus == 0) {
				return (TUI_OK);
			}
			for (i = 0; i < count; i++) {
				items[i].selected = state[i];
			}
			return (TUI_CANCEL);
		}
		if (key.code == TUI_KEY_ESC) {
			for (i = 0; i < count; i++) {
				items[i].selected = state[i];
			}
			return (TUI_ESC);
		}
		pick = tui_buttons_key(&row, &key);
		if (pick == 0) {
			return (TUI_OK);
		}
		if (pick > 0) {
			for (i = 0; i < count; i++) {
				items[i].selected = state[i];
			}
			return (TUI_CANCEL);
		}
	}
}
