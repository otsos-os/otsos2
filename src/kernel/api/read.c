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
$define %type pipe as struct with pipe ring buffer
$define %type vnode as VFS vnode

$define %func api_term_read as function with args buffer, count, flags
$define %func api_data_read as function with args handle, buffer, count

*/

/* !SPACE!

$space %export api_term_read, api_data_read

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/console/terminal.h>
#include <kernel/entity/entity.h>
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
		thread_t	*td;

		td = thread_current();
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
	process_t	*proc;
	entity_id_t	id;
	vnode_t		*vn;
	pipe_t		*p;
	u32		access;
	u16		arch;
	s32		offset;
	int		n, ret;

	proc = process_current();
	ret = entity_handle_lookup(proc, handle, &id, &access);
	if (ret != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	if ((access & ENTITY_ACCESS_READ) == 0) {
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
	if (!user_range_fault_in(buf, count, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	arch = entity_arch(id);
	if (arch == ENTITY_ARCH_PIPE) {
		p = (pipe_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
		if (!p) {
			return (-API_ERR_BAD_HANDLE);
		}
		return (pipe_read(p, buf, count));
	}
	if (arch != ENTITY_ARCH_FILE && arch != ENTITY_ARCH_VNODE) {
		return (-API_ERR_BAD_HANDLE);
	}
	vn = (vnode_t *)entity_io_ptr(id, ENTITY_IO_PTR_BACKING);
	if (!vn) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = entity_io_i32(id, ENTITY_IO_I32_OFFSET, &offset);
	if (ret != 0) {
		return (ret);
	}
	n = vnode_read(vn, buf, count, (u64)offset);
	if (n < 0) {
		return (n);
	}
	offset += (s32)n;
	entity_io_set_i32(id, ENTITY_IO_I32_OFFSET, offset);
	return (n);
}
