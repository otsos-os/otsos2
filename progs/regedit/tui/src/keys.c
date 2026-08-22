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

$define %type uint16_t as 16 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type api_key_event as native keyboard event
$define %type re_path as hive name plus dot separated key path
$define %type rt_state as regedit tui global state

$define %func rt_sel as function with args rt_state *
$define %func rt_sel_count as function with args rt_state *
$define %func rt_reset_view as procedure with args rt_state *
$define %func rt_move as procedure with args rt_state *, int
$define %func rt_focus_next as procedure with args rt_state *
$define %func rt_enter_key as procedure with args rt_state *
$define %func rt_edit_value as procedure with args rt_state *
$define %func rt_activate as procedure with args rt_state *
$define %func rt_leave as procedure with args rt_state *
$define %func rt_new_key as procedure with args rt_state *
$define %func rt_new_value as procedure with args rt_state *
$define %func rt_delete as procedure with args rt_state *
$define %func rt_goto as procedure with args rt_state *
$define %func rt_notify as procedure with args rt_state *
$define %func rt_ctrl as procedure with args rt_state *, uint16_t
$define %func rt_dispatch as function with args rt_state *, api_key_event *

*/

/* !SPACE!

$space %internal rt_sel, rt_sel_count, rt_reset_view, rt_move
$space %internal rt_focus_next, rt_enter_key, rt_edit_value, rt_activate
$space %internal rt_leave, rt_new_key, rt_new_value, rt_delete
$space %internal rt_goto, rt_notify, rt_ctrl
$space %export rt_dispatch

*/

#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

#define RT_TYPE_LIST	"string bool i32 u32 u64 ipv4 bytes multi_string"
#define RT_CONSUMER_LIST	"net scheduler kusr console input"

static uint8_t	rt_data[RE_DATA_MAX];
static char	rt_text[RE_TEXT_MAX];

static int *
rt_sel(rt_state_t *st)
{
	if (st->focus == RT_FOCUS_VALUES) {
		return (&st->value_sel);
	}
	return (&st->key_sel);
}

static int
rt_sel_count(const rt_state_t *st)
{
	if (st->focus == RT_FOCUS_VALUES) {
		return ((int)st->values.count);
	}
	return (rt_key_count(st));
}

static void
rt_reset_view(rt_state_t *st)
{
	st->key_sel = 0;
	st->key_off = 0;
	st->value_sel = 0;
	st->value_off = 0;
	st->focus = RT_FOCUS_KEYS;
}

static void
rt_move(rt_state_t *st, int delta)
{
	int	*sel, count;

	sel = rt_sel(st);
	count = rt_sel_count(st);
	if (count == 0) {
		return;
	}
	*sel += delta;
	if (*sel < 0) {
		*sel = 0;
	}
	if (*sel >= count) {
		*sel = count - 1;
	}
}

static void
rt_focus_next(rt_state_t *st)
{
	if (st->path.hive[0] == '\0') {
		rt_status(st, "No values at hive level");
		return;
	}
	if (st->focus == RT_FOCUS_KEYS) {
		if (st->values.count == 0) {
			rt_status(st, "No values in this key");
			return;
		}
		st->focus = RT_FOCUS_VALUES;
		return;
	}
	st->focus = RT_FOCUS_KEYS;
}

static void
rt_enter_key(rt_state_t *st)
{
	re_path_t	saved;
	int		count;

	count = rt_key_count(st);
	if (count == 0) {
		rt_status(st, "Nothing to open");
		return;
	}
	memcpy(&saved, &st->path, sizeof(saved));
	if (st->path.hive[0] == '\0') {
		if (st->hives.items[st->key_sel].access == RE_ACCESS_NONE) {
			rt_status_fmt(st, "%s: %s",
			    st->hives.items[st->key_sel].name,
			    re_error(EACCES));
			return;
		}
		if (re_path_set_hive(&st->path,
		    st->hives.items[st->key_sel].name) != 0) {
			memcpy(&st->path, &saved, sizeof(saved));
			rt_status_error(st, "open hive", errno);
			return;
		}
	} else if (re_path_push(&st->path,
	    st->keys.items[st->key_sel].name) != 0) {
		memcpy(&st->path, &saved, sizeof(saved));
		rt_status_error(st, "open key", errno);
		return;
	}
	rt_reset_view(st);
	rt_status(st, "");
	rt_reload(st);
	if (!st->loaded) {
		memcpy(&st->path, &saved, sizeof(saved));
		rt_reload(st);
		rt_status_error(st, "open key", EACCES);
	}
}

static void
rt_edit_value(rt_state_t *st)
{
	const re_value_t	*item;
	char			label[RT_STATUS_MAX];
	uint32_t		type, bytes;

	if (st->values.count == 0) {
		rt_status(st, "No value selected");
		return;
	}
	item = &st->values.items[st->value_sel];
	memset(rt_data, 0, sizeof(rt_data));
	memset(rt_text, 0, sizeof(rt_text));
	errno = 0;
	if (re_value_read(&st->path, item->name, &type, rt_data,
	    sizeof(rt_data), &bytes) != 0) {
		rt_status_error(st, item->name, errno);
		return;
	}
	if (re_format(type, rt_data, bytes, rt_text,
	    sizeof(rt_text)) != 0) {
		rt_status_fmt(st, "%s: value too large to edit",
		    item->name);
		return;
	}
	snprintf(label, sizeof(label), "%s (%s) = ", item->name,
	    re_type_name(type));
	if (!rt_prompt(st, label, rt_text, sizeof(rt_text))) {
		return;
	}
	errno = 0;
	if (re_value_write(&st->path, item->name, type, rt_text) != 0) {
		rt_status_error(st, item->name, errno);
		return;
	}
	rt_reload(st);
	rt_status_fmt(st, "%s updated", item->name);
}

static void
rt_activate(rt_state_t *st)
{
	if (st->focus == RT_FOCUS_VALUES) {
		rt_edit_value(st);
		return;
	}
	rt_enter_key(st);
}

static void
rt_leave(rt_state_t *st)
{
	char	leaf[RE_NAME_MAX];
	int	ret;

	if (st->path.hive[0] == '\0') {
		rt_status(st, "Already at hive list");
		return;
	}
	if (st->path.key[0] != '\0') {
		snprintf(leaf, sizeof(leaf), "%s",
		    re_path_leaf(st->path.key));
	} else {
		snprintf(leaf, sizeof(leaf), "%s", st->path.hive);
	}
	ret = re_path_pop(&st->path);
	if (ret != 0) {
		rt_status(st, "Already at hive list");
		return;
	}
	rt_reset_view(st);
	rt_status(st, "");
	rt_reload(st);
	for (st->key_sel = 0; st->key_sel < rt_key_count(st);
	    st->key_sel++) {
		if (st->path.hive[0] == '\0') {
			if (strcmp(st->hives.items[st->key_sel].name,
			    leaf) == 0) {
				return;
			}
		} else if (strcmp(st->keys.items[st->key_sel].name,
		    leaf) == 0) {
			return;
		}
	}
	st->key_sel = 0;
}

static void
rt_new_key(rt_state_t *st)
{
	char	name[RE_NAME_MAX];

	if (st->path.hive[0] == '\0') {
		rt_status(st, "Select a hive first");
		return;
	}
	name[0] = '\0';
	if (!rt_prompt(st, "New key name: ", name, sizeof(name))) {
		return;
	}
	if (name[0] == '\0') {
		rt_status(st, "Empty key name");
		return;
	}
	errno = 0;
	if (re_key_create(&st->path, name) != 0) {
		rt_status_error(st, name, errno);
		return;
	}
	rt_reload(st);
	rt_status_fmt(st, "key %s created", name);
}

static void
rt_new_value(rt_state_t *st)
{
	char		name[RE_NAME_MAX];
	char		type_name[RE_NAME_MAX];
	char		label[RT_STATUS_MAX];
	uint32_t	type;

	if (st->path.hive[0] == '\0') {
		rt_status(st, "Select a hive first");
		return;
	}
	name[0] = '\0';
	if (!rt_prompt(st, "New value name: ", name, sizeof(name))) {
		return;
	}
	if (name[0] == '\0') {
		rt_status(st, "Empty value name");
		return;
	}
	snprintf(type_name, sizeof(type_name), "%s", "string");
	snprintf(label, sizeof(label), "Type (%s): ", RT_TYPE_LIST);
	if (!rt_prompt(st, label, type_name, sizeof(type_name))) {
		return;
	}
	type = re_type_id(type_name);
	if (type == 0) {
		rt_status_fmt(st, "unknown type %s", type_name);
		return;
	}
	memset(rt_text, 0, sizeof(rt_text));
	snprintf(label, sizeof(label), "%s (%s) = ", name,
	    re_type_hint(type));
	if (!rt_prompt(st, label, rt_text, sizeof(rt_text))) {
		return;
	}
	errno = 0;
	if (re_value_write(&st->path, name, type, rt_text) != 0) {
		rt_status_error(st, name, errno);
		return;
	}
	rt_reload(st);
	rt_status_fmt(st, "value %s created", name);
}

static void
rt_delete(rt_state_t *st)
{
	char	question[RT_STATUS_MAX];
	char	name[RE_NAME_MAX];

	if (st->path.hive[0] == '\0') {
		rt_status(st, "Cannot delete a hive");
		return;
	}
	if (st->focus == RT_FOCUS_VALUES) {
		if (st->values.count == 0) {
			rt_status(st, "No value selected");
			return;
		}
		snprintf(name, sizeof(name), "%s",
		    st->values.items[st->value_sel].name);
		snprintf(question, sizeof(question), "Delete value %s?",
		    name);
		if (!rt_confirm(st, question)) {
			return;
		}
		errno = 0;
		if (re_value_delete(&st->path, name) != 0) {
			rt_status_error(st, name, errno);
			return;
		}
		rt_reload(st);
		rt_status_fmt(st, "value %s deleted", name);
		return;
	}
	if (st->keys.count == 0) {
		rt_status(st, "No key selected");
		return;
	}
	snprintf(name, sizeof(name), "%s",
	    st->keys.items[st->key_sel].name);
	snprintf(question, sizeof(question), "Delete key %s?", name);
	if (!rt_confirm(st, question)) {
		return;
	}
	errno = 0;
	if (re_key_delete(&st->path, name) != 0) {
		rt_status_error(st, name, errno);
		return;
	}
	rt_reload(st);
	rt_status_fmt(st, "key %s deleted", name);
}

static void
rt_goto(rt_state_t *st)
{
	re_path_t	saved;
	char		target[RE_PATH_MAX];

	memcpy(&saved, &st->path, sizeof(saved));
	if (re_path_text(&st->path, target, sizeof(target)) != 0 ||
	    st->path.hive[0] == '\0') {
		target[0] = '\0';
	}
	if (!rt_prompt(st, "Go to: ", target, sizeof(target))) {
		return;
	}
	errno = 0;
	if (re_path_parse(&st->path, target) != 0) {
		memcpy(&st->path, &saved, sizeof(saved));
		rt_status_error(st, "path", errno);
		return;
	}
	rt_reset_view(st);
	rt_status(st, "");
	rt_reload(st);
	if (!st->loaded && st->path.hive[0] != '\0') {
		memcpy(&st->path, &saved, sizeof(saved));
		rt_reload(st);
		rt_status_fmt(st, "%s: %s", target, re_error(ENOENT));
	}
}

static void
rt_notify(rt_state_t *st)
{
	char		name[RE_NAME_MAX];
	char		label[RT_STATUS_MAX];
	uint32_t	consumer;

	name[0] = '\0';
	snprintf(label, sizeof(label), "Notify consumer (%s): ",
	    RT_CONSUMER_LIST);
	if (!rt_prompt(st, label, name, sizeof(name))) {
		return;
	}
	consumer = re_consumer_id(name);
	if (consumer == 0) {
		rt_status_fmt(st, "unknown consumer %s", name);
		return;
	}
	errno = 0;
	if (re_consumer_update(consumer) != 0) {
		rt_status_error(st, name, errno);
		return;
	}
	rt_status_fmt(st, "consumer %s notified", name);
}

static void
rt_ctrl(rt_state_t *st, uint16_t key)
{
	switch (key) {
	case RT_KEY_Q:
		st->quit = 1;
		break;
	case RT_KEY_R:
		rt_reload(st);
		rt_status(st, "Reloaded");
		break;
	case RT_KEY_G:
		rt_goto(st);
		break;
	case RT_KEY_S:
		rt_notify(st);
		break;
	case RT_KEY_N:
		rt_new_value(st);
		break;
	case RT_KEY_A:
		rt_new_key(st);
		break;
	default:
		break;
	}
}

int
rt_dispatch(rt_state_t *st, const struct api_key_event *ev)
{
	if ((ev->mods & RT_MOD_CTRL) != 0) {
		rt_ctrl(st, ev->key);
		return (st->quit ? 0 : 1);
	}
	switch (ev->key) {
	case RT_KEY_UP:
		rt_move(st, -1);
		break;
	case RT_KEY_DOWN:
		rt_move(st, 1);
		break;
	case RT_KEY_PAGEUP:
		rt_move(st, -rt_rows_visible(st));
		break;
	case RT_KEY_PAGEDOWN:
		rt_move(st, rt_rows_visible(st));
		break;
	case RT_KEY_HOME:
		rt_move(st, -rt_sel_count(st));
		break;
	case RT_KEY_END:
		rt_move(st, rt_sel_count(st));
		break;
	case RT_KEY_TAB:
		rt_focus_next(st);
		break;
	case RT_KEY_RIGHT:
		if (st->focus == RT_FOCUS_KEYS) {
			rt_focus_next(st);
		}
		break;
	case RT_KEY_LEFT:
		if (st->focus == RT_FOCUS_VALUES) {
			st->focus = RT_FOCUS_KEYS;
		} else {
			rt_leave(st);
		}
		break;
	case RT_KEY_ENTER:
	case RT_KEY_KP_ENTER:
		rt_activate(st);
		break;
	case RT_KEY_BACKSPACE:
		rt_leave(st);
		break;
	case RT_KEY_DELETE:
		rt_delete(st);
		break;
	case RT_KEY_F1:
		rt_help(st);
		break;
	case RT_KEY_F2:
		rt_new_value(st);
		break;
	case RT_KEY_F3:
		rt_new_key(st);
		break;
	case RT_KEY_F5:
		rt_reload(st);
		rt_status(st, "Reloaded");
		break;
	case RT_KEY_ESC:
		rt_status(st, "");
		break;
	default:
		break;
	}
	return (st->quit ? 0 : 1);
}
