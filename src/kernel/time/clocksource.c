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

#include <kernel/time/clocksource.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static struct timecounter	*timecounter_list = NULL;
static struct timecounter	*timecounter_current = NULL;

static int
quality_is_better(struct timecounter *a, struct timecounter *b)
{
	if (a == NULL) {
		return (0);
	}
	if (b == NULL) {
		return (1);
	}
	return (a->tc_quality > b->tc_quality);
}

static struct timecounter *
find_best(void)
{
	struct timecounter	*tc;
	struct timecounter	*best;

	best = NULL;
	for (tc = timecounter_list; tc != NULL; tc = tc->tc_next) {
		if (quality_is_better(tc, best)) {
			best = tc;
		}
	}
	return (best);
}

void
tc_register(struct timecounter *tc)
{
	struct timecounter	*tc_best_new;

	if (tc == NULL || tc->tc_get_timecount == NULL) {
		com1_printf("[TIMECOUNTER] refusing to register "
		    "invalid source\n");
		return;
	}

	if (tc->tc_counter_mask == 0) {
		tc->tc_counter_mask = ~0ULL;
	}

	if (tc->tc_frequency == 0) {
		com1_printf("[TIMECOUNTER] %s has zero frequency, "
		    "refusing registration\n", tc->tc_name);
		return;
	}
	tc->tc_next = timecounter_list;
	timecounter_list = tc;

	com1_printf("[TIMECOUNTER] registered %s: freq=", tc->tc_name);
	com1_write_dec(tc->tc_frequency);
	com1_printf(" Hz, quality=%d\n", tc->tc_quality);
	tc_best_new = find_best();
	if (tc_best_new != timecounter_current) {
		if (timecounter_current != NULL) {
			com1_printf("[TIMECOUNTER] switching %s -> %s\n",
			    timecounter_current->tc_name,
			    tc_best_new->tc_name);
		} else {
			com1_printf("[TIMECOUNTER] selecting %s\n",
			    tc_best_new->tc_name);
		}
		timecounter_current = tc_best_new;
	}
}

void
tc_deregister(struct timecounter *tc)
{
	struct timecounter	**tcpp;
	struct timecounter	*tc_best_new;

	if (tc == NULL) {
		return;
	}

	for (tcpp = &timecounter_list; *tcpp != NULL;
	    tcpp = &(*tcpp)->tc_next) {
		if (*tcpp == tc) {
			*tcpp = tc->tc_next;
			break;
		}
	}

	if (timecounter_current == tc) {
		tc_best_new = find_best();
		timecounter_current = tc_best_new;
		if (tc_best_new != NULL) {
			com1_printf("[TIMECOUNTER] fallback to %s\n",
			    tc_best_new->tc_name);
		}
	}
}

struct timecounter *
tc_best(void)
{
	return (find_best());
}

struct timecounter *
tc_get_current(void)
{
	return (timecounter_current);
}
