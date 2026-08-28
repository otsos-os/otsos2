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
$define %type u8 as 8 bit unsigned
$define %type int as 32 bit signed
$define %type cpu_context_t as struct with saved CPU registers for context switch
$define %type fpu_context_t as aligned 512 byte FXSAVE image
$define %type process_state_t as enum with thread scheduling states
$define %type thread_t as struct with per-thread CPU context, kernel stack, state, wait channel
$define %type registers_t as struct with CPU register snapshot from interrupt

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
$define %func thread_destroy as procedure with args thread_t *
$define %func thread_is_initialized as function with args void
$define %func thread_get as function with args u32
$define %func thread_exit as procedure with args int
$define %func thread_join as function with args u32, int *
$define %func thread_count_alive as function with args process_t *
$define %func thread_kill_all as procedure with args process_t *
$define %func thread_mark_dead as procedure with args thread_t *
$define %func thread_has_dead as function with args void
$define %func thread_retired_dead as procedure with args void

*/

/* !SPACE!

$space %export thread_init, thread_alloc, thread_create
$space %export thread_current, thread_set_current
$space %export thread_save_context, thread_load_context
$space %export thread_load_fpu_context, thread_copy_fpu_context
$space %export thread_get_by_proc, thread_destroy
$space %export thread_is_initialized, thread_get
$space %export thread_exit, thread_join
$space %export thread_count_alive, thread_kill_all
$space %export thread_mark_dead, thread_has_dead, thread_retired_dead

*/

#ifndef THREAD_H
#define THREAD_H

#include <kernel/interrupts/idt.h>
#include <kernel/entity/entity.h>
#include <mlibc/mlibc.h>

#define MAX_THREADS 256
#define FPU_FXSAVE_SIZE 512
#define THREAD_MAX_APCS 16

struct process;

/* CPU context saved during context switch */
typedef struct {
	u64	r15, r14, r13, r12, r11, r10, r9, r8;
	u64	rbp, rdi, rsi, rdx, rcx, rbx, rax;
	u64	rip;
	u64	cs;
	u64	rflags;
	u64	rsp;
	u64	ss;
} __attribute__((packed)) cpu_context_t;

typedef struct {
	u8	bytes[FPU_FXSAVE_SIZE];
} __attribute__((aligned(16))) fpu_context_t;

typedef enum {
	PROC_STATE_UNUSED = 0,
	PROC_STATE_EMBRYO,
	PROC_STATE_RUNNABLE,
	PROC_STATE_RUNNING,
	PROC_STATE_SLEEPING,
	PROC_STATE_TERMINATED
} process_state_t;

typedef struct thread {
	int			used;
	u32			tid;
	u64			entity;
	struct process		*proc;
	process_state_t		state;
	cpu_context_t		context;
	fpu_context_t		fpu_context;
	u64			kernel_stack;
	void			*wait_channel;
	cpu_context_t		saved_context;
	u64			saved_sigmask;
	int			exit_code;
	u64			tid_address;	/* set_tid_address: cleared on exit */
	u64			fs_base;	/* FS segment base (TLS) */
	u64			sleep_target_ticks; /* nanosleep wakeup target */
	u64			trace_last_tsc;
	u64			trace_runtime_cycles;
	u64			trace_switches;
	int			running_cpu;	/* -1 when not running on any CPU */
	int			fpu_valid;
	int			apc_head;
	int			apc_count;
	int			apc_alertable;
	int			apc_in_user;
	cpu_context_t		apc_saved_context;
	fpu_context_t		apc_saved_fpu;
	struct thread		*next;
	struct thread		*prev;
} thread_t;

void		thread_init(void);
int		thread_is_initialized(void);
thread_t	*thread_alloc(void);
thread_t	*thread_create(struct process *proc, u64 rip, u64 rsp,
			    u64 cs, u64 ss);
thread_t	*thread_current(void);
void		thread_set_current(thread_t *td);
void		thread_save_context(thread_t *td, registers_t *regs);
void		thread_load_context(thread_t *td, registers_t *regs);
void		thread_load_fpu_context(thread_t *td);
void		thread_copy_fpu_context(thread_t *dst, thread_t *src);
thread_t	*thread_get_by_proc(struct process *proc);
thread_t	*thread_get(u32 tid);
void		thread_destroy(thread_t *td);
void		thread_exit(int code);
int		thread_join(u32 tid, int *status);
int		thread_count_alive(struct process *proc);
void		thread_kill_all(struct process *proc);
void		thread_mark_dead(thread_t *td);
int		thread_has_dead(void);
void		thread_retired_dead(void);

/*
 * AoSoA: threads and their entity metadata live in one block so the
 * objects and metadata share the same allocation and cache lines.
 */
typedef struct thread_block {
	entity_meta_block_t	meta;
	thread_t		threads[MAX_THREADS];
} thread_block_t;

extern thread_block_t thread_block;
#define thread_table (thread_block.threads)
extern u32	next_tid;

#endif
