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
$define %type vnode_t as VFS vnode
$define %type vfs_dirent_t as VFS directory entry
$define %type posix_stat_t as POSIX stat record
$define %type vfs_back_ops_t as backend operation table
$define %type hivefs_pack_header_t as cmseed pack header
$define %type hivefs_pack_entry_t as cmseed pack hive entry
$define %type hivefs_hive_header_t as binary hive header
$define %type hivefs_node_entry_t as binary hive node entry
$define %type hivefs_value_entry_t as binary hive value entry
$define %type hivefs_node_t as mutable registry key
$define %type hivefs_value_t as mutable registry value
$define %type hivefs_hive_blob_t as serialized hive blob
$define %type hivefs_hive_t as validated hive view
$define %type hivefs_state_t as loaded hivefs image state
$define %type hivefs_vnode_data_t as vnode lookup handle

$define %func hivefs_mem_eq as function with args ptr, ptr, u32
$define %func hivefs_range_ok as function with args u32, u32, u32
$define %func hivefs_mul_size_ok as function with args u32, u32, u32 *
$define %func hivefs_crc32 as function with args const u8 *, u32
$define %func hivefs_string_ok as function with args hivefs_hive_t *, u32
$define %func hivefs_fixed_name_ok as function with args const char *
$define %func hivefs_string_at as function with args hivefs_hive_t *, u32
$define %func hivefs_name_copy as procedure with args const char *, char *
$define %func hivefs_name_from_part as function with args const char *, u32, char *
$define %func hivefs_name_eq as function with args string, string, u32
$define %func hivefs_strip_prefix as function with args const char *
$define %func hivefs_next_component as function with args path, next, len
$define %func hivefs_next_key_component as function with args path, next, len
$define %func hivefs_find_hive_name as function with args const char *
$define %func hivefs_find_hive as function with args const char *, u32
$define %func hivefs_find_child as function with args hive, parent, name, len
$define %func hivefs_find_value as function with args hive, node, name, len
$define %func hivefs_access_get as function with args flags, shift
$define %func hivefs_access_merge_one as procedure with args flags, shift, level
$define %func hivefs_access_merge as procedure with args flags, read, add, edit
$define %func hivefs_access_pack as function with args read, add, edit
$define %func hivefs_free_hive as procedure with args hive
$define %func hivefs_reserve_nodes as function with args hive, needed
$define %func hivefs_reserve_values as function with args hive, needed
$define %func hivefs_import_hive as function with args hive
$define %func hivefs_find_node_path as function with args hive, key, create, out
$define %func hivefs_path_append_key as function with args key, size, part, len
$define %func hivefs_path_split_key as function with args path, hive, key
$define %func hivefs_path_split_value as function with args path, hive, key, value
$define %func hivefs_set_value_memory as function with args hive, idx, type, data
$define %func hivefs_free_state as procedure with args hivefs_state_t *
$define %func hivefs_pack_load_into as function with args const void *, u32, state
$define %func hivefs_pack_load as function with args const void *, u32, int
$define %func hivefs_string_store_add as function with args char *, u32 *, const char *
$define %func hivefs_build_hive_blob as function with args hive, blob
$define %func hivefs_free_hive_blobs as procedure with args blobs, count
$define %func hivefs_pack_align as function with args u32
$define %func hivefs_build_pack as function with args out, out_size
$define %func hivefs_commit as function with args void
$define %func hivefs_validate_hive as function with args base, size, out
$define %func hivefs_make_vnode as function with args kind, hive, node, value
$define %func hivefs_vnode_read as function with args vnode, buf, count, off
$define %func hivefs_vnode_write as function with args vnode, buf, count, off
$define %func hivefs_vnode_stat as function with args vnode_t *, posix_stat_t *
$define %func hivefs_vnode_readdir as function with args vnode, idx, name, type
$define %func hivefs_vnode_listdir as function with args vnode, start, entries, max_entries, count
$define %func hivefs_back_init as function with args void
$define %func hivefs_back_lookup as function with args const char *
$define %func hivefs_back_create_file as function with args const char *
$define %func hivefs_back_mkdir as function with args const char *
$define %func hivefs_back_rmdir as function with args const char *
$define %func hivefs_back_unlink as function with args const char *
$define %func hivefs_back_truncate as function with args const char *, u64
$define %func hivefs_back_write_file as function with args const char *, const u8 *, u32
$define %func hivefs_reset as procedure with args void
$define %func hivefs_load_pack as function with args const void *, u32
$define %func hivefs_is_loaded as function with args void
$define %func hivefs_set_store_path as function with args const char *
$define %func hivefs_load_store as function with args const char *
$define %func hivefs_sync as function with args void
$define %func hivefs_create_key as function with args hive, key
$define %func hivefs_delete_key as function with args hive, key
$define %func hivefs_set_value as function with args hive, key, value, type
$define %func hivefs_delete_value as function with args hive, key, value
$define %func hivefs_value_info as function with args hive, key, value, type
$define %func hivefs_access_info as function with args hive, key, value, flags
$define %func hivefs_back_ops as function with args void

*/

/* !SPACE!

$space %internal hivefs_mem_eq, hivefs_range_ok, hivefs_mul_size_ok
$space %internal hivefs_crc32, hivefs_string_ok, hivefs_fixed_name_ok
$space %internal hivefs_string_at, hivefs_name_copy, hivefs_name_from_part
$space %internal hivefs_name_eq
$space %internal hivefs_strip_prefix, hivefs_next_component
$space %internal hivefs_next_key_component, hivefs_find_hive_name
$space %internal hivefs_find_hive, hivefs_find_child, hivefs_find_value
$space %internal hivefs_access_get, hivefs_access_merge_one
$space %internal hivefs_access_merge, hivefs_access_pack
$space %internal hivefs_free_hive, hivefs_reserve_nodes
$space %internal hivefs_reserve_values, hivefs_import_hive
$space %internal hivefs_find_node_path, hivefs_path_append_key
$space %internal hivefs_path_split_key, hivefs_path_split_value
$space %internal hivefs_set_value_memory, hivefs_free_state
$space %internal hivefs_pack_load_into, hivefs_pack_load
$space %internal hivefs_string_store_add, hivefs_build_hive_blob
$space %internal hivefs_free_hive_blobs, hivefs_pack_align
$space %internal hivefs_build_pack, hivefs_commit
$space %internal hivefs_validate_hive, hivefs_make_vnode
$space %internal hivefs_vnode_read, hivefs_vnode_write, hivefs_vnode_stat
$space %internal hivefs_vnode_readdir, hivefs_vnode_listdir
$space %internal hivefs_back_init, hivefs_back_lookup
$space %internal hivefs_back_create_file, hivefs_back_mkdir
$space %internal hivefs_back_rmdir, hivefs_back_unlink
$space %internal hivefs_back_truncate, hivefs_back_write_file
$space %export hivefs_reset, hivefs_load_pack
$space %export hivefs_is_loaded, hivefs_set_store_path
$space %export hivefs_load_store, hivefs_sync
$space %export hivefs_create_key, hivefs_delete_key
$space %export hivefs_set_value, hivefs_delete_value
$space %export hivefs_value_info, hivefs_access_info
$space %export hivefs_back_ops

*/

#include <kernel/api/errno.h>
#include <kernel/drivers/fs/hivefs/hivefs.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	HIVEFS_FORMAT_VERSION	1
#define	HIVEFS_MAX_HIVES	16
#define	HIVEFS_NO_INDEX		0xFFFFFFFFU
#define	HIVEFS_NAME_SIZE	32
#define	HIVEFS_PATH_SIZE	256

#define	HIVEFS_KIND_ROOT	1
#define	HIVEFS_KIND_HIVE	2
#define	HIVEFS_KIND_NODE	3
#define	HIVEFS_KIND_VALUE	4

typedef struct hivefs_pack_header {
	u8	magic[8];
	u32	version;
	u32	header_size;
	u32	hive_count;
	u32	table_off;
	u32	data_off;
	u32	total_size;
	u32	reserved;
	u32	crc32;
} __attribute__((packed)) hivefs_pack_header_t;

typedef struct hivefs_pack_entry {
	char	name[HIVEFS_NAME_SIZE];
	u32	offset;
	u32	size;
	u32	flags;
	u32	crc32;
} __attribute__((packed)) hivefs_pack_entry_t;

typedef struct hivefs_hive_header {
	u8	magic[8];
	u32	version;
	u32	header_size;
	u32	flags;
	u32	node_count;
	u32	value_count;
	u32	node_off;
	u32	value_off;
	u32	string_off;
	u32	string_size;
	u32	data_off;
	u32	data_size;
	u32	total_size;
	u32	crc32;
} __attribute__((packed)) hivefs_hive_header_t;

typedef struct hivefs_node_entry {
	u32	parent;
	u32	name_off;
	u32	flags;
	u32	first_child;
	u32	child_count;
	u32	first_value;
	u32	value_count;
} __attribute__((packed)) hivefs_node_entry_t;

typedef struct hivefs_value_entry {
	u32	node;
	u32	name_off;
	u32	type;
	u32	flags;
	u32	data_off;
	u32	size;
} __attribute__((packed)) hivefs_value_entry_t;

typedef struct hivefs_node {
	char	name[HIVEFS_NAME_SIZE];
	u32	parent;
	u32	flags;
	int	deleted;
} hivefs_node_t;

typedef struct hivefs_value {
	char	name[HIVEFS_NAME_SIZE];
	u8	*data;
	u32	node;
	u32	type;
	u32	flags;
	u32	size;
	int	deleted;
} hivefs_value_t;

typedef struct hivefs_hive_blob {
	u8	*data;
	u32	size;
} hivefs_hive_blob_t;

typedef struct hivefs_hive {
	char				name[HIVEFS_NAME_SIZE];
	const u8			*base;
	u32				size;
	u32				flags;
	const hivefs_hive_header_t	*raw_header;
	const hivefs_node_entry_t	*raw_nodes;
	const hivefs_value_entry_t	*raw_values;
	const char			*raw_strings;
	const u8			*raw_data;
	hivefs_node_t			*nodes;
	hivefs_value_t			*values;
	u32				node_count;
	u32				node_cap;
	u32				value_count;
	u32				value_cap;
	int				dirty;
} hivefs_hive_t;

typedef struct hivefs_state {
	const u8			*pack;
	const hivefs_pack_header_t	*header;
	const hivefs_pack_entry_t	*entries;
	hivefs_hive_t			hives[HIVEFS_MAX_HIVES];
	u32				size;
	u32				hive_count;
	int				loaded;
	int				pack_owned;
	int				syncing;
	char				store_path[HIVEFS_PATH_SIZE];
} hivefs_state_t;

typedef struct hivefs_vnode_data {
	int	kind;
	u32	hive;
	u32	node;
	u32	value;
} hivefs_vnode_data_t;

static hivefs_state_t	g_hivefs;

static const u8	hivefs_pack_magic[8] = {
	'O', 'T', 'S', 'H', 'P', 'K', '1', '\0'
};
static const u8	hivefs_hive_magic[8] = {
	'O', 'T', 'S', 'H', 'I', 'V', '1', '\0'
};

static int	hivefs_back_init(void);
static vnode_t	*hivefs_back_lookup(const char *path);
static int	hivefs_back_create_file(const char *path);
static int	hivefs_back_mkdir(const char *path);
static int	hivefs_back_rmdir(const char *path);
static int	hivefs_back_unlink(const char *path);
static int	hivefs_back_truncate(const char *path, u64 length);
static int	hivefs_back_write_file(const char *path, const u8 *data,
		    u32 size);
static int	hivefs_vnode_read(vnode_t *vn, void *buf, u64 count,
		    u64 offset);
static int	hivefs_vnode_write(vnode_t *vn, const void *buf, u64 count,
		    u64 offset);
static int	hivefs_vnode_stat(vnode_t *vn, posix_stat_t *st);
static int	hivefs_vnode_readdir(vnode_t *vn, u32 index, char *name,
		    int *type);
static int	hivefs_vnode_listdir(vnode_t *vn, u32 start,
		    vfs_dirent_t *entries, u32 max_entries, u32 *count);
static int	hivefs_commit(void);
static int	hivefs_pack_load(const void *data, u32 size, int owned);

static const vfs_back_ops_t	hivefs_ops = {
	.name = "hivefs",
	.init = hivefs_back_init,
	.lookup = hivefs_back_lookup,
	.create_file = hivefs_back_create_file,
	.mkdir = hivefs_back_mkdir,
	.rmdir = hivefs_back_rmdir,
	.unlink = hivefs_back_unlink,
	.rename = NULL,
	.truncate = hivefs_back_truncate,
	.symlink = NULL,
	.link = NULL,
	.chdir = NULL,
	.getcwd = NULL,
	.write_file = hivefs_back_write_file,
	.umount = NULL,
};

static int
hivefs_mem_eq(const void *a, const void *b, u32 size)
{
	const u8	*pa, *pb;
	u32	i;

	pa = (const u8 *)a;
	pb = (const u8 *)b;
	for (i = 0; i < size; i++) {
		if (pa[i] != pb[i]) {
			return (0);
		}
	}
	return (1);
}

static int
hivefs_range_ok(u32 off, u32 len, u32 total)
{
	if (off > total) {
		return (0);
	}
	if (len > total - off) {
		return (0);
	}
	return (1);
}

static int
hivefs_mul_size_ok(u32 count, u32 size, u32 *out)
{
	if (!out || size == 0) {
		return (0);
	}
	if (count > 0xFFFFFFFFU / size) {
		return (0);
	}
	*out = count * size;
	return (1);
}

static u32
hivefs_crc32(const u8 *data, u32 size)
{
	u32	crc;
	u32	i, bit;

	crc = 0xFFFFFFFFU;
	for (i = 0; i < size; i++) {
		crc ^= data[i];
		for (bit = 0; bit < 8; bit++) {
			if ((crc & 1U) != 0) {
				crc = (crc >> 1) ^ 0xEDB88320U;
			} else {
				crc = crc >> 1;
			}
		}
	}
	return (~crc);
}

static int
hivefs_string_ok(hivefs_hive_t *hive, u32 off)
{
	u32	i;

	if (!hive || !hive->raw_header ||
	    off >= hive->raw_header->string_size) {
		return (0);
	}
	for (i = off; i < hive->raw_header->string_size; i++) {
		if (hive->raw_strings[i] == '\0') {
			return (1);
		}
	}
	return (0);
}

static int
hivefs_fixed_name_ok(const char *name)
{
	int	i;

	if (!name) {
		return (0);
	}
	for (i = 0; i < HIVEFS_NAME_SIZE; i++) {
		if (name[i] == '\0') {
			return (1);
		}
	}
	return (0);
}

static const char *
hivefs_string_at(hivefs_hive_t *hive, u32 off)
{
	if (!hivefs_string_ok(hive, off)) {
		return ("");
	}
	return (hive->raw_strings + off);
}

static void
hivefs_name_copy(const char *src, char *dst)
{
	int	i;

	if (!dst) {
		return;
	}
	if (!src) {
		dst[0] = '\0';
		return;
	}
	for (i = 0; i < HIVEFS_NAME_SIZE - 1 && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

static int
hivefs_name_from_part(const char *part, u32 part_len, char *out)
{
	u32	i;

	if (!part || !out || part_len == 0 ||
	    part_len >= HIVEFS_NAME_SIZE) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < part_len; i++) {
		if (part[i] == '/' || part[i] == '\\' ||
		    part[i] == '.') {
			return (-API_ERR_BAD_VALUE);
		}
		out[i] = part[i];
	}
	out[part_len] = '\0';
	return (0);
}

static int
hivefs_name_eq(const char *name, const char *part, u32 part_len)
{
	u32	i;

	if (!name || !part) {
		return (0);
	}
	for (i = 0; i < part_len; i++) {
		if (name[i] == '\0' || name[i] != part[i]) {
			return (0);
		}
	}
	return (name[part_len] == '\0');
}

static const char *
hivefs_strip_prefix(const char *path)
{
	const char	*p, *start;
	u32		len, i;

	if (!path || path[0] == '\0') {
		return ("");
	}
	if (path[0] != '/') {
		return (path);
	}

	p = path + 1;
	while (*p != '\0') {
		while (*p == '/') {
			p++;
		}
		start = p;
		while (*p != '\0' && *p != '/') {
			p++;
		}
		len = (u32)(p - start);
		for (i = 0; i < g_hivefs.hive_count; i++) {
			if (hivefs_name_eq(g_hivefs.hives[i].name,
			    start, len)) {
				return (start);
			}
		}
	}
	return ("");
}

static const char *
hivefs_next_component(const char *path, const char **next, u32 *len)
{
	const char	*start;

	if (!path) {
		if (next) {
			*next = "";
		}
		if (len) {
			*len = 0;
		}
		return ("");
	}

	while (*path == '/') {
		path++;
	}
	start = path;
	while (*path != '\0' && *path != '/') {
		path++;
	}
	if (len) {
		*len = (u32)(path - start);
	}
	while (*path == '/') {
		path++;
	}
	if (next) {
		*next = path;
	}
	return (start);
}

static const char *
hivefs_next_key_component(const char *path, const char **next, u32 *len)
{
	const char	*start;

	if (!path) {
		if (next) {
			*next = "";
		}
		if (len) {
			*len = 0;
		}
		return ("");
	}

	while (*path == '.' || *path == '/' || *path == '\\') {
		path++;
	}
	start = path;
	while (*path != '\0' && *path != '.' &&
	    *path != '/' && *path != '\\') {
		path++;
	}
	if (len) {
		*len = (u32)(path - start);
	}
	while (*path == '.' || *path == '/' || *path == '\\') {
		path++;
	}
	if (next) {
		*next = path;
	}
	return (start);
}

static int
hivefs_find_hive(const char *part, u32 part_len)
{
	u32	i;

	for (i = 0; i < g_hivefs.hive_count; i++) {
		if (hivefs_name_eq(g_hivefs.hives[i].name,
		    part, part_len)) {
			return ((int)i);
		}
	}
	return (-1);
}

static int
hivefs_find_hive_name(const char *name)
{
	u32	len;

	if (!name || name[0] == '\0') {
		return (-1);
	}
	len = (u32)strlen(name);
	return (hivefs_find_hive(name, len));
}

static int
hivefs_find_child(hivefs_hive_t *hive, u32 parent, const char *part,
    u32 part_len)
{
	const char	*name;
	u32		i;

	if (!hive || parent >= hive->node_count ||
	    hive->nodes[parent].deleted) {
		return (-1);
	}
	for (i = 0; i < hive->node_count; i++) {
		if (hive->nodes[i].deleted) {
			continue;
		}
		if (hive->nodes[i].parent != parent) {
			continue;
		}
		name = hive->nodes[i].name;
		if (hivefs_name_eq(name, part, part_len)) {
			return ((int)i);
		}
	}
	return (-1);
}

static int
hivefs_find_value(hivefs_hive_t *hive, u32 node, const char *part,
    u32 part_len)
{
	const char	*name;
	u32		i;

	if (!hive || node >= hive->node_count ||
	    hive->nodes[node].deleted) {
		return (-1);
	}
	for (i = 0; i < hive->value_count; i++) {
		if (hive->values[i].deleted) {
			continue;
		}
		if (hive->values[i].node != node) {
			continue;
		}
		name = hive->values[i].name;
		if (hivefs_name_eq(name, part, part_len)) {
			return ((int)i);
		}
	}
	return (-1);
}

static u32
hivefs_access_get(u32 flags, u32 shift)
{
	return ((flags >> shift) & HIVEFS_ACCESS_MASK);
}

static void
hivefs_access_merge_one(u32 flags, u32 shift, u32 *level)
{
	u32	got;

	if (!level || *level != HIVEFS_ACCESS_INHERIT) {
		return;
	}
	got = hivefs_access_get(flags, shift);
	if (got != HIVEFS_ACCESS_INHERIT) {
		*level = got;
	}
}

static void
hivefs_access_merge(u32 flags, u32 *read, u32 *add, u32 *edit)
{
	hivefs_access_merge_one(flags, HIVEFS_ACCESS_READ_SHIFT, read);
	hivefs_access_merge_one(flags, HIVEFS_ACCESS_ADD_SHIFT, add);
	hivefs_access_merge_one(flags, HIVEFS_ACCESS_EDIT_SHIFT, edit);
}

static u32
hivefs_access_pack(u32 read, u32 add, u32 edit)
{
	return ((read << HIVEFS_ACCESS_READ_SHIFT) |
	    (add << HIVEFS_ACCESS_ADD_SHIFT) |
	    (edit << HIVEFS_ACCESS_EDIT_SHIFT));
}

static void
hivefs_free_hive(hivefs_hive_t *hive)
{
	u32	i;

	if (!hive) {
		return;
	}
	if (hive->values) {
		for (i = 0; i < hive->value_count; i++) {
			if (hive->values[i].data) {
				kmem_free(hive->values[i].data);
			}
		}
		kmem_free(hive->values);
	}
	if (hive->nodes) {
		kmem_free(hive->nodes);
	}
	hive->nodes = NULL;
	hive->values = NULL;
	hive->node_count = 0;
	hive->node_cap = 0;
	hive->value_count = 0;
	hive->value_cap = 0;
}

static void
hivefs_free_state(hivefs_state_t *state)
{
	u32	i;

	if (!state) {
		return;
	}
	for (i = 0; i < state->hive_count && i < HIVEFS_MAX_HIVES; i++) {
		hivefs_free_hive(&state->hives[i]);
	}
	if (state->pack_owned && state->pack) {
		kmem_free((void *)state->pack);
	}
	memset(state, 0, sizeof(hivefs_state_t));
}

static int
hivefs_reserve_nodes(hivefs_hive_t *hive, u32 needed)
{
	hivefs_node_t	*nodes;
	u32		old_cap, new_cap;

	if (!hive) {
		return (-API_ERR_BAD_VALUE);
	}
	if (needed <= hive->node_cap) {
		return (0);
	}
	old_cap = hive->node_cap;
	new_cap = old_cap ? old_cap * 2 : 8;
	while (new_cap < needed) {
		if (new_cap > 0x80000000U) {
			return (-API_ERR_TOO_BIG);
		}
		new_cap *= 2;
	}
	nodes = (hivefs_node_t *)kmem_realloc(hive->nodes,
	    sizeof(hivefs_node_t) * new_cap);
	if (!nodes) {
		return (-API_ERR_NO_MEMORY);
	}
	memset(nodes + old_cap, 0,
	    sizeof(hivefs_node_t) * (new_cap - old_cap));
	hive->nodes = nodes;
	hive->node_cap = new_cap;
	return (0);
}

static int
hivefs_reserve_values(hivefs_hive_t *hive, u32 needed)
{
	hivefs_value_t	*values;
	u32		old_cap, new_cap;

	if (!hive) {
		return (-API_ERR_BAD_VALUE);
	}
	if (needed <= hive->value_cap) {
		return (0);
	}
	old_cap = hive->value_cap;
	new_cap = old_cap ? old_cap * 2 : 8;
	while (new_cap < needed) {
		if (new_cap > 0x80000000U) {
			return (-API_ERR_TOO_BIG);
		}
		new_cap *= 2;
	}
	values = (hivefs_value_t *)kmem_realloc(hive->values,
	    sizeof(hivefs_value_t) * new_cap);
	if (!values) {
		return (-API_ERR_NO_MEMORY);
	}
	memset(values + old_cap, 0,
	    sizeof(hivefs_value_t) * (new_cap - old_cap));
	hive->values = values;
	hive->value_cap = new_cap;
	return (0);
}

static int
hivefs_import_hive(hivefs_hive_t *hive)
{
	const hivefs_node_entry_t	*node;
	const hivefs_value_entry_t	*value;
	u8				*buf;
	u32				i;
	int				ret;

	if (!hive || !hive->raw_header) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = hivefs_reserve_nodes(hive, hive->raw_header->node_count + 8);
	if (ret != 0) {
		return (ret);
	}
	ret = hivefs_reserve_values(hive, hive->raw_header->value_count + 8);
	if (ret != 0) {
		return (ret);
	}

	hive->node_count = hive->raw_header->node_count;
	for (i = 0; i < hive->raw_header->node_count; i++) {
		node = &hive->raw_nodes[i];
		hivefs_name_copy(hivefs_string_at(hive, node->name_off),
		    hive->nodes[i].name);
		hive->nodes[i].parent = node->parent;
		hive->nodes[i].flags = node->flags;
		hive->nodes[i].deleted = 0;
	}

	hive->value_count = hive->raw_header->value_count;
	for (i = 0; i < hive->raw_header->value_count; i++) {
		value = &hive->raw_values[i];
		hivefs_name_copy(hivefs_string_at(hive, value->name_off),
		    hive->values[i].name);
		hive->values[i].node = value->node;
		hive->values[i].type = value->type;
		hive->values[i].flags = value->flags;
		hive->values[i].size = value->size;
		hive->values[i].deleted = 0;
		hive->values[i].data = NULL;
		if (value->size == 0) {
			continue;
		}
		buf = (u8 *)kmem_alloc(value->size);
		if (!buf) {
			return (-API_ERR_NO_MEMORY);
		}
		memcpy(buf, hive->raw_data + value->data_off, value->size);
		hive->values[i].data = buf;
	}

	return (0);
}

static int
hivefs_find_node_path(hivefs_hive_t *hive, const char *key, int create,
    u32 *out)
{
	const char	*part, *next;
	char		name[HIVEFS_NAME_SIZE];
	u32		part_len, node, idx;
	int		found, ret;

	if (!hive || !out || hive->node_count == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!key || key[0] == '\0') {
		*out = 0;
		return (0);
	}

	node = 0;
	next = key;
	while (next[0] != '\0') {
		part = hivefs_next_key_component(next, &next, &part_len);
		if (part_len == 0) {
			return (-API_ERR_BAD_VALUE);
		}
		found = hivefs_find_child(hive, node, part, part_len);
		if (found >= 0) {
			node = (u32)found;
			continue;
		}
		if (!create) {
			return (-API_ERR_NOT_FOUND);
		}
		if (hivefs_find_value(hive, node, part, part_len) >= 0) {
			return (-API_ERR_EXISTS);
		}
		ret = hivefs_name_from_part(part, part_len, name);
		if (ret != 0) {
			return (ret);
		}
		ret = hivefs_reserve_nodes(hive, hive->node_count + 1);
		if (ret != 0) {
			return (ret);
		}
		idx = hive->node_count++;
		hivefs_name_copy(name, hive->nodes[idx].name);
		hive->nodes[idx].parent = node;
		hive->nodes[idx].flags = 0;
		hive->nodes[idx].deleted = 0;
		hive->dirty = 1;
		node = idx;
	}

	*out = node;
	return (0);
}

static int
hivefs_path_append_key(char *key, u32 size, const char *part, u32 part_len)
{
	u32	pos;

	if (!key || !part || part_len == 0 || size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	pos = (u32)strlen(key);
	if (pos != 0) {
		if (pos + 1 >= size) {
			return (-API_ERR_TOO_BIG);
		}
		key[pos++] = '.';
		key[pos] = '\0';
	}
	if (pos + part_len >= size) {
		return (-API_ERR_TOO_BIG);
	}
	memcpy(key + pos, part, part_len);
	key[pos + part_len] = '\0';
	return (0);
}

static int
hivefs_path_split_key(const char *path, char *hive_name, char *key)
{
	const char	*inner, *part, *next;
	u32		part_len;
	int		ret;

	if (!hive_name || !key) {
		return (-API_ERR_BAD_VALUE);
	}
	hive_name[0] = '\0';
	key[0] = '\0';
	inner = hivefs_strip_prefix(path);
	part = hivefs_next_component(inner, &next, &part_len);
	if (hivefs_name_from_part(part, part_len, hive_name) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	while (next[0] != '\0') {
		part = hivefs_next_component(next, &next, &part_len);
		if (part_len == 0) {
			return (-API_ERR_BAD_VALUE);
		}
		ret = hivefs_path_append_key(key, HIVEFS_PATH_SIZE,
		    part, part_len);
		if (ret != 0) {
			return (ret);
		}
	}
	return (0);
}

static int
hivefs_path_split_value(const char *path, char *hive_name, char *key,
    char *value_name)
{
	const char	*inner, *part, *next;
	char		last[HIVEFS_NAME_SIZE];
	u32		part_len;
	int		ret, have_last;

	if (!hive_name || !key || !value_name) {
		return (-API_ERR_BAD_VALUE);
	}
	hive_name[0] = '\0';
	key[0] = '\0';
	value_name[0] = '\0';
	last[0] = '\0';
	have_last = 0;

	inner = hivefs_strip_prefix(path);
	part = hivefs_next_component(inner, &next, &part_len);
	if (hivefs_name_from_part(part, part_len, hive_name) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	while (next[0] != '\0') {
		part = hivefs_next_component(next, &next, &part_len);
		if (part_len == 0) {
			return (-API_ERR_BAD_VALUE);
		}
		if (have_last) {
			ret = hivefs_path_append_key(key,
			    HIVEFS_PATH_SIZE, last, (u32)strlen(last));
			if (ret != 0) {
				return (ret);
			}
		}
		ret = hivefs_name_from_part(part, part_len, last);
		if (ret != 0) {
			return (ret);
		}
		have_last = 1;
	}
	if (!have_last) {
		return (-API_ERR_BAD_VALUE);
	}
	hivefs_name_copy(last, value_name);
	return (0);
}

static int
hivefs_set_value_memory(hivefs_hive_t *hive, u32 idx, u32 type,
    const void *data, u32 size)
{
	u8	*buf;

	if (!hive || idx >= hive->value_count || (!data && size != 0) ||
	    type < HIVEFS_TYPE_STRING || type > HIVEFS_TYPE_MULTI_STRING) {
		return (-API_ERR_BAD_VALUE);
	}

	buf = NULL;
	if (size != 0) {
		buf = (u8 *)kmem_alloc(size);
		if (!buf) {
			return (-API_ERR_NO_MEMORY);
		}
		memcpy(buf, data, size);
	}
	if (hive->values[idx].data) {
		kmem_free(hive->values[idx].data);
	}
	hive->values[idx].data = buf;
	hive->values[idx].size = size;
	hive->values[idx].type = type;
	hive->values[idx].deleted = 0;
	hive->dirty = 1;
	return (0);
}

static int
hivefs_string_store_add(char *strings, u32 *pos, u32 max,
    const char *name, u32 *out)
{
	u32	len;

	if (!strings || !pos || !name || !out) {
		return (-API_ERR_BAD_VALUE);
	}
	len = (u32)strlen(name);
	if (*pos > max || len + 1 > max - *pos) {
		return (-API_ERR_TOO_BIG);
	}
	*out = *pos;
	memcpy(strings + *pos, name, len);
	strings[*pos + len] = '\0';
	*pos = *pos + len + 1;
	return (0);
}

static int
hivefs_build_hive_blob(hivefs_hive_t *hive, hivefs_hive_blob_t *blob)
{
	hivefs_hive_header_t	*hdr;
	hivefs_node_entry_t	*nodes;
	hivefs_value_entry_t	*values;
	hivefs_node_t		*node;
	hivefs_value_t		*value;
	char			*strings;
	u8			*data_area;
	u32			*node_map;
	u32			node_bytes, value_bytes;
	u32			node_count, value_count;
	u32			string_size, string_pos;
	u32			data_size, data_pos;
	u32			node_off, value_off, string_off, data_off;
	u32			total_size, idx, nidx, pidx, i;
	u64			total64;
	int			ret;

	if (!hive || !blob || hive->node_count == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	memset(blob, 0, sizeof(*blob));
	node_map = (u32 *)kmem_alloc(sizeof(u32) * hive->node_count);
	if (!node_map) {
		return (-API_ERR_NO_MEMORY);
	}
	for (i = 0; i < hive->node_count; i++) {
		node_map[i] = HIVEFS_NO_INDEX;
	}

	node_count = 0;
	value_count = 0;
	string_size = 0;
	data_size = 0;
	for (i = 0; i < hive->node_count; i++) {
		node = &hive->nodes[i];
		if (node->deleted) {
			continue;
		}
		if (i != 0 && (node->parent >= hive->node_count ||
		    hive->nodes[node->parent].deleted)) {
			continue;
		}
		node_map[i] = node_count++;
		string_size += (u32)strlen(node->name) + 1;
	}
	if (node_count == 0 || node_map[0] == HIVEFS_NO_INDEX) {
		kmem_free(node_map);
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < hive->value_count; i++) {
		value = &hive->values[i];
		if (value->deleted || value->node >= hive->node_count ||
		    node_map[value->node] == HIVEFS_NO_INDEX) {
			continue;
		}
		value_count++;
		string_size += (u32)strlen(value->name) + 1;
		if (value->size > 0xFFFFFFFFU - data_size) {
			kmem_free(node_map);
			return (-API_ERR_TOO_BIG);
		}
		data_size += value->size;
	}

	if (!hivefs_mul_size_ok(node_count,
	    (u32)sizeof(hivefs_node_entry_t), &node_bytes) ||
	    !hivefs_mul_size_ok(value_count,
	    (u32)sizeof(hivefs_value_entry_t), &value_bytes)) {
		kmem_free(node_map);
		return (-API_ERR_TOO_BIG);
	}

	node_off = (u32)sizeof(hivefs_hive_header_t);
	value_off = node_off + node_bytes;
	string_off = value_off + value_bytes;
	data_off = string_off + string_size;
	total64 = (u64)data_off + data_size;
	if (total64 > 0xFFFFFFFFULL) {
		kmem_free(node_map);
		return (-API_ERR_TOO_BIG);
	}
	total_size = (u32)total64;

	blob->data = (u8 *)kmem_calloc(total_size, 1);
	if (!blob->data) {
		kmem_free(node_map);
		return (-API_ERR_NO_MEMORY);
	}
	blob->size = total_size;

	hdr = (hivefs_hive_header_t *)blob->data;
	nodes = (hivefs_node_entry_t *)(blob->data + node_off);
	values = (hivefs_value_entry_t *)(blob->data + value_off);
	strings = (char *)(blob->data + string_off);
	data_area = blob->data + data_off;
	string_pos = 0;
	data_pos = 0;

	for (i = 0; i < hive->node_count; i++) {
		if (node_map[i] == HIVEFS_NO_INDEX) {
			continue;
		}
		node = &hive->nodes[i];
		idx = node_map[i];
		nodes[idx].parent = (i == 0) ? HIVEFS_NO_INDEX :
		    node_map[node->parent];
		nodes[idx].flags = node->flags;
		nodes[idx].first_child = HIVEFS_NO_INDEX;
		nodes[idx].first_value = HIVEFS_NO_INDEX;
		ret = hivefs_string_store_add(strings, &string_pos,
		    string_size, node->name, &nodes[idx].name_off);
		if (ret != 0) {
			kmem_free(node_map);
			kmem_free(blob->data);
			memset(blob, 0, sizeof(*blob));
			return (ret);
		}
	}

	for (i = 1; i < hive->node_count; i++) {
		if (node_map[i] == HIVEFS_NO_INDEX) {
			continue;
		}
		pidx = node_map[hive->nodes[i].parent];
		nidx = node_map[i];
		if (nodes[pidx].child_count == 0) {
			nodes[pidx].first_child = nidx;
		}
		nodes[pidx].child_count++;
	}

	idx = 0;
	for (i = 0; i < hive->value_count; i++) {
		value = &hive->values[i];
		if (value->deleted || value->node >= hive->node_count ||
		    node_map[value->node] == HIVEFS_NO_INDEX) {
			continue;
		}
		nidx = node_map[value->node];
		values[idx].node = nidx;
		values[idx].type = value->type;
		values[idx].flags = value->flags;
		values[idx].data_off = data_pos;
		values[idx].size = value->size;
		ret = hivefs_string_store_add(strings, &string_pos,
		    string_size, value->name, &values[idx].name_off);
		if (ret != 0) {
			kmem_free(node_map);
			kmem_free(blob->data);
			memset(blob, 0, sizeof(*blob));
			return (ret);
		}
		if (value->data && value->size != 0) {
			memcpy(data_area + data_pos, value->data,
			    value->size);
		}
		data_pos += value->size;
		if (nodes[nidx].value_count == 0) {
			nodes[nidx].first_value = idx;
		}
		nodes[nidx].value_count++;
		idx++;
	}

	memcpy(hdr->magic, hivefs_hive_magic, 8);
	hdr->version = HIVEFS_FORMAT_VERSION;
	hdr->header_size = (u32)sizeof(hivefs_hive_header_t);
	hdr->flags = hive->flags;
	hdr->node_count = node_count;
	hdr->value_count = value_count;
	hdr->node_off = node_off;
	hdr->value_off = value_off;
	hdr->string_off = string_off;
	hdr->string_size = string_size;
	hdr->data_off = data_off;
	hdr->data_size = data_size;
	hdr->total_size = total_size;
	hdr->crc32 = hivefs_crc32(blob->data + node_off,
	    total_size - node_off);

	kmem_free(node_map);
	return (0);
}

static void
hivefs_free_hive_blobs(hivefs_hive_blob_t *blobs, u32 count)
{
	u32	i;

	if (!blobs) {
		return;
	}
	for (i = 0; i < count; i++) {
		if (blobs[i].data) {
			kmem_free(blobs[i].data);
		}
	}
}

static u32
hivefs_pack_align(u32 value)
{
	return ((value + 7U) & ~7U);
}

static int
hivefs_build_pack(u8 **out, u32 *out_size)
{
	hivefs_hive_blob_t	blobs[HIVEFS_MAX_HIVES];
	hivefs_pack_header_t	*hdr;
	hivefs_pack_entry_t	*entries;
	u8			*pack;
	u32			table_bytes, body_size, total_size;
	u32			data_off, body_pos, offset, i;
	u64			total64;
	int			ret;

	if (!out || !out_size || !g_hivefs.loaded ||
	    g_hivefs.hive_count == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	memset(blobs, 0, sizeof(blobs));
	body_size = 0;
	for (i = 0; i < g_hivefs.hive_count; i++) {
		ret = hivefs_build_hive_blob(&g_hivefs.hives[i],
		    &blobs[i]);
		if (ret != 0) {
			hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
			return (ret);
		}
		body_size = hivefs_pack_align(body_size);
		if (blobs[i].size > 0xFFFFFFFFU - body_size) {
			hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
			return (-API_ERR_TOO_BIG);
		}
		body_size += blobs[i].size;
	}

	if (!hivefs_mul_size_ok(g_hivefs.hive_count,
	    (u32)sizeof(hivefs_pack_entry_t), &table_bytes)) {
		hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
		return (-API_ERR_TOO_BIG);
	}
	data_off = (u32)sizeof(hivefs_pack_header_t) + table_bytes;
	total64 = (u64)data_off + body_size;
	if (total64 > 0xFFFFFFFFULL) {
		hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
		return (-API_ERR_TOO_BIG);
	}
	total_size = (u32)total64;

	pack = (u8 *)kmem_calloc(total_size, 1);
	if (!pack) {
		hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
		return (-API_ERR_NO_MEMORY);
	}

	hdr = (hivefs_pack_header_t *)pack;
	entries = (hivefs_pack_entry_t *)(pack +
	    sizeof(hivefs_pack_header_t));
	body_pos = 0;
	for (i = 0; i < g_hivefs.hive_count; i++) {
		body_pos = hivefs_pack_align(body_pos);
		offset = data_off + body_pos;
		hivefs_name_copy(g_hivefs.hives[i].name, entries[i].name);
		entries[i].offset = offset;
		entries[i].size = blobs[i].size;
		entries[i].flags = g_hivefs.hives[i].flags;
		entries[i].crc32 = hivefs_crc32(blobs[i].data,
		    blobs[i].size);
		memcpy(pack + offset, blobs[i].data, blobs[i].size);
		body_pos += blobs[i].size;
	}

	memcpy(hdr->magic, hivefs_pack_magic, 8);
	hdr->version = HIVEFS_FORMAT_VERSION;
	hdr->header_size = (u32)sizeof(hivefs_pack_header_t);
	hdr->hive_count = g_hivefs.hive_count;
	hdr->table_off = (u32)sizeof(hivefs_pack_header_t);
	hdr->data_off = data_off;
	hdr->total_size = total_size;
	hdr->reserved = 0;
	hdr->crc32 = hivefs_crc32(pack + hdr->table_off,
	    total_size - hdr->table_off);

	hivefs_free_hive_blobs(blobs, g_hivefs.hive_count);
	*out = pack;
	*out_size = total_size;
	return (0);
}

static int
hivefs_validate_hive(const u8 *base, u32 size, hivefs_hive_t *out)
{
	const hivefs_hive_header_t	*hdr;
	const hivefs_node_entry_t	*nodes;
	const hivefs_value_entry_t	*values;
	u32				node_bytes, value_bytes;
	u32				i;

	if (!base || !out || size < sizeof(hivefs_hive_header_t)) {
		return (-API_ERR_BAD_IMAGE);
	}

	hdr = (const hivefs_hive_header_t *)base;
	if (!hivefs_mem_eq(hdr->magic, hivefs_hive_magic, 8)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->version != HIVEFS_FORMAT_VERSION ||
	    hdr->header_size != sizeof(hivefs_hive_header_t) ||
	    hdr->total_size != size || hdr->node_count == 0) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!hivefs_mul_size_ok(hdr->node_count,
	    (u32)sizeof(hivefs_node_entry_t), &node_bytes)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!hivefs_mul_size_ok(hdr->value_count,
	    (u32)sizeof(hivefs_value_entry_t), &value_bytes)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->node_off != hdr->header_size ||
	    hdr->value_off != hdr->node_off + node_bytes ||
	    hdr->string_off != hdr->value_off + value_bytes ||
	    hdr->data_off != hdr->string_off + hdr->string_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!hivefs_range_ok(hdr->node_off, node_bytes, size) ||
	    !hivefs_range_ok(hdr->value_off, value_bytes, size) ||
	    !hivefs_range_ok(hdr->string_off, hdr->string_size, size) ||
	    !hivefs_range_ok(hdr->data_off, hdr->data_size, size)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->data_off + hdr->data_size != hdr->total_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hivefs_crc32(base + hdr->node_off,
	    size - hdr->node_off) != hdr->crc32) {
		return (-API_ERR_BAD_IMAGE);
	}

	out->base = base;
	out->size = size;
	out->flags = hdr->flags;
	out->raw_header = hdr;
	out->raw_nodes = (const hivefs_node_entry_t *)(base + hdr->node_off);
	out->raw_values = (const hivefs_value_entry_t *)(base + hdr->value_off);
	out->raw_strings = (const char *)(base + hdr->string_off);
	out->raw_data = base + hdr->data_off;

	nodes = out->raw_nodes;
	values = out->raw_values;
	for (i = 0; i < hdr->node_count; i++) {
		if (i == 0 && nodes[i].parent != HIVEFS_NO_INDEX) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (i != 0 && nodes[i].parent >= hdr->node_count) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (!hivefs_string_ok(out, nodes[i].name_off)) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (nodes[i].child_count != 0 &&
		    nodes[i].first_child >= hdr->node_count) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (nodes[i].value_count != 0 &&
		    nodes[i].first_value >= hdr->value_count) {
			return (-API_ERR_BAD_IMAGE);
		}
	}
	for (i = 0; i < hdr->value_count; i++) {
		if (values[i].node >= hdr->node_count ||
		    !hivefs_string_ok(out, values[i].name_off) ||
		    values[i].type < HIVEFS_TYPE_STRING ||
		    values[i].type > HIVEFS_TYPE_MULTI_STRING) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (!hivefs_range_ok(values[i].data_off,
		    values[i].size, hdr->data_size)) {
			return (-API_ERR_BAD_IMAGE);
		}
	}

	return (0);
}

static vnode_t *
hivefs_make_vnode(int kind, u32 hive, u32 node, u32 value,
    const char *name, u64 size)
{
	hivefs_vnode_data_t	*data;
	vnode_t			*vn;
	int			type;

	type = (kind == HIVEFS_KIND_VALUE) ? VREG : VDIR;
	vn = vnode_alloc(type, name);
	if (!vn) {
		return (NULL);
	}

	data = (hivefs_vnode_data_t *)kmem_calloc(1,
	    sizeof(hivefs_vnode_data_t));
	if (!data) {
		vnode_release(vn);
		return (NULL);
	}
	data->kind = kind;
	data->hive = hive;
	data->node = node;
	data->value = value;

	vn->data = data;
	vn->data_owned = 1;
	vn->size = size;
	vn->mode = (type == VDIR) ? (POSIX_S_IFDIR | 0755) :
	    (POSIX_S_IFREG | 0644);
	vn->read_fn = (type == VREG) ? hivefs_vnode_read : NULL;
	vn->write_fn = (type == VREG) ? hivefs_vnode_write : NULL;
	vn->stat_fn = hivefs_vnode_stat;
	vn->readdir_fn = (type == VDIR) ? hivefs_vnode_readdir : NULL;
	vn->listdir_fn = (type == VDIR) ? hivefs_vnode_listdir : NULL;
	return (vn);
}

static int
hivefs_vnode_read(vnode_t *vn, void *buf, u64 count, u64 offset)
{
	hivefs_vnode_data_t	*data;
	hivefs_hive_t		*hive;
	hivefs_value_t		*value;
	u32			available, to_read;

	if (!vn || !buf || !vn->data) {
		return (-API_ERR_BAD_VALUE);
	}
	data = (hivefs_vnode_data_t *)vn->data;
	if (data->kind != HIVEFS_KIND_VALUE ||
	    data->hive >= g_hivefs.hive_count) {
		return (-API_ERR_IS_DIR);
	}

	hive = &g_hivefs.hives[data->hive];
	if (data->value >= hive->value_count) {
		return (-API_ERR_BAD_VALUE);
	}
	value = &hive->values[data->value];
	if (value->deleted) {
		return (-API_ERR_NOT_FOUND);
	}
	if (offset >= value->size) {
		return (0);
	}

	available = value->size - (u32)offset;
	to_read = (count > 0x7FFFFFFFULL) ? 0x7FFFFFFFU : (u32)count;
	if (to_read > available) {
		to_read = available;
	}
	if (to_read != 0) {
		memcpy(buf, value->data + (u32)offset, to_read);
	}
	return ((int)to_read);
}

static int
hivefs_vnode_write(vnode_t *vn, const void *buf, u64 count, u64 offset)
{
	hivefs_vnode_data_t	*data;
	hivefs_hive_t		*hive;
	hivefs_value_t		old_value;
	u8			*new_data;
	u32			old_size, new_size, write_off;
	int			ret;

	if (!vn || !buf || !vn->data || offset > 0xFFFFFFFFULL ||
	    count > 0x7FFFFFFFULL) {
		return (-API_ERR_BAD_VALUE);
	}
	data = (hivefs_vnode_data_t *)vn->data;
	if (data->kind != HIVEFS_KIND_VALUE ||
	    data->hive >= g_hivefs.hive_count) {
		return (-API_ERR_IS_DIR);
	}
	hive = &g_hivefs.hives[data->hive];
	if (data->value >= hive->value_count ||
	    hive->values[data->value].deleted) {
		return (-API_ERR_NOT_FOUND);
	}

	write_off = (u32)offset;
	if ((u32)count > 0xFFFFFFFFU - write_off) {
		return (-API_ERR_FILE_TOO_BIG);
	}
	old_value = hive->values[data->value];
	old_size = old_value.size;
	new_size = write_off + (u32)count;
	if (new_size < old_size) {
		new_size = old_size;
	}

	new_data = NULL;
	if (new_size != 0) {
		new_data = (u8 *)kmem_calloc(new_size, 1);
		if (!new_data) {
			return (-API_ERR_NO_MEMORY);
		}
		if (old_value.data && old_size != 0) {
			memcpy(new_data, old_value.data, old_size);
		}
		memcpy(new_data + write_off, buf, (u32)count);
	}

	hive->values[data->value].data = new_data;
	hive->values[data->value].size = new_size;
	hive->values[data->value].type = HIVEFS_TYPE_BYTES;
	hive->values[data->value].deleted = 0;
	hive->dirty = 1;
	ret = hivefs_commit();
	if (ret != 0) {
		if (new_data) {
			kmem_free(new_data);
		}
		hive->values[data->value] = old_value;
		return (ret);
	}

	if (old_value.data) {
		kmem_free(old_value.data);
	}
	vn->size = new_size;
	return ((int)count);
}

static int
hivefs_vnode_stat(vnode_t *vn, posix_stat_t *st)
{
	hivefs_vnode_data_t	*data;
	hivefs_hive_t		*hive;
	u64			size;
	u64			ino;

	if (!vn || !st || !vn->data) {
		return (-API_ERR_BAD_VALUE);
	}

	data = (hivefs_vnode_data_t *)vn->data;
	size = vn->size;
	if (data->kind == HIVEFS_KIND_VALUE &&
	    data->hive < g_hivefs.hive_count) {
		hive = &g_hivefs.hives[data->hive];
		if (data->value >= hive->value_count ||
		    hive->values[data->value].deleted) {
			return (-API_ERR_NOT_FOUND);
		}
		size = hive->values[data->value].size;
		vn->size = size;
	}
	ino = ((u64)data->kind << 56) | ((u64)data->hive << 40) |
	    ((u64)data->node << 16) | data->value;

	memset(st, 0, sizeof(posix_stat_t));
	st->st_mode = vn->mode;
	st->st_size = (s64)size;
	st->st_blksize = 512;
	st->st_blocks = (s64)((size + 511) / 512);
	st->st_nlink = 1;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_ino = ino;
	return (0);
}

static int
hivefs_vnode_readdir(vnode_t *vn, u32 index, char *name, int *type)
{
	vfs_dirent_t	entry;
	u32		count;
	int		ret;

	count = 0;
	ret = hivefs_vnode_listdir(vn, index, &entry, 1, &count);
	if (ret != 0) {
		return (ret);
	}
	if (count == 0) {
		return (0);
	}
	hivefs_name_copy(entry.name, name);
	if (type) {
		*type = entry.type;
	}
	return (1);
}

static int
hivefs_vnode_listdir(vnode_t *vn, u32 start, vfs_dirent_t *entries,
    u32 max_entries, u32 *count)
{
	hivefs_vnode_data_t	*data;
	hivefs_hive_t		*hive;
	u32			node, seen, copied, value_start, i;

	if (!vn || !entries || !count || !vn->data) {
		return (-API_ERR_BAD_VALUE);
	}

	*count = 0;
	if (max_entries == 0) {
		return (0);
	}

	data = (hivefs_vnode_data_t *)vn->data;
	if (data->kind == HIVEFS_KIND_ROOT) {
		copied = 0;
		for (i = start; i < g_hivefs.hive_count &&
		    copied < max_entries; i++) {
			memset(&entries[copied], 0,
			    sizeof(entries[copied]));
			hivefs_name_copy(g_hivefs.hives[i].name,
			    entries[copied].name);
			entries[copied].type = VDIR;
			copied++;
		}
		*count = copied;
		return (0);
	}
	if (data->hive >= g_hivefs.hive_count ||
	    (data->kind != HIVEFS_KIND_HIVE &&
	    data->kind != HIVEFS_KIND_NODE)) {
		return (-API_ERR_BAD_VALUE);
	}

	hive = &g_hivefs.hives[data->hive];
	node = data->node;
	if (node >= hive->node_count || hive->nodes[node].deleted) {
		return (-API_ERR_BAD_VALUE);
	}

	seen = 0;
	copied = 0;
	for (i = 0; i < hive->node_count; i++) {
		if (hive->nodes[i].deleted ||
		    hive->nodes[i].parent != node) {
			continue;
		}
		if (seen >= start && copied < max_entries) {
			memset(&entries[copied], 0, sizeof(entries[copied]));
			hivefs_name_copy(hive->nodes[i].name,
			    entries[copied].name);
			entries[copied].type = VDIR;
			copied++;
		}
		seen++;
	}

	if (copied >= max_entries) {
		*count = copied;
		return (0);
	}

	value_start = (start > seen) ? start - seen : 0;
	seen = 0;
	for (i = 0; i < hive->value_count; i++) {
		if (hive->values[i].deleted ||
		    hive->values[i].node != node) {
			continue;
		}
		if (seen >= value_start && copied < max_entries) {
			memset(&entries[copied], 0, sizeof(entries[copied]));
			hivefs_name_copy(hive->values[i].name,
			    entries[copied].name);
			entries[copied].type = VREG;
			copied++;
		}
		seen++;
	}

	*count = copied;
	return (0);
}

static int
hivefs_back_init(void)
{
	return (0);
}

static vnode_t *
hivefs_back_lookup(const char *path)
{
	hivefs_hive_t	*hive;
	hivefs_value_t	*value;
	const char	*inner, *part, *next;
	const char	*name;
	u32		part_len, hive_idx, node_idx;
	int		idx;

	if (!g_hivefs.loaded) {
		return (NULL);
	}

	inner = hivefs_strip_prefix(path);
	part = hivefs_next_component(inner, &next, &part_len);
	if (part_len == 0) {
		return (hivefs_make_vnode(HIVEFS_KIND_ROOT, 0, 0, 0,
		    "hivefs", 0));
	}

	idx = hivefs_find_hive(part, part_len);
	if (idx < 0) {
		return (NULL);
	}
	hive_idx = (u32)idx;
	hive = &g_hivefs.hives[hive_idx];
	if (next[0] == '\0') {
		return (hivefs_make_vnode(HIVEFS_KIND_HIVE, hive_idx,
		    0, 0, hive->name, 0));
	}

	node_idx = 0;
	while (next[0] != '\0') {
		part = hivefs_next_component(next, &next, &part_len);
		if (part_len == 0) {
			break;
		}

		idx = hivefs_find_child(hive, node_idx, part, part_len);
		if (idx >= 0) {
			node_idx = (u32)idx;
			if (next[0] == '\0') {
				name = hive->nodes[node_idx].name;
				return (hivefs_make_vnode(HIVEFS_KIND_NODE,
				    hive_idx, node_idx, 0, name, 0));
			}
			continue;
		}

		idx = hivefs_find_value(hive, node_idx, part, part_len);
		if (idx < 0 || next[0] != '\0') {
			return (NULL);
		}
		value = &hive->values[idx];
		name = value->name;
		return (hivefs_make_vnode(HIVEFS_KIND_VALUE, hive_idx,
		    node_idx, (u32)idx, name, value->size));
	}

	return (hivefs_make_vnode(HIVEFS_KIND_NODE, hive_idx, node_idx,
	    0, hive->nodes[node_idx].name, 0));
}

int
hivefs_create_key(const char *hive_name, const char *key)
{
	hivefs_hive_t	*hive;
	u32		old_node_count, node_idx;
	int		idx, old_dirty, ret;

	if (!g_hivefs.loaded || !key || key[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	hive = &g_hivefs.hives[idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret == 0) {
		return (-API_ERR_EXISTS);
	}
	if (ret != -API_ERR_NOT_FOUND) {
		return (ret);
	}

	old_dirty = hive->dirty;
	old_node_count = hive->node_count;
	ret = hivefs_find_node_path(hive, key, 1, &node_idx);
	if (ret != 0) {
		hive->node_count = old_node_count;
		hive->dirty = old_dirty;
		return (ret);
	}
	ret = hivefs_commit();
	if (ret != 0) {
		hive->node_count = old_node_count;
		hive->dirty = old_dirty;
		return (ret);
	}
	return (0);
}

int
hivefs_delete_key(const char *hive_name, const char *key)
{
	hivefs_hive_t	*hive;
	u32		node_idx, i;
	int		idx, old_dirty, ret;

	if (!g_hivefs.loaded || !key || key[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	hive = &g_hivefs.hives[idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret != 0) {
		return (ret);
	}
	if (node_idx == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < hive->node_count; i++) {
		if (!hive->nodes[i].deleted &&
		    hive->nodes[i].parent == node_idx) {
			return (-API_ERR_NOT_EMPTY);
		}
	}
	for (i = 0; i < hive->value_count; i++) {
		if (!hive->values[i].deleted &&
		    hive->values[i].node == node_idx) {
			return (-API_ERR_NOT_EMPTY);
		}
	}

	old_dirty = hive->dirty;
	hive->nodes[node_idx].deleted = 1;
	hive->dirty = 1;
	ret = hivefs_commit();
	if (ret != 0) {
		hive->nodes[node_idx].deleted = 0;
		hive->dirty = old_dirty;
		return (ret);
	}
	return (0);
}

int
hivefs_set_value(const char *hive_name, const char *key,
    const char *value_name, u32 type, const void *data, u32 size)
{
	hivefs_hive_t	*hive;
	hivefs_value_t	old_value;
	char		value_tmp[HIVEFS_NAME_SIZE];
	u8		*buf;
	u32		old_node_count, old_value_count;
	u32		node_idx, value_idx, name_len;
	int		idx, value_found, old_dirty, ret;

	if (!g_hivefs.loaded || !value_name || value_name[0] == '\0' ||
	    (!data && size != 0) || type < HIVEFS_TYPE_STRING ||
	    type > HIVEFS_TYPE_MULTI_STRING) {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	name_len = (u32)strlen(value_name);
	if (hivefs_name_from_part(value_name, name_len, value_tmp) != 0) {
		return (-API_ERR_BAD_VALUE);
	}

	hive = &g_hivefs.hives[idx];
	old_dirty = hive->dirty;
	old_node_count = hive->node_count;
	old_value_count = hive->value_count;
	ret = hivefs_find_node_path(hive, key, 1, &node_idx);
	if (ret != 0) {
		hive->node_count = old_node_count;
		hive->dirty = old_dirty;
		return (ret);
	}
	if (hivefs_find_child(hive, node_idx, value_name, name_len) >= 0) {
		hive->node_count = old_node_count;
		hive->dirty = old_dirty;
		return (-API_ERR_EXISTS);
	}

	buf = NULL;
	if (size != 0) {
		buf = (u8 *)kmem_alloc(size);
		if (!buf) {
			hive->node_count = old_node_count;
			hive->dirty = old_dirty;
			return (-API_ERR_NO_MEMORY);
		}
		memcpy(buf, data, size);
	}

	value_found = hivefs_find_value(hive, node_idx, value_name,
	    name_len);
	if (value_found >= 0) {
		value_idx = (u32)value_found;
		old_value = hive->values[value_idx];
		hive->values[value_idx].data = buf;
		hive->values[value_idx].type = type;
		hive->values[value_idx].size = size;
		hive->values[value_idx].deleted = 0;
		hive->dirty = 1;
		ret = hivefs_commit();
		if (ret != 0) {
			if (buf) {
				kmem_free(buf);
			}
			hive->values[value_idx] = old_value;
			hive->node_count = old_node_count;
			hive->dirty = old_dirty;
			return (ret);
		}
		if (old_value.data) {
			kmem_free(old_value.data);
		}
		return (0);
	}

	ret = hivefs_reserve_values(hive, hive->value_count + 1);
	if (ret != 0) {
		if (buf) {
			kmem_free(buf);
		}
		hive->node_count = old_node_count;
		hive->dirty = old_dirty;
		return (ret);
	}
	value_idx = hive->value_count++;
	memset(&hive->values[value_idx], 0, sizeof(hive->values[value_idx]));
	hivefs_name_copy(value_name, hive->values[value_idx].name);
	hive->values[value_idx].data = buf;
	hive->values[value_idx].node = node_idx;
	hive->values[value_idx].type = type;
	hive->values[value_idx].flags = 0;
	hive->values[value_idx].size = size;
	hive->values[value_idx].deleted = 0;
	hive->dirty = 1;
	ret = hivefs_commit();
	if (ret != 0) {
		if (buf) {
			kmem_free(buf);
		}
		hive->node_count = old_node_count;
		hive->value_count = old_value_count;
		hive->dirty = old_dirty;
		return (ret);
	}
	return (0);
}

int
hivefs_delete_value(const char *hive_name, const char *key,
    const char *value_name)
{
	hivefs_hive_t	*hive;
	u32		node_idx, value_idx, name_len;
	int		idx, old_dirty, ret;

	if (!g_hivefs.loaded || !value_name || value_name[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	name_len = (u32)strlen(value_name);
	hive = &g_hivefs.hives[idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret != 0) {
		return (ret);
	}
	idx = hivefs_find_value(hive, node_idx, value_name, name_len);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}

	value_idx = (u32)idx;
	old_dirty = hive->dirty;
	hive->values[value_idx].deleted = 1;
	hive->dirty = 1;
	ret = hivefs_commit();
	if (ret != 0) {
		hive->values[value_idx].deleted = 0;
		hive->dirty = old_dirty;
		return (ret);
	}
	if (hive->values[value_idx].data) {
		kmem_free(hive->values[value_idx].data);
		hive->values[value_idx].data = NULL;
	}
	hive->values[value_idx].size = 0;
	return (0);
}

int
hivefs_value_info(const char *hive_name, const char *key,
    const char *value_name, u32 *type, u32 *size)
{
	hivefs_hive_t	*hive;
	hivefs_value_t	*value;
	u32		node_idx, name_len;
	int		idx, ret;

	if (!g_hivefs.loaded || !value_name || value_name[0] == '\0' ||
	    (!type && !size)) {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	name_len = (u32)strlen(value_name);
	hive = &g_hivefs.hives[idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret != 0) {
		return (ret);
	}
	idx = hivefs_find_value(hive, node_idx, value_name, name_len);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}

	value = &hive->values[idx];
	if (value->deleted) {
		return (-API_ERR_NOT_FOUND);
	}
	if (type) {
		*type = value->type;
	}
	if (size) {
		*size = value->size;
	}
	return (0);
}

int
hivefs_access_info(const char *hive_name, const char *key,
    const char *value_name, u32 *flags)
{
	hivefs_hive_t	*hive;
	hivefs_value_t	*value;
	u32		node_idx, name_len;
	u32		read, add, edit;
	int		idx, ret;

	if (!g_hivefs.loaded || !flags) {
		return (-API_ERR_BAD_VALUE);
	}
	idx = hivefs_find_hive_name(hive_name);
	if (idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}

	hive = &g_hivefs.hives[idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret != 0) {
		return (ret);
	}

	read = HIVEFS_ACCESS_INHERIT;
	add = HIVEFS_ACCESS_INHERIT;
	edit = HIVEFS_ACCESS_INHERIT;

	if (value_name && value_name[0] != '\0') {
		name_len = (u32)strlen(value_name);
		idx = hivefs_find_value(hive, node_idx, value_name,
		    name_len);
		if (idx < 0) {
			return (-API_ERR_NOT_FOUND);
		}
		value = &hive->values[idx];
		if (value->deleted) {
			return (-API_ERR_NOT_FOUND);
		}
		hivefs_access_merge(value->flags, &read, &add, &edit);
	}

	while (1) {
		if (node_idx >= hive->node_count ||
		    hive->nodes[node_idx].deleted) {
			return (-API_ERR_NOT_FOUND);
		}
		hivefs_access_merge(hive->nodes[node_idx].flags,
		    &read, &add, &edit);
		if (node_idx == 0) {
			break;
		}
		node_idx = hive->nodes[node_idx].parent;
	}

	hivefs_access_merge(hive->flags, &read, &add, &edit);
	hivefs_access_merge(HIVEFS_ACCESS_DEFAULT, &read, &add, &edit);
	*flags = hivefs_access_pack(read, add, edit);
	return (0);
}

static int
hivefs_back_create_file(const char *path)
{
	char	hive_name[HIVEFS_NAME_SIZE];
	char	key[HIVEFS_PATH_SIZE];
	char	value_name[HIVEFS_NAME_SIZE];
	u32	node_idx, value_len;
	int	hive_idx, ret;

	ret = hivefs_path_split_value(path, hive_name, key, value_name);
	if (ret != 0) {
		return (ret);
	}
	hive_idx = hivefs_find_hive_name(hive_name);
	if (hive_idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	ret = hivefs_find_node_path(&g_hivefs.hives[hive_idx], key, 0,
	    &node_idx);
	if (ret != 0) {
		return (ret);
	}
	value_len = (u32)strlen(value_name);
	if (hivefs_find_child(&g_hivefs.hives[hive_idx], node_idx,
	    value_name, value_len) >= 0 ||
	    hivefs_find_value(&g_hivefs.hives[hive_idx], node_idx,
	    value_name, value_len) >= 0) {
		return (-API_ERR_EXISTS);
	}
	return (hivefs_set_value(hive_name, key, value_name,
	    HIVEFS_TYPE_BYTES, NULL, 0));
}

static int
hivefs_back_mkdir(const char *path)
{
	char	hive_name[HIVEFS_NAME_SIZE];
	char	key[HIVEFS_PATH_SIZE];
	int	ret;

	ret = hivefs_path_split_key(path, hive_name, key);
	if (ret != 0) {
		return (ret);
	}
	return (hivefs_create_key(hive_name, key));
}

static int
hivefs_back_rmdir(const char *path)
{
	char	hive_name[HIVEFS_NAME_SIZE];
	char	key[HIVEFS_PATH_SIZE];
	int	ret;

	ret = hivefs_path_split_key(path, hive_name, key);
	if (ret != 0) {
		return (ret);
	}
	return (hivefs_delete_key(hive_name, key));
}

static int
hivefs_back_unlink(const char *path)
{
	char	hive_name[HIVEFS_NAME_SIZE];
	char	key[HIVEFS_PATH_SIZE];
	char	value_name[HIVEFS_NAME_SIZE];
	int	ret;

	ret = hivefs_path_split_value(path, hive_name, key, value_name);
	if (ret != 0) {
		return (ret);
	}
	return (hivefs_delete_value(hive_name, key, value_name));
}

static int
hivefs_back_truncate(const char *path, u64 length)
{
	hivefs_hive_t	*hive;
	hivefs_value_t	*value;
	char		hive_name[HIVEFS_NAME_SIZE];
	char		key[HIVEFS_PATH_SIZE];
	char		value_name[HIVEFS_NAME_SIZE];
	u8		*buf;
	u32		node_idx, value_idx, copy_size;
	int		hive_idx, ret;

	if (length > 0xFFFFFFFFULL) {
		return (-API_ERR_FILE_TOO_BIG);
	}
	ret = hivefs_path_split_value(path, hive_name, key, value_name);
	if (ret != 0) {
		return (ret);
	}
	hive_idx = hivefs_find_hive_name(hive_name);
	if (hive_idx < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	hive = &g_hivefs.hives[hive_idx];
	ret = hivefs_find_node_path(hive, key, 0, &node_idx);
	if (ret != 0) {
		return (ret);
	}
	ret = hivefs_find_value(hive, node_idx, value_name,
	    (u32)strlen(value_name));
	if (ret < 0) {
		return (-API_ERR_NOT_FOUND);
	}
	value_idx = (u32)ret;
	value = &hive->values[value_idx];

	buf = NULL;
	if (length != 0) {
		buf = (u8 *)kmem_calloc((u32)length, 1);
		if (!buf) {
			return (-API_ERR_NO_MEMORY);
		}
		copy_size = value->size < (u32)length ?
		    value->size : (u32)length;
		if (copy_size != 0 && value->data) {
			memcpy(buf, value->data, copy_size);
		}
	}
	ret = hivefs_set_value(hive_name, key, value_name,
	    value->type, buf, (u32)length);
	if (buf) {
		kmem_free(buf);
	}
	return (ret);
}

static int
hivefs_back_write_file(const char *path, const u8 *data, u32 size)
{
	char	hive_name[HIVEFS_NAME_SIZE];
	char	key[HIVEFS_PATH_SIZE];
	char	value_name[HIVEFS_NAME_SIZE];
	int	ret;

	ret = hivefs_path_split_value(path, hive_name, key, value_name);
	if (ret != 0) {
		return (ret);
	}
	return (hivefs_set_value(hive_name, key, value_name,
	    HIVEFS_TYPE_BYTES, data, size));
}

static int
hivefs_pack_load_into(const void *data, u32 size, hivefs_state_t *state)
{
	const hivefs_pack_header_t	*hdr;
	const hivefs_pack_entry_t	*entries;
	hivefs_hive_t			*hive;
	u32				table_bytes, i, j;
	int				ret;

	if (!data || !state || size < sizeof(hivefs_pack_header_t)) {
		return (-API_ERR_BAD_IMAGE);
	}
	memset(state, 0, sizeof(*state));

	hdr = (const hivefs_pack_header_t *)data;
	if (!hivefs_mem_eq(hdr->magic, hivefs_pack_magic, 8)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->version != HIVEFS_FORMAT_VERSION ||
	    hdr->header_size != sizeof(hivefs_pack_header_t) ||
	    hdr->hive_count == 0 || hdr->hive_count > HIVEFS_MAX_HIVES ||
	    hdr->total_size > size || hdr->table_off != hdr->header_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!hivefs_mul_size_ok(hdr->hive_count,
	    (u32)sizeof(hivefs_pack_entry_t), &table_bytes)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->data_off != hdr->table_off + table_bytes ||
	    !hivefs_range_ok(hdr->table_off, table_bytes,
	    hdr->total_size)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->data_off > hdr->total_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hivefs_crc32((const u8 *)data + hdr->table_off,
	    hdr->total_size - hdr->table_off) != hdr->crc32) {
		return (-API_ERR_BAD_IMAGE);
	}

	entries = (const hivefs_pack_entry_t *)
	    ((const u8 *)data + hdr->table_off);
	for (i = 0; i < hdr->hive_count; i++) {
		if (!hivefs_fixed_name_ok(entries[i].name) ||
		    entries[i].offset < hdr->data_off ||
		    !hivefs_range_ok(entries[i].offset, entries[i].size,
		    hdr->total_size)) {
			hivefs_free_state(state);
			return (-API_ERR_BAD_IMAGE);
		}
		if (hivefs_crc32((const u8 *)data + entries[i].offset,
		    entries[i].size) != entries[i].crc32) {
			hivefs_free_state(state);
			return (-API_ERR_BAD_IMAGE);
		}

		state->hive_count = i;
		for (j = 0; j < i; j++) {
			if (strcmp(entries[i].name, entries[j].name) == 0) {
				hivefs_free_state(state);
				return (-API_ERR_BAD_IMAGE);
			}
		}

		hive = &state->hives[i];
		ret = hivefs_validate_hive((const u8 *)data +
		    entries[i].offset, entries[i].size, hive);
		if (ret != 0) {
			hivefs_free_state(state);
			return (ret);
		}
		ret = hivefs_import_hive(hive);
		if (ret != 0) {
			hivefs_free_state(state);
			return (ret);
		}
		hivefs_name_copy(entries[i].name, hive->name);
		hive->flags = entries[i].flags;
		state->hive_count = i + 1;
	}

	state->pack = (const u8 *)data;
	state->header = hdr;
	state->entries = entries;
	state->size = hdr->total_size;
	state->hive_count = hdr->hive_count;
	state->loaded = 1;
	return (0);
}

static int
hivefs_pack_load(const void *data, u32 size, int owned)
{
	hivefs_state_t	next;
	char		store_path[HIVEFS_PATH_SIZE];
	int		syncing;
	int		ret;

	memset(store_path, 0, sizeof(store_path));
	memcpy(store_path, g_hivefs.store_path, sizeof(store_path));
	syncing = g_hivefs.syncing;

	ret = hivefs_pack_load_into(data, size, &next);
	if (ret != 0) {
		if (owned && data) {
			kmem_free((void *)data);
		}
		return (ret);
	}
	next.pack_owned = owned ? 1 : 0;
	memcpy(next.store_path, store_path, sizeof(next.store_path));
	next.syncing = syncing;

	hivefs_free_state(&g_hivefs);
	g_hivefs = next;

	drivers_log("[HIVEFS] loaded cmseed: %u hives, %u bytes\n",
	    g_hivefs.hive_count, g_hivefs.size);
	return (0);
}

static int
hivefs_commit(void)
{
	hivefs_state_t	verify;
	const u8	*old_pack;
	u8		*pack;
	u32		size;
	u32		i;
	int		old_owned;
	int		ret;

	if (!g_hivefs.loaded) {
		return (-API_ERR_NOT_FOUND);
	}
	if (g_hivefs.syncing) {
		return (0);
	}
	if (g_hivefs.store_path[0] == '\0' || !vfs_is_initialized()) {
		return (-API_ERR_IO);
	}

	pack = NULL;
	size = 0;
	ret = hivefs_build_pack(&pack, &size);
	if (ret != 0) {
		return (ret);
	}
	ret = hivefs_pack_load_into(pack, size, &verify);
	if (ret != 0) {
		kmem_free(pack);
		return (ret);
	}
	hivefs_free_state(&verify);

	g_hivefs.syncing = 1;
	ret = vfs_write_file(g_hivefs.store_path, pack, size);
	g_hivefs.syncing = 0;
	if (ret != 0) {
		kmem_free(pack);
		return (ret);
	}

	old_pack = g_hivefs.pack;
	old_owned = g_hivefs.pack_owned;
	g_hivefs.pack = pack;
	g_hivefs.header = (const hivefs_pack_header_t *)pack;
	g_hivefs.entries = (const hivefs_pack_entry_t *)(pack +
	    sizeof(hivefs_pack_header_t));
	g_hivefs.size = size;
	g_hivefs.pack_owned = 1;
	for (i = 0; i < g_hivefs.hive_count; i++) {
		hivefs_validate_hive(pack + g_hivefs.entries[i].offset,
		    g_hivefs.entries[i].size, &g_hivefs.hives[i]);
		g_hivefs.hives[i].dirty = 0;
	}
	if (old_owned && old_pack) {
		kmem_free((void *)old_pack);
	}
	return (0);
}

void
hivefs_reset(void)
{
	hivefs_free_state(&g_hivefs);
}

int
hivefs_load_pack(const void *data, u32 size)
{
	return (hivefs_pack_load(data, size, 0));
}

int
hivefs_is_loaded(void)
{
	return (g_hivefs.loaded);
}

int
hivefs_set_store_path(const char *path)
{
	u32	len;

	if (!path || path[0] != '/') {
		return (-API_ERR_BAD_VALUE);
	}
	len = (u32)strlen(path);
	if (len == 0 || len >= HIVEFS_PATH_SIZE) {
		return (-API_ERR_TOO_BIG);
	}
	memset(g_hivefs.store_path, 0, sizeof(g_hivefs.store_path));
	memcpy(g_hivefs.store_path, path, len);
	g_hivefs.store_path[len] = '\0';
	return (0);
}

int
hivefs_load_store(const char *path)
{
	vnode_t		*vn;
	posix_stat_t	st;
	u8		*buf;
	u32		size;
	int		n, ret;

	if (!path) {
		path = g_hivefs.store_path;
	}
	if (!path || path[0] == '\0' || !vfs_is_initialized()) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = vfs_resolve(path, &vn);
	if (ret != 0) {
		return (ret);
	}
	if (!vn) {
		return (-API_ERR_NOT_FOUND);
	}
	ret = vnode_stat(vn, &st);
	if (ret != 0) {
		vnode_release(vn);
		return (ret);
	}
	if (st.st_size <= 0 || st.st_size > 0xFFFFFFFFLL) {
		vnode_release(vn);
		return (-API_ERR_BAD_IMAGE);
	}

	size = (u32)st.st_size;
	buf = (u8 *)kmem_alloc(size);
	if (!buf) {
		vnode_release(vn);
		return (-API_ERR_NO_MEMORY);
	}
	n = vnode_read(vn, buf, size, 0);
	vnode_release(vn);
	if (n < 0) {
		kmem_free(buf);
		return (n);
	}
	if ((u32)n != size) {
		kmem_free(buf);
		return (-API_ERR_IO);
	}

	ret = hivefs_pack_load(buf, size, 1);
	if (ret != 0) {
		return (ret);
	}
	hivefs_set_store_path(path);
	drivers_log("[HIVEFS] loaded persistent store %s\n", path);
	return (0);
}

int
hivefs_sync(void)
{
	return (hivefs_commit());
}

const vfs_back_ops_t *
hivefs_back_ops(void)
{
	return (&hivefs_ops);
}
