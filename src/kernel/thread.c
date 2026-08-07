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
$define %type thread_t as struct with per-thread CPU context, stack, state
$define %type process_t as struct with process control block
$define %type registers_t as struct with CPU register snapshot
$define %type cpu_context_t as struct with saved CPU registers
$define %type fpu_context_t as aligned 512 byte FXSAVE image
$define %type u8 as 8 bit unsigned

$define %func thread_init as procedure with args void
$define %func thread_alloc as function with args void
$define %func thread_create as function with args process_t *, u64, u64, u64, u64
$define %func thread_current as function with args void
$define %func thread_set_current as procedure with args thread_t *
$define %func thread_save_context as procedure with args thread_t *, registers_t *
$define %func thread_load_context as procedure with args thread_t *, registers_t *
$define %func thread_load_fpu_context as procedure with args thread_t *
$define %func thread_copy_fpu_context as procedure with args thread_t *, thread_t *
$define %func thread_get_by_proc as function with args process_t *
$define %func thread_get as function with args u32
$define %func thread_destroy as procedure with args thread_t *
$define %func thread_is_initialized as function with args void
$define %func thread_exit as procedure with args int
$define %func thread_join as function with args u32, int *
$define %func thread_count_alive as function with args process_t *
$define %func thread_kill_all as procedure with args process_t *
$define %func thread_fpu_init_context as procedure with args fpu_context_t *
$define %func thread_fpu_save as procedure with args thread_t *
$define %func thread_link as procedure with args process_t *, thread_t *
$define %func thread_unlink as procedure with args thread_t *

*/

/* !SPACE!

$space %internal thread_init, thread_alloc
$space %internal thread_link, thread_unlink
$space %export thread_create, thread_current, thread_set_current
$space %export thread_save_context, thread_load_context
$space %export thread_load_fpu_context, thread_copy_fpu_context
$space %export thread_get_by_proc, thread_get, thread_destroy
$space %export thread_is_initialized
$space %export thread_exit, thread_join
$space %export thread_count_alive, thread_kill_all
$space %internal thread_fpu_init_context, thread_fpu_save

*/

#include <kernel/gdt.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/event/event.h>
#include <kernel/entity/entity.h>
#include <kernel/smp/smp.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

extern void	futex_wake_all(u64 uaddr);

#define	MSR_FS_BASE	0xC0000100
#define	FPU_FCW_DEFAULT	0x037F
#define	FPU_MXCSR_DEFAULT	0x1F80
#define	FPU_MXCSR_MASK_DEFAULT	0xFFFF

static inline u64
thread_rdmsr(u32 msr)
{
	u32	low, high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((u64)high << 32) | low);
}

static inline void
thread_wrmsr(u32 msr, u64 value)
{
	u32	low, high;

	low = (u32)value;
	high = (u32)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

thread_block_t	thread_block;
u32		next_tid = 1;
static int	thread_initialized = 0;

static void	thread_link(process_t *proc, thread_t *td);
static void	thread_unlink(thread_t *td);
static void	thread_fpu_init_context(fpu_context_t *ctx);
static void	thread_fpu_save(thread_t *td);

static void
thread_fpu_init_context(fpu_context_t *ctx)
{
	if (!ctx) {
		return;
	}

	memset(ctx, 0, sizeof(*ctx));
	ctx->bytes[0] = (u8)(FPU_FCW_DEFAULT & 0xff);
	ctx->bytes[1] = (u8)(FPU_FCW_DEFAULT >> 8);
	ctx->bytes[24] = (u8)(FPU_MXCSR_DEFAULT & 0xff);
	ctx->bytes[25] = (u8)((FPU_MXCSR_DEFAULT >> 8) & 0xff);
	ctx->bytes[26] = (u8)((FPU_MXCSR_DEFAULT >> 16) & 0xff);
	ctx->bytes[27] = (u8)(FPU_MXCSR_DEFAULT >> 24);
	ctx->bytes[28] = (u8)(FPU_MXCSR_MASK_DEFAULT & 0xff);
	ctx->bytes[29] = (u8)((FPU_MXCSR_MASK_DEFAULT >> 8) & 0xff);
	ctx->bytes[30] = (u8)((FPU_MXCSR_MASK_DEFAULT >> 16) & 0xff);
	ctx->bytes[31] = (u8)(FPU_MXCSR_MASK_DEFAULT >> 24);
}

static void
thread_fpu_save(thread_t *td)
{
	__asm__ volatile("fxsave64 %0" : "=m"(td->fpu_context) :: "memory");
	td->fpu_valid = 1;
}

void
thread_init(void)
{
	printk("[THREAD] Initializing thread subsystem...\n");
	memset(thread_table, 0, sizeof(thread_table));
	entity_meta_register(ENTITY_ARCH_THREAD, &thread_block.meta,
	    0, MAX_THREADS);
	next_tid = 1;
	smp_set_current_thread(NULL);
	thread_initialized = 1;
	printk("[THREAD] Thread table initialized "
	    "(%d slots)\n", MAX_THREADS);
}

int
thread_is_initialized(void)
{
	return (thread_initialized);
}

thread_t *
thread_alloc(void)
{
	int		i;
	thread_t	*td;

	for (i = 0; i < MAX_THREADS; i++) {
		if (!thread_table[i].used) {
			td = &thread_table[i];
			memset(td, 0, sizeof(thread_t));
			td->used = 1;
			td->tid = next_tid++;
			return (td);
		}
	}
	return (NULL);
}

thread_t *
thread_create(process_t *proc, u64 rip, u64 rsp, u64 cs, u64 ss)
{
	thread_t	*td;
	u8		*kstack;

	if (!proc) {
		return (NULL);
	}

	td = thread_alloc();
	if (!td) {
		printk("[THREAD] Error: no free thread slots\n");
		return (NULL);
	}

	kstack = (u8 *)kmem_alloc_aligned(KERNEL_STACK_SIZE, 16);
	if (!kstack) {
		printk("[THREAD] Error: failed to allocate "
		    "kernel stack\n");
		td->used = 0;
		return (NULL);
	}
	memset(kstack, 0, KERNEL_STACK_SIZE);

	td->proc = proc;
	td->state = PROC_STATE_EMBRYO;
	td->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);

	memset(&td->context, 0, sizeof(cpu_context_t));
	td->context.rip = rip;
	td->context.cs = cs;
	td->context.rflags = 0x202;
	if ((cs & 3) == 3)
		td->context.rsp = rsp - 8;
	else
		td->context.rsp = rsp;
	td->context.ss = ss;

	td->wait_channel = NULL;
	td->next = NULL;
	td->prev = NULL;
	td->exit_code = 0;
	td->fs_base = 0;
	td->trace_last_tsc = 0;
	td->trace_runtime_cycles = 0;
	td->trace_switches = 0;
	td->running_cpu = -1;
	td->fpu_valid = 0;
	thread_fpu_init_context(&td->fpu_context);
	if (entity_is_initialized()) {
		char	name[64];

		td->entity = entity_attach(ENTITY_ARCH_THREAD,
		    (u32)(td - thread_table), 0, proc->pid, proc->uid,
		    proc->gid, proc->euid, proc->egid,
		    proc->kusr_auth);
		if (td->entity != 0) {
			snprintf(name, sizeof(name), "/Entity/Thread/%u",
			    td->tid);
			entity_ns_bind(name, td->entity);
		}
	}

	td->state = PROC_STATE_RUNNABLE;

	thread_link(proc, td);

	printk("[THREAD] Created thread tid=%d for PID %d "
	    "rip=%p\n", td->tid, proc->pid, (void *)rip);

	return (td);
}

static void
thread_link(process_t *proc, thread_t *td)
{
	if (!proc || !td) {
		return;
	}

	td->prev = NULL;
	td->next = proc->thread_list;
	if (proc->thread_list) {
		proc->thread_list->prev = td;
	}
	proc->thread_list = td;
	proc->thread_count++;
}

static void
thread_unlink(thread_t *td)
{
	process_t	*proc;

	if (!td || !td->proc) {
		return;
	}

	proc = td->proc;

	if (td->prev) {
		td->prev->next = td->next;
	} else {
		proc->thread_list = td->next;
	}
	if (td->next) {
		td->next->prev = td->prev;
	}

	proc->thread_count--;
	if (proc->thread_count < 0) {
		proc->thread_count = 0;
	}
}

thread_t *
thread_current(void)
{
	return (smp_current_thread());
}

void
thread_set_current(thread_t *td)
{
	smp_set_current_thread(td);
	if (td) {
		td->state = PROC_STATE_RUNNING;
		td->running_cpu = smp_cpu_index();
		if (td->proc) {
			td->proc->cur_thread = td;
			td->proc->last_cpu = td->running_cpu;
		}
		tss_set_rsp0(td->kernel_stack);
	}
}

void
thread_save_context(thread_t *td, registers_t *regs)
{
	if (!td || !regs) {
		return;
	}

	td->context.r15 = regs->r15;
	td->context.r14 = regs->r14;
	td->context.r13 = regs->r13;
	td->context.r12 = regs->r12;
	td->context.r11 = regs->r11;
	td->context.r10 = regs->r10;
	td->context.r9 = regs->r9;
	td->context.r8 = regs->r8;
	td->context.rbp = regs->rbp;
	td->context.rdi = regs->rdi;
	td->context.rsi = regs->rsi;
	td->context.rdx = regs->rdx;
	td->context.rcx = regs->rcx;
	td->context.rbx = regs->rbx;
	td->context.rax = regs->rax;
	td->context.rip = regs->rip;
	td->context.cs = regs->cs;
	td->context.rflags = regs->rflags;
	td->context.rsp = regs->rsp;
	td->context.ss = regs->ss;
	td->fs_base = thread_rdmsr(MSR_FS_BASE);
	thread_fpu_save(td);
}

void
thread_load_context(thread_t *td, registers_t *regs)
{
	if (!td || !regs) {
		return;
	}

	regs->r15 = td->context.r15;
	regs->r14 = td->context.r14;
	regs->r13 = td->context.r13;
	regs->r12 = td->context.r12;
	regs->r11 = td->context.r11;
	regs->r10 = td->context.r10;
	regs->r9 = td->context.r9;
	regs->r8 = td->context.r8;
	regs->rbp = td->context.rbp;
	regs->rdi = td->context.rdi;
	regs->rsi = td->context.rsi;
	regs->rdx = td->context.rdx;
	regs->rcx = td->context.rcx;
	regs->rbx = td->context.rbx;
	regs->rax = td->context.rax;
	regs->rip = td->context.rip;
	regs->cs = td->context.cs;
	regs->rflags = td->context.rflags;
	regs->rsp = td->context.rsp;
	regs->ss = td->context.ss;
	thread_wrmsr(MSR_FS_BASE, td->fs_base);
	thread_load_fpu_context(td);
}

void
thread_load_fpu_context(thread_t *td)
{
	if (!td) {
		return;
	}

	if (!td->fpu_valid) {
		thread_fpu_init_context(&td->fpu_context);
		td->fpu_valid = 1;
	}
	__asm__ volatile("fxrstor64 %0" :: "m"(td->fpu_context) : "memory");
}

void
thread_copy_fpu_context(thread_t *dst, thread_t *src)
{
	if (!dst || !src) {
		return;
	}

	if (src == thread_current()) {
		thread_fpu_save(src);
	}
	if (!src->fpu_valid) {
		thread_fpu_init_context(&dst->fpu_context);
		dst->fpu_valid = 0;
		return;
	}
	memcpy(&dst->fpu_context, &src->fpu_context, sizeof(dst->fpu_context));
	dst->fpu_valid = src->fpu_valid;
}

thread_t *
thread_get_by_proc(process_t *proc)
{
	if (!proc) {
		return (NULL);
	}
	return (proc->main_thread);
}

thread_t *
thread_get(u32 tid)
{
	int	i;

	for (i = 0; i < MAX_THREADS; i++) {
		if (thread_table[i].used &&
		    thread_table[i].tid == tid) {
			return (&thread_table[i]);
		}
	}
	return (NULL);
}

void
thread_destroy(thread_t *td)
{
	if (!td || !td->used) {
		return;
	}

	thread_unlink(td);

	if (td->kernel_stack) {
		u8	*kstack_base;

		kstack_base = (u8 *)(td->kernel_stack -
		    KERNEL_STACK_SIZE);
		kmem_free(kstack_base);
	}

	if (td->entity != 0) {
		entity_destroy(td->entity);
		td->entity = 0;
	}
	memset(td, 0, sizeof(thread_t));
}

void
thread_exit(int code)
{
	thread_t	*td;
	process_t	*proc;

	td = thread_current();
	if (!td) {
		return;
	}

	proc = td->proc;

	printk("[THREAD] tid=%d (PID %d) exiting code=%d\n",
	    td->tid, proc ? (int)proc->pid : 0, code);

	td->exit_code = code;
	td->state = PROC_STATE_ZOMBIE;
	td->running_cpu = smp_cpu_index();

	/* If set_tid_address was called, clear the TID field
	 * and do a futex wake so anyone waiting on it wakes up */
	if (td->tid_address) {
		u32	*tid_ptr;

		tid_ptr = (u32 *)td->tid_address;
		*tid_ptr = 0;
		futex_wake_all(td->tid_address);
	}

	/* Wake up anyone joining this thread */
	proc_wakeup((void *)td);

	/* If this was the last alive thread, kill the whole process */
	if (proc && thread_count_alive(proc) == 0) {
		printk("[THREAD] last thread exited, "
		    "process PID %d terminating\n",
		    (int)proc->pid);
		process_exit(code);
	}

	while (1) {
		process_yield();
	}
}

int
thread_join(u32 tid, int *status)
{
	thread_t	*td;
	int		i;

	td = thread_get(tid);
	if (!td) {
		return (-1);
	}

	/* Wait for the thread to become zombie */
	for (i = 0; i < 100000; i++) {
		if (td->state == PROC_STATE_ZOMBIE) {
			break;
		}
		proc_sleep((void *)td);
	}

	if (td->state != PROC_STATE_ZOMBIE) {
		return (-1);
	}

	if (status) {
		*status = td->exit_code;
	}

	int	reaped_tid;

	reaped_tid = (int)td->tid;
	thread_destroy(td);
	return (reaped_tid);
}

int
thread_count_alive(process_t *proc)
{
	thread_t	*td;
	int		count;

	if (!proc) {
		return (0);
	}

	count = 0;
	for (td = proc->thread_list; td; td = td->next) {
		if (td->state != PROC_STATE_ZOMBIE &&
		    td->state != PROC_STATE_UNUSED) {
			count++;
		}
	}
	return (count);
}

void
thread_kill_all(process_t *proc)
{
	thread_t	*td, *next;

	if (!proc) {
		return;
	}

	td = proc->thread_list;
	while (td) {
		next = td->next;
		if (td != proc->cur_thread) {
			td->state = PROC_STATE_ZOMBIE;
			td->running_cpu = -1;
			td->exit_code = -1;
		}
		td = next;
	}
}
