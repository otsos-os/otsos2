/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

$define %func copy_user_path as function with args const char *, char *
$define %func api_fs_linknew as function with args const char *, const char *
$define %func api_fs_linkgo as function with args const char *, char *, u32

*/

/* !SPACE!

$space %internal copy_user_path
$space %export api_fs_linknew, api_fs_linkgo

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
api_fs_linknew(const char *target, const char *linkpath)
{
	char	ktarget[256];
	char	klinkpath[256];
	int	ret;

	ret = copy_user_path(target, ktarget);
	if (ret != 0) {
		return (ret);
	}
	ret = copy_user_path(linkpath, klinkpath);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(ktarget) || restrict_kusr_check(klinkpath)) {
		return (-API_ERR_PERM);
	}
	if (vfs_symlink(ktarget, klinkpath) != 0) {
		return (-API_ERR_IO);
	}
	return (0);
}

int
api_fs_linkgo(const char *path, char *buf, u32 bufsize)
{
	char	kpath[256];
	char	ktarget[256];
	int	ret;
	u32	len;
	if (!buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!is_user_address(buf, bufsize)) {
		return (-API_ERR_BAD_ADDR);
	}

	ret = copy_user_path(path, kpath);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(kpath)) {
		return (-API_ERR_PERM);
	}
	if (vfs_readlink(kpath, ktarget, sizeof(ktarget)) != 0) {
		return (-API_ERR_NOT_FOUND);
	}
	ktarget[sizeof(ktarget) - 1] = '\0';
	len = strlen(ktarget);
	if (len >= bufsize) {
		memcpy(buf, ktarget, bufsize);
		return (-API_ERR_TOO_BIG);
	}
	memcpy(buf, ktarget, len + 1);
	return (0);
}
