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
$define %type pcpu_t as per CPU data with identity, current thread, nesting counters and witness stack

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

*/

/* !SPACE!

$space %export witness_init, witness_enable, witness_enabled
$space %export witness_lock, witness_unlock, witness_check_sleep
$space %export witness_held_count, witness_dump
$space %export witness_sleep_lock, witness_sleep_unlock

*/

#include <kernel/panic.h>
#include <kernel/smp/pcpu.h>
#include <kernel/sync/sync.h>
#include <kernel/thread.h>
#include <mlibc/stdio.h>

static int	witness_on;
static u64	witness_violations;

void
witness_init(void)
{
	witness_on = 0;
	witness_violations = 0;
}

void
witness_enable(int on)
{
	witness_on = on ? 1 : 0;
}

int
witness_enabled(void)
{
	return (witness_on);
}

u32
witness_held_count(void)
{
	return (pcpu_current()->witness_depth);
}

void
witness_dump(void)
{
	pcpu_t	*pc;
	u32	i;

	pc = pcpu_current();
	printk("[WITNESS] cpu %u holds %u lock(s), critnest=%u spin=%u\n",
	    pc->cpu_index, pc->witness_depth, pc->critnest, pc->spin_held);
	for (i = 0; i < pc->witness_depth && i < PCPU_WITNESS_DEPTH; i++) {
		printk("[WITNESS]   [%u] %s level=%u\n", i,
		    pc->witness_name[i] ? pc->witness_name[i] : "?",
		    pc->witness_stack[i]);
	}
	printk("[WITNESS] violations=%llu\n",
	    (unsigned long long)witness_violations);
}

void
witness_lock(const char *name, u32 level)
{
	pcpu_t	*pc;
	u32	top;

	if (!witness_on) {
		return;
	}
	pc = pcpu_current();
	if (pc->witness_depth >= PCPU_WITNESS_DEPTH) {
		witness_violations++;
		witness_dump();
		panic("[WITNESS] lock nesting past %d while taking %s\n",
		    PCPU_WITNESS_DEPTH, name ? name : "?");
	}
	if (pc->witness_depth > 0) {
		top = pc->witness_stack[pc->witness_depth - 1];
		if (level <= top) {
			witness_violations++;
			witness_dump();
			panic("[WITNESS] order violation: %s (level %u) "
			    "after %s (level %u)\n", name ? name : "?", level,
			    pc->witness_name[pc->witness_depth - 1] ?
			    pc->witness_name[pc->witness_depth - 1] : "?", top);
		}
	}
	pc->witness_name[pc->witness_depth] = name;
	pc->witness_stack[pc->witness_depth] = level;
	pc->witness_depth++;
	pc->witness_level = level;
}

void
witness_unlock(const char *name, u32 level)
{
	pcpu_t	*pc;

	if (!witness_on) {
		return;
	}
	pc = pcpu_current();
	if (pc->witness_depth == 0) {
		witness_violations++;
		panic("[WITNESS] release of %s with empty stack on cpu %u\n",
		    name ? name : "?", pc->cpu_index);
	}
	if (pc->witness_stack[pc->witness_depth - 1] != level) {
		witness_violations++;
		witness_dump();
		panic("[WITNESS] out of order release: %s (level %u), "
		    "top is %s (level %u)\n", name ? name : "?", level,
		    pc->witness_name[pc->witness_depth - 1] ?
		    pc->witness_name[pc->witness_depth - 1] : "?",
		    pc->witness_stack[pc->witness_depth - 1]);
	}
	pc->witness_depth--;
	pc->witness_name[pc->witness_depth] = NULL;
	pc->witness_stack[pc->witness_depth] = 0;
	if (pc->witness_depth > 0) {
		pc->witness_level = pc->witness_stack[pc->witness_depth - 1];
	} else {
		pc->witness_level = 0;
	}
}

void
witness_sleep_lock(const char *name, u32 level)
{
	thread_t	*td;

	if (!witness_on) {
		return;
	}
	td = thread_current();
	if (td == NULL) {
		return;
	}
	if (td->lock_depth >= THREAD_WITNESS_DEPTH) {
		witness_violations++;
		panic("[WITNESS] sleepable lock nesting past %d taking %s\n",
		    THREAD_WITNESS_DEPTH, name ? name : "?");
	}
	if (td->lock_depth > 0 &&
	    level <= td->lock_level[td->lock_depth - 1]) {
		witness_violations++;
		panic("[WITNESS] order violation: %s (level %u) after "
		    "%s (level %u) on tid %u\n", name ? name : "?", level,
		    td->lock_name[td->lock_depth - 1] ?
		    td->lock_name[td->lock_depth - 1] : "?",
		    td->lock_level[td->lock_depth - 1], td->tid);
	}
	td->lock_name[td->lock_depth] = name;
	td->lock_level[td->lock_depth] = level;
	td->lock_depth++;
}

void
witness_sleep_unlock(const char *name, u32 level)
{
	thread_t	*td;

	if (!witness_on) {
		return;
	}
	td = thread_current();
	if (td == NULL) {
		return;
	}
	if (td->lock_depth == 0) {
		witness_violations++;
		panic("[WITNESS] release of %s with empty thread stack "
		    "on tid %u\n", name ? name : "?", td->tid);
	}
	if (td->lock_level[td->lock_depth - 1] != level) {
		witness_violations++;
		panic("[WITNESS] out of order release: %s (level %u), "
		    "top is %s (level %u) on tid %u\n", name ? name : "?",
		    level, td->lock_name[td->lock_depth - 1] ?
		    td->lock_name[td->lock_depth - 1] : "?",
		    td->lock_level[td->lock_depth - 1], td->tid);
	}
	td->lock_depth--;
	td->lock_name[td->lock_depth] = NULL;
	td->lock_level[td->lock_depth] = 0;
}

void
witness_check_sleep(const char *who)
{
	pcpu_t	*pc;

	pc = pcpu_current();
	if (pc->spin_held != 0) {
		witness_dump();
		panic("[WITNESS] %s would sleep holding %u spin mutex(es)\n",
		    who ? who : "?", pc->spin_held);
	}
	if (pc->critnest != 0) {
		witness_dump();
		panic("[WITNESS] %s would sleep in critical section (%u)\n",
		    who ? who : "?", pc->critnest);
	}
}
