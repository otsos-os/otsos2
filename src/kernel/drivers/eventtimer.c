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

$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type struct eventtimer as event timer descriptor

$define %func et_register as function with args struct eventtimer *
$define %func et_deregister as function with args struct eventtimer *
$define %func et_change_frequency as procedure with args struct eventtimer *, u64
$define %func et_find as function with args const char *, int, int
$define %func et_init as function with args struct eventtimer *, et_event_cb_t *, et_deregister_cb_t *, void *
$define %func et_start as function with args struct eventtimer *, u64, u64
$define %func et_stop as function with args struct eventtimer *
$define %func et_ban as function with args struct eventtimer *
$define %func et_free as function with args struct eventtimer *
$define %func eventtimer_dispatch as procedure with args void

$define %const ET_MAX as 8

*/

/* !SPACE!

$space %internal et_list_insert, et_list_remove
$space %export et_register, et_deregister, et_change_frequency
$space %export et_find, et_init, et_start, et_stop, et_ban, et_free
$space %export eventtimer_dispatch

*/

#include <kernel/drivers/eventtimer.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define ET_MAX	8

static struct eventtimer	*et_list[ET_MAX];
static int			et_count;

static void
et_list_insert(struct eventtimer *et)
{
	int	i, j;

	for (i = 0; i < et_count; i++) {
		if (et_list[i]->et_quality < et->et_quality)
			break;
	}
	for (j = et_count; j > i; j--)
		et_list[j] = et_list[j - 1];
	et_list[i] = et;
	et_count++;
}

static int
et_list_remove(struct eventtimer *et)
{
	int	i;

	for (i = 0; i < et_count; i++) {
		if (et_list[i] != et)
			continue;
		for (; i < et_count - 1; i++)
			et_list[i] = et_list[i + 1];
		et_count--;
		return (0);
	}
	return (-1);
}

int
et_register(struct eventtimer *et)
{
	ET_LOCK();
	if (et == NULL || et->et_name == NULL || et->et_start == NULL) {
		ET_UNLOCK();
		return (-1);
	}
	if (et_count >= ET_MAX) {
		ET_UNLOCK();
		com1_printf("[EVENTTIMER] too many event timers\n");
		return (-1);
	}
	et_list_insert(et);
	ET_UNLOCK();

	if (et->et_frequency == 0) {
		com1_printf("[EVENTTIMER] registered \"%s\" quality %d\n",
		    et->et_name, et->et_quality);
	} else {
		com1_printf("[EVENTTIMER] registered \"%s\" "
		    "frequency %u Hz quality %d\n", et->et_name,
		    (u32)et->et_frequency, et->et_quality);
	}
	return (0);
}

int
et_deregister(struct eventtimer *et)
{
	int	err;

	if (et == NULL)
		return (-1);

	if (et->et_deregister_cb != NULL) {
		err = et->et_deregister_cb(et, et->et_arg);
		if (err != 0)
			return (err);
	}

	ET_LOCK();
	err = et_list_remove(et);
	ET_UNLOCK();
	return (err);
}

void
et_change_frequency(struct eventtimer *et, u64 newfreq)
{
	u64	first, period;

	if (et == NULL || newfreq == 0)
		return;

	first = et->et_first;
	period = et->et_period;

	if (et->et_active) {
		if (et->et_stop != NULL)
			et->et_stop(et);
		et->et_frequency = newfreq;
		if (period != 0 || first != 0)
			et->et_start(et, first, period);
	} else {
		et->et_frequency = newfreq;
	}
}

struct eventtimer *
et_find(const char *name, int check, int want)
{
	struct eventtimer	*et;
	int			i;

	ET_LOCK();
	for (i = 0; i < et_count; i++) {
		et = et_list[i];
		if (et->et_active)
			continue;
		if (name != NULL && strcmp(et->et_name, name) != 0)
			continue;
		if (name == NULL && et->et_quality < 0)
			continue;
		if ((et->et_flags & check) != want)
			continue;
		ET_UNLOCK();
		return (et);
	}
	ET_UNLOCK();
	return (NULL);
}

int
et_init(struct eventtimer *et, et_event_cb_t *event,
    et_deregister_cb_t *deregister, void *arg)
{
	if (et == NULL || event == NULL)
		return (-1);
	if (et->et_active)
		return (-1);

	et->et_active = 1;
	et->et_event_cb = event;
	et->et_deregister_cb = deregister;
	et->et_arg = arg;
	return (0);
}

int
et_start(struct eventtimer *et, u64 first, u64 period)
{
	if (et == NULL || !et->et_active)
		return (-1);
	if (period == 0 && first == 0)
		return (-1);
	if (period != 0 && !(et->et_flags & ET_FLAGS_PERIODIC))
		return (-1);
	if (period == 0 && !(et->et_flags & ET_FLAGS_ONESHOT))
		return (-1);

	if (period != 0) {
		if (period < et->et_min_period)
			period = et->et_min_period;
		else if (period > et->et_max_period)
			period = et->et_max_period;
	}
	if (first != 0) {
		if (first < et->et_min_period)
			first = et->et_min_period;
		else if (first > et->et_max_period)
			first = et->et_max_period;
	}

	et->et_first = first;
	et->et_period = period;
	return (et->et_start(et, first, period));
}

int
et_stop(struct eventtimer *et)
{
	if (et == NULL || !et->et_active)
		return (-1);
	if (et->et_stop != NULL)
		return (et->et_stop(et));
	return (0);
}

int
et_ban(struct eventtimer *et)
{
	if (et == NULL)
		return (-1);
	et->et_flags &= ~(ET_FLAGS_PERIODIC | ET_FLAGS_ONESHOT);
	return (0);
}

int
et_free(struct eventtimer *et)
{
	if (et == NULL || !et->et_active)
		return (-1);
	et->et_active = 0;
	return (0);
}

void
eventtimer_dispatch(void)
{
	struct eventtimer	*et;
	int			i;

	for (i = 0; i < et_count; i++) {
		et = et_list[i];
		if (et->et_active && et->et_event_cb != NULL) {
			et->et_event_cb(et, et->et_arg);
			return;
		}
	}
}
