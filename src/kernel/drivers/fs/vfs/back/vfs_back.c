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
$define %type vfs_dirent_t as VFS directory entry
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
$define %func chainfs_vnode_listdir as function with args vnode_t *, u32, vfs_dirent_t *, u32, u32 *
$define %func vfs_root_readdir as function with args vnode_t *, u32, char *, int *
$define %func vfs_root_listdir as function with args vnode_t *, u32, vfs_dirent_t *, u32, u32 *
$define %func vfs_root_chain_name_exists as function with args const char *
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
$define %func vfs_back_register_ops as function with args const vfs_back_ops_t *
$define %func vfs_chainfs_back_ops as function with args void
$define %func vfs_devfs_back_ops as function with args void
$define %func vfs_back_mount as function with args const char *, const vfs_back_ops_t *
$define %func vfs_back_mount_flags as function with args const char *, const vfs_back_ops_t *, u64
$define %func vfs_back_mount_named as function with args const char *, const char *, u64
$define %func vfs_back_umount as function with args const char *
$define %func vfs_back_mount_can_read as function with args u32
$define %func vfs_back_mount_can_write as function with args u32
$define %func vfs_back_mount_can_exec as function with args const char *
$define %func vfs_back_mount_can_exec_id as function with args u32
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
$space %internal chainfs_vnode_readdir, chainfs_vnode_listdir
$space %internal vfs_root_readdir, vfs_root_listdir
$space %internal vfs_root_chain_name_exists
$space %internal chainfs_back_lookup, chainfs_back_init
$space %internal chainfs_back_create_file, chainfs_back_mkdir
$space %internal chainfs_back_rmdir, chainfs_back_unlink
$space %internal chainfs_back_rename, chainfs_back_truncate
$space %internal chainfs_back_symlink, chainfs_back_link
$space %internal chainfs_back_chdir, chainfs_back_getcwd
$space %internal chainfs_back_write_file
$space %internal devfs_back_init, devfs_back_lookup
$space %export vfs_back_init, vfs_back_register_ops
$space %export vfs_chainfs_back_ops, vfs_devfs_back_ops
$space %export vfs_back_mount, vfs_back_mount_flags
$space %export vfs_back_mount_named, vfs_back_umount
$space %export vfs_back_mount_can_read, vfs_back_mount_can_write
$space %export vfs_back_mount_can_exec, vfs_back_mount_can_exec_id
$space %export vfs_back_mount_ref, vfs_back_mount_unref
$space %export vfs_back_resolve
$space %export vfs_back_create_file, vfs_back_mkdir, vfs_back_rmdir
$space %export vfs_back_unlink, vfs_back_rename, vfs_back_truncate
$space %export vfs_back_symlink, vfs_back_link, vfs_back_readlink
$space %export vfs_back_chdir, vfs_back_getcwd, vfs_back_write_file

*/

#include <kernel/api/errno.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/drivers/fs/vfs/back/vfs_back.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	VFS_BACK_MAX_MOUNTS	8
#define	VFS_BACK_MAX_OPS	16
#define	VFS_BACK_MAX_PATH	256
#define	VFS_BACK_LISTDIR_BATCH	32

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
static const vfs_back_ops_t *vfs_back_ops_registry[VFS_BACK_MAX_OPS];
static int		vfs_back_ops_count;

static int	chainfs_vnode_listdir(vnode_t *vn, u32 start,
		    vfs_dirent_t *entries, u32 max_entries, u32 *count);
static int	vfs_root_listdir(vnode_t *vn, u32 start,
		    vfs_dirent_t *entries, u32 max_entries, u32 *count);
static int	vfs_root_chain_name_exists(const char *name);
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
		return (-API_ERR_BAD_VALUE);
	}

	len = strlen(src);
	while (len > 1 && src[len - 1] == '/') {
		len--;
	}
	if (len <= 0 || len >= size) {
		return (-API_ERR_TOO_BIG);
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
	int	i;

	if (!fstype) {
		return (NULL);
	}
	for (i = 0; i < vfs_back_ops_count; i++) {
		if (vfs_back_ops_registry[i] != NULL &&
		    vfs_back_ops_registry[i]->name != NULL &&
		    strcmp(vfs_back_ops_registry[i]->name, fstype) == 0) {
			return (vfs_back_ops_registry[i]);
		}
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
		return (-API_ERR_BAD_VALUE);
	}
	if (path[0] != '/' || path[1] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	i = 1;
	while (path[i] != '\0' && path[i] != '/' && i < size) {
		out[i - 1] = path[i];
		i++;
	}
	if (i == 1 || i >= size) {
		return (-API_ERR_TOO_BIG);
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
	int			ret;

	path = (char *)vn->data;
	if (!path || !buf) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}
	if (offset >= entry.size) {
		return (0);
	}

	to_read = (count > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (u32)count;
	if (to_read > entry.size - (u32)offset) {
		to_read = entry.size - (u32)offset;
	}

	bytes_read = 0;
	ret = chainfs_read_file_range(path, (u8 *)buf, to_read,
	    (u32)offset, &bytes_read);
	if (ret != 0) {
		return (ret);
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
	int			result, ret;

	path = (char *)vn->data;
	if (!path || !buf || offset > 0xFFFFFFFFULL ||
	    count > 0x7FFFFFFFULL) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}

	write_off = (u32)offset;
	if ((u32)count > 0xFFFFFFFFU - write_off) {
		return (-API_ERR_FILE_TOO_BIG);
	}

	old_size = entry.size;
	end_pos = write_off + (u32)count;
	new_size = (end_pos > old_size) ? end_pos : old_size;
	if (new_size == 0) {
		result = chainfs_write_file(path, (const u8 *)"", 0);
		return (result == 0 ? 0 : result);
	}

	new_data = (u8 *)kmem_calloc(new_size, 1);
	if (!new_data) {
		return (-API_ERR_NO_MEMORY);
	}

	if (old_size > 0) {
		bytes_read = 0;
		ret = chainfs_read_file(path, new_data, old_size,
		    &bytes_read);
		if (ret != 0) {
			kmem_free(new_data);
			return (ret);
		}
	}

	memcpy(new_data + write_off, buf, (unsigned long)count);
	result = chainfs_write_file(path, new_data, new_size);
	kmem_free(new_data);

	if (result != 0) {
		return (result);
	}

	vn->size = new_size;
	return ((int)count);
}

static int
chainfs_vnode_readlink(vnode_t *vn, char *buf, size_t bufsize)
{
	char	*path;

	path = (char *)vn->data;
	if (!path || !buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (chainfs_readlink(path, buf, (u32)bufsize));
}

static int
chainfs_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	chainfs_file_entry_t	entry;
	char			*path;
	u32			entry_block, entry_offset;
	int			ret;

	path = (char *)vn->data;
	if (!path || !st) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
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
	vfs_dirent_t		entry;
	u32			count;
	int			ret;

	count = 0;
	ret = chainfs_vnode_listdir(vn, index, &entry, 1, &count);
	if (ret != 0) {
		return (ret);
	}
	if (count == 0) {
		return (0);
	}
	chainfs_copy_name(entry.name, name);
	if (type) {
		*type = entry.type;
	}
	return (1);
}

static int
chainfs_vnode_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	chainfs_file_entry_t	files[VFS_BACK_LISTDIR_BATCH];
	char			*path;
	u32			fs_count, i, limit;
	int			ret;

	path = (char *)vn->data;
	if (!path || !entries || !count) {
		return (-API_ERR_BAD_VALUE);
	}

	*count = 0;
	if (max_entries == 0) {
		return (0);
	}
	limit = max_entries;
	if (limit > VFS_BACK_LISTDIR_BATCH) {
		limit = VFS_BACK_LISTDIR_BATCH;
	}

	fs_count = 0;
	ret = chainfs_list_dir_range(path, start, files, limit,
	    &fs_count, NULL);
	if (ret != 0) {
		return (ret);
	}

	for (i = 0; i < fs_count; i++) {
		memset(&entries[i], 0, sizeof(entries[i]));
		chainfs_copy_name(files[i].name, entries[i].name);
		entries[i].type = chainfs_entry_dtype(files[i].type);
	}
	*count = fs_count;
	return (0);
}

static int
vfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	vfs_dirent_t	entry;
	u32		count;
	int		ret;

	if (!name) {
		return (-API_ERR_BAD_VALUE);
	}
	count = 0;
	ret = vfs_root_listdir(vn, index, &entry, 1, &count);
	if (ret != 0) {
		return (ret);
	}
	if (count == 0) {
		return (0);
	}
	chainfs_copy_name(entry.name, name);
	if (type) {
		*type = entry.type;
	}
	return (1);
}

static int
vfs_root_chain_name_exists(const char *name)
{
	chainfs_file_entry_t	entry;
	char			path[VFS_BACK_MAX_PATH];
	u32			entry_block, entry_offset;
	int			len, i;

	if (!name || name[0] == '\0') {
		return (0);
	}
	len = strlen(name);
	if (len + 2 > VFS_BACK_MAX_PATH) {
		return (0);
	}

	path[0] = '/';
	for (i = 0; i < len; i++) {
		path[i + 1] = name[i];
	}
	path[len + 1] = '\0';

	return (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) == 0);
}

static int
vfs_root_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	chainfs_file_entry_t	files[VFS_BACK_LISTDIR_BATCH];
	char			mount_name[32];
	u32			chain_count, fs_count, copied, mount_index;
	u32			seen, i, limit;
	int			ret;

	(void)vn;

	if (!entries || !count) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}

	*count = 0;
	if (max_entries == 0) {
		return (0);
	}

	limit = max_entries;
	if (limit > VFS_BACK_LISTDIR_BATCH) {
		limit = VFS_BACK_LISTDIR_BATCH;
	}

	chain_count = 0;
	fs_count = 0;
	ret = chainfs_list_dir_range("/", start, files, limit,
	    &fs_count, &chain_count);
	if (ret != 0) {
		return (ret);
	}

	copied = 0;
	for (i = 0; i < fs_count; i++) {
		memset(&entries[copied], 0, sizeof(entries[copied]));
		chainfs_copy_name(files[i].name, entries[copied].name);
		entries[copied].type = chainfs_entry_dtype(files[i].type);
		copied++;
	}
	if (copied >= max_entries ||
	    (start < chain_count && start + copied < chain_count)) {
		*count = copied;
		return (0);
	}

	mount_index = (start > chain_count) ? start - chain_count : 0;
	seen = 0;
	for (i = 0; i < (u32)vfs_mount_count && copied < max_entries; i++) {
		if (strcmp(vfs_mounts[i].path, "/") == 0) {
			continue;
		}
		if (vfs_mount_root_name(vfs_mounts[i].path, mount_name,
		    sizeof(mount_name)) != 0) {
			continue;
		}
		if (vfs_root_chain_name_exists(mount_name)) {
			continue;
		}
		if (seen < mount_index) {
			seen++;
			continue;
		}

		memset(&entries[copied], 0, sizeof(entries[copied]));
		chainfs_copy_name(mount_name, entries[copied].name);
		entries[copied].type = VDIR;
		copied++;
		seen++;
	}

	*count = copied;
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
	vn->listdir_fn = chainfs_vnode_listdir;
	vn->readlink_fn = chainfs_vnode_readlink;

	if (strcmp(path, "/") == 0) {
		vn->readdir_fn = vfs_root_readdir;
		vn->listdir_fn = vfs_root_listdir;
	}
	return (vn);
}

static int
chainfs_back_init(void)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (0);
}

static int
chainfs_back_create_file(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_write_file(path, (const u8 *)"", 0));
}

static int
chainfs_back_mkdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_mkdir(path));
}

static int
chainfs_back_rmdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_rmdir(path));
}

static int
chainfs_back_unlink(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
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
	int			ret;

	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	ret = chainfs_find_file(oldpath, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}
	if (entry.size == 0) {
		ret = chainfs_write_file(newpath, (const u8 *)"", 0);
		if (ret != 0) {
			return (ret);
		}
		return (chainfs_delete_file(oldpath));
	}

	buf = (u8 *)kmem_calloc(entry.size, 1);
	if (!buf) {
		return (-API_ERR_NO_MEMORY);
	}

	bytes_read = 0;
	ret = chainfs_read_file(oldpath, buf, entry.size, &bytes_read);
	if (ret != 0) {
		kmem_free(buf);
		return (ret);
	}
	ret = chainfs_write_file(newpath, buf, entry.size);
	if (ret != 0) {
		kmem_free(buf);
		return (ret);
	}

	kmem_free(buf);
	return (chainfs_delete_file(oldpath));
}

static int
chainfs_back_truncate(const char *path, u64 length)
{
	chainfs_file_entry_t	entry;
	u8			*buf;
	u32			entry_block, entry_offset;
	u32			bytes_read, to_read;
	int			ret;

	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	if (length > 0xFFFFFFFFULL) {
		return (-API_ERR_FILE_TOO_BIG);
	}
	ret = chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}
	if (length == 0) {
		return (chainfs_write_file(path, (const u8 *)"", 0));
	}
	if (length == entry.size) {
		return (0);
	}

	buf = (u8 *)kmem_calloc((size_t)length, 1);
	if (!buf) {
		return (-API_ERR_NO_MEMORY);
	}

	if (entry.size > 0) {
		to_read = (entry.size < length) ? entry.size : (u32)length;
		bytes_read = 0;
		ret = chainfs_read_file(path, buf, to_read,
		    &bytes_read);
		if (ret != 0) {
			kmem_free(buf);
			return (ret);
		}
	}

	ret = chainfs_write_file(path, buf, (u32)length);
	if (ret != 0) {
		kmem_free(buf);
		return (ret);
	}

	kmem_free(buf);
	return (0);
}

static int
chainfs_back_symlink(const char *target, const char *linkpath)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_symlink(target, linkpath));
}

static int
chainfs_back_link(const char *oldpath, const char *newpath)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_link(oldpath, newpath));
}

static int
chainfs_back_chdir(const char *path)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	return (chainfs_chdir(path));
}

static int
chainfs_back_getcwd(char *buf, u32 size)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
	}
	if (chainfs_get_current_path(buf, size) == NULL) {
		return (-API_ERR_TOO_BIG);
	}
	return (0);
}

static int
chainfs_back_write_file(const char *path, const u8 *data, u32 size)
{
	if (!chainfs_back_ready()) {
		return (-API_ERR_IO);
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
			vn->listdir_fn = devfs_root_listdir;
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
	vfs_back_ops_count = 0;
	memset(vfs_back_ops_registry, 0, sizeof(vfs_back_ops_registry));
	return (0);
}

int
vfs_back_register_ops(const vfs_back_ops_t *ops)
{
	int	i;

	if (ops == NULL || ops->name == NULL || ops->lookup == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < vfs_back_ops_count; i++) {
		if (vfs_back_ops_registry[i] == ops ||
		    strcmp(vfs_back_ops_registry[i]->name, ops->name) == 0) {
			return (0);
		}
	}
	if (vfs_back_ops_count >= VFS_BACK_MAX_OPS) {
		return (-API_ERR_OBJECTS_FULL);
	}
	vfs_back_ops_registry[vfs_back_ops_count++] = ops;
	drivers_log("[VFS] backend registered: %s\n", ops->name);
	return (0);
}

const vfs_back_ops_t *
vfs_chainfs_back_ops(void)
{
	return (&chainfs_back_ops);
}

const vfs_back_ops_t *
vfs_devfs_back_ops(void)
{
	return (&devfs_back_ops);
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
	int	i, len, ret;

	if (!path || !ops || !ops->lookup) {
		return (-API_ERR_BAD_VALUE);
	}
	if ((flags & ~VFS_MNT_SUPPORTED) != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (vfs_mount_count >= VFS_BACK_MAX_MOUNTS) {
		return (-API_ERR_OBJECTS_FULL);
	}

	ret = vfs_mount_path_copy(path, mount_path,
	    sizeof(mount_path));
	if (ret != 0) {
		return (ret);
	}
	len = strlen(mount_path);

	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, mount_path) == 0) {
			return (-API_ERR_EXISTS);
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
		return (-API_ERR_NODEV);
	}
	return (vfs_back_mount_flags(path, ops, flags));
}

int
vfs_back_umount(const char *path)
{
	char	mount_path[VFS_BACK_MAX_PATH];
	int	i, j;
	int	ret;

	ret = vfs_mount_path_copy(path, mount_path,
	    sizeof(mount_path));
	if (ret != 0) {
		return (ret);
	}
	if (strcmp(mount_path, "/") == 0) {
		return (-API_ERR_BUSY);
	}

	for (i = 0; i < vfs_mount_count; i++) {
		if (strcmp(vfs_mounts[i].path, mount_path) != 0) {
			continue;
		}
		if (vfs_mounts[i].refs != 0) {
			return (-API_ERR_BUSY);
		}
		for (j = 0; j < vfs_mount_count; j++) {
			if (j == i) {
				continue;
			}
			if (vfs_path_starts_with(vfs_mounts[j].path,
			    mount_path)) {
				return (-API_ERR_BUSY);
			}
		}
		if (vfs_mounts[i].ops && vfs_mounts[i].ops->umount) {
			ret = vfs_mounts[i].ops->umount(mount_path);
			if (ret != 0) {
				return (ret);
			}
		}
		for (j = i; j < vfs_mount_count - 1; j++) {
			vfs_mounts[j] = vfs_mounts[j + 1];
		}
		memset(&vfs_mounts[vfs_mount_count - 1], 0,
		    sizeof(vfs_mount_t));
		vfs_mount_count--;
		return (0);
	}

	return (-API_ERR_NOT_FOUND);
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
vfs_back_mount_can_exec_id(u32 mount_id)
{
	vfs_mount_t	*mnt;
	if (mount_id == 0) {
		return (1);
	}
	mnt = vfs_back_find_mount_id(mount_id);
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
	return (-API_ERR_BAD_VALUE);
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
	return (-API_ERR_BAD_VALUE);
}

int
vfs_back_resolve(const char *path, vnode_t **out, int follow)
{
	char		link_target[VFS_BACK_MAX_PATH];
	char		resolved[VFS_BACK_MAX_PATH];
	const char	*cur;
	vnode_t		*vn;
	int		link_count, last_slash, link_len, ret, i;

	if (!path || !out || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	cur = path;
	link_count = 0;
	for (;;) {
		vn = vfs_back_lookup(cur);
		if (!vn) {
			return (-API_ERR_NOT_FOUND);
		}
		if (!follow || vn->type != VLNK) {
			*out = vn;
			return (0);
		}
		if (!vn->readlink_fn || link_count++ >= 40) {
			vnode_release(vn);
			return (-API_ERR_TOO_BIG);
		}
		ret = vn->readlink_fn(vn, link_target,
		    sizeof(link_target));
		if (ret < 0) {
			vnode_release(vn);
			return (ret);
		}
		vnode_release(vn);

		if (link_target[0] == '/') {
			if (strlen(link_target) >= VFS_BACK_MAX_PATH) {
				return (-API_ERR_TOO_BIG);
			}
			cur = link_target;
		} else {
			last_slash = -1;
			for (i = 0; cur[i] != '\0'; i++) {
				if (cur[i] == '/') {
					last_slash = i;
				}
			}
			if (last_slash > 0) {
				link_len = strlen(link_target);
				if (last_slash + 1 + link_len >=
				    VFS_BACK_MAX_PATH) {
					return (-API_ERR_TOO_BIG);
				}
				memcpy(resolved, cur, last_slash);
				resolved[last_slash] = '\0';
				strcat(resolved, "/");
				strcat(resolved, link_target);
			} else {
				if (strlen(link_target) >=
				    VFS_BACK_MAX_PATH) {
					return (-API_ERR_TOO_BIG);
				}
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

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->create_file) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->create_file(path));
}

int
vfs_back_mkdir(const char *path)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->mkdir) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->mkdir(path));
}

int
vfs_back_rmdir(const char *path)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->rmdir) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->rmdir(path));
}

int
vfs_back_unlink(const char *path)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->unlink) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->unlink(path));
}

int
vfs_back_rename(const char *oldpath, const char *newpath)
{
	vfs_mount_t	*old_mnt, *new_mnt;

	if (!oldpath || !newpath || oldpath[0] == '\0' ||
	    newpath[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	old_mnt = vfs_back_find_mount(oldpath);
	new_mnt = vfs_back_find_mount(newpath);
	if (!old_mnt || !new_mnt) {
		return (-API_ERR_NOT_FOUND);
	}
	if (old_mnt != new_mnt) {
		return (-API_ERR_CROSS_DEVICE);
	}
	if (!old_mnt->ops || !old_mnt->ops->rename) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((old_mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(old_mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (old_mnt->ops->rename(oldpath, newpath));
}

int
vfs_back_truncate(const char *path, u64 length)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->truncate) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->truncate(path, length));
}

int
vfs_back_symlink(const char *target, const char *linkpath)
{
	vfs_mount_t	*mnt;

	if (!target || !linkpath || target[0] == '\0' ||
	    linkpath[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(linkpath);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->symlink) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->symlink(target, linkpath));
}

int
vfs_back_link(const char *oldpath, const char *newpath)
{
	vfs_mount_t	*old_mnt, *new_mnt;

	if (!oldpath || !newpath || oldpath[0] == '\0' ||
	    newpath[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	old_mnt = vfs_back_find_mount(oldpath);
	new_mnt = vfs_back_find_mount(newpath);
	if (!old_mnt || !new_mnt) {
		return (-API_ERR_NOT_FOUND);
	}
	if (old_mnt != new_mnt) {
		return (-API_ERR_CROSS_DEVICE);
	}
	if (!old_mnt->ops || !old_mnt->ops->link) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((old_mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(old_mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (old_mnt->ops->link(oldpath, newpath));
}

int
vfs_back_readlink(const char *path, char *buf, size_t bufsize)
{
	vnode_t	*vn;
	int	ret;

	if (!path || !buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = vfs_back_resolve(path, &vn, 0);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}

	ret = vnode_readlink(vn, buf, bufsize);
	vnode_release(vn);
	return (ret);
}

int
vfs_back_chdir(const char *path)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->chdir) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (mnt->ops->chdir(path));
}

int
vfs_back_getcwd(char *buf, u32 size)
{
	vfs_mount_t	*mnt;

	if (!buf || size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_root_mount();
	if (!mnt || !mnt->ops || !mnt->ops->getcwd) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (mnt->ops->getcwd(buf, size));
}

int
vfs_back_write_file(const char *path, const u8 *data, u32 size)
{
	vfs_mount_t	*mnt;

	if (!path || path[0] == '\0' || (!data && size != 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	mnt = vfs_back_find_mount(path);
	if (!mnt || !mnt->ops) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!mnt->ops->write_file) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if ((mnt->flags & VFS_MNT_RDONLY) != 0) {
		return (-API_ERR_READ_ONLY);
	}
	if (!vfs_back_mount_access_ok(mnt, 1, 0)) {
		return (-API_ERR_ACCESS);
	}
	return (mnt->ops->write_file(path, data, size));
}
