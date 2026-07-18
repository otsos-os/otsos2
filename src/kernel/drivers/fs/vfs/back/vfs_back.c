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
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vnode_t as VFS vnode
$define %type vfs_back_ops_t as backend operation table
$define %type vfs_mount_t as mounted backend slot
$define %type chainfs_file_entry_t as ChainFS file table entry

$define %func vfs_path_starts_with as function with args const char *, const char *
$define %func vfs_mount_path_copy as function with args const char *, char *, int
$define %func vfs_back_root_mount as function with args void
$define %func vfs_back_find_ops as function with args const char *
$define %func vfs_back_find_mount as function with args const char *
$define %func vfs_back_find_mount_id as function with args u32
$define %func vfs_back_mount_access_ok as function with args vfs_mount_t *, int, int
$define %func vfs_back_lookup as function with args const char *
$define %func vfs_mount_root_name as function with args const char *, char *, int
$define %func chainfs_back_ready as function with args void
$define %func chainfs_entry_vtype as function with args u8
$define %func chainfs_entry_dtype as function with args u8
$define %func chainfs_copy_name as procedure with args const char *, char *
$define %func chainfs_vnode_read as function with args vnode_t *, void *, u64, u64
$define %func chainfs_vnode_write as function with args vnode_t *, const void *, u64, u64
$define %func chainfs_vnode_readlink as function with args vnode_t *, char *, size_t
$define %func chainfs_vnode_stat as function with args vnode_t *, posix_stat_t *
$define %func chainfs_vnode_readdir as function with args vnode_t *, u32, char *, int *
$define %func vfs_root_readdir as function with args vnode_t *, u32, char *, int *
$define %func chainfs_back_lookup as function with args const char *
$define %func chainfs_back_init as function with args void
$define %func chainfs_back_create_file as function with args const char *
$define %func chainfs_back_mkdir as function with args const char *
$define %func chainfs_back_rmdir as function with args const char *
$define %func chainfs_back_unlink as function with args const char *
$define %func chainfs_back_rename as function with args const char *, const char *
$define %func chainfs_back_truncate as function with args const char *, u64
$define %func chainfs_back_symlink as function with args const char *, const char *
$define %func chainfs_back_link as function with args const char *, const char *
$define %func chainfs_back_chdir as function with args const char *
$define %func chainfs_back_getcwd as function with args char *, u32
$define %func chainfs_back_write_file as function with args const char *, const u8 *, u32
$define %func devfs_back_init as function with args void
$define %func devfs_back_lookup as function with args const char *
$define %func vfs_back_init as function with args void
$define %func vfs_back_mount as function with args const char *, const vfs_back_ops_t *
$define %func vfs_back_mount_flags as function with args const char *, const vfs_back_ops_t *, u64
$define %func vfs_back_mount_named as function with args const char *, const char *, u64
$define %func vfs_back_umount as function with args const char *
$define %func vfs_back_mount_can_read as function with args u32
$define %func vfs_back_mount_can_write as function with args u32
$define %func vfs_back_mount_can_exec as function with args const char *
$define %func vfs_back_mount_ref as function with args u32
$define %func vfs_back_mount_unref as function with args u32
$define %func vfs_back_resolve as function with args const char *, vnode_t **, int
$define %func vfs_back_create_file as function with args const char *
$define %func vfs_back_mkdir as function with args const char *
$define %func vfs_back_rmdir as function with args const char *
$define %func vfs_back_unlink as function with args const char *
$define %func vfs_back_rename as function with args const char *, const char *
$define %func vfs_back_truncate as function with args const char *, u64
$define %func vfs_back_symlink as function with args const char *, const char *
$define %func vfs_back_link as function with args const char *, const char *
$define %func vfs_back_readlink as function with args const char *, char *, size_t
$define %func vfs_back_chdir as function with args const char *
$define %func vfs_back_getcwd as function with args char *, u32
$define %func vfs_back_write_file as function with args const char *, const u8 *, u32

*/

/* !SPACE!

$space %internal vfs_path_starts_with, vfs_mount_path_copy
$space %internal vfs_back_root_mount, vfs_back_find_ops
$space %internal vfs_back_find_mount, vfs_back_find_mount_id
$space %internal vfs_back_mount_access_ok, vfs_back_lookup
$space %internal vfs_mount_root_name
$space %internal chainfs_back_ready, chainfs_entry_vtype
$space %internal chainfs_entry_dtype, chainfs_copy_name
$space %internal chainfs_vnode_read, chainfs_vnode_write
$space %internal chainfs_vnode_readlink, chainfs_vnode_stat
$space %internal chainfs_vnode_readdir, vfs_root_readdir
$space %internal chainfs_back_lookup, chainfs_back_init
$space %internal chainfs_back_create_file, chainfs_back_mkdir
$space %internal chainfs_back_rmdir, chainfs_back_unlink
$space %internal chainfs_back_rename, chainfs_back_truncate
$space %internal chainfs_back_symlink, chainfs_back_link
$space %internal chainfs_back_chdir, chainfs_back_getcwd
$space %internal chainfs_back_write_file
$space %internal devfs_back_init, devfs_back_lookup
$space %export vfs_back_init, vfs_back_mount, vfs_back_mount_flags
$space %export vfs_back_mount_named, vfs_back_umount
$space %export vfs_back_mount_can_read, vfs_back_mount_can_write
$space %export vfs_back_mount_can_exec
$space %export vfs_back_mount_ref, vfs_back_mount_unref
$space %export vfs_back_resolve
$space %export vfs_back_create_file, vfs_back_mkdir, vfs_back_rmdir
$space %export vfs_back_unlink, vfs_back_rename, vfs_back_truncate
$space %export vfs_back_symlink, vfs_back_link, vfs_back_readlink
$space %export vfs_back_chdir, vfs_back_getcwd, vfs_back_write_file

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/drivers/fs/vfs/back/vfs_back.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	VFS_BACK_MAX_MOUNTS	8
#define	VFS_BACK_MAX_PATH	256
#define	VFS_BACK_ROOT_ENTRIES	128

typedef struct vfs_mount {
	char			path[VFS_BACK_MAX_PATH];
	const vfs_back_ops_t	*ops;
	u64			flags;
	u32			id;
	u32			refs;
} vfs_mount_t;

static vfs_mount_t	vfs_mounts[VFS_BACK_MAX_MOUNTS];
static int		vfs_mount_count;
static u32		vfs_next_mount_id;

static int	chainfs_back_init(void);
static vnode_t	*chainfs_back_lookup(const char *path);
static int	chainfs_back_create_file(const char *path);
static int	chainfs_back_mkdir(const char *path);
static int	chainfs_back_rmdir(const char *path);
static int	chainfs_back_unlink(const char *path);
static int	chainfs_back_rename(const char *oldpath,
		    const char *newpath);
static int	chainfs_back_truncate(const char *path, u64 length);
static int	chainfs_back_symlink(const char *target,
		    const char *linkpath);
static int	chainfs_back_link(const char *oldpath,
		    const char *newpath);
static int	chainfs_back_chdir(const char *path);
static int	chainfs_back_getcwd(char *buf, u32 size);
static int	chainfs_back_write_file(const char *path, const u8 *data,
		    u32 size);
static int	devfs_back_init(void);
static vnode_t	*devfs_back_lookup(const char *path);

static const vfs_back_ops_t	chainfs_back_ops = {
	.name = "chainfs",
	.init = chainfs_back_init,
	.lookup = chainfs_back_lookup,
	.create_file = chainfs_back_create_file,
	.mkdir = chainfs_back_mkdir,
	.rmdir = chainfs_back_rmdir,
	.unlink = chainfs_back_unlink,
	.rename = chainfs_back_rename,
	.truncate = chainfs_back_truncate,
	.symlink = chainfs_back_symlink,
	.link = chainfs_back_link,
	.chdir = chainfs_back_chdir,
	.getcwd = chainfs_back_getcwd,
	.write_file = chainfs_back_write_file,
	.umount = NULL,
};

static const vfs_back_ops_t	devfs_back_ops = {
	.name = "devfs",
	.init = devfs_back_init,
	.lookup = devfs_back_lookup,
	.create_file = NULL,
	.mkdir = NULL,
	.rmdir = NULL,
	.unlink = NULL,
	.rename = NULL,
	.truncate = NULL,
	.symlink = NULL,
	.link = NULL,
	.chdir = NULL,
	.getcwd = NULL,
	.write_file = NULL,
	.umount = NULL,
};

static int
vfs_path_starts_with(const char *path, const char *prefix)
{
	int	i;

	if (!path || !prefix) {
		return (0);
	}
	if (strcmp(prefix, "/") == 0) {
		return (path[0] == '/');
	}
	for (i = 0; prefix[i] != '\0'; i++) {
		if (path[i] != prefix[i]) {
			return (0);
		}
	}
	if (path[i] == '\0' || path[i] == '/') {
		return (1);
	}
	return (0);
}

static int
vfs_mount_path_copy(const char *src, char *dst, int size)
{
	int	len;

	if (!src || !dst || size <= 1 || src[0] != '/') {
		return (-1);
	}

	len = strlen(src);
	while (len > 1 && src[len - 1] == '/') {
		len--;
	}
	if (len <= 0 || len >= size) {
		return (-1);
	}

	memset(dst, 0, (size_t)size);
	memcpy(dst, src, len);
	dst[len] = '\0';
	return (0);
}

static vfs_mount_t *
vfs_back_root_mount(void)
{
	int	i;

	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, "/") == 0) {
			return (&vfs_mounts[i]);
		}
	}
	return (NULL);
}

static const vfs_back_ops_t *
vfs_back_find_ops(const char *fstype)
{
	if (!fstype) {
		return (NULL);
	}
	if (strcmp(fstype, "devfs") == 0) {
		return (&devfs_back_ops);
	}
	return (NULL);
}

static vfs_mount_t *
vfs_back_find_mount(const char *path)
{
	vfs_mount_t	*best_mount;
	int		best_len, len, i;

	if (!path) {
		return (NULL);
	}

	best_mount = NULL;
	best_len = -1;
	for (i = 0; i < vfs_mount_count; i++) {
		if (!vfs_path_starts_with(path, vfs_mounts[i].path)) {
			continue;
		}
		len = strlen(vfs_mounts[i].path);
		if (len > best_len) {
			best_mount = &vfs_mounts[i];
			best_len = len;
		}
	}
	if (best_mount) {
		return (best_mount);
	}
	return (vfs_back_root_mount());
}

static vfs_mount_t *
vfs_back_find_mount_id(u32 mount_id)
{
	int	i;

	if (mount_id == 0) {
		return (NULL);
	}
	for (i = 0; i < vfs_mount_count; i++) {
		if (vfs_mounts[i].id == mount_id) {
			return (&vfs_mounts[i]);
		}
	}
	return (NULL);
}

static int
vfs_back_mount_access_ok(vfs_mount_t *mnt, int write, int exec)
{
	process_t	*proc;

	if (!mnt) {
		return (0);
	}
	if (write && (mnt->flags & VFS_MNT_RDONLY)) {
		return (0);
	}
	if (exec && (mnt->flags & VFS_MNT_NOEXEC)) {
		return (0);
	}
	if ((mnt->flags & VFS_MNT_KUSR_ONLY) == 0) {
		return (1);
	}

	proc = process_current();
	if (!proc) {
		return (1);
	}
	return (proc_has_privilege(proc));
}

static vnode_t *
vfs_back_lookup(const char *path)
{
	vfs_mount_t	*mnt;
	vnode_t		*vn;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->lookup) {
		return (NULL);
	}
	if (!vfs_back_mount_access_ok(mnt, 0, 0)) {
		return (NULL);
	}
	vn = mnt->ops->lookup(path);
	if (!vn) {
		return (NULL);
	}
	if ((mnt->flags & VFS_MNT_NODEV) && vn->type == VCHR) {
		vnode_release(vn);
		return (NULL);
	}

	vn->mount_id = mnt->id;
	mnt->refs++;
	return (vn);
}

static int
vfs_mount_root_name(const char *path, char *out, int size)
{
	int	i;

	if (!path || !out || size <= 1) {
		return (-1);
	}
	if (path[0] != '/' || path[1] == '\0') {
		return (-1);
	}

	i = 1;
	while (path[i] != '\0' && path[i] != '/' && i < size) {
		out[i - 1] = path[i];
		i++;
	}
	if (i == 1 || i >= size) {
		return (-1);
	}
	out[i - 1] = '\0';
	return (0);
}

static int
chainfs_back_ready(void)
{
	return (g_chainfs.superblock.magic == CHAINFS_MAGIC);
}

static int
chainfs_entry_vtype(u8 type)
{
	if (type == CHAINFS_TYPE_DIR) {
		return (VDIR);
	}
	if (type == CHAINFS_TYPE_SYMLINK) {
		return (VLNK);
	}
	if (type == CHAINFS_TYPE_SOCK) {
		return (VSOCK);
	}
	return (VREG);
}

static int
chainfs_entry_dtype(u8 type)
{
	if (type == CHAINFS_TYPE_DIR) {
		return (VDIR);
	}
	if (type == CHAINFS_TYPE_SYMLINK) {
		return (VLNK);
	}
	if (type == CHAINFS_TYPE_SOCK) {
		return (VSOCK);
	}
	return (VREG);
}

static void
chainfs_copy_name(const char *src, char *dst)
{
	int	j;

	for (j = 0; j < 31 && src[j] != '\0'; j++) {
		dst[j] = src[j];
	}
	dst[j] = '\0';
}

static int
chainfs_vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	chainfs_file_entry_t	entry;
	char			*path;
	u32			entry_block, entry_offset;
	u32			bytes_read, to_read;

	path = (char *)vn->data;
	if (!path) {
		return (-1);
	}
	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}
	if (offset >= entry.size) {
		return (0);
	}

	to_read = (count > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (u32)count;
	if (to_read > entry.size - (u32)offset) {
		to_read = entry.size - (u32)offset;
	}

	bytes_read = 0;
	if (chainfs_read_file_range(path, (u8 *)buf, to_read,
	    (u32)offset, &bytes_read) != 0) {
		return (-1);
	}

	vn->size = entry.size;
	return ((int)bytes_read);
}

static int
chainfs_vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	chainfs_file_entry_t	entry;
	char			*path;
	u8			*new_data;
	u32			entry_block, entry_offset;
	u32			bytes_read, end_pos, new_size, old_size;
	u32			write_off;
	int			result;

	path = (char *)vn->data;
	if (!path || offset > 0xFFFFFFFFULL || count > 0xFFFFFFFFULL) {
		return (-1);
	}
	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	write_off = (u32)offset;
	if ((u32)count > 0xFFFFFFFFU - write_off) {
		return (-1);
	}

	old_size = entry.size;
	end_pos = write_off + (u32)count;
	new_size = (end_pos > old_size) ? end_pos : old_size;
	if (new_size == 0) {
		result = chainfs_write_file(path, (const u8 *)"", 0);
		return (result == 0 ? 0 : -1);
	}

	new_data = (u8 *)kmem_calloc(new_size, 1);
	if (!new_data) {
		return (-1);
	}

	if (old_size > 0) {
		bytes_read = 0;
		if (chainfs_read_file(path, new_data, old_size,
		    &bytes_read) != 0) {
			kmem_free(new_data);
			return (-1);
		}
	}

	memcpy(new_data + write_off, buf, (unsigned long)count);
	result = chainfs_write_file(path, new_data, new_size);
	kmem_free(new_data);

	if (result != 0) {
		return (-1);
	}

	vn->size = new_size;
	return ((int)count);
}

static int
chainfs_vnode_readlink(vnode_t *vn, char *buf, size_t bufsize)
{
	char	*path;

	path = (char *)vn->data;
	if (!path) {
		return (-1);
	}
	return (chainfs_readlink(path, buf, (u32)bufsize));
}

static int
chainfs_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	chainfs_file_entry_t	entry;
	char			*path;
	u32			entry_block, entry_offset;

	path = (char *)vn->data;
	if (!path || !st) {
		return (-1);
	}
	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = vn->mode;
	st->st_size = (s64)entry.size;
	st->st_blksize = CHAINFS_BLOCK_SIZE;
	st->st_blocks = (s64)((entry.size + CHAINFS_BLOCK_SIZE - 1) /
	    CHAINFS_BLOCK_SIZE);
	st->st_nlink = entry.nlink;
	st->st_uid = vn->uid;
	st->st_gid = vn->gid;
	st->st_ino = (u64)entry_block;
	vn->size = entry.size;
	return (0);
}

static int
chainfs_vnode_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	chainfs_file_entry_t	entries[VFS_BACK_ROOT_ENTRIES];
	char			*path;
	u32			count;

	path = (char *)vn->data;
	if (!path || !name) {
		return (-1);
	}

	count = 0;
	if (chainfs_list_dir(path, entries, VFS_BACK_ROOT_ENTRIES,
	    &count) != 0) {
		return (-1);
	}
	if (index >= count) {
		return (0);
	}

	chainfs_copy_name(entries[index].name, name);
	if (type) {
		*type = chainfs_entry_dtype(entries[index].type);
	}
	return (1);
}

static int
vfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	chainfs_file_entry_t	entries[VFS_BACK_ROOT_ENTRIES];
	char			mount_name[32];
	u32			count, seen, mount_index;
	int			i;

	(void)vn;

	if (!name || !chainfs_back_ready()) {
		return (-1);
	}

	count = 0;
	if (chainfs_list_dir("/", entries, VFS_BACK_ROOT_ENTRIES,
	    &count) != 0) {
		return (-1);
	}

	if (index < count) {
		chainfs_copy_name(entries[index].name, name);
		if (type) {
			*type = chainfs_entry_dtype(entries[index].type);
		}
		return (1);
	}

	mount_index = index - count;
	seen = 0;
	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, "/") == 0) {
			continue;
		}
		if (vfs_mount_root_name(vfs_mounts[i].path, mount_name,
		    sizeof(mount_name)) != 0) {
			continue;
		}
		if (seen == mount_index) {
			chainfs_copy_name(mount_name, name);
			if (type) {
				*type = VDIR;
			}
			return (1);
		}
		seen++;
	}
	return (0);
}

static vnode_t *
chainfs_back_lookup(const char *path)
{
	chainfs_file_entry_t	entry;
	char			*path_copy;
	vnode_t			*vn;
	u32			entry_block, entry_offset;
	int			len, vtype;

	if (!chainfs_back_ready()) {
		return (NULL);
	}
	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (NULL);
	}

	vtype = chainfs_entry_vtype(entry.type);
	vn = vnode_alloc(vtype, entry.name);
	if (!vn) {
		return (NULL);
	}

	if (vtype == VSOCK) {
		vn->mode = POSIX_S_IFSOCK | 0600;
		return (vn);
	}

	path_copy = (char *)kmem_calloc(VFS_BACK_MAX_PATH, 1);
	if (!path_copy) {
		vnode_release(vn);
		return (NULL);
	}

	len = strlen(path);
	if (len >= VFS_BACK_MAX_PATH) {
		len = VFS_BACK_MAX_PATH - 1;
	}
	memcpy(path_copy, path, len);
	path_copy[len] = '\0';

	vn->data = path_copy;
	vn->data_owned = 1;
	vn->size = entry.size;
	vn->read_fn = chainfs_vnode_read;
	vn->write_fn = chainfs_vnode_write;
	vn->stat_fn = chainfs_vnode_stat;
	vn->readdir_fn = chainfs_vnode_readdir;
	vn->readlink_fn = chainfs_vnode_readlink;

	if (strcmp(path, "/") == 0) {
		vn->readdir_fn = vfs_root_readdir;
	}
	return (vn);
}

static int
chainfs_back_init(void)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (0);
}

static int
chainfs_back_create_file(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_write_file(path, (const u8 *)"", 0));
}

static int
chainfs_back_mkdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_mkdir(path));
}

static int
chainfs_back_rmdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_rmdir(path));
}

static int
chainfs_back_unlink(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_delete_file(path));
}

static int
chainfs_back_rename(const char *oldpath, const char *newpath)
{
	chainfs_file_entry_t	entry;
	u8			*buf;
	u32			entry_block, entry_offset;
	u32			bytes_read;

	if (!chainfs_back_ready()) {
		return (-1);
	}
	if (chainfs_find_file(oldpath, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-1);
	}
	if (entry.size == 0) {
		if (chainfs_write_file(newpath, (const u8 *)"", 0) != 0) {
			return (-1);
		}
		chainfs_delete_file(oldpath);
		return (0);
	}

	buf = (u8 *)kmem_calloc(entry.size, 1);
	if (!buf) {
		return (-1);
	}

	bytes_read = 0;
	if (chainfs_read_file(oldpath, buf, entry.size, &bytes_read) != 0) {
		kmem_free(buf);
		return (-1);
	}
	if (chainfs_write_file(newpath, buf, entry.size) != 0) {
		kmem_free(buf);
		return (-1);
	}

	kmem_free(buf);
	chainfs_delete_file(oldpath);
	return (0);
}

static int
chainfs_back_truncate(const char *path, u64 length)
{
	chainfs_file_entry_t	entry;
	u8			*buf;
	u32			entry_block, entry_offset;
	u32			bytes_read, to_read;

	if (!chainfs_back_ready() || length > 0xFFFFFFFFULL) {
		return (-1);
	}
	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-1);
	}
	if (length == 0) {
		return (chainfs_write_file(path, (const u8 *)"", 0));
	}
	if (length == entry.size) {
		return (0);
	}

	buf = (u8 *)kmem_calloc((size_t)length, 1);
	if (!buf) {
		return (-1);
	}

	if (entry.size > 0) {
		to_read = (entry.size < length) ? entry.size : (u32)length;
		bytes_read = 0;
		if (chainfs_read_file(path, buf, to_read,
		    &bytes_read) != 0) {
			kmem_free(buf);
			return (-1);
		}
	}

	if (chainfs_write_file(path, buf, (u32)length) != 0) {
		kmem_free(buf);
		return (-1);
	}

	kmem_free(buf);
	return (0);
}

static int
chainfs_back_symlink(const char *target, const char *linkpath)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_symlink(target, linkpath));
}

static int
chainfs_back_link(const char *oldpath, const char *newpath)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_link(oldpath, newpath));
}

static int
chainfs_back_chdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_chdir(path));
}

static int
chainfs_back_getcwd(char *buf, u32 size)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	if (chainfs_get_current_path(buf, size) == NULL) {
		return (-1);
	}
	return (0);
}

static int
chainfs_back_write_file(const char *path, const u8 *data, u32 size)
{
	if (!chainfs_back_ready()) {
		return (-1);
	}
	return (chainfs_write_file(path, data, size));
}

static int
devfs_back_init(void)
{
	devfs_init();
	return (0);
}

static vnode_t *
devfs_back_lookup(const char *path)
{
	vnode_t	*vn;

	vn = devfs_lookup(path);
	if (vn) {
		return (vn);
	}
	if (strcmp(path, "/dev") == 0 || strcmp(path, "/dev/") == 0) {
		vn = vnode_alloc(VDIR, "dev");
		if (vn) {
			vn->readdir_fn = devfs_root_readdir;
			vn->data = NULL;
		}
		return (vn);
	}
	return (NULL);
}

int
vfs_back_init(void)
{
	vfs_mount_count = 0;
	vfs_next_mount_id = 1;

	if (chainfs_back_ops.init && chainfs_back_ops.init() != 0) {
		return (-1);
	}
	if (vfs_back_mount("/", &chainfs_back_ops) != 0) {
		return (-1);
	}
	if (devfs_back_ops.init && devfs_back_ops.init() != 0) {
		return (-1);
	}
	if (vfs_back_mount("/dev", &devfs_back_ops) != 0) {
		return (-1);
	}

	drivers_log("[VFS] backends mounted: / chainfs, /dev devfs\n");
	return (0);
}

int
vfs_back_mount(const char *path, const vfs_back_ops_t *ops)
{
	return (vfs_back_mount_flags(path, ops, 0));
}

int
vfs_back_mount_flags(const char *path, const vfs_back_ops_t *ops, u64 flags)
{
	char	mount_path[VFS_BACK_MAX_PATH];
	int	i, len;

	if (!path || !ops || !ops->lookup) {
		return (-1);
	}
	if ((flags & ~VFS_MNT_SUPPORTED) != 0) {
		return (-1);
	}
	if (vfs_mount_count >= VFS_BACK_MAX_MOUNTS) {
		return (-1);
	}

	if (vfs_mount_path_copy(path, mount_path,
	    sizeof(mount_path)) != 0) {
		return (-1);
	}
	len = strlen(mount_path);

	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, mount_path) == 0) {
			return (-1);
		}
	}

	memset(vfs_mounts[vfs_mount_count].path, 0, VFS_BACK_MAX_PATH);
	memcpy(vfs_mounts[vfs_mount_count].path, mount_path, len);
	vfs_mounts[vfs_mount_count].path[len] = '\0';
	vfs_mounts[vfs_mount_count].ops = ops;
	vfs_mounts[vfs_mount_count].flags = flags;
	vfs_mounts[vfs_mount_count].id = vfs_next_mount_id++;
	vfs_mounts[vfs_mount_count].refs = 0;
	if (vfs_next_mount_id == 0) {
		vfs_next_mount_id = 1;
	}
	vfs_mount_count++;
	return (0);
}

int
vfs_back_mount_named(const char *path, const char *fstype, u64 flags)
{
	const vfs_back_ops_t	*ops;

	ops = vfs_back_find_ops(fstype);
	if (!ops) {
		return (-2);
	}
	return (vfs_back_mount_flags(path, ops, flags));
}

int
vfs_back_umount(const char *path)
{
	char	mount_path[VFS_BACK_MAX_PATH];
	int	i, j;

	if (vfs_mount_path_copy(path, mount_path,
	    sizeof(mount_path)) != 0) {
		return (-1);
	}
	if (strcmp(mount_path, "/") == 0) {
		return (-1);
	}

	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, mount_path) != 0) {
			continue;
		}
		if (vfs_mounts[i].refs != 0) {
			return (-2);
		}
		for (j = 0; j < vfs_mount_count; j++) {
			if (j == i) {
				continue;
			}
			if (vfs_path_starts_with(vfs_mounts[j].path,
			    mount_path)) {
				return (-2);
			}
		}
		if (vfs_mounts[i].ops && vfs_mounts[i].ops->umount &&
		    vfs_mounts[i].ops->umount(mount_path) != 0) {
			return (-1);
		}
		for (j = i; j < vfs_mount_count - 1; j++) {
			vfs_mounts[j] = vfs_mounts[j + 1];
		}
		memset(&vfs_mounts[vfs_mount_count - 1], 0,
		    sizeof(vfs_mount_t));
		vfs_mount_count--;
		return (0);
	}

	return (-1);
}

int
vfs_back_mount_can_read(u32 mount_id)
{
	vfs_mount_t	*mnt;

	if (mount_id == 0) {
		return (1);
	}
	mnt = vfs_back_find_mount_id(mount_id);
	return (vfs_back_mount_access_ok(mnt, 0, 0));
}

int
vfs_back_mount_can_write(u32 mount_id)
{
	vfs_mount_t	*mnt;

	if (mount_id == 0) {
		return (1);
	}
	mnt = vfs_back_find_mount_id(mount_id);
	return (vfs_back_mount_access_ok(mnt, 1, 0));
}

int
vfs_back_mount_can_exec(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	return (vfs_back_mount_access_ok(mnt, 0, 1));
}

int
vfs_back_mount_ref(u32 mount_id)
{
	int	i;

	if (mount_id == 0) {
		return (0);
	}
	for (i = 0; i < vfs_mount_count; i++) {
		if (vfs_mounts[i].id == mount_id) {
			vfs_mounts[i].refs++;
			return (0);
		}
	}
	return (-1);
}

int
vfs_back_mount_unref(u32 mount_id)
{
	int	i;

	if (mount_id == 0) {
		return (0);
	}
	for (i = 0; i < vfs_mount_count; i++) {
		if (vfs_mounts[i].id != mount_id) {
			continue;
		}
		if (vfs_mounts[i].refs > 0) {
			vfs_mounts[i].refs--;
		}
		return (0);
	}
	return (-1);
}

int
vfs_back_resolve(const char *path, vnode_t **out, int follow)
{
	char		link_target[VFS_BACK_MAX_PATH];
	char		resolved[VFS_BACK_MAX_PATH];
	const char	*cur;
	vnode_t		*vn;
	int		link_count, last_slash, i;

	if (!path || !out || path[0] == '\0') {
		return (-1);
	}

	cur = path;
	link_count = 0;
	for (;;) {
		vn = vfs_back_lookup(cur);
		if (!vn) {
			return (-1);
		}
		if (!follow || vn->type != VLNK) {
			*out = vn;
			return (0);
		}
		if (!vn->readlink_fn || link_count++ >= 40) {
			vnode_release(vn);
			return (-1);
		}
		if (vn->readlink_fn(vn, link_target,
		    sizeof(link_target)) < 0) {
			vnode_release(vn);
			return (-1);
		}
		vnode_release(vn);

		if (link_target[0] == '/') {
			cur = link_target;
		} else {
			last_slash = -1;
			for (i = 0; cur[i] != '\0'; i++) {
				if (cur[i] == '/') {
					last_slash = i;
				}
			}
			if (last_slash > 0) {
				memcpy(resolved, cur, last_slash);
				resolved[last_slash] = '\0';
				strcat(resolved, "/");
				strcat(resolved, link_target);
			} else {
				strcpy(resolved, link_target);
			}
			cur = resolved;
		}
	}
}

int
vfs_back_create_file(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->create_file) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->create_file(path));
}

int
vfs_back_mkdir(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->mkdir) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->mkdir(path));
}

int
vfs_back_rmdir(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->rmdir) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->rmdir(path));
}

int
vfs_back_unlink(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->unlink) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->unlink(path));
}

int
vfs_back_rename(const char *oldpath, const char *newpath)
{
	vfs_mount_t	*old_mnt, *new_mnt;

	old_mnt = vfs_back_find_mount(oldpath);
	new_mnt = vfs_back_find_mount(newpath);
	if (!old_mnt || old_mnt != new_mnt || !old_mnt->ops ||
	    !old_mnt->ops->rename) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(old_mnt, 1, 0)) {
		return (-1);
	}
	return (old_mnt->ops->rename(oldpath, newpath));
}

int
vfs_back_truncate(const char *path, u64 length)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->truncate) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->truncate(path, length));
}

int
vfs_back_symlink(const char *target, const char *linkpath)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(linkpath);
	if (!mnt || !mnt->ops || !mnt->ops->symlink) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->symlink(target, linkpath));
}

int
vfs_back_link(const char *oldpath, const char *newpath)
{
	vfs_mount_t	*old_mnt, *new_mnt;

	old_mnt = vfs_back_find_mount(oldpath);
	new_mnt = vfs_back_find_mount(newpath);
	if (!old_mnt || old_mnt != new_mnt || !old_mnt->ops ||
	    !old_mnt->ops->link) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(old_mnt, 1, 0)) {
		return (-1);
	}
	return (old_mnt->ops->link(oldpath, newpath));
}

int
vfs_back_readlink(const char *path, char *buf, size_t bufsize)
{
	vnode_t	*vn;
	int	ret;

	if (!path || !buf || bufsize == 0) {
		return (-1);
	}
	if (vfs_back_resolve(path, &vn, 0) != 0 || !vn) {
		return (-1);
	}

	ret = vnode_readlink(vn, buf, bufsize);
	vnode_release(vn);
	return (ret);
}

int
vfs_back_chdir(const char *path)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->chdir) {
		return (-1);
	}
	return (mnt->ops->chdir(path));
}

int
vfs_back_getcwd(char *buf, u32 size)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_root_mount();
	if (!mnt || !mnt->ops || !mnt->ops->getcwd) {
		return (-1);
	}
	return (mnt->ops->getcwd(buf, size));
}

int
vfs_back_write_file(const char *path, const u8 *data, u32 size)
{
	vfs_mount_t	*mnt;

	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops || !mnt->ops->write_file) {
		return (-1);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-1);
	}
	return (mnt->ops->write_file(path, data, size));
}
