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

$define %func tui_theme as function with args void
$define %func tui_theme_set as procedure with args const tui_theme_t *
$define %func tui_fill as procedure with args const tui_rect_t *, int, int
$define %func tui_hline as procedure with args int, int, int
$define %func tui_border as procedure with args const tui_rect_t *, const char *, int
$define %func tui_center as procedure with args tui_rect_t *, int, int
$define %func tui_backdrop as procedure with args const char *
$define %func tui_wrap_line as function with args const char *, int, int *
$define %func tui_text_rows as function with args const char *, int
$define %func tui_text_draw as function with args const tui_rect_t *, const char *, int

*/

/* !SPACE!

$space %export tui_theme, tui_theme_set
$space %export tui_fill, tui_hline, tui_border, tui_center, tui_backdrop
$space %export tui_wrap_line, tui_text_rows, tui_text_draw

*/

#include <stddef.h>
#include <string.h>
#include "tui_int.h"


static tui_theme_t	tui_cur_theme = {
	.backdrop_fg		= TUI_FG_CYAN,
	.backdrop_bg		= TUI_BG_BLUE,
	.title_fg		= TUI_FG_BRIGHT,
	.title_bg		= TUI_BG_BLUE,
	.win_fg			= TUI_FG_WHITE,
	.win_bg			= TUI_BG_BLACK,
	.border_fg		= TUI_FG_CYAN,
	.border_focus_fg	= TUI_FG_BRIGHT,
	.text_fg		= TUI_FG_WHITE,
	.dim_fg			= TUI_FG_GREY,
	.sel_fg			= TUI_FG_BLACK,
	.sel_bg			= TUI_BG_CYAN,
	.sel_idle_fg		= TUI_FG_BRIGHT,
	.sel_idle_bg		= TUI_BG_BLACK,
	.button_fg		= TUI_FG_WHITE,
	.button_bg		= TUI_BG_BLACK,
	.button_focus_fg	= TUI_FG_BLACK,
	.button_focus_bg	= TUI_BG_CYAN,
	.field_fg		= TUI_FG_BLACK,
	.field_bg		= TUI_BG_WHITE,
	.status_fg		= TUI_FG_YELLOW,
	.status_bg		= TUI_BG_BLUE,
	.gauge_fg		= TUI_FG_BLACK,
	.gauge_bg		= TUI_BG_CYAN,
};

const tui_theme_t *
tui_theme(void)
{
	return (&tui_cur_theme);
}

void
tui_theme_set(const tui_theme_t *theme)
{
	if (theme == NULL) {
		return;
	}
	tui_cur_theme = *theme;
}

void
tui_fill(const tui_rect_t *rect, int fg, int bg)
{
	int	y;

	if (rect == NULL || rect->width <= 0 || rect->height <= 0) {
		return;
	}
	tui_attr(fg, bg);
	for (y = 0; y < rect->height; y++) {
		tui_move(rect->row + y, rect->col);
		tui_pad(' ', rect->width);
	}
}

void
tui_hline(int row, int col, int width)
{
	if (width <= 0) {
		return;
	}
	tui_move(row, col);
	tui_pad(TUI_BOX_H, width);
}


void
tui_border(const tui_rect_t *rect, const char *title, int focused)
{
	const tui_theme_t	*th;
	int			room;
	int			len;
	int			y;

	if (rect == NULL || rect->width < 2 || rect->height < 2) {
		return;
	}
	th = tui_theme();
	tui_attr(focused ? th->border_focus_fg : th->border_fg, th->win_bg);

	tui_move(rect->row, rect->col);
	tui_put_n((const char[]){ TUI_BOX_TL }, 1);
	tui_pad(TUI_BOX_H, rect->width - 2);
	tui_put_n((const char[]){ TUI_BOX_TR }, 1);

	for (y = 1; y < rect->height - 1; y++) {
		tui_move(rect->row + y, rect->col);
		tui_put_n((const char[]){ TUI_BOX_V }, 1);
		tui_move(rect->row + y, rect->col + rect->width - 1);
		tui_put_n((const char[]){ TUI_BOX_V }, 1);
	}

	tui_move(rect->row + rect->height - 1, rect->col);
	tui_put_n((const char[]){ TUI_BOX_BL }, 1);
	tui_pad(TUI_BOX_H, rect->width - 2);
	tui_put_n((const char[]){ TUI_BOX_BR }, 1);

	if (title == NULL || title[0] == '\0') {
		return;
	}
	room = rect->width - 6;
	if (room < 1) {
		return;
	}
	len = (int)strlen(title);
	if (len > room) {
		len = room;
	}
	tui_move(rect->row, rect->col + ((rect->width - len - 2) / 2));
	tui_attr_bold(th->title_fg, th->win_bg);
	tui_put(" ");
	tui_put_n(title, len);
	tui_put(" ");
}

void
tui_center(tui_rect_t *rect, int width, int height)
{
	int	rows;
	int	cols;

	if (rect == NULL) {
		return;
	}
	rows = tui_rows();
	cols = tui_cols();
	if (width > cols) {
		width = cols;
	}
	if (height > rows) {
		height = rows;
	}
	rect->width = width;
	rect->height = height;
	rect->row = ((rows - height) / 2) + 1;
	rect->col = ((cols - width) / 2) + 1;
	if (rect->row < 1) {
		rect->row = 1;
	}
	if (rect->col < 1) {
		rect->col = 1;
	}
}

void
tui_backdrop(const char *title)
{
	const tui_theme_t	*th;
	tui_rect_t		all;
	int			len;

	th = tui_theme();
	all.row = 1;
	all.col = 1;
	all.width = tui_cols();
	all.height = tui_rows();
	tui_fill(&all, th->backdrop_fg, th->backdrop_bg);

	if (title == NULL || title[0] == '\0') {
		return;
	}
	len = (int)strlen(title);
	if (len > all.width) {
		len = all.width;
	}
	tui_move(1, ((all.width - len) / 2) + 1);
	tui_attr_bold(th->title_fg, th->title_bg);
	tui_put_n(title, len);
}


int
tui_wrap_line(const char *text, int width, int *skip)
{
	int	len;
	int	brk;
	int	i;

	if (text == NULL || width < 1) {
		*skip = 0;
		return (0);
	}
	for (len = 0; len < width && text[len] != '\0'; len++) {
		if (text[len] == '\n') {
			*skip = len + 1;
			return (len);
		}
	}
	if (text[len] == '\0' || text[len] == '\n') {
		*skip = len + (text[len] == '\n' ? 1 : 0);
		return (len);
	}

	brk = -1;
	for (i = 0; i < len; i++) {
		if (text[i] == ' ') {
			brk = i;
		}
	}
	if (brk <= 0) {
		*skip = len;
		return (len);
	}
	*skip = brk + 1;
	return (brk);
}

int
tui_text_rows(const char *text, int width)
{
	int	rows;
	int	skip;
	int	pos;

	if (text == NULL || text[0] == '\0' || width < 1) {
		return (0);
	}
	rows = 0;
	pos = 0;
	while (text[pos] != '\0' && rows < TUI_TEXT_MAX) {
		(void)tui_wrap_line(text + pos, width, &skip);
		if (skip <= 0) {
			break;
		}
		pos += skip;
		rows++;
	}
	return (rows);
}


int
tui_text_draw(const tui_rect_t *rect, const char *text, int skip)
{
	const tui_theme_t	*th;
	int			drawn;
	int			line;
	int			step;
	int			len;
	int			pos;

	if (rect == NULL || rect->width < 1 || rect->height < 1) {
		return (0);
	}
	th = tui_theme();
	tui_attr(th->text_fg, th->win_bg);

	pos = 0;
	line = 0;
	drawn = 0;
	while (line < skip && text != NULL && text[pos] != '\0') {
		(void)tui_wrap_line(text + pos, rect->width, &step);
		if (step <= 0) {
			break;
		}
		pos += step;
		line++;
	}
	while (drawn < rect->height) {
		tui_move(rect->row + drawn, rect->col);
		if (text == NULL || text[pos] == '\0') {
			tui_pad(' ', rect->width);
			drawn++;
			continue;
		}
		len = tui_wrap_line(text + pos, rect->width, &step);
		tui_put_n(text + pos, len);
		tui_pad(' ', rect->width - len);
		if (step <= 0) {
			drawn++;
			break;
		}
		pos += step;
		drawn++;
	}
	return (drawn);
}
