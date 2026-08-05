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

#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/driver_ns.h>
#include <kernel/api/api.h>
#include <kernel/other/restrict.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

#define	API_FS_LISTDIR_BATCH	32
#define	API_FS_LISTDIR_MAX	4096U
#define	API_FS_PATH_MAX		256

static int
copy_user_path(const char *path, char *out, int *use_cwd)
{
	int	len;

	if (!out || !use_cwd) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!path) {
		out[0] = '\0';
		*use_cwd = 1;
		return (0);
	}
	if (!is_user_address(path, 1) ||
	    !user_range_fault_in(path, 1, 0)) {
		return (-API_ERR_BAD_ADDR);
	}

	len = 0;
	while (len < API_FS_PATH_MAX - 1) {
		if (!is_user_address(path + len, 1) ||
		    !user_range_fault_in(path + len, 1, 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		out[len] = path[len];
		if (out[len] == '\0') {
			*use_cwd = (len == 0);
			return (0);
		}
		len++;
	}

	out[0] = '\0';
	return (-API_ERR_TOO_BIG);
}

static u8
api_fs_dtype(int type)
{
	switch (type) {
	case VREG:
		return (API_FS_TYPE_REG);
	case VDIR:
		return (API_FS_TYPE_DIR);
	case VCHR:
		return (API_FS_TYPE_CHR);
	case VPIPE:
		return (API_FS_TYPE_PIPE);
	case VLNK:
		return (API_FS_TYPE_LNK);
	default:
		return (0);
	}
}

static void
copy_dirent_to_user(struct api_dirent *dst, const vfs_dirent_t *src)
{
	int	i;

	memset(dst, 0, sizeof(*dst));
	for (i = 0; i < 31 && src->name[i] != '\0'; i++) {
		dst->name[i] = src->name[i];
	}
	dst->name[i] = '\0';
	dst->type = api_fs_dtype(src->type);
}

int
api_fs_listdir(const char *path, struct api_dirent *buf, u32 max_entries)
{
	vfs_dirent_t	entries[API_FS_LISTDIR_BATCH];
	vfs_dirent_t	root_entry;
	vnode_t		*vn;
	size_t		user_size;
	u32		listed, count, want, i;
	char		kpath[API_FS_PATH_MAX];
	int		use_cwd, ret;

	if (!buf || max_entries == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (max_entries > API_FS_LISTDIR_MAX) {
		return (-API_ERR_TOO_BIG);
	}

	user_size = (size_t)max_entries * sizeof(struct api_dirent);
	if (!is_user_address(buf, user_size) ||
	    !user_range_fault_in(buf, user_size, 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	ret = copy_user_path(path, kpath, &use_cwd);
	if (ret != 0) {
		return (ret);
	}
	if (use_cwd) {
		ret = vfs_getcwd(kpath, sizeof(kpath));
		if (ret != 0) {
			return (ret);
		}
	}
	if (restrict_kusr_check(kpath)) {
		return (-API_ERR_PERM);
	}

	vn = NULL;
	if (driver_ns_is_path(kpath)) {
		vn = driver_ns_lookup(kpath);
		if (vn == NULL) {
			return (-API_ERR_NOT_FOUND);
		}
	} else {
		ret = vfs_resolve(kpath, &vn);
		if (ret != 0) {
			return (ret);
		}
		if (vn == NULL) {
			return (-API_ERR_NOT_FOUND);
		}
	}
	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-API_ERR_NOT_DIR);
	}

	listed = 0;
	while (listed < max_entries) {
		want = max_entries - listed;
		if (want > API_FS_LISTDIR_BATCH) {
			want = API_FS_LISTDIR_BATCH;
		}

		count = 0;
		ret = vnode_listdir(vn, listed, entries, want, &count);
		if (ret != 0) {
			vnode_release(vn);
			return (ret);
		}
		if (count == 0) {
			break;
		}
		if (count > want) {
			count = want;
		}

		for (i = 0; i < count; i++) {
			copy_dirent_to_user(&buf[listed + i], &entries[i]);
		}
		listed += count;
	}

	if (strcmp(kpath, "/") == 0 && listed < max_entries) {
		memset(&root_entry, 0, sizeof(root_entry));
		memcpy(root_entry.name, "Driver", 7);
		root_entry.type = VDIR;
		copy_dirent_to_user(&buf[listed], &root_entry);
		listed++;
	}

	vnode_release(vn);
	return ((int)listed);
}
