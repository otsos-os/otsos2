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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, NOT LIMITED TO, THE
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

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/fs/devtmpfs/devtmpfs.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static vnode_t	vnode_pool[VFS_MAX_VNODES];
static int	vfs_initialized = 0;

static int
path_starts_with(const char *path, const char *prefix)
{
	int	i;

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
copy_path_component(const char *path, char *out, int max)
{
	int	len;

	len = 0;
	while (path[len] != '\0' && path[len] != '/' && len < max - 1) {
		out[len] = path[len];
		len++;
	}
	out[len] = '\0';
	return (len);
}

void
vfs_init(void)
{
	int	i;

	for (i = 0; i < VFS_MAX_VNODES; i++) {
		vnode_pool[i].type = 0;
		vnode_pool[i].refcount = 0;
		vnode_pool[i].size = 0;
		vnode_pool[i].mode = 0;
		vnode_pool[i].data = NULL;
		vnode_pool[i].read_fn = NULL;
		vnode_pool[i].write_fn = NULL;
		vnode_pool[i].stat_fn = NULL;
		vnode_pool[i].readdir_fn = NULL;
	}

	devtmpfs_init();
	vfs_initialized = 1;
	drivers_log("[VFS] initialized (vnode pool: %d slots)\n",
	    VFS_MAX_VNODES);
}

int
vfs_is_initialized(void)
{
	return (vfs_initialized);
}

vnode_t *
vnode_alloc(int type, const char *name)
{
	vnode_t	*vn;
	int	i;

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
	vn->size = 0;
	vn->mode = 0;

	if (name) {
		int	j;
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
	default:
		vn->mode = 0;
		break;
	}

	return (vn);
}

vnode_t *
vnode_acquire(vnode_t *vn)
{
	if (vn) {
		vn->refcount++;
	}
	return (vn);
}

void
vnode_release(vnode_t *vn)
{
	if (!vn) {
		return;
	}

	vn->refcount--;
	if (vn->refcount <= 0) {
		if (vn->data_owned && vn->data) {
			kmem_free(vn->data);
		}
		vn->data = NULL;
		vn->data_owned = 0;
		vn->type = 0;
		vn->refcount = 0;
		vn->read_fn = NULL;
		vn->write_fn = NULL;
		vn->stat_fn = NULL;
		vn->readdir_fn = NULL;
	}
}

int
vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	if (!vn || !vn->read_fn) {
		return (-1);
	}
	return (vn->read_fn(vn, buf, count, offset));
}

int
vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	if (!vn || !vn->write_fn) {
		return (-1);
	}
	return (vn->write_fn(vn, buf, count, offset));
}

int
vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	if (!vn) {
		return (-1);
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
	st->st_uid = 0;
	st->st_gid = 0;
	return (0);
}

int
vnode_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	if (!vn || vn->type != VDIR) {
		return (-1);
	}

	if (vn->readdir_fn) {
		return (vn->readdir_fn(vn, index, name, type));
	}

	return (-1);
}

static int
chainfs_vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	char		*path;
	u32		bytes_read;
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;

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

	u32		to_read;

	to_read = (u32)count;
	if (to_read > entry.size - offset) {
		to_read = entry.size - offset;
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
	char		*path;
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u8			*new_data;
	u32			new_size, old_size;

	path = (char *)vn->data;
	if (!path) {
		return (-1);
	}

	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	old_size = entry.size;
	u32	end_pos;
	u32	write_off;

	write_off = (u32)offset;
	end_pos = write_off + (u32)count;
	new_size = (end_pos > old_size) ? end_pos : old_size;

	new_data = (u8 *)kmem_calloc(new_size, 1);
	if (!new_data) {
		return (-1);
	}

	if (old_size > 0) {
		u32	bytes_read;

		bytes_read = 0;
		if (chainfs_read_file(path, new_data, old_size,
		    &bytes_read) != 0) {
			kmem_free(new_data);
			return (-1);
		}
	}

	memcpy(new_data + write_off, buf, (unsigned long)count);

	int	result;

	result = chainfs_write_file(path, new_data, new_size);
	kmem_free(new_data);

	if (result != 0) {
		return (-1);
	}

	vn->size = new_size;
	return ((int)count);
}

static int
chainfs_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	char		*path;
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;

	path = (char *)vn->data;
	if (!path) {
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
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_ino = (u64)entry_block;
	vn->size = entry.size;
	return (0);
}

static int
chainfs_vnode_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	char			*path;
	chainfs_file_entry_t	entries[128];
	u32			count, i;

	path = (char *)vn->data;
	if (!path) {
		return (-1);
	}

	count = 0;
	if (chainfs_list_dir(path, entries, 128, &count) != 0) {
		return (-1);
	}

	if (index >= count) {
		return (0);
	}

	int	j;

	for (j = 0; j < 31 && entries[index].name[j] != '\0'; j++) {
		name[j] = entries[index].name[j];
	}
	name[j] = '\0';

	if (type) {
		*type = (entries[index].type == CHAINFS_TYPE_DIR) ?
		    VDIR : VREG;
	}

	return (1);
}

/*
 * Root directory readdir: merges ChainFS root entries with the
 * virtual "dev" directory so that `ls /` shows /dev.
 */
static int
vfs_root_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	chainfs_file_entry_t	entries[128];
	u32			count;
	int			j;

	(void)vn;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	count = 0;
	if (chainfs_list_dir("/", entries, 128, &count) != 0) {
		return (-1);
	}

	if (index < count) {
		for (j = 0; j < 31 && entries[index].name[j] != '\0';
		    j++) {
			name[j] = entries[index].name[j];
		}
		name[j] = '\0';
		if (type) {
			*type = (entries[index].type ==
			    CHAINFS_TYPE_DIR) ? VDIR : VREG;
		}
		return (1);
	}

	if (index == count) {
		name[0] = 'd';
		name[1] = 'e';
		name[2] = 'v';
		name[3] = '\0';
		if (type) {
			*type = VDIR;
		}
		return (1);
	}

	return (0);
}

static vnode_t *
vfs_lookup_chainfs(const char *path)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	vnode_t			*vn;
	int			vtype;
	char			*path_copy;

	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (NULL);
	}

	if (entry.type == CHAINFS_TYPE_DIR) {
		vtype = VDIR;
	} else {
		vtype = VREG;
	}

	vn = vnode_alloc(vtype, entry.name);
	if (!vn) {
		return (NULL);
	}

	path_copy = (char *)kmem_calloc(256, 1);
	if (!path_copy) {
		vnode_release(vn);
		return (NULL);
	}

	int	len;

	len = strlen(path);
	if (len >= 256) {
		len = 255;
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

	/*
	 * The root directory merges ChainFS entries with the virtual
	 * /dev mount point so that `ls /` shows dev alongside regular
	 * files.
	 */
	if (strcmp(path, "/") == 0) {
		vn->readdir_fn = vfs_root_readdir;
	}

	return (vn);
}

static vnode_t *
vfs_lookup_devtmpfs(const char *path)
{
	vnode_t	*vn;

	vn = devtmpfs_lookup(path);
	if (vn) {
		return (vn);
	}

	if (strcmp(path, "/dev") == 0 || strcmp(path, "/dev/") == 0) {
		vn = vnode_alloc(VDIR, "dev");
		if (vn) {
			vn->readdir_fn = devtmpfs_root_readdir;
			vn->data = NULL;
		}
		return (vn);
	}

	return (NULL);
}

int
vfs_resolve(const char *path, vnode_t **out)
{
	vnode_t	*vn;

	if (!path || !out) {
		return (-1);
	}

	if (path[0] == '\0') {
		return (-1);
	}

	if (path_starts_with(path, "/dev")) {
		vn = vfs_lookup_devtmpfs(path);
		if (vn) {
			*out = vn;
			return (0);
		}
		return (-1);
	}

	vn = vfs_lookup_chainfs(path);
	if (vn) {
		*out = vn;
		return (0);
	}

	return (-1);
}

int
vfs_create_file(const char *path)
{
	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	return (chainfs_write_file(path, (const u8 *)"", 0));
}

int
vfs_mkdir(const char *path)
{
	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	return (chainfs_mkdir(path));
}

int
vfs_rmdir(const char *path)
{
	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	return (chainfs_rmdir(path));
}

int
vfs_unlink(const char *path)
{
	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	return (chainfs_delete_file(path));
}

int
vfs_rename(const char *oldpath, const char *newpath)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u8			*buf;
	u32			bytes_read;

	if (path_starts_with(oldpath, "/dev") ||
	    path_starts_with(newpath, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
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

int
vfs_truncate(const char *path, u64 length)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u8			*buf;

	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
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

	buf = (u8 *)kmem_calloc(length, 1);
	if (!buf) {
		return (-1);
	}

	if (entry.size > 0) {
		u32	to_read;
		u32	bytes_read;

		to_read = (entry.size < length) ? entry.size : (u32)length;
		bytes_read = 0;
		if (chainfs_read_file(path, buf, to_read, &bytes_read) != 0) {
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

int
vfs_chdir(const char *path)
{
	vnode_t	*vn;

	if (!path || path[0] == '\0') {
		return (-1);
	}

	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
		return (-1);
	}

	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-1);
	}

	vnode_release(vn);
	return (chainfs_chdir(path));
}

int
vfs_getcwd(char *buf, u32 size)
{
	if (!buf || size == 0) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	if (chainfs_get_current_path(buf, size) == NULL) {
		return (-1);
	}

	return (0);
}

int
vfs_read_file_full(const char *path, u8 *buf, u32 bufsize,
    u32 *bytes_read)
{
	vnode_t		*vn;
	posix_stat_t	st;
	int		n;
	u32		total;
	u8		*tbuf;
	u32		file_size;

	if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
		return (-1);
	}

	if (vn->type == VDIR) {
		vnode_release(vn);
		return (-1);
	}

	if (vnode_stat(vn, &st) != 0) {
		vnode_release(vn);
		return (-1);
	}

	file_size = (u32)st.st_size;
	if (file_size == 0) {
		vnode_release(vn);
		*bytes_read = 0;
		return (0);
	}

	if (file_size > bufsize) {
		vnode_release(vn);
		return (-1);
	}

	tbuf = buf;
	total = 0;
	while (total < file_size) {
		u32	to_read;

		to_read = file_size - total;
		if (to_read > 4096) {
			to_read = 4096;
		}

		n = vnode_read(vn, tbuf + total, to_read, total);
		if (n < 0) {
			vnode_release(vn);
			return (-1);
		}
		if (n == 0) {
			break;
		}
		total += (u32)n;
	}

	vnode_release(vn);
	*bytes_read = total;
	return (0);
}

int
vfs_write_file(const char *path, const u8 *data, u32 size)
{
	if (path_starts_with(path, "/dev")) {
		return (-1);
	}

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	return (chainfs_write_file(path, data, size));
}
