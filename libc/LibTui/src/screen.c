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

$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type size_t as native object size
$define %type ssize_t as native signed size
$define %type va_list as variadic argument cursor
$define %type api_term_info as native terminal geometry
$define %type tui_screen_t as library-wide screen state

$define %func tui_screen as function with args void
$define %func tui_out as procedure with args const char *, int
$define %func tui_flush as procedure with args void
$define %func tui_resize as procedure with args void
$define %func tui_init as function with args void
$define %func tui_fini as procedure with args void
$define %func tui_rows as function with args void
$define %func tui_cols as function with args void
$define %func tui_clear as procedure with args void
$define %func tui_move as procedure with args int, int
$define %func tui_attr as procedure with args int
$define %func tui_attr_reset as procedure with args void
$define %func tui_erase_line as procedure with args void
$define %func tui_cursor as procedure with args int
$define %func tui_put as procedure with args const char *
$define %func tui_put_n as procedure with args const char *, int
$define %func tui_putf as procedure with args const char *, args
$define %func tui_pad as procedure with args char, int
$define %func tui_field as procedure with args const char *, int
$define %func tui_field_right as procedure with args const char *, int

*/

/* !SPACE!

$space %export tui_screen, tui_out, tui_flush, tui_resize
$space %export tui_init, tui_fini, tui_rows, tui_cols
$space %export tui_clear, tui_move, tui_attr, tui_attr_reset
$space %export tui_erase_line, tui_cursor
$space %export tui_put, tui_put_n, tui_putf, tui_pad
$space %export tui_field, tui_field_right

*/

#include <native.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tui_int.h"

#define TUI_PAD_CHUNK	64

static tui_screen_t	tui_state;

tui_screen_t *
tui_screen(void)
{
	return (&tui_state);
}


static void
tui_write_raw(const char *text, int len)
{
	ssize_t	done, n;

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
tui_out(const char *text, int len)
{
	tui_screen_t	*sc;
	int		room, take;

	if (text == NULL || len <= 0) {
		return;
	}
	sc = &tui_state;
	while (len > 0) {
		room = TUI_OUT_BUF - sc->pending;
		if (room == 0) {
			tui_flush();
			continue;
		}
		take = len < room ? len : room;
		memcpy(sc->buf + sc->pending, text, (size_t)take);
		sc->pending += take;
		text += take;
		len -= take;
	}
}

void
tui_flush(void)
{
	tui_screen_t	*sc;

	sc = &tui_state;
	if (sc->pending <= 0) {
		return;
	}
	tui_write_raw(sc->buf, sc->pending);
	sc->pending = 0;
}



void
tui_resize(void)
{
	struct api_term_info	info;
	tui_screen_t		*sc;

	sc = &tui_state;
	memset(&info, 0, sizeof(info));
	if (termInfo(&info) == 0 && info.rows >= 6 && info.cols >= 32) {
		sc->rows = (int)info.rows;
		sc->cols = (int)info.cols;
		return;
	}
	sc->rows = TUI_ROWS_MIN;
	sc->cols = TUI_COLS_MIN;
}

int
tui_init(void)
{
	tui_screen_t	*sc;

	sc = &tui_state;
	sc->pending = 0;
	tui_resize();
	sc->ready = 1;
	tui_cursor(0);
	tui_clear();
	tui_flush();
	return (0);
}


void
tui_fini(void)
{
	tui_screen_t	*sc;

	sc = &tui_state;
	if (!sc->ready) {
		return;
	}
	tui_attr_reset();
	tui_clear();
	tui_cursor(1);
	tui_flush();
	sc->ready = 0;
}

int
tui_rows(void)
{
	return (tui_state.rows);
}

int
tui_cols(void)
{
	return (tui_state.cols);
}

void
tui_clear(void)
{
	tui_out("\033[2J\033[H", 7);
}

void
tui_move(int row, int col)
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
		tui_out(buf, len);
	}
}

void
tui_attr(int sgr)
{
	char	buf[12];
	int	len;

	len = snprintf(buf, sizeof(buf), "\033[%dm", sgr);
	if (len > 0) {
		tui_out(buf, len);
	}
}

void
tui_attr_reset(void)
{
	tui_out("\033[0m", 4);
}

void
tui_erase_line(void)
{
	tui_out("\033[K", 3);
}

void
tui_cursor(int visible)
{
	if (visible) {
		tui_out("\033[?25h", 6);
	} else {
		tui_out("\033[?25l", 6);
	}
}

void
tui_put(const char *text)
{
	if (text == NULL) {
		return;
	}
	tui_out(text, (int)strlen(text));
}

void
tui_put_n(const char *text, int len)
{
	tui_out(text, len);
}

void
tui_putf(const char *fmt, ...)
{
	va_list	ap;
	char	buf[TUI_TEXT_MAX];
	int	len;

	if (fmt == NULL) {
		return;
	}
	va_start(ap, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	if (len <= 0) {
		return;
	}

	if (len > (int)sizeof(buf) - 1) {
		len = (int)sizeof(buf) - 1;
	}
	tui_out(buf, len);
}

void
tui_pad(char fill, int count)
{
	char	buf[TUI_PAD_CHUNK];
	int	n;

	while (count > 0) {
		n = count;
		if (n > (int)sizeof(buf)) {
			n = (int)sizeof(buf);
		}
		memset(buf, fill, (size_t)n);
		tui_out(buf, n);
		count -= n;
	}
}


void
tui_field(const char *text, int width)
{
	int	len;

	if (width <= 0) {
		return;
	}
	len = text != NULL ? (int)strlen(text) : 0;
	if (len > width) {
		len = width;
	}
	tui_out(text, len);
	tui_pad(' ', width - len);
}

void
tui_field_right(const char *text, int width)
{
	int	len;

	if (width <= 0) {
		return;
	}
	len = text != NULL ? (int)strlen(text) : 0;
	if (len > width) {
		text += len - width;
		len = width;
	}
	tui_pad(' ', width - len);
	tui_out(text, len);
}
