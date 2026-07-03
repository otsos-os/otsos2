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

#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

extern int	futex_wait(u64 uaddr, u32 expected_val);
extern int	futex_wake(u64 uaddr, u32 max_waiters);

int
api_proc_gettid(void)
{
	thread_t	*td;

	td = thread_current();
	if (!td) {
		return (0);
	}
	return ((int)td->tid);
}

void
api_thread_exit(int code)
{
	thread_exit(code);
}

int
api_thread_join(u32 tid, int *status)
{
	if (status && !is_user_address(status, sizeof(int))) {
		return (-API_ERR_BAD_ADDR);
	}

	return (thread_join(tid, status));
}

void
api_proc_exit_group(int code)
{
	process_t	*proc;

	proc = process_current();
	if (!proc) {
		return;
	}

	process_exit(code);
}

int
api_proc_set_tid_address(u64 tidptr)
{
	thread_t	*td;

	td = thread_current();
	if (!td) {
		return (-API_ERR_BAD_VALUE);
	}

	td->tid_address = tidptr;

	if (tidptr && is_user_address((void *)tidptr, sizeof(u32))) {
		*(u32 *)tidptr = td->tid;
	}

	return ((int)td->tid);
}

int
api_futex_wait(u64 uaddr, u32 expected_val)
{
	if (!is_user_address((void *)uaddr, sizeof(u32))) {
		return (-API_ERR_BAD_ADDR);
	}

	return (futex_wait(uaddr, expected_val));
}

int
api_futex_wake(u64 uaddr, u32 max_waiters)
{
	if (!is_user_address((void *)uaddr, sizeof(u32))) {
		return (-API_ERR_BAD_ADDR);
	}

	return (futex_wake(uaddr, max_waiters));
}
