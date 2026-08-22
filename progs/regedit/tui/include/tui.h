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
$define %type size_t as native object size
$define %type api_key_event as native keyboard event
$define %type rt_focus as which pane owns the selection
$define %type rt_state as regedit tui global state

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
$define %func rt_screen_field_tail as procedure with args const char *, int
$define %func rt_key_read as procedure with args api_key_event *
$define %func rt_reload as procedure with args rt_state *
$define %func rt_status as procedure with args rt_state *, const char *
$define %func rt_status_fmt as procedure with args rt_state *, fmt, args
$define %func rt_status_error as procedure with args rt_state *, what, code
$define %func rt_rows_visible as function with args rt_state *
$define %func rt_key_count as function with args rt_state *
$define %func rt_draw as procedure with args rt_state *
$define %func rt_help as procedure with args rt_state *
$define %func rt_prompt as function with args rt_state *, label, char *, size_t
$define %func rt_confirm as function with args rt_state *, const char *
$define %func rt_dispatch as function with args rt_state *, api_key_event *

*/

/* !SPACE!

$space %export rt_focus_t, rt_state_t
$space %export rt_screen_size, rt_screen_home, rt_screen_clear
$space %export rt_screen_move, rt_screen_color, rt_screen_reset
$space %export rt_screen_erase_line, rt_screen_put, rt_screen_put_n
$space %export rt_screen_pad, rt_screen_field, rt_screen_field_tail
$space %export rt_key_read
$space %export rt_reload, rt_status, rt_status_fmt, rt_status_error
$space %export rt_rows_visible, rt_key_count, rt_draw, rt_help
$space %export rt_prompt, rt_confirm, rt_dispatch

*/

#ifndef PROGS_REGEDIT_TUI_H
#define PROGS_REGEDIT_TUI_H

#include <native.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>

#define RT_KEY_A		0x0004
#define RT_KEY_G		0x000a
#define RT_KEY_N		0x0011
#define RT_KEY_Q		0x0014
#define RT_KEY_R		0x0015
#define RT_KEY_S		0x0016
#define RT_KEY_V		0x0019
#define RT_KEY_ENTER		0x0028
#define RT_KEY_ESC		0x0029
#define RT_KEY_BACKSPACE	0x002a
#define RT_KEY_TAB		0x002b
#define RT_KEY_F1		0x003a
#define RT_KEY_F2		0x003b
#define RT_KEY_F3		0x003c
#define RT_KEY_F4		0x003d
#define RT_KEY_F5		0x003e
#define RT_KEY_HOME		0x004a
#define RT_KEY_PAGEUP		0x004b
#define RT_KEY_DELETE		0x004c
#define RT_KEY_END		0x004d
#define RT_KEY_PAGEDOWN		0x004e
#define RT_KEY_RIGHT		0x004f
#define RT_KEY_LEFT		0x0050
#define RT_KEY_DOWN		0x0051
#define RT_KEY_UP		0x0052
#define RT_KEY_KP_ENTER		0x0058

#define RT_KEY_EVENT_PRESS	0x00000001
#define RT_MOD_CTRL		0x0000000c

#define RT_STATUS_MAX		192
#define RT_ROWS_MIN		25
#define RT_COLS_MIN		80
#define RT_PANE_MIN		20
#define RT_PANE_SHARE		38

typedef enum rt_focus {
	RT_FOCUS_KEYS = 0,
	RT_FOCUS_VALUES = 1
} rt_focus_t;

typedef struct rt_state {
	re_path_t	path;
	re_hives_t	hives;
	re_keys_t	keys;
	re_values_t	values;
	rt_focus_t	focus;
	int		key_sel;
	int		key_off;
	int		value_sel;
	int		value_off;
	int		rows;
	int		cols;
	int		quit;
	int		loaded;
	char		status[RT_STATUS_MAX];
} rt_state_t;

void	rt_screen_size(rt_state_t *st);
void	rt_screen_home(void);
void	rt_screen_clear(void);
void	rt_screen_move(int row, int col);
void	rt_screen_color(int code);
void	rt_screen_reset(void);
void	rt_screen_erase_line(void);
void	rt_screen_put(const char *text);
void	rt_screen_put_n(const char *text, int len);
void	rt_screen_pad(char fill, int count);
void	rt_screen_field(const char *text, int width);
void	rt_screen_field_tail(const char *text, int width);
void	rt_key_read(struct api_key_event *ev);

void	rt_reload(rt_state_t *st);
void	rt_status(rt_state_t *st, const char *text);
void	rt_status_fmt(rt_state_t *st, const char *fmt, ...);
void	rt_status_error(rt_state_t *st, const char *what, int code);
int	rt_rows_visible(const rt_state_t *st);
int	rt_key_count(const rt_state_t *st);

void	rt_draw(rt_state_t *st);
void	rt_help(rt_state_t *st);
int	rt_prompt(rt_state_t *st, const char *label, char *out, size_t size);
int	rt_confirm(rt_state_t *st, const char *question);
int	rt_dispatch(rt_state_t *st, const struct api_key_event *ev);

#endif
