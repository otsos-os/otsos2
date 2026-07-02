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
$define %type struct eventtimer as event timer descriptor

$define %func timer_init as procedure with args u32
$define %func timer_get_ticks as function with args void
$define %func timer_is_initialized as function with args void
$define %func timer_get_frequency as function with args void

*/

/* !SPACE!

$space %internal timer_event_cb
$space %export timer_init, timer_get_ticks
$space %export timer_is_initialized, timer_get_frequency

*/

#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/timer.h>
#include <kernel/time.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static struct eventtimer	*timer_et;
static u64			timer_ticks;
static u32			timer_frequency;
static int			timer_initialized;

static void
timer_event_cb(struct eventtimer *et, void *arg)
{

	(void)et;
	(void)arg;
	timer_ticks++;
	time_tick();
}

void
timer_init(u32 frequency)
{
	struct eventtimer	*et;
	u64			period_ns;

	et = et_find(NULL, ET_FLAGS_PERIODIC, ET_FLAGS_PERIODIC);
	if (et == NULL) {
		com1_printf("[TIMER] no periodic event timer available\n");
		return;
	}

	if (et_init(et, timer_event_cb, NULL, NULL) != 0) {
		com1_printf("[TIMER] failed to init event timer\n");
		return;
	}

	period_ns = 1000000000ULL / frequency;
	if (et_start(et, 0, period_ns) != 0) {
		com1_printf("[TIMER] failed to start event timer\n");
		et_free(et);
		return;
	}

	timer_et = et;
	timer_frequency = frequency;
	timer_initialized = 1;
	com1_printf("[TIMER] using event timer \"%s\" at %u Hz\n",
	    et->et_name, timer_frequency);
}

u64
timer_get_ticks(void)
{
	return (timer_ticks);
}

int
timer_is_initialized(void)
{
	return (timer_initialized);
}

u32
timer_get_frequency(void)
{
	return (timer_frequency);
}
