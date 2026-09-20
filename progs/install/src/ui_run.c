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

$define %type inst_step_t as one destructive stage with a name for the log

$define %func inst_run as function with args inst_ctx_t *
$define %func inst_sections as function with args int *

*/

/* !SPACE!

$space %internal inst_step_t, inst_run_progress, inst_run_log
$space %export inst_run, inst_sections

*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "stage.h"
#include "ui.h"

#define INST_LOG_ROWS		16

typedef struct inst_step {
	const char	*name;
	int		(*fn)(inst_ctx_t *ctx);
	int		progress;
} inst_step_t;

static tui_gauge_t	inst_run_bar;
static int		inst_run_bar_open;

static char		inst_log[INST_LOG_ROWS][TUI_LABEL_MAX];
static int		inst_log_used;

static void
inst_run_log(const char *fmt, ...)
{
	va_list	ap;
	int	i;

	if (inst_log_used == INST_LOG_ROWS) {
		for (i = 1; i < INST_LOG_ROWS; i++)
			memcpy(inst_log[i - 1], inst_log[i],
			    sizeof(inst_log[0]));
		inst_log_used--;
	}
	va_start(ap, fmt);
	vsnprintf(inst_log[inst_log_used], sizeof(inst_log[0]), fmt, ap);
	va_end(ap);
	inst_log_used++;
}

static void
inst_run_paint(inst_ctx_t *ctx, const char *stage)
{
	const tui_theme_t	*theme;
	tui_rect_t		body;
	tui_win_t		win;
	char			line[TUI_LABEL_MAX];
	int			i;

	theme = tui_theme();
	tui_screen_begin("otsos installer", stage);
	tui_screen_body(&body);


	tui_win_at(&win, "Installing", &body);
	tui_win_draw(&win);

	tui_move(win.body.row, win.body.col);
	snprintf(line, sizeof(line), " Installing onto %s",
	    ctx->target.disk.info.name);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(line, win.body.width);
	tui_attr(theme->text_fg, theme->win_bg);

	for (i = 0; i < inst_log_used && i + 2 < win.body.height; i++) {
		tui_move(win.body.row + 2 + i, win.body.col);
		snprintf(line, sizeof(line), " %s", inst_log[i]);
		tui_field(line, win.body.width);
	}
	tui_attr_reset();
	tui_cursor(0);
	tui_flush();
}


static void
inst_run_progress(const char *label, uint64_t done, uint64_t total, void *arg)
{
	(void)arg;

	if (!inst_run_bar_open) {
		tui_gauge_open(&inst_run_bar, "Copying the system",
		    (unsigned long)total);
		inst_run_bar_open = 1;
	}
	tui_gauge_step(&inst_run_bar, (unsigned long)done, label);
}


static int
inst_run(inst_ctx_t *ctx)
{
	static const inst_step_t	steps[] = {
		{ "Partitioning",	inst_partition,	0 },
		{ "Creating filesystems", inst_format,	0 },
		{ "Copying files",	NULL,		1 },
		{ "Installing boot code", inst_boot,	0 },
		{ "Verifying",		inst_verify,	0 },
	};
	const inst_step_t		*step;
	int				count;
	int				ret;
	int				i;

	inst_log_used = 0;
	count = (int)(sizeof(steps) / sizeof(steps[0]));

	for (i = 0; i < count; i++) {
		step = &steps[i];
		inst_run_log("%-24s ...", step->name);
		inst_run_paint(ctx, step->name);

		if (step->progress) {
			inst_run_bar_open = 0;
			ret = inst_copy(ctx, inst_run_progress, ctx);
			if (inst_run_bar_open) {
				tui_gauge_close(&inst_run_bar);
				inst_run_bar_open = 0;
			}
		} else {
			ret = step->fn(ctx);
		}

		inst_log_used--;
		if (ret != 0) {
			inst_run_log("%-24s failed", step->name);
			inst_run_paint(ctx, "failed");
			inst_ui_error(ctx, step->name);
			return (-1);
		}
		if (step->progress)
			inst_run_log("%-24s %d files", step->name,
			    ctx->copied);
		else
			inst_run_log("%-24s done", step->name);
		inst_run_paint(ctx, step->name);
	}
	return (0);
}


static void
inst_install_draw(inst_ctx_t *ctx, const tui_rect_t *rect, int focused)
{
	static const char	idle[] = "Nothing has been written yet. "
	    "Press Enter to start the installation; it asks once more, naming "
	    "the disk, before anything is written.";
	const tui_theme_t	*theme;
	char			line[TUI_TEXT_MAX];
	tui_rect_t		text;
	int			i;

	(void)ctx;
	(void)focused;
	theme = tui_theme();

	tui_move(rect->row, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" Install", rect->width);
	tui_attr(theme->text_fg, theme->win_bg);

	if (inst_log_used == 0) {
		text = *rect;
		text.row = rect->row + 2;
		text.col = rect->col + 1;
		text.width = rect->width - 2;
		text.height = rect->height - 2;
		if (text.width > 0 && text.height > 0) {
			(void)tui_text_draw(&text, idle, 0);
		}
		tui_attr_reset();
		return;
	}

	for (i = 0; i < inst_log_used && i + 2 < rect->height; i++) {
		tui_move(rect->row + 2 + i, rect->col);
		snprintf(line, sizeof(line), " %s", inst_log[i]);
		tui_field(line, rect->width);
	}
	tui_attr_reset();
}

static int
inst_install_key(inst_ctx_t *ctx, const tui_key_t *key)
{
	char	text[TUI_TEXT_MAX];

	switch (key->code) {
	case TUI_KEY_ENTER:
	case TUI_KEY_KP_ENTER:
		if (ctx->boot_done) {
			return (INST_ACT_FINISH);
		}
		snprintf(text, sizeof(text),
		    "Erase %s and install otsos onto it?\n"
		    "\n"
		    "Every partition and every file on that disk is lost.",
		    ctx->target.disk.info.name);
		if (tui_yesno("Install", text, 0) != TUI_OK)
			return (INST_ACT_NONE);
		if (inst_run(ctx) != 0)
			return (INST_ACT_NONE);
		return (INST_ACT_FINISH);
	default:
		break;
	}
	return (INST_ACT_NONE);
}


static int
inst_install_ready(inst_ctx_t *ctx)
{
	return (ctx->boot_done);
}

static const char *
inst_install_blocked(inst_ctx_t *ctx)
{
	(void)ctx;
	return ("press Enter to start the installation");
}

static void
inst_install_summary(inst_ctx_t *ctx, char *out, size_t size)
{
	if (ctx->boot_done)
		snprintf(out, size, "%s", "installed");
	else
		out[0] = '\0';
}

static const inst_section_t	inst_sec_install = {
	.title		= "Install",
	.summary	= inst_install_summary,
	.enter		= NULL,
	.draw		= inst_install_draw,
	.key		= inst_install_key,
	.ready		= inst_install_ready,
	.blocked	= inst_install_blocked,
};

extern const inst_section_t	inst_sec_disk;
extern const inst_section_t	inst_sec_layout;
extern const inst_section_t	inst_sec_boot;
extern const inst_section_t	inst_sec_confirm;


const inst_section_t *
inst_sections(int *count)
{
	static const inst_section_t	*const table[] = {
		&inst_sec_disk,
		&inst_sec_layout,
		&inst_sec_boot,
		&inst_sec_confirm,
		&inst_sec_install,
	};
	static inst_section_t		flat[sizeof(table) / sizeof(table[0])];
	static int			built;
	int				n;
	int				i;

	n = (int)(sizeof(table) / sizeof(table[0]));

	if (!built) {
		for (i = 0; i < n; i++)
			flat[i] = *table[i];
		built = 1;
	}
	if (count != NULL)
		*count = n;
	return (flat);
}
