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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type pcpu_t as per CPU data with identity, current thread, nesting counters and witness stack
$define %type pcpu_idmode_t as enum with CPU identity read method

$define %func pcpu_init as procedure with args void
$define %func pcpu_attach as procedure with args int, u8
$define %func pcpu_is_ready as function with args void
$define %func pcpu_idmode as function with args void
$define %func pcpu_idmode_name as function with args void
$define %func pcpu_id_lapic as function with args void
$define %func pcpu_wrmsr as procedure with args u32, u64
$define %func pcpu_cpuid as procedure with args u32, u32, u32 *, u32 *, u32 *, u32 *
$define %func pcpu_detect_mode as function with args void
$define %func pcpu_set_syscall_stack as procedure with args u64
$define %func pcpu_syscall_stack as function with args void
$define %func pcpu_reload_gsbase as procedure with args void

$define %const MSR_TSC_AUX as 0xC0000103
$define %const CPUID_EXT_RDTSCP as bit 27 of leaf 0x80000001 edx
$define %const CPUID_RDPID as bit 22 of leaf 7 subleaf 0 ecx
$define %const PCPU_LAPIC_UNMAPPED as 0xFF marking a lapic id bound to no cpu index
$define %const PCPU_LAPIC_NONE as 0xFFFFFFFF marking a never attached pcpu slot

*/

/* !SPACE!

$space %internal pcpu_wrmsr, pcpu_cpuid, pcpu_detect_mode
$space %export pcpu_init, pcpu_attach, pcpu_is_ready
$space %export pcpu_set_syscall_stack, pcpu_syscall_stack
$space %export pcpu_reload_gsbase
$space %export pcpu_idmode, pcpu_idmode_name, pcpu_id_lapic
$space %export pcpu_data, pcpu_id_mode, pcpu_lapic_to_index

*/

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/panic.h>
#include <kernel/smp/pcpu.h>
#include <mlibc/stdio.h>

#define	MSR_TSC_AUX		0xC0000103
#define	CPUID_LEAF_EXT_FEAT	0x80000001
#define	CPUID_LEAF_EXT_MAX	0x80000000
#define	CPUID_LEAF_STRUCT_EXT	0x00000007
#define	CPUID_EDX_RDTSCP	(1U << 27)
#define	CPUID_ECX_RDPID		(1U << 22)
#define	PCPU_LAPIC_UNMAPPED	0xFFU
#define	PCPU_LAPIC_NONE		0xFFFFFFFFU

pcpu_t		pcpu_data[PCPU_MAX_CPUS] __attribute__((aligned(PCPU_CACHELINE)));
pcpu_idmode_t	pcpu_id_mode = PCPU_ID_BOOT;
u8		pcpu_lapic_to_index[256];

static int	pcpu_ready;

static void
pcpu_wrmsr(u32 msr, u64 value)
{
	u32	low;
	u32	high;

	low = (u32)(value & 0xFFFFFFFFULL);
	high = (u32)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static void
pcpu_cpuid(u32 leaf, u32 subleaf, u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	u32	a;
	u32	b;
	u32	c;
	u32	d;

	a = leaf;
	c = subleaf;
	__asm__ volatile("cpuid"
	    : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
	    : "a"(a), "c"(c));
	if (eax != NULL) {
		*eax = a;
	}
	if (ebx != NULL) {
		*ebx = b;
	}
	if (ecx != NULL) {
		*ecx = c;
	}
	if (edx != NULL) {
		*edx = d;
	}
}

static pcpu_idmode_t
pcpu_detect_mode(void)
{
	u32	eax;
	u32	ebx;
	u32	ecx;
	u32	edx;

	pcpu_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
	if (eax >= CPUID_LEAF_STRUCT_EXT) {
		pcpu_cpuid(CPUID_LEAF_STRUCT_EXT, 0, &eax, &ebx, &ecx, &edx);
		if (ecx & CPUID_ECX_RDPID) {
			return (PCPU_ID_RDPID);
		}
	}
	pcpu_cpuid(CPUID_LEAF_EXT_MAX, 0, &eax, &ebx, &ecx, &edx);
	if (eax >= CPUID_LEAF_EXT_FEAT) {
		pcpu_cpuid(CPUID_LEAF_EXT_FEAT, 0, &eax, &ebx, &ecx, &edx);
		if (edx & CPUID_EDX_RDTSCP) {
			return (PCPU_ID_RDTSCP);
		}
	}
	return (PCPU_ID_LAPIC);
}

u32
pcpu_id_lapic(void)
{
	u8	idx;

	if (!lapic_is_enabled()) {
		return (0);
	}
	idx = pcpu_lapic_to_index[lapic_get_id()];
	if (idx >= PCPU_MAX_CPUS) {
		return (0);
	}
	return ((u32)idx);
}

pcpu_idmode_t
pcpu_idmode(void)
{
	return (pcpu_id_mode);
}

const char *
pcpu_idmode_name(void)
{
	switch (pcpu_id_mode) {
	case PCPU_ID_RDPID:
		return ("rdpid");
	case PCPU_ID_RDTSCP:
		return ("rdtscp");
	case PCPU_ID_LAPIC:
		return ("lapic");
	case PCPU_ID_BOOT:
	default:
		return ("boot");
	}
}

int
pcpu_is_ready(void)
{
	return (pcpu_ready);
}

void
pcpu_set_syscall_stack(u64 stack_top)
{
	pcpu_t	*pc;

	if (!pcpu_ready) {
		return;
	}
	pc = pcpu_current();
	if (pc == NULL) {
		return;
	}
	__atomic_store_n(&pc->syscall_stack, stack_top, __ATOMIC_RELEASE);
}

u64
pcpu_syscall_stack(void)
{
	pcpu_t	*pc;

	if (!pcpu_ready) {
		return (0);
	}
	pc = pcpu_current();
	if (pc == NULL) {
		return (0);
	}
	return (__atomic_load_n(&pc->syscall_stack, __ATOMIC_ACQUIRE));
}

void
pcpu_reload_gsbase(void)
{
	pcpu_t	*pc;

	if (!pcpu_ready) {
		return;
	}
	pc = pcpu_current();
	if (pc == NULL) {
		return;
	}
	pcpu_wrmsr(MSR_GS_BASE, (u64)pc);
	pcpu_wrmsr(MSR_KERNEL_GS_BASE, 0);
}

void
pcpu_attach(int cpu_index, u8 lapic_id)
{
	pcpu_t	*pc;

	if (cpu_index < 0 || cpu_index >= PCPU_MAX_CPUS) {
		panic("[PCPU] cpu index %d out of range\n", cpu_index);
	}
	pc = &pcpu_data[cpu_index];
	if (pc->lapic_id == PCPU_LAPIC_NONE) {
		pc->critnest = 0;
		pc->spin_held = 0;
		pc->witness_depth = 0;
		pc->witness_level = 0;
	} else if (pc->lapic_id != (u32)lapic_id &&
	    pc->lapic_id < sizeof(pcpu_lapic_to_index)) {
		pcpu_lapic_to_index[pc->lapic_id] = PCPU_LAPIC_UNMAPPED;
	}
	pc->cpu_index = (u32)cpu_index;
	pc->lapic_id = lapic_id;
	pc->online = 1;
	pcpu_lapic_to_index[lapic_id] = (u8)cpu_index;
	pcpu_wrmsr(MSR_GS_BASE, (u64)pc);
	pcpu_wrmsr(MSR_KERNEL_GS_BASE, 0);
	if (pcpu_id_mode == PCPU_ID_RDPID ||
	    pcpu_id_mode == PCPU_ID_RDTSCP) {
		pcpu_wrmsr(MSR_TSC_AUX, (u64)(u32)cpu_index);
	}
	if (pcpu_id() != (u32)cpu_index) {
		panic("[PCPU] identity mismatch: read %u, expected %d (%s)\n",
		    pcpu_id(), cpu_index, pcpu_idmode_name());
	}
}

void
pcpu_init(void)
{
	int	i;

	if (pcpu_ready) {
		return;
	}
	for (i = 0; i < PCPU_MAX_CPUS; i++) {
		memset(&pcpu_data[i], 0, sizeof(pcpu_data[i]));
		pcpu_data[i].cpu_index = (u32)i;
		pcpu_data[i].lapic_id = PCPU_LAPIC_NONE;
	}
	memset(pcpu_lapic_to_index, PCPU_LAPIC_UNMAPPED,
	    sizeof(pcpu_lapic_to_index));
	pcpu_id_mode = pcpu_detect_mode();
	pcpu_ready = 1;
	printk("[PCPU] identity via %s\n", pcpu_idmode_name());
}
