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
$define %type uint64_t as 64 bit unsigned
$define %type int32_t as 32 bit signed
$define %type sprot_event as one compositor event
$define %type rg_layout as per frame pixel geometry of every hit region
$define %type rg_state as regedit gui global state

$define %func rg_key_char as function with args uint32_t, uint32_t
$define %func rg_move_pane as procedure with args rg_state *, pane, delta
$define %func rg_focus_pane as procedure with args rg_state *, int
$define %func rg_toolbar_action as procedure with args rg_state *, int
$define %func rg_hit_row as function with args libg_rect, int32_t, off, count
$define %func rg_hot_update as function with args rg_state *, rg_layout *
$define %func rg_click_row as procedure with args rg_state *, pane, row
$define %func rg_bar_jump as procedure with args rg_state *, pane, rect, y, rows
$define %func rg_click as procedure with args rg_state *, layout, x, y
$define %func rg_wheel as procedure with args rg_state *, layout, int32_t
$define %func rg_key as procedure with args rg_state *, layout, key, mods
$define %func rg_dispatch as procedure with args rg_state *, sprot_event *

*/

/* !SPACE!

$space %internal rg_move_pane, rg_focus_pane, rg_toolbar_action, rg_hit_row
$space %internal rg_click_row, rg_bar_jump, rg_click, rg_wheel, rg_key
$space %export rg_key_char, rg_dispatch

*/

#include <stdint.h>
#include <string.h>
#include "gui.h"
#include "keys.h"

#define RG_WHEEL_ROWS	3


uint32_t
rg_key_char(uint32_t key, uint32_t mods)
{
	uint32_t	shift, caps;

	shift = (mods & RG_MOD_SHIFT) != 0;
	caps = (mods & RG_MOD_CAPS) != 0;

	if (key >= RG_KEY_A && key <= RG_KEY_Z) {
		if (shift ^ caps) {
			return ('A' + (key - RG_KEY_A));
		}
		return ('a' + (key - RG_KEY_A));
	}

	switch (key) {
	case RG_KEY_1: return (shift ? '!' : '1');
	case RG_KEY_2: return (shift ? '@' : '2');
	case RG_KEY_3: return (shift ? '#' : '3');
	case RG_KEY_4: return (shift ? '$' : '4');
	case RG_KEY_5: return (shift ? '%' : '5');
	case RG_KEY_6: return (shift ? '^' : '6');
	case RG_KEY_7: return (shift ? '&' : '7');
	case RG_KEY_8: return (shift ? '*' : '8');
	case RG_KEY_9: return (shift ? '(' : '9');
	case RG_KEY_0: return (shift ? ')' : '0');
	case RG_KEY_SPACE: return (' ');
	case RG_KEY_MINUS: return (shift ? '_' : '-');
	case RG_KEY_EQUAL: return (shift ? '+' : '=');
	case RG_KEY_LEFTBRACE: return (shift ? '{' : '[');
	case RG_KEY_RIGHTBRACE: return (shift ? '}' : ']');
	case RG_KEY_BACKSLASH: return (shift ? '|' : '\\');
	case RG_KEY_SEMICOLON: return (shift ? ':' : ';');
	case RG_KEY_APOSTROPHE: return (shift ? '"' : '\'');
	case RG_KEY_GRAVE: return (shift ? '~' : '`');
	case RG_KEY_COMMA: return (shift ? '<' : ',');
	case RG_KEY_DOT: return (shift ? '>' : '.');
	case RG_KEY_SLASH: return (shift ? '?' : '/');
	case RG_KEY_KP_0: return ('0');
	case RG_KEY_KP_1: return ('1');
	case RG_KEY_KP_2: return ('2');
	case RG_KEY_KP_3: return ('3');
	case RG_KEY_KP_4: return ('4');
	case RG_KEY_KP_5: return ('5');
	case RG_KEY_KP_6: return ('6');
	case RG_KEY_KP_7: return ('7');
	case RG_KEY_KP_8: return ('8');
	case RG_KEY_KP_9: return ('9');
	case RG_KEY_KP_DOT: return ('.');
	case RG_KEY_KP_SLASH: return ('/');
	case RG_KEY_KP_ASTERISK: return ('*');
	case RG_KEY_KP_MINUS: return ('-');
	case RG_KEY_KP_PLUS: return ('+');
	default:
		return (0);
	}
}

static void
rg_move_pane(rg_state_t *st, int pane, int delta)
{
	int	*sel, count;

	count = rg_row_count(st, pane);
	if (count == 0) {
		return;
	}
	sel = pane == RG_FOCUS_VALUES ? &st->value_sel : &st->key_sel;
	*sel += delta;
	if (*sel < 0) {
		*sel = 0;
	}
	if (*sel >= count) {
		*sel = count - 1;
	}
	st->dirty = 1;
}

static void
rg_focus_pane(rg_state_t *st, int pane)
{
	if (pane == RG_FOCUS_VALUES) {
		if (st->path.hive[0] == '\0') {
			rg_status(st, "No values at the hive list");
			return;
		}
		if (st->values.count == 0) {
			rg_status(st, "No values in this key");
			return;
		}
		st->focus = RG_FOCUS_VALUES;
	} else {
		st->focus = RG_FOCUS_KEYS;
	}
	st->dirty = 1;
}

static void
rg_toolbar_action(rg_state_t *st, int id)
{
	switch (id) {
	case RG_BTN_UP:
		rg_act_leave(st);
		break;
	case RG_BTN_NEW_KEY:
		rg_dialog_open(st, RG_ACT_NEW_KEY);
		break;
	case RG_BTN_NEW_VALUE:
		rg_dialog_open(st, RG_ACT_NEW_VALUE);
		break;
	case RG_BTN_DELETE:
		rg_act_delete(st);
		break;
	case RG_BTN_GOTO:
		rg_dialog_open(st, RG_ACT_GOTO);
		break;
	case RG_BTN_NOTIFY:
		rg_dialog_open(st, RG_ACT_NOTIFY);
		break;
	case RG_BTN_RELOAD:
		rg_reload(st);
		rg_status(st, "Reloaded");
		break;
	case RG_BTN_HELP:
		st->help = !st->help;
		st->dirty = 1;
		break;
	default:
		break;
	}
}

static int
rg_hit_row(libg_rect_t rect, int32_t y, int off, int count)
{
	int	index;

	index = off + (int)((y - rect.y) / RG_ROW_H);
	if (index < 0 || index >= count) {
		return (-1);
	}
	return (index);
}

static void
rg_click_row(rg_state_t *st, int pane, int row)
{
	uint64_t	now;
	int		same;

	now = rg_now_ms();
	same = (st->click_pane == pane && st->click_row == row);
	if (pane == RG_FOCUS_VALUES) {
		st->value_sel = row;
		st->focus = RG_FOCUS_VALUES;
	} else {
		st->key_sel = row;
		st->focus = RG_FOCUS_KEYS;
	}
	st->dirty = 1;
	if (same && now != 0 && now - st->click_ms <= RG_DOUBLE_MS) {
		st->click_ms = 0;
		st->click_pane = -1;
		st->click_row = -1;
		rg_act_activate(st);
		return;
	}
	st->click_ms = now;
	st->click_pane = pane;
	st->click_row = row;
}

static void
rg_bar_jump(rg_state_t *st, int pane, libg_rect_t bar, int32_t y, int rows)
{
	int	count, target;

	count = rg_row_count(st, pane);
	if (count == 0 || bar.height <= 0) {
		return;
	}
	target = (int)(((int64_t)(y - bar.y) * count) / bar.height);
	target += rows / 2;
	if (target >= count) {
		target = count - 1;
	}
	if (target < 0) {
		target = 0;
	}
	if (pane == RG_FOCUS_VALUES) {
		st->value_sel = target;
	} else {
		st->key_sel = target;
	}
	st->dirty = 1;
}

static void
rg_click(rg_state_t *st, const rg_layout_t *lay, int32_t x, int32_t y)
{
	int	i, row;

	if (st->help) {
		st->help = 0;
		st->dirty = 1;
		return;
	}
	if (st->dialog.kind != RG_DLG_NONE) {
		rg_dialog_click(st, lay, x, y);
		return;
	}

	for (i = 0; i < RG_BTN_COUNT; i++) {
		if (rg_rect_hit(lay->buttons[i], x, y)) {
			rg_toolbar_action(st, i);
			return;
		}
	}
	for (i = 0; i < st->crumb_count; i++) {
		if (rg_rect_hit(st->crumbs[i].rect, x, y)) {
			rg_act_goto_path(st, st->crumbs[i].target);
			return;
		}
	}
	if (rg_rect_hit(lay->keys, x, y)) {
		row = rg_hit_row(lay->keys, y, st->key_off,
		    rg_row_count(st, RG_FOCUS_KEYS));
		if (row >= 0) {
			rg_click_row(st, RG_FOCUS_KEYS, row);
		} else {
			rg_focus_pane(st, RG_FOCUS_KEYS);
		}
		return;
	}
	if (rg_rect_hit(lay->values, x, y)) {
		row = rg_hit_row(lay->values, y, st->value_off,
		    rg_row_count(st, RG_FOCUS_VALUES));
		if (row >= 0) {
			rg_click_row(st, RG_FOCUS_VALUES, row);
		}
		return;
	}
	if (rg_rect_hit(lay->keys_bar, x, y)) {
		rg_bar_jump(st, RG_FOCUS_KEYS, lay->keys_bar, y, lay->rows);
		return;
	}
	if (rg_rect_hit(lay->values_bar, x, y)) {
		rg_bar_jump(st, RG_FOCUS_VALUES, lay->values_bar, y,
		    lay->rows);
	}
}

static void
rg_wheel(rg_state_t *st, const rg_layout_t *lay, int32_t dy)
{
	int	pane, step;

	if (dy == 0 || st->dialog.kind != RG_DLG_NONE || st->help) {
		return;
	}
	if (rg_rect_hit(lay->values, st->mouse_x, st->mouse_y) ||
	    rg_rect_hit(lay->values_bar, st->mouse_x, st->mouse_y)) {
		pane = RG_FOCUS_VALUES;
	} else if (rg_rect_hit(lay->keys, st->mouse_x, st->mouse_y) ||
	    rg_rect_hit(lay->keys_bar, st->mouse_x, st->mouse_y)) {
		pane = RG_FOCUS_KEYS;
	} else {
		pane = st->focus;
	}
	step = dy > 0 ? -RG_WHEEL_ROWS : RG_WHEEL_ROWS;
	rg_move_pane(st, pane, step);
}

static void
rg_key(rg_state_t *st, const rg_layout_t *lay, uint32_t key, uint32_t mods)
{
	if (st->help) {
		st->help = 0;
		st->dirty = 1;
		return;
	}
	if (st->dialog.kind != RG_DLG_NONE) {
		rg_dialog_key(st, key, mods);
		return;
	}
	st->dirty = 1;

	if ((mods & RG_MOD_CTRL) != 0) {
		switch (key) {
		case RG_KEY_Q:
			st->running = 0;
			break;
		case RG_KEY_R:
			rg_reload(st);
			rg_status(st, "Reloaded");
			break;
		case RG_KEY_G:
			rg_dialog_open(st, RG_ACT_GOTO);
			break;
		case RG_KEY_S:
			rg_dialog_open(st, RG_ACT_NOTIFY);
			break;
		case RG_KEY_N:
			rg_dialog_open(st, RG_ACT_NEW_VALUE);
			break;
		case RG_KEY_A:
			rg_dialog_open(st, RG_ACT_NEW_KEY);
			break;
		default:
			break;
		}
		return;
	}

	switch (key) {
	case RG_KEY_UP:
		rg_move_pane(st, st->focus, -1);
		break;
	case RG_KEY_DOWN:
		rg_move_pane(st, st->focus, 1);
		break;
	case RG_KEY_PAGEUP:
		rg_move_pane(st, st->focus, -lay->rows);
		break;
	case RG_KEY_PAGEDOWN:
		rg_move_pane(st, st->focus, lay->rows);
		break;
	case RG_KEY_HOME:
		rg_move_pane(st, st->focus, -rg_row_count(st, st->focus));
		break;
	case RG_KEY_END:
		rg_move_pane(st, st->focus, rg_row_count(st, st->focus));
		break;
	case RG_KEY_TAB:
		rg_focus_pane(st, st->focus == RG_FOCUS_KEYS ?
		    RG_FOCUS_VALUES : RG_FOCUS_KEYS);
		break;
	case RG_KEY_RIGHT:
		if (st->focus == RG_FOCUS_KEYS) {
			rg_focus_pane(st, RG_FOCUS_VALUES);
		}
		break;
	case RG_KEY_LEFT:
		if (st->focus == RG_FOCUS_VALUES) {
			rg_focus_pane(st, RG_FOCUS_KEYS);
		} else {
			rg_act_leave(st);
		}
		break;
	case RG_KEY_ENTER:
	case RG_KEY_KP_ENTER:
		rg_act_activate(st);
		break;
	case RG_KEY_BACKSPACE:
		rg_act_leave(st);
		break;
	case RG_KEY_DELETE:
		rg_act_delete(st);
		break;
	case RG_KEY_F1:
		st->help = 1;
		break;
	case RG_KEY_F2:
		rg_dialog_open(st, RG_ACT_NEW_VALUE);
		break;
	case RG_KEY_F3:
		rg_dialog_open(st, RG_ACT_NEW_KEY);
		break;
	case RG_KEY_F4:
		rg_dialog_open(st, RG_ACT_GOTO);
		break;
	case RG_KEY_F5:
		rg_reload(st);
		rg_status(st, "Reloaded");
		break;
	case RG_KEY_F11:
		(void)sprot_set_fullscreen(st->surface, !st->fullscreen);
		break;
	case RG_KEY_ESC:
		rg_status(st, "");
		break;
	default:
		break;
	}
}

void
rg_dispatch(rg_state_t *st, const sprot_event_t *ev)
{
	rg_layout_t	lay;

	if (!st || !ev) {
		return;
	}
	rg_layout_compute(st, &lay);
	rg_clamp_view(st, &lay);

	switch (ev->kind) {
	case SPROT_EVENT_SURFACE_CLOSE:
	case SPROT_EVENT_DISCONNECT:
		st->running = 0;
		break;
	case SPROT_EVENT_SURFACE_CONFIGURE:
		st->focused = (ev->u.configure.state &
		    SPROT_SURFACE_STATE_FOCUSED) != 0;
		st->fullscreen = (ev->u.configure.state &
		    SPROT_SURFACE_STATE_FULLSCREEN) != 0;
		if (ev->u.configure.width != 0 &&
		    ev->u.configure.height != 0) {
			(void)rg_window_resize(st, ev->u.configure.width,
			    ev->u.configure.height);
		}
		st->dirty = 1;
		break;
	case SPROT_EVENT_POINTER_ENTER:
	case SPROT_EVENT_POINTER_MOTION:
		st->mouse_x = ev->u.pointer_motion.x;
		st->mouse_y = ev->u.pointer_motion.y;
		if (rg_hot_update(st, &lay) && !st->dirty) {
			st->hot_only = 1;
		}
		break;
	case SPROT_EVENT_POINTER_LEAVE:
		st->mouse_x = -1;
		st->mouse_y = -1;
		st->buttons = 0;
		if (rg_hot_update(st, &lay) && !st->dirty) {
			st->hot_only = 1;
		}
		break;
	case SPROT_EVENT_POINTER_BUTTON:
		if (ev->u.pointer_button.state == SPROT_BUTTON_STATE_PRESSED) {
			st->buttons |= ev->u.pointer_button.button;
			if ((ev->u.pointer_button.button &
			    SRAPI_MOUSE_LEFT) != 0) {
				rg_click(st, &lay, st->mouse_x, st->mouse_y);
			}
		} else {
			st->buttons &= ~ev->u.pointer_button.button;
		}
		if (!st->dirty) {
			st->hot_only = 1;
		}
		break;
	case SPROT_EVENT_POINTER_AXIS:
		rg_wheel(st, &lay, ev->u.pointer_axis.dy);
		break;
	case SPROT_EVENT_KEY:
		if (ev->u.key.state == SPROT_KEY_STATE_PRESSED) {
			rg_key(st, &lay, ev->u.key.scancode,
			    ev->u.key.modifiers);
		}
		break;
	default:
		break;
	}
}
