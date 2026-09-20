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

$define %func inst_sec_disk as the disk-selection section of the installer

*/

/* !SPACE!

$space %export inst_sec_disk

*/

#include <stdio.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "stage.h"
#include "ui.h"

#define INST_DISK_DETAIL	6

static tui_list_t	inst_disk_list;
static int		inst_disk_bound;


static void
inst_disk_row(int index, int width, int selected, void *arg)
{
	inst_ctx_t	*ctx = arg;
	inst_cand_t	*cand;
	char		line[TUI_LABEL_MAX];


	(void)selected;
	if (index < 0 || index >= ctx->cand_count) {
		tui_pad(' ', width);
		return;
	}
	cand = &ctx->cands[index];

	if (cand->usable)
		snprintf(line, sizeof(line), " %-12s %s",
		    cand->entry.info.name, cand->label);
	else
		snprintf(line, sizeof(line), " %-12s %s  (%s)",
		    cand->entry.info.name, cand->label, cand->reason);

	tui_field(line, width);
}

static int
inst_disk_enter(inst_ctx_t *ctx)
{
	
	if (inst_scan(ctx) != 0)
		return (-1);
	inst_disk_bound = 0;
	if (ctx->selected >= 0 && ctx->selected < ctx->cand_count)
		tui_list_select(&inst_disk_list, ctx->selected);
	return (0);
}

static void
inst_disk_draw(inst_ctx_t *ctx, const tui_rect_t *rect, int focused)
{
	const tui_theme_t	*theme;
	tui_rect_t		lr;
	inst_cand_t		*cand;
	char			line[TUI_TEXT_MAX];
	int			row;

	theme = tui_theme();
	lr = *rect;
	lr.row = rect->row + 2;
	lr.height = rect->height - 2 - INST_DISK_DETAIL;
	if (lr.height < 1)
		lr.height = 1;

	tui_move(rect->row, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" Choose the disk to install onto", rect->width);
	tui_attr(theme->text_fg, theme->win_bg);

	if (ctx->cand_count == 0) {
		tui_move(rect->row + 2, rect->col);
		tui_field(" No disk is available. Attach one and re-enter this "
		    "section to rescan.", rect->width);
		tui_attr_reset();
		return;
	}

	if (!inst_disk_bound) {
		tui_list_init(&inst_disk_list, &lr, ctx->cand_count);
		if (ctx->selected >= 0)
			tui_list_select(&inst_disk_list, ctx->selected);
		inst_disk_bound = 1;
	}
	inst_disk_list.rect = lr;
	tui_list_count_set(&inst_disk_list, ctx->cand_count);
	tui_list_draw(&inst_disk_list, inst_disk_row, ctx, focused);

	cand = &ctx->cands[inst_disk_list.sel];
	row = lr.row + lr.height;

	tui_attr(theme->border_fg, theme->win_bg);
	tui_hline(row, rect->col, rect->width);
	tui_attr(theme->text_fg, theme->win_bg);
	row++;

	tui_move(row, rect->col);
	snprintf(line, sizeof(line), " Device   %s", cand->entry.info.name);
	tui_field(line, rect->width);

	tui_move(row + 1, rect->col);
	snprintf(line, sizeof(line), " Path     %s", cand->entry.path);
	tui_field(line, rect->width);

	tui_move(row + 2, rect->col);
	snprintf(line, sizeof(line), " Geometry %llu sectors of %u bytes",
	    (unsigned long long)cand->entry.info.total_sectors,
	    (unsigned)cand->entry.info.sector_size);
	tui_field(line, rect->width);

	tui_move(row + 3, rect->col);
	if (cand->usable) {
		snprintf(line, sizeof(line), " %s",
		    (ctx->selected == inst_disk_list.sel) ?
		    "Selected. Everything on this disk will be erased." :
		    "Press Space or Enter to select this disk.");
	} else {
		tui_attr(theme->dim_fg, theme->win_bg);
		snprintf(line, sizeof(line), " Unusable: %s", cand->reason);
	}
	tui_field(line, rect->width);
	tui_attr_reset();
}

static int
inst_disk_key(inst_ctx_t *ctx, const tui_key_t *key)
{
	inst_cand_t	*cand;

	if (tui_list_key(&inst_disk_list, key))
		return (INST_ACT_NONE);

	switch (key->code) {
	case TUI_KEY_ENTER:
	case TUI_KEY_KP_ENTER:
	case TUI_KEY_SPACE:
		if (ctx->cand_count == 0)
			return (INST_ACT_NONE);
		cand = &ctx->cands[inst_disk_list.sel];
		if (!cand->usable) {
			tui_msgbox(cand->entry.info.name, cand->reason);
			return (INST_ACT_NONE);
		}

		if (ctx->selected != inst_disk_list.sel) {
			memset(&ctx->target, 0, sizeof(ctx->target));
			ctx->copied = 0;
			ctx->boot_done = 0;
		}
		ctx->selected = inst_disk_list.sel;
		ctx->target.disk = cand->entry;
		return (INST_ACT_NONE);
	case TUI_KEY_R:
		if (inst_disk_enter(ctx) != 0)
			inst_ui_error(ctx, "Disk");
		return (INST_ACT_NONE);
	default:
		break;
	}
	return (INST_ACT_NONE);
}

static int
inst_disk_ready(inst_ctx_t *ctx)
{
	return (ctx->selected >= 0 && ctx->selected < ctx->cand_count &&
	    ctx->cands[ctx->selected].usable);
}

static const char *
inst_disk_blocked(inst_ctx_t *ctx)
{
	if (ctx->cand_count == 0)
		return ("no usable disk was found");
	return ("select a disk first");
}

static void
inst_disk_summary(inst_ctx_t *ctx, char *out, size_t size)
{
	if (inst_disk_ready(ctx))
		snprintf(out, size, "%s", ctx->target.disk.info.name);
	else
		out[0] = '\0';
}

const inst_section_t	inst_sec_disk = {
	.title		= "Disk",
	.summary	= inst_disk_summary,
	.enter		= inst_disk_enter,
	.draw		= inst_disk_draw,
	.key		= inst_disk_key,
	.ready		= inst_disk_ready,
	.blocked	= inst_disk_blocked,
};
