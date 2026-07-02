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
$define %type struct timecounter as FreeBSD-style hardware time source

$define %func tc_register as function with args struct timecounter *
$define %func tc_deregister as function with args struct timecounter *
$define %func tc_get_current as function with args void
$define %func tc_best as function with args void

*/

/* !SPACE!

$space %export tc_register, tc_deregister, tc_get_current, tc_best

*/

#ifndef KERNEL_TIME_CLOCKSOURCE_H
#define KERNEL_TIME_CLOCKSOURCE_H

#include <mlibc/mlibc.h>
struct timecounter {
	struct timecounter	*tc_next;
	u64			tc_counter_mask;
	u64			tc_frequency;
	u64			(*tc_get_timecount)(struct timecounter *tc);
	int			tc_quality;
	const char		*tc_name;
	void			*tc_priv;
};

void		tc_register(struct timecounter *tc);
void		tc_deregister(struct timecounter *tc);
struct timecounter *tc_best(void);
struct timecounter *tc_get_current(void);

#endif
