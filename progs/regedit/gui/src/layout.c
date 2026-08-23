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

$define %type int32_t as 32 bit signed
$define %type libg_rect as integer rectangle
$define %type rg_layout as per frame pixel geometry of every hit region
$define %type rg_state as regedit gui global state

$define %func rg_button_label as function with args int
$define %func rg_rect_hit as function with args libg_rect, int32_t, int32_t
$define %func rg_row_count as function with args rg_state *, int
$define %func rg_clamp_view as procedure with args rg_state *, rg_layout *
$define %func rg_crumbs_build as procedure with args rg_state *, libg_rect
$define %func rg_dialog_body_rows as function with args rg_dialog *
$define %func rg_layout_dialog as procedure with args rg_state *, rg_layout *
$define %func rg_layout_compute as procedure with args rg_state *, rg_layout *

*/

/* !SPACE!

$space %internal rg_crumbs_build, rg_dialog_body_rows, rg_layout_dialog
$space %export rg_button_label, rg_rect_hit, rg_row_count, rg_clamp_view
$space %export rg_layout_compute

*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

#define RG_BTN_PAD		10
#define RG_BTN_GAP		6
#define RG_CRUMB_PAD		6
#define RG_CRUMB_SEP		2

static const char	*rg_button_labels[RG_BTN_COUNT] = {
	"Up", "New Key", "New Value", "Delete", "Goto", "Notify",
	"Reload", "Help"
};

const char *
rg_button_label(int id)
{
	if (id < 0 || id >= RG_BTN_COUNT) {
		return ("");
	}
	return (rg_button_labels[id]);
}

int
rg_rect_hit(libg_rect_t rect, int32_t x, int32_t y)
{
	if (rect.width <= 0 || rect.height <= 0) {
		return (0);
	}
	return (x >= rect.x && x < rect.x + rect.width &&
	    y >= rect.y && y < rect.y + rect.height);
}

int
rg_row_count(const rg_state_t *st, int pane)
{
	if (!st) {
		return (0);
	}
	if (pane == RG_FOCUS_VALUES) {
		return ((int)st->values.count);
	}
	if (st->path.hive[0] == '\0') {
		return ((int)st->hives.count);
	}
	return ((int)st->keys.count);
}

void
rg_clamp_view(rg_state_t *st, const rg_layout_t *lay)
{
	int	rows, keys, values;

	if (!st || !lay) {
		return;
	}
	rows = lay->rows > 0 ? lay->rows : 1;
	keys = rg_row_count(st, RG_FOCUS_KEYS);
	values = rg_row_count(st, RG_FOCUS_VALUES);

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
	if (values == 0 && st->focus == RG_FOCUS_VALUES) {
		st->focus = RG_FOCUS_KEYS;
	}
}

static void
rg_crumbs_build(rg_state_t *st, libg_rect_t bar)
{
	rg_crumb_t	*crumb;
	const char	*seg;
	size_t		len;
	int32_t		x, avail, used;
	int		i, first, count;

	st->crumb_count = 0;
	crumb = &st->crumbs[0];
	memset(crumb, 0, sizeof(*crumb));
	snprintf(crumb->label, sizeof(crumb->label), "Hives");
	crumb->target[0] = '\0';
	st->crumb_count = 1;

	if (st->path.hive[0] != '\0' && st->crumb_count < RG_CRUMB_MAX) {
		crumb = &st->crumbs[st->crumb_count];
		memset(crumb, 0, sizeof(*crumb));
		snprintf(crumb->label, sizeof(crumb->label), "%s",
		    st->path.hive);
		snprintf(crumb->target, sizeof(crumb->target), "%s",
		    st->path.hive);
		st->crumb_count++;

		seg = st->path.key;
		while (*seg != '\0' && st->crumb_count < RG_CRUMB_MAX) {
			len = 0;
			while (seg[len] != '\0' && seg[len] != '.') {
				len++;
			}
			if (len == 0) {
				break;
			}
			crumb = &st->crumbs[st->crumb_count];
			memset(crumb, 0, sizeof(*crumb));
			if (len >= sizeof(crumb->label)) {
				len = sizeof(crumb->label) - 1;
			}
			memcpy(crumb->label, seg, len);
			crumb->label[len] = '\0';
			snprintf(crumb->target, sizeof(crumb->target),
			    "%s.%.*s", st->path.hive,
			    (int)(seg - st->path.key) + (int)len, st->path.key);
			st->crumb_count++;
			seg += len;
			if (*seg == '.') {
				seg++;
			}
		}
	}
	avail = bar.width - 2 * RG_PAD;
	if (avail < 0) {
		avail = 0;
	}
	count = st->crumb_count;
	for (first = 0; first < count; first++) {
		used = 0;
		for (i = first; i < count; i++) {
			used += (int32_t)strlen(st->crumbs[i].label) *
			    RG_GLYPH_W + 2 * RG_CRUMB_PAD;
			if (i + 1 < count) {
				used += RG_GLYPH_W + 2 * RG_CRUMB_SEP;
			}
		}
		if (used <= avail || first + 1 >= count) {
			break;
		}
	}

	for (i = 0; i < first; i++) {
		memset(&st->crumbs[i].rect, 0, sizeof(st->crumbs[i].rect));
	}
	x = bar.x + RG_PAD;
	for (i = first; i < count; i++) {
		st->crumbs[i].rect.x = x;
		st->crumbs[i].rect.y = bar.y + 4;
		st->crumbs[i].rect.width =
		    (int32_t)strlen(st->crumbs[i].label) * RG_GLYPH_W +
		    2 * RG_CRUMB_PAD;
		st->crumbs[i].rect.height = bar.height - 8;
		x += st->crumbs[i].rect.width;
		if (i + 1 < count) {
			x += RG_GLYPH_W + 2 * RG_CRUMB_SEP;
		}
	}
}

static int
rg_dialog_body_rows(const rg_dialog_t *dlg)
{
	int	count;

	if (dlg->kind != RG_DLG_CHOICE) {
		return (0);
	}
	count = rg_choice_count(dlg->choice_kind);
	if (count < 1) {
		count = 1;
	}
	return (count);
}

static void
rg_layout_dialog(rg_state_t *st, rg_layout_t *out)
{
	int32_t	w, h, x, y, body, btn_w, btn_h;

	if (st->dialog.kind == RG_DLG_NONE) {
		return;
	}
	btn_h = RG_GLYPH_H + 12;
	btn_w = 9 * RG_GLYPH_W;

	switch (st->dialog.kind) {
	case RG_DLG_TEXT:
		body = (RG_GLYPH_H + 12) + RG_PAD + RG_GLYPH_H;
		break;
	case RG_DLG_CHOICE:
		body = rg_dialog_body_rows(&st->dialog) * RG_ROW_H + RG_PAD +
		    RG_GLYPH_H;
		break;
	default:
		body = RG_GLYPH_H;
		break;
	}

	w = (int32_t)st->width - 4 * RG_PAD;
	if (w > 660) {
		w = 660;
	}
	if (w < 300) {
		w = 300;
	}
	h = RG_PAD + RG_GLYPH_H + RG_PAD + body + RG_PAD + btn_h + RG_PAD;
	if (h > (int32_t)st->height - 2 * RG_PAD) {
		h = (int32_t)st->height - 2 * RG_PAD;
	}
	if (h < 2 * RG_PAD + btn_h) {
		h = 2 * RG_PAD + btn_h;
	}
	x = ((int32_t)st->width - w) / 2;
	y = ((int32_t)st->height - h) / 2;
	if (x < 0) {
		x = 0;
	}
	if (y < 0) {
		y = 0;
	}

	out->dlg_frame.x = x;
	out->dlg_frame.y = y;
	out->dlg_frame.width = w;
	out->dlg_frame.height = h;

	out->dlg_field.x = x + RG_PAD;
	out->dlg_field.y = y + RG_PAD + RG_GLYPH_H + RG_PAD;
	out->dlg_field.width = w - 2 * RG_PAD;
	if (st->dialog.kind == RG_DLG_CHOICE) {
		out->dlg_field.height =
		    rg_dialog_body_rows(&st->dialog) * RG_ROW_H;
	} else if (st->dialog.kind == RG_DLG_TEXT) {
		out->dlg_field.height = RG_GLYPH_H + 12;
	} else {
		out->dlg_field.height = 0;
	}

	out->dlg_cancel.x = x + w - RG_PAD - btn_w;
	out->dlg_cancel.y = y + h - RG_PAD - btn_h;
	out->dlg_cancel.width = btn_w;
	out->dlg_cancel.height = btn_h;

	out->dlg_ok = out->dlg_cancel;
	out->dlg_ok.x -= btn_w + RG_BTN_GAP;
}

void
rg_layout_compute(rg_state_t *st, rg_layout_t *out)
{
	int32_t	w, h, top, list_h, pane, x;
	int	i;

	if (!st || !out) {
		return;
	}
	memset(out, 0, sizeof(*out));
	w = (int32_t)st->width;
	h = (int32_t)st->height;

	out->header.x = 0;
	out->header.y = 0;
	out->header.width = w;
	out->header.height = RG_HEADER_H;

	out->crumbs.x = 0;
	out->crumbs.y = RG_HEADER_H;
	out->crumbs.width = w;
	out->crumbs.height = RG_CRUMB_H;

	out->toolbar.x = 0;
	out->toolbar.y = out->crumbs.y + RG_CRUMB_H;
	out->toolbar.width = w;
	out->toolbar.height = RG_TOOLBAR_H;

	out->columns.x = 0;
	out->columns.y = out->toolbar.y + RG_TOOLBAR_H;
	out->columns.width = w;
	out->columns.height = RG_COLUMN_H;

	out->status.x = 0;
	out->status.y = h - RG_STATUS_H;
	out->status.width = w;
	out->status.height = RG_STATUS_H;

	top = out->columns.y + RG_COLUMN_H;
	list_h = out->status.y - top;
	if (list_h < RG_ROW_H) {
		list_h = RG_ROW_H;
	}
	out->rows = list_h / RG_ROW_H;
	if (out->rows < 1) {
		out->rows = 1;
	}

	pane = w * RG_PANE_SHARE / 100;
	if (pane < RG_PANE_MIN) {
		pane = RG_PANE_MIN;
	}
	if (pane > w - RG_PANE_MIN) {
		pane = w - RG_PANE_MIN;
	}
	if (pane < RG_SCROLL_W + 1) {
		pane = RG_SCROLL_W + 1;
	}

	out->keys.x = 0;
	out->keys.y = top;
	out->keys.width = pane - RG_SCROLL_W;
	out->keys.height = list_h;
	out->keys_bar = out->keys;
	out->keys_bar.x = pane - RG_SCROLL_W;
	out->keys_bar.width = RG_SCROLL_W;

	out->values.x = pane;
	out->values.y = top;
	out->values.width = w - pane - RG_SCROLL_W;
	if (out->values.width < 0) {
		out->values.width = 0;
	}
	out->values.height = list_h;
	out->values_bar = out->values;
	out->values_bar.x = w - RG_SCROLL_W;
	out->values_bar.width = RG_SCROLL_W;
	out->name_w = out->values.width * 34 / 100;
	if (out->name_w < 8 * RG_GLYPH_W) {
		out->name_w = 8 * RG_GLYPH_W;
	}
	out->type_w = out->values.width * 24 / 100;
	if (out->type_w < 4 * RG_GLYPH_W) {
		out->type_w = 4 * RG_GLYPH_W;
	}
	if (out->name_w + out->type_w > out->values.width) {
		out->name_w = out->values.width / 2;
		out->type_w = out->values.width - out->name_w;
	}
	out->data_w = out->values.width - out->name_w - out->type_w;
	if (out->data_w < 0) {
		out->data_w = 0;
	}

	x = RG_PAD;
	for (i = 0; i < RG_BTN_COUNT; i++) {
		out->buttons[i].x = x;
		out->buttons[i].y = out->toolbar.y + 5;
		out->buttons[i].width =
		    (int32_t)strlen(rg_button_labels[i]) * RG_GLYPH_W +
		    2 * RG_BTN_PAD;
		out->buttons[i].height = out->toolbar.height - 10;
		x += out->buttons[i].width + RG_BTN_GAP;
		if (x > w - RG_PAD) {
			out->buttons[i].width = 0;
		}
	}

	rg_crumbs_build(st, out->crumbs);
	rg_layout_dialog(st, out);
}
