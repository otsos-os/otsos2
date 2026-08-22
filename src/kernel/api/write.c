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
$define %type int as 32 bit signed
$define %type process as struct with process control block
$define %type entity_id as 64 bit packed archetype/generation/index

$define %func api_init as procedure with args void
$define %func api_init_process as procedure with args process *
$define %func api_copy_handles as procedure with args process *, process *
$define %func api_release_handles as procedure with args process *
$define %func api_term_write as function with args buffer, count
$define %func api_data_write as function with args handle, buffer, count

*/

/* !SPACE!

$space %export api_init, api_init_process, api_copy_handles
$space %export api_release_handles, api_term_write, api_data_write

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/entity/entity.h>
#include <kernel/ipc/ipc.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

void
api_init(void)
{
	printk("[API] Initializing native API (entity-backed)\n");
	entity_io_init();
	ipc_init();
}

void
api_init_process(struct process *proc)
{
	if (!proc) {
		return;
	}
	entity_handle_init_process(proc);
}

void
api_copy_handles(struct process *dst, const struct process *src)
{
	if (!dst || !src) {
		return;
	}
	if (entity_handle_copy_all(dst, src) != 0) {
		printk("[API] entity handle copy failed "
		    "(pid %d -> pid %d)\n", src->pid, dst->pid);
	}
}

void
api_release_handles(struct process *proc)
{
	if (!proc) {
		return;
	}
	entity_handle_release_all(proc);
}

int
api_term_write(const void *buf, u32 count)
{
	process_t	*proc;
	thread_t	*td;
	int		pty_id;

	if (count == 0) {
		return (0);
	}
	if (!is_user_address(buf, count)) {
		td = thread_current();
		if (td && (td->context.cs & 3) == 3) {
			process_exit(-1);
		}
		return (-API_ERR_BAD_ADDR);
	}
	proc = process_current();
	if (proc && proc->controlling_tty < -1) {
		pty_id = -proc->controlling_tty - 2;
		return (pty_slave_write_idx(pty_id, buf, count, 0));
	}
	return (terminal_write(buf, count));
}

int
api_data_write(int handle, const void *buf, u32 count)
{
	return (api_entity_write(handle, buf, count));
}
