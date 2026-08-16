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
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type process as struct with process control block
$define %type pipe as struct with pipe ring buffer
$define %type net_endpoint as native network endpoint state
$define %type ipc_endpoint as native IPC endpoint state
$define %type vnode as VFS vnode

$define %func entity_io_release_pipe as procedure with args entity id
$define %func entity_io_release_file as procedure with args entity id
$define %func entity_io_release_net as procedure with args entity id
$define %func entity_io_release_ipc as procedure with args entity id
$define %func entity_io_release_reg as procedure with args entity id
$define %func entity_io_init as procedure with args void
$define %func entity_io_create_raw as function with args archetype, flags
$define %func entity_io_attach as function with args entity id, access
$define %func entity_io_open_id as function with args entity id, access
$define %func entity_io_ptr as function with args entity id, index
$define %func entity_io_set_ptr as function with args entity id, index, pointer
$define %func entity_io_i32 as function with args entity id, index, s32 *
$define %func entity_io_set_i32 as function with args entity id, index, s32
$define %func entity_io_vnode_read as function with args entity id, buffer, count, offset
$define %func entity_io_vnode_write as function with args entity id, buffer, count, offset
$define %func entity_io_vnode_seek as function with args entity id, offset, whence, result
$define %func entity_io_vnode_ioctl as function with args entity id, command, argument
$define %func entity_io_vnode_stat as function with args entity id, size
$define %func entity_io_pipe_read as function with args entity id, buffer, count, offset
$define %func entity_io_pipe_write as function with args entity id, buffer, count, offset
$define %func entity_io_pipe_seek as function with args entity id, offset, whence, result
$define %func entity_io_pipe_ioctl as function with args entity id, command, argument
$define %func entity_io_pipe_stat as function with args entity id, size
$define %func entity_io_nb_read as function with args entity id, buffer, count, offset
$define %func entity_io_nb_write as function with args entity id, buffer, count, offset
$define %func entity_io_nb_ioctl as function with args entity id, command, argument
$define %func entity_io_nb_stat as function with args entity id, size
$define %func entity_io_read as function with args entity id, buffer, count
$define %func entity_io_write as function with args entity id, buffer, count
$define %func entity_io_seek as function with args entity id, offset, whence, result
$define %func entity_io_ioctl as function with args entity id, command, argument

*/

/* !SPACE!

$space %internal entity_io_release_pipe, entity_io_release_file
$space %internal entity_io_release_net, entity_io_release_ipc
$space %internal entity_io_release_reg
$space %internal entity_io_vnode_read, entity_io_vnode_write
$space %internal entity_io_vnode_seek, entity_io_vnode_ioctl
$space %internal entity_io_vnode_stat
$space %internal entity_io_pipe_read, entity_io_pipe_write
$space %internal entity_io_pipe_seek, entity_io_pipe_ioctl
$space %internal entity_io_pipe_stat
$space %internal entity_io_nb_read, entity_io_nb_write
$space %internal entity_io_nb_ioctl, entity_io_nb_stat
$space %export entity_io_init, entity_io_create_raw, entity_io_attach
$space %export entity_io_open_id, entity_io_ptr, entity_io_set_ptr
$space %export entity_io_i32, entity_io_set_i32
$space %export entity_io_read, entity_io_write, entity_io_seek
$space %export entity_io_ioctl

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/api/shm.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/entity/entity.h>
#include <kernel/event/event.h>
#include <kernel/ipc/ipc.h>
#include <kernel/net/endpoint.h>
#include <kernel/process.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>

void	*entity_io_ptr(entity_id_t id, u32 index);
int	entity_io_set_ptr(entity_id_t id, u32 index, void *ptr);

static void
entity_io_release_pipe(entity_id_t id)
{
	pipe_t	*p;

	p = (pipe_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (!p) {
		return;
	}
	if (p->readers > 0) {
		p->readers--;
	}
	if (p->writers > 0) {
		p->writers--;
	}
	if (p->readers == 0 && p->writers == 0) {
		kmem_free(p);
	}
}

static void
entity_io_release_file(entity_id_t id)
{
	vnode_t	*vn;
	void	*path;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	path = entity_io_ptr(id, ENTITY_IO_PTR_PATH);
	if (vn) {
		vnode_release(vn);
	}
	if (path) {
		kmem_free(path);
	}
}

static void
entity_io_release_net(entity_id_t id)
{
	net_endpoint_t	*ep;

	ep = (net_endpoint_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (ep) {
		net_endpoint_close(ep);
	}
}

static void
entity_io_release_ipc(entity_id_t id)
{
	ipc_endpoint_t	*endpoint;

	endpoint = (ipc_endpoint_t *)entity_io_ptr(id,
	    ENTITY_IO_PTR_BACKING);
	if (endpoint) {
		ipc_endpoint_release(endpoint);
	}
}

static void
entity_io_release_reg(entity_id_t id)
{
	void	*buf;

	buf = entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (buf) {
		kmem_free(buf);
	}
}

static int
entity_io_vnode_read(entity_id_t id, void *buf, u64 count, u64 offset)
{
	vnode_t	*vn;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (vnode_read(vn, buf, count, offset));
}

static int
entity_io_vnode_write(entity_id_t id, const void *buf, u64 count,
    u64 offset)
{
	posix_stat_t	st;
	vnode_t		*vn;
	s32		flags;
	int		ret;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = entity_io_i32(id, ENTITY_IO_I32_FLAGS, &flags);
	if (ret != 0) {
		return (ret);
	}
	if (flags & API_OPEN_APPEND) {
		if (vnode_stat(vn, &st) == 0) {
			offset = (u64)st.st_size;
		}
	}
	return (vnode_write(vn, buf, count, offset));
}

static int
entity_io_vnode_seek(entity_id_t id, s64 offset, int whence, s64 *result)
{
	posix_stat_t	st;
	vnode_t		*vn;
	u64		cur;
	s64		new_off;
	int		ret;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (entity_arch(id) == ENTITY_ARCH_VNODE &&
	    (vn->name[0] == '\0' || strcmp(vn->name, "fb0") != 0)) {
		return (-API_ERR_NOT_SEEKABLE);
	}
	ret = entity_get_data(id, ENTITY_IO_DATA_OFFSET, &cur);
	if (ret != 0) {
		return (ret);
	}
	new_off = 0;
	switch (whence) {
	case API_SEEK_SET:
		new_off = offset;
		break;
	case API_SEEK_CUR:
		new_off = (s64)cur + offset;
		break;
	case API_SEEK_END:
		if (vnode_stat(vn, &st) != 0) {
			return (-API_ERR_NOT_SUPPORTED);
		}
		new_off = (s64)st.st_size + offset;
		break;
	default:
		return (-API_ERR_BAD_VALUE);
	}
	if (new_off < 0) {
		return (-API_ERR_BAD_VALUE);
	}
	*result = new_off;
	return (0);
}

static int
entity_io_vnode_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	vnode_t	*vn;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (vn == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (vnode_ioctl(vn, cmd, arg));
}

static int
entity_io_vnode_stat(entity_id_t id, u64 *size)
{
	posix_stat_t	st;
	vnode_t		*vn;

	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (vn == NULL || size == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (vnode_stat(vn, &st) != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	*size = (u64)st.st_size;
	return (0);
}

static int
entity_io_pipe_read(entity_id_t id, void *buf, u64 count, u64 offset)
{
	pipe_t	*p;

	(void)offset;
	p = (pipe_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (p == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (pipe_read(p, buf, (u32)count));
}

static int
entity_io_pipe_write(entity_id_t id, const void *buf, u64 count,
    u64 offset)
{
	pipe_t	*p;

	(void)offset;
	p = (pipe_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (p == NULL) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (pipe_write(p, buf, (u32)count));
}

static int
entity_io_pipe_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	(void)id;
	(void)cmd;
	(void)arg;
	return (-API_ERR_NOT_SUPPORTED);
}

static int
entity_io_pipe_seek(entity_id_t id, s64 offset, int whence, s64 *result)
{
	(void)id;
	(void)offset;
	(void)whence;
	(void)result;
	return (-API_ERR_NOT_SEEKABLE);
}

static int
entity_io_pipe_stat(entity_id_t id, u64 *size)
{
	(void)id;
	if (size == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	*size = 0;
	return (0);
}

static int
entity_io_nb_read(entity_id_t id, void *buf, u64 count, u64 offset)
{
	return (newbus_interface_read_entity(id, buf, count, offset));
}

static int
entity_io_nb_write(entity_id_t id, const void *buf, u64 count, u64 offset)
{
	return (newbus_interface_write_entity(id, buf, count, offset));
}

static int
entity_io_nb_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	return (newbus_interface_ioctl_entity(id, cmd, arg));
}

static int
entity_io_nb_stat(entity_id_t id, u64 *size)
{
	return (newbus_interface_stat_entity(id, size));
}

static const entity_io_ops_t entity_io_file_ops = {
	.read	= entity_io_vnode_read,
	.write	= entity_io_vnode_write,
	.seek	= entity_io_vnode_seek,
	.ioctl	= entity_io_vnode_ioctl,
	.stat	= entity_io_vnode_stat,
};

static const entity_io_ops_t entity_io_vnode_ops = {
	.read	= entity_io_vnode_read,
	.write	= entity_io_vnode_write,
	.seek	= entity_io_vnode_seek,
	.ioctl	= entity_io_vnode_ioctl,
	.stat	= entity_io_vnode_stat,
};

static const entity_io_ops_t entity_io_pipe_ops = {
	.read	= entity_io_pipe_read,
	.write	= entity_io_pipe_write,
	.seek	= entity_io_pipe_seek,
	.ioctl	= entity_io_pipe_ioctl,
	.stat	= entity_io_pipe_stat,
};

static const entity_io_ops_t entity_io_nb_ops = {
	.read	= entity_io_nb_read,
	.write	= entity_io_nb_write,
	.seek	= NULL,
	.ioctl	= entity_io_nb_ioctl,
	.stat	= entity_io_nb_stat,
};

void
entity_io_init(void)
{
	entity_arch_release_register(ENTITY_ARCH_FILE,
	    entity_io_release_file);
	entity_arch_release_register(ENTITY_ARCH_VNODE,
	    entity_io_release_file);
	entity_arch_release_register(ENTITY_ARCH_PIPE,
	    entity_io_release_pipe);
	entity_arch_release_register(ENTITY_ARCH_NET,
	    entity_io_release_net);
	entity_arch_release_register(ENTITY_ARCH_IPC,
	    entity_io_release_ipc);
	entity_arch_release_register(ENTITY_ARCH_REG,
	    entity_io_release_reg);
	entity_arch_release_register(ENTITY_ARCH_KQUEUE,
	    kqueue_entity_release);
	entity_arch_release_register(ENTITY_ARCH_TRACE,
	    api_trace_entity_release);
	shm_init();
	terminal_entity_register_all();
	entity_arch_io_register(ENTITY_ARCH_FILE, &entity_io_file_ops);
	entity_arch_io_register(ENTITY_ARCH_VNODE, &entity_io_vnode_ops);
	entity_arch_io_register(ENTITY_ARCH_PIPE, &entity_io_pipe_ops);
	entity_arch_io_register(ENTITY_ARCH_NB_INTERFACE,
	    &entity_io_nb_ops);
	newbus_entity_init();
}

entity_id_t
entity_io_create_raw(u16 arch, u32 flags)
{
	process_t	*proc;
	u32		uid, gid, euid, egid;
	int		kusr;

	proc = process_current();
	uid = proc ? proc->uid : 0;
	gid = proc ? proc->gid : 0;
	euid = proc ? proc->euid : 0;
	egid = proc ? proc->egid : 0;
	kusr = proc ? proc->kusr_auth : 0;
	return (entity_create(arch, flags, proc ? proc->pid : 0,
	    uid, gid, euid, egid, kusr));
}

int
entity_io_attach(entity_id_t id, u32 access)
{
	process_t	*proc;
	int		handle;

	if (id == 0 || access == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	proc = process_current();
	handle = entity_handle_alloc(proc, id, access);
	if (handle < 0) {
		return (handle);
	}
	entity_release(id);
	return (handle);
}

int
entity_io_open_id(entity_id_t id, u32 access)
{
	return (entity_handle_alloc(process_current(), id, access));
}

void *
entity_io_ptr(entity_id_t id, u32 index)
{
	u64	value;

	if (entity_get_data(id, index, &value) != 0) {
		return (NULL);
	}
	return ((void *)(u64)value);
}

int
entity_io_set_ptr(entity_id_t id, u32 index, void *ptr)
{
	return (entity_set_data(id, index, (u64)(u64)ptr));
}

int
entity_io_i32(entity_id_t id, u32 index, s32 *value)
{
	return (entity_get_i32(id, index, value));
}

int
entity_io_set_i32(entity_id_t id, u32 index, s32 value)
{
	return (entity_set_i32(id, index, value));
}

int
entity_io_read(entity_id_t id, void *buf, u64 count)
{
	const entity_io_ops_t	*ops;
	u64			offset;
	int			n, ret;

	ops = entity_arch_io_get(entity_arch(id));
	if (ops == NULL || ops->read == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	ret = entity_get_data(id, ENTITY_IO_DATA_OFFSET, &offset);
	if (ret != 0) {
		return (ret);
	}
	n = ops->read(id, buf, count, offset);
	if (n > 0) {
		entity_set_data(id, ENTITY_IO_DATA_OFFSET,
		    offset + (u64)n);
	}
	return (n);
}

int
entity_io_write(entity_id_t id, const void *buf, u64 count)
{
	const entity_io_ops_t	*ops;
	u64			offset;
	int			n, ret;

	ops = entity_arch_io_get(entity_arch(id));
	if (ops == NULL || ops->write == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	ret = entity_get_data(id, ENTITY_IO_DATA_OFFSET, &offset);
	if (ret != 0) {
		return (ret);
	}
	n = ops->write(id, buf, count, offset);
	if (n > 0) {
		entity_set_data(id, ENTITY_IO_DATA_OFFSET,
		    offset + (u64)n);
	}
	return (n);
}

int
entity_io_seek(entity_id_t id, s64 offset, int whence, s64 *new_offset)
{
	const entity_io_ops_t	*ops;
	u64			cur;
	u64			size;
	s64			value;
	int			ret;

	if (new_offset == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	ops = entity_arch_io_get(entity_arch(id));
	if (ops == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	ret = entity_get_data(id, ENTITY_IO_DATA_OFFSET, &cur);
	if (ret != 0) {
		return (ret);
	}
	if (ops->seek != NULL) {
		ret = ops->seek(id, offset, whence, &value);
		if (ret != 0) {
			return (ret);
		}
	} else {
		value = 0;
		switch (whence) {
		case API_SEEK_SET:
			value = offset;
			break;
		case API_SEEK_CUR:
			value = (s64)cur + offset;
			break;
		case API_SEEK_END:
			if (ops->stat == NULL ||
			    ops->stat(id, &size) != 0) {
				return (-API_ERR_NOT_SUPPORTED);
			}
			value = (s64)size + offset;
			break;
		default:
			return (-API_ERR_BAD_VALUE);
		}
	}
	if (value < 0) {
		return (-API_ERR_BAD_VALUE);
	}
	entity_set_data(id, ENTITY_IO_DATA_OFFSET, (u64)value);
	*new_offset = value;
	return (0);
}

int
entity_io_ioctl(entity_id_t id, u64 cmd, void *arg)
{
	const entity_io_ops_t	*ops;

	ops = entity_arch_io_get(entity_arch(id));
	if (ops == NULL || ops->ioctl == NULL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (ops->ioctl(id, cmd, arg));
}
