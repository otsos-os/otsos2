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

#include <kernel/api/api.h>

long
api_data_seek(int handle, long offset, int whence)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index, ret;
	posix_stat_t	st;
	long long	new_off;

	handles = api_get_handle_table();
	objects = api_get_object_table();

	if (handle < 0 || handle >= MAX_HANDLES) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (!handles[handle].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	object_index = handles[handle].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS) {
		return (-API_ERR_NOT_SEEKABLE);
	}
	if (!objects[object_index].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (objects[object_index].type == API_OBJECT_PIPE) {
		return (-API_ERR_NOT_SEEKABLE);
	}

	if (objects[object_index].type == API_OBJECT_VNODE &&
	    (objects[object_index].vn == NULL ||
	    strcmp(objects[object_index].vn->name, "fb0") != 0)) {
		return (-API_ERR_NOT_SEEKABLE);
	}

	if (objects[object_index].vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}

	ret = vnode_stat(objects[object_index].vn, &st);
	if (ret != 0) {
		return (ret);
	}

	new_off = 0;
	switch (whence) {
	case API_SEEK_SET:
		new_off = (long long)offset;
		break;
	case API_SEEK_CUR:
		new_off = (long long)objects[object_index].offset +
		    (long long)offset;
		break;
	case API_SEEK_END:
		new_off = (long long)st.st_size + (long long)offset;
		break;
	default:
		return (-API_ERR_BAD_VALUE);
	}

	if (new_off < 0) {
		return (-API_ERR_BAD_VALUE);
	}

	objects[object_index].offset = (u32)new_off;
	return ((long)objects[object_index].offset);
}
