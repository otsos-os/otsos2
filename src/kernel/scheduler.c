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
$define %func pick_next_thread as function with args thread_t *
$define %func pick_any_runnable_thread as function with args thread_t *
$define %func scheduler_retire_zombies as procedure with args thread_t *
$define %func scheduler_reap_orphans as procedure with args thread_t *
$define %func scheduler_cm_update as function with args u32
$define %func scheduler_tick as procedure with args registers_t *

*/

/* !SPACE!

$space %internal scheduler_load_config
$space %internal pick_next_thread, pick_any_runnable_thread
$space %internal scheduler_retire_zombies, scheduler_reap_orphans
$space %export scheduler_cm_update, scheduler_tick

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <mm/vm/pmap.h>
#include <kernel/cm/cm.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/smp/smp.h>
#include <kernel/thread.h>
#include <kernel/trace/trace.h>
#include <kernel/api/posix/posix.h>
#include <mm/vm/vm_map.h>
static int	sched_strict_process_separation;
static int	sched_smart_migration = 1;
static int	sched_migration_threshold = 2;
static int	sched_next_cpu;
static int	sched_zkill;

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
	sched_zkill =
	    cm_get_bool_default("SYSTEM", "Scheduler", "Zkill", 0);
}
static int
scheduler_cpu_runnable_count(int cpu)
{
	int		i, count;
	thread_t	*td;
	count = 0;
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (!td->used || !td->proc) {
			continue;
		}
		if (td->state == PROC_STATE_ZOMBIE ||
		    td->state == PROC_STATE_UNUSED) {
			continue;
		}
		if (td->proc->preferred_cpu == cpu || td->running_cpu == cpu) {
			count++;
		}
	}
	return (count);
}

static int
scheduler_least_loaded_cpu(void)
{
	int	cpu, cpus, best_cpu, best_load, load;

	cpus = smp_cpu_count();
	if (cpus <= 0) {
		return (0);
	}
	best_cpu = 0;
	best_load = scheduler_cpu_runnable_count(0);
	for (cpu = 1; cpu < cpus; cpu++) {
		load = scheduler_cpu_runnable_count(cpu);
		if (load < best_load) {
			best_cpu = cpu;
			best_load = load;
		}
	}
	return (best_cpu);
}

static int
scheduler_thread_can_run_on(thread_t *td, int cpu)
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
	return (scheduler_cpu_runnable_count(cpu) + sched_migration_threshold <
	    scheduler_cpu_runnable_count(proc->preferred_cpu));
}

void
scheduler_init(void)
{
	scheduler_load_config();
	cm_register_consumer(CM_CONSUMER_SCHEDULER, "scheduler",
	    scheduler_cm_update);
	sched_next_cpu = 0;
	printk("[SCHED] strict=%d smart=%d migration_threshold=%d zkill=%d\n",
	    sched_strict_process_separation, sched_smart_migration,
	    sched_migration_threshold, sched_zkill);
}

int
scheduler_cm_update(u32 flags)
{
	(void)flags;
	scheduler_load_config();
	printk("[SCHED] updated strict=%d smart=%d migration_threshold=%d "
	    "zkill=%d\n", sched_strict_process_separation,
	    sched_smart_migration, sched_migration_threshold,
	    sched_zkill);
	return (0);
}

void
scheduler_assign_process(process_t *proc)
{
	int	cpus;

	if (!proc) {
		return;
	}
	cpus = smp_cpu_count();
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
	if (entity_is_initialized() && proc->entity == 0) {
		char	name[64];

		proc->entity = entity_attach(ENTITY_ARCH_PROCESS,
		    (u32)(proc - process_table), 0, proc->pid,
		    proc->uid, proc->gid, proc->euid, proc->egid,
		    proc->kusr_auth);
		if (proc->entity != 0) {
			snprintf(name, sizeof(name), "/Entity/Process/%u",
			    proc->pid);
			entity_ns_bind(name, proc->entity);
		}
	}
	printk("[SCHED] PID %d preferred_cpu=%d\n", proc->pid,
	    proc->preferred_cpu);
}

static thread_t *
pick_next_thread(thread_t *current)
{
	thread_t	*cand;
	int		start, i, idx, cpu;

	start = 0;
	cpu = smp_cpu_index();
	if (current) {
		start = (int)(current - thread_table) + 1;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		idx = (start + i) % MAX_THREADS;
		cand = &thread_table[idx];
		if (cand->used &&
		    cand->state == PROC_STATE_RUNNABLE &&
		    scheduler_thread_can_run_on(cand, cpu)) {
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
	cpu = smp_cpu_index();
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
scheduler_retire_zombies(thread_t *current)
{
	thread_t	*td;
	int	cpu, i;

	cpu = smp_cpu_index();
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (td->used && td != current &&
		    td->state == PROC_STATE_ZOMBIE &&
		    td->running_cpu == cpu) {
			td->running_cpu = -1;
		}
	}
}

static void
scheduler_reap_orphans(thread_t *skip)
{
	process_t	*proc;
	thread_t	*td;
	int		i;

	if (!sched_zkill) {
		return;
	}

	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (!td->used || td == skip ||
		    td->state != PROC_STATE_ZOMBIE ||
		    td->running_cpu >= 0 || td->proc == NULL) {
			continue;
		}
		proc = td->proc;
		if (proc->ppid != 0 && process_get(proc->ppid) != NULL) {
			continue;
		}
		api_trace_cleanup_process(proc);
		api_release_handles(proc);
		posix_cleanup_process(proc);
		if (proc->owns_address_space && proc->cr3 != 0) {
			u64	old_cr3;

			old_cr3 = pmap_get_cr3();
			pmap_load(proc->cr3);
			vm_map_free_all(proc);
			pmap_load(old_cr3);
			pmap_destroy(proc->cr3);
			proc->cr3 = 0;
			proc->owns_address_space = 0;
		}
		thread_destroy(td);
		memset(proc, 0, sizeof(process_t));
	}
}

void
scheduler_tick(registers_t *regs)
{
	static u32	last_magic = 0;
	thread_t	*current, *next;
	process_t	*cur_proc;
	u32		trace_reason;
	int		locked_here;

	if (last_magic == 0) {
		last_magic = g_chainfs.superblock.magic;
	} else if (g_chainfs.superblock.magic != last_magic) {
		process_t *proc = process_current();
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

	current = thread_current();
	if (!current) {
		locked_here = !smp_lock_held();
		if (locked_here) {
			smp_lock();
		}
		next = pick_next_thread(NULL);
		if (!next || next->state != PROC_STATE_RUNNABLE) {
			if (locked_here) {
				smp_unlock();
			}
			return;
		}
		thread_set_current(next);
		if (locked_here) {
			smp_unlock();
		}
		if (next->proc) {
			pmap_load(next->proc->cr3);
		}
		trace_sched_switch(NULL, next, TRACE_SCHED_BOOT, regs);
		thread_load_context(next, regs);
		return;
	}

	cur_proc = current->proc;

	if ((regs->cs & 3) == 0 &&
	    current->state == PROC_STATE_RUNNING) {
		return;
	}

	if (current->state == PROC_STATE_SLEEPING) {
		locked_here = !smp_lock_held();
		if (locked_here) {
			smp_lock();
		}
		current->running_cpu = -1;
		next = pick_next_thread(current);
		if (!next || next == current) {
			current->running_cpu = smp_cpu_index();
			if (locked_here) {
				smp_unlock();
			}
			return;
		}
		thread_save_context(current, regs);
		trace_sched_switch(current, next, TRACE_SCHED_SLEEP, regs);
		thread_set_current(next);
		if (locked_here) {
			smp_unlock();
		}
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
	locked_here = !smp_lock_held();
	if (locked_here) {
		smp_lock();
	}
	scheduler_retire_zombies(current);
	scheduler_reap_orphans(current);
	thread_save_context(current, regs);

	if (current->state == PROC_STATE_RUNNING) {
		current->state = PROC_STATE_RUNNABLE;
		current->running_cpu = -1;
	} else if (current->state != PROC_STATE_ZOMBIE) {
		current->running_cpu = -1;
	}

	next = pick_next_thread(current);
	if (!next || next == current) {
		if (current->state == PROC_STATE_ZOMBIE) {
			next = pick_any_runnable_thread(current);
		}
	}

	if (!next || next == current) {
		if (current->state == PROC_STATE_ZOMBIE) {
			current->running_cpu = smp_cpu_index();
			if (locked_here) {
				smp_unlock();
			}
			return;
		}
		current->state = PROC_STATE_RUNNING;
		current->running_cpu = smp_cpu_index();
		if (locked_here) {
			smp_unlock();
		}
		return;
	}

	if (current->state == PROC_STATE_ZOMBIE) {
		trace_reason = TRACE_SCHED_EXIT;
	}
	trace_sched_switch(current, next, trace_reason, regs);
	thread_set_current(next);
	if (locked_here) {
		smp_unlock();
	}
	if (next->proc && cur_proc &&
	    next->proc->cr3 != cur_proc->cr3) {
		pmap_load(next->proc->cr3);
	} else if (next->proc && !cur_proc) {
		pmap_load(next->proc->cr3);
	}

	thread_load_context(next, regs);
}
