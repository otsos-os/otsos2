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

#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/driver_ns.h>
#include <kernel/api/api.h>
#include <kernel/other/restrict.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

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
api_find_free_handle(void)
{
	api_handle_t	*handles;
	int		i;

	handles = api_get_handle_table();
	for (i = 0; i < MAX_HANDLES; i++) {
		if (!handles[i].used) {
			return (i);
		}
	}
	return (-API_ERR_HANDLES_FULL);
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

int
api_data_open(const char *path, int flags)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	char		*kpath;
	vnode_t		*vn;
	int		handle;
	int		object_index;
	int		ret;
	posix_stat_t	st;

	handles = api_get_handle_table();
	objects = api_get_object_table();

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
				return (ret != 0 ? ret : -API_ERR_NOT_FOUND);
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

	handle = api_find_free_handle();
	if (handle < 0) {
		vnode_release(vn);
		kmem_free(kpath);
		return (handle);
	}

	object_index = api_alloc_object();
	if (object_index < 0) {
		vnode_release(vn);
		kmem_free(kpath);
		return (object_index);
	}

	objects[object_index].flags = flags;
	objects[object_index].vn = vn;
	objects[object_index].type =
	    (vn->type == VCHR) ? API_OBJECT_VNODE : API_OBJECT_FILE;

	if (flags & API_OPEN_APPEND) {
		if (vnode_stat(vn, &st) == 0) {
			objects[object_index].offset = (u32)st.st_size;
		} else {
			objects[object_index].offset = 0;
		}
	} else {
		objects[object_index].offset = 0;
	}

	memset(objects[object_index].path, 0,
	    sizeof(objects[object_index].path));
	{
		int	path_len;

		path_len = strlen(kpath);
		if (path_len >= (int)sizeof(objects[object_index].path)) {
			path_len = (int)sizeof(objects[object_index].path) - 1;
		}
		memcpy(objects[object_index].path, kpath, path_len);
	}

	kmem_free(kpath);

	handles[handle].used = 1;
	handles[handle].flags = flags;
	handles[handle].object_index = object_index;

	return (handle);
}
