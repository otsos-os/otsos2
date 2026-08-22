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
$define %type rt_state as regedit tui global state

$define %func rt_status as procedure with args rt_state *, const char *
$define %func rt_status_fmt as procedure with args rt_state *, fmt, args
$define %func rt_status_error as procedure with args rt_state *, what, code
$define %func rt_rows_visible as function with args rt_state *
$define %func rt_key_count as function with args rt_state *
$define %func rt_reload as procedure with args rt_state *

*/

/* !SPACE!

$space %export rt_status, rt_status_fmt, rt_status_error
$space %export rt_rows_visible, rt_key_count, rt_reload

*/

#include <errno.h>
#include <regedit/regedit.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

void
rt_status(rt_state_t *st, const char *text)
{
	snprintf(st->status, sizeof(st->status), "%s", text ? text : "");
}

void
rt_status_fmt(rt_state_t *st, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vsnprintf(st->status, sizeof(st->status), fmt, ap);
	va_end(ap);
}

void
rt_status_error(rt_state_t *st, const char *what, int code)
{
	rt_status_fmt(st, "%s: %s", what, re_error(code));
}

int
rt_rows_visible(const rt_state_t *st)
{
	int	rows;

	rows = st->rows - 4;
	if (rows < 1) {
		rows = 1;
	}
	return (rows);
}

int
rt_key_count(const rt_state_t *st)
{
	if (st->path.hive[0] == '\0') {
		return ((int)st->hives.count);
	}
	return ((int)st->keys.count);
}

void
rt_reload(rt_state_t *st)
{
	int	keys_ok, values_ok;

	if (st->path.hive[0] == '\0') {
		memset(&st->keys, 0, sizeof(st->keys));
		memset(&st->values, 0, sizeof(st->values));
		errno = 0;
		if (re_hives_load(&st->hives) != 0) {
			memset(&st->hives, 0, sizeof(st->hives));
			st->loaded = 0;
			rt_status_error(st, "hives", errno);
			return;
		}
		st->loaded = 1;
		st->focus = RT_FOCUS_KEYS;
		return;
	}

	errno = 0;
	keys_ok = re_keys_load(&st->path, &st->keys) == 0;
	if (!keys_ok) {
		memset(&st->keys, 0, sizeof(st->keys));
		rt_status_error(st, "keys", errno);
	}
	errno = 0;
	values_ok = re_values_load(&st->path, &st->values) == 0;
	if (!values_ok) {
		memset(&st->values, 0, sizeof(st->values));
		if (keys_ok) {
			rt_status_error(st, "values", errno);
		}
	}
	st->loaded = (keys_ok || values_ok) ? 1 : 0;
	if (st->keys.count == 0 && st->focus == RT_FOCUS_KEYS &&
	    st->values.count != 0) {
		st->focus = RT_FOCUS_VALUES;
	}
	if (st->values.count == 0) {
		st->focus = RT_FOCUS_KEYS;
	}
}
