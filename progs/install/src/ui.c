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

$define %type inst_win_t as the window's own navigation state

$define %func inst_ui_wrap as function with args const char *, int, int, char *, size_t
$define %func inst_ui_error as procedure with args inst_ctx_t *, const char *
$define %func inst_welcome as function with args void
$define %func inst_ui_run as function with args inst_ctx_t *
$define %func inst_finish as function with args inst_ctx_t *

$define %func inst_nav_width as function with args void
$define %func inst_win_layout as procedure with args tui_win_t *, tui_win_t *
$define %func inst_nav_row as procedure with args int, int, int, void *
$define %func inst_button_draw as procedure with args inst_win_t *, const tui_win_t *
$define %func inst_win_draw as procedure with args inst_win_t *
$define %func inst_win_enter as function with args inst_win_t *, int
$define %func inst_win_next as function with args inst_win_t *

*/

/* !SPACE!

$space %internal inst_win_t, inst_nav_width, inst_win_layout, inst_nav_row
$space %internal inst_button_draw, inst_win_draw, inst_win_enter, inst_win_next
$space %export inst_ui_wrap, inst_ui_error, inst_welcome
$space %export inst_ui_run, inst_finish

*/

#include <native.h>
#include <stdio.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "ui.h"


#define INST_NAV_NUM		30
#define INST_NAV_DEN		100
#define INST_NAV_MIN		24
#define INST_NAV_MAX		36
#define INST_NAV_SUMMARY	30

#define INST_TITLE		"otsos installer"

#define INST_FOCUS_NAV		0
#define INST_FOCUS_PANE		1

typedef struct inst_win {
	tui_win_t		nav_win;
	tui_win_t		pane_win;
	tui_list_t		nav;
	const inst_section_t	*sec;
	inst_ctx_t		*ctx;
	int			count;
	int			focus;
	int			cur;
} inst_win_t;

int
inst_ui_wrap(const char *text, int width, int line, char *out, size_t size)
{
	size_t		start;
	size_t		len;
	size_t		take;
	size_t		brk;
	size_t		i;
	int		at;

	if (out == NULL || size == 0) {
		return (0);
	}
	out[0] = '\0';
	if (text == NULL || width <= 0 || line < 0) {
		return (0);
	}

	len = strlen(text);
	start = 0;
	at = 0;
	while (start <= len) {
		while (start < len && text[start] == ' ') {
			start++;
		}
		if (start >= len) {
			return (0);
		}

		take = len - start;
		if (take > (size_t)width) {

			brk = 0;
			for (i = 0; i <= (size_t)width; i++) {
				if (text[start + i] == ' ') {
					brk = i;
				}
			}

			take = (brk > 0) ? brk : (size_t)width;
		}

		if (at == line) {
			if (take >= size) {
				take = size - 1;
			}
			memcpy(out, text + start, take);
			out[take] = '\0';
			return (1);
		}
		at++;
		start += take;
	}
	return (0);
}

void
inst_ui_error(inst_ctx_t *ctx, const char *what)
{
	const char	*msg;

	msg = inst_error(ctx);
	if (msg == NULL || msg[0] == '\0') {
		msg = "the operation failed without reporting a reason";
	}
	tui_msgbox(what, msg);

	inst_error_clear(ctx);
}

int
inst_welcome(void)
{
	static const char *const	labels[] = { "Return", "Continue" };
	int				pick;

	tui_backdrop(INST_TITLE);
	pick = tui_buttonbox("Welcome to installer",
	    "This will install otsos onto a disk of this machine. "
	    "The disk you choose is erased in full.\n"
	    "\n"
	    "please choose",
	    labels, 2, 1);
	return (pick == 1);
}


static int
inst_nav_width(void)
{
	int	w;

	w = (tui_cols() * INST_NAV_NUM) / INST_NAV_DEN;
	if (w < INST_NAV_MIN) {
		w = INST_NAV_MIN;
	}
	if (w > INST_NAV_MAX) {
		w = INST_NAV_MAX;
	}
	return (w);
}


static void
inst_win_layout(tui_win_t *nav, tui_win_t *pane)
{
	tui_rect_t	body;
	tui_rect_t	frame;
	int		w;

	tui_screen_body(&body);
	w = inst_nav_width();
	if (w > body.width - 4) {
		w = body.width - 4;
	}
	if (w < 3) {
		w = 3;
	}

	frame.row = body.row;
	frame.col = body.col;
	frame.width = w;
	frame.height = body.height;
	tui_win_at(nav, "Sections", &frame);

	frame.col = body.col + w - 1;
	frame.width = body.width - w + 1;
	if (frame.width < 3) {
		frame.width = 3;
	}
	tui_win_at(pane, "", &frame);
}

static void
inst_nav_row(int index, int width, int selected, void *arg)
{
	inst_win_t	*win = arg;
	char		line[TUI_LABEL_MAX];
	char		sum[TUI_LABEL_MAX];
	const char	*mark;

	(void)selected;
	if (index < 0 || index >= win->count) {
		tui_pad(' ', width);
		return;
	}


	mark = " ";
	if (win->sec[index].ready != NULL &&
	    win->sec[index].ready(win->ctx) != 0) {
		mark = "*";
	}

	sum[0] = '\0';
	if (win->sec[index].summary != NULL) {
		win->sec[index].summary(win->ctx, sum, sizeof(sum));
	}


	if (sum[0] != '\0' && width >= INST_NAV_SUMMARY) {
		snprintf(line, sizeof(line), " %s %s: %s", mark,
		    win->sec[index].title, sum);
	} else {
		snprintf(line, sizeof(line), " %s %s", mark,
		    win->sec[index].title);
	}
	tui_field(line, width);
}


static void
inst_button_draw(inst_win_t *win, const tui_win_t *pane)
{
	const inst_section_t	*sec;
	const tui_theme_t	*theme;
	char			label[TUI_LABEL_MAX];
	const char		*why;
	int			ready;
	int			row;
	int			len;
	int			left;

	sec = &win->sec[win->cur];
	theme = tui_theme();
	ready = (sec->ready == NULL) || (sec->ready(win->ctx) != 0);

	row = pane->frame.row + pane->frame.height - 1;
	len = snprintf(label, sizeof(label), "  %s  ",
	    (win->cur + 1 < win->count) ? "Continue" : "Install");
	if (len < 0 || len + 4 > pane->frame.width) {
		return;
	}

	tui_move(row, pane->frame.col + 2);
	if (!ready) {
		tui_attr(theme->dim_fg, theme->win_bg);
	} else if (win->focus == INST_FOCUS_PANE) {
		tui_attr_bold(theme->button_focus_fg, theme->button_focus_bg);
	} else {
		tui_attr(theme->button_fg, theme->button_bg);
	}
	tui_put(label);
	tui_attr(theme->border_fg, theme->win_bg);

	if (!ready && sec->blocked != NULL) {
		why = sec->blocked(win->ctx);
		left = pane->frame.width - len - 6;
		if (why != NULL && why[0] != '\0' && left > 0) {
			tui_attr(theme->dim_fg, theme->win_bg);
			tui_put(" ");
			tui_field(why, left);
		}
	}
	tui_attr_reset();
}

static void
inst_win_draw(inst_win_t *win)
{
	tui_rect_t	inner;

	inst_win_layout(&win->nav_win, &win->pane_win);
	(void)snprintf(win->pane_win.title, sizeof(win->pane_win.title), "%s",
	    win->sec[win->cur].title);

	tui_screen_begin(INST_TITLE, (win->focus == INST_FOCUS_NAV) ?
	    "Up/Down select   Right/Enter open   Esc abort" :
	    "Left back to sections   Tab continue   Esc abort");


	tui_win_draw(&win->pane_win);
	tui_win_draw(&win->nav_win);

	win->nav.rect = win->nav_win.body;
	tui_list_draw(&win->nav, inst_nav_row, win,
	    win->focus == INST_FOCUS_NAV);

	inner = win->pane_win.body;
	if (win->sec[win->cur].draw != NULL) {
		win->sec[win->cur].draw(win->ctx, &inner,
		    win->focus == INST_FOCUS_PANE);
	}

	inst_button_draw(win, &win->pane_win);
	tui_attr_reset();
	tui_cursor(0);
	tui_flush();
}


static int
inst_win_enter(inst_win_t *win, int index)
{
	if (index < 0 || index >= win->count) {
		return (-1);
	}
	if (win->sec[index].enter != NULL &&
	    win->sec[index].enter(win->ctx) != 0) {
		inst_ui_error(win->ctx, win->sec[index].title);
		return (-1);
	}
	win->cur = index;
	tui_list_select(&win->nav, index);
	win->focus = INST_FOCUS_PANE;
	return (0);
}


static int
inst_win_next(inst_win_t *win)
{
	const inst_section_t	*sec;
	const char		*why;
	int			i;

	sec = &win->sec[win->cur];
	if (sec->ready != NULL && sec->ready(win->ctx) == 0) {
		why = (sec->blocked != NULL) ? sec->blocked(win->ctx) : NULL;
		tui_msgbox(sec->title, (why != NULL && why[0] != '\0') ? why :
		    "this section is not finished yet");
		return (INST_ACT_NONE);
	}

	for (i = win->cur + 1; i < win->count; i++) {
		if (inst_win_enter(win, i) == 0) {
			return (INST_ACT_NONE);
		}
	}
	return (INST_ACT_FINISH);
}

int
inst_ui_run(inst_ctx_t *ctx)
{
	inst_win_t	win;
	tui_key_t	key;
	int		act;

	memset(&win, 0, sizeof(win));
	win.ctx = ctx;
	win.sec = inst_sections(&win.count);
	if (win.sec == NULL || win.count <= 0) {
		tui_msgbox(INST_TITLE, "the installer has no sections to show");
		return (-1);
	}

	inst_win_layout(&win.nav_win, &win.pane_win);
	tui_list_init(&win.nav, &win.nav_win.body, win.count);
	win.focus = INST_FOCUS_NAV;
	win.cur = 0;

	if (win.sec[0].enter != NULL && win.sec[0].enter(ctx) != 0) {
		inst_ui_error(ctx, win.sec[0].title);
	}

	for (;;) {
		inst_win_draw(&win);
		tui_key_read(&key);

		if (key.code == TUI_KEY_ESC) {
			if (win.focus == INST_FOCUS_PANE) {
				win.focus = INST_FOCUS_NAV;
				continue;
			}
			if (tui_yesno(INST_TITLE,
			    "Abandon the installation and leave the disk "
			    "untouched?", 0) == TUI_OK) {
				return (-1);
			}
			continue;
		}

		if (win.focus == INST_FOCUS_NAV) {
			if (tui_list_key(&win.nav, &key)) {

				win.cur = win.nav.sel;
				continue;
			}
			if (key.code == TUI_KEY_RIGHT ||
			    key.code == TUI_KEY_ENTER ||
			    key.code == TUI_KEY_KP_ENTER) {
				(void)inst_win_enter(&win, win.nav.sel);
			}
			continue;
		}

		if (key.code == TUI_KEY_LEFT) {
			win.focus = INST_FOCUS_NAV;
			continue;
		}


		if (key.code == TUI_KEY_TAB) {
			if (inst_win_next(&win) == INST_ACT_FINISH) {
				return (0);
			}
			continue;
		}

		act = INST_ACT_NONE;
		if (win.sec[win.cur].key != NULL) {
			act = win.sec[win.cur].key(ctx, &key);
		}

		switch (act) {
		case INST_ACT_LEAVE:
			win.focus = INST_FOCUS_NAV;
			break;
		case INST_ACT_NEXT:
			if (inst_win_next(&win) == INST_ACT_FINISH) {
				return (0);
			}
			break;
		case INST_ACT_QUIT:
			return (-1);
		case INST_ACT_FINISH:
			return (0);
		default:
			break;
		}
	}
}

int
inst_finish(inst_ctx_t *ctx)
{
	static const char *const	labels[] = { "Live system", "Reboot" };
	char				text[TUI_TEXT_MAX];
	int				pick;

	snprintf(text, sizeof(text),
	    "otsos is installed on %s: %d files written%s.\n"
	    "\n"
	    "Stay in the live system, or reboot into the installed one?",
	    ctx->target.disk.info.name, ctx->copied,
	    ctx->boot_done ? " and the boot code is in place" : "");

	tui_backdrop(INST_TITLE);
	pick = tui_buttonbox("Installation complete", text, labels, 2, 1);
	if (pick != 1) {
		return (0);
	}

	tui_fini();
	if (powerState(API_POWER_STATE_REBOOT) != 0) {

		printf("install: reboot was refused, "
		    "power-cycle the machine manually\n");
		return (-1);
	}
	return (0);
}
