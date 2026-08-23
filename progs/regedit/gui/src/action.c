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
$define %type re_path as hive name plus dot separated key path
$define %type rg_state as regedit gui global state

$define %func rg_act_reset_view as procedure with args rg_state *
$define %func rg_act_select_name as procedure with args rg_state *, const char *
$define %func rg_act_open as procedure with args rg_state *
$define %func rg_act_leave as procedure with args rg_state *
$define %func rg_act_activate as procedure with args rg_state *
$define %func rg_act_goto_path as procedure with args rg_state *, const char *
$define %func rg_act_delete as procedure with args rg_state *
$define %func rg_act_commit_delete as procedure with args rg_state *
$define %func rg_act_write as function with args rg_state *, name, type, text
$define %func rg_act_new_key as function with args rg_state *, const char *
$define %func rg_act_notify as function with args rg_state *, const char *

*/

/* !SPACE!

$space %internal rg_act_reset_view, rg_act_select_name
$space %export rg_act_open, rg_act_leave, rg_act_activate, rg_act_goto_path
$space %export rg_act_delete, rg_act_commit_delete
$space %export rg_act_write, rg_act_new_key, rg_act_notify

*/

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"

static void
rg_act_reset_view(rg_state_t *st)
{
	st->key_sel = 0;
	st->key_off = 0;
	st->value_sel = 0;
	st->value_off = 0;
	st->focus = RG_FOCUS_KEYS;
	st->click_row = -1;
	st->click_pane = -1;
}

static void
rg_act_select_name(rg_state_t *st, const char *name)
{
	int	i, count;

	count = rg_row_count(st, RG_FOCUS_KEYS);
	for (i = 0; i < count; i++) {
		if (st->path.hive[0] == '\0') {
			if (strcmp(st->hives.items[i].name, name) == 0) {
				st->key_sel = i;
				return;
			}
		} else if (strcmp(st->keys.items[i].name, name) == 0) {
			st->key_sel = i;
			return;
		}
	}
	st->key_sel = 0;
}

void
rg_act_open(rg_state_t *st)
{
	re_path_t	saved;
	int		count;

	count = rg_row_count(st, RG_FOCUS_KEYS);
	if (count == 0) {
		rg_status(st, "Nothing to open");
		return;
	}
	memcpy(&saved, &st->path, sizeof(saved));
	if (st->path.hive[0] == '\0') {
		if (st->hives.items[st->key_sel].access == RE_ACCESS_NONE) {
			rg_status_fmt(st, "%s: %s",
			    st->hives.items[st->key_sel].name,
			    re_error(EACCES));
			return;
		}
		errno = 0;
		if (re_path_set_hive(&st->path,
		    st->hives.items[st->key_sel].name) != 0) {
			memcpy(&st->path, &saved, sizeof(saved));
			rg_status_error(st, "open hive", errno);
			return;
		}
	} else {
		errno = 0;
		if (re_path_push(&st->path,
		    st->keys.items[st->key_sel].name) != 0) {
			memcpy(&st->path, &saved, sizeof(saved));
			rg_status_error(st, "open key", errno);
			return;
		}
	}
	rg_act_reset_view(st);
	rg_status(st, "");
	rg_reload(st);
	if (!st->loaded) {
		memcpy(&st->path, &saved, sizeof(saved));
		rg_reload(st);
		rg_status_error(st, "open key", EACCES);
	}
}

void
rg_act_leave(rg_state_t *st)
{
	char	leaf[RE_NAME_MAX];

	if (st->path.hive[0] == '\0') {
		rg_status(st, "Already at the hive list");
		return;
	}
	if (st->path.key[0] != '\0') {
		snprintf(leaf, sizeof(leaf), "%s",
		    re_path_leaf(st->path.key));
	} else {
		snprintf(leaf, sizeof(leaf), "%s", st->path.hive);
	}
	if (re_path_pop(&st->path) != 0) {
		rg_status(st, "Already at the hive list");
		return;
	}
	rg_act_reset_view(st);
	rg_status(st, "");
	rg_reload(st);
	rg_act_select_name(st, leaf);
}

void
rg_act_activate(rg_state_t *st)
{
	if (st->focus == RG_FOCUS_VALUES) {
		rg_dialog_open(st, RG_ACT_EDIT_VALUE);
		return;
	}
	rg_act_open(st);
}

void
rg_act_goto_path(rg_state_t *st, const char *text)
{
	re_path_t	saved;

	memcpy(&saved, &st->path, sizeof(saved));
	errno = 0;
	if (re_path_parse(&st->path, text) != 0) {
		memcpy(&st->path, &saved, sizeof(saved));
		rg_status_error(st, "path", errno);
		return;
	}
	rg_act_reset_view(st);
	rg_status(st, "");
	rg_reload(st);
	if (!st->loaded && st->path.hive[0] != '\0') {
		memcpy(&st->path, &saved, sizeof(saved));
		rg_reload(st);
		rg_status_fmt(st, "%s: %s", text, re_error(ENOENT));
	}
}

void
rg_act_delete(rg_state_t *st)
{
	if (st->path.hive[0] == '\0') {
		rg_status(st, "A hive cannot be deleted");
		return;
	}
	if (st->focus == RG_FOCUS_VALUES) {
		if (st->values.count == 0) {
			rg_status(st, "No value selected");
			return;
		}
		rg_dialog_open(st, RG_ACT_DELETE_VALUE);
		return;
	}
	if (st->keys.count == 0) {
		rg_status(st, "No key selected");
		return;
	}
	rg_dialog_open(st, RG_ACT_DELETE_KEY);
}

void
rg_act_commit_delete(rg_state_t *st)
{
	char	name[RE_NAME_MAX];

	if (st->path.hive[0] == '\0') {
		rg_status(st, "A hive cannot be deleted");
		return;
	}
	if (st->dialog.action == RG_ACT_DELETE_VALUE) {
		if (st->values.count == 0 ||
		    st->value_sel >= (int)st->values.count) {
			rg_status(st, "No value selected");
			return;
		}
		snprintf(name, sizeof(name), "%s",
		    st->values.items[st->value_sel].name);
		errno = 0;
		if (re_value_delete(&st->path, name) != 0) {
			rg_status_error(st, name, errno);
			return;
		}
		rg_reload(st);
		rg_status_fmt(st, "value %s deleted", name);
		return;
	}
	if (st->keys.count == 0 || st->key_sel >= (int)st->keys.count) {
		rg_status(st, "No key selected");
		return;
	}
	snprintf(name, sizeof(name), "%s", st->keys.items[st->key_sel].name);
	errno = 0;
	if (re_key_delete(&st->path, name) != 0) {
		rg_status_error(st, name, errno);
		return;
	}
	rg_reload(st);
	rg_status_fmt(st, "key %s deleted", name);
}

int
rg_act_write(rg_state_t *st, const char *name, uint32_t type, const char *text)
{
	if (st->path.hive[0] == '\0') {
		rg_status(st, "Select a hive first");
		return (-1);
	}
	if (!name || name[0] == '\0') {
		rg_status(st, "Empty value name");
		return (-1);
	}
	errno = 0;
	if (re_value_write(&st->path, name, type, text) != 0) {
		rg_status_error(st, name, errno);
		return (-1);
	}
	rg_reload(st);
	rg_status_fmt(st, "%s written as %s", name, re_type_name(type));
	return (0);
}

int
rg_act_new_key(rg_state_t *st, const char *name)
{
	if (st->path.hive[0] == '\0') {
		rg_status(st, "Select a hive first");
		return (-1);
	}
	if (!name || name[0] == '\0') {
		rg_status(st, "Empty key name");
		return (-1);
	}
	errno = 0;
	if (re_key_create(&st->path, name) != 0) {
		rg_status_error(st, name, errno);
		return (-1);
	}
	rg_reload(st);
	rg_status_fmt(st, "key %s created", name);
	return (0);
}

int
rg_act_notify(rg_state_t *st, const char *name)
{
	uint32_t	consumer;

	consumer = re_consumer_id(name);
	if (consumer == 0) {
		rg_status_fmt(st, "unknown consumer %s",
		    name != NULL ? name : "");
		return (-1);
	}
	errno = 0;
	if (re_consumer_update(consumer) != 0) {
		rg_status_error(st, name, errno);
		return (-1);
	}
	rg_status_fmt(st, "consumer %s notified", name);
	return (0);
}
