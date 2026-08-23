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

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %type rg_dialog as modal dialog state with edit buffer
$define %type rg_layout as per frame pixel geometry of every hit region
$define %type rg_state as regedit gui global state

$define %func rg_text_len as function with args rg_dialog *
$define %func rg_text_insert as procedure with args rg_dialog *, char
$define %func rg_text_erase as procedure with args rg_dialog *, int
$define %func rg_dialog_reset as procedure with args rg_dialog *
$define %func rg_dialog_text as procedure with args rg_state *, title, hint, limit
$define %func rg_dialog_edit_value as procedure with args rg_state *
$define %func rg_dialog_stage_type as procedure with args rg_state *
$define %func rg_dialog_stage_data as procedure with args rg_state *
$define %func rg_dialog_open as procedure with args rg_state *, action
$define %func rg_dialog_cancel as procedure with args rg_state *
$define %func rg_dialog_accept as procedure with args rg_state *
$define %func rg_dialog_move_choice as procedure with args rg_state *, int
$define %func rg_dialog_key as procedure with args rg_state *, key, mods
$define %func rg_dialog_click as procedure with args rg_state *, layout, x, y

*/

/* !SPACE!

$space %internal rg_text_len, rg_text_insert, rg_text_erase, rg_dialog_reset
$space %internal rg_dialog_text, rg_dialog_edit_value, rg_dialog_stage_type
$space %internal rg_dialog_stage_data, rg_dialog_move_choice
$space %export rg_dialog_open, rg_dialog_cancel, rg_dialog_accept
$space %export rg_dialog_key, rg_dialog_click

*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "keys.h"

static uint8_t	rg_data[RE_DATA_MAX];

static int
rg_text_len(const rg_dialog_t *dlg)
{
	return ((int)strnlen(dlg->text, sizeof(dlg->text) - 1));
}

static void
rg_text_insert(rg_dialog_t *dlg, char ch)
{
	int	len, limit;

	len = rg_text_len(dlg);
	limit = dlg->limit;
	if (limit <= 0 || limit > (int)sizeof(dlg->text) - 1) {
		limit = (int)sizeof(dlg->text) - 1;
	}
	if (len >= limit) {
		return;
	}
	if (dlg->caret < 0) {
		dlg->caret = 0;
	}
	if (dlg->caret > len) {
		dlg->caret = len;
	}
	memmove(dlg->text + dlg->caret + 1, dlg->text + dlg->caret,
	    (size_t)(len - dlg->caret) + 1);
	dlg->text[dlg->caret] = ch;
	dlg->caret++;
}

static void
rg_text_erase(rg_dialog_t *dlg, int offset)
{
	int	len, at;

	len = rg_text_len(dlg);
	at = dlg->caret + offset;
	if (at < 0 || at >= len) {
		return;
	}
	memmove(dlg->text + at, dlg->text + at + 1,
	    (size_t)(len - at - 1) + 1);
	if (offset < 0) {
		dlg->caret = at;
	}
}

static void
rg_dialog_reset(rg_dialog_t *dlg)
{
	memset(dlg, 0, sizeof(*dlg));
	dlg->kind = RG_DLG_NONE;
	dlg->action = RG_ACT_NONE;
}

static void
rg_dialog_text(rg_state_t *st, const char *title, const char *hint, int limit)
{
	st->dialog.kind = RG_DLG_TEXT;
	st->dialog.limit = limit;
	st->dialog.view = 0;
	st->dialog.caret = rg_text_len(&st->dialog);
	snprintf(st->dialog.title, sizeof(st->dialog.title), "%s", title);
	snprintf(st->dialog.hint, sizeof(st->dialog.hint), "%s", hint);
}

static void
rg_dialog_edit_value(rg_state_t *st)
{
	const re_value_t	*item;
	char			hint[RG_LABEL_MAX];
	char			title[RG_LABEL_MAX];
	uint32_t		type, bytes;

	if (st->values.count == 0 || st->value_sel < 0 ||
	    st->value_sel >= (int)st->values.count) {
		rg_status(st, "No value selected");
		return;
	}
	item = &st->values.items[st->value_sel];
	memset(rg_data, 0, sizeof(rg_data));
	errno = 0;
	if (re_value_read(&st->path, item->name, &type, rg_data,
	    sizeof(rg_data), &bytes) != 0) {
		rg_status_error(st, item->name, errno);
		return;
	}
	rg_dialog_reset(&st->dialog);
	if (re_format(type, rg_data, bytes, st->dialog.text,
	    sizeof(st->dialog.text)) != 0) {
		rg_dialog_reset(&st->dialog);
		rg_status_fmt(st, "%s: value too large to edit", item->name);
		return;
	}
	st->dialog.action = RG_ACT_EDIT_VALUE;
	st->dialog.type = type;
	snprintf(st->dialog.name, sizeof(st->dialog.name), "%s", item->name);
	snprintf(title, sizeof(title), "Edit %s", item->name);
	snprintf(hint, sizeof(hint), "%s: %s", re_type_name(type),
	    re_type_hint(type));
	rg_dialog_text(st, title, hint, (int)sizeof(st->dialog.text) - 1);
}

static void
rg_dialog_stage_type(rg_state_t *st)
{
	st->dialog.stage = 1;
	st->dialog.kind = RG_DLG_CHOICE;
	st->dialog.choice_kind = RG_CHOICE_TYPE;
	st->dialog.choice = 0;
	snprintf(st->dialog.title, sizeof(st->dialog.title),
	    "Type for %s", st->dialog.name);
	snprintf(st->dialog.hint, sizeof(st->dialog.hint),
	    "Up/Down to pick, Enter to continue");
}

static void
rg_dialog_stage_data(rg_state_t *st)
{
	char	hint[RG_LABEL_MAX];
	char	title[RG_LABEL_MAX];

	st->dialog.stage = 2;
	st->dialog.text[0] = '\0';
	snprintf(title, sizeof(title), "Value of %s", st->dialog.name);
	snprintf(hint, sizeof(hint), "%s: %s",
	    re_type_name(st->dialog.type), re_type_hint(st->dialog.type));
	rg_dialog_text(st, title, hint, (int)sizeof(st->dialog.text) - 1);
}

void
rg_dialog_open(rg_state_t *st, int action)
{
	char	path[RE_PATH_MAX];

	if (!st) {
		return;
	}
	st->dirty = 1;

	if (action == RG_ACT_EDIT_VALUE) {
		rg_dialog_edit_value(st);
		return;
	}

	rg_dialog_reset(&st->dialog);
	st->dialog.action = action;

	switch (action) {
	case RG_ACT_NEW_VALUE:
		if (st->path.hive[0] == '\0') {
			rg_dialog_reset(&st->dialog);
			rg_status(st, "Select a hive first");
			return;
		}
		st->dialog.stage = 0;
		rg_dialog_text(st, "New value", "name of the new value",
		    RE_NAME_MAX - 1);
		break;
	case RG_ACT_NEW_KEY:
		if (st->path.hive[0] == '\0') {
			rg_dialog_reset(&st->dialog);
			rg_status(st, "Select a hive first");
			return;
		}
		rg_dialog_text(st, "New key", "name of the new subkey",
		    RE_NAME_MAX - 1);
		break;
	case RG_ACT_GOTO:
		if (re_path_text(&st->path, path, sizeof(path)) != 0 ||
		    st->path.hive[0] == '\0') {
			path[0] = '\0';
		}
		snprintf(st->dialog.text, sizeof(st->dialog.text), "%s", path);
		rg_dialog_text(st, "Go to path", "HIVE.Key.Subkey",
		    RE_PATH_MAX - 1);
		break;
	case RG_ACT_NOTIFY:
		st->dialog.kind = RG_DLG_CHOICE;
		st->dialog.choice_kind = RG_CHOICE_CONSUMER;
		st->dialog.choice = 0;
		snprintf(st->dialog.title, sizeof(st->dialog.title),
		    "Notify consumer");
		snprintf(st->dialog.hint, sizeof(st->dialog.hint),
		    "Up/Down to pick, Enter to notify");
		break;
	case RG_ACT_DELETE_KEY:
		st->dialog.kind = RG_DLG_CONFIRM;
		snprintf(st->dialog.title, sizeof(st->dialog.title),
		    "Delete key");
		snprintf(st->dialog.hint, sizeof(st->dialog.hint),
		    "Delete key %s and everything under it?",
		    st->keys.count != 0 ?
		    st->keys.items[st->key_sel].name : "");
		break;
	case RG_ACT_DELETE_VALUE:
		st->dialog.kind = RG_DLG_CONFIRM;
		snprintf(st->dialog.title, sizeof(st->dialog.title),
		    "Delete value");
		snprintf(st->dialog.hint, sizeof(st->dialog.hint),
		    "Delete value %s?", st->values.count != 0 ?
		    st->values.items[st->value_sel].name : "");
		break;
	default:
		rg_dialog_reset(&st->dialog);
		break;
	}
}

void
rg_dialog_cancel(rg_state_t *st)
{
	if (st->dialog.kind == RG_DLG_NONE) {
		return;
	}
	rg_dialog_reset(&st->dialog);
	rg_status(st, "Cancelled");
}

void
rg_dialog_accept(rg_state_t *st)
{
	uint32_t	type;

	if (st->dialog.kind == RG_DLG_NONE) {
		return;
	}
	st->dirty = 1;

	switch (st->dialog.action) {
	case RG_ACT_EDIT_VALUE:
		if (rg_act_write(st, st->dialog.name, st->dialog.type,
		    st->dialog.text) != 0) {
			return;
		}
		rg_dialog_reset(&st->dialog);
		break;
	case RG_ACT_NEW_VALUE:
		if (st->dialog.stage == 0) {
			if (rg_text_len(&st->dialog) == 0) {
				rg_status(st, "Empty value name");
				return;
			}
			snprintf(st->dialog.name, sizeof(st->dialog.name),
			    "%s", st->dialog.text);
			rg_dialog_stage_type(st);
			return;
		}
		if (st->dialog.stage == 1) {
			type = re_type_id(rg_choice_item(RG_CHOICE_TYPE,
			    st->dialog.choice));
			if (type == 0) {
				rg_status(st, "Pick a value type");
				return;
			}
			st->dialog.type = type;
			rg_dialog_stage_data(st);
			return;
		}
		if (rg_act_write(st, st->dialog.name, st->dialog.type,
		    st->dialog.text) != 0) {
			return;
		}
		rg_dialog_reset(&st->dialog);
		break;
	case RG_ACT_NEW_KEY:
		if (rg_act_new_key(st, st->dialog.text) != 0) {
			return;
		}
		rg_dialog_reset(&st->dialog);
		break;
	case RG_ACT_GOTO:
		rg_act_goto_path(st, st->dialog.text);
		rg_dialog_reset(&st->dialog);
		break;
	case RG_ACT_NOTIFY:
		(void)rg_act_notify(st, rg_choice_item(RG_CHOICE_CONSUMER,
		    st->dialog.choice));
		rg_dialog_reset(&st->dialog);
		break;
	case RG_ACT_DELETE_KEY:
	case RG_ACT_DELETE_VALUE:
		rg_act_commit_delete(st);
		rg_dialog_reset(&st->dialog);
		break;
	default:
		rg_dialog_reset(&st->dialog);
		break;
	}
}

static void
rg_dialog_move_choice(rg_state_t *st, int delta)
{
	int	count;

	count = rg_choice_count(st->dialog.choice_kind);
	if (count <= 0) {
		return;
	}
	st->dialog.choice += delta;
	if (st->dialog.choice < 0) {
		st->dialog.choice = 0;
	}
	if (st->dialog.choice >= count) {
		st->dialog.choice = count - 1;
	}
	st->dirty = 1;
}

void
rg_dialog_key(rg_state_t *st, uint32_t key, uint32_t mods)
{
	uint32_t	ch;
	int		len;

	if (st->dialog.kind == RG_DLG_NONE) {
		return;
	}
	st->dirty = 1;

	if (key == RG_KEY_ESC) {
		rg_dialog_cancel(st);
		return;
	}
	if (key == RG_KEY_ENTER || key == RG_KEY_KP_ENTER) {
		rg_dialog_accept(st);
		return;
	}

	if (st->dialog.kind == RG_DLG_CONFIRM) {
		ch = rg_key_char(key, mods);
		if (ch == 'y' || ch == 'Y') {
			rg_dialog_accept(st);
		} else if (ch == 'n' || ch == 'N') {
			rg_dialog_cancel(st);
		}
		return;
	}

	if (st->dialog.kind == RG_DLG_CHOICE) {
		switch (key) {
		case RG_KEY_UP:
			rg_dialog_move_choice(st, -1);
			break;
		case RG_KEY_DOWN:
			rg_dialog_move_choice(st, 1);
			break;
		case RG_KEY_HOME:
		case RG_KEY_PAGEUP:
			rg_dialog_move_choice(st,
			    -rg_choice_count(st->dialog.choice_kind));
			break;
		case RG_KEY_END:
		case RG_KEY_PAGEDOWN:
			rg_dialog_move_choice(st,
			    rg_choice_count(st->dialog.choice_kind));
			break;
		default:
			break;
		}
		return;
	}

	len = rg_text_len(&st->dialog);
	switch (key) {
	case RG_KEY_BACKSPACE:
		rg_text_erase(&st->dialog, -1);
		return;
	case RG_KEY_DELETE:
		rg_text_erase(&st->dialog, 0);
		return;
	case RG_KEY_LEFT:
		if (st->dialog.caret > 0) {
			st->dialog.caret--;
		}
		return;
	case RG_KEY_RIGHT:
		if (st->dialog.caret < len) {
			st->dialog.caret++;
		}
		return;
	case RG_KEY_HOME:
		st->dialog.caret = 0;
		return;
	case RG_KEY_END:
		st->dialog.caret = len;
		return;
	default:
		break;
	}

	if ((mods & RG_MOD_CTRL) != 0) {
		if (key == RG_KEY_U) {
			st->dialog.text[0] = '\0';
			st->dialog.caret = 0;
			st->dialog.view = 0;
		}
		return;
	}

	ch = rg_key_char(key, mods);
	if (ch >= 32 && ch < 127) {
		rg_text_insert(&st->dialog, (char)ch);
	}
}

void
rg_dialog_click(rg_state_t *st, const rg_layout_t *lay, int32_t x, int32_t y)
{
	uint64_t	now;
	int		index, count;

	if (st->dialog.kind == RG_DLG_NONE) {
		return;
	}
	if (rg_rect_hit(lay->dlg_ok, x, y)) {
		rg_dialog_accept(st);
		return;
	}
	if (rg_rect_hit(lay->dlg_cancel, x, y)) {
		rg_dialog_cancel(st);
		return;
	}
	if (st->dialog.kind == RG_DLG_CHOICE &&
	    rg_rect_hit(lay->dlg_field, x, y)) {
		count = rg_choice_count(st->dialog.choice_kind);
		index = (int)((y - lay->dlg_field.y) / RG_ROW_H);
		if (index < 0 || index >= count) {
			return;
		}
		now = rg_now_ms();
		if (index == st->dialog.choice && now != 0 &&
		    now - st->click_ms <= RG_DOUBLE_MS) {
			st->click_ms = 0;
			rg_dialog_accept(st);
			return;
		}
		st->dialog.choice = index;
		st->click_ms = now;
		st->dirty = 1;
	}
}
