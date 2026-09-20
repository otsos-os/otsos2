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

$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type size_t as native object size
$define %type tui_rect_t as a rectangle of the terminal
$define %type tui_key_t as one decoded key press
$define %type inst_ctx_t as installer run state
$define %type inst_section_t as one pane of the installer, its own module
$define %type inst_ui_t as navigation state of the installer window

$define %func inst_welcome as function with args void
$define %func inst_sections as function with args int *
$define %func inst_ui_run as function with args inst_ctx_t *
$define %func inst_finish as function with args inst_ctx_t *
$define %func inst_ui_error as procedure with args inst_ctx_t *, const char *
$define %func inst_ui_wrap as function with args const char *, int, int, char *, size_t

*/

/* !SPACE!

$space %export inst_section_t, inst_ui_t
$space %export inst_welcome, inst_sections, inst_ui_run, inst_finish
$space %export inst_ui_error, inst_ui_wrap

*/

#ifndef INSTALL_UI_H
#define INSTALL_UI_H

#include <tui.h>

#include "inst.h"

#define INST_ACT_NONE		0
#define INST_ACT_LEAVE		1
#define INST_ACT_NEXT		2
#define INST_ACT_QUIT		3
#define INST_ACT_FINISH		4

typedef struct inst_section {
	const char	*title;
	void		(*summary)(inst_ctx_t *ctx, char *out, size_t size);
	int		(*enter)(inst_ctx_t *ctx);
	void		(*draw)(inst_ctx_t *ctx, const tui_rect_t *rect,
			    int focused);
	int		(*key)(inst_ctx_t *ctx, const tui_key_t *key);
	int		(*ready)(inst_ctx_t *ctx);
	const char	*(*blocked)(inst_ctx_t *ctx);
} inst_section_t;


int	inst_welcome(void);
const inst_section_t	*inst_sections(int *count);
int	inst_ui_run(inst_ctx_t *ctx);
int	inst_finish(inst_ctx_t *ctx);
void	inst_ui_error(inst_ctx_t *ctx, const char *what);
int	inst_ui_wrap(const char *text, int width, int line, char *out,
	    size_t size);

#endif
