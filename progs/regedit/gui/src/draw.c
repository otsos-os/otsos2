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

$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %type libg_rect as pixel rectangle
$define %type rg_layout as per frame pixel geometry of every hit region
$define %type rg_state as regedit gui global state

$define %func rg_clip_text as procedure with args out, size, text, cells
$define %func rg_text_at as procedure with args ctx, rect, text, color, cells
$define %func rg_draw_header as procedure with args rg_state *, layout
$define %func rg_draw_crumbs as procedure with args rg_state *, layout
$define %func rg_draw_toolbar as procedure with args rg_state *, layout
$define %func rg_draw_columns as procedure with args rg_state *, layout
$define %func rg_draw_bar as procedure with args ctx, style, rect, off, rows, count
$define %func rg_draw_keys as procedure with args rg_state *, layout
$define %func rg_draw_values as procedure with args rg_state *, layout
$define %func rg_draw_status as procedure with args rg_state *, layout
$define %func rg_draw_field as procedure with args rg_state *, layout
$define %func rg_draw_dialog as procedure with args rg_state *, layout
$define %func rg_draw_help as procedure with args rg_state *
$define %func rg_draw as procedure with args rg_state *

*/

/* !SPACE!

$space %internal rg_clip_text, rg_text_at, rg_draw_header, rg_draw_crumbs
$space %internal rg_draw_toolbar, rg_draw_columns, rg_draw_bar, rg_draw_keys
$space %internal rg_draw_values, rg_draw_status, rg_draw_field
$space %internal rg_draw_dialog, rg_draw_help
$space %export rg_draw

*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

/* Row insets: text baseline offset inside a row, and the left gutter. */
#define RG_TEXT_DY		((RG_ROW_H - RG_GLYPH_H) / 2)
#define RG_GUTTER		6

/* Locked rows and the selection bar are drawn from these, not from style. */
#define RG_COL_ROW_ALT		0xff1b1f24U
#define RG_COL_SEL		0xff1f5f74U
#define RG_COL_SEL_BLUR		0xff2a3038U
#define RG_COL_HOVER		0xff262c34U
#define RG_COL_SHADE		0xff0b0d10U
#define RG_COL_BAR		0xff394250U

#define RG_HELP_LINES		14

static const char	*rg_help_text[RG_HELP_LINES] = {
	"Registry Editor",
	"",
	"Enter / double click   open key, edit value",
	"Backspace / Left       leave key",
	"Tab / Left / Right     switch pane",
	"Up Down PgUp PgDn      move selection",
	"Home / End             first / last row",
	"Delete                 delete key or value",
	"F2 / Ctrl+N            new value",
	"F3 / Ctrl+A            new key",
	"F4 / Ctrl+G            go to path",
	"Ctrl+S                 notify a consumer",
	"F5 / Ctrl+R            reload",
	"F1 close this  F11 fullscreen  Ctrl+Q quit"
};

/*
 * Copy at most `cells` glyphs of text, ending with ".." when it did not fit.
 * Truncating in the copy rather than clipping in the blit keeps every column
 * boundary honest: a name can never bleed into the column next to it.
 */
static void
rg_clip_text(char *out, size_t size, const char *text, int cells)
{
	size_t	len;

	if (size == 0) {
		return;
	}
	out[0] = '\0';
	if (!text || cells <= 0) {
		return;
	}
	if ((size_t)cells > size - 1) {
		cells = (int)(size - 1);
	}
	len = strnlen(text, size);
	if (len <= (size_t)cells) {
		memcpy(out, text, len);
		out[len] = '\0';
		return;
	}
	if (cells <= 2) {
		memcpy(out, "..", (size_t)cells);
		out[cells] = '\0';
		return;
	}
	memcpy(out, text, (size_t)cells - 2);
	out[cells - 2] = '.';
	out[cells - 1] = '.';
	out[cells] = '\0';
}

static void
rg_text_at(libg_context_t *ctx, libg_rect_t rect, const char *text,
    uint32_t color, int cells)
{
	char	buf[RG_STATUS_MAX];

	rg_clip_text(buf, sizeof(buf), text, cells);
	if (buf[0] == '\0') {
		return;
	}
	libgTextScale(ctx, rect.x, rect.y + (rect.height - RG_GLYPH_H) / 2,
	    buf, color, RG_TEXT_SCALE);
}

static void
rg_draw_header(rg_state_t *st, const rg_layout_t *lay)
{
	char		text[RE_PATH_MAX + 32];
	char		path[RE_PATH_MAX];
	libg_rect_t	rect;
	int32_t		w;

	libgFillRect(st->ui, lay->header, st->style.panel);
	libgLine(st->ui, lay->header.x, lay->header.y + lay->header.height - 1,
	    lay->header.x + lay->header.width,
	    lay->header.y + lay->header.height - 1, st->style.panel_border);

	rect = lay->header;
	rect.x += RG_PAD;
	rect.width -= 2 * RG_PAD;
	if (re_path_text(&st->path, path, sizeof(path)) != 0) {
		snprintf(path, sizeof(path), "(hives)");
	}
	snprintf(text, sizeof(text), "%s  %s", RG_TITLE, path);
	rg_text_at(st->ui, rect, text, st->style.text,
	    rect.width / RG_GLYPH_W);

	snprintf(text, sizeof(text), "%u keys  %u values",
	    st->path.hive[0] == '\0' ? st->hives.count : st->keys.count,
	    st->values.count);
	w = (int32_t)strlen(text) * RG_GLYPH_W;
	if (w < rect.width / 2) {
		rect.x = lay->header.x + lay->header.width - RG_PAD - w;
		rect.width = w;
		rg_text_at(st->ui, rect, text, st->style.text_muted,
		    rect.width / RG_GLYPH_W);
	}
}

static void
rg_draw_crumbs(rg_state_t *st, const rg_layout_t *lay)
{
	const rg_crumb_t	*crumb;
	libg_rect_t		rect;
	uint32_t		color;
	int			i, hot, last;

	libgFillRect(st->ui, lay->crumbs, st->style.background);
	last = -1;
	for (i = 0; i < st->crumb_count; i++) {
		if (st->crumbs[i].rect.width > 0) {
			last = i;
		}
	}
	for (i = 0; i < st->crumb_count; i++) {
		crumb = &st->crumbs[i];
		if (crumb->rect.width <= 0) {
			continue;
		}
		hot = rg_rect_hit(crumb->rect, st->mouse_x, st->mouse_y);
		if (i == last) {
			libgFillRect(st->ui, crumb->rect,
			    st->style.control_active);
		} else if (hot) {
			libgFillRect(st->ui, crumb->rect,
			    st->style.control_hot);
		} else {
			libgFillRect(st->ui, crumb->rect, st->style.control);
		}
		color = i == last ? st->style.text :
		    (hot ? st->style.accent_hot : st->style.text_muted);
		rect = crumb->rect;
		rect.x += (crumb->rect.width -
		    (int32_t)strlen(crumb->label) * RG_GLYPH_W) / 2;
		rg_text_at(st->ui, rect, crumb->label, color,
		    (int)strlen(crumb->label));
		if (i != last) {
			rect = crumb->rect;
			rect.x = crumb->rect.x + crumb->rect.width + 2;
			rect.width = RG_GLYPH_W;
			rg_text_at(st->ui, rect, ">", st->style.panel_border,
			    1);
		}
	}
}

static void
rg_draw_toolbar(rg_state_t *st, const rg_layout_t *lay)
{
	libg_rect_t	rect;
	const char	*label;
	uint32_t	fill, color;
	int		i, hot, off;

	libgFillRect(st->ui, lay->toolbar, st->style.panel);
	libgLine(st->ui, lay->toolbar.x,
	    lay->toolbar.y + lay->toolbar.height - 1,
	    lay->toolbar.x + lay->toolbar.width,
	    lay->toolbar.y + lay->toolbar.height - 1, st->style.panel_border);

	for (i = 0; i < RG_BTN_COUNT; i++) {
		if (lay->buttons[i].width <= 0) {
			continue;
		}
		label = rg_button_label(i);
		off = 0;
		if (st->path.hive[0] == '\0' &&
		    (i == RG_BTN_UP || i == RG_BTN_NEW_KEY ||
		    i == RG_BTN_NEW_VALUE || i == RG_BTN_DELETE)) {
			off = 1;
		}
		hot = rg_rect_hit(lay->buttons[i], st->mouse_x, st->mouse_y);
		fill = st->style.control;
		if (hot) {
			fill = (st->buttons & SRAPI_MOUSE_LEFT) != 0 ?
			    st->style.control_active : st->style.control_hot;
		}
		libgFillRect(st->ui, lay->buttons[i], fill);
		libgStrokeRect(st->ui, lay->buttons[i],
		    hot ? st->style.accent : st->style.panel_border);
		color = off ? st->style.panel_border : st->style.text;
		if (i == RG_BTN_DELETE && !off) {
			color = hot ? st->style.danger : st->style.text;
		}
		rect = lay->buttons[i];
		rect.x += (rect.width -
		    (int32_t)strlen(label) * RG_GLYPH_W) / 2;
		rg_text_at(st->ui, rect, label, color, (int)strlen(label));
	}
}

static void
rg_draw_columns(rg_state_t *st, const rg_layout_t *lay)
{
	libg_rect_t	rect;

	libgFillRect(st->ui, lay->columns, st->style.background);
	libgLine(st->ui, lay->columns.x,
	    lay->columns.y + lay->columns.height - 1,
	    lay->columns.x + lay->columns.width,
	    lay->columns.y + lay->columns.height - 1, st->style.panel_border);

	rect = lay->columns;
	rect.x = lay->keys.x + RG_GUTTER;
	rect.width = lay->keys.width - RG_GUTTER;
	rg_text_at(st->ui, rect,
	    st->path.hive[0] == '\0' ? "Hive" : "Key",
	    st->style.text_muted, rect.width / RG_GLYPH_W);

	rect.x = lay->values.x + RG_GUTTER;
	rect.width = lay->name_w - RG_GUTTER;
	rg_text_at(st->ui, rect, "Name", st->style.text_muted,
	    rect.width / RG_GLYPH_W);

	rect.x = lay->values.x + lay->name_w;
	rect.width = lay->type_w;
	rg_text_at(st->ui, rect, "Type", st->style.text_muted,
	    rect.width / RG_GLYPH_W);

	rect.x = lay->values.x + lay->name_w + lay->type_w;
	rect.width = lay->data_w;
	rg_text_at(st->ui, rect, "Data", st->style.text_muted,
	    rect.width / RG_GLYPH_W);
}

static void
rg_draw_bar(libg_context_t *ctx, const libg_style_t *style, libg_rect_t rect,
    int off, int rows, int count)
{
	libg_rect_t	thumb;
	int32_t		h, y;

	libgFillRect(ctx, rect, style->field);
	if (count <= rows || count <= 0 || rect.height <= 0) {
		return;
	}
	h = rect.height * rows / count;
	if (h < RG_ROW_H / 2) {
		h = RG_ROW_H / 2;
	}
	if (h > rect.height) {
		h = rect.height;
	}
	y = rect.height - h > 0 ?
	    (int32_t)(((int64_t)off * (rect.height - h)) / (count - rows)) : 0;
	thumb = rect;
	thumb.x += 2;
	thumb.width -= 4;
	thumb.y += y;
	thumb.height = h;
	if (thumb.width < 1) {
		thumb.width = 1;
	}
	libgFillRect(ctx, thumb, RG_COL_BAR);
}

static void
rg_draw_keys(rg_state_t *st, const rg_layout_t *lay)
{
	libg_rect_t	row, text;
	const char	*name;
	uint32_t	color;
	int		i, index, count, locked, focus, sel;

	libgFillRect(st->ui, lay->keys, st->style.background);
	count = rg_row_count(st, RG_FOCUS_KEYS);
	focus = st->focus == RG_FOCUS_KEYS;
	sel = st->key_sel;

	for (i = 0; i < lay->rows; i++) {
		index = st->key_off + i;
		row = lay->keys;
		row.y = lay->keys.y + i * RG_ROW_H;
		row.height = RG_ROW_H;
		if (row.y + row.height > lay->keys.y + lay->keys.height) {
			break;
		}
		if (index >= count) {
			break;
		}
		locked = 0;
		if (st->path.hive[0] == '\0') {
			name = st->hives.items[index].name;
			locked = st->hives.items[index].access ==
			    RE_ACCESS_NONE;
		} else {
			name = st->keys.items[index].name;
		}
		if (index == sel) {
			libgFillRect(st->ui, row,
			    focus ? RG_COL_SEL : RG_COL_SEL_BLUR);
		} else if (rg_rect_hit(row, st->mouse_x, st->mouse_y)) {
			libgFillRect(st->ui, row, RG_COL_HOVER);
		} else if ((index & 1) != 0) {
			libgFillRect(st->ui, row, RG_COL_ROW_ALT);
		}
		color = locked ? st->style.panel_border : st->style.text;
		if (index == sel && focus) {
			color = st->style.text;
		}
		text = row;
		text.x += RG_GUTTER;
		text.width -= 2 * RG_GUTTER;
		rg_text_at(st->ui, text, name, color,
		    text.width / RG_GLYPH_W);
		if (locked) {
			text.x = row.x + row.width - RG_GUTTER - RG_GLYPH_W;
			text.width = RG_GLYPH_W;
			rg_text_at(st->ui, text, "-", st->style.danger, 1);
		}
	}

	if (count == 0) {
		row = lay->keys;
		row.x += RG_GUTTER;
		row.height = RG_ROW_H;
		rg_text_at(st->ui, row,
		    st->loaded ? "(no subkeys)" : "(unavailable)",
		    st->style.text_muted, row.width / RG_GLYPH_W);
	}
	if (st->keys.truncated != 0 && st->path.hive[0] != '\0') {
		row = lay->keys;
		row.x += RG_GUTTER;
		row.y += lay->keys.height - RG_ROW_H;
		row.height = RG_ROW_H;
		libgFillRect(st->ui, row, st->style.panel);
		rg_text_at(st->ui, row, "list truncated", st->style.danger,
		    row.width / RG_GLYPH_W);
	}
	rg_draw_bar(st->ui, &st->style, lay->keys_bar, st->key_off, lay->rows,
	    count);
	libgLine(st->ui, lay->values.x - RG_SCROLL_W, lay->keys.y,
	    lay->values.x - RG_SCROLL_W, lay->keys.y + lay->keys.height,
	    st->style.panel_border);
}

static void
rg_draw_values(rg_state_t *st, const rg_layout_t *lay)
{
	const re_value_t	*item;
	libg_rect_t		row, text;
	uint32_t		color;
	int			i, index, count, focus, sel;

	libgFillRect(st->ui, lay->values, st->style.background);
	count = (int)st->values.count;
	focus = st->focus == RG_FOCUS_VALUES;
	sel = st->value_sel;

	for (i = 0; i < lay->rows; i++) {
		index = st->value_off + i;
		row = lay->values;
		row.y = lay->values.y + i * RG_ROW_H;
		row.height = RG_ROW_H;
		if (row.y + row.height > lay->values.y + lay->values.height) {
			break;
		}
		if (index >= count) {
			break;
		}
		item = &st->values.items[index];
		if (index == sel) {
			libgFillRect(st->ui, row,
			    focus ? RG_COL_SEL : RG_COL_SEL_BLUR);
		} else if (rg_rect_hit(row, st->mouse_x, st->mouse_y)) {
			libgFillRect(st->ui, row, RG_COL_HOVER);
		} else if ((index & 1) != 0) {
			libgFillRect(st->ui, row, RG_COL_ROW_ALT);
		}

		text = row;
		text.x = lay->values.x + RG_GUTTER;
		text.width = lay->name_w - RG_GUTTER;
		rg_text_at(st->ui, text, item->name, st->style.text,
		    text.width / RG_GLYPH_W);

		text.x = lay->values.x + lay->name_w;
		text.width = lay->type_w;
		rg_text_at(st->ui, text, re_type_name(item->type),
		    st->style.accent, text.width / RG_GLYPH_W);

		text.x = lay->values.x + lay->name_w + lay->type_w;
		text.width = lay->data_w;
		color = item->readable != 0 ? st->style.text_muted :
		    st->style.danger;
		rg_text_at(st->ui, text,
		    item->readable != 0 ? item->preview : "(denied)", color,
		    text.width / RG_GLYPH_W);
	}

	if (count == 0) {
		row = lay->values;
		row.x += RG_GUTTER;
		row.height = RG_ROW_H;
		rg_text_at(st->ui, row,
		    st->path.hive[0] == '\0' ? "(select a hive)" :
		    "(no values)", st->style.text_muted,
		    row.width / RG_GLYPH_W);
	}
	if (st->values.truncated != 0) {
		row = lay->values;
		row.x += RG_GUTTER;
		row.y += lay->values.height - RG_ROW_H;
		row.height = RG_ROW_H;
		libgFillRect(st->ui, row, st->style.panel);
		rg_text_at(st->ui, row, "list truncated", st->style.danger,
		    row.width / RG_GLYPH_W);
	}
	rg_draw_bar(st->ui, &st->style, lay->values_bar, st->value_off,
	    lay->rows, count);
}

static void
rg_draw_status(rg_state_t *st, const rg_layout_t *lay)
{
	libg_rect_t	rect;
	const char	*hint;
	int32_t		w;

	libgFillRect(st->ui, lay->status, st->style.panel);
	libgLine(st->ui, lay->status.x, lay->status.y, lay->status.x +
	    lay->status.width, lay->status.y, st->style.panel_border);

	rect = lay->status;
	rect.x += RG_PAD;
	rect.width -= 2 * RG_PAD;
	if (st->status[0] != '\0') {
		rg_text_at(st->ui, rect, st->status, st->style.text,
		    rect.width / RG_GLYPH_W);
		return;
	}
	hint = st->focus == RG_FOCUS_VALUES ?
	    "Enter edit  Del delete  Tab keys  F1 help" :
	    "Enter open  Bksp up  Tab values  F1 help";
	w = (int32_t)strlen(hint) * RG_GLYPH_W;
	if (w <= rect.width) {
		rect.x = lay->status.x + lay->status.width - RG_PAD - w;
	}
	rg_text_at(st->ui, rect, hint, st->style.text_muted,
	    rect.width / RG_GLYPH_W);
}

static void
rg_draw_field(rg_state_t *st, const rg_layout_t *lay)
{
	char		buf[RE_TEXT_MAX];
	libg_rect_t	rect;
	int32_t		cx;
	int		cells, len, caret;

	rect = lay->dlg_field;
	libgFillRect(st->ui, rect, st->style.field_focus);
	libgStrokeRect(st->ui, rect, st->style.accent);

	cells = (rect.width - 2 * RG_GUTTER) / RG_GLYPH_W;
	if (cells < 1) {
		cells = 1;
	}
	len = (int)strnlen(st->dialog.text, sizeof(st->dialog.text) - 1);
	caret = st->dialog.caret;
	if (caret > len) {
		caret = len;
	}
	if (caret < 0) {
		caret = 0;
	}
	if (st->dialog.view > caret) {
		st->dialog.view = caret;
	}
	if (caret - st->dialog.view >= cells) {
		st->dialog.view = caret - cells + 1;
	}
	if (st->dialog.view > len) {
		st->dialog.view = len;
	}
	if (st->dialog.view < 0) {
		st->dialog.view = 0;
	}

	snprintf(buf, sizeof(buf), "%.*s", cells,
	    st->dialog.text + st->dialog.view);
	rect.x += RG_GUTTER;
	rect.width -= 2 * RG_GUTTER;
	rg_text_at(st->ui, rect, buf, st->style.text, cells);

	cx = rect.x + (caret - st->dialog.view) * RG_GLYPH_W;
	libgLine(st->ui, cx, rect.y + (rect.height - RG_GLYPH_H) / 2, cx,
	    rect.y + (rect.height + RG_GLYPH_H) / 2, st->style.accent_hot);
}

static void
rg_draw_dialog(rg_state_t *st, const rg_layout_t *lay)
{
	libg_rect_t	rect, shade;
	const char	*label;
	uint32_t	color;
	int		i, count, hot;

	shade.x = 0;
	shade.y = 0;
	shade.width = (int32_t)st->width;
	shade.height = (int32_t)st->height;
	libgFillRect(st->ui, shade, RG_COL_SHADE);

	libgFillRect(st->ui, lay->dlg_frame, st->style.panel);
	libgStrokeRect(st->ui, lay->dlg_frame, st->style.accent);

	rect = lay->dlg_frame;
	rect.x += RG_PAD;
	rect.y += RG_PAD;
	rect.width -= 2 * RG_PAD;
	rect.height = RG_GLYPH_H;
	rg_text_at(st->ui, rect, st->dialog.title, st->style.text,
	    rect.width / RG_GLYPH_W);

	if (st->dialog.kind == RG_DLG_TEXT) {
		rg_draw_field(st, lay);
	} else if (st->dialog.kind == RG_DLG_CHOICE) {
		libgFillRect(st->ui, lay->dlg_field, st->style.field);
		libgStrokeRect(st->ui, lay->dlg_field,
		    st->style.panel_border);
		count = rg_choice_count(st->dialog.choice_kind);
		for (i = 0; i < count; i++) {
			rect = lay->dlg_field;
			rect.y = lay->dlg_field.y + i * RG_ROW_H;
			rect.height = RG_ROW_H;
			if (rect.y + rect.height >
			    lay->dlg_field.y + lay->dlg_field.height) {
				break;
			}
			if (i == st->dialog.choice) {
				libgFillRect(st->ui, rect, RG_COL_SEL);
			} else if (rg_rect_hit(rect, st->mouse_x,
			    st->mouse_y)) {
				libgFillRect(st->ui, rect, RG_COL_HOVER);
			}
			rect.x += RG_GUTTER;
			rect.width -= 2 * RG_GUTTER;
			rg_text_at(st->ui, rect,
			    rg_choice_item(st->dialog.choice_kind, i),
			    st->style.text, rect.width / RG_GLYPH_W);
		}
	}

	rect = lay->dlg_frame;
	rect.x += RG_PAD;
	rect.width -= 2 * RG_PAD;
	rect.height = RG_GLYPH_H;
	if (st->dialog.kind == RG_DLG_CONFIRM) {
		rect.y = lay->dlg_frame.y + RG_PAD + RG_GLYPH_H + RG_PAD;
	} else {
		rect.y = lay->dlg_field.y + lay->dlg_field.height + 4;
	}
	if (st->dialog.kind == RG_DLG_CHOICE) {
		rg_text_at(st->ui, rect,
		    rg_choice_hint(st->dialog.choice_kind,
		    st->dialog.choice), st->style.text_muted,
		    rect.width / RG_GLYPH_W);
	} else {
		rg_text_at(st->ui, rect, st->dialog.hint,
		    st->style.text_muted, rect.width / RG_GLYPH_W);
	}

	for (i = 0; i < 2; i++) {
		rect = i == 0 ? lay->dlg_ok : lay->dlg_cancel;
		label = i == 0 ?
		    (st->dialog.kind == RG_DLG_CONFIRM ? "Yes" : "OK") :
		    (st->dialog.kind == RG_DLG_CONFIRM ? "No" : "Cancel");
		hot = rg_rect_hit(rect, st->mouse_x, st->mouse_y);
		libgFillRect(st->ui, rect, hot ? st->style.control_hot :
		    st->style.control);
		color = st->style.text;
		if (i == 0) {
			libgStrokeRect(st->ui, rect,
			    st->dialog.kind == RG_DLG_CONFIRM ?
			    st->style.danger : st->style.accent);
			if (st->dialog.kind == RG_DLG_CONFIRM) {
				color = st->style.danger;
			}
		} else {
			libgStrokeRect(st->ui, rect, st->style.panel_border);
		}
		rect.x += (rect.width -
		    (int32_t)strlen(label) * RG_GLYPH_W) / 2;
		rg_text_at(st->ui, rect, label, color, (int)strlen(label));
	}
}

static void
rg_draw_help(rg_state_t *st)
{
	libg_rect_t	rect, frame;
	int32_t		w, h;
	int		i;

	w = 46 * RG_GLYPH_W + 2 * RG_PAD;
	h = RG_HELP_LINES * (RG_GLYPH_H + 4) + 2 * RG_PAD;
	if (w > (int32_t)st->width - 2 * RG_PAD) {
		w = (int32_t)st->width - 2 * RG_PAD;
	}
	if (h > (int32_t)st->height - 2 * RG_PAD) {
		h = (int32_t)st->height - 2 * RG_PAD;
	}
	frame.x = ((int32_t)st->width - w) / 2;
	frame.y = ((int32_t)st->height - h) / 2;
	frame.width = w;
	frame.height = h;
	if (frame.x < 0) {
		frame.x = 0;
	}
	if (frame.y < 0) {
		frame.y = 0;
	}

	rect.x = 0;
	rect.y = 0;
	rect.width = (int32_t)st->width;
	rect.height = (int32_t)st->height;
	libgFillRect(st->ui, rect, RG_COL_SHADE);
	libgFillRect(st->ui, frame, st->style.panel);
	libgStrokeRect(st->ui, frame, st->style.accent);

	for (i = 0; i < RG_HELP_LINES; i++) {
		rect.x = frame.x + RG_PAD;
		rect.y = frame.y + RG_PAD + i * (RG_GLYPH_H + 4);
		rect.width = frame.width - 2 * RG_PAD;
		rect.height = RG_GLYPH_H;
		if (rect.y + rect.height > frame.y + frame.height) {
			break;
		}
		rg_text_at(st->ui, rect, rg_help_text[i],
		    i == 0 ? st->style.text : st->style.text_muted,
		    rect.width / RG_GLYPH_W);
	}
}

void
rg_draw(rg_state_t *st)
{
	rg_layout_t	lay;
	libg_rect_t	clear;

	if (!st || !st->ui) {
		return;
	}
	rg_layout_compute(st, &lay);
	rg_clamp_view(st, &lay);
	if (libgBeginOverlay(st->ui) != LIBG_OK) {
		return;
	}
	clear.x = 0;
	clear.y = 0;
	clear.width = (int32_t)st->width;
	clear.height = (int32_t)st->height;
	libgFillRect(st->ui, clear, st->style.background);

	rg_draw_header(st, &lay);
	rg_draw_crumbs(st, &lay);
	rg_draw_toolbar(st, &lay);
	rg_draw_columns(st, &lay);
	rg_draw_keys(st, &lay);
	rg_draw_values(st, &lay);
	rg_draw_status(st, &lay);
	if (st->dialog.kind != RG_DLG_NONE) {
		rg_draw_dialog(st, &lay);
	}
	if (st->help) {
		rg_draw_help(st);
	}
}
