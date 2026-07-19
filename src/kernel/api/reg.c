/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type api_handle_t as process handle table entry
$define %type api_object_t as global API object table entry
$define %type api_reg_value as native registry value IO descriptor
$define %type api_reg_entry as native registry enumeration entry

$define %func api_reg_copy_string as function with args const char *, int
$define %func api_reg_find_free_handle as function with args void
$define %func api_reg_flags_valid as function with args u32
$define %func api_reg_type_valid as function with args u32
$define %func api_reg_is_kusr as function with args void
$define %func api_reg_get_object as function with args int, object, flags
$define %func api_reg_object_hive as function with args api_object_t *
$define %func api_reg_object_key as function with args api_object_t *
$define %func api_reg_join_key as function with args base, child, out
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

$space %internal api_reg_copy_string, api_reg_find_free_handle
$space %internal api_reg_flags_valid, api_reg_type_valid, api_reg_is_kusr
$space %internal api_reg_get_object, api_reg_object_hive
$space %internal api_reg_object_key, api_reg_join_key, api_reg_install
$space %internal api_reg_parent_key, api_reg_check_write_open
$space %export api_reg_open, api_reg_close, api_reg_get, api_reg_set
$space %export api_reg_create_key, api_reg_delete_key
$space %export api_reg_delete_value, api_reg_enum, api_reg_upd

*/

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

#include <kernel/api/api.h>
#include <kernel/cm/cm.h>
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
api_reg_find_free_handle(void)
{
	api_handle_t	*handles;
	int		i;

	handles = api_get_handle_table();
	for (i = 3; i < MAX_HANDLES; i++) {
		if (!handles[i].used) {
			return (i);
		}
	}
	return (-API_ERR_HANDLES_FULL);
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
api_reg_get_object(int handle, api_object_t **out_obj, int *out_flags)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index;

	if (!out_obj) {
		return (-API_ERR_BAD_VALUE);
	}
	*out_obj = NULL;
	if (out_flags) {
		*out_flags = 0;
	}

	if (handle < 0 || handle >= MAX_HANDLES) {
		return (-API_ERR_BAD_HANDLE);
	}

	handles = api_get_handle_table();
	if (!handles[handle].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	object_index = handles[handle].object_index;
	objects = api_get_object_table();
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used ||
	    objects[object_index].type != API_OBJECT_REG) {
		return (-API_ERR_BAD_HANDLE);
	}

	*out_obj = &objects[object_index];
	if (out_flags) {
		*out_flags = handles[handle].flags;
	}
	return (0);
}

static const char *
api_reg_object_hive(api_object_t *obj)
{
	return (obj->path);
}

static const char *
api_reg_object_key(api_object_t *obj)
{
	return (obj->path + strlen(obj->path) + 1);
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
	api_handle_t	*handles;
	api_object_t	*objects;
	api_object_t	*obj;
	u32		hive_len, key_len;
	int		handle, object_index;

	hive_len = (u32)strlen(hive);
	key_len = (u32)strlen(key);
	if (hive_len == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (hive_len + 1 + key_len + 1 > 256) {
		return (-API_ERR_TOO_BIG);
	}

	handle = api_reg_find_free_handle();
	if (handle < 0) {
		return (handle);
	}

	object_index = api_alloc_object();
	if (object_index < 0) {
		return (object_index);
	}

	handles = api_get_handle_table();
	objects = api_get_object_table();
	obj = &objects[object_index];

	obj->type = API_OBJECT_REG;
	obj->flags = (int)flags;
	memset(obj->path, 0, sizeof(obj->path));
	memcpy(obj->path, hive, hive_len);
	memcpy(obj->path + hive_len + 1, key, key_len);

	handles[handle].used = 1;
	handles[handle].flags = (int)flags;
	handles[handle].object_index = object_index;
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
	api_object_t	*obj;
	int		ret;

	ret = api_reg_get_object(handle, &obj, NULL);
	if (ret != 0) {
		return (ret);
	}
	(void)obj;
	return (api_data_close(handle));
}

int
api_reg_get(int handle, struct api_reg_value *uvalue)
{
	struct api_reg_value	value;
	api_object_t		*obj;
	char			*name;
	u32			type, need, got;
	int			flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_READ) == 0) {
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
	ret = cm_check_access(api_reg_object_hive(obj),
	    api_reg_object_key(obj), name, CM_ACCESS_READ, is_kusr);
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}

	type = 0;
	need = 0;
	ret = cm_value_info(api_reg_object_hive(obj), api_reg_object_key(obj),
	    name, &type, &need);
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
	ret = cm_read_value(api_reg_object_hive(obj),
	    api_reg_object_key(obj), name, value.data, value.size, &got);
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
	api_object_t		*obj;
	char			*name;
	u32			old_type, old_size;
	int			flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_WRITE) == 0) {
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
	ret = cm_value_info(api_reg_object_hive(obj),
	    api_reg_object_key(obj), name, &old_type, &old_size);
	if (ret == 0) {
		ret = cm_check_access(api_reg_object_hive(obj),
		    api_reg_object_key(obj), name, CM_ACCESS_EDIT,
		    is_kusr);
	} else if (ret == -API_ERR_NOT_FOUND) {
		ret = cm_check_access(api_reg_object_hive(obj),
		    api_reg_object_key(obj), NULL, CM_ACCESS_ADD,
		    is_kusr);
	}
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}

	ret = cm_set_value(api_reg_object_hive(obj), api_reg_object_key(obj),
	    name, value.type, value.data, value.size);
	kmem_free(name);
	return (ret);
}

int
api_reg_create_key(int handle, const char *uname)
{
	api_object_t	*obj;
	char		*name;
	char		key[256];
	int		flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}

	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(obj),
	    api_reg_object_key(obj), NULL, CM_ACCESS_ADD, is_kusr);
	if (ret != 0) {
		kmem_free(name);
		return (ret);
	}
	ret = api_reg_join_key(api_reg_object_key(obj), name, key,
	    sizeof(key));
	if (ret == 0) {
		ret = cm_create_key(api_reg_object_hive(obj), key);
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_delete_key(int handle, const char *uname)
{
	api_object_t	*obj;
	char		*name;
	char		key[256];
	int		flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}

	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	ret = api_reg_join_key(api_reg_object_key(obj), name, key,
	    sizeof(key));
	if (ret == 0) {
		is_kusr = api_reg_is_kusr();
		ret = cm_check_access(api_reg_object_hive(obj), key,
		    NULL, CM_ACCESS_EDIT, is_kusr);
		if (ret == 0) {
			ret = cm_delete_key(api_reg_object_hive(obj), key);
		}
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_delete_value(int handle, const char *uname)
{
	api_object_t	*obj;
	char		*name;
	int		flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_WRITE) == 0) {
		return (-API_ERR_ACCESS);
	}

	name = api_reg_copy_string(uname, 0);
	if (!name) {
		return (-API_ERR_BAD_ADDR);
	}
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(obj),
	    api_reg_object_key(obj), name, CM_ACCESS_EDIT, is_kusr);
	if (ret == 0) {
		ret = cm_delete_value(api_reg_object_hive(obj),
		    api_reg_object_key(obj), name);
	}
	kmem_free(name);
	return (ret);
}

int
api_reg_enum(int handle, struct api_reg_entry *uentry)
{
	struct api_reg_entry	entry;
	api_object_t		*obj;
	cm_entry_t		cm_entry;
	u32			index;
	int			flags, is_kusr, ret;

	ret = api_reg_get_object(handle, &obj, &flags);
	if (ret != 0) {
		return (ret);
	}
	if ((flags & API_REG_OPEN_READ) == 0) {
		return (-API_ERR_ACCESS);
	}
	if (!uentry || !is_user_address(uentry, sizeof(*uentry)) ||
	    !user_range_fault_in(uentry, sizeof(*uentry), 1)) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&entry, uentry, sizeof(entry));
	index = entry.index;
	is_kusr = api_reg_is_kusr();
	ret = cm_check_access(api_reg_object_hive(obj),
	    api_reg_object_key(obj), NULL, CM_ACCESS_READ, is_kusr);
	if (ret != 0) {
		return (ret);
	}

	memset(&cm_entry, 0, sizeof(cm_entry));
	ret = cm_enum_entry(api_reg_object_hive(obj),
	    api_reg_object_key(obj), index, &cm_entry);
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
