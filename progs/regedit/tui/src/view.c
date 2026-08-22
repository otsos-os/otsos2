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
$define %type rt_state as regedit tui global state

$define %func rt_pane_width as function with args rt_state *
$define %func rt_tail as function with args const char *, int
$define %func rt_clamp as procedure with args rt_state *
$define %func rt_draw_header as procedure with args rt_state *, int
$define %func rt_draw_columns as procedure with args rt_state *, int
$define %func rt_key_row as procedure with args rt_state *, int, int
$define %func rt_value_row as procedure with args rt_state *, int, int
$define %func rt_draw_rows as procedure with args rt_state *, int
$define %func rt_draw_status as procedure with args rt_state *
$define %func rt_draw as procedure with args rt_state *
$define %func rt_help as procedure with args rt_state *

*/

/* !SPACE!

$space %internal rt_pane_width, rt_tail, rt_clamp, rt_draw_header
$space %internal rt_draw_columns, rt_key_row, rt_value_row
$space %internal rt_draw_rows, rt_draw_status
$space %export rt_draw, rt_help

*/

#include <native.h>
#include <regedit/regedit.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

#define RT_COLOR_HEADER		36
#define RT_COLOR_COLUMN		96
#define RT_COLOR_SELECT		92
#define RT_COLOR_INACTIVE	97
#define RT_COLOR_PLAIN		37
#define RT_COLOR_DIM		90
#define RT_COLOR_STATUS		33
#define RT_COLOR_TYPE		94

static int
rt_pane_width(const rt_state_t *st)
{
	int	width;

	width = (st->cols * RT_PANE_SHARE) / 100;
	if (width < RT_PANE_MIN) {
		width = RT_PANE_MIN;
	}
	if (width > st->cols - RT_PANE_MIN - 1) {
		width = st->cols - RT_PANE_MIN - 1;
	}
	if (width < 1) {
		width = 1;
	}
	return (width);
}

static const char *
rt_tail(const char *text, int width)
{
	int	len;

	len = (int)strlen(text);
	if (width <= 0 || len <= width) {
		return (text);
	}
	return (text + (len - width));
}

static void
rt_clamp(rt_state_t *st)
{
	int	rows, keys, values;

	rows = rt_rows_visible(st);
	keys = rt_key_count(st);
	values = (int)st->values.count;

	if (st->key_sel >= keys) {
		st->key_sel = keys - 1;
	}
	if (st->key_sel < 0) {
		st->key_sel = 0;
	}
	if (st->value_sel >= values) {
		st->value_sel = values - 1;
	}
	if (st->value_sel < 0) {
		st->value_sel = 0;
	}
	if (st->key_sel < st->key_off) {
		st->key_off = st->key_sel;
	}
	if (st->key_sel >= st->key_off + rows) {
		st->key_off = st->key_sel - rows + 1;
	}
	if (st->key_off > keys - rows) {
		st->key_off = keys - rows;
	}
	if (st->key_off < 0) {
		st->key_off = 0;
	}
	if (st->value_sel < st->value_off) {
		st->value_off = st->value_sel;
	}
	if (st->value_sel >= st->value_off + rows) {
		st->value_off = st->value_sel - rows + 1;
	}
	if (st->value_off > values - rows) {
		st->value_off = values - rows;
	}
	if (st->value_off < 0) {
		st->value_off = 0;
	}
}

static void
rt_draw_header(rt_state_t *st, int pane)
{
	char	path[RT_STATUS_MAX];
	char	line[RT_STATUS_MAX];

	(void)pane;
	if (re_path_text(&st->path, path, sizeof(path)) != 0) {
		snprintf(path, sizeof(path), "%s", "(path too long)");
	}
	snprintf(line, sizeof(line), " regedit  %s",
	    rt_tail(path, st->cols - 11));
	rt_screen_move(1, 1);
	rt_screen_color(RT_COLOR_HEADER);
	rt_screen_field(line, st->cols);
	rt_screen_reset();
}

static void
rt_draw_columns(rt_state_t *st, int pane)
{
	rt_screen_move(2, 1);
	rt_screen_color(RT_COLOR_COLUMN);
	rt_screen_field(st->path.hive[0] == '\0' ? " Hives" : " Keys",
	    pane);
	rt_screen_put("|");
	rt_screen_field(" Values", st->cols - pane - 1);
	rt_screen_reset();
}

static void
rt_key_row(rt_state_t *st, int index, int pane)
{
	char	line[RE_PATH_MAX];
	int	selected, locked;

	selected = (index == st->key_sel);
	locked = 0;
	if (st->path.hive[0] == '\0') {
		locked = st->hives.items[index].access == RE_ACCESS_NONE;
		snprintf(line, sizeof(line), "%s%s%s",
		    selected ? ">" : " ", st->hives.items[index].name,
		    locked ? " [locked]" : "");
	} else {
		snprintf(line, sizeof(line), "%s%s.",
		    selected ? ">" : " ", st->keys.items[index].name);
	}
	if (locked) {
		rt_screen_color(RT_COLOR_DIM);
	} else if (selected && st->focus == RT_FOCUS_KEYS) {
		rt_screen_color(RT_COLOR_SELECT);
	} else if (selected) {
		rt_screen_color(RT_COLOR_INACTIVE);
	} else {
		rt_screen_color(RT_COLOR_PLAIN);
	}
	rt_screen_field(line, pane);
	rt_screen_reset();
}

static void
rt_value_row(rt_state_t *st, int index, int width)
{
	const re_value_t	*item;
	char			line[RT_STATUS_MAX];
	int			selected, name_w, type_w, data_w;

	item = &st->values.items[index];
	selected = (index == st->value_sel);
	name_w = width / 3;
	if (name_w > RE_NAME_MAX) {
		name_w = RE_NAME_MAX;
	}
	if (name_w < 6) {
		name_w = 6;
	}
	type_w = 13;
	if (type_w > width - name_w - 2) {
		type_w = width - name_w - 2;
	}
	if (type_w < 0) {
		type_w = 0;
	}
	data_w = width - name_w - type_w - 1;
	if (data_w < 0) {
		data_w = 0;
	}

	if (selected && st->focus == RT_FOCUS_VALUES) {
		rt_screen_color(RT_COLOR_SELECT);
	} else if (selected) {
		rt_screen_color(RT_COLOR_INACTIVE);
	} else {
		rt_screen_color(RT_COLOR_PLAIN);
	}
	snprintf(line, sizeof(line), "%s%s", selected ? ">" : " ",
	    item->name);
	rt_screen_field(line, name_w);
	if (type_w > 0) {
		if (!selected) {
			rt_screen_color(RT_COLOR_TYPE);
		}
		snprintf(line, sizeof(line), "%s", re_type_name(item->type));
		rt_screen_field(line, type_w);
		if (!selected) {
			rt_screen_color(RT_COLOR_PLAIN);
		}
	}
	if (data_w > 0) {
		snprintf(line, sizeof(line), " %s", item->preview);
		rt_screen_field(line, data_w);
	}
	rt_screen_reset();
}

static void
rt_draw_rows(rt_state_t *st, int pane)
{
	int	rows, right, y, key_index, value_index, keys, values;

	rows = rt_rows_visible(st);
	right = st->cols - pane - 1;
	keys = rt_key_count(st);
	values = (int)st->values.count;
	for (y = 0; y < rows; y++) {
		rt_screen_move(y + 3, 1);
		rt_screen_reset();
		rt_screen_erase_line();
		key_index = st->key_off + y;
		if (key_index < keys) {
			rt_key_row(st, key_index, pane);
		} else if (y == 0 && keys == 0) {
			rt_screen_color(RT_COLOR_DIM);
			rt_screen_field(st->loaded ? " (no keys)" :
			    " (unavailable)", pane);
			rt_screen_reset();
		} else {
			rt_screen_pad(' ', pane);
		}
		rt_screen_color(RT_COLOR_DIM);
		rt_screen_put("|");
		rt_screen_reset();
		value_index = st->value_off + y;
		if (st->path.hive[0] == '\0') {
			if (y == 0) {
				rt_screen_color(RT_COLOR_DIM);
				rt_screen_field(" select a hive", right);
				rt_screen_reset();
			}
			continue;
		}
		if (value_index < values) {
			rt_value_row(st, value_index, right);
		} else if (y == 0 && values == 0) {
			rt_screen_color(RT_COLOR_DIM);
			rt_screen_field(st->loaded ? " (no values)" :
			    " (unavailable)", right);
			rt_screen_reset();
		}
	}
}

static void
rt_draw_status(rt_state_t *st)
{
	char	line[RT_STATUS_MAX];
	int	keys, values;

	keys = rt_key_count(st);
	values = (int)st->values.count;
	snprintf(line, sizeof(line),
	    " %s %d/%d | values %d/%d | focus %s%s%s",
	    st->path.hive[0] == '\0' ? "hives" : "keys",
	    keys != 0 ? st->key_sel + 1 : 0, keys,
	    values != 0 ? st->value_sel + 1 : 0, values,
	    st->focus == RT_FOCUS_KEYS ? "keys" : "values",
	    st->keys.truncated || st->values.truncated ? " | " : "",
	    st->keys.truncated || st->values.truncated ?
	    "list truncated" : "");
	rt_screen_move(st->rows - 1, 1);
	rt_screen_color(RT_COLOR_HEADER);
	rt_screen_field(line, st->cols);
	rt_screen_reset();

	rt_screen_move(st->rows, 1);
	rt_screen_color(RT_COLOR_STATUS);
	snprintf(line, sizeof(line), " %s", st->status);
	rt_screen_field_tail(line, st->cols);
	rt_screen_reset();
}

void
rt_draw(rt_state_t *st)
{
	int	pane;

	rt_screen_size(st);
	rt_clamp(st);
	pane = rt_pane_width(st);
	rt_screen_home();
	rt_draw_header(st, pane);
	rt_draw_columns(st, pane);
	rt_draw_rows(st, pane);
	rt_draw_status(st);
	rt_screen_move(st->rows, 1);
}

void
rt_help(rt_state_t *st)
{
	struct api_key_event	ev;

	rt_screen_clear();
	rt_screen_color(RT_COLOR_HEADER);
	rt_screen_put("regedit help\r\n\r\n");
	rt_screen_reset();
	rt_screen_put("Up/Down/PageUp/PageDown/Home/End  move\r\n");
	rt_screen_put("Tab or Left/Right                 switch pane\r\n");
	rt_screen_put("Enter                             open key or "
	    "edit value\r\n");
	rt_screen_put("Backspace                         go up one "
	    "level\r\n");
	rt_screen_put("F2                                new value in "
	    "current key\r\n");
	rt_screen_put("F3                                new subkey in "
	    "current key\r\n");
	rt_screen_put("F5 or Ctrl+R                      reload\r\n");
	rt_screen_put("Delete                            delete selected "
	    "key or value\r\n");
	rt_screen_put("Ctrl+G                            go to path\r\n");
	rt_screen_put("Ctrl+S                            notify a "
	    "registry consumer\r\n");
	rt_screen_put("Ctrl+Q                            quit\r\n");
	rt_screen_put("F1                                this help\r\n");
	rt_screen_put("\r\nValue text formats:\r\n");
	rt_screen_put("  string        raw text\r\n");
	rt_screen_put("  bool          true or false\r\n");
	rt_screen_put("  i32/u32/u64   decimal\r\n");
	rt_screen_put("  ipv4          a.b.c.d\r\n");
	rt_screen_put("  bytes         hex pairs, e.g. 0a ff 10\r\n");
	rt_screen_put("  multi_string  items separated by |\r\n");
	rt_screen_put("\r\nPress any key to return.");
	rt_key_read(&ev);
	rt_screen_clear();
	rt_status(st, "Help closed");
}
