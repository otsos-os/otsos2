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
$define %type rw_t as reader writer lock with reader count and writer owner
$define %type thread_t as struct with per-thread CPU context and state

$define %func rw_init as procedure with args rw_t *, const char *, u32
$define %func rw_rlock as procedure with args rw_t *
$define %func rw_runlock as procedure with args rw_t *
$define %func rw_wlock as procedure with args rw_t *
$define %func rw_wunlock as procedure with args rw_t *
$define %func rw_try_upgrade as function with args rw_t *
$define %func rw_downgrade as procedure with args rw_t *
$define %func rw_wowned as function with args rw_t *

*/

/* !SPACE!

$space %export rw_init, rw_rlock, rw_runlock, rw_wlock, rw_wunlock
$space %export rw_try_upgrade, rw_downgrade, rw_wowned

*/

#include <kernel/event/event.h>
#include <kernel/panic.h>
#include <kernel/sync/sync.h>
#include <kernel/thread.h>

void
rw_init(rw_t *rp, const char *name, u32 level)
{
	if (rp == NULL) {
		panic("[SYNC] rw_init on NULL\n");
	}
	rp->readers = 0;
	rp->writer = 0;
	rp->level = level;
	rp->wwaiters = 0;
	rp->rwaiters = 0;
	rp->wowner = NULL;
	rp->name = name;
	rp->contested = 0;
	rp->rchan = 0;
	rp->wchan = 0;
	spin_init(&rp->interlock, name, LO_INTERLOCK);
}

void
rw_rlock(rw_t *rp)
{
	thread_t	*td;

	witness_check_sleep(rp->name ? rp->name : "rw_rlock");
	witness_sleep_lock(rp->name, rp->level);
	td = thread_current();
	for (;;) {
		spin_lock(&rp->interlock);
		if (rp->writer == 0) {
			rp->readers++;
			spin_unlock(&rp->interlock);
			return;
		}
		rp->contested++;
		if (td == NULL) {
			spin_unlock(&rp->interlock);
			__asm__ volatile("pause");
			continue;
		}
		rp->rwaiters++;
		proc_sleep_interlock(&rp->rchan, &rp->interlock);
	}
}

void
rw_runlock(rw_t *rp)
{
	u32	wake;

	spin_lock(&rp->interlock);
	if (rp->readers == 0) {
		spin_unlock(&rp->interlock);
		panic("[SYNC] read release of unheld rwlock %s\n",
		    rp->name ? rp->name : "?");
	}
	rp->readers--;
	wake = 0;
	if (rp->readers == 0 && rp->wwaiters != 0) {
		rp->wwaiters = 0;
		wake = 1;
	}
	spin_unlock(&rp->interlock);
	witness_sleep_unlock(rp->name, rp->level);
	if (wake) {
		proc_wakeup(&rp->wchan);
	}
}

void
rw_wlock(rw_t *rp)
{
	thread_t	*td;

	witness_check_sleep(rp->name ? rp->name : "rw_wlock");
	td = thread_current();
	if (td != NULL && rp->writer != 0 && rp->wowner == td) {
		panic("[SYNC] write recursion on rwlock %s\n",
		    rp->name ? rp->name : "?");
	}
	witness_sleep_lock(rp->name, rp->level);
	for (;;) {
		spin_lock(&rp->interlock);
		if (rp->writer == 0 && rp->readers == 0) {
			rp->writer = 1;
			rp->wowner = td;
			spin_unlock(&rp->interlock);
			return;
		}
		rp->contested++;
		if (td == NULL) {
			spin_unlock(&rp->interlock);
			__asm__ volatile("pause");
			continue;
		}
		rp->wwaiters++;
		proc_sleep_interlock(&rp->wchan, &rp->interlock);
	}
}

void
rw_wunlock(rw_t *rp)
{
	u32	wake_w;
	u32	wake_r;

	spin_lock(&rp->interlock);
	if (rp->writer == 0) {
		spin_unlock(&rp->interlock);
		panic("[SYNC] write release of unheld rwlock %s\n",
		    rp->name ? rp->name : "?");
	}
	if (rp->wowner != thread_current()) {
		spin_unlock(&rp->interlock);
		panic("[SYNC] write release of rwlock %s owned elsewhere\n",
		    rp->name ? rp->name : "?");
	}
	rp->writer = 0;
	rp->wowner = NULL;
	wake_w = rp->wwaiters;
	wake_r = rp->rwaiters;
	rp->wwaiters = 0;
	rp->rwaiters = 0;
	spin_unlock(&rp->interlock);
	witness_sleep_unlock(rp->name, rp->level);
	if (wake_r != 0) {
		proc_wakeup(&rp->rchan);
	}
	if (wake_w != 0) {
		proc_wakeup(&rp->wchan);
	}
}

int
rw_try_upgrade(rw_t *rp)
{
	spin_lock(&rp->interlock);
	if (rp->writer != 0 || rp->readers != 1) {
		spin_unlock(&rp->interlock);
		return (0);
	}
	rp->readers = 0;
	rp->writer = 1;
	rp->wowner = thread_current();
	spin_unlock(&rp->interlock);
	return (1);
}

void
rw_downgrade(rw_t *rp)
{
	u32	wake_r;

	spin_lock(&rp->interlock);
	if (rp->writer == 0 || rp->wowner != thread_current()) {
		spin_unlock(&rp->interlock);
		panic("[SYNC] downgrade of rwlock %s not write held\n",
		    rp->name ? rp->name : "?");
	}
	rp->writer = 0;
	rp->wowner = NULL;
	rp->readers = 1;
	wake_r = rp->rwaiters;
	rp->rwaiters = 0;
	spin_unlock(&rp->interlock);
	if (wake_r != 0) {
		proc_wakeup(&rp->rchan);
	}
}

int
rw_wowned(rw_t *rp)
{
	if (rp == NULL || rp->writer == 0) {
		return (0);
	}
	return (rp->wowner == thread_current());
}
