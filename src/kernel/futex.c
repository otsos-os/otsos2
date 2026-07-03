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

$define %type u64 as 64 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type futex_entry_t as struct with uaddr, refcount, wait_channel

$define %func futex_wait as function with args u64, u32
$define %func futex_wake as function with args u64, u32
$define %func futex_wake_all as procedure with args u64

*/

/* !SPACE!

$space %internal futex_find, futex_alloc
$space %export futex_wait, futex_wake, futex_wake_all

*/

#include <kernel/event/event.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define MAX_FUTEX_ENTRIES 64

typedef struct {
	int	used;
	u64	uaddr;
	int	refcount;
	int	dummy;		/* unique wait channel per entry */
} futex_entry_t;

static futex_entry_t	futex_table[MAX_FUTEX_ENTRIES];

static futex_entry_t *
futex_find(u64 uaddr)
{
	int		i;

	for (i = 0; i < MAX_FUTEX_ENTRIES; i++) {
		if (futex_table[i].used &&
		    futex_table[i].uaddr == uaddr) {
			return (&futex_table[i]);
		}
	}
	return (NULL);
}

static futex_entry_t *
futex_alloc(u64 uaddr)
{
	int		i;
	futex_entry_t	*fe;

	for (i = 0; i < MAX_FUTEX_ENTRIES; i++) {
		if (!futex_table[i].used) {
			fe = &futex_table[i];
			memset(fe, 0, sizeof(futex_entry_t));
			fe->used = 1;
			fe->uaddr = uaddr;
			fe->refcount = 0;
			return (fe);
		}
	}
	return (NULL);
}

int
futex_wait(u64 uaddr, u32 expected_val)
{
	u32		*ptr;
	futex_entry_t	*fe;

	ptr = (u32 *)uaddr;
	if (!is_user_address(ptr, sizeof(u32))) {
		return (-14);	/* EFAULT */
	}

	if (*ptr != expected_val) {
		return (-11);	/* EAGAIN */
	}

	fe = futex_find(uaddr);
	if (!fe) {
		fe = futex_alloc(uaddr);
		if (!fe) {
			return (-12);	/* ENOMEM */
		}
	}

	fe->refcount++;
	proc_sleep((void *)&fe->dummy);
	fe->refcount--;

	if (fe->refcount <= 0) {
		memset(fe, 0, sizeof(futex_entry_t));
	}

	return (0);
}

int
futex_wake(u64 uaddr, u32 max_waiters)
{
	futex_entry_t	*fe;
	int		woken;

	fe = futex_find(uaddr);
	if (!fe) {
		return (0);
	}

	woken = 0;
	while (woken < (int)max_waiters) {
		if (fe->refcount <= woken) {
			break;
		}
		proc_wakeup_one((void *)&fe->dummy);
		woken++;
	}

	return (woken);
}

void
futex_wake_all(u64 uaddr)
{
	futex_entry_t	*fe;

	fe = futex_find(uaddr);
	if (!fe) {
		return;
	}

	proc_wakeup((void *)&fe->dummy);
}
