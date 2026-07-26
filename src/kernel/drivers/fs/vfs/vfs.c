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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vfs_dirent_t as VFS directory entry
$define %type vnode_t as VFS vnode
$define %type vfs_back_ops as backend operation table forward declaration
$define %type posix_stat_t as POSIX stat fields

$define %func vfs_init as procedure with args void
$define %func vfs_is_initialized as function with args void
$define %func vfs_mount as function with args const char *, const struct vfs_back_ops *
$define %func vfs_mount_named as function with args const char *, const char *, u64
$define %func vfs_umount as function with args const char *
$define %func vfs_unmount as function with args const char *
$define %func vfs_mount_can_exec as function with args const char *
$define %func vnode_alloc as function with args int, const char *
$define %func vnode_acquire as function with args vnode_t *
$define %func vnode_release as procedure with args vnode_t *
$define %func vnode_can_exec as function with args vnode_t *
$define %func vnode_read as function with args vnode_t *, void *, u64, u64
$define %func vnode_write as function with args vnode_t *, const void *, u64, u64
$define %func vnode_stat as function with args vnode_t *, posix_stat_t *
$define %func vnode_readdir as function with args vnode_t *, u32, char *, int *
$define %func vnode_listdir as function with args vnode_t *, u32, vfs_dirent_t *, u32, u32 *
$define %func vnode_ioctl as function with args vnode_t *, u64, void *
$define %func vnode_readlink as function with args vnode_t *, char *, size_t
$define %func vfs_resolve as function with args const char *, vnode_t **
$define %func vfs_resolve_nofollow as function with args const char *, vnode_t **
$define %func vfs_create_file as function with args const char *
$define %func vfs_mkdir as function with args const char *
$define %func vfs_rmdir as function with args const char *
$define %func vfs_unlink as function with args const char *
$define %func vfs_rename as function with args const char *, const char *
$define %func vfs_truncate as function with args const char *, u64
$define %func vfs_symlink as function with args const char *, const char *
$define %func vfs_link as function with args const char *, const char *
$define %func vfs_readlink as function with args const char *, char *, size_t
$define %func vfs_chdir as function with args const char *
$define %func vfs_getcwd as function with args char *, u32
$define %func vfs_read_file_full as function with args const char *, u8 *, u32, u32 *
$define %func vfs_write_file as function with args const char *, const u8 *, u32

*/

/* !SPACE!

$space %export vfs_init, vfs_is_initialized
$space %export vfs_mount, vfs_mount_named
$space %export vfs_umount, vfs_unmount, vfs_mount_can_exec
$space %export vnode_alloc, vnode_acquire, vnode_release, vnode_can_exec
$space %export vnode_read, vnode_write, vnode_stat, vnode_readdir
$space %export vnode_listdir, vnode_ioctl, vnode_readlink
$space %export vfs_resolve, vfs_resolve_nofollow, vfs_create_file
$space %export vfs_mkdir, vfs_rmdir, vfs_unlink, vfs_rename
$space %export vfs_truncate, vfs_symlink, vfs_link, vfs_readlink
$space %export vfs_chdir, vfs_getcwd
$space %export vfs_read_file_full, vfs_write_file

*/

#include <kernel/api/posix/posix.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/fs/vfs/back/vfs_back.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

static vnode_t	vnode_pool[VFS_MAX_VNODES];
static int	vfs_initialized;

void
vfs_init(void)
{
	int	i;

	for (i = 0; i < VFS_MAX_VNODES; i++) {
		memset(&vnode_pool[i], 0, sizeof(vnode_t));
	}

	if (vfs_back_init() != 0) {
		drivers_log("[VFS] backend init failed\n");
		return;
	}

	vfs_initialized = 1;
	drivers_log("[VFS] initialized (vnode pool: %d slots)\n",
	    VFS_MAX_VNODES);
}

int
vfs_is_initialized(void)
{
	return (vfs_initialized);
}

int
vfs_mount(const char *path, const struct vfs_back_ops *ops)
{
	vnode_t	*vn;
	int	ret;

	if (!vfs_initialized || !path || !ops || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strcmp(path, "/") == 0) {
		return (-API_ERR_BUSY);
	}
	ret = vfs_resolve_nofollow(path, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-API_ERR_NOT_DIR);
	}

	vnode_release(vn);
	return (vfs_back_mount(path, ops));
}

int
vfs_mount_named(const char *path, const char *fstype, u64 flags)
{
	vnode_t	*vn;
	int	ret;

	if (!vfs_initialized || !path || !fstype || path[0] == '\0' ||
	    fstype[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if ((flags & ~VFS_MNT_SUPPORTED) != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (strcmp(path, "/") == 0) {
		return (-API_ERR_BUSY);
	}
	ret = vfs_resolve_nofollow(path, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-API_ERR_NOT_DIR);
	}

	vnode_release(vn);
	return (vfs_back_mount_named(path, fstype, flags));
}

int
vfs_umount(const char *path)
{
	if (!vfs_initialized || !path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	return (vfs_back_umount(path));
}

int
vfs_unmount(const char *path)
{
	return (vfs_umount(path));
}

int
vfs_mount_can_exec(const char *path)
{
	vnode_t	*vn;
	int	ok, ret;

	if (!vfs_initialized || !path || path[0] == '\0') {
		return (0);
	}
	ret = vfs_resolve(path, &vn);
	if (ret != 0 || !vn) {
		return (0);
	}
	ok = vnode_can_exec(vn);
	vnode_release(vn);
	return (ok);
}

vnode_t *
vnode_alloc(int type, const char *name)
{
	vnode_t	*vn;
	int	i, j;

	vn = NULL;
	for (i = 0; i < VFS_MAX_VNODES; i++) {
		if (vnode_pool[i].refcount == 0) {
			vn = &vnode_pool[i];
			break;
		}
	}
	if (!vn) {
		return (NULL);
	}

	memset(vn, 0, sizeof(vnode_t));
	vn->type = type;
	vn->refcount = 1;
	vn->data_owned = 0;
	vn->uid = 0;
	vn->gid = 0;

	if (name) {
		for (j = 0; j < 31 && name[j] != '\0'; j++) {
			vn->name[j] = name[j];
		}
		vn->name[j] = '\0';
	}

	switch (type) {
	case VREG:
		vn->mode = POSIX_S_IFREG | 0644;
		break;
	case VDIR:
		vn->mode = POSIX_S_IFDIR | 0755;
		break;
	case VCHR:
		vn->mode = POSIX_S_IFCHR | 0666;
		break;
	case VPIPE:
		vn->mode = POSIX_S_IFIFO | 0600;
		break;
	case VLNK:
		vn->mode = POSIX_S_IFLNK | 0777;
		break;
	case VSOCK:
		vn->mode = POSIX_S_IFSOCK | 0600;
		break;
	default:
		vn->mode = 0;
		break;
	}

	return (vn);
}

vnode_t *
vnode_acquire(vnode_t *vn)
{
	if (vn && vn->refcount > 0) {
		vn->refcount++;
		vfs_back_mount_ref(vn->mount_id);
	}
	return (vn);
}

void
vnode_release(vnode_t *vn)
{
	u32	mount_id;

	if (!vn) {
		return;
	}
	if (vn->refcount <= 0) {
		return;
	}

	mount_id = vn->mount_id;
	vn->refcount--;
	vfs_back_mount_unref(mount_id);
	if (vn->refcount > 0) {
		return;
	}

	if (vn->data_owned && vn->data) {
		kmem_free(vn->data);
	}
	memset(vn, 0, sizeof(vnode_t));
}

int
vnode_can_exec(vnode_t *vn)
{
	if (!vfs_initialized || !vn) {
		return (0);
	}
	return (vfs_back_mount_can_exec_id(vn->mount_id));
}

int
vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	if (!vn || !buf) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!vn->read_fn) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}
	return (vn->read_fn(vn, buf, count, offset));
}

int
vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	if (!vn || !buf) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!vn->write_fn) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (!vfs_back_mount_can_write(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}
	return (vn->write_fn(vn, buf, count, offset));
}

int
vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	if (!vn || !st) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}

	if (vn->stat_fn) {
		return (vn->stat_fn(vn, st));
	}

	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = vn->mode;
	st->st_size = (s64)vn->size;
	st->st_blksize = 512;
	st->st_blocks = (s64)((vn->size + 511) / 512);
	st->st_nlink = 1;
	st->st_uid = vn->uid;
	st->st_gid = vn->gid;
	return (0);
}

int
vnode_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	vfs_dirent_t	entry;
	u32		count;
	int		ret;

	if (!vn || !name) {
		return (-API_ERR_BAD_VALUE);
	}
	if (vn->type != VDIR) {
		return (-API_ERR_NOT_DIR);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}
	if (!vn->readdir_fn && !vn->listdir_fn) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (vn->readdir_fn) {
		return (vn->readdir_fn(vn, index, name, type));
	}

	count = 0;
	ret = vn->listdir_fn(vn, index, &entry, 1, &count);
	if (ret != 0) {
		return (ret);
	}
	if (count == 0) {
		return (0);
	}
	memcpy(name, entry.name, sizeof(entry.name));
	name[sizeof(entry.name) - 1] = '\0';
	if (type) {
		*type = entry.type;
	}
	return (1);
}

int
vnode_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	char	name[32];
	u32	i, out;
	int	type, ret;

	if (!vn || !entries || !count) {
		return (-API_ERR_BAD_VALUE);
	}
	if (vn->type != VDIR) {
		return (-API_ERR_NOT_DIR);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}

	*count = 0;
	if (max_entries == 0) {
		return (0);
	}
	if (vn->listdir_fn) {
		return (vn->listdir_fn(vn, start, entries, max_entries,
		    count));
	}
	if (!vn->readdir_fn) {
		return (-API_ERR_NOT_SUPPORTED);
	}

	out = 0;
	for (i = 0; i < max_entries; i++) {
		memset(name, 0, sizeof(name));
		type = 0;
		ret = vn->readdir_fn(vn, start + i, name, &type);
		if (ret < 0) {
			return (ret);
		}
		if (ret == 0) {
			break;
		}
		memset(&entries[out], 0, sizeof(entries[out]));
		memcpy(entries[out].name, name, sizeof(entries[out].name) - 1);
		entries[out].name[sizeof(entries[out].name) - 1] = '\0';
		entries[out].type = type;
		out++;
	}

	*count = out;
	return (0);
}

int
vnode_ioctl(vnode_t *vn, u64 cmd, void *arg)
{
	if (!vn) {
		return (-POSIX_ENOTTY);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}
	if (vn->ioctl_fn) {
		return (vn->ioctl_fn(vn, cmd, arg));
	}
	return (-POSIX_ENOTTY);
}

int
vnode_readlink(vnode_t *vn, char *buf, size_t bufsize)
{
	if (!vn || !buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (vn->type != VLNK || !vn->readlink_fn) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!vfs_back_mount_can_read(vn->mount_id)) {
		return (-API_ERR_ACCESS);
	}
	return (vn->readlink_fn(vn, buf, bufsize));
}

int
vfs_resolve_nofollow(const char *path, vnode_t **out)
{
	return (vfs_back_resolve(path, out, 0));
}

int
vfs_resolve(const char *path, vnode_t **out)
{
	return (vfs_back_resolve(path, out, 1));
}

int
vfs_create_file(const char *path)
{
	return (vfs_back_create_file(path));
}

int
vfs_mkdir(const char *path)
{
	return (vfs_back_mkdir(path));
}

int
vfs_rmdir(const char *path)
{
	return (vfs_back_rmdir(path));
}

int
vfs_unlink(const char *path)
{
	return (vfs_back_unlink(path));
}

int
vfs_rename(const char *oldpath, const char *newpath)
{
	return (vfs_back_rename(oldpath, newpath));
}

int
vfs_truncate(const char *path, u64 length)
{
	return (vfs_back_truncate(path, length));
}

int
vfs_symlink(const char *target, const char *linkpath)
{
	return (vfs_back_symlink(target, linkpath));
}

int
vfs_link(const char *oldpath, const char *newpath)
{
	return (vfs_back_link(oldpath, newpath));
}

int
vfs_readlink(const char *path, char *buf, size_t bufsize)
{
	return (vfs_back_readlink(path, buf, bufsize));
}

int
vfs_chdir(const char *path)
{
	vnode_t	*vn;
	int	ret;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	ret = vfs_resolve(path, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-API_ERR_NOT_DIR);
	}

	vnode_release(vn);
	ret = vfs_back_chdir(path);
	return (ret);
}

int
vfs_getcwd(char *buf, u32 size)
{
	if (!buf || size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (vfs_back_getcwd(buf, size));
}

int
vfs_read_file_full(const char *path, u8 *buf, u32 bufsize,
    u32 *bytes_read)
{
	vnode_t		*vn;
	posix_stat_t	st;
	u32		file_size;
	int		n, ret;

	if (!bytes_read || (!buf && bufsize != 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = vfs_resolve(path, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type == VDIR) {
		vnode_release(vn);
		return (-API_ERR_IS_DIR);
	}
	ret = vnode_stat(vn, &st);
	if (ret != 0) {
		vnode_release(vn);
		return (ret);
	}

	file_size = (u32)st.st_size;
	if (file_size == 0) {
		vnode_release(vn);
		*bytes_read = 0;
		return (0);
	}
	if (file_size > bufsize) {
		vnode_release(vn);
		return (-API_ERR_TOO_BIG);
	}

	n = vnode_read(vn, buf, file_size, 0);
	if (n < 0) {
		vnode_release(vn);
		return (n);
	}

	vnode_release(vn);
	*bytes_read = (u32)n;
	return (0);
}

int
vfs_write_file(const char *path, const u8 *data, u32 size)
{
	return (vfs_back_write_file(path, data, size));
}
