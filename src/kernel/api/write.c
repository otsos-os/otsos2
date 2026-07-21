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
#include <kernel/net/endpoint.h>
#include <kernel/ipc/ipc.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static api_handle_t	kernel_handles[MAX_HANDLES];
static api_object_t	api_objects[MAX_DATA_OBJECTS];

api_handle_t *
api_get_handle_table(void)
{
	process_t	*proc;

	proc = process_current();
	if (proc) {
		return (proc->handles);
	}
	return (kernel_handles);
}

api_object_t *
api_get_object_table(void)
{
	return (api_objects);
}

int
api_alloc_object(void)
{
	int	i;

	for (i = 0; i < MAX_DATA_OBJECTS; i++) {
		if (!api_objects[i].used) {
			api_objects[i].used = 1;
			api_objects[i].refcount = 1;
			api_objects[i].offset = 0;
			api_objects[i].flags = 0;
			api_objects[i].type = API_OBJECT_FILE;
			api_objects[i].pipe = NULL;
			api_objects[i].net = NULL;
			api_objects[i].ipc = NULL;
			api_objects[i].vn = NULL;
			memset(api_objects[i].path, 0,
			    sizeof(api_objects[i].path));
			return (i);
		}
	}
	return (-API_ERR_OBJECTS_FULL);
}

void
api_release_object(int index)
{
	if (index < 0 || index >= MAX_DATA_OBJECTS) {
		return;
	}
	if (!api_objects[index].used) {
		return;
	}

	api_objects[index].refcount--;
	if (api_objects[index].refcount <= 0) {
		if (api_objects[index].type == API_OBJECT_PIPE &&
		    api_objects[index].pipe) {
			pipe_t	*p;

			p = (pipe_t *)api_objects[index].pipe;
			if (api_objects[index].flags & API_OPEN_WRITE) {
				if (p->writers > 0) {
					p->writers--;
				}
			} else {
				if (p->readers > 0) {
					p->readers--;
				}
			}
			if (p->readers == 0 && p->writers == 0) {
				kmem_free(p);
			}
		}

		if (api_objects[index].type == API_OBJECT_NET &&
		    api_objects[index].net) {
			net_endpoint_close(
			    (net_endpoint_t *)api_objects[index].net);
			api_objects[index].net = NULL;
		}
		if (api_objects[index].type == API_OBJECT_IPC &&
		    api_objects[index].ipc) {
			ipc_endpoint_release(
			    (ipc_endpoint_t *)api_objects[index].ipc);
			api_objects[index].ipc = NULL;
		}

		if (api_objects[index].vn) {
			vnode_release(api_objects[index].vn);
			api_objects[index].vn = NULL;
		}

		memset(&api_objects[index], 0, sizeof(api_objects[index]));
	}
}

void
api_init(void)
{
	printk("[API] Initializing handle tables at %p\n",
	    kernel_handles);
	memset(kernel_handles, 0, sizeof(kernel_handles));
	memset(api_objects, 0, sizeof(api_objects));
	ipc_init();
}

void
api_init_process(struct process *proc)
{
	if (!proc) {
		return;
	}
	memcpy(proc->handles, kernel_handles, sizeof(kernel_handles));
}

void
api_copy_handles(struct process *dst, const struct process *src)
{
	int	i;

	if (!dst || !src) {
		return;
	}
	memcpy(dst->handles, src->handles, sizeof(dst->handles));
	for (i = 0; i < MAX_HANDLES; i++) {
		if (!dst->handles[i].used) {
			continue;
		}
		int	object_index;

		object_index = dst->handles[i].object_index;
		if (object_index >= 0 && object_index < MAX_DATA_OBJECTS &&
		    api_objects[object_index].used) {
			api_objects[object_index].refcount++;
		}
	}
}

void
api_release_handles(struct process *proc)
{
	int	i;

	if (!proc) {
		return;
	}
	for (i = 0; i < MAX_HANDLES; i++) {
		if (!proc->handles[i].used) {
			continue;
		}
		int	object_index;

		object_index = proc->handles[i].object_index;
		if (object_index >= 0) {
			api_release_object(object_index);
		}
		proc->handles[i].used = 0;
		proc->handles[i].flags = 0;
		proc->handles[i].object_index = -1;
	}
}

int
api_term_write(const void *buf, u32 count)
{
	if (count == 0) {
		return (0);
	}

	if (!is_user_address(buf, count)) {
		thread_t	*td;

		td = thread_current();
		if (td && (td->context.cs & 3) == 3) {
			process_exit(-1);
		}
		return (-API_ERR_BAD_ADDR);
	}

	return (terminal_write(buf, count));
}

int
api_data_write(int handle, const void *buf, u32 count)
{
	api_handle_t	*handles;
	api_object_t	*objects;
	int		object_index;
	int		n;

	handles = api_get_handle_table();
	objects = api_get_object_table();

	if (handle < 0 || handle >= MAX_HANDLES) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (!handles[handle].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (count == 0) {
		return (0);
	}

	if (!is_user_address(buf, count)) {
		thread_t	*td;

		td = thread_current();
		if (td && (td->context.cs & 3) == 3) {
			process_exit(-1);
		}
		return (-API_ERR_BAD_ADDR);
	}

	if (!(handles[handle].flags & API_OPEN_WRITE)) {
		return (-API_ERR_BAD_HANDLE);
	}

	object_index = handles[handle].object_index;
	if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
	    !objects[object_index].used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (objects[object_index].type == API_OBJECT_PIPE) {
		return (pipe_write((pipe_t *)objects[object_index].pipe,
		    buf, count));
	}

	if (objects[object_index].vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (handles[handle].flags & API_OPEN_APPEND) {
		posix_stat_t	st;

		if (vnode_stat(objects[object_index].vn, &st) == 0) {
			objects[object_index].offset = (u32)st.st_size;
		}
	}

	n = vnode_write(objects[object_index].vn, buf, count,
	    objects[object_index].offset);
	if (n < 0) {
		return (n);
	}

	objects[object_index].offset += (u32)n;
	return (n);
}
