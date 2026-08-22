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
$define %type process as struct with process control block
$define %type entity_id as 64 bit packed archetype/generation/index

$define %func api_term_read as function with args buffer, count, flags
$define %func api_data_read as function with args handle, buffer, count

*/

/* !SPACE!

$space %export api_term_read, api_data_read

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int
api_term_read(void *buf, u32 count, u32 flags)
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
		return (pty_slave_read_idx(pty_id, buf, count, 0));
	}
	return (terminal_read(buf, count, flags));
}

int
api_data_read(int handle, void *buf, u32 count)
{
	return (api_entity_read(handle, buf, count));
}
