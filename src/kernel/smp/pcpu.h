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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type thread_t as struct with per-thread CPU context and state
$define %type tss_t as struct with task state segment
$define %type pcpu_t as per CPU data with identity, current thread, nesting counters and witness stack
$define %type pcpu_idmode_t as enum with CPU identity read method

$define %func pcpu_init as procedure with args void
$define %func pcpu_attach as procedure with args int, u8
$define %func pcpu_id as function with args void
$define %func pcpu_current as function with args void
$define %func pcpu_by_index as function with args int
$define %func pcpu_idmode as function with args void
$define %func pcpu_idmode_name as function with args void
$define %func pcpu_is_ready as function with args void
$define %func pcpu_set_syscall_stack as procedure with args u64
$define %func pcpu_syscall_stack as function with args void
$define %func pcpu_reload_gsbase as procedure with args void

$const PCPU_OFF_SYSCALL_SCRATCH as byte offset of syscall_scratch inside pcpu_t as seen from syscall_asm.asm
$const PCPU_OFF_SYSCALL_STACK as byte offset of syscall_stack inside pcpu_t as seen from syscall_asm.asm
$const MSR_GS_BASE as 0xC0000101
$const MSR_KERNEL_GS_BASE as 0xC0000102

$const PCPU_WITNESS_DEPTH as ceiling on nested tracked locks per CPU
$const PCPU_SPIN_DEPTH as ceiling on nested spin mutexes per CPU

*/

/* !SPACE!

$space %export pcpu_init, pcpu_attach, pcpu_id, pcpu_current
$space %export pcpu_by_index, pcpu_idmode, pcpu_idmode_name, pcpu_is_ready
$space %export pcpu_set_syscall_stack, pcpu_syscall_stack
$space %export pcpu_reload_gsbase

*/

#ifndef KERNEL_SMP_PCPU_H
#define KERNEL_SMP_PCPU_H

#include <kernel/gdt.h>
#include <mlibc/mlibc.h>

#define	PCPU_MAX_CPUS		32
#define	PCPU_WITNESS_DEPTH	16
#define	PCPU_SPIN_DEPTH		16
#define	PCPU_CACHELINE		64

#define	PCPU_OFF_SYSCALL_SCRATCH	8
#define	PCPU_OFF_SYSCALL_STACK		16

#define	MSR_GS_BASE		0xC0000101
#define	MSR_KERNEL_GS_BASE	0xC0000102

struct thread;

typedef enum pcpu_idmode {
	PCPU_ID_BOOT = 0,
	PCPU_ID_RDPID,
	PCPU_ID_RDTSCP,
	PCPU_ID_LAPIC
} pcpu_idmode_t;

typedef struct pcpu {
	struct thread	*curthread;
	u64		syscall_scratch;
	u64		syscall_stack;
	struct thread	*idlethread;
	tss_t		*tss;
	u64		stack_top;
	u64		ipi_count;
	u64		switch_count;
	u32		cpu_index;
	u32		lapic_id;
	u32		critnest;
	u32		spin_held;
	u32		preempt_pending;
	u32		runnable;
	u32		online;
	u32		witness_depth;
	u32		witness_level;
	u64		spin_flags[PCPU_SPIN_DEPTH];
	const char	*witness_name[PCPU_WITNESS_DEPTH];
	u32		witness_stack[PCPU_WITNESS_DEPTH];
} __attribute__((aligned(PCPU_CACHELINE))) pcpu_t;

_Static_assert(__builtin_offsetof(pcpu_t, syscall_scratch) ==
    PCPU_OFF_SYSCALL_SCRATCH,
    "PCPU_OFF_SYSCALL_SCRATCH out of sync with pcpu_t, fix syscall_asm.asm");
_Static_assert(__builtin_offsetof(pcpu_t, syscall_stack) ==
    PCPU_OFF_SYSCALL_STACK,
    "PCPU_OFF_SYSCALL_STACK out of sync with pcpu_t, fix syscall_asm.asm");

extern pcpu_t		pcpu_data[PCPU_MAX_CPUS];
extern pcpu_idmode_t	pcpu_id_mode;
extern u8		pcpu_lapic_to_index[256];

void		pcpu_init(void);
void		pcpu_attach(int cpu_index, u8 lapic_id);
int		pcpu_is_ready(void);
pcpu_idmode_t	pcpu_idmode(void);
const char	*pcpu_idmode_name(void);
u32		pcpu_id_lapic(void);
void		pcpu_set_syscall_stack(u64 stack_top);
u64		pcpu_syscall_stack(void);
void		pcpu_reload_gsbase(void);

static inline u32
pcpu_id(void)
{
	u32	aux;

	switch (pcpu_id_mode) {
	case PCPU_ID_RDPID:
		__asm__ volatile(".byte 0xf3,0x0f,0xc7,0xf8"
		    : "=a"(aux) :: "memory");
		return (aux & (PCPU_MAX_CPUS - 1));
	case PCPU_ID_RDTSCP:
		__asm__ volatile("rdtscp" : "=c"(aux) :: "rax", "rdx");
		return (aux & (PCPU_MAX_CPUS - 1));
	case PCPU_ID_LAPIC:
		return (pcpu_id_lapic());
	case PCPU_ID_BOOT:
	default:
		return (0);
	}
}

static inline pcpu_t *
pcpu_current(void)
{
	return (&pcpu_data[pcpu_id()]);
}

static inline pcpu_t *
pcpu_by_index(int cpu_index)
{
	if (cpu_index < 0 || cpu_index >= PCPU_MAX_CPUS) {
		return (NULL);
	}
	return (&pcpu_data[cpu_index]);
}

#endif
