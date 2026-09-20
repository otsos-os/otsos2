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
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_entry_t as one enumerated block device
$define %type inst_cand_t as one install target candidate
$define %type inst_ctx_t as installer run state

$define %func inst_cand_reject as function with args inst_cand_t *, const char *
$define %func inst_cand_describe as function with args inst_cand_t *
$define %func inst_scan as function with args inst_ctx_t *

*/

/* !SPACE!

$space %internal inst_cand_reject, inst_cand_describe
$space %export inst_scan

*/

#include <disk.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"


static void
inst_cand_reject(inst_cand_t *cand, const char *reason)
{
	if (cand->usable == 0) {
		return;
	}
	cand->usable = 0;
	(void)snprintf(cand->reason, sizeof(cand->reason), "%s", reason);
}


static void
inst_cand_describe(inst_cand_t *cand)
{
	const ldisk_info_t	*info;
	char			size[48];

	info = &cand->entry.info;
	inst_size_label(size, sizeof(size), info->total_sectors,
	    info->sector_size);

	if (cand->usable != 0) {
		(void)snprintf(cand->label, sizeof(cand->label), "%-12s %s",
		    info->name, size);
	} else {
		(void)snprintf(cand->label, sizeof(cand->label),
		    "%-12s %s  (%s)", info->name, size, cand->reason);
	}
}

int
inst_scan(inst_ctx_t *ctx)
{
	ldisk_entry_t	*list;
	inst_cand_t	*cand;
	uint64_t	min;
	int		count;
	int		usable;
	int		i;

	if (ctx == NULL) {
		return (-1);
	}

	ctx->cand_count = 0;
	ctx->selected = -1;
	memset(ctx->cands, 0, sizeof(ctx->cands));


	list = calloc(LDISK_DEVS_MAX, sizeof(*list));
	if (list == NULL) {
		inst_fail(ctx, "out of memory listing block devices");
		return (-1);
	}

	count = 0;
	if (ldisk_enumerate(list, LDISK_DEVS_MAX, &count) != 0) {
		inst_fail(ctx, "cannot enumerate block devices: %s",
		    strerror(errno));
		free(list);
		return (-1);
	}
	if (count <= 0) {
		inst_fail(ctx, "no block devices found");
		free(list);
		return (-1);
	}

	usable = 0;
	for (i = 0; i < count && ctx->cand_count < INST_CANDS_MAX; i++) {
		cand = &ctx->cands[ctx->cand_count];
		memset(cand, 0, sizeof(*cand));
		cand->entry = list[i];
		cand->usable = 1;


		if ((cand->entry.info.flags & LDISK_F_SLICE) != 0) {
			continue;
		}

		if ((cand->entry.info.flags & LDISK_F_ROOT) != 0) {
			inst_cand_reject(cand, "live boot medium");
		}
		if ((cand->entry.info.flags & LDISK_F_READONLY) != 0) {
			inst_cand_reject(cand, "read-only");
		}
		if (cand->entry.info.sector_size == 0 ||
		    cand->entry.info.total_sectors == 0) {
			inst_cand_reject(cand, "no usable geometry");
		} else {
			min = inst_sectors(INST_MIN_BYTES,
			    cand->entry.info.sector_size);
			if (cand->entry.info.total_sectors < min) {
				inst_cand_reject(cand, "too small");
			}
		}

		if (cand->usable != 0) {
			usable++;
		}
		inst_cand_describe(cand);
		ctx->cand_count++;
	}
	free(list);

	if (ctx->cand_count == 0) {
		inst_fail(ctx, "no whole block devices found");
		return (-1);
	}
	if (usable == 0) {
		inst_fail(ctx, "no device can be installed to; every candidate "
		    "was rejected");
		return (-1);
	}


	for (i = 0; i < ctx->cand_count; i++) {
		if (ctx->cands[i].usable != 0) {
			ctx->selected = i;
			break;
		}
	}
	return (0);
}
