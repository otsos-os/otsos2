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
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_dev_t as an open block device
$define %type inst_slice_t as one placed partition and its published slice
$define %type inst_ctx_t as installer run state

$define %func inst_add_part as function with args inst_ctx_t *, ldisk_dev_t *, inst_slice_t *, const char *, uint32_t, uint64_t
$define %func inst_bind_slice as function with args inst_ctx_t *, const char *, inst_slice_t *
$define %func inst_partition as function with args inst_ctx_t *

*/

/* !SPACE!

$space %internal inst_add_part, inst_bind_slice
$space %export inst_partition

*/

#include <disk.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"

static int
inst_add_part(inst_ctx_t *ctx, ldisk_dev_t *dev, inst_slice_t *out,
    const char *name, uint32_t kind, uint64_t size_sectors)
{
	memset(out, 0, sizeof(*out));
	out->part.kind = kind;
	out->part.size_sectors = size_sectors;
	(void)snprintf(out->part.name, sizeof(out->part.name), "%s", name);

	if (ldisk_gpt_add(dev, &out->part) != 0) {
		inst_fail(ctx, "cannot place partition %s: %s", name,
		    strerror(errno));
		return (-1);
	}
	if (out->part.first_lba == 0 || out->part.last_lba <
	    out->part.first_lba) {
		inst_fail(ctx, "partition %s got an unusable placement", name);
		return (-1);
	}
	return (0);
}


static int
inst_bind_slice(inst_ctx_t *ctx, const char *parent, inst_slice_t *slice)
{
	if (ldisk_find_slice(parent, slice->part.first_lba,
	    &slice->slice) != 0) {
		inst_fail(ctx, "partition %s was committed but no device "
		    "appeared for it: %s", slice->part.name, strerror(errno));
		slice->present = 0;
		return (-1);
	}
	slice->present = 1;
	return (0);
}

int
inst_partition(inst_ctx_t *ctx)
{
	inst_target_t	*target;
	ldisk_dev_t	dev;
	uint64_t	bios;
	uint64_t	esp;
	int		ret;

	if (ctx == NULL || ctx->selected < 0 ||
	    ctx->selected >= ctx->cand_count) {
		inst_fail(ctx, "no install target selected");
		return (-1);
	}

	target = &ctx->target;
	memset(target, 0, sizeof(*target));
	target->disk = ctx->cands[ctx->selected].entry;

	if (ldisk_open(target->disk.path, &dev) != 0) {
		inst_fail(ctx, "cannot open %s: %s", target->disk.info.name,
		    strerror(errno));
		return (-1);
	}

	bios = inst_sectors(INST_BIOS_BYTES, dev.info.sector_size);
	esp = inst_sectors(ctx->esp_bytes, dev.info.sector_size);
	if (bios == 0 || esp == 0) {
		inst_fail(ctx, "%s reports no usable sector size",
		    target->disk.info.name);
		ldisk_close(&dev);
		return (-1);
	}


	if (ldisk_gpt_begin(&dev) != 0) {
		inst_fail(ctx, "cannot start a new partition table on %s: %s",
		    target->disk.info.name, strerror(errno));
		ldisk_close(&dev);
		return (-1);
	}

	ret = inst_add_part(ctx, &dev, &target->bios, INST_NAME_BIOS,
	    LDISK_PART_BIOS, bios);
	if (ret == 0) {
		ret = inst_add_part(ctx, &dev, &target->esp, INST_NAME_ESP,
		    LDISK_PART_ESP, esp);
	}
	if (ret == 0) {
		ret = inst_add_part(ctx, &dev, &target->root, INST_NAME_ROOT,
		    LDISK_PART_ROOT, LDISK_PART_REST);
	}
	if (ret != 0) {
		ldisk_close(&dev);
		return (-1);
	}

	if (ldisk_gpt_commit(&dev) != 0) {
		inst_fail(ctx, "cannot write the partition table to %s: %s",
		    target->disk.info.name, strerror(errno));
		ldisk_close(&dev);
		return (-1);
	}
	if (ldisk_rescan(&dev) != 0) {
		inst_fail(ctx, "cannot re-read the partition table on %s: %s",
		    target->disk.info.name, strerror(errno));
		ldisk_close(&dev);
		return (-1);
	}
	ldisk_close(&dev);

	if (inst_bind_slice(ctx, target->disk.info.name, &target->bios) != 0 ||
	    inst_bind_slice(ctx, target->disk.info.name, &target->esp) != 0 ||
	    inst_bind_slice(ctx, target->disk.info.name, &target->root) != 0) {
		return (-1);
	}

	target->partitioned = 1;
	return (0);
}
