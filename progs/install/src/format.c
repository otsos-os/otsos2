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
$define %type inst_ctx_t as installer run state

$define %func inst_format as function with args inst_ctx_t *

*/

/* !SPACE!

$space %export inst_format

*/

#include <disk.h>
#include <errno.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"

int
inst_format(inst_ctx_t *ctx)
{
	inst_target_t	*target;
	ldisk_dev_t	dev;

	if (ctx == NULL) {
		return (-1);
	}
	target = &ctx->target;
	if (target->partitioned == 0 || target->root.present == 0 ||
	    target->esp.present == 0) {
		inst_fail(ctx, "target is not partitioned");
		return (-1);
	}


	if (ldisk_open(target->root.slice.path, &dev) != 0) {
		inst_fail(ctx, "cannot open the root partition: %s",
		    strerror(errno));
		return (-1);
	}
	if (ldisk_mkfs_chainfs(&dev, 0, ctx->root_max_files) != 0) {
		inst_fail(ctx, "cannot create a ChainFS on the root partition: "
		    "%s", strerror(errno));
		ldisk_close(&dev);
		return (-1);
	}
	ldisk_close(&dev);

	if (ldisk_open(target->esp.slice.path, &dev) != 0) {
		inst_fail(ctx, "cannot open the EFI system partition: %s",
		    strerror(errno));
		return (-1);
	}
	if (ldisk_mkfs_fat32(&dev, INST_LABEL_ESP) != 0) {
		inst_fail(ctx, "cannot create a FAT32 filesystem on the EFI "
		    "system partition: %s", strerror(errno));
		ldisk_close(&dev);
		return (-1);
	}
	ldisk_close(&dev);

	return (0);
}
