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
$define %type inst_ctx_t as installer run state

$define %func inst_slurp as function with args inst_ctx_t *, const char *, void **, uint64_t *
$define %func inst_boot_bios as function with args inst_ctx_t *
$define %func inst_boot_uefi as function with args inst_ctx_t *
$define %func inst_boot as function with args inst_ctx_t *

*/

/* !SPACE!

$space %internal inst_slurp, inst_boot_bios, inst_boot_uefi
$space %export inst_boot

*/

#include <disk.h>
#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "plan.h"
#include "stage.h"

static const char	*const inst_esp_dirs[] = { "/EFI", "/EFI/BOOT" };
#define INST_ESP_BOOTX64	"/EFI/BOOT/BOOTX64.EFI"


static int
inst_slurp(inst_ctx_t *ctx, const char *path, uint64_t cap, void **out,
    uint64_t *size)
{
	struct api_fs_stat	st;
	unsigned char		*buf;
	uint64_t		done;
	ssize_t			got;
	int			fd;

	*out = NULL;
	*size = 0;

	memset(&st, 0, sizeof(st));
	if (fsStat(path, &st) != 0) {
		inst_fail(ctx, "cannot stat %s: %s", path, strerror(errno));
		return (-1);
	}
	if (st.type != API_FS_TYPE_REG || st.size == 0) {
		inst_fail(ctx, "%s is not a usable file", path);
		return (-1);
	}
	if (st.size > cap) {
		inst_fail(ctx, "%s is %llu bytes, over the %llu byte limit",
		    path, (unsigned long long)st.size,
		    (unsigned long long)cap);
		return (-1);
	}

	buf = malloc((size_t)st.size);
	if (buf == NULL) {
		inst_fail(ctx, "out of memory reading %s", path);
		return (-1);
	}
	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		inst_fail(ctx, "cannot open %s: %s", path, strerror(errno));
		free(buf);
		return (-1);
	}

	done = 0;
	while (done < st.size) {
		got = dataRead(fd, buf + done, (size_t)(st.size - done));
		if (got < 0) {
			inst_fail(ctx, "cannot read %s: %s", path,
			    strerror(errno));
			(void)dataClose(fd);
			free(buf);
			return (-1);
		}
		if (got == 0) {
			inst_fail(ctx, "%s ended early at %llu of %llu bytes",
			    path, (unsigned long long)done,
			    (unsigned long long)st.size);
			(void)dataClose(fd);
			free(buf);
			return (-1);
		}
		done += (uint64_t)got;
	}
	(void)dataClose(fd);

	*out = buf;
	*size = st.size;
	return (0);
}


static int
inst_boot_bios(inst_ctx_t *ctx)
{
	const inst_module_t	*m1;
	const inst_module_t	*m2;
	ldisk_dev_t		dev;
	void			*stage1;
	void			*stage2;
	uint64_t		size1;
	uint64_t		size2;
	int			ret;

	m1 = inst_module_by_role(&ctx->plan, INST_ROLE_STAGE1);
	m2 = inst_module_by_role(&ctx->plan, INST_ROLE_STAGE2);
	if (m1 == NULL || m2 == NULL) {
		inst_fail(ctx, "this build ships no BIOS boot blocks; clear "
		    "the BIOS option or build with them");
		return (-1);
	}
	if (ctx->target.bios.present == 0) {
		inst_fail(ctx, "the target has no BIOS boot partition");
		return (-1);
	}

	stage1 = NULL;
	stage2 = NULL;
	if (inst_slurp(ctx, m1->dest, ldisk_boot_sector(), &stage1,
	    &size1) != 0) {
		return (-1);
	}
	if (inst_slurp(ctx, m2->dest, ldisk_stage2_max(), &stage2,
	    &size2) != 0) {
		free(stage1);
		return (-1);
	}

	if (ldisk_open(ctx->target.disk.path, &dev) != 0) {
		inst_fail(ctx, "cannot open %s: %s", ctx->target.disk.info.name,
		    strerror(errno));
		free(stage1);
		free(stage2);
		return (-1);
	}
	ret = ldisk_boot_install(&dev, stage1, size1, stage2, size2,
	    ctx->target.bios.part.first_lba);
	if (ret != 0) {
		inst_fail(ctx, "cannot install the BIOS boot blocks: %s",
		    strerror(errno));
	}
	ldisk_close(&dev);
	free(stage1);
	free(stage2);
	return (ret);
}


static int
inst_boot_uefi(inst_ctx_t *ctx)
{
	const inst_module_t	*mod;
	ldisk_dev_t		dev;
	void			*image;
	uint64_t		size;
	size_t			i;
	int			ret;

	mod = inst_module_by_role(&ctx->plan, INST_ROLE_UEFI);
	if (mod == NULL) {
		inst_fail(ctx, "this build ships no UEFI loader; clear the "
		    "UEFI option or build with one");
		return (-1);
	}
	if (ctx->target.esp.present == 0) {
		inst_fail(ctx, "the target has no EFI system partition");
		return (-1);
	}

	image = NULL;
	if (inst_slurp(ctx, mod->dest, ldisk_chunk_max(), &image, &size) != 0) {
		return (-1);
	}

	if (ldisk_open(ctx->target.esp.slice.path, &dev) != 0) {
		inst_fail(ctx, "cannot open the EFI system partition: %s",
		    strerror(errno));
		free(image);
		return (-1);
	}

	ret = 0;
	for (i = 0; i < sizeof(inst_esp_dirs) / sizeof(inst_esp_dirs[0]);
	    i++) {
		if (ldisk_mkdir(&dev, LDISK_FS_FAT32,
		    inst_esp_dirs[i]) != 0) {
			inst_fail(ctx, "cannot create %s on the EFI system "
			    "partition: %s", inst_esp_dirs[i],
			    strerror(errno));
			ret = -1;
			break;
		}
	}
	if (ret == 0 && ldisk_write(&dev, LDISK_FS_FAT32, INST_ESP_BOOTX64,
	    image, size, 0, size) != 0) {
		inst_fail(ctx, "cannot write %s: %s", INST_ESP_BOOTX64,
		    strerror(errno));
		ret = -1;
	}
	if (ret == 0 && ldisk_flush(&dev) != 0) {
		inst_fail(ctx, "cannot flush the EFI system partition: %s",
		    strerror(errno));
		ret = -1;
	}

	ldisk_close(&dev);
	free(image);
	return (ret);
}

int
inst_boot(inst_ctx_t *ctx)
{
	if (ctx == NULL) {
		return (-1);
	}
	if (ctx->target.partitioned == 0) {
		inst_fail(ctx, "target is not partitioned");
		return (-1);
	}
	if (ctx->want_bios == 0 && ctx->want_uefi == 0) {
		inst_fail(ctx, "no boot method selected; the target would not "
		    "be bootable");
		return (-1);
	}


	if (ctx->want_bios != 0 && inst_boot_bios(ctx) != 0) {
		return (-1);
	}
	if (ctx->want_uefi != 0 && inst_boot_uefi(ctx) != 0) {
		return (-1);
	}

	ctx->boot_done = 1;
	return (0);
}
