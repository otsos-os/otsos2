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
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_dev_t as an opened block device

$define %func ldisk_writable as function with args const ldisk_dev_t *
$define %func ldisk_mkfs_chainfs as function with args ldisk_dev_t *, uint64_t, uint32_t
$define %func ldisk_mkfs_fat32 as function with args ldisk_dev_t *, const char *

*/

/* !SPACE!

$space %internal ldisk_writable
$space %export ldisk_mkfs_chainfs, ldisk_mkfs_fat32

*/

#include <errno.h>
#include <native.h>
#include <string.h>

#include "disk_int.h"


static int
ldisk_writable(const ldisk_dev_t *dev)
{
	if ((dev->info.flags & LDISK_F_READONLY) != 0) {
		errno = EROFS;
		return (0);
	}
	if ((dev->info.flags & LDISK_F_ROOT) != 0) {
		errno = EBUSY;
		return (0);
	}
	if (dev->info.sector_size == 0 || dev->info.total_sectors == 0) {
		errno = ENODEV;
		return (0);
	}
	return (1);
}


int
ldisk_mkfs_chainfs(ldisk_dev_t *dev, uint64_t total_blocks, uint32_t max_files)
{
	dioc_mkfs_chainfs_t	req;

	if (!ldisk_dev_ok(dev) || !ldisk_writable(dev)) {
		return (-1);
	}
	if (max_files == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (total_blocks == 0) {
		total_blocks = dev->info.total_sectors;
	}
	if (total_blocks > dev->info.total_sectors) {
		errno = ENOSPC;
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	req.total_blocks = total_blocks;
	req.max_files = max_files;

	if (entityIoctl(dev->handle, DIOC_MKFS_CHAINFS, &req) != 0) {
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_NOFLUSH) == 0) {
		(void)ldisk_flush(dev);
	}

	(void)ldisk_probe_fs(dev);
	return (0);
}

int
ldisk_mkfs_fat32(ldisk_dev_t *dev, const char *label)
{
	dioc_mkfs_fat32_t	req;

	if (!ldisk_dev_ok(dev) || !ldisk_writable(dev)) {
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	if (label != NULL && label[0] != '\0') {
		if (ldisk_str_copy(req.label, sizeof(req.label), label) != 0) {
			return (-1);
		}
	}

	if (entityIoctl(dev->handle, DIOC_MKFS_FAT32, &req) != 0) {
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_NOFLUSH) == 0) {
		(void)ldisk_flush(dev);
	}
	(void)ldisk_probe_fs(dev);
	return (0);
}
