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

$define %func scheduler_load_config as procedure with args void
$define %func scheduler_load_snapshot as procedure with args int *
$define %func scheduler_cpu_runnable_count as function with args int
$define %func scheduler_least_loaded_cpu as function with args void
$define %func scheduler_thread_can_run_on as function with args thread_t *, int, const int *
$define %func pick_next_thread as function with args thread_t *
$define %func pick_any_runnable_thread as function with args thread_t *
$define %func scheduler_reap_apc as procedure with args u64, u64, u64
$define %func scheduler_retire_dead as procedure with args thread_t *
$define %func scheduler_cm_update as function with args u32
$define %func scheduler_redirect_to_death as procedure with args thread_t *, registers_t *, int
$define %func scheduler_user_return_work as procedure with args registers_t *
$define %func scheduler_switch as procedure with args registers_t *, int
$define %func scheduler_tick as procedure with args registers_t *
$define %func scheduler_yield as procedure with args registers_t *

*/

/* !SPACE!

$space %internal scheduler_load_config, scheduler_load_snapshot
$space %internal scheduler_cpu_runnable_count, scheduler_least_loaded_cpu
$space %internal scheduler_thread_can_run_on
$space %internal pick_next_thread, pick_any_runnable_thread
$space %internal scheduler_reap_apc, scheduler_retire_dead
$space %internal scheduler_redirect_to_death, scheduler_user_return_work
$space %internal scheduler_switch
$space %export scheduler_cm_update, scheduler_tick, scheduler_yield

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <mm/vm/pmap.h>
#include <kernel/apc.h>
#include <kernel/api/signal.h>
#include <kernel/gdt.h>
#include <kernel/cm/cm.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <kernel/thread.h>
#include <kernel/trace/trace.h>
#include <kernel/api/posix/posix.h>
#include <mm/vm/vm_map.h>
static int	sched_strict_process_separation;
static int	sched_smart_migration = 1;
static int	sched_migration_threshold = 2;
static int	sched_next_cpu;
static spin_t	sched_spin = SPIN_INITIALIZER("sched", LO_SCHED);

static void
scheduler_load_config(void)
{
	sched_strict_process_separation =
	    cm_get_bool_default("SYSTEM", "Scheduler",
	    "StrictProcessSeparation", 0);
	sched_smart_migration =
	    cm_get_bool_default("SYSTEM", "Scheduler",
	    "SmartMigration", 1);
	sched_migration_threshold =
	    (int)cm_get_u32_default("SYSTEM", "Scheduler",
	    "MigrationThreshold", 0);
	if (sched_migration_threshold < 0) {
		sched_migration_threshold = 0;
	}
}
static void
scheduler_load_snapshot(int *load)
{
	thread_t	*td;
	int		i;

	for (i = 0; i < PCPU_MAX_CPUS; i++) {
		load[i] = 0;
	}
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (!td->used || !td->proc) {
			continue;
		}
		if (td->state == PROC_STATE_TERMINATED ||
		    td->state == PROC_STATE_UNUSED) {
			continue;
		}
		if (td->proc->preferred_cpu >= 0 &&
		    td->proc->preferred_cpu < PCPU_MAX_CPUS) {
			load[td->proc->preferred_cpu]++;
		}
		if (td->running_cpu >= 0 && td->running_cpu < PCPU_MAX_CPUS &&
		    td->running_cpu != td->proc->preferred_cpu) {
			load[td->running_cpu]++;
		}
	}
}

static int
scheduler_cpu_runnable_count(int cpu)
{
	int	load[PCPU_MAX_CPUS];

	if (cpu < 0 || cpu >= PCPU_MAX_CPUS) {
		return (0);
	}
	thread_lock();
	scheduler_load_snapshot(load);
	thread_unlock();
	return (load[cpu]);
}

static int
scheduler_least_loaded_cpu(void)
{
	int	load[PCPU_MAX_CPUS];
	int	cpu, cpus, best_cpu;

	cpus = smp_sched_cpu_count();
	if (cpus <= 0) {
		return (0);
	}
	if (cpus > PCPU_MAX_CPUS) {
		cpus = PCPU_MAX_CPUS;
	}
	thread_lock();
	scheduler_load_snapshot(load);
	thread_unlock();
	best_cpu = 0;
	for (cpu = 1; cpu < cpus; cpu++) {
		if (load[cpu] < load[best_cpu]) {
			best_cpu = cpu;
		}
	}
	return (best_cpu);
}

static int
scheduler_thread_can_run_on(thread_t *td, int cpu, const int *load)
{
	process_t	*proc;

	if (!td || !td->proc) {
		return (0);
	}
	proc = td->proc;
	if (td->running_cpu >= 0 && td->running_cpu != cpu) {
		return (0);
	}
	if (proc->preferred_cpu < 0) {
		return (1);
	}
	if (sched_strict_process_separation) {
		return (proc->preferred_cpu == cpu);
	}
	if (!sched_smart_migration) {
		return (1);
	}
	if (proc->last_cpu == cpu || proc->preferred_cpu == cpu) {
		return (1);
	}
	if (cpu < 0 || cpu >= PCPU_MAX_CPUS ||
	    proc->preferred_cpu >= PCPU_MAX_CPUS) {
		return (0);
	}
	return (load[cpu] + sched_migration_threshold <
	    load[proc->preferred_cpu]);
}

void
scheduler_init(void)
{
	scheduler_load_config();
	cm_register_consumer(CM_CONSUMER_SCHEDULER, "scheduler",
	    scheduler_cm_update);
	sched_next_cpu = 0;
	printk("[SCHED] strict=%d smart=%d migration_threshold=%d\n",
	    sched_strict_process_separation, sched_smart_migration,
	    sched_migration_threshold);
}

int
scheduler_cm_update(u32 flags)
{
	(void)flags;
	scheduler_load_config();
	printk("[SCHED] updated strict=%d smart=%d migration_threshold=%d\n",
	    sched_strict_process_separation, sched_smart_migration,
	    sched_migration_threshold);
	return (0);
}

void
scheduler_assign_process(process_t *proc)
{
	int	cpus;

	if (!proc) {
		return;
	}
	cpus = smp_sched_cpu_count();
	if (cpus <= 0) {
		cpus = 1;
	}
	if (sched_strict_process_separation) {
		proc->preferred_cpu = sched_next_cpu % cpus;
		sched_next_cpu = (sched_next_cpu + 1) % cpus;
	} else if (sched_smart_migration) {
		proc->preferred_cpu = scheduler_least_loaded_cpu();
	} else {
		proc->preferred_cpu = -1;
	}
	proc->last_cpu = -1;
	printk("[SCHED] PID %d preferred_cpu=%d\n", proc->pid,
	    proc->preferred_cpu);
}

static thread_t *
pick_next_thread(thread_t *current)
{
	thread_t	*cand;
	int		load[PCPU_MAX_CPUS];
	int		start, i, idx, cpu;

	start = 0;
	cpu = (int)pcpu_current()->cpu_index;
	if (current) {
		start = (int)(current - thread_table) + 1;
	}

	scheduler_load_snapshot(load);
	for (i = 0; i < MAX_THREADS; i++) {
		idx = (start + i) % MAX_THREADS;
		cand = &thread_table[idx];
		if (cand->used &&
		    cand->state == PROC_STATE_RUNNABLE &&
		    scheduler_thread_can_run_on(cand, cpu, load)) {
			return (cand);
		}
	}

	return (current);
}

static thread_t *
pick_any_runnable_thread(thread_t *current)
{
	thread_t	*cand;
	int	start, i, idx, cpu;

	start = 0;
	cpu = (int)pcpu_current()->cpu_index;
	if (current) {
		start = (int)(current - thread_table) + 1;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		idx = (start + i) % MAX_THREADS;
		cand = &thread_table[idx];
		if (!cand->used || cand->state != PROC_STATE_RUNNABLE) {
			continue;
		}
		if (cand->running_cpu >= 0 && cand->running_cpu != cpu) {
			continue;
		}
		return (cand);
	}

	return (current);
}

static void
scheduler_reap_apc(u64 arg1, u64 arg2, u64 arg3)
{
	(void)arg1;
	(void)arg2;
	(void)arg3;
	process_reap();
}

static void
scheduler_retire_dead(thread_t *current)
{
	thread_t	*td;
	int	cpu, i, retired;

	if (!thread_has_dead()) {
		return;
	}

	retired = 0;
	cpu = (int)pcpu_current()->cpu_index;
	thread_lock();
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (td->used && td != current &&
		    thread_state_get(td) == PROC_STATE_TERMINATED &&
		    td->running_cpu == cpu) {
			__atomic_store_n(&td->running_cpu, -1,
			    __ATOMIC_RELEASE);
			thread_retired_dead();
			retired++;
		}
	}
	thread_unlock();

	if (retired > 0 && current != NULL &&
	    thread_state_get(current) != PROC_STATE_TERMINATED &&
	    process_has_reapable()) {
		(void)apc_queue_kernel(current, scheduler_reap_apc, 0, 0, 0);
	}
}

static void
scheduler_redirect_to_death(thread_t *td, registers_t *regs, int code)
{
	u64	rsp;
	rsp = (td->kernel_stack - 64) & ~0xFULL;
	rsp -= 8;

	regs->rip = (u64)process_exit_signalled;
	regs->cs = KERNEL_CS;
	regs->ss = KERNEL_DS;
	regs->rsp = rsp;
	regs->rflags = 0x202;
	regs->rdi = (u64)(u32)code;
}

static void
scheduler_user_return_work(registers_t *regs)
{
	process_t	*proc;
	thread_t	*td;
	int		sig;

	if (!regs || (regs->cs & 3) != 3) {
		return;
	}

	td = thread_current();
	if (!td || td->state == PROC_STATE_TERMINATED) {
		return;
	}
	proc = td->proc;
	if (!proc || proc->pid == 0) {
		return;
	}

	sig = signal_fatal_pending(proc);
	if (sig > 0) {
		signal_clear_pending(proc, sig);
		scheduler_redirect_to_death(td, regs, 128 + sig);
		return;
	}

	(void)apc_deliver(td, regs, APC_AT_USER_RETURN);
}

static void
scheduler_switch(registers_t *regs, int voluntary)
{
	thread_t	*current, *next;
	process_t	*cur_proc;
	u32		trace_reason;
	int		cpu;

	scheduler_user_return_work(regs);

	cpu = (int)pcpu_current()->cpu_index;
	current = thread_current();
	if (!current) {
		spin_lock(&sched_spin);
		thread_lock();
		next = pick_next_thread(NULL);
		if (!next || next->state != PROC_STATE_RUNNABLE) {
			thread_unlock();
			spin_unlock(&sched_spin);
			return;
		}
		thread_set_current(next);
		thread_unlock();
		spin_unlock(&sched_spin);
		if (next->proc) {
			pmap_load(next->proc->cr3);
		}
		trace_sched_switch(NULL, next, TRACE_SCHED_BOOT, regs);
		thread_load_context(next, regs);
		return;
	}

	cur_proc = current->proc;

	if (!voluntary && (regs->cs & 3) == 0 &&
	    thread_state_get(current) == PROC_STATE_RUNNING) {
		return;
	}

	if (thread_state_get(current) == PROC_STATE_SLEEPING) {
		spin_lock(&sched_spin);
		thread_lock();
		current->running_cpu = -1;
		next = pick_next_thread(current);
		if (!next || next == current) {
			current->running_cpu = cpu;
			thread_unlock();
			spin_unlock(&sched_spin);
			return;
		}
		thread_save_context(current, regs);
		thread_set_current(next);
		thread_unlock();
		spin_unlock(&sched_spin);
		trace_sched_switch(current, next, TRACE_SCHED_SLEEP, regs);
		if (next->proc && cur_proc &&
		    next->proc->cr3 != cur_proc->cr3) {
			pmap_load(next->proc->cr3);
		} else if (next->proc && !cur_proc) {
			pmap_load(next->proc->cr3);
		}
		thread_load_context(next, regs);
		return;
	}

	trace_reason = TRACE_SCHED_PREEMPT;
	spin_lock(&sched_spin);
	scheduler_retire_dead(current);
	process_reap();

	thread_lock();
	thread_save_context(current, regs);

	if (current->state == PROC_STATE_RUNNING) {
		thread_state_set(current, PROC_STATE_RUNNABLE);
		current->running_cpu = -1;
	} else if (current->state != PROC_STATE_TERMINATED) {
		current->running_cpu = -1;
	}

	next = pick_next_thread(current);
	if (!next || next == current) {
		if (current->state == PROC_STATE_TERMINATED) {
			next = pick_any_runnable_thread(current);
		}
	}

	if (!next || next == current) {
		if (current->state == PROC_STATE_TERMINATED) {
			current->running_cpu = cpu;
			thread_unlock();
			spin_unlock(&sched_spin);
			return;
		}
		thread_state_set(current, PROC_STATE_RUNNING);
		current->running_cpu = cpu;
		thread_unlock();
		spin_unlock(&sched_spin);
		return;
	}

	if (current->state == PROC_STATE_TERMINATED) {
		trace_reason = TRACE_SCHED_EXIT;
	}
	thread_set_current(next);
	thread_unlock();
	spin_unlock(&sched_spin);
	trace_sched_switch(current, next, trace_reason, regs);
	if (next->proc && cur_proc &&
	    next->proc->cr3 != cur_proc->cr3) {
		pmap_load(next->proc->cr3);
	} else if (next->proc && !cur_proc) {
		pmap_load(next->proc->cr3);
	}

	thread_load_context(next, regs);
}

void
scheduler_tick(registers_t *regs)
{
	static u32	last_magic = 0;
	process_t	*proc;

	if (last_magic == 0) {
		last_magic = g_chainfs.superblock.magic;
	} else if (g_chainfs.superblock.magic != last_magic) {
		proc = process_current();
		printk("[CHAINFS] magic changed in tick "
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
	trace_sched_tick(regs);
	scheduler_switch(regs, 0);
}

void
scheduler_yield(registers_t *regs)
{
	if (!regs) {
		return;
	}
	scheduler_switch(regs, 1);
}
