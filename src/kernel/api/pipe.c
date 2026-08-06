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
$define %type pipe as struct with pipe ring buffer

$define %func pipe_read as function with args pipe *, void *, u32
$define %func pipe_write as function with args pipe *, const void *, u32
$define %func api_data_pipe as function with args int *

*/

/* !SPACE!

$space %export pipe_read, pipe_write, api_data_pipe

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/api/posix/posix.h>
#include <kernel/entity/entity.h>
#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mm/kmem.h>

int
pipe_read(pipe_t *p, void *buf, u32 count)
{
	u32	to_read;
	u32	i;
	u8	*out;

	if (!p || !buf || count == 0) {
		return (0);
	}
	while (p->size == 0) {
		if (p->writers == 0) {
			return (0);
		}
		proc_sleep((void *)p);
	}
	to_read = count;
	if (to_read > p->size) {
		to_read = p->size;
	}
	out = (u8 *)buf;
	for (i = 0; i < to_read; i++) {
		out[i] = p->buffer[p->read_pos];
		p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
	}
	p->size -= to_read;
	event_notify_pipe_change(p);
	proc_wakeup((void *)p);
	posix_poll_notify();
	return ((int)to_read);
}

int
pipe_write(pipe_t *p, const void *buf, u32 count)
{
	u32	space;
	u32	to_write;
	u32	i;
	const u8	*in;

	if (!p || !buf || count == 0) {
		return (0);
	}
	if (p->readers == 0) {
		return (-API_ERR_PIPE_CLOSED);
	}
	while (PIPE_BUF_SIZE - p->size == 0) {
		if (p->readers == 0) {
			return (-API_ERR_PIPE_CLOSED);
		}
		proc_sleep((void *)p);
	}
	space = PIPE_BUF_SIZE - p->size;
	to_write = count;
	if (to_write > space) {
		to_write = space;
	}
	in = (const u8 *)buf;
	for (i = 0; i < to_write; i++) {
		p->buffer[p->write_pos] = in[i];
		p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
	}
	p->size += to_write;
	event_notify_pipe_change(p);
	proc_wakeup((void *)p);
	posix_poll_notify();
	return ((int)to_write);
}

int
api_data_pipe(int handles_out[2])
{
	pipe_t		*p;
	entity_id_t	id;
	int		read_handle, write_handle;

	if (!is_user_address(handles_out, sizeof(int) * 2)) {
		return (-API_ERR_BAD_ADDR);
	}
	id = entity_io_create_raw(ENTITY_ARCH_PIPE, 0);
	if (id == 0) {
		return (-API_ERR_NO_MEMORY);
	}
	p = (pipe_t *)kmem_alloc(sizeof(pipe_t));
	if (!p) {
		entity_destroy(id);
		return (-API_ERR_NO_MEMORY);
	}
	memset(p, 0, sizeof(pipe_t));
	p->readers = 1;
	p->writers = 1;
	entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING, p);

	read_handle = entity_io_attach(id, ENTITY_ACCESS_READ);
	if (read_handle < 0) {
		entity_destroy(id);
		return (read_handle);
	}
	write_handle = entity_io_open_id(id, ENTITY_ACCESS_WRITE);
	if (write_handle < 0) {
		entity_handle_free(process_current(), read_handle);
		return (write_handle);
	}
	handles_out[0] = read_handle;
	handles_out[1] = write_handle;
	return (0);
}
