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

$define %type api_term_info as native terminal geometry

$define %func tui_out as procedure with args const char *, int
$define %func tui_out_str as procedure with args const char *
$define %func tui_out_int as procedure with args int
$define %func tui_clip_row as function with args int
$define %func tui_clip_col as function with args int
$define %func tui_init as function with args void
$define %func tui_fini as procedure with args void
$define %func tui_rows as function with args void
$define %func tui_cols as function with args void
$define %func tui_resize as function with args void
$define %func tui_flush as procedure with args void
$define %func tui_move as procedure with args int, int
$define %func tui_attr as procedure with args int, int
$define %func tui_attr_bold as procedure with args int, int
$define %func tui_attr_reset as procedure with args void
$define %func tui_put as procedure with args const char *
$define %func tui_put_n as procedure with args const char *, int
$define %func tui_putf as procedure with args const char *, args
$define %func tui_pad as procedure with args char, int
$define %func tui_field as procedure with args const char *, int
$define %func tui_field_right as procedure with args const char *, int
$define %func tui_cursor as procedure with args int

*/

/* !SPACE!

$space %internal tui_raw_write, tui_sgr
$space %export tui_out, tui_out_str, tui_out_int
$space %export tui_clip_row, tui_clip_col
$space %export tui_init, tui_fini, tui_rows, tui_cols, tui_resize
$space %export tui_flush, tui_move, tui_attr, tui_attr_bold, tui_attr_reset
$space %export tui_put, tui_put_n, tui_putf, tui_pad
$space %export tui_field, tui_field_right, tui_cursor

*/

#include <native.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "tui_int.h"

static char	tui_buf[TUI_BUF_MAX];
static int	tui_buf_len;
static int	tui_screen_rows;
static int	tui_screen_cols;
static int	tui_ready;


static void
tui_raw_write(const char *text, int len)
{
	ssize_t	done;
	ssize_t	n;

	if (text == NULL || len <= 0) {
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
tui_out(const char *text, int len)
{
	if (text == NULL || len <= 0) {
		return;
	}
	if (len > TUI_BUF_MAX) {

		tui_flush();
		tui_raw_write(text, len);
		return;
	}
	if (tui_buf_len + len > TUI_BUF_MAX) {
		tui_flush();
	}
	memcpy(tui_buf + tui_buf_len, text, (size_t)len);
	tui_buf_len += len;
}

void
tui_out_str(const char *text)
{
	if (text == NULL) {
		return;
	}
	tui_out(text, (int)strlen(text));
}

void
tui_out_int(int value)
{
	char	num[12];
	int	len;

	len = snprintf(num, sizeof(num), "%d", value);
	if (len > 0) {
		tui_out(num, len);
	}
}

void
tui_flush(void)
{
	if (tui_buf_len == 0) {
		return;
	}
	tui_raw_write(tui_buf, tui_buf_len);
	tui_buf_len = 0;
}

int
tui_resize(void)
{
	struct api_term_info	info;
	int			rows;
	int			cols;

	memset(&info, 0, sizeof(info));
	if (termInfo(&info) != 0 || info.rows == 0 || info.cols == 0) {

		return (tui_ready ? 0 : -1);
	}
	rows = (int)info.rows;
	cols = (int)info.cols;
	if (rows == tui_screen_rows && cols == tui_screen_cols) {
		return (0);
	}
	tui_screen_rows = rows;
	tui_screen_cols = cols;
	return (1);
}

int
tui_init(void)
{
	tui_buf_len = 0;
	tui_screen_rows = 0;
	tui_screen_cols = 0;
	tui_ready = 0;

	if (tui_resize() < 0) {
		return (-1);
	}
	if (tui_screen_rows < TUI_ROWS_MIN || tui_screen_cols < TUI_COLS_MIN) {

		return (-1);
	}
	tui_ready = 1;

	

	tui_cursor(0);
	tui_attr_reset();
	tui_flush();
	(void)inputFlush();
	return (0);
}

void
tui_fini(void)
{
	if (!tui_ready) {
		return;
	}
	tui_attr_reset();
	tui_cursor(1);
	tui_flush();
	tui_ready = 0;
}

int
tui_rows(void)
{
	return (tui_screen_rows);
}

int
tui_cols(void)
{
	return (tui_screen_cols);
}

int
tui_clip_row(int row)
{
	if (row < 1) {
		return (1);
	}
	if (row > tui_screen_rows) {
		return (tui_screen_rows);
	}
	return (row);
}

int
tui_clip_col(int col)
{
	if (col < 1) {
		return (1);
	}
	if (col > tui_screen_cols) {
		return (tui_screen_cols);
	}
	return (col);
}

void
tui_move(int row, int col)
{
	tui_out_str("\033[");
	tui_out_int(tui_clip_row(row));
	tui_out(";", 1);
	tui_out_int(tui_clip_col(col));
	tui_out("H", 1);
}


static void
tui_sgr(int bold, int fg, int bg)
{
	tui_out_str("\033[0");
	if (bold) {
		tui_out_str(";1");
	}
	if (fg > 0) {
		tui_out(";", 1);
		tui_out_int(fg);
	}
	if (bg > 0) {
		tui_out(";", 1);
		tui_out_int(bg);
	}
	tui_out("m", 1);
}

void
tui_attr(int fg, int bg)
{
	tui_sgr(0, fg, bg);
}

void
tui_attr_bold(int fg, int bg)
{
	tui_sgr(1, fg, bg);
}

void
tui_attr_reset(void)
{
	tui_out_str("\033[0m");
}

void
tui_put(const char *text)
{
	tui_out_str(text);
}

void
tui_put_n(const char *text, int len)
{
	tui_out(text, len);
}

void
tui_putf(const char *fmt, ...)
{
	char	line[TUI_TEXT_MAX];
	va_list	ap;
	int	len;

	if (fmt == NULL) {
		return;
	}
	va_start(ap, fmt);
	len = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	if (len <= 0) {
		return;
	}
	if (len > (int)sizeof(line) - 1) {
		len = (int)sizeof(line) - 1;
	}
	tui_out(line, len);
}

void
tui_pad(char fill, int count)
{
	char	chunk[64];
	int	n;

	if (count <= 0) {
		return;
	}
	memset(chunk, fill, sizeof(chunk));
	while (count > 0) {
		n = count;
		if (n > (int)sizeof(chunk)) {
			n = (int)sizeof(chunk);
		}
		tui_out(chunk, n);
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
	len = (text != NULL) ? (int)strlen(text) : 0;
	if (len > width) {
		len = width;
	}
	if (len > 0) {
		tui_out(text, len);
	}
	tui_pad(' ', width - len);
}

void
tui_field_right(const char *text, int width)
{
	int	len;

	if (width <= 0) {
		return;
	}
	len = (text != NULL) ? (int)strlen(text) : 0;
	if (len > width) {
		tui_out(text + (len - width), width);
		return;
	}
	tui_pad(' ', width - len);
	if (len > 0) {
		tui_out(text, len);
	}
}

void
tui_cursor(int visible)
{
	tui_out_str(visible ? "\033[?25h" : "\033[?25l");
}
