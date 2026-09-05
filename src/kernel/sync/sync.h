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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type spin_t as spin mutex with owner cpu, recursion and lock order level
$define %type mtx_t as adaptive mutex with owner thread and sleep queue channel
$define %type rw_t as reader writer lock with reader count and writer owner
$define %type thread_t as struct with per-thread CPU context and state
$define %type pcpu_t as per CPU data with identity, current thread, nesting counters and witness stack

$define %func spin_init as procedure with args spin_t *, const char *, u32
$define %func spin_lock as procedure with args spin_t *
$define %func spin_unlock as procedure with args spin_t *
$define %func spin_trylock as function with args spin_t *
$define %func spin_owned as function with args spin_t *
$define %func spin_assert_owned as procedure with args spin_t *, const char *
$define %func spin_assert_unowned as procedure with args spin_t *, const char *
$define %func spin_unlock_nocli as function with args spin_t *
$define %func spin_flags_restore as procedure with args u64
$define %func mtx_init as procedure with args mtx_t *, const char *, u32
$define %func mtx_lock as procedure with args mtx_t *
$define %func mtx_unlock as procedure with args mtx_t *
$define %func mtx_trylock as function with args mtx_t *
$define %func mtx_owned as function with args mtx_t *
$define %func mtx_assert_owned as procedure with args mtx_t *, const char *
$define %func rw_init as procedure with args rw_t *, const char *, u32
$define %func rw_rlock as procedure with args rw_t *
$define %func rw_runlock as procedure with args rw_t *
$define %func rw_wlock as procedure with args rw_t *
$define %func rw_wunlock as procedure with args rw_t *
$define %func rw_try_upgrade as function with args rw_t *
$define %func rw_downgrade as procedure with args rw_t *
$define %func rw_wowned as function with args rw_t *
$define %func critical_enter as procedure with args void
$define %func critical_exit as procedure with args void
$define %func critical_nest as function with args void
$define %func critical_preempt_pending as function with args void
$define %func critical_defer_preempt as procedure with args void
$define %func witness_init as procedure with args void
$define %func witness_enable as procedure with args int
$define %func witness_enabled as function with args void
$define %func witness_lock as procedure with args const char *, u32
$define %func witness_unlock as procedure with args const char *, u32
$define %func witness_check_sleep as procedure with args const char *
$define %func witness_held_count as function with args void
$define %func witness_dump as procedure with args void
$define %func witness_sleep_lock as procedure with args const char *, u32
$define %func witness_sleep_unlock as procedure with args const char *, u32
$define %func sync_init as procedure with args void
$define %func sync_configure as procedure with args void

$const LO_APC as lock order level of the APC pool lock
$const LO_SCHED as lock order level of the scheduler lock
$const LO_PROC as lock order level of the process table lock
$const LO_EVENT as lock order level of the kqueue lock
$const LO_THREAD as lock order level of the thread table and thread state lock
$const LO_HANDLE as lock order level of the entity handle table lock
$const LO_ENTITY as lock order level of the entity store lock
$const LO_ENTITY_NS as lock order level of the entity namespace lock
$const LO_VM_MAP as lock order level of an address space lock
$const LO_DMA as lock order level of the DMA tag registry and bounce pool
$const LO_UMA as lock order level of the UMA zone registry lock
$const LO_UMA_ZONE as lock order level of a single UMA zone lock
$const LO_KMEM as lock order level of the kernel heap lock
$const LO_VM_PAGE as lock order level of the physical page allocator lock
$const LO_BOOTMEM as lock order level of the boot memory lock
$const LO_INTERLOCK as lock order level of a sleep queue interlock
$const SPIN_UNOWNED as owner value stored in an unheld spin mutex
$const SPIN_INITIALIZER as static initializer for a spin mutex with name and level
$const MTX_SPIN_TRIES as adaptive mutex spin budget before sleeping

*/

/* !SPACE!

$space %export spin_init, spin_lock, spin_unlock, spin_trylock, spin_owned
$space %export spin_assert_owned, spin_assert_unowned
$space %export spin_unlock_nocli, spin_flags_restore
$space %export mtx_init, mtx_lock, mtx_unlock, mtx_trylock, mtx_owned
$space %export mtx_assert_owned
$space %export rw_init, rw_rlock, rw_runlock, rw_wlock, rw_wunlock
$space %export rw_try_upgrade, rw_downgrade, rw_wowned
$space %export critical_enter, critical_exit, critical_nest
$space %export critical_preempt_pending, critical_defer_preempt
$space %export witness_init, witness_enable, witness_enabled
$space %export witness_lock, witness_unlock, witness_check_sleep
$space %export witness_held_count, witness_dump
$space %export witness_sleep_lock, witness_sleep_unlock
$space %export sync_init, sync_configure

*/

#ifndef KERNEL_SYNC_SYNC_H
#define KERNEL_SYNC_SYNC_H

#include <kernel/smp/pcpu.h>
#include <mlibc/mlibc.h>

#define	LO_SCHED	32
#define	LO_APC		36
#define	LO_PROC		40
#define	LO_EVENT	48
#define	LO_THREAD	56
#define	LO_HANDLE	60
#define	LO_ENTITY	64
#define	LO_ENTITY_NS	72
#define	LO_VM_MAP	76
#define	LO_DMA		78
#define	LO_KMEM		80
#define	LO_UMA		84
#define	LO_UMA_ZONE	88
#define	LO_VM_PAGE	96
#define	LO_BOOTMEM	104
#define	LO_INTERLOCK	4088
#define	SPIN_UNOWNED	0xFFFFFFFFU
#define	MTX_SPIN_TRIES	2048
#define	SPIN_INITIALIZER(nm, lvl)	\
	{ 0, SPIN_UNOWNED, 0, (lvl), (nm), 0 }

typedef struct spin {
	volatile u32	lock;
	u32		owner;
	u32		recursion;
	u32		level;
	const char	*name;
	u64		contested;
} spin_t;

typedef struct mtx {
	volatile u32	lock;
	u32		level;
	struct thread	*owner;
	u32		waiters;
	const char	*name;
	u64		contested;
	u64		slept;
	spin_t		interlock;
} mtx_t;

typedef struct rw {
	volatile u32	readers;
	volatile u32	writer;
	u32		level;
	u32		wwaiters;
	u32		rwaiters;
	struct thread	*wowner;
	const char	*name;
	u64		contested;
	spin_t		interlock;
	u32		rchan;
	u32		wchan;
} rw_t;

void	sync_init(void);
void	sync_configure(void);
void	spin_init(spin_t *sp, const char *name, u32 level);
void	spin_lock(spin_t *sp);
void	spin_unlock(spin_t *sp);
int	spin_trylock(spin_t *sp);
int	spin_owned(spin_t *sp);
void	spin_assert_owned(spin_t *sp, const char *who);
void	spin_assert_unowned(spin_t *sp, const char *who);
u64	spin_unlock_nocli(spin_t *sp);
void	spin_flags_restore(u64 flags);
void	mtx_init(mtx_t *mp, const char *name, u32 level);
void	mtx_lock(mtx_t *mp);
void	mtx_unlock(mtx_t *mp);
int	mtx_trylock(mtx_t *mp);
int	mtx_owned(mtx_t *mp);
void	mtx_assert_owned(mtx_t *mp, const char *who);
void	rw_init(rw_t *rp, const char *name, u32 level);
void	rw_rlock(rw_t *rp);
void	rw_runlock(rw_t *rp);
void	rw_wlock(rw_t *rp);
void	rw_wunlock(rw_t *rp);
int	rw_try_upgrade(rw_t *rp);
void	rw_downgrade(rw_t *rp);
int	rw_wowned(rw_t *rp);
void	critical_enter(void);
void	critical_exit(void);
u32	critical_nest(void);
int	critical_preempt_pending(void);
void	critical_defer_preempt(void);
void	witness_init(void);
void	witness_enable(int on);
int	witness_enabled(void);
void	witness_lock(const char *name, u32 level);
void	witness_unlock(const char *name, u32 level);
void	witness_check_sleep(const char *who);
u32	witness_held_count(void);
void	witness_dump(void);
void	witness_sleep_lock(const char *name, u32 level);
void	witness_sleep_unlock(const char *name, u32 level);

#endif
