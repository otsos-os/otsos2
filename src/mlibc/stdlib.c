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
$define %type process_t as struct with process control block

$define %func __stack_chk_fail as start with args void
$define %func sleep as procedure with args u32

*/

/* !SPACE!

$space %export __stack_chk_fail, sleep
$space %export __stack_chk_guard

*/

#include <kernel/drivers/timer.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <lib/com1.h>
#include <mlibc/stdlib.h>

u64	__stack_chk_guard = 0x595e9fbd94fda766ULL;

__attribute__((noreturn)) void
__stack_chk_fail(void)
{
	com1_write_string("[STACK] stack smashing detected\n");
	__asm__ volatile("cli");
	while (1) {
		__asm__ volatile("hlt");
	}
}

void
sleep(u32 ms)
{
	u64		start_ticks;
	u64		deadline;
	thread_t	*td;

	td = thread_current();

	if (!td || (td->context.cs & 3) == 0) {
		start_ticks = timer_get_ticks();
		while (timer_get_ticks() < start_ticks + ms) {
			__asm__ volatile("pause");
		}
		return;
	}

	deadline = timer_get_ticks() +
	    (u64)ms * timer_get_frequency() / 1000;
	while (timer_get_ticks() < deadline) {
		process_yield();
		if (timer_get_ticks() >= deadline) {
			break;
		}
	}
}
