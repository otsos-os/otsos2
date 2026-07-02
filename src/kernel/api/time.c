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

#include <kernel/api/api.h>
#include <kernel/time.h>
#include <kernel/time/clocksource.h>
#include <kernel/drivers/timer.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static u64
bintime_to_nsec_frac(u64 frac)
{
	return ((unsigned __int128)frac * NSEC_PER_SEC >> 64);
}

int
api_timeinfo(struct api_timeinfo *buf)
{
	struct bintime	bt;
	struct bintime	bu;
	struct timecounter *tc;

	if (buf == NULL) {
		return (-API_ERR_BAD_ADDR);
	}

	if (!is_user_address(buf, sizeof(struct api_timeinfo))) {
		return (-API_ERR_BAD_ADDR);
	}

	bintime(&bt);
	binuptime(&bu);

	buf->wall_sec = bt.sec;
	buf->wall_nsec = bintime_to_nsec_frac(bt.frac);
	buf->local_sec = bt.sec + time_get_offset();
	buf->local_nsec = buf->wall_nsec;
	buf->uptime_sec = bu.sec;
	buf->uptime_nsec = bintime_to_nsec_frac(bu.frac);
	buf->ticks = timer_get_ticks();
	buf->frequency = timer_get_frequency();
	buf->timezone_offset = time_get_offset();

	tc = tc_get_current();
	if (tc != NULL) {
		int	len;

		len = strlen(tc->tc_name);
		if (len > (int)sizeof(buf->clocksource) - 1) {
			len = (int)sizeof(buf->clocksource) - 1;
		}
		memcpy(buf->clocksource, tc->tc_name, len);
		buf->clocksource[len] = '\0';
	} else {
		buf->clocksource[0] = '\0';
	}

	return (0);
}

int
api_time(void)
{
	return ((int)time_second);
}
