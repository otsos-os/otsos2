/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func pmu_init as procedure with args void
$define %func pmu_cpu_online as procedure with args int
$define %func pmu_sample as procedure with args int, u64 *, u32
$define %func pmu_counter_count as function with args void
$define %func pmu_counter_name as function with args u32
$define %func pmu_counter_active as function with args u32

*/

/* !SPACE!

$space %export pmu_init, pmu_cpu_online, pmu_sample
$space %export pmu_counter_count, pmu_counter_name, pmu_counter_active

*/

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

#ifndef KERNEL_DRIVERS_PMU_PMU_H
#define KERNEL_DRIVERS_PMU_PMU_H

#include <mlibc/mlibc.h>

enum pmu_counter_id {
	PMU_COUNTER_CYCLES = 0,
	PMU_COUNTER_INSTRUCTIONS,
	PMU_COUNTER_CACHE_REFERENCES,
	PMU_COUNTER_CACHE_MISSES,
	PMU_COUNTER_BRANCH_INSTRUCTIONS,
	PMU_COUNTER_BRANCH_MISSES,
	PMU_COUNTER_COUNT
};

void	pmu_init(void);
void	pmu_cpu_online(int cpu);
void	pmu_sample(int cpu, u64 *values, u32 max_values);
u32	pmu_counter_count(void);
const char *pmu_counter_name(u32 counter);
int	pmu_counter_active(u32 counter);

#endif
