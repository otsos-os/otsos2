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

$define %type size_t as native object size
$define %type ssize_t as native signed size
$define %type api_term_info as native terminal geometry
$define %type api_key_event as native keyboard event
$define %type rt_state as regedit tui global state

$define %func rt_write as procedure with args const char *, int
$define %func rt_screen_size as procedure with args rt_state *
$define %func rt_screen_home as procedure with args void
$define %func rt_screen_clear as procedure with args void
$define %func rt_screen_move as procedure with args int, int
$define %func rt_screen_color as procedure with args int
$define %func rt_screen_reset as procedure with args void
$define %func rt_screen_erase_line as procedure with args void
$define %func rt_screen_put as procedure with args const char *
$define %func rt_screen_put_n as procedure with args const char *, int
$define %func rt_screen_pad as procedure with args char, int
$define %func rt_screen_field as procedure with args const char *, int
$define %func rt_key_read as procedure with args api_key_event *

*/

/* !SPACE!

$space %internal rt_write
$space %export rt_screen_size, rt_screen_home, rt_screen_clear
$space %export rt_screen_move, rt_screen_color, rt_screen_reset
$space %export rt_screen_erase_line, rt_screen_put, rt_screen_put_n
$space %export rt_screen_pad, rt_screen_field, rt_key_read

*/

#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tui.h"

#define RT_PAD_CHUNK	64

static void
rt_write(const char *text, int len)
{
	ssize_t	done, n;

	if (!text || len <= 0) {
		return;
	}
	done = 0;
	while (done < (ssize_t)len) {
		n = termWrite(text + done, (size_t)((ssize_t)len - done));
		if (n <= 0) {
			return;
		}
		done += n;
	}
}

void
rt_screen_size(rt_state_t *st)
{
	struct api_term_info	info;

	memset(&info, 0, sizeof(info));
	if (termInfo(&info) == 0 && info.rows >= 6 && info.cols >= 32) {
		st->rows = (int)info.rows;
		st->cols = (int)info.cols;
		return;
	}
	st->rows = RT_ROWS_MIN;
	st->cols = RT_COLS_MIN;
}

void
rt_screen_home(void)
{
	rt_write("\033[H", 3);
}

void
rt_screen_clear(void)
{
	rt_write("\033[2J\033[H", 7);
}

void
rt_screen_move(int row, int col)
{
	char	buf[24];
	int	len;

	if (row < 1) {
		row = 1;
	}
	if (col < 1) {
		col = 1;
	}
	len = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
	if (len > 0) {
		rt_write(buf, len);
	}
}

void
rt_screen_color(int code)
{
	char	buf[12];
	int	len;

	len = snprintf(buf, sizeof(buf), "\033[%dm", code);
	if (len > 0) {
		rt_write(buf, len);
	}
}

void
rt_screen_reset(void)
{
	rt_write("\033[0m", 4);
}

void
rt_screen_erase_line(void)
{
	rt_write("\033[K", 3);
}

void
rt_screen_put(const char *text)
{
	if (!text) {
		return;
	}
	rt_write(text, (int)strlen(text));
}

void
rt_screen_put_n(const char *text, int len)
{
	rt_write(text, len);
}

void
rt_screen_pad(char fill, int count)
{
	char	buf[RT_PAD_CHUNK];
	int	n;

	while (count > 0) {
		n = count;
		if (n > (int)sizeof(buf)) {
			n = (int)sizeof(buf);
		}
		memset(buf, fill, (size_t)n);
		rt_write(buf, n);
		count -= n;
	}
}

void
rt_screen_field(const char *text, int width)
{
	int	len;

	if (width <= 0) {
		return;
	}
	len = text ? (int)strlen(text) : 0;
	if (len > width) {
		len = width;
	}
	rt_write(text, len);
	rt_screen_pad(' ', width - len);
}

void
rt_key_read(struct api_key_event *ev)
{
	int	n;

	for (;;) {
		memset(ev, 0, sizeof(*ev));
		n = inputRead(ev, 1, 0);
		if (n == 1 && (ev->flags & RT_KEY_EVENT_PRESS) != 0) {
			return;
		}
	}
}
