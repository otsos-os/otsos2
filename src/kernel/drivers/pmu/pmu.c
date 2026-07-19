/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type pmu_def_t as struct with architectural PMU event encoding

$define %func pmu_cpuid as procedure with args leaf, subleaf, registers
$define %func pmu_rdmsr as function with args u32
$define %func pmu_wrmsr as procedure with args u32, u64
$define %func pmu_mask as function with args void
$define %func pmu_event_available as function with args u32
$define %func pmu_counter_enabled as function with args u32
$define %func pmu_assign_counters as procedure with args u32, u32
$define %func pmu_read_counter as function with args u32
$define %func pmu_init as procedure with args void
$define %func pmu_cpu_online as procedure with args int
$define %func pmu_sample as procedure with args int, u64 *, u32
$define %func pmu_counter_count as function with args void
$define %func pmu_counter_name as function with args u32
$define %func pmu_counter_active as function with args u32

*/

/* !SPACE!

$space %internal pmu_cpuid, pmu_rdmsr, pmu_wrmsr
$space %internal pmu_mask, pmu_event_available, pmu_counter_enabled
$space %internal pmu_assign_counters, pmu_read_counter
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

#include <kernel/drivers/pmu/pmu.h>
#include <kernel/cm/cm.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define	CPUID_ARCH_PERFMON		0x0A
#define	MSR_IA32_PERFEVTSEL0		0x186
#define	MSR_IA32_PMC0			0x0C1
#define	MSR_IA32_PERF_GLOBAL_CTRL	0x38F

#define	PERFEVTSEL_USR			(1ULL << 16)
#define	PERFEVTSEL_OS			(1ULL << 17)
#define	PERFEVTSEL_ENABLE		(1ULL << 22)

typedef struct pmu_def {
	const char	*name;
	const char	*config_key;
	u32		event_select;
	u32		unit_mask;
	u32		arch_bit;
} pmu_def_t;

static const pmu_def_t g_pmu_defs[PMU_COUNTER_COUNT] = {
	{ "cycles", "Cycles", 0x3C, 0x00, 0 },
	{ "instructions", "Instructions", 0xC0, 0x00, 1 },
	{ "cache_references", "CacheReferences", 0x2E, 0x4F, 3 },
	{ "cache_misses", "CacheMisses", 0x2E, 0x41, 4 },
	{ "branch_instructions", "BranchInstructions", 0xC4, 0x00, 5 },
	{ "branch_misses", "BranchMisses", 0xC5, 0x00, 6 }
};

static int	g_pmu_initialized;
static int	g_pmu_available;
static u32	g_pmu_version;
static u32	g_pmu_general_count;
static u32	g_pmu_counter_width;
static u32	g_pmu_unavailable_mask;
static u32	g_pmu_enabled_mask;
static int	g_pmu_hw_index[PMU_COUNTER_COUNT];
static u64	g_pmu_last[SMP_MAX_CPUS][PMU_COUNTER_COUNT];
static u64	g_pmu_total[SMP_MAX_CPUS][PMU_COUNTER_COUNT];

static void
pmu_cpuid(u32 leaf, u32 subleaf, u32 *a, u32 *b, u32 *c, u32 *d)
{
	__asm__ volatile("cpuid"
	    : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
	    : "a"(leaf), "c"(subleaf));
}

static u64
pmu_rdmsr(u32 msr)
{
	u32	lo, hi;

	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return (((u64)hi << 32) | lo);
}

static void
pmu_wrmsr(u32 msr, u64 value)
{
	u32	lo, hi;

	lo = (u32)value;
	hi = (u32)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static u64
pmu_mask(void)
{
	if (g_pmu_counter_width == 0 || g_pmu_counter_width >= 64) {
		return (~0ULL);
	}
	return ((1ULL << g_pmu_counter_width) - 1);
}

static int
pmu_event_available(u32 counter)
{
	u32	bit;

	if (counter >= PMU_COUNTER_COUNT) {
		return (0);
	}
	bit = g_pmu_defs[counter].arch_bit;
	if (bit >= 32) {
		return (0);
	}
	return ((g_pmu_unavailable_mask & (1U << bit)) == 0);
}

static int
pmu_counter_enabled(u32 counter)
{
	const char	*key;
	int		enabled;

	if (counter >= PMU_COUNTER_COUNT) {
		return (0);
	}
	key = g_pmu_defs[counter].config_key;
	enabled = cm_get_bool_default("SYSTEM", "Trace.Pmu", key, 1);
	return (enabled);
}

static void
pmu_assign_counters(u32 general_count, u32 unavailable)
{
	u32	counter, slot;

	g_pmu_unavailable_mask = unavailable;
	g_pmu_enabled_mask = 0;
	slot = 0;
	for (counter = 0; counter < PMU_COUNTER_COUNT; counter++) {
		g_pmu_hw_index[counter] = -1;
		if (!pmu_counter_enabled(counter)) {
			continue;
		}
		if (!pmu_event_available(counter)) {
			continue;
		}
		if (slot >= general_count) {
			continue;
		}
		g_pmu_hw_index[counter] = (int)slot;
		g_pmu_enabled_mask |= 1U << counter;
		slot++;
	}
}

static u64
pmu_read_counter(u32 counter)
{
	u64	value, mask;
	int	hw;

	if (counter >= PMU_COUNTER_COUNT) {
		return (0);
	}
	hw = g_pmu_hw_index[counter];
	if (hw < 0) {
		return (0);
	}
	value = pmu_rdmsr(MSR_IA32_PMC0 + (u32)hw);
	mask = pmu_mask();
	return (value & mask);
}

void
pmu_init(void)
{
	u32	max_leaf, eax, ebx, ecx, edx;
	u32	version, general_count, width;
	u32	i;
	int	enabled;

	g_pmu_initialized = 0;
	g_pmu_available = 0;
	g_pmu_version = 0;
	g_pmu_general_count = 0;
	g_pmu_counter_width = 0;
	g_pmu_unavailable_mask = 0;
	g_pmu_enabled_mask = 0;
	memset(g_pmu_last, 0, sizeof(g_pmu_last));
	memset(g_pmu_total, 0, sizeof(g_pmu_total));
	for (i = 0; i < PMU_COUNTER_COUNT; i++) {
		g_pmu_hw_index[i] = -1;
	}

	enabled = cm_get_bool_default("SYSTEM", "Trace.Pmu",
	    "Enabled", 1);
	if (!enabled) {
		g_pmu_initialized = 1;
		drivers_log("[PMU] disabled by registry\n");
		return;
	}

	pmu_cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
	if (max_leaf < CPUID_ARCH_PERFMON) {
		g_pmu_initialized = 1;
		drivers_log("[PMU] architectural PMU not exposed\n");
		return;
	}

	pmu_cpuid(CPUID_ARCH_PERFMON, 0, &eax, &ebx, &ecx, &edx);
	version = eax & 0xFF;
	general_count = (eax >> 8) & 0xFF;
	width = (eax >> 16) & 0xFF;
	if (version == 0 || general_count == 0 || width == 0) {
		g_pmu_initialized = 1;
		drivers_log("[PMU] unavailable version=%u counters=%u\n",
		    version, general_count);
		return;
	}

	g_pmu_version = version;
	g_pmu_general_count = general_count;
	g_pmu_counter_width = width;
	pmu_assign_counters(general_count, ebx);
	if (g_pmu_enabled_mask == 0) {
		g_pmu_initialized = 1;
		drivers_log("[PMU] no configured counters available\n");
		return;
	}

	g_pmu_available = 1;
	g_pmu_initialized = 1;
	drivers_log("[PMU] arch v%u counters=%u width=%u mask=0x%x\n",
	    g_pmu_version, g_pmu_general_count, g_pmu_counter_width,
	    g_pmu_enabled_mask);
}

void
pmu_cpu_online(int cpu)
{
	const pmu_def_t	*def;
	u64		global, select;
	u32		counter;
	int		hw;

	if (!g_pmu_initialized || !g_pmu_available ||
	    cpu < 0 || cpu >= SMP_MAX_CPUS) {
		return;
	}

	global = 0;
	if (g_pmu_version >= 2) {
		pmu_wrmsr(MSR_IA32_PERF_GLOBAL_CTRL, 0);
	}

	for (counter = 0; counter < PMU_COUNTER_COUNT; counter++) {
		hw = g_pmu_hw_index[counter];
		if (hw < 0) {
			continue;
		}
		def = &g_pmu_defs[counter];
		select = (u64)def->event_select |
		    ((u64)def->unit_mask << 8) |
		    PERFEVTSEL_USR | PERFEVTSEL_OS |
		    PERFEVTSEL_ENABLE;
		pmu_wrmsr(MSR_IA32_PERFEVTSEL0 + (u32)hw, 0);
		pmu_wrmsr(MSR_IA32_PMC0 + (u32)hw, 0);
		pmu_wrmsr(MSR_IA32_PERFEVTSEL0 + (u32)hw, select);
		if ((u32)hw < 64) {
			global |= 1ULL << (u32)hw;
		}
		g_pmu_last[cpu][counter] = 0;
		g_pmu_total[cpu][counter] = 0;
	}

	if (g_pmu_version >= 2) {
		pmu_wrmsr(MSR_IA32_PERF_GLOBAL_CTRL, global);
	}
}

void
pmu_sample(int cpu, u64 *values, u32 max_values)
{
	u64	now, last, delta, mask;
	u32	counter;

	if (values == NULL) {
		return;
	}
	if (max_values > PMU_COUNTER_COUNT) {
		max_values = PMU_COUNTER_COUNT;
	}
	for (counter = 0; counter < max_values; counter++) {
		values[counter] = 0;
	}
	if (!g_pmu_initialized || !g_pmu_available ||
	    cpu < 0 || cpu >= SMP_MAX_CPUS) {
		return;
	}

	mask = pmu_mask();
	for (counter = 0; counter < max_values; counter++) {
		if ((g_pmu_enabled_mask & (1U << counter)) == 0) {
			continue;
		}
		now = pmu_read_counter(counter);
		last = g_pmu_last[cpu][counter];
		if (last == 0) {
			delta = 0;
		} else {
			delta = (now - last) & mask;
		}
		g_pmu_last[cpu][counter] = now;
		g_pmu_total[cpu][counter] += delta;
		values[counter] = delta;
	}
}

u32
pmu_counter_count(void)
{
	return (PMU_COUNTER_COUNT);
}

const char *
pmu_counter_name(u32 counter)
{
	if (counter >= PMU_COUNTER_COUNT) {
		return ("unknown");
	}
	return (g_pmu_defs[counter].name);
}

int
pmu_counter_active(u32 counter)
{
	if (counter >= PMU_COUNTER_COUNT) {
		return (0);
	}
	if (!g_pmu_available) {
		return (0);
	}
	return ((g_pmu_enabled_mask & (1U << counter)) != 0);
}
