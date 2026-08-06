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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type process as struct with process control block
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type api_entity_create_args as native entity create descriptor
$define %type api_entity_stat as native entity stat descriptor
$define %type api_entity_entry as native entity list entry
$define %type api_entity_data as native entity data access descriptor
$define %type api_entity_list as native entity namespace list descriptor
$define %type api_entity_query as native entity query descriptor

$define %func entity_copy_user_string as function with args const char *, char *, u32
$define %func entity_access_valid as function with args u32
$define %func entity_fill_stat as procedure with args entity id, stat
$define %func entity_fill_entry as procedure with args entity id, entry
$define %func entity_query_cb as function with args entity id, void *
$define %func api_entity_create as function with args create args
$define %func api_entity_open as function with args const char *, u32
$define %func api_entity_close as function with args int
$define %func api_entity_dup as function with args int, u32
$define %func api_entity_stat as function with args int, stat *
$define %func api_entity_list as function with args list *
$define %func api_entity_query as function with args query *
$define %func api_entity_ctl as function with args int, u32, void *

*/

/* !SPACE!

$space %internal entity_copy_user_string, entity_access_valid
$space %internal entity_fill_stat, entity_fill_entry, entity_query_cb
$space %export api_entity_create, api_entity_open, api_entity_close
$space %export api_entity_dup, api_entity_stat, api_entity_list
$space %export api_entity_query, api_entity_ctl

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

typedef struct entity_query_ctx {
	struct api_entity_entry	*entries;
	u32			max_entries;
	u32			*count;
} entity_query_ctx_t;

static int
entity_copy_user_string(const char *user, char *buf, u32 bufsize)
{
	u32	len;

	if (!user || !buf || bufsize == 0) {
		return (-API_ERR_BAD_ADDR);
	}
	if (!is_user_address(user, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	len = 0;
	while (len < bufsize - 1) {
		if (!is_user_address(user + len, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		if (user[len] == '\0') {
			break;
		}
		len++;
	}
	if (len == bufsize - 1) {
		if (!is_user_address(user + len, 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		if (user[len] != '\0') {
			return (-API_ERR_TOO_BIG);
		}
	}
	memcpy(buf, user, len);
	buf[len] = '\0';
	return (0);
}

static int
entity_access_valid(u32 access)
{
	if (access == 0) {
		return (0);
	}
	return ((access & ~ENTITY_ACCESS_ALL) == 0);
}

static void
entity_fill_stat(entity_id_t id, struct api_entity_stat *st)
{
	memset(st, 0, sizeof(*st));
	st->id = id;
	st->archetype = entity_arch(id);
	st->state = entity_state(id);
	st->flags = entity_flags(id);
	st->refs = entity_refs(id);
	st->owner_pid = entity_owner(id);
	st->uid = entity_uid(id);
	st->gid = entity_gid(id);
	st->euid = entity_euid(id);
	st->egid = entity_egid(id);
	st->size = entity_size(id);
	st->created = entity_created(id);
	entity_name(id, st->name, sizeof(st->name));
}

static void
entity_fill_entry(entity_id_t id, struct api_entity_entry *entry)
{
	memset(entry, 0, sizeof(*entry));
	entry->id = id;
	entry->archetype = entity_arch(id);
	entry->state = entity_state(id);
	entry->owner_pid = entity_owner(id);
	entity_name(id, entry->name, sizeof(entry->name));
}

static int
entity_query_cb(entity_id_t id, void *ctx)
{
	entity_query_ctx_t	*qc;

	qc = (entity_query_ctx_t *)ctx;
	if (*qc->count >= qc->max_entries) {
		return (1);
	}
	entity_fill_entry(id, &qc->entries[*qc->count]);
	(*qc->count)++;
	return (0);
}

int
api_entity_create(const struct api_entity_create_args *uargs)
{
	struct api_entity_create_args	args;
	process_t			*proc;
	entity_id_t			id;
	char				name[ENTITY_PATH_MAX];
	u32				uid, gid, euid, egid;
	int				handle, kusr, ret;

	if (!uargs || !is_user_address(uargs, sizeof(args))) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&args, uargs, sizeof(args));
	if (args.archetype == 0 || args.archetype > ENTITY_ARCH_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!entity_access_valid(args.access)) {
		return (-API_ERR_BAD_VALUE);
	}
	name[0] = '\0';
	if (args.name) {
		ret = entity_copy_user_string(args.name, name,
		    sizeof(name));
		if (ret != 0) {
			return (ret);
		}
		if (name[0] != '\0' && !entity_ns_is_path(name)) {
			return (-API_ERR_BAD_VALUE);
		}
	}
	proc = process_current();
	uid = proc ? proc->uid : 0;
	gid = proc ? proc->gid : 0;
	euid = proc ? proc->euid : 0;
	egid = proc ? proc->egid : 0;
	kusr = proc ? proc->kusr_auth : 0;
	id = entity_create(args.archetype, args.flags,
	    proc ? proc->pid : 0, uid, gid, euid, egid, kusr);
	if (id == 0) {
		return (-API_ERR_NO_MEMORY);
	}
	if (name[0] != '\0') {
		ret = entity_ns_bind(name, id);
		if (ret != 0) {
			entity_destroy(id);
			return (ret);
		}
	}
	handle = entity_handle_alloc(proc, id, args.access);
	if (handle < 0) {
		entity_destroy(id);
		return (handle);
	}
	entity_release(id);
	return (handle);
}

int
api_entity_open(const char *uname, u32 access)
{
	process_t	*proc;
	entity_id_t	id;
	char		name[ENTITY_PATH_MAX];
	int		ret;

	if (!entity_access_valid(access)) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = entity_copy_user_string(uname, name, sizeof(name));
	if (ret != 0) {
		return (ret);
	}
	if (!entity_ns_is_path(name)) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = entity_ns_lookup(name, &id);
	if (ret != 0) {
		return (ret);
	}
	proc = process_current();
	ret = entity_access(proc, id, access);
	if (ret != 0) {
		return (ret);
	}
	return (entity_handle_alloc(proc, id, access));
}

int
api_entity_close(int handle)
{
	return (entity_handle_free(process_current(), handle));
}

int
api_entity_dup(int handle, u32 access)
{
	return (entity_handle_dup(process_current(), handle, access));
}

int
api_entity_stat(int handle, struct api_entity_stat *ustat)
{
	struct api_entity_stat	out;
	process_t		*proc;
	entity_id_t		id;
	u32			access;
	int			ret;

	if (!ustat || !is_user_address(ustat, sizeof(out)) ||
	    !user_range_fault_in(ustat, sizeof(out), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	proc = process_current();
	ret = entity_handle_lookup(proc, handle, &id, &access);
	if (ret != 0) {
		return (ret);
	}
	if ((access & ENTITY_ACCESS_READ) == 0) {
		return (-API_ERR_ACCESS);
	}
	ret = entity_access(proc, id, ENTITY_ACCESS_READ);
	if (ret != 0) {
		return (ret);
	}
	entity_fill_stat(id, &out);
	memcpy(ustat, &out, sizeof(out));
	return (0);
}

int
api_entity_list(const struct api_entity_list *ulist)
{
	struct api_entity_list	list;
	char			path[ENTITY_PATH_MAX];
	size_t			bytes;
	int			ret;

	if (!ulist || !is_user_address(ulist, sizeof(list)) ||
	    !user_range_fault_in(ulist, sizeof(list), 0)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&list, ulist, sizeof(list));
	if (list.max_entries == 0 ||
	    list.max_entries > API_ENTITY_LIST_MAX_ENTRIES) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!list.entries) {
		return (-API_ERR_BAD_ADDR);
	}
	bytes = (size_t)list.max_entries * sizeof(*list.entries);
	if (!is_user_address(list.entries, bytes) ||
	    !user_range_fault_in(list.entries, bytes, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	ret = entity_copy_user_string(list.path, path, sizeof(path));
	if (ret != 0) {
		return (ret);
	}
	if (!entity_ns_is_path(path)) {
		return (-API_ERR_BAD_VALUE);
	}
	list.count = 0;
	ret = entity_ns_list(path, list.entries, list.max_entries,
	    &list.count);
	memcpy((void *)ulist, &list, sizeof(list));
	return (ret);
}

int
api_entity_query(const struct api_entity_query *uquery)
{
	struct api_entity_query	query;
	entity_query_ctx_t	qc;
	size_t			bytes;
	int			ret;

	if (!uquery || !is_user_address(uquery, sizeof(query)) ||
	    !user_range_fault_in(uquery, sizeof(query), 0)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&query, uquery, sizeof(query));
	if (query.archetype > ENTITY_ARCH_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	if (query.max_entries == 0 ||
	    query.max_entries > API_ENTITY_LIST_MAX_ENTRIES) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!query.entries) {
		return (-API_ERR_BAD_ADDR);
	}
	bytes = (size_t)query.max_entries * sizeof(*query.entries);
	if (!is_user_address(query.entries, bytes) ||
	    !user_range_fault_in(query.entries, bytes, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	qc.entries = query.entries;
	qc.max_entries = query.max_entries;
	qc.count = &query.count;
	query.count = 0;
	ret = entity_foreach(query.archetype, query.start,
	    entity_query_cb, &qc);
	(void)ret;
	memcpy((void *)uquery, &query, sizeof(query));
	return (0);
}

int
api_entity_ctl(int handle, u32 op, void *uarg)
{
	process_t	*proc;
	entity_id_t	id;
	u32		access;
	int		ret;

	proc = process_current();
	ret = entity_handle_lookup(proc, handle, &id, &access);
	if (ret != 0) {
		return (ret);
	}
	switch (op) {
	case ENTITY_CTL_GET_INFO:
		return (api_entity_stat(handle,
		    (struct api_entity_stat *)uarg));
	case ENTITY_CTL_GET_DATA: {
		struct api_entity_data	data;
		u64			value;

		if ((access & ENTITY_ACCESS_READ) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_READ);
		if (ret != 0) {
			return (ret);
		}
		if (!uarg || !is_user_address(uarg, sizeof(data)) ||
		    !user_range_fault_in(uarg, sizeof(data), 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		memcpy(&data, uarg, sizeof(data));
		ret = entity_get_data(id, data.index, &value);
		if (ret != 0) {
			return (ret);
		}
		data.value = value;
		memcpy(uarg, &data, sizeof(data));
		return (0);
	}
	case ENTITY_CTL_SET_DATA: {
		struct api_entity_data	data;

		if ((access & ENTITY_ACCESS_WRITE) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_WRITE);
		if (ret != 0) {
			return (ret);
		}
		if (!uarg || !is_user_address(uarg, sizeof(data)) ||
		    !user_range_fault_in(uarg, sizeof(data), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		memcpy(&data, uarg, sizeof(data));
		return (entity_set_data(id, data.index, data.value));
	}
	case ENTITY_CTL_GET_I32: {
		struct api_entity_data	data;
		s32			value;

		if ((access & ENTITY_ACCESS_READ) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_READ);
		if (ret != 0) {
			return (ret);
		}
		if (!uarg || !is_user_address(uarg, sizeof(data)) ||
		    !user_range_fault_in(uarg, sizeof(data), 1)) {
			return (-API_ERR_BAD_ADDR);
		}
		memcpy(&data, uarg, sizeof(data));
		ret = entity_get_i32(id, data.index, &value);
		if (ret != 0) {
			return (ret);
		}
		data.value = (u64)(s64)value;
		memcpy(uarg, &data, sizeof(data));
		return (0);
	}
	case ENTITY_CTL_SET_I32: {
		struct api_entity_data	data;

		if ((access & ENTITY_ACCESS_WRITE) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_WRITE);
		if (ret != 0) {
			return (ret);
		}
		if (!uarg || !is_user_address(uarg, sizeof(data)) ||
		    !user_range_fault_in(uarg, sizeof(data), 0)) {
			return (-API_ERR_BAD_ADDR);
		}
		memcpy(&data, uarg, sizeof(data));
		return (entity_set_i32(id, data.index, (s32)data.value));
	}
	case ENTITY_CTL_BIND: {
		char	name[ENTITY_PATH_MAX];

		if ((access & ENTITY_ACCESS_WRITE) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_WRITE);
		if (ret != 0) {
			return (ret);
		}
		ret = entity_copy_user_string((const char *)uarg, name,
		    sizeof(name));
		if (ret != 0) {
			return (ret);
		}
		if (!entity_ns_is_path(name)) {
			return (-API_ERR_BAD_VALUE);
		}
		return (entity_ns_bind(name, id));
	}
	case ENTITY_CTL_UNBIND:
		if ((access & ENTITY_ACCESS_WRITE) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_WRITE);
		if (ret != 0) {
			return (ret);
		}
		return (entity_ns_unbind_id(id));
	case ENTITY_CTL_DELETE:
		if ((access & ENTITY_ACCESS_WRITE) == 0) {
			return (-API_ERR_ACCESS);
		}
		ret = entity_access(proc, id, ENTITY_ACCESS_WRITE);
		if (ret != 0) {
			return (ret);
		}
		ret = entity_destroy(id);
		if (ret != 0) {
			return (ret);
		}
		return (entity_handle_drop(proc, handle, 0));
	default:
		return (-API_ERR_NOT_SUPPORTED);
	}
}
