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
$define %type spin_t as spin mutex with owner cpu, recursion and lock order level
$define %type pcpu_t as per CPU data with identity, current thread, nesting counters and witness stack

$define %func spin_init as procedure with args spin_t *, const char *, u32
$define %func spin_lock as procedure with args spin_t *
$define %func spin_unlock as procedure with args spin_t *
$define %func spin_trylock as function with args spin_t *
$define %func spin_owned as function with args spin_t *
$define %func spin_assert_owned as procedure with args spin_t *, const char *
$define %func spin_assert_unowned as procedure with args spin_t *, const char *
$define %func spin_enter as function with args void
$define %func spin_exit as procedure with args void
$define %func spin_unlock_nocli as function with args spin_t *
$define %func spin_flags_restore as procedure with args u64
$define %func spin_release as procedure with args spin_t *

$const SPIN_DEADLOCK_SPINS as spin budget before the lock is declared deadlocked
$const IRQ_VECTOR_YIELD as software interrupt vector that drives a voluntary switch

*/

/* !SPACE!

$space %internal spin_enter, spin_exit, spin_release
$space %export spin_init, spin_lock, spin_unlock, spin_trylock, spin_owned
$space %export spin_assert_owned, spin_assert_unowned
$space %export spin_unlock_nocli, spin_flags_restore
$space %export spin_deadlock_spins

*/

#include <kernel/interrupts/irq.h>
#include <kernel/panic.h>
#include <kernel/smp/pcpu.h>
#include <kernel/sync/sync.h>

#define	SPIN_DEADLOCK_SPINS	200000000UL

u64	spin_deadlock_spins = SPIN_DEADLOCK_SPINS;

static pcpu_t *
spin_enter(void)
{
	pcpu_t	*pc;
	u64	flags;

	__asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
	pc = pcpu_current();
	if (pc->spin_held >= PCPU_SPIN_DEPTH) {
		panic("[SYNC] spin nesting past %d on cpu %u\n",
		    PCPU_SPIN_DEPTH, pc->cpu_index);
	}
	pc->spin_flags[pc->spin_held] = flags;
	pc->spin_held++;
	return (pc);
}

static void
spin_exit(void)
{
	pcpu_t	*pc;
	u64	flags;
	u32	preempt;

	pc = pcpu_current();
	if (pc->spin_held == 0) {
		panic("[SYNC] spin_exit with no spin mutex held on cpu %u\n",
		    pc->cpu_index);
	}
	pc->spin_held--;
	flags = pc->spin_flags[pc->spin_held];
	preempt = 0;
	if (pc->spin_held == 0 && pc->critnest == 0 &&
	    pc->preempt_pending != 0 && (flags & 0x200ULL) != 0) {
		pc->preempt_pending = 0;
		preempt = 1;
	}
	__asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
	if (preempt) {
		__asm__ volatile("int %0" :: "i"(IRQ_VECTOR_YIELD) : "memory");
	}
}

void
spin_init(spin_t *sp, const char *name, u32 level)
{
	if (sp == NULL) {
		panic("[SYNC] spin_init on NULL\n");
	}
	sp->lock = 0;
	sp->owner = SPIN_UNOWNED;
	sp->recursion = 0;
	sp->level = level;
	sp->name = name;
	sp->contested = 0;
}

void
spin_lock(spin_t *sp)
{
	pcpu_t	*pc;
	u64	spins;

	pc = spin_enter();
	if (sp->owner == pc->cpu_index &&
	    __atomic_load_n(&sp->lock, __ATOMIC_RELAXED) != 0) {
		panic("[SYNC] recursion on spin mutex %s from cpu %u\n",
		    sp->name ? sp->name : "?", pc->cpu_index);
	}
	witness_lock(sp->name, sp->level);
	spins = 0;
	while (__atomic_exchange_n(&sp->lock, 1, __ATOMIC_ACQUIRE) != 0) {
		sp->contested++;
		do {
			__asm__ volatile("pause");
			spins++;
			if (spin_deadlock_spins != 0 &&
			    spins > spin_deadlock_spins) {
				witness_dump();
				panic("[SYNC] cpu %u stuck on spin mutex %s "
				    "held by cpu %u\n", pc->cpu_index,
				    sp->name ? sp->name : "?", sp->owner);
			}
		} while (__atomic_load_n(&sp->lock, __ATOMIC_RELAXED) != 0);
	}
	sp->owner = pc->cpu_index;
	sp->recursion = 0;
}

int
spin_trylock(spin_t *sp)
{
	pcpu_t	*pc;

	pc = spin_enter();
	if (__atomic_exchange_n(&sp->lock, 1, __ATOMIC_ACQUIRE) != 0) {
		spin_exit();
		return (0);
	}
	witness_lock(sp->name, sp->level);
	sp->owner = pc->cpu_index;
	sp->recursion = 0;
	return (1);
}

static void
spin_release(spin_t *sp)
{
	pcpu_t	*pc;

	pc = pcpu_current();
	if (__atomic_load_n(&sp->lock, __ATOMIC_RELAXED) == 0 ||
	    sp->owner != pc->cpu_index) {
		panic("[SYNC] release of spin mutex %s not held by cpu %u "
		    "(owner %u)\n", sp->name ? sp->name : "?", pc->cpu_index,
		    sp->owner);
	}
	witness_unlock(sp->name, sp->level);
	sp->owner = SPIN_UNOWNED;
	__atomic_store_n(&sp->lock, 0, __ATOMIC_RELEASE);
}

void
spin_unlock(spin_t *sp)
{
	spin_release(sp);
	spin_exit();
}

u64
spin_unlock_nocli(spin_t *sp)
{
	pcpu_t	*pc;

	spin_release(sp);
	pc = pcpu_current();
	if (pc->spin_held == 0) {
		panic("[SYNC] spin_unlock_nocli with no spin mutex held "
		    "on cpu %u\n", pc->cpu_index);
	}
	pc->spin_held--;
	return (pc->spin_flags[pc->spin_held]);
}

void
spin_flags_restore(u64 flags)
{
	pcpu_t	*pc;
	u32	preempt;

	pc = pcpu_current();
	preempt = 0;
	if (pc->spin_held == 0 && pc->critnest == 0 &&
	    pc->preempt_pending != 0 && (flags & 0x200ULL) != 0) {
		pc->preempt_pending = 0;
		preempt = 1;
	}
	__asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
	if (preempt) {
		__asm__ volatile("int %0" :: "i"(IRQ_VECTOR_YIELD) : "memory");
	}
}

int
spin_owned(spin_t *sp)
{
	if (sp == NULL) {
		return (0);
	}
	if (__atomic_load_n(&sp->lock, __ATOMIC_RELAXED) == 0) {
		return (0);
	}
	return (sp->owner == pcpu_current()->cpu_index);
}

void
spin_assert_owned(spin_t *sp, const char *who)
{
	if (!spin_owned(sp)) {
		panic("[SYNC] %s requires spin mutex %s\n",
		    who ? who : "?", (sp && sp->name) ? sp->name : "?");
	}
}

void
spin_assert_unowned(spin_t *sp, const char *who)
{
	if (spin_owned(sp)) {
		panic("[SYNC] %s must not hold spin mutex %s\n",
		    who ? who : "?", (sp && sp->name) ? sp->name : "?");
	}
}
