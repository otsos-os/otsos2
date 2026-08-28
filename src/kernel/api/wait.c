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

$define %type int as 32 bit signed
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type process_t as struct with process state
$define %type entity_id_t as 64 bit generation tagged object id

$define %func proc_wait_reclaim_tty as procedure with args process_t *
$define %func proc_wait_take as function with args entity id, int *
$define %func api_proc_wait as function with args int *
$define %func api_proc_trywait as function with args int *
$define %func api_proc_open as function with args u32
$define %func api_proc_close as function with args int
$define %func api_proc_exitcode as function with args int, int *
$define %func api_proc_waith as function with args int, int *
$define %func api_proc_notify as function with args int, int
$define %func api_proc_upcall as function with args u64, int

*/

/* !SPACE!

$space %internal proc_wait_reclaim_tty, proc_wait_take
$space %export api_proc_wait, api_proc_trywait
$space %export api_proc_open, api_proc_close, api_proc_exitcode
$space %export api_proc_waith, api_proc_notify, api_proc_upcall

*/

#include <kernel/console/pty.h>
#include <kernel/console/terminal.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>

#define	PROC_WAIT_PASSES	4096

static void
proc_wait_reclaim_tty(process_t *current)
{
	int	pty_num;

	if (current->controlling_tty >= 0) {
		terminal_set_pgrp(current->controlling_tty, current->pgid);
	} else if (current->controlling_tty < -1) {
		pty_num = -current->controlling_tty - 2;
		pty_set_session_pgrp(pty_num, current->sid, current->pgid);
	}
}

static int
proc_wait_take(entity_id_t id, int *status)
{
	u32	pid, ppid;
	int	code, flags;

	if (process_record_read(id, &code, &flags, &pid, &ppid) != 0) {
		return (-API_ERR_NO_CHILD);
	}
	if (status && is_user_address(status, sizeof(int))) {
		*status = code;
	}
	if (process_record_consume(id) != 0) {
		return (-API_ERR_NO_CHILD);
	}
	return ((int)pid);
}

int
api_proc_wait(int *status)
{
	process_t	*current;
	entity_id_t	id;
	int		pass, ret;

	current = process_current();
	if (!current || current->entity == 0) {
		return (-API_ERR_NO_CHILD);
	}

	for (pass = 0; pass < PROC_WAIT_PASSES; pass++) {
		id = process_record_find_child(current->entity, 0);
		if (id != 0) {
			ret = proc_wait_take(id, status);
			if (ret >= 0) {
				proc_wait_reclaim_tty(current);
			}
			return (ret);
		}

		if (process_child_count(current->pid) == 0) {
			return (-API_ERR_NO_CHILD);
		}
		proc_sleep((void *)current);
	}

	return (-API_ERR_NO_CHILD);
}

int
api_proc_trywait(int *status)
{
	process_t	*current;
	entity_id_t	id;

	current = process_current();
	if (!current || current->entity == 0) {
		return (-API_ERR_NO_CHILD);
	}

	id = process_record_find_child(current->entity, 0);
	if (id == 0) {
		return (-API_ERR_NO_CHILD);
	}
	return (proc_wait_take(id, status));
}

int
api_proc_open(u32 pid)
{
	process_t	*current;
	entity_id_t	id;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}
	if (pid == 0) {
		return (-API_ERR_BAD_VALUE);
	}

	id = process_entity_of_pid(pid);
	if (id == 0) {
		return (-API_ERR_NO_PROC);
	}
	return (entity_handle_alloc(current, id, ENTITY_ACCESS_READ));
}

int
api_proc_close(int handle)
{
	process_t	*current;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}
	return (entity_handle_free(current, handle));
}

int
api_proc_exitcode(int handle, int *code)
{
	process_t	*current;
	entity_id_t	id;
	u32		access, pid, ppid;
	int		flags, value, ret;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!code || !is_user_address(code, sizeof(int))) {
		return (-API_ERR_BAD_ADDR);
	}

	ret = entity_handle_lookup(current, handle, &id, &access);
	if (ret != 0) {
		return (ret);
	}
	if (entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (process_record_read(id, &value, &flags, &pid, &ppid) != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	(void)pid;
	(void)ppid;
	if ((flags & PROC_EXIT_EXITED) == 0) {
		return (0);
	}
	*code = value;
	(void)process_record_consume(id);
	return (1);
}

int
api_proc_waith(int handle, int *code)
{
	process_t	*current;
	entity_id_t	id;
	u32		access, pid, ppid;
	int		flags, value, ret, pass;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = entity_handle_lookup(current, handle, &id, &access);
	if (ret != 0) {
		return (ret);
	}
	if (entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}

	for (pass = 0; pass < PROC_WAIT_PASSES; pass++) {
		if (process_record_read(id, &value, &flags, &pid,
		    &ppid) != 0) {
			return (-API_ERR_BAD_HANDLE);
		}
		if (flags & PROC_EXIT_EXITED) {
			if (code && is_user_address(code, sizeof(int))) {
				*code = value;
			}
			(void)process_record_consume(id);
			return (0);
		}
		proc_sleep((void *)current);
	}
	return (-API_ERR_TIMED_OUT);
}

int
api_proc_notify(int handle, int mode)
{
	process_t	*current;
	entity_id_t	id;
	u32		access;
	int		ret;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = entity_handle_lookup(current, handle, &id, &access);
	if (ret != 0) {
		return (ret);
	}
	if (entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (process_record_set_notify(id, mode));
}

int
api_proc_upcall(u64 handler, int special)
{
	process_t	*current;

	current = process_current();
	if (!current) {
		return (-API_ERR_BAD_VALUE);
	}
	if (handler != 0 && !is_user_address((const void *)handler, 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	current->exit_upcall = handler;
	current->exit_upcall_special = special ? 1 : 0;
	return (0);
}
