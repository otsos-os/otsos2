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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type process_t as struct with process control block
$define %type thread_t as struct with per-thread CPU context and state
$define %type registers_t as struct with CPU register snapshot

$define %func process_init as procedure with args void
$define %func alloc_process as function with args void
$define %func process_create_kernel as function with args const char *, void (*)(void)
$define %func process_get as function with args u32
$define %func process_current as function with args void
$define %func process_set_current as procedure with args process_t *
$define %func process_yield as procedure with args void
$define %func process_exit as procedure with args int
$define %func process_dump as procedure with args process_t *
$define %func process_save_context as procedure with args process_t *, registers_t *
$define %func process_kill as function with args u32
$define %func process_send_signal as function with args u32, int
$define %func process_exit_signalled as procedure with args int
$define %func process_entity_attach as function with args process_t *
$define %func process_entity_of_pid as function with args u32
$define %func process_creation_abort as procedure with args process_t *
$define %func process_ref as function with args u32
$define %func process_unref as procedure with args process_t *
$define %func process_has_reapable as function with args void
$define %func process_record_find_child as function with args entity id, u32
$define %func process_child_count as function with args u32
$define %func process_record_read as function with args entity id, int *, int *, u32 *, u32 *
$define %func process_record_mark_reaped as function with args entity id
$define %func process_record_consume as function with args entity id
$define %func process_record_notify_mode as function with args entity id
$define %func process_record_set_notify as function with args entity id, int
$define %func process_teardown_resources as procedure with args process_t *
$define %func process_records_release_children as procedure with args entity id
$define %func process_release_hook as procedure with args entity id
$define %func process_notify_parent as procedure with args process_t *, int
$define %func process_reap as procedure with args void
$define %func process_dump_records as procedure with args void

*/

/* !SPACE!

$space %internal alloc_process, process_teardown_resources
$space %internal process_release_hook, process_notify_parent
$space %internal process_records_release_children
$space %export process_init, process_current, process_set_current
$space %export process_yield, process_exit, process_dump
$space %export process_save_context, process_kill
$space %export process_send_signal, process_is_initialized
$space %export process_get, process_create, process_create_kernel
$space %export process_exit_signalled, process_entity_attach
$space %export process_entity_of_pid, process_record_read
$space %export process_child_count, process_creation_abort
$space %export process_ref, process_unref, process_has_reapable
$space %export process_record_find_child
$space %export process_record_mark_reaped, process_record_notify_mode
$space %export process_record_consume
$space %export process_record_set_notify
$space %export process_reap, process_dump_records

*/

#include <kernel/apc.h>
#include <kernel/gdt.h>
#include <mm/vm/pmap.h>
#include <kernel/drivers/timer.h>
#include <kernel/interrupts/irq.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/thread.h>
#include <kernel/console/terminal.h>
#include <kernel/event/event.h>
#include <kernel/entity/entity.h>
#include <kernel/smp/smp.h>
#include <mm/vm/vm_map.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#include <kernel/api/posix/posix.h>
#include <kernel/api/signal.h>

process_block_t	process_block;
u32		next_pid = 1;
static int	process_initialized = 0;
static volatile int	process_reapable_pending;

static void	process_teardown_resources(process_t *proc);
static void	process_release_hook(entity_id_t id);
static void	process_notify_parent(process_t *proc, int code);
static void	process_records_release_children(entity_id_t parent);

void
process_init(void)
{
	printk("[PROC] Initializing process subsystem...\n");
	memset(process_table, 0, sizeof(process_table));
	entity_meta_register(ENTITY_ARCH_PROCESS, &process_block.meta,
	    0, MAX_PROCESSES);
	entity_arch_release_register(ENTITY_ARCH_PROCESS,
	    process_release_hook);
	next_pid = 1;
	apc_init();
	thread_init();
	scheduler_init();
	process_initialized = 1;
	printk("[PROC] Process table initialized "
	    "(%d slots)\n", MAX_PROCESSES);
}

process_t *
alloc_process(void)
{
	int		i;

	for (i = 0; i < MAX_PROCESSES; i++) {
		if (process_table[i].pid != 0) {
			continue;
		}
		if (entity_is_initialized() &&
		    entity_slot_used(ENTITY_ARCH_PROCESS, (u32)i)) {
			continue;
		}
		return (&process_table[i]);
	}
	return (NULL);
}

int
process_entity_attach(process_t *proc)
{
	entity_id_t	id;
	char		name[64];

	if (!proc) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!entity_is_initialized() || proc->entity != 0) {
		return (0);
	}

	id = entity_attach(ENTITY_ARCH_PROCESS,
	    (u32)(proc - process_table), 0, proc->pid, proc->uid,
	    proc->gid, proc->euid, proc->egid, proc->kusr_auth);
	if (id == 0) {
		printk("[PROC] entity_attach failed for PID %d slot %d\n",
		    proc->pid, (int)(proc - process_table));
		return (-API_ERR_NO_MEMORY);
	}

	if (entity_retain_checked(id) != 0) {
		entity_destroy(id);
		return (-API_ERR_NO_MEMORY);
	}

	proc->entity = id;
	entity_set_i32(id, PROC_REC_PID, (s32)proc->pid);
	entity_set_i32(id, PROC_REC_PPID, (s32)proc->ppid);
	entity_set_i32(id, PROC_REC_FLAGS, 0);
	entity_set_i32(id, PROC_REC_CODE, 0);
	entity_set_i32(id, PROC_REC_NOTIFY, PROC_NOTIFY_NONE);
	entity_set_data(id, PROC_REC_TICK, 0);
	entity_set_data(id, PROC_REC_PARENT_ID,
	    (u64)process_entity_of_pid(proc->ppid));
	snprintf(name, sizeof(name), "/Entity/Process/%u", proc->pid);
	entity_ns_bind(name, id);
	return (0);
}

process_t *
process_create_kernel(const char *name, void (*entry)(void))
{
	process_t	*proc;
	thread_t	*td;

	proc = alloc_process();
	if (!proc) {
		printk("[PROC] Error: no free process slots\n");
		return (NULL);
	}

	memset(proc, 0, sizeof(process_t));

	proc->pid = next_pid++;
	proc->ppid = 0;

	int		i;
	for (i = 0; i < PROCESS_NAME_LEN - 1 && name[i]; i++) {
		proc->name[i] = name[i];
	}
	proc->name[i] = '\0';

	proc->cr3 = pmap_get_cr3();
	proc->entry_point = (u64)entry;
	proc->owns_address_space = 0;
	proc->mmap_base = MMAP_BASE;
	proc->preferred_cpu = -1;
	proc->last_cpu = -1;
  proc->personality = PERSONALITY_OTSOS;
  proc->sid = proc->pid;
  proc->pgid = proc->pid;
  proc->is_session_leader = 1;
  proc->uid = 0;
  proc->gid = 0;
  proc->euid = 0;
  proc->egid = 0;
  proc->suid = 0;
  proc->sgid = 0;
	if (process_entity_attach(proc) != 0) {
		process_creation_abort(proc);
		return (NULL);
	}
	scheduler_assign_process(proc);

  api_init_process(proc);
	posix_init_process(proc);

	td = thread_create(proc, (u64)entry, 0, KERNEL_CS,
	    KERNEL_DS);
	if (!td) {
		printk("[PROC] Error: failed to create thread "
		    "for kernel process\n");
		process_creation_abort(proc);
		return (NULL);
	}

	td->context.rsp = td->kernel_stack;

	proc->main_thread = td;
	proc->cur_thread = td;

	printk("[PROC] Created kernel process '%s' "
	    "(PID %d) entry=%p\n", proc->name, proc->pid,
	    (void *)proc->entry_point);

	return (proc);
}

process_t *
process_get(u32 pid)
{
	process_t	*proc;
	int		i;

	if (pid == 0) {
		return (NULL);
	}
	for (i = 0; i < MAX_PROCESSES; i++) {
		proc = &process_table[i];
		if (proc->pid != pid) {
			continue;
		}
		if (!entity_is_initialized() || proc->entity == 0) {
			return (proc);
		}
		if (entity_state(proc->entity) != ENTITY_STATE_ACTIVE) {
			return (NULL);
		}
		return (proc);
	}
	return (NULL);
}

int
process_has_reapable(void)
{
	return (process_reapable_pending != 0);
}

process_t *
process_ref(u32 pid)
{
	process_t	*proc;

	proc = process_get(pid);
	if (!proc) {
		return (NULL);
	}
	if (proc->entity == 0) {
		return (proc);
	}
	if (entity_retain_checked(proc->entity) != 0) {
		return (NULL);
	}
	return (proc);
}

void
process_unref(process_t *proc)
{
	entity_id_t	id;

	if (!proc) {
		return;
	}
	id = proc->entity;
	if (id == 0) {
		return;
	}
	entity_release(id);
}

entity_id_t
process_entity_of_pid(u32 pid)
{
	entity_id_t	id;
	s32		rec_pid;
	int		i;

	if (pid == 0 || !entity_is_initialized()) {
		return (0);
	}
	for (i = 0; i < MAX_PROCESSES; i++) {
		id = entity_id_at(ENTITY_ARCH_PROCESS, (u32)i);
		if (id == 0) {
			continue;
		}
		rec_pid = 0;
		if (entity_get_i32(id, PROC_REC_PID, &rec_pid) != 0) {
			continue;
		}
		if ((u32)rec_pid == pid) {
			return (id);
		}
	}
	return (0);
}

void
process_creation_abort(process_t *proc)
{
	entity_id_t	id;

	if (!proc) {
		return;
	}

	id = proc->entity;
	proc->entity = 0;
	if (id != 0) {
		entity_ns_unbind_all_id(id);
		entity_destroy(id);
		entity_release(id);
	}
	memset(proc, 0, sizeof(process_t));
}

entity_id_t
process_record_find_child(entity_id_t parent, u32 want_pid)
{
	entity_id_t	id;
	u64		parent_id;
	u32		pid, ppid;
	int		code, flags, i;

	if (parent == 0 || !entity_is_initialized()) {
		return (0);
	}

	for (i = 0; i < MAX_PROCESSES; i++) {
		id = entity_id_at(ENTITY_ARCH_PROCESS, (u32)i);
		if (id == 0 || id == parent) {
			continue;
		}
		parent_id = 0;
		if (entity_get_data(id, PROC_REC_PARENT_ID, &parent_id) != 0) {
			continue;
		}
		if ((entity_id_t)parent_id != parent) {
			continue;
		}
		if (process_record_read(id, &code, &flags, &pid, &ppid) != 0) {
			continue;
		}
		(void)code;
		(void)ppid;
		if ((flags & PROC_EXIT_EXITED) == 0) {
			continue;
		}
		if (flags & PROC_EXIT_REAPED) {
			continue;
		}
		if (want_pid != 0 && pid != want_pid) {
			continue;
		}
		return (id);
	}
	return (0);
}

int
process_child_count(u32 pid)
{
	int	count, i;

	if (pid == 0) {
		return (0);
	}

	count = 0;
	for (i = 0; i < MAX_PROCESSES; i++) {
		if (process_table[i].pid == 0) {
			continue;
		}
		if (process_table[i].ppid != pid) {
			continue;
		}
		count++;
	}
	return (count);
}

int
process_record_read(entity_id_t id, int *code, int *flags, u32 *pid,
    u32 *ppid)
{
	s32	value;

	if (id == 0 || entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (code) {
		value = 0;
		if (entity_get_i32(id, PROC_REC_CODE, &value) != 0) {
			return (-API_ERR_BAD_HANDLE);
		}
		*code = (int)value;
	}
	if (flags) {
		value = 0;
		if (entity_get_i32(id, PROC_REC_FLAGS, &value) != 0) {
			return (-API_ERR_BAD_HANDLE);
		}
		*flags = (int)value;
	}
	if (pid) {
		value = 0;
		entity_get_i32(id, PROC_REC_PID, &value);
		*pid = (u32)value;
	}
	if (ppid) {
		value = 0;
		entity_get_i32(id, PROC_REC_PPID, &value);
		*ppid = (u32)value;
	}
	return (0);
}

int
process_record_mark_reaped(entity_id_t id)
{
	s32	flags;

	if (id == 0 || entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	flags = 0;
	if (entity_get_i32(id, PROC_REC_FLAGS, &flags) != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	return (entity_set_i32(id, PROC_REC_FLAGS,
	    flags | PROC_EXIT_REAPED));
}

int
process_record_consume(entity_id_t id)
{
	s32	flags;

	if (id == 0 || entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	flags = 0;
	if (entity_get_i32(id, PROC_REC_FLAGS, &flags) != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (flags & PROC_EXIT_REAPED) {
		return (-API_ERR_NO_CHILD);
	}
	entity_set_i32(id, PROC_REC_FLAGS, flags | PROC_EXIT_REAPED);
	entity_release(id);
	return (0);
}

static void
process_records_release_children(entity_id_t parent)
{
	entity_id_t	id;
	u64		parent_id;
	s32		flags;
	int		i;

	if (parent == 0 || !entity_is_initialized()) {
		return;
	}

	for (i = 0; i < MAX_PROCESSES; i++) {
		id = entity_id_at(ENTITY_ARCH_PROCESS, (u32)i);
		if (id == 0 || id == parent) {
			continue;
		}
		parent_id = 0;
		if (entity_get_data(id, PROC_REC_PARENT_ID, &parent_id) != 0) {
			continue;
		}
		if ((entity_id_t)parent_id != parent) {
			continue;
		}
		flags = 0;
		entity_get_i32(id, PROC_REC_FLAGS, &flags);
		if ((flags & PROC_EXIT_EXITED) == 0) {
			continue;
		}
		(void)process_record_consume(id);
	}
}

int
process_record_notify_mode(entity_id_t id)
{
	s32	mode;

	mode = PROC_NOTIFY_NONE;
	if (id == 0 || entity_get_i32(id, PROC_REC_NOTIFY, &mode) != 0) {
		return (PROC_NOTIFY_NONE);
	}
	return ((int)mode);
}

int
process_record_set_notify(entity_id_t id, int mode)
{
	if (id == 0 || entity_arch(id) != ENTITY_ARCH_PROCESS) {
		return (-API_ERR_BAD_HANDLE);
	}
	if ((mode & ~PROC_NOTIFY_APC) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	return (entity_set_i32(id, PROC_REC_NOTIFY, (s32)mode));
}

process_t *
process_current(void)
{
	thread_t	*td;

	td = thread_current();
	if (!td) {
		return (NULL);
	}
	return (td->proc);
}

int
process_is_initialized(void)
{
	return (process_initialized);
}

void
process_set_current(process_t *proc)
{
	if (!proc) {
		return;
	}

	thread_t	*td;

	td = proc->cur_thread;
	if (!td) {
		td = proc->main_thread;
	}
	if (td) {
		thread_set_current(td);
	}
}

void
process_yield(void)
{
	u32	depth;

	depth = smp_lock_release_all();

	__asm__ volatile("int %0" :: "i"(IRQ_VECTOR_YIELD) : "memory");

	if (depth != 0) {
		smp_lock_acquire_depth(depth);
	}
}

static void
process_teardown_resources(process_t *proc)
{
	u64	old_cr3;

	if (!proc || proc->resources_released) {
		return;
	}
	proc->resources_released = 1;

	api_trace_cleanup_process(proc);
	api_release_handles(proc);
	posix_cleanup_process(proc);
	event_cleanup_process(proc);

	if (proc->owns_address_space && proc->cr3 != 0) {
		old_cr3 = pmap_get_cr3();
		if (old_cr3 == proc->cr3) {
			vm_map_free_all(proc);
			pmap_load(pmap_kernel_cr3());
		} else {
			pmap_load(proc->cr3);
			vm_map_free_all(proc);
			pmap_load(old_cr3);
		}
		pmap_destroy(proc->cr3);
		proc->cr3 = 0;
		proc->owns_address_space = 0;
	}
	proc->mmap_base = MMAP_BASE;
	terminal_drop_pgrp(proc->pgid);
}

static void
process_release_hook(entity_id_t id)
{
	process_t	*proc;
	u32		index;

	index = entity_id_index(id);
	if (index >= MAX_PROCESSES) {
		return;
	}

	proc = &process_table[index];
	if (proc->entity == id) {
		if (proc->reapable && process_reapable_pending > 0) {
			process_reapable_pending--;
		}
		memset(proc, 0, sizeof(process_t));
	}

	entity_set_i32(id, PROC_REC_FLAGS, 0);
	entity_set_i32(id, PROC_REC_PID, 0);
	entity_set_i32(id, PROC_REC_PPID, 0);
	entity_set_i32(id, PROC_REC_CODE, 0);
	entity_set_i32(id, PROC_REC_NOTIFY, PROC_NOTIFY_NONE);
	entity_set_data(id, PROC_REC_PARENT_ID, 0);
	entity_set_data(id, PROC_REC_TICK, 0);
}

static void
process_notify_parent(process_t *proc, int code)
{
	process_t	*parent;
	thread_t	*td;
	int		mode;

	if (!proc || proc->ppid == 0) {
		return;
	}
	parent = process_ref(proc->ppid);
	if (!parent) {
		return;
	}
	proc_wakeup((void *)parent);

	mode = process_record_notify_mode(proc->entity);
	if ((mode & PROC_NOTIFY_APC) == 0 || parent->exit_upcall == 0) {
		process_unref(parent);
		return;
	}
	td = parent->main_thread;
	if (!td || td->state == PROC_STATE_TERMINATED) {
		process_unref(parent);
		return;
	}
	(void)apc_queue_user(td, parent->exit_upcall,
	    parent->exit_upcall_special, (u64)proc->entity,
	    (u64)proc->pid, (u64)(s64)code);
	process_unref(parent);
}

void
process_exit(int code)
{
	process_t	*proc;
	thread_t	*td;
	int		 i;

	proc = process_current();
	if (!proc) {
		printk("[PROC] Error: no current process "
		    "to exit\n");
		return;
	}

	td = thread_current();
	if (!td) {
		td = proc->main_thread;
	}

	printk("[PROC] Process '%s' (PID %d) exited "
	    "with code %d\n", proc->name, proc->pid, code);

	if (proc->pid == 1) {
		panic("Init process terminated! (PID 1 exited "
		    "with code %d)", code);
	}

	if (proc->entity != 0) {
		s32	flags;

		flags = 0;
		entity_get_i32(proc->entity, PROC_REC_FLAGS, &flags);
		entity_set_i32(proc->entity, PROC_REC_CODE, (s32)code);
		entity_set_i32(proc->entity, PROC_REC_PID, (s32)proc->pid);
		entity_set_i32(proc->entity, PROC_REC_PPID, (s32)proc->ppid);
		entity_set_i32(proc->entity, PROC_REC_FLAGS,
		    flags | PROC_EXIT_EXITED);
		entity_set_data(proc->entity, PROC_REC_TICK,
		    timer_is_initialized() ? timer_get_ticks() : 0);
	}

	thread_kill_all(proc);

	if (td) {
		td->running_cpu = smp_cpu_index();
		thread_mark_dead(td);
	}

	proc->reapable = 1;
	process_reapable_pending++;

	process_teardown_resources(proc);

	event_notify_proc_exit(proc->pid, code);
	event_notify_proc_reap(proc->ppid);
	process_notify_parent(proc, code);

	for (i = 0; i < MAX_PROCESSES; i++) {
		if (process_table[i].pid == 0 ||
		    process_table[i].ppid != proc->pid) {
			continue;
		}
		process_table[i].ppid = 1;
		if (process_table[i].entity != 0) {
			entity_set_i32(process_table[i].entity,
			    PROC_REC_PPID, 1);
		}
	}
	if (proc->entity != 0) {
		process_records_release_children(proc->entity);
		entity_destroy(proc->entity);
	}
	while (1) {
		process_yield();
	}
}

void
process_exit_signalled(int code)
{
	process_exit(code);
	panic("process_exit returned for a signalled exit");
}

void
process_reap(void)
{
	process_t	*proc;
	thread_t	*td, *next;
	entity_id_t	id;
	u64		parent_id;
	int		i, busy, retain;

	if (process_reapable_pending == 0) {
		return;
	}

	for (i = 0; i < MAX_PROCESSES; i++) {
		proc = &process_table[i];
		if (proc->pid == 0 || !proc->reapable) {
			continue;
		}

		busy = 0;
		for (td = proc->thread_list; td != NULL; td = td->next) {
			if (td->running_cpu >= 0) {
				busy = 1;
				break;
			}
		}
		if (busy) {
			continue;
		}

		process_teardown_resources(proc);

		td = proc->thread_list;
		while (td != NULL) {
			next = td->next;
			thread_destroy(td);
			td = next;
		}

		id = proc->entity;

		parent_id = 0;
		retain = 0;
		if (id != 0) {
			entity_get_data(id, PROC_REC_PARENT_ID, &parent_id);
			if (parent_id != 0 &&
			    entity_state((entity_id_t)parent_id) ==
			    ENTITY_STATE_ACTIVE &&
			    process_record_notify_mode(id) ==
			    PROC_NOTIFY_NONE) {
				retain = 1;
			}
		}

		proc->reapable = 0;
		if (process_reapable_pending > 0) {
			process_reapable_pending--;
		}
		proc->thread_list = NULL;
		proc->thread_count = 0;
		proc->main_thread = NULL;
		proc->cur_thread = NULL;

		if (!retain) {
			(void)process_record_consume(id);
		}
	}
}

void
process_dump_records(void)
{
	entity_id_t	id;
	u64		tick;
	s32		pid, ppid, code, flags, notify;
	int		i;

	printk("=== Process records ===\n");
	for (i = 0; i < MAX_PROCESSES; i++) {
		id = entity_id_at(ENTITY_ARCH_PROCESS, (u32)i);
		if (id == 0) {
			continue;
		}
		pid = 0;
		ppid = 0;
		code = 0;
		flags = 0;
		notify = 0;
		tick = 0;
		entity_get_i32(id, PROC_REC_PID, &pid);
		entity_get_i32(id, PROC_REC_PPID, &ppid);
		entity_get_i32(id, PROC_REC_CODE, &code);
		entity_get_i32(id, PROC_REC_FLAGS, &flags);
		entity_get_i32(id, PROC_REC_NOTIFY, &notify);
		entity_get_data(id, PROC_REC_TICK, &tick);
		printk("  slot=%d id=%p pid=%d ppid=%d state=%s refs=%d "
		    "code=%d flags=0x%x notify=0x%x tick=%llu name=%s\n",
		    i, (void *)id, (int)pid, (int)ppid,
		    entity_state(id) == ENTITY_STATE_ACTIVE ? "ACTIVE" :
		    "EXITED", (int)entity_refs(id), (int)code,
		    (unsigned)flags, (unsigned)notify,
		    (unsigned long long)tick,
		    process_table[i].pid != 0 ? process_table[i].name :
		    "<freed>");
	}
	printk("  entity refcount saturations: %llu\n",
	    (unsigned long long)entity_saturations());
}

void
process_dump(process_t *proc)
{
	thread_t	*td;

	if (!proc) {
		printk("[PROC] NULL process\n");
		return;
	}

	td = proc->main_thread;

	const char	*state_names[] = {"UNUSED", "EMBRYO",
	    "RUNNABLE", "RUNNING", "SLEEPING", "TERMINATED"};

	printk("=== Process Dump ===\n");
	printk("  PID: %d, PPID: %d\n", proc->pid,
	    proc->ppid);
	printk("  Name: %s\n", proc->name);
	if (td) {
		printk("  State: %s\n",
		    state_names[td->state]);
		printk("  TID: %d\n", td->tid);
		printk("  Kernel Stack: %p\n",
		    (void *)td->kernel_stack);
		printk("  Context RIP: %p\n",
		    (void *)td->context.rip);
		printk("  Context RSP: %p\n",
		    (void *)td->context.rsp);
	}
	printk("  Entry: %p\n", (void *)proc->entry_point);
	printk("  CR3: %p\n", (void *)proc->cr3);
	printk("  User Stack: %p\n",
	    (void *)proc->user_stack);
	printk("====================\n");
}

void
process_save_context(process_t *proc, registers_t *regs)
{
	thread_t	*td;

	if (!proc || !regs) {
		return;
	}

	td = proc->cur_thread;
	if (!td) {
		td = proc->main_thread;
	}
	if (!td) {
		return;
	}

	thread_save_context(td, regs);
}

process_t *
process_create(const char *name, void *elf_data, u64 elf_size)
{
	extern process_t *userspace_load_elf(const char *name,
	    void *elf_data, u64 elf_size);

	return (userspace_load_elf(name, elf_data, elf_size));
}

int
process_kill(u32 pid)
{
	process_t	*proc;
	thread_t	*td;
	int		code, i;

	if (pid == 1) {
		return (-1);
	}

	proc = process_get(pid);
	if (!proc) {
		return (-1);
	}

	code = 128 + SIGKILL;

	if (proc == process_current()) {
		process_exit(code);
		return (0);
	}

	if (proc->entity != 0) {
		s32	flags;

		flags = 0;
		entity_get_i32(proc->entity, PROC_REC_FLAGS, &flags);
		entity_set_i32(proc->entity, PROC_REC_CODE, (s32)code);
		entity_set_i32(proc->entity, PROC_REC_PID, (s32)proc->pid);
		entity_set_i32(proc->entity, PROC_REC_PPID, (s32)proc->ppid);
		entity_set_i32(proc->entity, PROC_REC_FLAGS,
		    flags | PROC_EXIT_EXITED | PROC_EXIT_KILLED);
		entity_set_data(proc->entity, PROC_REC_TICK,
		    timer_is_initialized() ? timer_get_ticks() : 0);
	}

	thread_kill_all(proc);
	for (td = proc->thread_list; td != NULL; td = td->next) {
		thread_mark_dead(td);
	}
	proc->reapable = 1;
	process_reapable_pending++;

	event_notify_proc_exit(proc->pid, code);
	event_notify_proc_reap(proc->ppid);
	process_notify_parent(proc, code);

	for (i = 0; i < MAX_PROCESSES; i++) {
		if (process_table[i].pid == 0 ||
		    process_table[i].ppid != proc->pid) {
			continue;
		}
		process_table[i].ppid = 1;
		if (process_table[i].entity != 0) {
			entity_set_i32(process_table[i].entity,
			    PROC_REC_PPID, 1);
		}
	}

	if (proc->entity != 0) {
		process_records_release_children(proc->entity);
		entity_destroy(proc->entity);
	}

	return (0);
}

int
process_send_signal(u32 pid, int sig)
{
	if (sig == 0) {
		return (0);
	}

	return (signal_send(pid, sig));
}
