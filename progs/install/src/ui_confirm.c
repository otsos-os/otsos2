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

$define %func inst_sec_confirm as the review section of the installer

*/

/* !SPACE!

$space %export inst_sec_confirm

*/

#include <stdio.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"
#include "ui.h"

static tui_list_t	inst_conf_list;
static int		inst_conf_bound;


static void
inst_conf_row(int index, int width, int selected, void *arg)
{
	inst_ctx_t	*ctx = arg;
	char		line[TUI_LABEL_MAX];

	(void)selected;
	if (index < 0 || index >= ctx->plan.tree_count) {
		tui_pad(' ', width);
		return;
	}
	snprintf(line, sizeof(line), " %s", ctx->plan.trees[index]);
	tui_field(line, width);
}

static void
inst_conf_draw(inst_ctx_t *ctx, const tui_rect_t *rect, int focused)
{
	const tui_theme_t	*theme;
	tui_rect_t		lr;
	char			line[TUI_TEXT_MAX];
	char			size[TUI_LABEL_MAX];
	uint64_t		total;
	int			row;

	theme = tui_theme();
	tui_move(rect->row, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" Review", rect->width);
	tui_attr(theme->text_fg, theme->win_bg);

	row = rect->row + 2;

	tui_move(row, rect->col);
	if (ctx->selected < 0) {
		tui_attr(theme->dim_fg, theme->win_bg);
		tui_field(" No disk chosen. Go back to the Disk section.",
		    rect->width);
		tui_attr_reset();
		return;
	}

	inst_size_label(size, sizeof(size), ctx->target.disk.info.total_sectors,
	    ctx->target.disk.info.sector_size);
	snprintf(line, sizeof(line), " Target   %s (%s)",
	    ctx->target.disk.info.name, size);
	tui_field(line, rect->width);

	tui_move(row + 1, rect->col);
	snprintf(line, sizeof(line), " Boot     %s%s%s",
	    ctx->want_bios ? "BIOS" : "",
	    (ctx->want_bios && ctx->want_uefi) ? " + " : "",
	    ctx->want_uefi ? "UEFI" : "");
	tui_field(line, rect->width);

	tui_move(row + 2, rect->col);
	snprintf(line, sizeof(line), " Modules  %d declared by this build",
	    ctx->plan.module_count);
	tui_field(line, rect->width);

	tui_move(row + 3, rect->col);
	
	if (inst_copy_total(ctx, &total) == 0) {
		inst_size_label(size, sizeof(size), total, 1);
		snprintf(line, sizeof(line), " Payload  %s to copy", size);
	} else {
		snprintf(line, sizeof(line), " Payload  %s",
		    "cannot be measured, see the message below");
		inst_error_clear(ctx);
	}
	tui_field(line, rect->width);

	tui_move(row + 4, rect->col);
	tui_attr(theme->border_fg, theme->win_bg);
	tui_hline(row + 4, rect->col, rect->width);

	tui_move(row + 5, rect->col);
	tui_attr(theme->dim_fg, theme->win_bg);
	tui_field(" Directories to copy", rect->width);

	lr = *rect;
	lr.row = row + 6;
	lr.height = rect->height - (lr.row - rect->row) - 3;
	if (lr.height < 1)
		lr.height = 1;

	if (!inst_conf_bound) {
		tui_list_init(&inst_conf_list, &lr, ctx->plan.tree_count);
		inst_conf_bound = 1;
	}
	inst_conf_list.rect = lr;
	tui_list_count_set(&inst_conf_list, ctx->plan.tree_count);
	tui_list_draw(&inst_conf_list, inst_conf_row, ctx, focused);

	
	tui_move(rect->row + rect->height - 1, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" Continue erases every partition on this disk.",
	    rect->width);
	tui_attr_reset();
}

static int
inst_conf_key(inst_ctx_t *ctx, const tui_key_t *key)
{
	(void)ctx;

	if (tui_list_key(&inst_conf_list, key))
		return (INST_ACT_NONE);
	return (INST_ACT_NONE);
}

static int
inst_conf_enter(inst_ctx_t *ctx)
{
	if (ctx->selected < 0) {
		inst_fail(ctx, "choose a disk before reviewing the install");
		return (-1);
	}
	inst_conf_bound = 0;
	return (0);
}


static int
inst_conf_ready(inst_ctx_t *ctx)
{
	return (ctx->selected >= 0 && ctx->plan.tree_count > 0 &&
	    (ctx->want_bios != 0 || ctx->want_uefi != 0));
}

static const char *
inst_conf_blocked(inst_ctx_t *ctx)
{
	if (ctx->selected < 0)
		return ("no disk chosen");
	if (ctx->plan.tree_count == 0)
		return ("this build declares nothing to copy");
	return ("no boot method chosen");
}

static void
inst_conf_summary(inst_ctx_t *ctx, char *out, size_t size)
{
	if (inst_conf_ready(ctx))
		snprintf(out, size, "%s", "ready");
	else
		out[0] = '\0';
}

const inst_section_t	inst_sec_confirm = {
	.title		= "Review",
	.summary	= inst_conf_summary,
	.enter		= inst_conf_enter,
	.draw		= inst_conf_draw,
	.key		= inst_conf_key,
	.ready		= inst_conf_ready,
	.blocked	= inst_conf_blocked,
};
