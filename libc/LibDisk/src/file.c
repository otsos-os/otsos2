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
$define %type ldisk_dev_t as an opened block device
$define %type ldisk_fs_t as which filesystem writer to address on a target

$define %func ldisk_fs_cmd as function with args ldisk_fs_t, int, uint64_t *
$define %func ldisk_mkdir as function with args ldisk_dev_t *, ldisk_fs_t, const char *
$define %func ldisk_write as function with args ldisk_dev_t *, ldisk_fs_t, const char *, const void *, uint64_t, uint64_t, uint64_t
$define %func ldisk_truncate as function with args ldisk_dev_t *, const char *, uint64_t
$define %func ldisk_read as function with args ldisk_dev_t *, const char *, void *, uint64_t, uint64_t *

*/

/* !SPACE!

$space %internal ldisk_fs_cmd
$space %export ldisk_mkdir, ldisk_write, ldisk_truncate, ldisk_read

*/

#include <errno.h>
#include <native.h>
#include <string.h>

#include "disk_int.h"


static int
ldisk_fs_cmd(ldisk_fs_t fs, int want_dir, uint64_t *cmd)
{
	switch (fs) {
	case LDISK_FS_CHAINFS:
		*cmd = want_dir ? DIOC_CFS_MKDIR : DIOC_CFS_WRITE;
		return (0);
	case LDISK_FS_FAT32:
		*cmd = want_dir ? DIOC_FAT_MKDIR : DIOC_FAT_WRITE;
		return (0);
	default:
		errno = EINVAL;
		return (-1);
	}
}

int
ldisk_mkdir(ldisk_dev_t *dev, ldisk_fs_t fs, const char *path)
{
	dioc_file_t	req;
	uint64_t	cmd;

	if (!ldisk_dev_ok(dev) || !ldisk_fs_path_ok(path)) {
		return (-1);
	}
	if (ldisk_fs_cmd(fs, 1, &cmd) != 0) {
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	if (ldisk_str_copy(req.path, sizeof(req.path), path) != 0) {
		return (-1);
	}
	return (entityIoctl(dev->handle, cmd, &req));
}


int
ldisk_write(ldisk_dev_t *dev, ldisk_fs_t fs, const char *path,
    const void *data, uint64_t size, uint64_t offset, uint64_t total)
{
	dioc_file_t	req;
	uint64_t	cmd;

	if (!ldisk_dev_ok(dev) || !ldisk_fs_path_ok(path)) {
		return (-1);
	}
	if (ldisk_fs_cmd(fs, 0, &cmd) != 0) {
		return (-1);
	}
	if (data == NULL && size != 0) {
		errno = EINVAL;
		return (-1);
	}
	if (size > DIOC_FILE_MAX) {
		errno = E2BIG;
		return (-1);
	}

	if (total < size || offset > total || offset + size < offset ||
	    offset + size > total) {
		errno = EINVAL;
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	if (ldisk_str_copy(req.path, sizeof(req.path), path) != 0) {
		return (-1);
	}
	req.data = (uint64_t)(uintptr_t)data;
	req.size = size;
	req.offset = offset;
	req.total = total;

	return (entityIoctl(dev->handle, cmd, &req));
}


int
ldisk_truncate(ldisk_dev_t *dev, const char *path, uint64_t size)
{
	dioc_file_t	req;

	if (!ldisk_dev_ok(dev) || !ldisk_fs_path_ok(path)) {
		return (-1);
	}
	if (size > 0xFFFFFFFFULL) {
		errno = EFBIG;
		return (-1);
	}

	memset(&req, 0, sizeof(req));
	if (ldisk_str_copy(req.path, sizeof(req.path), path) != 0) {
		return (-1);
	}
	req.total = size;
	return (entityIoctl(dev->handle, DIOC_CFS_TRUNC, &req));
}

int
ldisk_read(ldisk_dev_t *dev, const char *path, void *buf, uint64_t size,
    uint64_t *got)
{
	dioc_file_t	req;
	int		ret;

	if (!ldisk_dev_ok(dev) || !ldisk_fs_path_ok(path)) {
		return (-1);
	}
	if (buf == NULL || size == 0 || size > DIOC_FILE_MAX) {
		errno = EINVAL;
		return (-1);
	}
	if (got != NULL) {
		*got = 0;
	}

	memset(&req, 0, sizeof(req));
	if (ldisk_str_copy(req.path, sizeof(req.path), path) != 0) {
		return (-1);
	}
	req.data = (uint64_t)(uintptr_t)buf;
	req.size = size;

	ret = entityIoctl(dev->handle, DIOC_CFS_READ, &req);
	if (ret < 0) {

		memset(buf, 0, (size_t)size);
		return (-1);
	}
	if (got != NULL) {
		*got = req.done;
	}
	return (0);
}
