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
$define %type uint64_t as 64 bit unsigned
$define %type int32_t as 32 bit signed
$define %type size_t as native object size
$define %type sprot_event as one compositor event
$define %type libg_rect as integer rectangle
$define %type rg_focus as which list pane owns the keyboard selection
$define %type rg_dialog as modal dialog state with edit buffer
$define %type rg_crumb as one clickable breadcrumb segment
$define %type rg_layout as per frame pixel geometry of every hit region
$define %type rg_state as regedit gui global state

$define %func rg_layout_compute as procedure with args rg_state *, rg_layout *
$define %func rg_button_label as function with args int
$define %func rg_rect_hit as function with args libg_rect, int32_t, int32_t
$define %func rg_row_count as function with args rg_state *, int
$define %func rg_row_rect as function with args libg_rect, int
$define %func rg_crumb_last as function with args rg_state *
$define %func rg_hot_update as function with args rg_state *, rg_layout *
$define %func rg_draw_hot as procedure with args rg_state *
$define %func rg_window_open as function with args rg_state *, const char *
$define %func rg_window_close as procedure with args rg_state *
$define %func rg_window_resize as function with args rg_state *, uint32_t, uint32_t
$define %func rg_window_present as function with args rg_state *
$define %func rg_now_ms as function with args void
$define %func rg_reload as procedure with args rg_state *
$define %func rg_status as procedure with args rg_state *, const char *
$define %func rg_status_fmt as procedure with args rg_state *, fmt, args
$define %func rg_status_error as procedure with args rg_state *, what, code
$define %func rg_clamp_view as procedure with args rg_state *, rg_layout *
$define %func rg_draw as procedure with args rg_state *
$define %func rg_dispatch as procedure with args rg_state *, sprot_event *
$define %func rg_key_char as function with args uint32_t, uint32_t
$define %func rg_choice_count as function with args int
$define %func rg_choice_item as function with args int, int
$define %func rg_choice_hint as function with args int, int
$define %func rg_dialog_open as procedure with args rg_state *, action
$define %func rg_dialog_key as procedure with args rg_state *, key, mods
$define %func rg_dialog_click as procedure with args rg_state *, layout, x, y
$define %func rg_dialog_accept as procedure with args rg_state *
$define %func rg_dialog_cancel as procedure with args rg_state *
$define %func rg_act_open as procedure with args rg_state *
$define %func rg_act_leave as procedure with args rg_state *
$define %func rg_act_activate as procedure with args rg_state *
$define %func rg_act_goto_path as procedure with args rg_state *, const char *
$define %func rg_act_delete as procedure with args rg_state *
$define %func rg_act_commit_delete as procedure with args rg_state *
$define %func rg_act_write as function with args rg_state *, name, type, text
$define %func rg_act_new_key as function with args rg_state *, const char *
$define %func rg_act_notify as function with args rg_state *, const char *

*/

/* !SPACE!

$space %export rg_focus_t, rg_dialog_t, rg_crumb_t, rg_layout_t, rg_state_t
$space %export rg_layout_compute, rg_button_label, rg_rect_hit, rg_row_count
$space %export rg_row_rect, rg_crumb_last, rg_hot_update, rg_draw_hot
$space %export rg_window_open, rg_window_close, rg_window_resize
$space %export rg_window_present, rg_now_ms
$space %export rg_reload, rg_status, rg_status_fmt, rg_status_error
$space %export rg_clamp_view, rg_draw, rg_dispatch, rg_key_char
$space %export rg_choice_count, rg_choice_item, rg_choice_hint
$space %export rg_dialog_open, rg_dialog_key, rg_dialog_click
$space %export rg_dialog_accept, rg_dialog_cancel
$space %export rg_act_open, rg_act_leave, rg_act_activate, rg_act_goto_path
$space %export rg_act_delete, rg_act_commit_delete
$space %export rg_act_write, rg_act_new_key, rg_act_notify

*/

#ifndef PROGS_REGEDIT_GUI_H
#define PROGS_REGEDIT_GUI_H

#include <font.h>
#include <libg.h>
#include <regedit/regedit.h>
#include <sprot/client.h>
#include <sprot/sprot.h>
#include <stddef.h>
#include <stdint.h>
#define RG_TEXT_SCALE		2
#define RG_GLYPH_W		(LIBG_FONT_ADVANCE * RG_TEXT_SCALE)
#define RG_GLYPH_H		(LIBG_FONT_HEIGHT * RG_TEXT_SCALE)
#define RG_ROW_H		(RG_GLYPH_H + 8)
#define RG_PAD			8
#define RG_HEADER_H		(RG_GLYPH_H + 14)
#define RG_CRUMB_H		(RG_GLYPH_H + 12)
#define RG_TOOLBAR_H		(RG_GLYPH_H + 16)
#define RG_COLUMN_H		(RG_GLYPH_H + 10)
#define RG_STATUS_H		(RG_GLYPH_H + 12)
#define RG_SCROLL_W		10
#define RG_PANE_SHARE		36
#define RG_PANE_MIN		160
#define RG_DEFAULT_W		960
#define RG_DEFAULT_H		640
#define RG_MIN_W		560
#define RG_MIN_H		360
#define RG_IDLE_MS		120
#define RG_DOUBLE_MS		400
#define RG_BURST_MAX		32
#define RG_STATUS_MAX		192
#define RG_LABEL_MAX		128
#define RG_CRUMB_MAX		12
#define RG_TITLE		"Registry Editor"
#define RG_BTN_NONE		(-1)
#define RG_BTN_UP		0
#define RG_BTN_NEW_KEY		1
#define RG_BTN_NEW_VALUE	2
#define RG_BTN_DELETE		3
#define RG_BTN_GOTO		4
#define RG_BTN_NOTIFY		5
#define RG_BTN_RELOAD		6
#define RG_BTN_HELP		7
#define RG_BTN_COUNT		8
#define RG_DLG_NONE		0
#define RG_DLG_TEXT		1
#define RG_DLG_CHOICE		2
#define RG_DLG_CONFIRM		3
#define RG_ACT_NONE		0
#define RG_ACT_EDIT_VALUE	1
#define RG_ACT_NEW_VALUE	2
#define RG_ACT_NEW_KEY		3
#define RG_ACT_GOTO		4
#define RG_ACT_NOTIFY		5
#define RG_ACT_DELETE_KEY	6
#define RG_ACT_DELETE_VALUE	7
#define RG_CHOICE_TYPE		0
#define RG_CHOICE_CONSUMER	1
#define RG_DLG_HIT_NONE		0
#define RG_DLG_HIT_OK		1
#define RG_DLG_HIT_CANCEL	2
#define RG_HOT_NONE		0
#define RG_HOT_BUTTON		1
#define RG_HOT_CRUMB		2
#define RG_HOT_KEY_ROW		3
#define RG_HOT_VALUE_ROW	4
#define RG_HOT_CHOICE		5
#define RG_HOT_DLG_OK		6
#define RG_HOT_DLG_CANCEL	7

typedef enum rg_focus {
	RG_FOCUS_KEYS = 0,
	RG_FOCUS_VALUES = 1
} rg_focus_t;

typedef struct rg_dialog {
	uint32_t	type;
	int		kind;
	int		action;
	int		stage;
	int		choice_kind;
	int		choice;
	int		caret;
	int		view;
	int		limit;
	char		title[RG_LABEL_MAX];
	char		hint[RG_LABEL_MAX];
	char		name[RE_NAME_MAX];
	char		text[RE_TEXT_MAX];
} rg_dialog_t;

typedef struct rg_crumb {
	libg_rect_t	rect;
	char		label[RE_NAME_MAX];
	char		target[RE_PATH_MAX];
} rg_crumb_t;

typedef struct rg_layout {
	libg_rect_t	header;
	libg_rect_t	crumbs;
	libg_rect_t	toolbar;
	libg_rect_t	columns;
	libg_rect_t	keys;
	libg_rect_t	keys_bar;
	libg_rect_t	values;
	libg_rect_t	values_bar;
	libg_rect_t	status;
	libg_rect_t	buttons[RG_BTN_COUNT];
	libg_rect_t	dlg_frame;
	libg_rect_t	dlg_field;
	libg_rect_t	dlg_ok;
	libg_rect_t	dlg_cancel;
	int		rows;
	int		name_w;
	int		type_w;
	int		data_w;
} rg_layout_t;

typedef struct rg_state {
	re_path_t		path;
	re_hives_t		hives;
	re_keys_t		keys;
	re_values_t		values;
	rg_dialog_t		dialog;
	sprot_connection_t	*conn;
	sprot_surface_t		*surface;
	libg_context_t		*ui;
	libg_style_t		style;
	uint64_t		click_ms;
	uint32_t		width;
	uint32_t		height;
	uint32_t		buttons;
	int32_t			mouse_x;
	int32_t			mouse_y;
	rg_focus_t		focus;
	int			key_sel;
	int			key_off;
	int			value_sel;
	int			value_off;
	int			click_pane;
	int			click_row;
	int			running;
	int			dirty;
	int			hot_only;
	int			hot_zone;
	int			hot_index;
	int			prev_hot_zone;
	int			prev_hot_index;
	int			loaded;
	int			focused;
	int			fullscreen;
	int			help;
	int			crumb_count;
	rg_crumb_t		crumbs[RG_CRUMB_MAX];
	char			status[RG_STATUS_MAX];
} rg_state_t;

void		rg_layout_compute(rg_state_t *st, rg_layout_t *out);
const char	*rg_button_label(int id);
int		rg_rect_hit(libg_rect_t rect, int32_t x, int32_t y);
int	rg_row_count(const rg_state_t *st, int pane);
libg_rect_t	rg_row_rect(libg_rect_t pane, int slot);
int		rg_crumb_last(const rg_state_t *st);
int		rg_hot_update(rg_state_t *st, const rg_layout_t *lay);

int	rg_window_open(rg_state_t *st, const char *title);
void	rg_window_close(rg_state_t *st);
int	rg_window_resize(rg_state_t *st, uint32_t width, uint32_t height);
int	rg_window_present(rg_state_t *st);
uint64_t	rg_now_ms(void);

void	rg_reload(rg_state_t *st);
void	rg_status(rg_state_t *st, const char *text);
void	rg_status_fmt(rg_state_t *st, const char *fmt, ...);
void	rg_status_error(rg_state_t *st, const char *what, int code);
void	rg_clamp_view(rg_state_t *st, const rg_layout_t *lay);

void	rg_draw(rg_state_t *st);
void	rg_draw_hot(rg_state_t *st);
void	rg_dispatch(rg_state_t *st, const sprot_event_t *ev);
uint32_t	rg_key_char(uint32_t key, uint32_t mods);
int		rg_choice_count(int kind);
const char	*rg_choice_item(int kind, int index);
const char	*rg_choice_hint(int kind, int index);
void	rg_dialog_open(rg_state_t *st, int action);
void	rg_dialog_key(rg_state_t *st, uint32_t key, uint32_t mods);
void	rg_dialog_click(rg_state_t *st, const rg_layout_t *lay, int32_t x,
	    int32_t y);
void	rg_dialog_accept(rg_state_t *st);
void	rg_dialog_cancel(rg_state_t *st);
void	rg_act_open(rg_state_t *st);
void	rg_act_leave(rg_state_t *st);
void	rg_act_activate(rg_state_t *st);
void	rg_act_goto_path(rg_state_t *st, const char *text);
void	rg_act_delete(rg_state_t *st);
void	rg_act_commit_delete(rg_state_t *st);
int	rg_act_write(rg_state_t *st, const char *name, uint32_t type,
	    const char *text);
int	rg_act_new_key(rg_state_t *st, const char *name);
int	rg_act_notify(rg_state_t *st, const char *name);

#endif
