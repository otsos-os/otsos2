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

$define %type va_list as variadic argument cursor
$define %type rg_state as regedit gui global state

$define %func rg_status as procedure with args rg_state *, const char *
$define %func rg_status_fmt as procedure with args rg_state *, fmt, args
$define %func rg_status_error as procedure with args rg_state *, what, code
$define %func rg_reload as procedure with args rg_state *
$define %func rg_choice_count as function with args int
$define %func rg_choice_item as function with args int, int
$define %func rg_choice_hint as function with args int, int

*/

/* !SPACE!

$space %export rg_status, rg_status_fmt, rg_status_error, rg_reload
$space %export rg_choice_count, rg_choice_item, rg_choice_hint

*/

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"


static const char	*rg_type_names[] = {
	"string", "bool", "i32", "u32", "u64", "ipv4", "bytes",
	"multi_string"
};

static const char	*rg_consumer_names[] = {
	"net", "scheduler", "kusr", "console", "input"
};

void
rg_status(rg_state_t *st, const char *text)
{
	snprintf(st->status, sizeof(st->status), "%s", text ? text : "");
	st->dirty = 1;
}

void
rg_status_fmt(rg_state_t *st, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(st->status, sizeof(st->status), fmt, ap);
	va_end(ap);
	st->dirty = 1;
}

void
rg_status_error(rg_state_t *st, const char *what, int code)
{
	rg_status_fmt(st, "%s: %s", what, re_error(code));
}

void
rg_reload(rg_state_t *st)
{
	int	keys_ok, values_ok;

	if (!st) {
		return;
	}
	st->dirty = 1;
	if (st->path.hive[0] == '\0') {
		memset(&st->keys, 0, sizeof(st->keys));
		memset(&st->values, 0, sizeof(st->values));
		errno = 0;
		if (re_hives_load(&st->hives) != 0) {
			memset(&st->hives, 0, sizeof(st->hives));
			st->loaded = 0;
			rg_status_error(st, "hives", errno);
			return;
		}
		st->loaded = 1;
		st->focus = RG_FOCUS_KEYS;
		return;
	}

	errno = 0;
	keys_ok = re_keys_load(&st->path, &st->keys) == 0;
	if (!keys_ok) {
		memset(&st->keys, 0, sizeof(st->keys));
		rg_status_error(st, "keys", errno);
	}
	errno = 0;
	values_ok = re_values_load(&st->path, &st->values) == 0;
	if (!values_ok) {
		memset(&st->values, 0, sizeof(st->values));
		if (keys_ok) {
			rg_status_error(st, "values", errno);
		}
	}
	st->loaded = (keys_ok || values_ok) ? 1 : 0;
	if (st->values.count == 0) {
		st->focus = RG_FOCUS_KEYS;
	}
}

int
rg_choice_count(int kind)
{
	if (kind == RG_CHOICE_CONSUMER) {
		return ((int)(sizeof(rg_consumer_names) /
		    sizeof(rg_consumer_names[0])));
	}
	return ((int)(sizeof(rg_type_names) / sizeof(rg_type_names[0])));
}

const char *
rg_choice_item(int kind, int index)
{
	if (index < 0 || index >= rg_choice_count(kind)) {
		return ("");
	}
	if (kind == RG_CHOICE_CONSUMER) {
		return (rg_consumer_names[index]);
	}
	return (rg_type_names[index]);
}

const char *
rg_choice_hint(int kind, int index)
{
	if (index < 0 || index >= rg_choice_count(kind)) {
		return ("");
	}
	if (kind == RG_CHOICE_CONSUMER) {
		return ("re-read this subsystem's registry settings");
	}
	return (re_type_hint(re_type_id(rg_type_names[index])));
}
