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
$define %type vnode as VFS vnode

$define %func copy_user_path as function with args const char *
$define %func api_flags_valid as function with args int
$define %func api_flags_to_access as function with args int
$define %func api_data_open as function with args const char *, int

*/

/* !SPACE!

$space %internal copy_user_path, api_flags_valid, api_flags_to_access
$space %export api_data_open

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/driver_ns.h>
#include <kernel/entity/entity.h>
#include <kernel/other/restrict.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static char *
copy_user_path(const char *path)
{
	char	*buf;
	int	len;

	if (!path) {
		return (NULL);
	}
	if (!is_user_address(path, 1)) {
		return (NULL);
	}
	len = 0;
	while (len < 255) {
		if (!is_user_address(path + len, 1)) {
			return (NULL);
		}
		if (path[len] == '\0') {
			break;
		}
		len++;
	}
	if (len == 255) {
		return (NULL);
	}
	buf = (char *)kmem_calloc(len + 1, 1);
	if (!buf) {
		return (NULL);
	}
	memcpy(buf, path, len);
	buf[len] = '\0';
	return (buf);
}

static int
api_flags_valid(int flags)
{
	if ((flags & API_OPEN_RW) == 0) {
		return (0);
	}
	if ((flags & (API_OPEN_CREATE | API_OPEN_TRUNC | API_OPEN_APPEND)) &&
	    !(flags & API_OPEN_WRITE)) {
		return (0);
	}
	return (1);
}

static u32
api_flags_to_access(int flags)
{
	u32	access;

	access = 0;
	if (flags & API_OPEN_READ) {
		access |= ENTITY_ACCESS_READ;
	}
	if (flags & API_OPEN_WRITE) {
		access |= ENTITY_ACCESS_WRITE;
	}
	return (access);
}

int
api_data_open(const char *path, int flags)
{
	char		*kpath;
	char		*kpath_copy;
	vnode_t		*vn;
	entity_id_t	id;
	posix_stat_t	st;
	int		handle, ret, path_len;

	kpath = copy_user_path(path);
	if (!kpath || kpath[0] == 0) {
		if (kpath) {
			kmem_free(kpath);
		}
		return (-API_ERR_BAD_ADDR);
	}
	if (!api_flags_valid(flags)) {
		kmem_free(kpath);
		return (-API_ERR_BAD_VALUE);
	}
	if (restrict_kusr_check(kpath)) {
		kmem_free(kpath);
		return (-API_ERR_PERM);
	}
	vn = NULL;
	if (driver_ns_is_path(kpath)) {
		vn = driver_ns_lookup(kpath);
		if (vn == NULL) {
			kmem_free(kpath);
			return (-API_ERR_NOT_FOUND);
		}
		if (flags & (API_OPEN_CREATE | API_OPEN_TRUNC)) {
			vnode_release(vn);
			kmem_free(kpath);
			return (-API_ERR_NOT_SUPPORTED);
		}
	} else {
		ret = vfs_resolve(kpath, &vn);
		if (ret != 0 || vn == NULL) {
			if (!(flags & API_OPEN_CREATE)) {
				kmem_free(kpath);
				return (ret != 0 ? ret :
				    -API_ERR_NOT_FOUND);
			}
			ret = vfs_create_file(kpath);
			if (ret != 0) {
				kmem_free(kpath);
				return (ret);
			}
			ret = vfs_resolve(kpath, &vn);
			if (ret != 0 || vn == NULL) {
				kmem_free(kpath);
				return (ret != 0 ? ret : -API_ERR_IO);
			}
		}
	}
	if (vn->type == VDIR) {
		vnode_release(vn);
		kmem_free(kpath);
		return (-API_ERR_IS_DIR);
	}
	if (vn->type == VCHR && strcmp(vn->name, "fb0") == 0 &&
	    !proc_has_privilege(process_current())) {
		vnode_release(vn);
		kmem_free(kpath);
		return (-API_ERR_PERM);
	}
	if ((flags & API_OPEN_TRUNC) && vn->type != VCHR) {
		ret = vfs_truncate(kpath, 0);
		if (ret != 0) {
			vnode_release(vn);
			kmem_free(kpath);
			return (ret);
		}
		vn->size = 0;
	}
	id = entity_io_create_raw(
	    vn->type == VCHR ? ENTITY_ARCH_VNODE : ENTITY_ARCH_FILE,
	    0);
	if (id == 0) {
		vnode_release(vn);
		kmem_free(kpath);
		return (-API_ERR_NO_MEMORY);
	}
	kpath_copy = (char *)kmem_calloc(255, 1);
	if (!kpath_copy) {
		entity_destroy(id);
		vnode_release(vn);
		kmem_free(kpath);
		return (-API_ERR_NO_MEMORY);
	}
	path_len = (int)strlen(kpath);
	if (path_len > 254) {
		path_len = 254;
	}
	memcpy(kpath_copy, kpath, path_len);
	entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING, vn);
	entity_io_set_ptr(id, ENTITY_IO_PTR_PATH, kpath_copy);
	entity_io_set_i32(id, ENTITY_IO_I32_FLAGS, (s32)flags);
	if (flags & API_OPEN_APPEND) {
		if (vnode_stat(vn, &st) == 0) {
			entity_io_set_i32(id, ENTITY_IO_I32_OFFSET,
			    (s32)st.st_size);
		} else {
			entity_io_set_i32(id, ENTITY_IO_I32_OFFSET, 0);
		}
	} else {
		entity_io_set_i32(id, ENTITY_IO_I32_OFFSET, 0);
	}
	handle = entity_io_attach(id, api_flags_to_access(flags));
	kmem_free(kpath);
	if (handle < 0) {
		entity_destroy(id);
		return (handle);
	}
	return (handle);
}
