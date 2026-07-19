/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %func apic_timer_start as static function with args struct eventtimer *, u64, u64
$define %func apic_timer_stop as static function with args struct eventtimer *
$define %func apic_timer_init as procedure with args void

*/

/* !SPACE!

$space %internal apic_timer_start, apic_timer_stop
$space %export apic_timer_init

*/

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/drivers/eventtimer.h>
#include <kernel/drivers/timer.h>
#include <kernel/cm/cm.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static struct eventtimer	apic_et;
static u64			apic_freq;

static int
apic_timer_start(struct eventtimer *et, u64 first, u64 period)
{
	u64	count;

	(void)et;
	(void)first;
	if (period != 0) {
		count = apic_freq * period / 1000000000ULL;
		if (count > 0xFFFFFFFFULL)
			count = 0xFFFFFFFFULL;
		if (count == 0)
			count = 1;
		lapic_timer_set(APIC_TIMER_VECTOR, 1);
		lapic_timer_set_count((u32)count);
	} else {
		count = apic_freq * first / 1000000000ULL;
		if (count > 0xFFFFFFFFULL)
			count = 0xFFFFFFFFULL;
		if (count == 0)
			count = 1;
		lapic_timer_set(APIC_TIMER_VECTOR, 0);
		lapic_timer_set_count((u32)count);
	}
	return (0);
}

static int
apic_timer_stop(struct eventtimer *et)
{

	(void)et;
	lapic_timer_stop();
	return (0);
}

void
apic_timer_init(void)
{
	int	enabled, quality;
	u64	max_ns;
	u32	timer_hz;

	enabled = cm_get_bool_default("SYSTEM", "Timer",
	    "ApicEnabled", 1);
	if (!enabled)
		return;
	if (!lapic_is_enabled())
		return;

	apic_freq = lapic_timer_calibrate();
	if (apic_freq == 0) {
		drivers_log("[APIC] calibration fail "
		    "let skip it?\n");
		return;
	}

	quality = (int)cm_get_u32_default("SYSTEM", "Timer",
	    "ApicQuality", 1000);
	max_ns = 0xFFFFFFFFULL * 1000000000ULL / apic_freq;

	apic_et.et_name = "LAPIC";
	apic_et.et_flags = ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT;
	apic_et.et_quality = quality;
	apic_et.et_frequency = apic_freq;
	apic_et.et_min_period = 1000000000ULL / apic_freq;
	if (apic_et.et_min_period < 1)
		apic_et.et_min_period = 1;
	apic_et.et_max_period = max_ns;
	apic_et.et_start = apic_timer_start;
	apic_et.et_stop = apic_timer_stop;
	apic_et.et_event_cb = NULL;
	apic_et.et_deregister_cb = NULL;
	apic_et.et_arg = NULL;
	apic_et.et_priv = NULL;

	et_register(&apic_et);

	timer_hz = cm_get_u32_default("SYSTEM", "Timer", "Hz", 1000);
	drivers_log("[APIC] switching to lapic timer %u with hz "
	    " %d\n", timer_hz, quality);
	timer_reinit(timer_hz);
}
