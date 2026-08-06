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
$define %type int as 32 bit signed
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type api_reg_value as native registry value IO descriptor
$define %type api_reg_entry as native registry enumeration entry
$define %type cm_entry as registry key or value enumeration record

$define %func api_reg_copy_string as function with args const char *, int
$define %func api_reg_flags_valid as function with args u32
$define %func api_reg_type_valid as function with args u32
$define %func api_reg_is_kusr as function with args void
$define %func api_reg_get_entity as function with args int, u64 *, u32 *
$define %func api_reg_object_hive as function with args entity_id
$define %func api_reg_object_key as function with args entity_id
$define %func api_reg_join_key as function with args base, child, out, size
$define %func api_reg_parent_key as function with args key, out, out_size
$define %func api_reg_check_write_open as function with args hive, key, kusr
$define %func api_reg_install as function with args hive, key, flags
$define %func api_reg_open as function with args hive, key, flags
$define %func api_reg_close as function with args int
$define %func api_reg_get as function with args int, api_reg_value *
$define %func api_reg_set as function with args int, api_reg_value *
$define %func api_reg_create_key as function with args int, const char *
$define %func api_reg_delete_key as function with args int, const char *
$define %func api_reg_delete_value as function with args int, const char *
$define %func api_reg_enum as function with args int, api_reg_entry *
$define %func api_reg_upd as function with args u32

*/

/* !SPACE!

$space %internal api_reg_copy_string, api_reg_flags_valid
$space %internal api_reg_type_valid, api_reg_is_kusr
$space %internal api_reg_get_entity, api_reg_object_hive
$space %internal api_reg_object_key, api_reg_join_key, api_reg_install
$space %internal api_reg_parent_key, api_reg_check_write_open
$space %export api_reg_open, api_reg_close, api_reg_get, api_reg_set
$space %export api_reg_create_key, api_reg_delete_key
$space %export api_reg_delete_value, api_reg_enum, api_reg_upd

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/cm/cm.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

#define	API_REG_MAX_STRING	256
#define	API_REG_MAX_VALUE_SIZE	(64 * 1024)

static char *
api_reg_copy_string(const char *src, int allow_null)
{
	char	*buf;
	u32	len;

	if (!src) {
		if (!allow_null) {
			return (NULL);
		}
		buf = (char *)kmem_calloc(1, 1);
		return (buf);
	}
	for (len = 0; len < API_REG_MAX_STRING; len++) {
		if (!is_user_address(src + len, 1)) {
			return (NULL);
		}
		if (src[len] == '\0') {
			break;
		}
	}
	if (len == API_REG_MAX_STRING) {
		return (NULL);
	}
	buf = (char *)kmem_calloc(len + 1, 1);
	if (!buf) {
		return (NULL);
	}
	if (len != 0) {
		memcpy(buf, src, len);
	}
	buf[len] = '\0';
	return (buf);
}

static int
api_reg_flags_valid(u32 flags)
{
	u32	known;

	known = API_REG_OPEN_READ | API_REG_OPEN_WRITE |
	    API_REG_OPEN_CREATE;
	if ((flags & ~known) != 0) {
		return (0);
	}
	if ((flags & (API_REG_OPEN_READ | API_REG_OPEN_WRITE)) == 0) {
		return (0);
	}
	if ((flags & API_REG_OPEN_CREATE) &&
	    (flags & API_REG_OPEN_WRITE) == 0) {
		return (0);
	}
	return (1);
}

static int
api_reg_type_valid(u32 type)
{
	return (type >= API_REG_TYPE_STRING &&
	    type <= API_REG_TYPE_MULTI_STRING);
}

static int
api_reg_is_kusr(void)
{
	process_t	*proc;

	proc = process_current();
	return (proc != NULL && proc->kusr_auth ? 1 : 0);
}

static int
api_reg_get_entity(int handle, entity_id_t *id, u32 *flags)
{
	process_t	*proc;
	u32		access;
	int		ret;

	if (!id) {
		return (-API_ERR_BAD_VALUE);
	}
	*id = 0;
	if (flags) {
		*flags = 0;
	}
	proc = process_current();
	ret = entity_handle_lookup(proc, handle, id, &access);
	if (ret != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entity_arch(*id) != ENTITY_ARCH_REG) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (flags) {
		*flags = access;
	}
	return (0);
}

static const char *
api_reg_object_hive(entity_id_t id)
{
	char	*buf;

	buf = (char *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (!buf) {
		return ("");
	}
	return (buf);
}

static const char *
api_reg_object_key(entity_id_t id)
{
	char	*buf;

	buf = (char *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (!buf) {
		return ("");
	}
	return (buf + strlen(buf) + 1);
}

static int
api_reg_join_key(const char *base, const char *child, char *out, u32 out_size)
{
	u32	base_len, child_len, need;

	if (!base || !child || child[0] == '\0' || !out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	base_len = (u32)strlen(base);
	child_len = (u32)strlen(child);
	need = child_len + 1;
	if (base_len != 0) {
		need += base_len + 1;
	}
	if (need > out_size) {
		return (-API_ERR_TOO_BIG);
	}
	out[0] = '\0';
	if (base_len != 0) {
		strcpy(out, base);
		strcat(out, ".");
	}
	strcat(out, child);
	return (0);
}

static int
api_reg_parent_key(const char *key, char *out, u32 out_size)
{
	u32	i, len, parent_len;

	if (!out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	out[0] = '\0';
	if (!key || key[0] == '\0') {
		return (0);
	}
	len = (u32)strlen(key);
	parent_len = 0;
	for (i = len; i > 0; i--) {
		if (key[i - 1] == '.' || key[i - 1] == '/' ||
		    key[i - 1] == '\\') {
			parent_len = i - 1;
			break;
		}
	}
	if (parent_len == 0) {
		return (0);
	}
	if (parent_len >= out_size) {
		return (-API_ERR_TOO_BIG);
	}
	memcpy(out, key, parent_len);
	out[parent_len] = '\0';
	return (0);
}

static int
api_reg_check_write_open(const char *hive, const char *key, int is_kusr)
{
	int	add_ret, edit_ret;

	add_ret = cm_check_access(hive, key, NULL, CM_ACCESS_ADD,
	    is_kusr);
	if (add_ret == 0) {
		return (0);
	}
	edit_ret = cm_check_access(hive, key, NULL, CM_ACCESS_EDIT,
	    is_kusr);
	if (edit_ret == 0) {
		return (0);
	}
	if (add_ret == -API_ERR_PERM || edit_ret == -API_ERR_PERM) {
		return (-API_ERR_PERM);
	}
	return (edit_ret);
}

static int
api_reg_install(const char *hive, const char *key, u32 flags)
{
	entity_id_t	id;
	char		*buf;
	u32		hive_len, key_len, access;
	int		handle;

	hive_len = (u32)strlen(hive);
	key_len = (u32)strlen(key);
	if (hive_len == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (hive_len + 1 + key_len + 1 > 256) {
		return (-API_ERR_TOO_BIG);
	}
	id = entity_io_create_raw(ENTITY_ARCH_REG, 0);
	if (id == 0) {
		return (-API_ERR_NO_MEMORY);
	}
	buf = (char *)kmem_calloc(hive_len + 1 + key_len + 1, 1);
	if (!buf) {
		entity_destroy(id);
		return (-API_ERR_NO_MEMORY);
	}
	memcpy(buf, hive, hive_len);
	memcpy(buf + hive_len + 1, key, key_len);
	entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING, buf);
	access = 0;
	if (flags & API_REG_OPEN_READ) {
		access |= ENTITY_ACCESS_READ;
	}
	if (flags & API_REG_OPEN_WRITE) {
		access |= ENTITY_ACCESS_WRITE;
	}
	handle = entity_io_attach(id, access);
	if (handle < 0) {
		entity_destroy(id);
		return (handle);
	}
	return (handle);
}

int
api_reg_open(const char *uhive, const char *ukey, u32 flags)
{
	char	*hive;
	char	*key;
	char	parent[256];
	int	is_kusr, ret;

	if (!api_reg_flags_valid(flags)) {
		return (-API_ERR_BAD_VALUE);
	}
	hive = api_reg_copy_string(uhive, 0);
	key = api_reg_copy_string(ukey, 1);
	if (!hive || !key) {
		if (hive) {
			kmem_free(hive);
		}
		if (key) {
			kmem_free(key);
		}
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_key_exists(hive, key);
	if (ret != 0 && (flags & API_REG_OPEN_CREATE) &&
	    key[0] != '\0') {
		ret = api_reg_parent_key(key, parent, sizeof(parent));
		if (ret == 0) {
			ret = cm_check_access(hive, parent, NULL,
			    CM_ACCESS_ADD, is_kusr);
		}
		if (ret == 0) {
			ret = cm_create_key(hive, key);
		}
	}
	if (ret != 0) {
		kmem_free(hive);
		kmem_free(key);
		return (ret);
	}
	if (flags & API_REG_OPEN_READ) {
		ret = cm_check_access(hive, key, NULL, CM_ACCESS_READ,
		    is_kusr);
		if (ret != 0) {
			kmem_free(hive);
			kmem_free(key);
			return (ret);
		}
	}
	if (flags & API_REG_OPEN_WRITE) {
		ret = api_reg_check_write_open(hive, key, is_kusr);
		if (ret != 0) {
			kmem_free(hive);
			kmem_free(key);
			return (ret);
		}
	}
	ret = api_reg_install(hive, key, flags);
	kmem_free(hive);
	kmem_free(key);
	return (ret);
}

int
api_reg_close(int handle)
{
	entity_id_t	id;
	int		ret;

	ret = api_reg_get_entity(handle, &id, NULL);
	if (ret != 0) {
		return (ret);
	}
	return (api_data_close(handle));
}

int
api_reg_get(int handle, struct api_reg_value *uvalue)
{
	struct api_reg_value	value;
	entity_id_t		id;
	char			*name;
	u32			type, need, got;
	u32			flags;
	int			is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_READ) == 0) {
		return (-API_ERR_ACCESS);
	}
	if (!uvalue || !is_user_address(uvalue, sizeof(*uvalue)) ||
	    !user_range_fault_in(uvalue, sizeof(*uvalue), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&value, uvalue, sizeof(value));
	name = api_reg_copy_string(value.name, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(id),
	    api_reg_object_key(id), name, CM_ACCESS_READ, is_kusr);
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}
	type = 0;
	need = 0;
	ret = cm_value_info(api_reg_object_hive(id),
	    api_reg_object_key(id), name, &type, &need);
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}
	if (need > 0x7FFFFFFFU) {
		kmem_free(name);
		return (-API_ERR_FILE_TOO_BIG);
	}
	value.type = type;
	value.bytes = need;
	if (!value.data || value.size == 0) {
		memcpy(uvalue, &value, sizeof(value));
		kmem_free(name);
		return ((int)need);
	}
	if (need > value.size) {
		memcpy(uvalue, &value, sizeof(value));
		kmem_free(name);
		return (-API_ERR_TOO_BIG);
	}
	if (need != 0 && (!is_user_address(value.data, need) ||
	    !user_range_fault_in(value.data, need, 1))) {
		kmem_free(name);
		return (-API_ERR_BAD_ADDR);
	}
	got = 0;
	ret = cm_read_value(api_reg_object_hive(id),
	    api_reg_object_key(id), name, value.data, value.size, &got);
	if (ret == 0) {
		value.bytes = got;
		memcpy(uvalue, &value, sizeof(value));
		ret = (int)got;
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_set(int handle, const struct api_reg_value *uvalue)
{
	struct api_reg_value	value;
	entity_id_t		id;
	char			*name;
	u32			old_type, old_size;
	u32			flags;
	int			is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}
	if (!uvalue || !is_user_address(uvalue, sizeof(*uvalue)) ||
	    !user_range_fault_in(uvalue, sizeof(*uvalue), 0)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&value, uvalue, sizeof(value));
	if (value.flags != 0 || !api_reg_type_valid(value.type) ||
	    value.size > API_REG_MAX_VALUE_SIZE) {
		return (-API_ERR_BAD_VALUE);
	}
	if (value.size != 0 && (!value.data ||
	    !is_user_address(value.data, value.size) ||
	    !user_range_fault_in(value.data, value.size, 0))) {
		return (-API_ERR_BAD_ADDR);
	}
	name = api_reg_copy_string(value.name, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	old_type = 0;
	old_size = 0;
	ret = cm_value_info(api_reg_object_hive(id),
	    api_reg_object_key(id), name, &old_type, &old_size);
	if (ret == 0) {
		ret = cm_check_access(api_reg_object_hive(id),
		    api_reg_object_key(id), name, CM_ACCESS_EDIT,
		    is_kusr);
	} else if (ret == -API_ERR_NOT_FOUND) {
		ret = cm_check_access(api_reg_object_hive(id),
		    api_reg_object_key(id), NULL, CM_ACCESS_ADD,
		    is_kusr);
	}
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}
	ret = cm_set_value(api_reg_object_hive(id),
	    api_reg_object_key(id), name, value.type, value.data,
	    value.size);
	kmem_free(name);
	return (ret);
}

int
api_reg_create_key(int handle, const char *uname)
{
	entity_id_t	id;
	char		*name;
	char		key[256];
	u32		flags;
	int		is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}
	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(id),
	    api_reg_object_key(id), NULL, CM_ACCESS_ADD, is_kusr);
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}
	ret = api_reg_join_key(api_reg_object_key(id), name, key,
	    sizeof(key));
	if (ret == 0) {
		ret = cm_create_key(api_reg_object_hive(id), key);
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_delete_key(int handle, const char *uname)
{
	entity_id_t	id;
	char		*name;
	char		key[256];
	u32		flags;
	int		is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}
	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	ret = api_reg_join_key(api_reg_object_key(id), name, key,
	    sizeof(key));
	if (ret == 0) {
		is_kusr = api_reg_is_kusr();
		ret = cm_check_access(api_reg_object_hive(id), key,
		    NULL, CM_ACCESS_EDIT, is_kusr);
		if (ret == 0) {
			ret = cm_delete_key(api_reg_object_hive(id), key);
		}
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_delete_value(int handle, const char *uname)
{
	entity_id_t	id;
	char		*name;
	u32		flags;
	int		is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}
	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(id),
	    api_reg_object_key(id), name, CM_ACCESS_EDIT, is_kusr);
	if (ret == 0) {
		ret = cm_delete_value(api_reg_object_hive(id),
		    api_reg_object_key(id), name);
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_enum(int handle, struct api_reg_entry *uentry)
{
	struct api_reg_entry	entry;
	entity_id_t		id;
	cm_entry_t		cm_entry;
	u32			index;
	u32			flags;
	int			is_kusr, ret;

	ret = api_reg_get_entity(handle, &id, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & ENTITY_ACCESS_READ) == 0) {
		return (-API_ERR_ACCESS);
	}
	if (!uentry || !is_user_address(uentry, sizeof(*uentry)) ||
	    !user_range_fault_in(uentry, sizeof(*uentry), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	memcpy(&entry, uentry, sizeof(entry));
	index = entry.index;
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(id),
	    api_reg_object_key(id), NULL, CM_ACCESS_READ, is_kusr);
	if (ret != 0) {
		return (ret);
	}
	memset(&cm_entry, 0, sizeof(cm_entry));
	ret = cm_enum_entry(api_reg_object_hive(id),
	    api_reg_object_key(id), index, &cm_entry);
	if (ret <= 0) {
		return (ret);
	}
	memset(&entry, 0, sizeof(entry));
	entry.index = index;
	entry.kind = cm_entry.kind;
	entry.type = cm_entry.type;
	entry.size = cm_entry.size;
	memcpy(entry.name, cm_entry.name, sizeof(entry.name));
	memcpy(uentry, &entry, sizeof(entry));
	return (1);
}

int
api_reg_upd(u32 consumer)
{
	return (cm_update_consumer_user(consumer, 0, api_reg_is_kusr()));
}
