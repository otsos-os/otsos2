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

$define %func critical_enter as procedure with args void
$define %func critical_exit as procedure with args void
$define %func critical_nest as function with args void
$define %func critical_preempt_pending as function with args void
$define %func critical_defer_preempt as procedure with args void

$const IRQ_VECTOR_YIELD as software interrupt vector that drives a voluntary switch

*/

/* !SPACE!

$space %export critical_enter, critical_exit, critical_nest
$space %export critical_preempt_pending, critical_defer_preempt

*/

#include <kernel/interrupts/irq.h>
#include <kernel/panic.h>
#include <kernel/smp/pcpu.h>
#include <kernel/sync/sync.h>

void
critical_enter(void)
{
	pcpu_t	*pc;

	pc = pcpu_current();
	pc->critnest++;
	__atomic_signal_fence(__ATOMIC_SEQ_CST);
}

void
critical_exit(void)
{
	pcpu_t	*pc;

	pc = pcpu_current();
	if (pc->critnest == 0) {
		panic("[SYNC] critical_exit with critnest 0 on cpu %u\n",
		    pc->cpu_index);
	}
	__atomic_signal_fence(__ATOMIC_SEQ_CST);
	pc->critnest--;
	if (pc->critnest != 0 || pc->spin_held != 0) {
		return;
	}
	if (pc->preempt_pending == 0) {
		return;
	}
	pc->preempt_pending = 0;
	__asm__ volatile("int %0" :: "i"(IRQ_VECTOR_YIELD) : "memory");
}

u32
critical_nest(void)
{
	return (pcpu_current()->critnest);
}

int
critical_preempt_pending(void)
{
	return (pcpu_current()->preempt_pending != 0);
}

void
critical_defer_preempt(void)
{
	pcpu_current()->preempt_pending = 1;
}
