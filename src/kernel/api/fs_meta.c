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

$define %type api_fs_stat as struct with native file metadata
$define %type posix_stat_t as struct with VFS stat result
$define %type vnode_t as struct with VFS node state
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed

$define %func copy_user_path as function with args const char *, char *
$define %func api_fs_stat as function with args const char *, api_fs_stat *
$define %func api_fs_rename as function with args const char *, const char *
$define %func api_fs_unlink as function with args const char *

*/

/* !SPACE!

$space %internal copy_user_path
$space %export api_fs_stat, api_fs_rename, api_fs_unlink

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
	return (-API_ERR_BAD_VALUE);
}

int
api_fs_stat(const char *path, struct api_fs_stat *buf)
{
	struct api_fs_stat	kst;
	posix_stat_t		st;
	vnode_t			*vn;
	char			kpath[256];
	int			ret;

	if (!buf || !is_user_address(buf, sizeof(*buf))) {
		return (-API_ERR_BAD_ADDR);
	}

	ret = copy_user_path(path, kpath);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(kpath)) {
		return (-API_ERR_PERM);
	}

	ret = vfs_resolve(kpath, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (vn == NULL) {
		return (-API_ERR_NOT_FOUND);
	}
	ret = vnode_stat(vn, &st);
	if (ret != 0) {
		vnode_release(vn);
		return (ret);
	}

	memset(&kst, 0, sizeof(kst));
	kst.type = (u32)vn->type;
	kst.mode = st.st_mode;
	kst.uid = st.st_uid;
	kst.gid = st.st_gid;
	kst.size = (u64)st.st_size;
	kst.blocks = (u64)st.st_blocks;
	kst.atime = st.st_atime;
	kst.mtime = st.st_mtime;
	kst.ctime = st.st_ctime;
	memcpy(kst.name, vn->name, sizeof(kst.name) - 1);

	vnode_release(vn);
	memcpy(buf, &kst, sizeof(kst));
	return (0);
}

int
api_fs_rename(const char *oldpath, const char *newpath)
{
	char	kold[256];
	char	knew[256];
	int	ret;

	ret = copy_user_path(oldpath, kold);
	if (ret != 0) {
		return (ret);
	}
	ret = copy_user_path(newpath, knew);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(kold) || restrict_kusr_check(knew)) {
		return (-API_ERR_PERM);
	}
	ret = vfs_rename(kold, knew);
	if (ret != 0) {
		return (ret);
	}
	return (0);
}

int
api_fs_unlink(const char *path)
{
	char	kpath[256];
	int	ret;

	ret = copy_user_path(path, kpath);
	if (ret != 0) {
		return (ret);
	}
	if (restrict_kusr_check(kpath)) {
		return (-API_ERR_PERM);
	}
	ret = vfs_unlink(kpath);
	if (ret != 0) {
		return (ret);
	}
	return (0);
}
