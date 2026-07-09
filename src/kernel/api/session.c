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

$define %type process_t as process control block

$define %func api_session_setsid as function with args void
$define %func api_session_getsid as function with args void
$define %func api_session_init as procedure with args process_t *
$define %func api_session_fork as procedure with args process_t *, process_t *

*/

/* !SPACE!

$space %export api_session_setsid, api_session_getsid
$space %export api_session_init, api_session_fork

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/api/session.h>
#include <kernel/console/terminal.h>
#include <kernel/process.h>
#include <mlibc/mlibc.h>

int
api_session_setsid(void)
{
	process_t	*proc;

	proc = process_current();
	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}
	if (proc->is_session_leader) {
		return (-API_ERR_PERM);
	}
	proc->sid = proc->pid;
	proc->pgid = proc->pid;
	proc->is_session_leader = 1;
	if (proc->controlling_tty >= 0) {
		terminal_hangup(proc->controlling_tty);
		proc->controlling_tty = -1;
	}

	return ((int)proc->sid);
}

int
api_session_getsid(void)
{
	process_t	*proc;

	proc = process_current();
	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}

	return ((int)proc->sid);
}

void
api_session_init(process_t *proc)
{
	if (!proc) {
		return;
	}

	proc->sid = 0;
	proc->pgid = 0;
	proc->is_session_leader = 0;
	proc->controlling_tty = -1;
}

void
api_session_fork(process_t *parent, process_t *child)
{
	if (!parent || !child) {
		return;
	}
	child->sid = parent->sid;
	child->pgid = parent->pgid;
	child->is_session_leader = 0;
	child->controlling_tty = parent->controlling_tty;
}
