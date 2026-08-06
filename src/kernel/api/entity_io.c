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

*/

/* !SPACE!

$space %internal entity_io_release_pipe, entity_io_release_file
$space %internal entity_io_release_net, entity_io_release_ipc
$space %internal entity_io_release_reg
$space %export entity_io_init, entity_io_create_raw, entity_io_attach
$space %export entity_io_open_id, entity_io_ptr, entity_io_set_ptr
$space %export entity_io_i32, entity_io_set_i32

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/fs/vfs/vfs.h>
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
	terminal_entity_register_all();
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
