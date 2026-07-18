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

$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func copy_user_path as function with args const char *, char *
$define %func api_data_dir as function with args u32, const char *, const char *

*/

/* !SPACE!

$space %internal copy_user_path
$space %export api_data_dir

*/

#include <kernel/api/api.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/other/restrict.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static int
copy_user_path(const char *path, char *out)
{
	int	len;

	if (!path || !is_user_address(path, 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	len = 0;
	while (len < 255) {
		if (!is_user_address(path + len, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		out[len] = path[len];
		if (path[len] == '\0') {
			if (len == 0) {
				return (-API_ERR_BAD_VALUE);
			}
			return (0);
		}
		len++;
	}

	out[0] = '\0';
	return (-API_ERR_TOO_BIG);
}

int
api_data_dir(u32 op, const char *path, const char *newpath)
{
	char	kpath[256];
	char	knewpath[256];
	int	ret;

	ret = copy_user_path(path, kpath);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(kpath)) {
		return (-API_ERR_PERM);
	}

	switch (op) {
	case API_DATA_DIR_MKDIR:
		if (vfs_mkdir(kpath) != 0) {
			return (-API_ERR_IO);
		}
		return (0);
	case API_DATA_DIR_RMDIR:
		if (vfs_rmdir(kpath) != 0) {
			return (-API_ERR_NOT_FOUND);
		}
		return (0);
	case API_DATA_DIR_RENAME:
		ret = copy_user_path(newpath, knewpath);
		if (ret != 0) {
			return (ret);
		}
		if (restrict_kusr_check(knewpath)) {
			return (-API_ERR_PERM);
		}
		if (vfs_rename(kpath, knewpath) != 0) {
			return (-API_ERR_NOT_FOUND);
		}
		return (0);
	default:
		return (-API_ERR_BAD_VALUE);
	}
}
