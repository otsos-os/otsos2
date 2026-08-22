/* !DEFINES!

$define %type term_cell as terminal cell structure
$define %type terminal_app as terminal application state
$define %func term_present as target present callback
$define %func term_wait_surface as function with args connection, surface
$define %func term_init_colors as procedure with args app
$define %func term_clear_line as procedure with args app, row, start_col, end_col
$define %func term_clear_screen as procedure with args app, mode
$define %func term_scroll_up as procedure with args app
$define %func term_scroll_down as procedure with args app
$define %func term_put_char as procedure with args app, ch
$define %func term_handle_csi as procedure with args app, final_char
$define %func term_process_byte as procedure with args app, byte
$define %func term_process_output as procedure with args app, buf, len
$define %func term_translate_key as function with args key, mods, out, max_len
$define %func term_render as procedure with args app
$define %func term_handle_event as function with args app, event
$define %func term_cleanup as procedure with args app
$define %func main as start with args void

*/

/* !SPACE!

$space %internal term_present, term_wait_surface, term_init_colors
$space %internal term_clear_line, term_clear_screen, term_scroll_up
$space %internal term_scroll_down, term_put_char, term_handle_csi
$space %internal term_process_byte, term_process_output
$space %internal term_translate_key, term_render, term_handle_event
$space %internal term_cleanup
$space %export main

*/

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
 * ARE DISCLAIMED.
 */

#include <libg.h>
#include <native.h>
#include <sprot/client.h>
#include <sprot/sprot.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERM_COLS		80
#define TERM_ROWS		24
#define TERM_CELL_W		8
#define TERM_CELL_H		16
#define TERM_PAD_X		4
#define TERM_PAD_Y		4
#define TERM_MAX_PARAMS		16
#define TERM_BLINK_INTERVAL	500
#define TERM_READ_BUF_SIZE	1024

#define TERM_FLAG_BOLD		0x01
#define TERM_FLAG_INVERSE	0x02
#define TERM_FLAG_UNDERLINE	0x04

#define ESC_STATE_NONE		0
#define ESC_STATE_ESC		1
#define ESC_STATE_CSI		2
#define ESC_STATE_OSC		3

#define KEY_A			0x0004
#define KEY_Z			0x001d
#define KEY_1			0x001e
#define KEY_2			0x001f
#define KEY_3			0x0020
#define KEY_4			0x0021
#define KEY_5			0x0022
#define KEY_6			0x0023
#define KEY_7			0x0024
#define KEY_8			0x0025
#define KEY_9			0x0026
#define KEY_0			0x0027
#define KEY_ENTER		0x0028
#define KEY_ESC			0x0029
#define KEY_BACKSPACE		0x002a
#define KEY_TAB			0x002b
#define KEY_SPACE		0x002c
#define KEY_MINUS		0x002d
#define KEY_EQUAL		0x002e
#define KEY_LEFTBRACE		0x002f
#define KEY_RIGHTBRACE		0x0030
#define KEY_BACKSLASH		0x0031
#define KEY_SEMICOLON		0x0033
#define KEY_APOSTROPHE		0x0034
#define KEY_GRAVE		0x0035
#define KEY_COMMA		0x0036
#define KEY_DOT			0x0037
#define KEY_SLASH		0x0038
#define KEY_CAPSLOCK		0x0039
#define KEY_F1			0x003a
#define KEY_F2			0x003b
#define KEY_F3			0x003c
#define KEY_F4			0x003d
#define KEY_F5			0x003e
#define KEY_F6			0x003f
#define KEY_F7			0x0040
#define KEY_F8			0x0041
#define KEY_F9			0x0042
#define KEY_F10			0x0043
#define KEY_F11			0x0044
#define KEY_F12			0x0045
#define KEY_INSERT		0x0049
#define KEY_HOME		0x004a
#define KEY_PAGEUP		0x004b
#define KEY_DELETE		0x004c
#define KEY_END			0x004d
#define KEY_PAGEDOWN		0x004e
#define KEY_RIGHT		0x004f
#define KEY_LEFT		0x0050
#define KEY_DOWN		0x0051
#define KEY_UP			0x0052
#define KEY_KP_SLASH		0x0054
#define KEY_KP_ASTERISK		0x0055
#define KEY_KP_MINUS		0x0056
#define KEY_KP_PLUS		0x0057
#define KEY_KP_ENTER		0x0058
#define KEY_KP_1		0x0059
#define KEY_KP_2		0x005a
#define KEY_KP_3		0x005b
#define KEY_KP_4		0x005c
#define KEY_KP_5		0x005d
#define KEY_KP_6		0x005e
#define KEY_KP_7		0x005f
#define KEY_KP_8		0x0060
#define KEY_KP_9		0x0061
#define KEY_KP_0		0x0062
#define KEY_KP_DOT		0x0063

#define MOD_LSHIFT		0x00000001
#define MOD_RSHIFT		0x00000002
#define MOD_LCTRL		0x00000004
#define MOD_RCTRL		0x00000008
#define MOD_LALT		0x00000010
#define MOD_RALT		0x00000020
#define MOD_CAPS		0x00000100
#define MOD_SHIFT		(MOD_LSHIFT | MOD_RSHIFT)
#define MOD_CTRL		(MOD_LCTRL | MOD_RCTRL)
#define MOD_ALT			(MOD_LALT | MOD_RALT)

struct term_cell {
	uint32_t	fg;
	uint32_t	bg;
	uint16_t	ch;
	uint8_t		flags;
};

struct terminal_app {
	sprot_connection_t	*conn;
	sprot_surface_t		*surface;
	libg_context_t		*ui;
	uint32_t		width;
	uint32_t		height;
	uint32_t		cols;
	uint32_t		rows;
	int			master_fd;
	int			pts_id;
	int			child_pid;
	int			running;
	int			dirty;
	int			focused;
	int			cursor_x;
	int			cursor_y;
	int			saved_x;
	int			saved_y;
	int			cursor_visible;
	int			cursor_blink_state;
	uint64_t		last_blink_time;
	uint32_t		cur_fg;
	uint32_t		cur_bg;
	uint8_t			cur_flags;
	uint32_t		default_fg;
	uint32_t		default_bg;
	uint32_t		palette[16];
	int			esc_state;
	int			esc_params[TERM_MAX_PARAMS];
	int			esc_param_count;
	int			esc_private;
	struct term_cell	cells[TERM_ROWS][TERM_COLS];
};

static int
term_present(void *userdata, const struct srapi_region *region)
{
	sprot_surface_t	*surface;
	uint32_t	width, height;

	surface = (sprot_surface_t *)userdata;
	if (surface == NULL) {
		return (-1);
	}
	width = sprot_surface_width(surface);
	height = sprot_surface_height(surface);
	if (region != NULL) {
		if (sprot_damage(surface, region->x, region->y, region->width,
		    region->height) != 0) {
			return (-1);
		}
	} else if (sprot_damage(surface, 0, 0, width, height) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}

static int
term_wait_surface(sprot_connection_t *connection, sprot_surface_t *surface)
{
	sprot_event_t	event;
	int		i, ret;

	for (i = 0; i < 100; i++) {
		if (sprot_surface_id(surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(connection, &event, 20);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_SURFACE_CREATED &&
		    sprot_surface_id(surface) != 0) {
			return (0);
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (sprot_surface_id(surface) != 0 ? 0 : -1);
}

static void
term_init_colors(struct terminal_app *app)
{
	app->palette[0] = 0xff1e1e2eU;
	app->palette[1] = 0xfff38ba8U;
	app->palette[2] = 0xffa6e3a1U;
	app->palette[3] = 0xfff9e2afU;
	app->palette[4] = 0xff89b4faU;
	app->palette[5] = 0xffcba6f7U;
	app->palette[6] = 0xff89dcebU;
	app->palette[7] = 0xffcdd6f4U;
	app->palette[8] = 0xff585b70U;
	app->palette[9] = 0xffeba0acU;
	app->palette[10] = 0xff94e2d5U;
	app->palette[11] = 0xfff9e2afU;
	app->palette[12] = 0xff74c7ecU;
	app->palette[13] = 0xfff5c2e7U;
	app->palette[14] = 0xff94e2d5U;
	app->palette[15] = 0xffffffffU;

	app->default_fg = app->palette[7];
	app->default_bg = 0xff11111bU;
	app->cur_fg = app->default_fg;
	app->cur_bg = app->default_bg;
	app->cur_flags = 0;
}

static void
term_clear_line(struct terminal_app *app, int row, int start_col, int end_col)
{
	int	c;

	if (row < 0 || row >= (int)app->rows) {
		return;
	}
	if (start_col < 0) {
		start_col = 0;
	}
	if (end_col > (int)app->cols) {
		end_col = (int)app->cols;
	}
	for (c = start_col; c < end_col; c++) {
		app->cells[row][c].ch = ' ';
		app->cells[row][c].fg = app->cur_fg;
		app->cells[row][c].bg = app->cur_bg;
		app->cells[row][c].flags = 0;
	}
}

static void
term_clear_screen(struct terminal_app *app, int mode)
{
	int	r;

	if (mode == 0) {
		term_clear_line(app, app->cursor_y, app->cursor_x, app->cols);
		for (r = app->cursor_y + 1; r < (int)app->rows; r++) {
			term_clear_line(app, r, 0, app->cols);
		}
	} else if (mode == 1) {
		for (r = 0; r < app->cursor_y; r++) {
			term_clear_line(app, r, 0, app->cols);
		}
		term_clear_line(app, app->cursor_y, 0, app->cursor_x + 1);
	} else {
		for (r = 0; r < (int)app->rows; r++) {
			term_clear_line(app, r, 0, app->cols);
		}
		if (mode == 2 || mode == 3) {
			app->cursor_x = 0;
			app->cursor_y = 0;
		}
	}
}

static void
term_scroll_up(struct terminal_app *app)
{
	int	r;

	for (r = 0; r < (int)app->rows - 1; r++) {
		memcpy(app->cells[r], app->cells[r + 1],
		    sizeof(struct term_cell) * app->cols);
	}
	term_clear_line(app, (int)app->rows - 1, 0, (int)app->cols);
}

static void
term_scroll_down(struct terminal_app *app)
{
	int	r;

	for (r = (int)app->rows - 1; r > 0; r--) {
		memcpy(app->cells[r], app->cells[r - 1],
		    sizeof(struct term_cell) * app->cols);
	}
	term_clear_line(app, 0, 0, (int)app->cols);
}

static void
term_put_char(struct terminal_app *app, char ch)
{
	if (ch == '\r') {
		app->cursor_x = 0;
		return;
	}
	if (ch == '\n') {
		app->cursor_x = 0;
		app->cursor_y++;
		if (app->cursor_y >= (int)app->rows) {
			term_scroll_up(app);
			app->cursor_y = (int)app->rows - 1;
		}
		return;
	}
	if (ch == '\b') {
		if (app->cursor_x > 0) {
			app->cursor_x--;
		}
		return;
	}
	if (ch == '\t') {
		app->cursor_x = (app->cursor_x + 8) & ~7;
		if (app->cursor_x >= (int)app->cols) {
			app->cursor_x = (int)app->cols - 1;
		}
		return;
	}
	if (ch == '\a') {
		return;
	}

	if (app->cursor_x >= (int)app->cols) {
		app->cursor_x = 0;
		app->cursor_y++;
		if (app->cursor_y >= (int)app->rows) {
			term_scroll_up(app);
			app->cursor_y = (int)app->rows - 1;
		}
	}

	if (app->cursor_y >= 0 && app->cursor_y < (int)app->rows &&
	    app->cursor_x >= 0 && app->cursor_x < (int)app->cols) {
		app->cells[app->cursor_y][app->cursor_x].ch =
		    (uint16_t)(unsigned char)ch;
		app->cells[app->cursor_y][app->cursor_x].fg = app->cur_fg;
		app->cells[app->cursor_y][app->cursor_x].bg = app->cur_bg;
		app->cells[app->cursor_y][app->cursor_x].flags = app->cur_flags;
		app->cursor_x++;
	}
}

static void
term_handle_csi(struct terminal_app *app, char final_char)
{
	int	p1, p2, i, p;

	p1 = app->esc_param_count > 0 ? app->esc_params[0] : 0;
	p2 = app->esc_param_count > 1 ? app->esc_params[1] : 0;

	switch (final_char) {
	case 'm':
		if (app->esc_param_count == 0) {
			app->cur_fg = app->default_fg;
			app->cur_bg = app->default_bg;
			app->cur_flags = 0;
			break;
		}
		for (i = 0; i < app->esc_param_count; i++) {
			p = app->esc_params[i];
			if (p == 0) {
				app->cur_fg = app->default_fg;
				app->cur_bg = app->default_bg;
				app->cur_flags = 0;
			} else if (p == 1) {
				app->cur_flags |= TERM_FLAG_BOLD;
			} else if (p == 4) {
				app->cur_flags |= TERM_FLAG_UNDERLINE;
			} else if (p == 7) {
				app->cur_flags |= TERM_FLAG_INVERSE;
			} else if (p == 22) {
				app->cur_flags &= ~TERM_FLAG_BOLD;
			} else if (p == 24) {
				app->cur_flags &= ~TERM_FLAG_UNDERLINE;
			} else if (p == 27) {
				app->cur_flags &= ~TERM_FLAG_INVERSE;
			} else if (p >= 30 && p <= 37) {
				app->cur_fg = app->palette[p - 30 +
				    ((app->cur_flags & TERM_FLAG_BOLD) ? 8 : 0)];
			} else if (p == 39) {
				app->cur_fg = app->default_fg;
			} else if (p >= 40 && p <= 47) {
				app->cur_bg = app->palette[p - 40];
			} else if (p == 49) {
				app->cur_bg = app->default_bg;
			} else if (p >= 90 && p <= 97) {
				app->cur_fg = app->palette[p - 90 + 8];
			} else if (p >= 100 && p <= 107) {
				app->cur_bg = app->palette[p - 100 + 8];
			}
		}
		break;
	case 'H':
	case 'f':
		app->cursor_y = p1 > 0 ? p1 - 1 : 0;
		app->cursor_x = p2 > 0 ? p2 - 1 : 0;
		if (app->cursor_y >= (int)app->rows) {
			app->cursor_y = (int)app->rows - 1;
		}
		if (app->cursor_x >= (int)app->cols) {
			app->cursor_x = (int)app->cols - 1;
		}
		break;
	case 'A':
		app->cursor_y -= (p1 > 0 ? p1 : 1);
		if (app->cursor_y < 0) {
			app->cursor_y = 0;
		}
		break;
	case 'B':
		app->cursor_y += (p1 > 0 ? p1 : 1);
		if (app->cursor_y >= (int)app->rows) {
			app->cursor_y = (int)app->rows - 1;
		}
		break;
	case 'C':
		app->cursor_x += (p1 > 0 ? p1 : 1);
		if (app->cursor_x >= (int)app->cols) {
			app->cursor_x = (int)app->cols - 1;
		}
		break;
	case 'D':
		app->cursor_x -= (p1 > 0 ? p1 : 1);
		if (app->cursor_x < 0) {
			app->cursor_x = 0;
		}
		break;
	case 'G':
		app->cursor_x = p1 > 0 ? p1 - 1 : 0;
		if (app->cursor_x >= (int)app->cols) {
			app->cursor_x = (int)app->cols - 1;
		}
		break;
	case 'd':
		app->cursor_y = p1 > 0 ? p1 - 1 : 0;
		if (app->cursor_y >= (int)app->rows) {
			app->cursor_y = (int)app->rows - 1;
		}
		break;
	case 'J':
		term_clear_screen(app, p1);
		break;
	case 'K':
		if (p1 == 0) {
			term_clear_line(app, app->cursor_y, app->cursor_x,
			    app->cols);
		} else if (p1 == 1) {
			term_clear_line(app, app->cursor_y, 0,
			    app->cursor_x + 1);
		} else {
			term_clear_line(app, app->cursor_y, 0, app->cols);
		}
		break;
	case 'L':
		p = p1 > 0 ? p1 : 1;
		while (p-- > 0) {
			term_scroll_down(app);
		}
		break;
	case 'M':
		p = p1 > 0 ? p1 : 1;
		while (p-- > 0) {
			term_scroll_up(app);
		}
		break;
	case 'P':
		p = p1 > 0 ? p1 : 1;
		if (app->cursor_x + p > (int)app->cols) {
			p = (int)app->cols - app->cursor_x;
		}
		memmove(&app->cells[app->cursor_y][app->cursor_x],
		    &app->cells[app->cursor_y][app->cursor_x + p],
		    sizeof(struct term_cell) * (app->cols - app->cursor_x - p));
		term_clear_line(app, app->cursor_y, app->cols - p, app->cols);
		break;
	case 's':
		app->saved_x = app->cursor_x;
		app->saved_y = app->cursor_y;
		break;
	case 'u':
		app->cursor_x = app->saved_x;
		app->cursor_y = app->saved_y;
		break;
	case 'h':
		if (app->esc_private && p1 == 25) {
			app->cursor_visible = 1;
		}
		break;
	case 'l':
		if (app->esc_private && p1 == 25) {
			app->cursor_visible = 0;
		}
		break;
	default:
		break;
	}
}

static void
term_process_byte(struct terminal_app *app, unsigned char b)
{
	if (app->esc_state == ESC_STATE_NONE) {
		if (b == 0x1b) {
			app->esc_state = ESC_STATE_ESC;
		} else {
			term_put_char(app, (char)b);
		}
	} else if (app->esc_state == ESC_STATE_ESC) {
		if (b == '[') {
			app->esc_state = ESC_STATE_CSI;
			app->esc_param_count = 0;
			app->esc_private = 0;
			memset(app->esc_params, 0, sizeof(app->esc_params));
		} else if (b == ']') {
			app->esc_state = ESC_STATE_OSC;
		} else {
			app->esc_state = ESC_STATE_NONE;
		}
	} else if (app->esc_state == ESC_STATE_CSI) {
		if (b == '?') {
			app->esc_private = 1;
		} else if (b >= '0' && b <= '9') {
			if (app->esc_param_count == 0) {
				app->esc_param_count = 1;
			}
			app->esc_params[app->esc_param_count - 1] =
			    app->esc_params[app->esc_param_count - 1] * 10 +
			    (b - '0');
		} else if (b == ';') {
			if (app->esc_param_count < TERM_MAX_PARAMS) {
				app->esc_param_count++;
			}
		} else if (b >= 0x40 && b <= 0x7e) {
			term_handle_csi(app, (char)b);
			app->esc_state = ESC_STATE_NONE;
		}
	} else if (app->esc_state == ESC_STATE_OSC) {
		if (b == 0x07 || b == 0x1b) {
			app->esc_state = ESC_STATE_NONE;
		}
	}
}

static void
term_process_output(struct terminal_app *app, const char *buf, size_t len)
{
	size_t	i;

	for (i = 0; i < len; i++) {
		term_process_byte(app, (unsigned char)buf[i]);
	}
	app->dirty = 1;
}

static size_t
term_translate_key(uint32_t key, uint32_t mods, char *out, size_t max_len)
{
	int	shift, caps, ctrl;
	char	ch;

	if (!out || max_len < 8) {
		return (0);
	}
	shift = (mods & MOD_SHIFT) != 0;
	caps = (mods & MOD_CAPS) != 0;
	ctrl = (mods & MOD_CTRL) != 0;

	if (key >= KEY_A && key <= KEY_Z) {
		ch = (char)('a' + (key - KEY_A));
		if (ctrl) {
			out[0] = (char)(ch - 'a' + 1);
			return (1);
		}
		if (shift ^ caps) {
			ch = (char)(ch - 32);
		}
		out[0] = ch;
		return (1);
	}

	switch (key) {
	case KEY_1: out[0] = shift ? '!' : '1'; return (1);
	case KEY_2:
		if (ctrl) {
			out[0] = 0x00;
			return (1);
		}
		out[0] = shift ? '@' : '2';
		return (1);
	case KEY_3: out[0] = shift ? '#' : '3'; return (1);
	case KEY_4: out[0] = shift ? '$' : '4'; return (1);
	case KEY_5: out[0] = shift ? '%' : '5'; return (1);
	case KEY_6:
		if (ctrl) {
			out[0] = 0x1e;
			return (1);
		}
		out[0] = shift ? '^' : '6';
		return (1);
	case KEY_7: out[0] = shift ? '&' : '7'; return (1);
	case KEY_8: out[0] = shift ? '*' : '8'; return (1);
	case KEY_9: out[0] = shift ? '(' : '9'; return (1);
	case KEY_0: out[0] = shift ? ')' : '0'; return (1);
	case KEY_ENTER:
	case KEY_KP_ENTER:
		out[0] = '\r';
		return (1);
	case KEY_ESC:
		out[0] = '\x1b';
		return (1);
	case KEY_BACKSPACE:
		out[0] = '\x7f';
		return (1);
	case KEY_TAB:
		out[0] = '\t';
		return (1);
	case KEY_SPACE:
		out[0] = ' ';
		return (1);
	case KEY_MINUS:
		if (ctrl) {
			out[0] = 0x1f;
			return (1);
		}
		out[0] = shift ? '_' : '-';
		return (1);
	case KEY_EQUAL: out[0] = shift ? '+' : '='; return (1);
	case KEY_LEFTBRACE:
		if (ctrl) {
			out[0] = 0x1b;
			return (1);
		}
		out[0] = shift ? '{' : '[';
		return (1);
	case KEY_RIGHTBRACE:
		if (ctrl) {
			out[0] = 0x1d;
			return (1);
		}
		out[0] = shift ? '}' : ']';
		return (1);
	case KEY_BACKSLASH:
		if (ctrl) {
			out[0] = 0x1c;
			return (1);
		}
		out[0] = shift ? '|' : '\\';
		return (1);
	case KEY_SEMICOLON: out[0] = shift ? ':' : ';'; return (1);
	case KEY_APOSTROPHE: out[0] = shift ? '"' : '\''; return (1);
	case KEY_GRAVE: out[0] = shift ? '~' : '`'; return (1);
	case KEY_COMMA: out[0] = shift ? '<' : ','; return (1);
	case KEY_DOT: out[0] = shift ? '>' : '.'; return (1);
	case KEY_SLASH:
		if (ctrl) {
			out[0] = 0x1f;
			return (1);
		}
		out[0] = shift ? '?' : '/';
		return (1);
	case KEY_UP:
		memcpy(out, "\033[A", 3);
		return (3);
	case KEY_DOWN:
		memcpy(out, "\033[B", 3);
		return (3);
	case KEY_RIGHT:
		memcpy(out, "\033[C", 3);
		return (3);
	case KEY_LEFT:
		memcpy(out, "\033[D", 3);
		return (3);
	case KEY_HOME:
		memcpy(out, "\033[H", 3);
		return (3);
	case KEY_END:
		memcpy(out, "\033[F", 3);
		return (3);
	case KEY_PAGEUP:
		memcpy(out, "\033[5~", 4);
		return (4);
	case KEY_PAGEDOWN:
		memcpy(out, "\033[6~", 4);
		return (4);
	case KEY_INSERT:
		memcpy(out, "\033[2~", 4);
		return (4);
	case KEY_DELETE:
		memcpy(out, "\033[3~", 4);
		return (4);
	case KEY_KP_0: out[0] = '0'; return (1);
	case KEY_KP_1: out[0] = '1'; return (1);
	case KEY_KP_2: out[0] = '2'; return (1);
	case KEY_KP_3: out[0] = '3'; return (1);
	case KEY_KP_4: out[0] = '4'; return (1);
	case KEY_KP_5: out[0] = '5'; return (1);
	case KEY_KP_6: out[0] = '6'; return (1);
	case KEY_KP_7: out[0] = '7'; return (1);
	case KEY_KP_8: out[0] = '8'; return (1);
	case KEY_KP_9: out[0] = '9'; return (1);
	case KEY_KP_DOT: out[0] = '.'; return (1);
	case KEY_KP_SLASH: out[0] = '/'; return (1);
	case KEY_KP_ASTERISK: out[0] = '*'; return (1);
	case KEY_KP_MINUS: out[0] = '-'; return (1);
	case KEY_KP_PLUS: out[0] = '+'; return (1);
	default:
		return (0);
	}
}

static void
term_render(struct terminal_app *app)
{
	char			str[2];
	libg_rect_t		rect, cursor_rect;
	struct term_cell	*cell;
	uint32_t		fg, bg;
	int32_t			px, py;
	uint32_t		r, c;

	rect.x = 0;
	rect.y = 0;
	rect.width = (int32_t)app->width;
	rect.height = (int32_t)app->height;
	libgFillRect(app->ui, rect, app->default_bg);

	str[1] = '\0';
	for (r = 0; r < app->rows; r++) {
		py = (int32_t)(TERM_PAD_Y + r * TERM_CELL_H);
		for (c = 0; c < app->cols; c++) {
			cell = &app->cells[r][c];
			px = (int32_t)(TERM_PAD_X + c * TERM_CELL_W);

			fg = cell->fg;
			bg = cell->bg;
			if ((cell->flags & TERM_FLAG_INVERSE) != 0) {
				fg = cell->bg;
				bg = cell->fg;
			}

			if (bg != app->default_bg) {
				rect.x = px;
				rect.y = py;
				rect.width = TERM_CELL_W;
				rect.height = TERM_CELL_H;
				libgFillRect(app->ui, rect, bg);
			}

			if (cell->ch > ' ' && cell->ch < 127) {
				str[0] = (char)cell->ch;
				libgText(app->ui, px + 1, py + 4, str, fg);
			}

			if ((cell->flags & TERM_FLAG_UNDERLINE) != 0) {
				libgLine(app->ui, px, py + TERM_CELL_H - 2,
				    px + TERM_CELL_W - 1, py + TERM_CELL_H - 2,
				    fg);
			}
		}
	}

	if (app->cursor_visible && app->cursor_blink_state &&
	    app->cursor_y >= 0 && app->cursor_y < (int)app->rows &&
	    app->cursor_x >= 0 && app->cursor_x < (int)app->cols) {
		cursor_rect.x = (int32_t)(TERM_PAD_X +
		    app->cursor_x * TERM_CELL_W);
		cursor_rect.y = (int32_t)(TERM_PAD_Y +
		    app->cursor_y * TERM_CELL_H);
		cursor_rect.width = TERM_CELL_W;
		cursor_rect.height = TERM_CELL_H;

		if (app->focused) {
			libgFillRect(app->ui, (libg_rect_t){
			    cursor_rect.x,
			    cursor_rect.y + TERM_CELL_H - 3,
			    TERM_CELL_W, 2 }, app->default_fg);
		} else {
			libgStrokeRect(app->ui, cursor_rect,
			    app->palette[8]);
		}
	}

	(void)libgPresent(app->ui);
}

static int
term_handle_event(struct terminal_app *app, const sprot_event_t *event)
{
	char	key_buf[16];
	size_t	len;

	if (event->kind == SPROT_EVENT_SURFACE_CLOSE) {
		app->running = 0;
		return (0);
	}
	if (event->kind == SPROT_EVENT_SURFACE_CONFIGURE) {
		app->focused = (event->u.configure.state &
		    SPROT_SURFACE_STATE_FOCUSED) != 0;
		app->dirty = 1;
		return (1);
	}
	if (event->kind == SPROT_EVENT_KEY &&
	    event->u.key.state == SPROT_KEY_STATE_PRESSED) {
		len = term_translate_key(event->u.key.scancode,
		    event->u.key.modifiers, key_buf, sizeof(key_buf));
		if (len > 0 && app->master_fd >= 0) {
			(void)dataWrite(app->master_fd, key_buf, len);
		}
		return (1);
	}
	return (0);
}

static void
term_cleanup(struct terminal_app *app)
{
	if (app->child_pid > 0) {
		(void)procKill((uint32_t)app->child_pid, 9);
	}
	if (app->master_fd >= 0) {
		dataClose(app->master_fd);
		app->master_fd = -1;
	}
	if (app->ui != NULL) {
		libgDestroy(app->ui);
		app->ui = NULL;
	}
	if (app->surface != NULL) {
		sprot_destroy_surface(app->surface);
		app->surface = NULL;
	}
	if (app->conn != NULL) {
		sprot_disconnect(app->conn);
		app->conn = NULL;
	}
}

int
main(void)
{
	struct terminal_app	app;
	struct api_timeinfo	ti;
	struct api_winsize	ws;
	sprot_event_t		event;
	libg_style_t		style;
	const char		*argv[2];
	char			read_buf[TERM_READ_BUF_SIZE];
	uint64_t		now_ms;
	ssize_t			nread;
	int			ret, avail;

	memset(&app, 0, sizeof(app));
	app.cols = TERM_COLS;
	app.rows = TERM_ROWS;
	app.width = TERM_COLS * TERM_CELL_W + 2 * TERM_PAD_X;
	app.height = TERM_ROWS * TERM_CELL_H + 2 * TERM_PAD_Y;
	app.master_fd = -1;
	app.cursor_visible = 1;
	app.cursor_blink_state = 1;
	app.focused = 1;
	app.running = 1;

	term_init_colors(&app);
	term_clear_screen(&app, 2);

	ret = ptyOpen(&app.master_fd, &app.pts_id);
	if (ret != 0 || app.master_fd < 0) {
		termPrint("term: failed to open pty\n");
		return (1);
	}

	ws.ws_col = (uint16_t)app.cols;
	ws.ws_row = (uint16_t)app.rows;
	ws.ws_xpixel = (uint16_t)app.width;
	ws.ws_ypixel = (uint16_t)app.height;
	(void)entityIoctl(app.master_fd, API_TIOCSWINSZ, &ws);

	argv[0] = "sh";
	argv[1] = NULL;
	app.child_pid = procSpawnPty("/bin/sh", (char *const *)argv,
	    NULL, app.pts_id, API_PROC_SPAWN_ABI_NATIVE);
	if (app.child_pid < 0) {
		termPrint("term: failed to spawn shell\n");
		term_cleanup(&app);
		return (1);
	}

	app.conn = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (app.conn == NULL) {
		termPrint("term: cannot connect to Sprot\n");
		term_cleanup(&app);
		return (1);
	}

	app.surface = sprot_create_surface(app.conn, app.width, app.height);
	if (app.surface == NULL ||
	    term_wait_surface(app.conn, app.surface) != 0) {
		termPrint("term: cannot create surface\n");
		term_cleanup(&app);
		return (1);
	}

	if (sprot_set_role(app.surface, SPROT_SURFACE_ROLE_TOPLEVEL, 0,
	    60, 60) != 0 ||
	    sprot_set_title(app.surface, "Terminal") != 0 ||
	    sprot_set_visible(app.surface, 1) != 0) {
		termPrint("term: failed to configure window surface\n");
		term_cleanup(&app);
		return (1);
	}

	libgDefaultStyle(&style);
	style.background = app.default_bg;
	ret = libgCreateForTarget(sprot_surface_pixels(app.surface),
	    app.width, app.height, sprot_surface_stride(app.surface),
	    term_present, app.surface, &style, &app.ui);
	if (ret != LIBG_OK || app.ui == NULL) {
		termPrint("term: LibG initialization failed\n");
		term_cleanup(&app);
		return (1);
	}

	app.dirty = 1;

	while (app.running) {
		while (sprot_poll_event(app.conn, &event, 0) > 0) {
			(void)term_handle_event(&app, &event);
		}

		if (app.master_fd >= 0) {
			avail = 0;
			ret = entityIoctl(app.master_fd, API_FIONREAD, &avail);
			if (ret == 0 && avail > 0) {
				nread = dataRead(app.master_fd, read_buf,
				    (size_t)avail > sizeof(read_buf) ?
				    sizeof(read_buf) : (size_t)avail);
				if (nread > 0) {
					term_process_output(&app, read_buf,
					    (size_t)nread);
				} else if (nread == 0) {
					app.running = 0;
					break;
				}
			}
		}

		if (sysTimeInfo(&ti) == 0) {
			now_ms = ti.uptime_sec * 1000 +
			    ti.uptime_nsec / 1000000;
			if (now_ms - app.last_blink_time >= TERM_BLINK_INTERVAL) {
				app.cursor_blink_state = !app.cursor_blink_state;
				app.last_blink_time = now_ms;
				app.dirty = 1;
			}
		}

		if (app.dirty) {
			term_render(&app);
			app.dirty = 0;
		}

		ret = sprot_poll_event(app.conn, &event, 16);
		if (ret > 0) {
			(void)term_handle_event(&app, &event);
		}
	}

	term_cleanup(&app);
	return (0);
}
