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
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_dev_t as an opened block device

$define %func ldisk_str_copy as function with args char *, size_t, const char *
$define %func ldisk_copy_info as procedure with args ldisk_info_t *, const dioc_info_t *
$define %func ldisk_dev_ok as function with args const ldisk_dev_t *
$define %func ldisk_fs_path_ok as function with args const char *
$define %func ldisk_open as function with args const char *, ldisk_dev_t *
$define %func ldisk_close as procedure with args ldisk_dev_t *
$define %func ldisk_refresh as function with args ldisk_dev_t *
$define %func ldisk_probe_fs as function with args ldisk_dev_t *
$define %func ldisk_flush as function with args ldisk_dev_t *
$define %func ldisk_rescan as function with args ldisk_dev_t *
$define %func ldisk_chunk_max as function with args void
$define %func ldisk_boot_sector as function with args void
$define %func ldisk_stage2_max as function with args void

*/

/* !SPACE!

$space %internal ldisk_str_copy, ldisk_copy_info, ldisk_dev_ok
$space %internal ldisk_fs_path_ok
$space %export ldisk_open, ldisk_close, ldisk_refresh, ldisk_probe_fs
$space %export ldisk_flush, ldisk_rescan
$space %export ldisk_chunk_max, ldisk_boot_sector, ldisk_stage2_max

*/

#include <errno.h>
#include <native.h>
#include <string.h>

#include "disk_int.h"

int
ldisk_str_copy(char *dst, size_t size, const char *src)
{
	size_t	len;

	if (dst == NULL || size == 0) {
		return (-1);
	}
	dst[0] = '\0';
	if (src == NULL) {
		return (-1);
	}
	len = strlen(src);
	if (len + 1 > size) {
		errno = E2BIG;
		return (-1);
	}
	memcpy(dst, src, len + 1);
	return (0);
}

void
ldisk_copy_info(ldisk_info_t *out, const dioc_info_t *in)
{
	memset(out, 0, sizeof(*out));
	memcpy(out->name, in->name, LDISK_NAME_MAX - 1);
	memcpy(out->parent, in->parent, LDISK_NAME_MAX - 1);
	memcpy(out->model, in->model, LDISK_MODEL_MAX - 1);
	out->total_sectors = in->total_sectors;
	out->base_lba = in->base_lba;
	out->sector_size = in->sector_size;
	out->max_io_sectors = in->max_io_sectors;
	out->type = in->type;
	out->flags = in->flags;
	out->index = in->index;
}

int
ldisk_dev_ok(const ldisk_dev_t *dev)
{
	if (dev == NULL || dev->handle < 0) {
		errno = EBADF;
		return (0);
	}
	return (1);
}


int
ldisk_fs_path_ok(const char *path)
{
	size_t	len;

	if (path == NULL || path[0] != '/') {
		errno = EINVAL;
		return (0);
	}
	len = strlen(path);
	if (len == 0 || len + 1 > LDISK_FS_PATH_MAX) {
		errno = E2BIG;
		return (0);
	}
	return (1);
}

int
ldisk_open(const char *entity_path, ldisk_dev_t *dev)
{
	int	handle;

	if (dev == NULL) {
		errno = EINVAL;
		return (-1);
	}
	memset(dev, 0, sizeof(*dev));
	dev->handle = -1;
	if (ldisk_str_copy(dev->path, sizeof(dev->path), entity_path) != 0) {
		return (-1);
	}


	handle = entityOpen(dev->path, ENTITY_ACCESS_READ |
	    ENTITY_ACCESS_WRITE);
	if (handle < 0) {
		dev->path[0] = '\0';
		return (-1);
	}
	dev->handle = handle;

	if (ldisk_refresh(dev) != 0) {
		ldisk_close(dev);
		return (-1);
	}
	return (0);
}

void
ldisk_close(ldisk_dev_t *dev)
{
	if (dev == NULL || dev->handle < 0) {
		return;
	}
	(void)entityClose(dev->handle);
	dev->handle = -1;
	dev->path[0] = '\0';
	memset(&dev->info, 0, sizeof(dev->info));
}

int
ldisk_refresh(ldisk_dev_t *dev)
{
	dioc_info_t	raw;

	if (!ldisk_dev_ok(dev)) {
		return (-1);
	}
	memset(&raw, 0, sizeof(raw));
	if (entityIoctl(dev->handle, DIOC_GETINFO, &raw) != 0) {

		return (-1);
	}
	ldisk_copy_info(&dev->info, &raw);
	return (0);
}


int
ldisk_probe_fs(ldisk_dev_t *dev)
{
	dioc_info_t	raw;

	if (!ldisk_dev_ok(dev)) {
		return (-1);
	}
	memset(&raw, 0, sizeof(raw));
	if (entityIoctl(dev->handle, DIOC_PROBEFS, &raw) != 0) {
		return (-1);
	}
	ldisk_copy_info(&dev->info, &raw);
	return (0);
}

int
ldisk_flush(ldisk_dev_t *dev)
{
	if (!ldisk_dev_ok(dev)) {
		return (-1);
	}
	return (entityIoctl(dev->handle, DIOC_FLUSH, NULL));
}

int
ldisk_rescan(ldisk_dev_t *dev)
{
	if (!ldisk_dev_ok(dev)) {
		return (-1);
	}
	if ((dev->info.flags & LDISK_F_SLICE) != 0) {

		errno = EINVAL;
		return (-1);
	}
	return (entityIoctl(dev->handle, DIOC_RESCAN, NULL));
}

uint64_t
ldisk_chunk_max(void)
{
	return (DIOC_FILE_MAX);
}

uint64_t
ldisk_boot_sector(void)
{
	return (DIOC_BOOT_SECTOR);
}

uint64_t
ldisk_stage2_max(void)
{
	return (DIOC_BOOT_STAGE2_MAX);
}
