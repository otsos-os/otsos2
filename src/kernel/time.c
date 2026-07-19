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

#include <kernel/time.h>
#include <kernel/time/clocksource.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/rtc/rtc.h>
#include <kernel/event/event.h>
#include <kernel/cm/cm.h>
#include <kernel/thread.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static struct bintime	boottime;

static struct timehands	th0;
struct timehands	*timehands = &th0;

volatile u64		time_second;
volatile u64		time_uptime;

static int		time_initialized;
static s64		time_zone_offset;


#define	BINTIME_NS_SCALE	((~0ULL / NSEC_PER_SEC) + 1)
#define	BINTIME_US_SCALE	((~0ULL / USEC_PER_SEC) + 1)

static void
bintime_add_delta(struct bintime *bt, u64 delta, u64 scale)
{
	unsigned __int128	prod;
	u64			frac_inc;
	u64			sec_inc;

	prod = (unsigned __int128)delta * scale;
	sec_inc = (u64)(prod >> 64);
	frac_inc = (u64)prod;

	bt->sec += sec_inc;
	bt->frac += frac_inc;
	if (bt->frac < frac_inc) {
		bt->sec++;
	}
}

static void
bintime_from_counter(struct bintime *bt)
{
	struct timehands	*th;
	struct timecounter	*tc;
	u64			now;
	u64			delta;

	th = timehands;
	tc = th->th_counter;
	if (tc == NULL) {
		bt->sec = 0;
		bt->frac = 0;
		return;
	}

	now = tc->tc_get_timecount(tc);
	delta = (now - th->th_offset_count) & th->th_counter_mask;

	*bt = th->th_offset;
	bintime_add_delta(bt, delta, th->th_scale);
}

void
bintime_add(struct bintime *bt, const struct bintime *bt2)
{
	u64	frac;

	frac = bt->frac;
	bt->frac += bt2->frac;
	bt->sec += bt2->sec;
	if (bt->frac < frac) {
		bt->sec++;
	}
}

void
bintime_sub(struct bintime *bt, const struct bintime *bt2)
{
	u64	frac;

	frac = bt->frac;
	bt->frac -= bt2->frac;
	bt->sec -= bt2->sec;
	if (bt->frac > frac) {
		bt->sec--;
	}
}

void
bintime_add_ns(struct bintime *bt, u64 ns)
{
	unsigned __int128	prod;
	u64			frac_inc;
	u64			sec_inc;

	prod = (unsigned __int128)ns * BINTIME_NS_SCALE;
	sec_inc = (u64)(prod >> 64);
	frac_inc = (u64)prod;

	bt->sec += sec_inc;
	bt->frac += frac_inc;
	if (bt->frac < frac_inc) {
		bt->sec++;
	}
}

void
bintime_sub_ns(struct bintime *bt, u64 ns)
{
	unsigned __int128	prod;
	u64			frac_dec;
	u64			sec_dec;
	u64			frac;

	prod = (unsigned __int128)ns * BINTIME_NS_SCALE;
	sec_dec = (u64)(prod >> 64);
	frac_dec = (u64)prod;

	bt->sec -= sec_dec;
	frac = bt->frac;
	bt->frac -= frac_dec;
	if (bt->frac > frac) {
		bt->sec--;
	}
}

static void
bintime_to_timespec(const struct bintime *bt, struct timespec *ts)
{
	ts->tv_sec = bt->sec;
	ts->tv_nsec = (long)((unsigned __int128)bt->frac * NSEC_PER_SEC >>
	    64);
}

static void
bintime_to_timeval(const struct bintime *bt, struct timeval *tv)
{
	tv->tv_sec = bt->sec;
	tv->tv_usec = (long)((unsigned __int128)bt->frac * USEC_PER_SEC >>
	    64);
}

void
binuptime(struct bintime *bt)
{
	bintime_from_counter(bt);
}

void
nanouptime(struct timespec *ts)
{
	struct bintime	bt;

	binuptime(&bt);
	bintime_to_timespec(&bt, ts);
}

void
microuptime(struct timeval *tv)
{
	struct bintime	bt;

	binuptime(&bt);
	bintime_to_timeval(&bt, tv);
}

void
bintime(struct bintime *bt)
{
	binuptime(bt);
	bintime_add(bt, &boottime);
}

void
nanotime(struct timespec *ts)
{
	struct bintime	bt;

	bintime(&bt);
	bintime_to_timespec(&bt, ts);
}

void
microtime(struct timeval *tv)
{
	struct bintime	bt;

	bintime(&bt);
	bintime_to_timeval(&bt, tv);
}

void
getbinuptime(struct bintime *bt)
{
	binuptime(bt);
}

void
getnanouptime(struct timespec *ts)
{
	nanouptime(ts);
}

void
getmicrouptime(struct timeval *tv)
{
	microuptime(tv);
}

void
getbintime(struct bintime *bt)
{
	bintime(bt);
}

void
getnanotime(struct timespec *ts)
{
	nanotime(ts);
}

void
getmicrotime(struct timeval *tv)
{
	microtime(tv);
}

static void
time_check_sleepers(void)
{
	thread_t	*td;
	u64		now;
	int		i;

	now = timer_get_ticks();
	for (i = 0; i < MAX_THREADS; i++) {
		td = &thread_table[i];
		if (!td->used || td->state != PROC_STATE_SLEEPING) {
			continue;
		}
		if (td->sleep_target_ticks == 0) {
			continue;
		}
		if (td->sleep_target_ticks <= now) {
			td->sleep_target_ticks = 0;
			proc_wakeup_one(td->wait_channel);
		}
	}
}

void
time_tick(void)
{
	struct timehands	*th;
	struct timecounter	*tc;
	u64			now;
	u64			delta;

	if (!time_initialized) {
		return;
	}

	th = timehands;
	tc = th->th_counter;
	if (tc == NULL) {
		return;
	}

	now = tc->tc_get_timecount(tc);
	delta = (now - th->th_offset_count) & th->th_counter_mask;

	bintime_add_delta(&th->th_offset, delta, th->th_scale);
	th->th_offset_count = now;

	time_uptime = th->th_offset.sec;
	time_second = boottime.sec + time_uptime;

	time_check_sleepers();
}

static void
time_windup(void)
{
	struct timecounter	*tc;
	struct timehands	*th;
	struct bintime		bt;

	th = timehands;
	tc = tc_get_current();
	if (tc == NULL) {
		return;
	}

	if (th->th_counter != tc) {
		if (th->th_counter != NULL) {
			bintime_from_counter(&bt);
			th->th_offset = bt;
		}
		th->th_counter = tc;
		th->th_offset_count = tc->tc_get_timecount(tc);
		th->th_counter_mask = tc->tc_counter_mask;
		th->th_scale = (~0ULL / tc->tc_frequency) + 1;
		printk("[TIME] windup with %s: scale=0x%lx\n",
		    tc->tc_name, th->th_scale);
	}
}

s64
time_get_offset(void)
{
	return (time_zone_offset);
}

void
time_set_offset(s64 offset)
{
	time_zone_offset = offset;
}

void
time_init(void)
{
	struct timecounter	*tc;
	int			zone_hours;

	if (time_initialized) {
		return;
	}

	if (rtc_read_time(&boottime) != 0) {
		boottime.sec = 0;
		boottime.frac = 0;
	}

	zone_hours = (int)cm_get_i32_default("SYSTEM", "Time",
	    "TimezoneOffset", 0);
	time_zone_offset = (s64)zone_hours * 3600;
	printk("[TIME] timezone offset: %d hours\n",
	    zone_hours);

	th0.th_offset.sec = 0;
	th0.th_offset.frac = 0;
	th0.th_offset_count = 0;
	th0.th_scale = 0;
	th0.th_counter_mask = 0;
	th0.th_counter = NULL;
	timehands = &th0;
	time_second = 0;
	time_uptime = 0;
	time_initialized = 1;

	tc = tc_get_current();
	if (tc != NULL) {
		time_windup();
		printk("[TIME] initialized with %s\n",
		    tc->tc_name);
	} else {
		printk("[TIME] initialized, waiting for "
		    "clocksource\n");
	}
}

void	time_windup_current(void);

void
time_windup_current(void)
{
	if (time_initialized) {
		time_windup();
	}
}
