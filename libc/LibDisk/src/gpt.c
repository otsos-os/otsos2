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
$define %type ldisk_dev_t as an opened block device
$define %type ldisk_part_t as one GPT partition request and its placement

$define %func ldisk_whole_disk as function with args const ldisk_dev_t *
$define %func ldisk_gpt_begin as function with args ldisk_dev_t *
$define %func ldisk_gpt_add as function with args ldisk_dev_t *, ldisk_part_t *
$define %func ldisk_gpt_commit as function with args ldisk_dev_t *

*/

/* !SPACE!

$space %internal ldisk_whole_disk
$space %export ldisk_gpt_begin, ldisk_gpt_add, ldisk_gpt_commit

*/

#include <errno.h>
#include <native.h>
#include <string.h>

#include "disk_int.h"


static int
ldisk_whole_disk(const ldisk_dev_t *dev)
{
	if ((dev->info.flags & LDISK_F_SLICE) != 0) {
		errno = EINVAL;
		return (0);
	}
	if ((dev->info.flags & LDISK_F_READONLY) != 0) {
		errno = EROFS;
		return (0);
	}
	return (1);
}

int
ldisk_gpt_begin(ldisk_dev_t *dev)
{
	if (!ldisk_dev_ok(dev) || !ldisk_whole_disk(dev)) {
		return (-1);
	}
	return (entityIoctl(dev->handle, DIOC_GPT_INIT, NULL));
}


int
ldisk_gpt_add(ldisk_dev_t *dev, ldisk_part_t *part)
{
	dioc_gpt_add_t	req;

	if (!ldisk_dev_ok(dev) || !ldisk_whole_disk(dev)) {
		return (-1);
	}
	if (part == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (part->kind != LDISK_PART_ESP && part->kind != LDISK_PART_BIOS &&
	    part->kind != LDISK_PART_ROOT) {
		errno = EINVAL;
		return (-1);
	}
	if (part->size_sectors == 0) {
		errno = EINVAL;
		return (-1);
	}

	if (part->size_sectors == LDISK_PART_REST &&
	    part->kind != LDISK_PART_ROOT) {
		errno = EINVAL;
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	if (ldisk_str_copy(req.name, sizeof(req.name), part->name) != 0) {
		return (-1);
	}
	req.kind = part->kind;
	req.size_sectors = part->size_sectors;

	if (entityIoctl(dev->handle, DIOC_GPT_ADD, &req) != 0) {
		return (-1);
	}

	part->index = req.index;
	part->first_lba = req.first_lba;
	part->last_lba = req.last_lba;
	return (0);
}

int
ldisk_gpt_commit(ldisk_dev_t *dev)
{
	if (!ldisk_dev_ok(dev) || !ldisk_whole_disk(dev)) {
		return (-1);
	}
	if (entityIoctl(dev->handle, DIOC_GPT_COMMIT, NULL) != 0) {
		return (-1);
	}

	if ((dev->info.flags & LDISK_F_NOFLUSH) == 0) {
		(void)ldisk_flush(dev);
	}
	return (0);
}
