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

$define %func tui_out as procedure with args const char *, int
$define %func tui_out_str as procedure with args const char *
$define %func tui_out_int as procedure with args int
$define %func tui_clip_row as function with args int
$define %func tui_clip_col as function with args int
$define %func tui_wrap_line as function with args const char *, int, int *
$define %func tui_dialog_width as function with args const char *, const char *
$define %func tui_run_modal as function with args tui_win_t *, const char *, tui_buttons_t *

*/

/* !SPACE!

$space %internal tui_out, tui_out_str, tui_out_int
$space %internal tui_clip_row, tui_clip_col, tui_wrap_line
$space %internal tui_dialog_width, tui_run_modal

*/

#ifndef LIBTUI_TUI_INT_H
#define LIBTUI_TUI_INT_H

#include <stddef.h>
#include <tui.h>

#define TUI_BUF_MAX		(64 * 1024)
#define TUI_DLG_MARGIN		6
#define TUI_DLG_WIDTH_MIN	32
#define TUI_DLG_WIDTH_MAX	72
#define TUI_DLG_TEXT_MAX	10
#define TUI_BOX_H		'-'
#define TUI_BOX_V		'|'
#define TUI_BOX_TL		'+'
#define TUI_BOX_TR		'+'
#define TUI_BOX_BL		'+'
#define TUI_BOX_BR		'+'

void	tui_out(const char *text, int len);
void	tui_out_str(const char *text);
void	tui_out_int(int value);

int	tui_clip_row(int row);
int	tui_clip_col(int col);

int	tui_wrap_line(const char *text, int width, int *skip);
int	tui_dialog_width(const char *title, const char *text);
int	tui_run_modal(tui_win_t *win, const char *text, tui_buttons_t *row);

#endif
