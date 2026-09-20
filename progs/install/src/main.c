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

$define %func main as function with args int, char **

*/

/* !SPACE!

$space %export main

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tui.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"
#include "ui.h"

int
main(int argc, char **argv)
{
	inst_ctx_t	*ctx;
	int		ret;

	(void)argc;
	(void)argv;


	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
		printf("install: out of memory\n");
		return (1);
	}
	ctx->selected = -1;
	ctx->esp_bytes = INST_ESP_BYTES;
	ctx->root_max_files = INST_ROOT_MAX_FILES;

	if (inst_plan_load(ctx) != 0) {
		printf("install: %s\n", inst_error(ctx));
		free(ctx);
		return (1);
	}

	if (tui_init() != 0) {
		printf("install: cannot initialise the terminal\n");
		free(ctx);
		return (1);
	}

	if (!inst_welcome()) {
		tui_fini();
		free(ctx);
		return (0);
	}

	ret = inst_ui_run(ctx);
	if (ret != 0) {

		if (ctx->target.partitioned)
			tui_msgbox("Installation abandoned",
			    "The target disk was already repartitioned, so it "
			    "carries an incomplete system. Run the installer "
			    "again to finish it.");
		tui_fini();
		free(ctx);
		return (1);
	}

	ret = inst_finish(ctx);
	if (ret == 0)
		tui_fini();
	free(ctx);
	return (0);
}
