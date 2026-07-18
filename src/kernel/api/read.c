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

#include <kernel/console/terminal.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

int
api_term_read(void *buf, u32 count, u32 flags)
{
	if (count == 0) {
		return (0);
	}

	if (!is_user_address(buf, count)) {
		printk("[DEBUG] api_term_read: invalid user buffer %p (%d)\n",
		    buf, (int)count);
		thread_t *td = thread_current();
		if (td && (td->context.cs & 3) == 3) {
			process_exit(-1);
		}
		return (-API_ERR_BAD_ADDR);
	}

	return (terminal_read(buf, count, flags));
}

int
api_data_read(int handle, void *buf, u32 count)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index;
	int		n;

	handles = api_get_handle_table();
	objects = api_get_object_table();

	if (handle < 0 || handle >= MAX_HANDLES) {
		printk("[DEBUG] api_data_read: invalid handle %d\n",
		    handle);
		return (-API_ERR_BAD_HANDLE);
	}

	if (!handles[handle].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (count == 0) {
		return (0);
	}

	if (!is_user_address(buf, count)) {
		printk("[DEBUG] api_data_read: invalid user buffer %p (%d)\n",
		    buf, (int)count);
		thread_t *td = thread_current();
		if (td && (td->context.cs & 3) == 3) {
			process_exit(-1);
		}
		return (-API_ERR_BAD_ADDR);
	}
	if (!user_range_fault_in(buf, count, 1)) {
		printk("[DEBUG] api_data_read: cannot fault user buffer %p (%d)\n",
		    buf, (int)count);
		return (-API_ERR_BAD_ADDR);
	}

	if (!(handles[handle].flags & API_OPEN_READ)) {
		return (-API_ERR_BAD_HANDLE);
	}

	object_index = handles[handle].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (objects[object_index].type == API_OBJECT_PIPE) {
		return (pipe_read((pipe_t *)objects[object_index].pipe,
		    buf, count));
	}

	if (objects[object_index].vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}

	n = vnode_read(objects[object_index].vn, buf, count,
	    objects[object_index].offset);
	if (n < 0) {
		return (n);
	}

	objects[object_index].offset += (u32)n;
	return (n);
}
