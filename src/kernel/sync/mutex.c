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
 * and/or other materials provided with the distribution.
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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type mtx_t as adaptive mutex with owner thread and sleep queue channel
$define %type thread_t as struct with per-thread CPU context and state

$define %func mtx_init as procedure with args mtx_t *, const char *, u32
$define %func mtx_lock as procedure with args mtx_t *
$define %func mtx_unlock as procedure with args mtx_t *
$define %func mtx_trylock as function with args mtx_t *
$define %func mtx_owned as function with args mtx_t *
$define %func mtx_assert_owned as procedure with args mtx_t *, const char *
$define %func mtx_owner_running as function with args mtx_t *

$const MTX_SPIN_TRIES as adaptive mutex spin budget before sleeping

*/

/* !SPACE!

$space %internal mtx_owner_running
$space %export mtx_init, mtx_lock, mtx_unlock, mtx_trylock, mtx_owned
$space %export mtx_assert_owned

*/

#include <kernel/event/event.h>
#include <kernel/panic.h>
#include <kernel/smp/pcpu.h>
#include <kernel/sync/sync.h>
#include <kernel/thread.h>

static int
mtx_owner_running(mtx_t *mp)
{
	thread_t	*owner;

	owner = __atomic_load_n(&mp->owner, __ATOMIC_RELAXED);
	if (owner == NULL) {
		return (0);
	}
	if (owner->state != PROC_STATE_RUNNING) {
		return (0);
	}
	if (owner->running_cpu < 0) {
		return (0);
	}
	return (owner->running_cpu != (int)pcpu_current()->cpu_index);
}

void
mtx_init(mtx_t *mp, const char *name, u32 level)
{
	if (mp == NULL) {
		panic("[SYNC] mtx_init on NULL\n");
	}
	mp->lock = 0;
	mp->level = level;
	mp->owner = NULL;
	mp->waiters = 0;
	mp->name = name;
	mp->contested = 0;
	mp->slept = 0;
	spin_init(&mp->interlock, name, LO_INTERLOCK);
}

int
mtx_trylock(mtx_t *mp)
{
	if (__atomic_exchange_n(&mp->lock, 1, __ATOMIC_ACQUIRE) != 0) {
		return (0);
	}
	witness_sleep_lock(mp->name, mp->level);
	__atomic_store_n(&mp->owner, thread_current(), __ATOMIC_RELAXED);
	return (1);
}

void
mtx_lock(mtx_t *mp)
{
	thread_t	*td;
	u32		spins;

	td = thread_current();
	if (td != NULL &&
	    __atomic_load_n(&mp->owner, __ATOMIC_RELAXED) == td &&
	    __atomic_load_n(&mp->lock, __ATOMIC_RELAXED) != 0) {
		panic("[SYNC] recursion on mutex %s\n",
		    mp->name ? mp->name : "?");
	}
	witness_check_sleep(mp->name ? mp->name : "mtx_lock");
	witness_sleep_lock(mp->name, mp->level);
	for (;;) {
		if (__atomic_exchange_n(&mp->lock, 1, __ATOMIC_ACQUIRE) == 0) {
			__atomic_store_n(&mp->owner, td, __ATOMIC_RELAXED);
			return;
		}
		mp->contested++;
		spins = 0;
		while (mtx_owner_running(mp) && spins < MTX_SPIN_TRIES) {
			__asm__ volatile("pause");
			spins++;
			if (__atomic_load_n(&mp->lock,
			    __ATOMIC_RELAXED) == 0) {
				break;
			}
		}
		if (__atomic_load_n(&mp->lock, __ATOMIC_RELAXED) == 0) {
			continue;
		}
		if (td == NULL) {
			__asm__ volatile("pause");
			continue;
		}
		spin_lock(&mp->interlock);
		if (__atomic_load_n(&mp->lock, __ATOMIC_RELAXED) == 0) {
			spin_unlock(&mp->interlock);
			continue;
		}
		mp->waiters++;
		mp->slept++;
		proc_sleep_interlock(mp, &mp->interlock);
	}
}

void
mtx_unlock(mtx_t *mp)
{
	thread_t	*td;
	u32		wake;

	td = thread_current();
	if (__atomic_load_n(&mp->lock, __ATOMIC_RELAXED) == 0) {
		panic("[SYNC] release of unheld mutex %s\n",
		    mp->name ? mp->name : "?");
	}
	if (__atomic_load_n(&mp->owner, __ATOMIC_RELAXED) != td) {
		panic("[SYNC] release of mutex %s owned by another thread\n",
		    mp->name ? mp->name : "?");
	}
	witness_sleep_unlock(mp->name, mp->level);
	__atomic_store_n(&mp->owner, NULL, __ATOMIC_RELAXED);
	__atomic_store_n(&mp->lock, 0, __ATOMIC_RELEASE);
	spin_lock(&mp->interlock);
	wake = mp->waiters;
	mp->waiters = 0;
	spin_unlock(&mp->interlock);
	if (wake != 0) {
		proc_wakeup(mp);
	}
}

int
mtx_owned(mtx_t *mp)
{
	if (mp == NULL) {
		return (0);
	}
	if (__atomic_load_n(&mp->lock, __ATOMIC_RELAXED) == 0) {
		return (0);
	}
	return (__atomic_load_n(&mp->owner, __ATOMIC_RELAXED) ==
	    thread_current());
}

void
mtx_assert_owned(mtx_t *mp, const char *who)
{
	if (!mtx_owned(mp)) {
		panic("[SYNC] %s requires mutex %s\n", who ? who : "?",
		    (mp && mp->name) ? mp->name : "?");
	}
}
