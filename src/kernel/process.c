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

*/

/* !SPACE!

$space %internal alloc_process
$space %export process_init, process_current, process_set_current
$space %export process_yield, process_exit, process_dump
$space %export process_save_context, process_kill
$space %export process_send_signal, process_is_initialized
$space %export process_get, process_create, process_create_kernel

*/

#include <kernel/gdt.h>
#include <mm/vm/pmap.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <kernel/scheduler.h>
#include <kernel/signal.h>
#include <kernel/thread.h>
#include <kernel/console/terminal.h>
#include <kernel/event/event.h>
#include <kernel/smp/smp.h>
#include <mm/vm/vm_map.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#include <kernel/api/posix/posix.h>

process_t	process_table[MAX_PROCESSES];
u32		next_pid = 1;
static int	process_initialized = 0;

void
process_init(void)
{
	printk("[PROC] Initializing process subsystem...\n");
	memset(process_table, 0, sizeof(process_table));
	next_pid = 1;
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
		if (process_table[i].pid == 0) {
			return (&process_table[i]);
		}
	}
	return (NULL);
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
	proc->exit_code = 0;
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
	scheduler_assign_process(proc);

  api_init_process(proc);
	posix_init_process(proc);

	td = thread_create(proc, (u64)entry, 0, KERNEL_CS,
	    KERNEL_DS);
	if (!td) {
		printk("[PROC] Error: failed to create thread "
		    "for kernel process\n");
		memset(proc, 0, sizeof(process_t));
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
	int	i;

	for (i = 0; i < MAX_PROCESSES; i++) {
		if (process_table[i].pid != 0 &&
		    process_table[i].pid == pid) {
			return (&process_table[i]);
		}
	}
	return (NULL);
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
	int	had_lock;

	had_lock = smp_lock_held();
	if (had_lock) {
		smp_unlock();
	}

	__asm__ volatile("int $32");

	if (had_lock) {
		smp_lock();
	}
}

void
process_exit(int code)
{
	process_t	*proc;
	thread_t	*td;

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

	proc->exit_code = code;

	thread_kill_all(proc);

	if (td) {
		td->state = PROC_STATE_ZOMBIE;
		td->running_cpu = -1;
	}

	api_release_handles(proc);
	posix_cleanup_process(proc);
	if (proc->owns_address_space) {
		u64	old_cr3;

		old_cr3 = proc->cr3;
		vm_map_free_all(proc);
		pmap_load(pmap_kernel_cr3());
		pmap_destroy(old_cr3);
		proc->cr3 = 0;
		proc->owns_address_space = 0;
	}
	proc->mmap_base = MMAP_BASE;

	event_notify_proc_exit(proc->pid, code);
	event_cleanup_process(proc);

	if (proc->ppid > 0) {
		process_t	*parent;

		parent = process_get(proc->ppid);
		if (parent) {
			proc_wakeup((void *)parent);
		}
	}

	terminal_drop_pgrp(proc->pgid);

	__asm__ volatile("sti");
	while (1) {
		__asm__ volatile("hlt");
	}
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
	    "RUNNABLE", "RUNNING", "SLEEPING", "ZOMBIE"};

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

	if (pid == 1) {
		return (-1);
	}

	proc = process_get(pid);
	if (!proc) {
		return (-1);
	}

	if (proc->main_thread &&
	    proc->main_thread->state == PROC_STATE_ZOMBIE) {
		return (0);
	}

	if (proc == process_current()) {
		thread_t	*td;

		thread_kill_all(proc);
		td = thread_current();
		if (td) {
			td->state = PROC_STATE_ZOMBIE;
			td->running_cpu = -1;
		}
	} else {
		thread_kill_all(proc);
		if (proc->main_thread) {
			proc->main_thread->state = PROC_STATE_ZOMBIE;
			proc->main_thread->running_cpu = -1;
		}
		if (proc->cur_thread && proc->cur_thread != proc->main_thread) {
			proc->cur_thread->state = PROC_STATE_ZOMBIE;
			proc->cur_thread->running_cpu = -1;
		}
	}

	api_release_handles(proc);
	posix_cleanup_process(proc);
	if (proc->owns_address_space) {
		u64	old_cr3;

		old_cr3 = pmap_get_cr3();
		pmap_load(proc->cr3);
		vm_map_free_all(proc);
		pmap_load(old_cr3);
		pmap_destroy(proc->cr3);
		proc->cr3 = 0;
		proc->owns_address_space = 0;
	}
	proc->mmap_base = MMAP_BASE;

	event_notify_proc_exit(proc->pid, proc->exit_code);
	event_cleanup_process(proc);

	if (proc->ppid > 0) {
		process_t	*parent;

		parent = process_get(proc->ppid);
		if (parent) {
			proc_wakeup((void *)parent);
		}
	}

	terminal_drop_pgrp(proc->pgid);

	return (0);
}

int
process_send_signal(u32 pid, int sig)
{
	process_t	*proc;
	thread_t	*td;

	if (sig == 0) {
		return (0);
	}

	if (sig < 1 || sig > MAX_POSIX_SIGS) {
		return (-1);
	}

	proc = process_get(pid);
	if (!proc) {
		return (-1);
	}

	event_notify_signal(pid, sig);

	if (sig == SIGKILL) {
		proc->exit_code = 128 + SIGKILL;
		return (process_kill(pid));
	}

	proc->sigpending |= (1ULL << (sig - 1));

	if (proc->personality != PERSONALITY_POSIX) {
		int	dfl;

		dfl = posix_signal_default(sig);
		if (dfl == SIG_DFL_TERMINATE) {
			proc->exit_code = 128 + sig;
			return (process_kill(pid));
		}
	}

	/*
	 * Wake up any sleeping threads so the pending signal can be
	 * delivered instead of waiting indefinitely for the next
	 * syscall entry.
	 */
	for (td = proc->thread_list; td != NULL; td = td->next) {
		if (td->used && td->state == PROC_STATE_SLEEPING) {
			td->state = PROC_STATE_RUNNABLE;
			td->wait_channel = NULL;
		}
	}

	return (0);
}
