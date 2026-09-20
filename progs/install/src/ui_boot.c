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

$define %func inst_sec_boot as the boot-method section of the installer

*/

/* !SPACE!

$space %export inst_sec_boot

*/

#include <stdio.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "plan.h"
#include "ui.h"

#define INST_BOOT_BIOS		0
#define INST_BOOT_UEFI		1
#define INST_BOOT_ROWS		2

static tui_list_t	inst_boot_list;
static int		inst_boot_bound;


static int
inst_boot_have(const inst_ctx_t *ctx, int row)
{
	switch (row) {
	case INST_BOOT_BIOS:
		return (inst_module_by_role(&ctx->plan, INST_ROLE_STAGE1) !=
		    NULL && inst_module_by_role(&ctx->plan,
		    INST_ROLE_STAGE2) != NULL);
	case INST_BOOT_UEFI:
		return (inst_module_by_role(&ctx->plan, INST_ROLE_UEFI) !=
		    NULL);
	default:
		return (0);
	}
}

static int *
inst_boot_flag(inst_ctx_t *ctx, int row)
{
	switch (row) {
	case INST_BOOT_BIOS:
		return (&ctx->want_bios);
	case INST_BOOT_UEFI:
		return (&ctx->want_uefi);
	default:
		return (NULL);
	}
}

static const char *
inst_boot_name(int row)
{
	switch (row) {
	case INST_BOOT_BIOS:
		return ("Legacy BIOS (MBR stage1 + stage2)");
	case INST_BOOT_UEFI:
		return ("UEFI (BOOTX64.EFI on the ESP)");
	default:
		return ("");
	}
}

static void
inst_boot_row(int index, int width, int selected, void *arg)
{
	inst_ctx_t	*ctx = arg;
	const int	*flag;
	char		line[TUI_LABEL_MAX];

	(void)selected;
	if (index < 0 || index >= INST_BOOT_ROWS) {
		tui_pad(' ', width);
		return;
	}

	flag = inst_boot_flag(ctx, index);
	if (!inst_boot_have(ctx, index))
		snprintf(line, sizeof(line), " [-] %s  (not in this build)",
		    inst_boot_name(index));
	else
		snprintf(line, sizeof(line), " [%c] %s",
		    (flag != NULL && *flag) ? 'x' : ' ',
		    inst_boot_name(index));

	tui_field(line, width);
}

static void
inst_boot_draw(inst_ctx_t *ctx, const tui_rect_t *rect, int focused)
{
	
	static const char *const	roles[] = {
		INST_ROLE_STAGE1, INST_ROLE_STAGE2, INST_ROLE_UEFI,
		INST_ROLE_KERNEL, INST_ROLE_CMSEED
	};
	const inst_module_t		*mod;
	const tui_theme_t		*theme;
	tui_rect_t			lr;
	char				line[TUI_TEXT_MAX];
	int				row;
	int				i;

	theme = tui_theme();
	lr = *rect;
	lr.row = rect->row + 2;
	lr.height = INST_BOOT_ROWS;

	tui_move(rect->row, rect->col);
	tui_attr_bold(theme->title_fg, theme->win_bg);
	tui_field(" How should this machine boot the installed system?",
	    rect->width);
	tui_attr(theme->text_fg, theme->win_bg);

	if (!inst_boot_bound) {
		tui_list_init(&inst_boot_list, &lr, INST_BOOT_ROWS);
		inst_boot_bound = 1;
	}
	inst_boot_list.rect = lr;
	tui_list_draw(&inst_boot_list, inst_boot_row, ctx, focused);

	row = lr.row + lr.height;
	tui_attr(theme->border_fg, theme->win_bg);
	tui_hline(row, rect->col, rect->width);
	row++;

	tui_move(row, rect->col);
	tui_attr(theme->text_fg, theme->win_bg);
	tui_field(" Space toggles.", rect->width);

	row += 2;
	tui_move(row, rect->col);
	tui_attr(theme->dim_fg, theme->win_bg);
	tui_field(" Boot components of this build", rect->width);

	for (i = 0; i < (int)(sizeof(roles) / sizeof(roles[0])); i++) {
		mod = inst_module_by_role(&ctx->plan, roles[i]);
		tui_move(row + 1 + i, rect->col);
		if (mod != NULL) {
			tui_attr(theme->text_fg, theme->win_bg);
			snprintf(line, sizeof(line), " %-11s %s", roles[i],
			    mod->dest);
		} else {
			tui_attr(theme->dim_fg, theme->win_bg);
			snprintf(line, sizeof(line), " %-11s %s", roles[i],
			    "absent");
		}
		tui_field(line, rect->width);
	}
	tui_attr_reset();
}

static int
inst_boot_key(inst_ctx_t *ctx, const tui_key_t *key)
{
	int	*flag;

	if (tui_list_key(&inst_boot_list, key))
		return (INST_ACT_NONE);

	switch (key->code) {
	case TUI_KEY_SPACE:
	case TUI_KEY_ENTER:
	case TUI_KEY_KP_ENTER:
		if (!inst_boot_have(ctx, inst_boot_list.sel)) {
			tui_msgbox(inst_boot_name(inst_boot_list.sel),
			    "This build carries no module for that boot "
			    "method, so it cannot be installed.");
			return (INST_ACT_NONE);
		}
		flag = inst_boot_flag(ctx, inst_boot_list.sel);
		if (flag != NULL)
			*flag = !*flag;
		return (INST_ACT_NONE);
	default:
		break;
	}
	return (INST_ACT_NONE);
}


static int
inst_boot_ready(inst_ctx_t *ctx)
{
	return (ctx->want_bios != 0 || ctx->want_uefi != 0);
}

static const char *
inst_boot_blocked(inst_ctx_t *ctx)
{
	if (!inst_boot_have(ctx, INST_BOOT_BIOS) &&
	    !inst_boot_have(ctx, INST_BOOT_UEFI))
		return ("this build has no boot components at all");
	return ("choose at least one boot method");
}

static void
inst_boot_summary(inst_ctx_t *ctx, char *out, size_t size)
{
	if (ctx->want_bios && ctx->want_uefi)
		snprintf(out, size, "%s", "BIOS + UEFI");
	else if (ctx->want_bios)
		snprintf(out, size, "%s", "BIOS");
	else if (ctx->want_uefi)
		snprintf(out, size, "%s", "UEFI");
	else
		out[0] = '\0';
}

const inst_section_t	inst_sec_boot = {
	.title		= "Boot",
	.summary	= inst_boot_summary,
	.draw		= inst_boot_draw,
	.key		= inst_boot_key,
	.ready		= inst_boot_ready,
	.blocked	= inst_boot_blocked,
};
