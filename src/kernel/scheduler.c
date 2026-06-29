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
$define %type int as 32 bit signed
$define %type thread_t as struct with per-thread CPU context and state
$define %type process_t as struct with process control block
$define %type registers_t as struct with CPU register snapshot

$define %func pick_next_thread as function with args thread_t *
$define %func scheduler_tick as procedure with args registers_t *

*/

/* !SPACE!

$space %internal pick_next_thread
$space %export scheduler_tick

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <mm/vm/pmap.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/thread.h>

static thread_t *
pick_next_thread(thread_t *current)
{
	thread_t	*cand;
	int		start, i, idx;

	start = 0;
	if (current) {
		start = (int)(current - thread_table) + 1;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		idx = (start + i) % MAX_THREADS;
		cand = &thread_table[idx];
		if (cand->used &&
		    cand->state == PROC_STATE_RUNNABLE) {
			return (cand);
		}
	}

	return (current);
}

void
scheduler_tick(registers_t *regs)
{
	static u32	last_magic = 0;
	thread_t	*current, *next;
	process_t	*cur_proc;

	if (last_magic == 0) {
		last_magic = g_chainfs.superblock.magic;
	} else if (g_chainfs.superblock.magic != last_magic) {
		process_t *proc = process_current();
		com1_printf("[CHAINFS] magic changed in tick "
		    "(pid=%d) old=0x%x new=0x%x "
		    "rip=%p cs=0x%x cr3=%p phys=%p init_phys=%p\n",
		    proc ? proc->pid : -1, last_magic,
		    g_chainfs.superblock.magic,
		    (void *)(regs ? regs->rip : 0),
		    regs ? regs->cs : 0,
		    (void *)pmap_get_cr3(),
		    (void *)pmap_extract((u64)&g_chainfs),
		    (void *)g_chainfs_phys);
		last_magic = g_chainfs.superblock.magic;
	}

	if (!regs) {
		return;
	}

	current = thread_current();
	if (!current) {
		return;
	}

	cur_proc = current->proc;

	if ((regs->cs & 3) == 0 &&
	    current->state == PROC_STATE_RUNNING) {
		return;
	}

	if (current->state == PROC_STATE_SLEEPING) {
		next = pick_next_thread(current);
		if (!next || next == current) {
			return;
		}
		thread_save_context(current, regs);
		thread_set_current(next);
		if (next->proc && cur_proc &&
		    next->proc->cr3 != cur_proc->cr3) {
			pmap_load(next->proc->cr3);
		}
		thread_load_context(next, regs);
		return;
	}

	thread_save_context(current, regs);

	if (current->state == PROC_STATE_RUNNING) {
		current->state = PROC_STATE_RUNNABLE;
	}

	next = pick_next_thread(current);
	if (!next || next == current) {
		current->state = PROC_STATE_RUNNING;
		return;
	}

	thread_set_current(next);
	if (next->proc && cur_proc &&
	    next->proc->cr3 != cur_proc->cr3) {
		pmap_load(next->proc->cr3);
	}

	thread_load_context(next, regs);
}
