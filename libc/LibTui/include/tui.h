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

/*
 * LibTui -- text user interface library.
 *
 * The model is bsddialog's, the one bsdinstall is built out of: the terminal
 * holds a painted backdrop, and everything the user looks at is a bordered
 * window drawn on top of it, with its title in the top border and a row of
 * buttons along the bottom. A screen with no border and no visible buttons
 * gives the user nothing to aim at, which is the whole reason the frame is not
 * optional here.
 *
 * Two shapes are built from the same primitives:
 *
 *   - a dialog: a centred window, sized to its text, that runs its own key loop
 *     and returns which button was pressed. tui_msgbox() and friends.
 *
 *   - a screen: the full terminal with a title bar, a status bar and a body the
 *     caller divides into bordered panes. This is regedit's layout, and what a
 *     multi-section program like an installer lives in.
 *
 * Every function that draws leaves the output buffered; nothing reaches the
 * terminal until tui_flush(). One flush per frame is what stops a redraw from
 * being visible as it happens.
 */

/* !DEFINES!

$define %type size_t as native object size
$define %type uint32_t as 32 bit unsigned
$define %type tui_rect_t as struct with row, col, width, height
$define %type tui_key_t as struct with code, character, modifiers
$define %type tui_theme_t as struct with the colour of every drawn element
$define %type tui_win_t as struct with a bordered window frame and its body
$define %type tui_buttons_t as struct with a row of labels and the focused one
$define %type tui_list_t as struct with a scrolling selection list
$define %type tui_row_fn as callback drawing one list row
$define %type tui_edit_t as struct with a single line text field
$define %type tui_item_t as struct with a label, detail and selected flag
$define %type tui_gauge_t as struct with a progress window and its total

$define %func tui_init as function with args void
$define %func tui_fini as procedure with args void
$define %func tui_rows as function with args void
$define %func tui_cols as function with args void
$define %func tui_resize as function with args void
$define %func tui_theme as function with args void
$define %func tui_theme_set as procedure with args const tui_theme_t *
$define %func tui_flush as procedure with args void
$define %func tui_backdrop as procedure with args const char *
$define %func tui_move as procedure with args int, int
$define %func tui_attr as procedure with args int, int
$define %func tui_attr_reset as procedure with args void
$define %func tui_put as procedure with args const char *
$define %func tui_put_n as procedure with args const char *, int
$define %func tui_putf as procedure with args const char *, args
$define %func tui_pad as procedure with args char, int
$define %func tui_field as procedure with args const char *, int
$define %func tui_field_right as procedure with args const char *, int
$define %func tui_cursor as procedure with args int
$define %func tui_fill as procedure with args const tui_rect_t *, int
$define %func tui_hline as procedure with args int, int, int
$define %func tui_border as procedure with args const tui_rect_t *, const char *, int
$define %func tui_text_rows as function with args const char *, int
$define %func tui_text_draw as function with args const tui_rect_t *, const char *, int
$define %func tui_center as procedure with args tui_rect_t *, int, int
$define %func tui_win_open as procedure with args tui_win_t *, const char *, int, int
$define %func tui_win_at as procedure with args tui_win_t *, const char *, const tui_rect_t *
$define %func tui_win_draw as procedure with args tui_win_t *
$define %func tui_win_body as procedure with args const tui_win_t *, tui_rect_t *
$define %func tui_buttons_init as procedure with args tui_buttons_t *, const char *const *, int, int
$define %func tui_buttons_draw as procedure with args const tui_buttons_t *, const tui_win_t *, int
$define %func tui_buttons_key as function with args tui_buttons_t *, const tui_key_t *
$define %func tui_list_init as procedure with args tui_list_t *, const tui_rect_t *, int
$define %func tui_list_draw as procedure with args tui_list_t *, tui_row_fn, void *, int
$define %func tui_list_key as function with args tui_list_t *, const tui_key_t *
$define %func tui_list_select as procedure with args tui_list_t *, int
$define %func tui_list_count_set as procedure with args tui_list_t *, int
$define %func tui_edit_init as procedure with args tui_edit_t *, char *, size_t
$define %func tui_edit_draw as procedure with args const tui_edit_t *, int, int, int
$define %func tui_edit_key as function with args tui_edit_t *, const tui_key_t *
$define %func tui_key_read as procedure with args tui_key_t *
$define %func tui_key_poll as function with args tui_key_t *
$define %func tui_msgbox as procedure with args const char *, const char *
$define %func tui_yesno as function with args const char *, const char *, int
$define %func tui_buttonbox as function with args const char *, const char *, const char *const *, int, int
$define %func tui_inputbox as function with args const char *, const char *, char *, size_t
$define %func tui_menu as function with args const char *, const char *, const tui_item_t *, int, int *
$define %func tui_checklist as function with args const char *, const char *, tui_item_t *, int
$define %func tui_textbox as procedure with args const char *, const char *
$define %func tui_gauge_open as procedure with args tui_gauge_t *, const char *, unsigned long
$define %func tui_gauge_step as procedure with args tui_gauge_t *, unsigned long, const char *
$define %func tui_gauge_close as procedure with args tui_gauge_t *
$define %func tui_screen_begin as procedure with args const char *, const char *
$define %func tui_screen_body as procedure with args tui_rect_t *

*/

/* !SPACE!

$space %export tui_rect_t, tui_key_t, tui_theme_t, tui_win_t
$space %export tui_buttons_t, tui_list_t, tui_row_fn, tui_edit_t
$space %export tui_item_t, tui_gauge_t
$space %export tui_init, tui_fini, tui_rows, tui_cols, tui_resize
$space %export tui_theme, tui_theme_set, tui_flush, tui_backdrop
$space %export tui_move, tui_attr, tui_attr_reset
$space %export tui_put, tui_put_n, tui_putf, tui_pad
$space %export tui_field, tui_field_right, tui_cursor
$space %export tui_fill, tui_hline, tui_border
$space %export tui_text_rows, tui_text_draw, tui_center
$space %export tui_win_open, tui_win_at, tui_win_draw, tui_win_body
$space %export tui_buttons_init, tui_buttons_draw, tui_buttons_key
$space %export tui_list_init, tui_list_draw, tui_list_key
$space %export tui_list_select, tui_list_count_set
$space %export tui_edit_init, tui_edit_draw, tui_edit_key
$space %export tui_key_read, tui_key_poll
$space %export tui_msgbox, tui_yesno, tui_buttonbox, tui_inputbox
$space %export tui_menu, tui_checklist, tui_textbox
$space %export tui_gauge_open, tui_gauge_step, tui_gauge_close
$space %export tui_screen_begin, tui_screen_body

*/

#ifndef LIBTUI_TUI_H
#define LIBTUI_TUI_H

#include <stddef.h>
#include <stdint.h>

#define TUI_KEY_A		0x0004
#define TUI_KEY_B		0x0005
#define TUI_KEY_C		0x0006
#define TUI_KEY_D		0x0007
#define TUI_KEY_E		0x0008
#define TUI_KEY_F		0x0009
#define TUI_KEY_G		0x000a
#define TUI_KEY_H		0x000b
#define TUI_KEY_I		0x000c
#define TUI_KEY_J		0x000d
#define TUI_KEY_K		0x000e
#define TUI_KEY_L		0x000f
#define TUI_KEY_M		0x0010
#define TUI_KEY_N		0x0011
#define TUI_KEY_O		0x0012
#define TUI_KEY_P		0x0013
#define TUI_KEY_Q		0x0014
#define TUI_KEY_R		0x0015
#define TUI_KEY_S		0x0016
#define TUI_KEY_T		0x0017
#define TUI_KEY_U		0x0018
#define TUI_KEY_V		0x0019
#define TUI_KEY_W		0x001a
#define TUI_KEY_X		0x001b
#define TUI_KEY_Y		0x001c
#define TUI_KEY_Z		0x001d
#define TUI_KEY_ENTER		0x0028
#define TUI_KEY_ESC		0x0029
#define TUI_KEY_BACKSPACE	0x002a
#define TUI_KEY_TAB		0x002b
#define TUI_KEY_SPACE		0x002c
#define TUI_KEY_F1		0x003a
#define TUI_KEY_F2		0x003b
#define TUI_KEY_F3		0x003c
#define TUI_KEY_F4		0x003d
#define TUI_KEY_F5		0x003e
#define TUI_KEY_F6		0x003f
#define TUI_KEY_F7		0x0040
#define TUI_KEY_F8		0x0041
#define TUI_KEY_F9		0x0042
#define TUI_KEY_F10		0x0043
#define TUI_KEY_HOME		0x004a
#define TUI_KEY_PAGEUP		0x004b
#define TUI_KEY_DELETE		0x004c
#define TUI_KEY_END		0x004d
#define TUI_KEY_PAGEDOWN	0x004e
#define TUI_KEY_RIGHT		0x004f
#define TUI_KEY_LEFT		0x0050
#define TUI_KEY_DOWN		0x0051
#define TUI_KEY_UP		0x0052
#define TUI_KEY_KP_ENTER	0x0058
#define TUI_EVENT_PRESS		0x00000001
#define TUI_MOD_SHIFT		0x00000003
#define TUI_MOD_CTRL		0x0000000c
#define TUI_MOD_ALT		0x00000030
#define TUI_FG_BLACK		30
#define TUI_FG_RED		31
#define TUI_FG_GREEN		32
#define TUI_FG_YELLOW		33
#define TUI_FG_BLUE		34
#define TUI_FG_MAGENTA		35
#define TUI_FG_CYAN		36
#define TUI_FG_WHITE		37
#define TUI_FG_GREY		90
#define TUI_FG_BRIGHT		97
#define TUI_BG_BLACK		40
#define TUI_BG_RED		41
#define TUI_BG_GREEN		42
#define TUI_BG_YELLOW		43
#define TUI_BG_BLUE		44
#define TUI_BG_MAGENTA		45
#define TUI_BG_CYAN		46
#define TUI_BG_WHITE		47
#define TUI_PLAIN		0
#define TUI_BOLD		1
#define TUI_ROWS_MIN		24
#define TUI_COLS_MIN		80
#define TUI_TEXT_MAX		1024
#define TUI_LABEL_MAX		96
#define TUI_BUTTONS_MAX		6
#define TUI_OK			0
#define TUI_CANCEL		1
#define TUI_ESC			(-1)

typedef struct tui_rect {
	int	row;
	int	col;
	int	width;
	int	height;
} tui_rect_t;

typedef struct tui_key {
	uint32_t	mods;
	uint32_t	ch;
	int		code;
} tui_key_t;


typedef struct tui_theme {
	int	backdrop_fg;
	int	backdrop_bg;
	int	title_fg;
	int	title_bg;
	int	win_fg;
	int	win_bg;
	int	border_fg;
	int	border_focus_fg;
	int	text_fg;
	int	dim_fg;
	int	sel_fg;
	int	sel_bg;
	int	sel_idle_fg;
	int	sel_idle_bg;
	int	button_fg;
	int	button_bg;
	int	button_focus_fg;
	int	button_focus_bg;
	int	field_fg;
	int	field_bg;
	int	status_fg;
	int	status_bg;
	int	gauge_fg;
	int	gauge_bg;
} tui_theme_t;

typedef struct tui_win {
	tui_rect_t	frame;
	tui_rect_t	body;
	char		title[TUI_LABEL_MAX];
} tui_win_t;

typedef struct tui_buttons {
	const char *const	*labels;
	int			count;
	int			focus;
} tui_buttons_t;


typedef void	(*tui_row_fn)(int index, int width, int selected, void *ctx);

typedef struct tui_list {
	tui_rect_t	rect;
	int		count;
	int		sel;
	int		off;
} tui_list_t;

typedef struct tui_edit {
	char	*buf;
	size_t	size;
	size_t	len;
	size_t	caret;
} tui_edit_t;

typedef struct tui_item {
	const char	*label;
	const char	*detail;
	int		selected;
} tui_item_t;

typedef struct tui_gauge {
	tui_win_t	win;
	unsigned long	total;
	unsigned long	done;
	char		label[TUI_LABEL_MAX];
} tui_gauge_t;

int	tui_init(void);
void	tui_fini(void);
int	tui_rows(void);
int	tui_cols(void);
int	tui_resize(void);

const tui_theme_t	*tui_theme(void);
void			tui_theme_set(const tui_theme_t *theme);

void	tui_flush(void);
void	tui_backdrop(const char *title);
void	tui_move(int row, int col);
void	tui_attr(int fg, int bg);
void	tui_attr_bold(int fg, int bg);
void	tui_attr_reset(void);
void	tui_put(const char *text);
void	tui_put_n(const char *text, int len);
void	tui_putf(const char *fmt, ...);
void	tui_pad(char fill, int count);
void	tui_field(const char *text, int width);
void	tui_field_right(const char *text, int width);
void	tui_cursor(int visible);

void	tui_fill(const tui_rect_t *rect, int fg, int bg);
void	tui_hline(int row, int col, int width);
void	tui_border(const tui_rect_t *rect, const char *title, int focused);
int	tui_text_rows(const char *text, int width);
int	tui_text_draw(const tui_rect_t *rect, const char *text, int skip);
void	tui_center(tui_rect_t *rect, int width, int height);

void	tui_win_open(tui_win_t *win, const char *title, int width, int height);
void	tui_win_at(tui_win_t *win, const char *title, const tui_rect_t *frame);
void	tui_win_draw(tui_win_t *win);

void	tui_buttons_init(tui_buttons_t *row, const char *const *labels,
	    int count, int focus);
void	tui_buttons_draw(const tui_buttons_t *row, const tui_win_t *win,
	    int focused);
int	tui_buttons_key(tui_buttons_t *row, const tui_key_t *key);

void	tui_list_init(tui_list_t *list, const tui_rect_t *rect, int count);
void	tui_list_draw(tui_list_t *list, tui_row_fn row, void *ctx, int focused);
int	tui_list_key(tui_list_t *list, const tui_key_t *key);
void	tui_list_select(tui_list_t *list, int index);
void	tui_list_count_set(tui_list_t *list, int count);

void	tui_edit_init(tui_edit_t *ed, char *buf, size_t size);
void	tui_edit_draw(const tui_edit_t *ed, int row, int col, int width);
int	tui_edit_key(tui_edit_t *ed, const tui_key_t *key);

void	tui_key_read(tui_key_t *key);
int	tui_key_poll(tui_key_t *key);

void	tui_msgbox(const char *title, const char *text);
int	tui_yesno(const char *title, const char *text, int def_yes);
int	tui_buttonbox(const char *title, const char *text,
	    const char *const *labels, int count, int initial);
int	tui_inputbox(const char *title, const char *label, char *out,
	    size_t size);
int	tui_menu(const char *title, const char *prompt, const tui_item_t *items,
	    int count, int *sel);
int	tui_checklist(const char *title, const char *prompt, tui_item_t *items,
	    int count);
void	tui_textbox(const char *title, const char *text);

void	tui_gauge_open(tui_gauge_t *g, const char *title, unsigned long total);
void	tui_gauge_step(tui_gauge_t *g, unsigned long done, const char *label);
void	tui_gauge_close(tui_gauge_t *g);

void	tui_screen_begin(const char *title, const char *status);
void	tui_screen_body(tui_rect_t *rect);

#endif
