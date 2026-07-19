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
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vnode_t as VFS vnode
$define %type posix_stat_t as POSIX stat record
$define %type vfs_back_ops_t as backend operation table
$define %type cm_key_cb as function pointer with args const char *, void *
$define %type cm_consumer_update_t as function pointer with args u32
$define %type cm_consumer_t as registered registry consumer
$define %type cm_entry_t as registry key or value enumeration record

$define %func cm_str_copy as procedure with args dst, size, src
$define %func cm_path_put as function with args path, pos, size, char
$define %func cm_path_put_name as function with args path, pos, size, text
$define %func cm_path_put_key as function with args path, pos, size, key
$define %func cm_build_node_path as function with args hive, key, out
$define %func cm_build_value_path as function with args hive, key, value, out
$define %func cm_lookup_node as function with args hive, key, out
$define %func cm_lookup_value as function with args hive, key, value, out
$define %func cm_read_exact as function with args hive, key, value, buf
$define %func cm_access_get as function with args flags, op
$define %func cm_access_allowed as function with args level, is_kusr
$define %func cm_mount_registry as function with args void
$define %func cm_init as function with args void
$define %func cm_is_initialized as function with args void
$define %func cm_mount_path as function with args void
$define %func cm_foreach_key as function with args hive, key, cb, ctx
$define %func cm_key_exists as function with args hive, key
$define %func cm_value_info as function with args hive, key, value, type
$define %func cm_enum_entry as function with args hive, key, index, entry
$define %func cm_check_access as function with args hive, key, value, op
$define %func cm_register_consumer as function with args id, name, update
$define %func cm_update_consumer as function with args id, flags
$define %func cm_update_consumer_user as function with args id, flags, kusr
$define %func cm_read_value as function with args hive, key, value, buf
$define %func cm_get_bool as function with args hive, key, value, out
$define %func cm_get_i32 as function with args hive, key, value, out
$define %func cm_get_u32 as function with args hive, key, value, out
$define %func cm_get_u64 as function with args hive, key, value, out
$define %func cm_get_ipv4 as function with args hive, key, value, out
$define %func cm_get_string as function with args hive, key, value, out
$define %func cm_create_key as function with args hive, key
$define %func cm_delete_key as function with args hive, key
$define %func cm_set_value as function with args hive, key, value, type
$define %func cm_delete_value as function with args hive, key, value
$define %func cm_set_bool as function with args hive, key, value, val
$define %func cm_set_i32 as function with args hive, key, value, val
$define %func cm_set_u32 as function with args hive, key, value, val
$define %func cm_set_u64 as function with args hive, key, value, val
$define %func cm_set_ipv4 as function with args hive, key, value, val
$define %func cm_set_string as function with args hive, key, value, val
$define %func cm_get_bool_default as function with args hive, key, value, default
$define %func cm_get_i32_default as function with args hive, key, value, default
$define %func cm_get_u32_default as function with args hive, key, value, default
$define %func cm_get_ipv4_default as function with args hive, key, value, default
$define %func cm_get_string_default as function with args hive, key, value, out

*/

/* !SPACE!

$space %internal cm_str_copy, cm_path_put, cm_path_put_name
$space %internal cm_path_put_key, cm_build_node_path
$space %internal cm_build_value_path, cm_lookup_node, cm_lookup_value
$space %internal cm_read_exact, cm_access_get, cm_access_allowed
$space %internal cm_mount_registry
$space %export cm_init, cm_is_initialized, cm_mount_path
$space %export cm_foreach_key, cm_key_exists, cm_value_info
$space %export cm_enum_entry, cm_check_access
$space %export cm_register_consumer, cm_update_consumer
$space %export cm_update_consumer_user
$space %export cm_read_value, cm_get_bool, cm_get_i32
$space %export cm_get_u32, cm_get_u64, cm_get_ipv4, cm_get_string
$space %export cm_create_key, cm_delete_key, cm_set_value
$space %export cm_delete_value, cm_set_bool, cm_set_i32
$space %export cm_set_u32, cm_set_u64, cm_set_ipv4, cm_set_string
$space %export cm_get_bool_default, cm_get_i32_default
$space %export cm_get_u32_default, cm_get_ipv4_default
$space %export cm_get_string_default

*/
/* configuration mananger */
#include <kernel/api/errno.h>
#include <kernel/cm/cm.h>
#include <kernel/drivers/fs/hivefs/hivefs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	CM_MAX_PATH	256
#define	CM_STORE_PATH	"/conf/registry.hpk"
#define	CM_MAX_CONSUMERS	16

typedef struct cm_consumer {
	int			used;
	u32			id;
	cm_consumer_update_t	update;
	char			name[32];
} cm_consumer_t;

static int	g_cm_initialized;
static int	g_cm_mounted;
static cm_consumer_t	g_cm_consumers[CM_MAX_CONSUMERS];

static void
cm_str_copy(char *dst, u32 size, const char *src)
{
	u32	i;

	if (!dst || size == 0) {
		return;
	}
	if (!src) {
		src = "";
	}
	for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

static int
cm_path_put(char *path, u32 *pos, u32 size, char c)
{
	if (!path || !pos || size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (*pos + 1 >= size) {
		return (-API_ERR_TOO_BIG);
	}
	path[*pos] = c;
	*pos = *pos + 1;
	path[*pos] = '\0';
	return (0);
}

static int
cm_path_put_name(char *path, u32 *pos, u32 size, const char *text)
{
	int	i, ret;

	if (!text || text[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; text[i] != '\0'; i++) {
		if (text[i] == '/' || text[i] == '\\') {
			return (-API_ERR_BAD_VALUE);
		}
		ret = cm_path_put(path, pos, size, text[i]);
		if (ret != 0) {
			return (ret);
		}
	}
	return (0);
}

static int
cm_path_put_key(char *path, u32 *pos, u32 size, const char *key)
{
	int	i, last_sep, ret;
	char	c;

	if (!key || key[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	last_sep = 1;
	for (i = 0; key[i] != '\0'; i++) {
		c = key[i];
		if (c == '.' || c == '/' || c == '\\') {
			if (last_sep) {
				return (-API_ERR_BAD_VALUE);
			}
			ret = cm_path_put(path, pos, size, '/');
			if (ret != 0) {
				return (ret);
			}
			last_sep = 1;
			continue;
		}
		ret = cm_path_put(path, pos, size, c);
		if (ret != 0) {
			return (ret);
		}
		last_sep = 0;
	}
	if (last_sep) {
		return (-API_ERR_BAD_VALUE);
	}
	return (0);
}

static int
cm_build_node_path(const char *hive, const char *key, char *out,
    u32 out_size)
{
	u32	pos;
	int	ret;

	if (!out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	memset(out, 0, out_size);
	pos = (u32)strlen(CM_MOUNT_PATH);
	if (pos + 1 >= out_size) {
		return (-API_ERR_TOO_BIG);
	}
	memcpy(out, CM_MOUNT_PATH, pos);
	out[pos] = '\0';
	ret = cm_path_put(out, &pos, out_size, '/');
	if (ret != 0) {
		return (ret);
	}
	ret = cm_path_put_name(out, &pos, out_size, hive);
	if (ret != 0) {
		return (ret);
	}
	if (key && key[0] != '\0') {
		ret = cm_path_put(out, &pos, out_size, '/');
		if (ret != 0) {
			return (ret);
		}
		ret = cm_path_put_key(out, &pos, out_size, key);
		if (ret != 0) {
			return (ret);
		}
	}

	return (0);
}

static int
cm_build_value_path(const char *hive, const char *key, const char *value,
    char *out, u32 out_size)
{
	u32	pos;
	int	ret;

	ret = cm_build_node_path(hive, key, out, out_size);
	if (ret != 0) {
		return (ret);
	}
	pos = (u32)strlen(out);
	ret = cm_path_put(out, &pos, out_size, '/');
	if (ret != 0) {
		return (ret);
	}
	return (cm_path_put_name(out, &pos, out_size, value));
}

static int
cm_lookup_node(const char *hive, const char *key, vnode_t **out)
{
	const vfs_back_ops_t	*ops;
	vnode_t			*vn;
	char			path[CM_MAX_PATH];
	int			ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	*out = NULL;
	ret = cm_build_node_path(hive, key, path, sizeof(path));
	if (ret != 0) {
		return (ret);
	}

	ops = hivefs_back_ops();
	if (!ops || !ops->lookup) {
		return (-API_ERR_NODEV);
	}
	vn = ops->lookup(path);
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type != VDIR) {
		vnode_release(vn);
		return (-API_ERR_NOT_DIR);
	}

	*out = vn;
	return (0);
}

static int
cm_lookup_value(const char *hive, const char *key, const char *value,
    vnode_t **out)
{
	const vfs_back_ops_t	*ops;
	vnode_t			*vn;
	char			path[CM_MAX_PATH];
	int			ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	*out = NULL;
	ret = cm_build_value_path(hive, key, value, path, sizeof(path));
	if (ret != 0) {
		return (ret);
	}

	ops = hivefs_back_ops();
	if (!ops || !ops->lookup) {
		return (-API_ERR_NODEV);
	}
	vn = ops->lookup(path);
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	if (vn->type == VDIR) {
		vnode_release(vn);
		return (-API_ERR_IS_DIR);
	}

	*out = vn;
	return (0);
}

static int
cm_read_exact(const char *hive, const char *key, const char *value,
    u8 *buf, u32 size)
{
	u32	got;
	int	ret;

	got = 0;
	ret = cm_read_value(hive, key, value, buf, size, &got);
	if (ret != 0) {
		return (ret);
	}
	if (got != size) {
		return (-API_ERR_BAD_VALUE);
	}
	return (0);
}

static u32
cm_access_get(u32 flags, u32 op)
{
	u32	shift;

	switch (op) {
	case CM_ACCESS_READ:
		shift = HIVEFS_ACCESS_READ_SHIFT;
		break;
	case CM_ACCESS_ADD:
		shift = HIVEFS_ACCESS_ADD_SHIFT;
		break;
	case CM_ACCESS_EDIT:
		shift = HIVEFS_ACCESS_EDIT_SHIFT;
		break;
	default:
		return (HIVEFS_ACCESS_INHERIT);
	}
	return ((flags >> shift) & HIVEFS_ACCESS_MASK);
}

static int
cm_access_allowed(u32 level, int is_kusr)
{
	if (level == HIVEFS_ACCESS_USER) {
		return (1);
	}
	if (level == HIVEFS_ACCESS_KUSR && is_kusr) {
		return (1);
	}
	return (0);
}

static int
cm_mount_registry(void)
{
	u64	flags;
	int	ret;

	if (g_cm_mounted || !vfs_is_initialized()) {
		return (0);
	}

	ret = vfs_mkdir("/conf");
	if (ret != 0 && ret != -API_ERR_EXISTS) {
		return (ret);
	}
	ret = hivefs_set_store_path(CM_STORE_PATH);
	if (ret != 0) {
		return (ret);
	}
	ret = hivefs_load_store(CM_STORE_PATH);
	if (ret == -API_ERR_NOT_FOUND) {
		ret = hivefs_sync();
		if (ret != 0) {
			return (ret);
		}
		drivers_log("[CM] registry store created at %s\n",
		    CM_STORE_PATH);
	} else if (ret != 0) {
		return (ret);
	}
	ret = vfs_mkdir(CM_MOUNT_PATH);
	if (ret != 0 && ret != -API_ERR_EXISTS) {
		return (ret);
	}
	flags = VFS_MNT_NOEXEC | VFS_MNT_NODEV | VFS_MNT_KUSR_ONLY;
	ret = vfs_mount_named(CM_MOUNT_PATH, "hivefs", flags);
	if (ret != 0 && ret != -API_ERR_EXISTS) {
		return (ret);
	}

	g_cm_mounted = 1;
	drivers_log("[CM] registry mounted at %s\n", CM_MOUNT_PATH);
	return (0);
}

int
cm_init(void)
{
	int	ret, was_initialized;

	if (!hivefs_is_loaded()) {
		return (-API_ERR_NOT_FOUND);
	}

	was_initialized = g_cm_initialized;
	g_cm_initialized = 1;

	ret = cm_mount_registry();
	if (ret != 0) {
		return (ret);
	}

	if (!was_initialized) {
		drivers_log("[CM] registry ready\n");
	}
	return (0);
}

int
cm_is_initialized(void)
{
	return (g_cm_initialized);
}

const char *
cm_mount_path(void)
{
	return (CM_MOUNT_PATH);
}

int
cm_key_exists(const char *hive, const char *key)
{
	vnode_t	*vn;
	int	ret;

	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}

	ret = cm_lookup_node(hive, key, &vn);
	if (ret != 0) {
		return (ret);
	}
	vnode_release(vn);
	return (0);
}

int
cm_value_info(const char *hive, const char *key, const char *value,
    u32 *type, u32 *size)
{
	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	return (hivefs_value_info(hive, key, value, type, size));
}

int
cm_enum_entry(const char *hive, const char *key, u32 index,
    cm_entry_t *entry)
{
	vnode_t	*vn;
	char	name[32];
	u32	type, size;
	int	vtype, ret;

	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!entry) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = cm_lookup_node(hive, key, &vn);
	if (ret != 0) {
		return (ret);
	}
	memset(name, 0, sizeof(name));
	vtype = 0;
	ret = vnode_readdir(vn, index, name, &vtype);
	vnode_release(vn);
	if (ret <= 0) {
		return (ret);
	}

	memset(entry, 0, sizeof(*entry));
	cm_str_copy(entry->name, sizeof(entry->name), name);
	if (vtype == VDIR) {
		entry->kind = CM_ENTRY_KEY;
		return (1);
	}
	if (vtype != VREG) {
		return (-API_ERR_BAD_VALUE);
	}

	type = 0;
	size = 0;
	ret = cm_value_info(hive, key, name, &type, &size);
	if (ret != 0) {
		return (ret);
	}
	entry->kind = CM_ENTRY_VALUE;
	entry->type = type;
	entry->size = size;
	return (1);
}

int
cm_check_access(const char *hive, const char *key, const char *value,
    u32 op, int is_kusr)
{
	u32	flags, level;
	int	ret;

	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	if (op != CM_ACCESS_READ && op != CM_ACCESS_ADD &&
	    op != CM_ACCESS_EDIT) {
		return (-API_ERR_BAD_VALUE);
	}

	flags = 0;
	ret = hivefs_access_info(hive, key, value, &flags);
	if (ret != 0) {
		return (ret);
	}
	level = cm_access_get(flags, op);
	if (cm_access_allowed(level, is_kusr)) {
		return (0);
	}
	return (-API_ERR_PERM);
}

int
cm_register_consumer(u32 id, const char *name, cm_consumer_update_t update)
{
	cm_consumer_t	*consumer;
	int		i;

	if (id == 0 || !update) {
		return (-API_ERR_BAD_VALUE);
	}

	for (i = 0; i < CM_MAX_CONSUMERS; i++) {
		consumer = &g_cm_consumers[i];
		if (consumer->used && consumer->id == id) {
			consumer->update = update;
			cm_str_copy(consumer->name,
			    sizeof(consumer->name), name);
			return (0);
		}
	}

	for (i = 0; i < CM_MAX_CONSUMERS; i++) {
		consumer = &g_cm_consumers[i];
		if (!consumer->used) {
			memset(consumer, 0, sizeof(*consumer));
			consumer->used = 1;
			consumer->id = id;
			consumer->update = update;
			cm_str_copy(consumer->name,
			    sizeof(consumer->name), name);
			return (0);
		}
	}
	return (-API_ERR_NO_SPACE);
}

int
cm_update_consumer(u32 id, u32 flags)
{
	cm_consumer_t	*consumer;
	int		i;

	if (id == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < CM_MAX_CONSUMERS; i++) {
		consumer = &g_cm_consumers[i];
		if (!consumer->used || consumer->id != id) {
			continue;
		}
		if (!consumer->update) {
			return (-API_ERR_BAD_VALUE);
		}
		return (consumer->update(flags));
	}
	return (-API_ERR_NOT_FOUND);
}

int
cm_update_consumer_user(u32 id, u32 flags, int is_kusr)
{
	if (!is_kusr) {
		return (-API_ERR_PERM);
	}
	return (cm_update_consumer(id, flags));
}

int
cm_foreach_key(const char *hive, const char *key, cm_key_cb cb, void *ctx)
{
	vnode_t	*vn;
	char	name[64];
	u32	index;
	int	type, ret;

	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!cb) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = cm_lookup_node(hive, key, &vn);
	if (ret != 0) {
		return (ret);
	}

	index = 0;
	while (1) {
		memset(name, 0, sizeof(name));
		type = 0;
		ret = vnode_readdir(vn, index, name, &type);
		if (ret <= 0) {
			break;
		}
		index++;
		if (type != VDIR) {
			continue;
		}
		ret = cb(name, ctx);
		if (ret != 0) {
			vnode_release(vn);
			return (ret);
		}
	}

	vnode_release(vn);
	if (ret < 0) {
		return (ret);
	}
	return (0);
}

int
cm_read_value(const char *hive, const char *key, const char *value,
    void *buf, u32 bufsize, u32 *bytes_read)
{
	vnode_t		*vn;
	posix_stat_t	st;
	u32		file_size;
	int		n, ret;

	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	if (!bytes_read || (!buf && bufsize != 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	*bytes_read = 0;

	ret = cm_lookup_value(hive, key, value, &vn);
	if (ret != 0) {
		return (ret);
	}
	ret = vnode_stat(vn, &st);
	if (ret != 0) {
		vnode_release(vn);
		return (ret);
	}
	if (st.st_size < 0 || st.st_size > 0xFFFFFFFFLL) {
		vnode_release(vn);
		return (-API_ERR_FILE_TOO_BIG);
	}

	file_size = (u32)st.st_size;
	*bytes_read = file_size;
	if (!buf && bufsize == 0) {
		vnode_release(vn);
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
cm_get_bool(const char *hive, const char *key, const char *value, int *out)
{
	u8	buf[1];
	int	ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = cm_read_exact(hive, key, value, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = buf[0] ? 1 : 0;
	return (0);
}

int
cm_get_i32(const char *hive, const char *key, const char *value, s32 *out)
{
	u32	val;
	int	ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = cm_get_u32(hive, key, value, &val);
	if (ret != 0) {
		return (ret);
	}
	*out = (s32)val;
	return (0);
}

int
cm_get_u32(const char *hive, const char *key, const char *value, u32 *out)
{
	u8	buf[4];
	int	ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = cm_read_exact(hive, key, value, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = ((u32)buf[0]) | ((u32)buf[1] << 8) |
	    ((u32)buf[2] << 16) | ((u32)buf[3] << 24);
	return (0);
}

int
cm_get_u64(const char *hive, const char *key, const char *value, u64 *out)
{
	u8	buf[8];
	int	ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = cm_read_exact(hive, key, value, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = ((u64)buf[0]) | ((u64)buf[1] << 8) |
	    ((u64)buf[2] << 16) | ((u64)buf[3] << 24) |
	    ((u64)buf[4] << 32) | ((u64)buf[5] << 40) |
	    ((u64)buf[6] << 48) | ((u64)buf[7] << 56);
	return (0);
}

int
cm_get_ipv4(const char *hive, const char *key, const char *value, u32 *out)
{
	u8	buf[4];
	int	ret;

	if (!out) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = cm_read_exact(hive, key, value, buf, sizeof(buf));
	if (ret != 0) {
		return (ret);
	}
	*out = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) |
	    ((u32)buf[2] << 8) | (u32)buf[3];
	return (0);
}

int
cm_get_string(const char *hive, const char *key, const char *value,
    char *out, u32 out_size)
{
	u32	got;
	int	ret;

	if (!out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	got = 0;
	ret = cm_read_value(hive, key, value, out, out_size - 1, &got);
	if (ret != 0) {
		out[0] = '\0';
		return (ret);
	}
	out[got] = '\0';
	return (0);
}

int
cm_create_key(const char *hive, const char *key)
{
	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	return (hivefs_create_key(hive, key));
}

int
cm_delete_key(const char *hive, const char *key)
{
	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	return (hivefs_delete_key(hive, key));
}

int
cm_set_value(const char *hive, const char *key, const char *value,
    u32 type, const void *data, u32 size)
{
	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	return (hivefs_set_value(hive, key, value, type, data, size));
}

int
cm_delete_value(const char *hive, const char *key, const char *value)
{
	if (!g_cm_initialized) {
		return (-API_ERR_NOT_FOUND);
	}
	return (hivefs_delete_value(hive, key, value));
}

int
cm_set_bool(const char *hive, const char *key, const char *value, int val)
{
	u8	buf[1];

	buf[0] = val ? 1 : 0;
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_BOOL,
	    buf, sizeof(buf)));
}

int
cm_set_i32(const char *hive, const char *key, const char *value, s32 val)
{
	u32	uval;
	u8	buf[4];

	uval = (u32)val;
	buf[0] = (u8)(uval & 0xFF);
	buf[1] = (u8)((uval >> 8) & 0xFF);
	buf[2] = (u8)((uval >> 16) & 0xFF);
	buf[3] = (u8)((uval >> 24) & 0xFF);
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_I32,
	    buf, sizeof(buf)));
}

int
cm_set_u32(const char *hive, const char *key, const char *value, u32 val)
{
	u8	buf[4];

	buf[0] = (u8)(val & 0xFF);
	buf[1] = (u8)((val >> 8) & 0xFF);
	buf[2] = (u8)((val >> 16) & 0xFF);
	buf[3] = (u8)((val >> 24) & 0xFF);
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_U32,
	    buf, sizeof(buf)));
}

int
cm_set_u64(const char *hive, const char *key, const char *value, u64 val)
{
	u8	buf[8];

	buf[0] = (u8)(val & 0xFF);
	buf[1] = (u8)((val >> 8) & 0xFF);
	buf[2] = (u8)((val >> 16) & 0xFF);
	buf[3] = (u8)((val >> 24) & 0xFF);
	buf[4] = (u8)((val >> 32) & 0xFF);
	buf[5] = (u8)((val >> 40) & 0xFF);
	buf[6] = (u8)((val >> 48) & 0xFF);
	buf[7] = (u8)((val >> 56) & 0xFF);
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_U64,
	    buf, sizeof(buf)));
}

int
cm_set_ipv4(const char *hive, const char *key, const char *value, u32 val)
{
	u8	buf[4];

	buf[0] = (u8)((val >> 24) & 0xFF);
	buf[1] = (u8)((val >> 16) & 0xFF);
	buf[2] = (u8)((val >> 8) & 0xFF);
	buf[3] = (u8)(val & 0xFF);
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_IPV4,
	    buf, sizeof(buf)));
}

int
cm_set_string(const char *hive, const char *key, const char *value,
    const char *val)
{
	if (!val) {
		return (-API_ERR_BAD_VALUE);
	}
	return (cm_set_value(hive, key, value, HIVEFS_TYPE_STRING,
	    val, (u32)strlen(val)));
}

int
cm_get_bool_default(const char *hive, const char *key, const char *value,
    int default_val)
{
	int	ret, out;

	out = default_val;
	ret = cm_get_bool(hive, key, value, &out);
	if (ret != 0) {
		return (default_val ? 1 : 0);
	}
	return (out ? 1 : 0);
}

s32
cm_get_i32_default(const char *hive, const char *key, const char *value,
    s32 default_val)
{
	s32	out;
	int	ret;

	out = default_val;
	ret = cm_get_i32(hive, key, value, &out);
	if (ret != 0) {
		return (default_val);
	}
	return (out);
}

u32
cm_get_u32_default(const char *hive, const char *key, const char *value,
    u32 default_val)
{
	u32	out;
	int	ret;

	out = default_val;
	ret = cm_get_u32(hive, key, value, &out);
	if (ret != 0) {
		return (default_val);
	}
	return (out);
}

u32
cm_get_ipv4_default(const char *hive, const char *key, const char *value,
    u32 default_val)
{
	u32	out;
	int	ret;

	out = default_val;
	ret = cm_get_ipv4(hive, key, value, &out);
	if (ret != 0) {
		return (default_val);
	}
	return (out);
}

int
cm_get_string_default(const char *hive, const char *key, const char *value,
    char *out, u32 out_size, const char *default_val)
{
	int	ret;

	if (!out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = cm_get_string(hive, key, value, out, out_size);
	if (ret != 0) {
		cm_str_copy(out, out_size, default_val);
	}
	return (ret);
}
