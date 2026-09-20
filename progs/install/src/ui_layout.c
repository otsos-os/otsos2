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

$define %func inst_sec_layout as the partition-layout section of the installer

*/

/* !SPACE!

$space %export inst_sec_layout

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "plan.h"
#include "ui.h"

#define INST_LAY_ESP		0
#define INST_LAY_FILES		1
#define INST_LAY_ROWS		2
#define INST_ESP_MIN_MIB	16ULL
#define INST_ESP_MAX_MIB	1024ULL
#define INST_FILES_MIN		256UL
#define INST_FILES_MAX		1048576UL

#define INST_MIB		(1024ULL * 1024ULL)

static tui_list_t	inst_lay_list;
static int		inst_lay_bound;

static void
inst_lay_value(inst_ctx_t *ctx, int row, char *out, size_t size)
{
	switch (row) {
	case INST_LAY_ESP:
		snprintf(out, size, "%llu MiB",
		    (unsigned long long)(ctx->esp_bytes / INST_MIB));
		break;
	case INST_LAY_FILES:
		snprintf(out, size, "%lu entries",
		    (unsigned long)ctx->root_max_files);
		break;
	default:
		snprintf(out, size, "%s", "");
		break;
	}
}

static const char *
inst_lay_name(int row)
{
	switch (row) {
	case INST_LAY_ESP:
		return ("EFI system partition");
	case INST_LAY_FILES:
		return ("Root filesystem file ceiling");
	default:
		return ("");
	}
}

static void
inst_lay_row(int index, int width, int selected, void *arg)
{
	inst_ctx_t	*ctx = arg;
	char		val[TUI_LABEL_MAX];
	char		line[TUI_LABEL_MAX];

	(void)selected;
	if (index < 0 || index >= INST_LAY_ROWS) {
		tui_pad(' ', width);
		return;
	}

	inst_lay_value(ctx, index, val, sizeof(val));
	snprintf(line, sizeof(line), " %-28s %s", inst_lay_name(index), val);
	tui_field(line, width);
}

static void
inst_lay_draw(inst_ctx_t *ctx, const tui_rect_t *rect, int focused)
{
	const tui_theme_t	*theme;
	tui_rect_t		lr;
	char			line[TUI_TEXT_MAX];
	uint64_t		root;
	uint64_t		used;
	uint32_t		ss;
	int			row;

	theme = tui_theme();
	lr = *rect;
	lr.row = rect->row + 2;
	lr.height = INST_LAY_ROWS;

	tui_move(rect->row, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" Partition layout", rect->width);
	tui_attr(theme->text_fg, theme->win_bg);

	if (!inst_lay_bound) {
		tui_list_init(&inst_lay_list, &lr, INST_LAY_ROWS);
		inst_lay_bound = 1;
	}
	inst_lay_list.rect = lr;
	tui_list_draw(&inst_lay_list, inst_lay_row, ctx, focused);

	row = lr.row + lr.height;
	tui_attr(theme->border_fg, theme->win_bg);
	tui_hline(row, rect->col, rect->width);
	row++;

	tui_move(row, rect->col);
	tui_attr(theme->text_fg, theme->win_bg);
	tui_field(" Enter edits the selected value.", rect->width);

	ss = ctx->target.disk.info.sector_size;
	if (ctx->selected < 0 || ss == 0) {
		tui_move(row + 2, rect->col);
		tui_attr(theme->dim_fg, theme->win_bg);
		tui_field(" Choose a disk to see the resulting layout.",
		    rect->width);
		tui_attr_reset();
		return;
	}

	used = inst_sectors(INST_BIOS_BYTES, ss) +
	    inst_sectors(ctx->esp_bytes, ss);
	root = ctx->target.disk.info.total_sectors;
	root = (root > used) ? (root - used) : 0;

	tui_move(row + 2, rect->col);
	snprintf(line, sizeof(line), " %-13s %-10s %s", "Partition", "Size",
	    "Contents");
	tui_attr(theme->dim_fg, theme->win_bg);
	tui_field(line, rect->width);

	tui_attr(theme->text_fg, theme->win_bg);
	tui_move(row + 3, rect->col);
	inst_size_label(line, sizeof(line), inst_sectors(INST_BIOS_BYTES, ss),
	    ss);
	tui_putf(" %-13s %-10s %s", INST_NAME_BIOS, line, "BIOS stage2, raw");

	tui_move(row + 4, rect->col);
	inst_size_label(line, sizeof(line), inst_sectors(ctx->esp_bytes, ss),
	    ss);
	tui_putf(" %-13s %-10s %s", INST_NAME_ESP, line, "FAT32, UEFI loader");

	tui_move(row + 5, rect->col);
	inst_size_label(line, sizeof(line), root, ss);
	tui_putf(" %-13s %-10s %s", INST_NAME_ROOT, line,
	    "ChainFS, the system");
	tui_attr_reset();
}


static void
inst_lay_edit(inst_ctx_t *ctx, int row)
{
	char		buf[TUI_LABEL_MAX];
	char		*end;
	unsigned long	val;

	buf[0] = '\0';
	switch (row) {
	case INST_LAY_ESP:
		snprintf(buf, sizeof(buf), "%llu",
		    (unsigned long long)(ctx->esp_bytes / INST_MIB));
		if (tui_inputbox("EFI system partition",
		    "Size in MiB:", buf, sizeof(buf)) != 0)
			return;
		val = strtoul(buf, &end, 10);
		if (end == buf || (*end != '\0' && *end != ' ') ||
		    val < INST_ESP_MIN_MIB || val > INST_ESP_MAX_MIB) {
			tui_msgbox("EFI system partition",
			    "Enter a whole number of MiB between 16 and 1024.");
			return;
		}
		ctx->esp_bytes = (uint64_t)val * INST_MIB;
		break;
	case INST_LAY_FILES:
		snprintf(buf, sizeof(buf), "%lu",
		    (unsigned long)ctx->root_max_files);
		if (tui_inputbox("Root filesystem",
		    "Maximum number of files:", buf, sizeof(buf)) != 0)
			return;
		val = strtoul(buf, &end, 10);
		if (end == buf || (*end != '\0' && *end != ' ') ||
		    val < INST_FILES_MIN || val > INST_FILES_MAX) {
			tui_msgbox("Root filesystem",
			    "Enter a count between 256 and 1048576. "
			    "ChainFS reserves its file table up front, so this "
			    "cannot be raised after the filesystem is made.");
			return;
		}
		ctx->root_max_files = (uint32_t)val;
		break;
	default:
		break;
	}
}

static int
inst_lay_key(inst_ctx_t *ctx, const tui_key_t *key)
{
	if (tui_list_key(&inst_lay_list, key))
		return (INST_ACT_NONE);

	switch (key->code) {
	case TUI_KEY_ENTER:
	case TUI_KEY_KP_ENTER:
	case TUI_KEY_SPACE:
		inst_lay_edit(ctx, inst_lay_list.sel);
		return (INST_ACT_NONE);
	default:
		break;
	}
	return (INST_ACT_NONE);
}

static int
inst_lay_enter(inst_ctx_t *ctx)
{
	if (ctx->selected < 0) {
		inst_fail(ctx, "choose a disk before laying it out");
		return (-1);
	}
	return (0);
}


static int
inst_lay_ready(inst_ctx_t *ctx)
{
	uint64_t	used;
	uint32_t	ss;

	ss = ctx->target.disk.info.sector_size;
	if (ctx->selected < 0 || ss == 0)
		return (0);

	used = inst_sectors(INST_BIOS_BYTES, ss) +
	    inst_sectors(ctx->esp_bytes, ss);
	return (ctx->target.disk.info.total_sectors > used);
}

static const char *
inst_lay_blocked(inst_ctx_t *ctx)
{
	if (ctx->selected < 0)
		return ("no disk chosen yet");
	return ("the disk is too small for this layout");
}

static void
inst_lay_summary(inst_ctx_t *ctx, char *out, size_t size)
{
	snprintf(out, size, "ESP %llu MiB",
	    (unsigned long long)(ctx->esp_bytes / INST_MIB));
}

const inst_section_t	inst_sec_layout = {
	.title		= "Layout",
	.summary	= inst_lay_summary,
	.enter		= inst_lay_enter,
	.draw		= inst_lay_draw,
	.key		= inst_lay_key,
	.ready		= inst_lay_ready,
	.blocked	= inst_lay_blocked,
};
