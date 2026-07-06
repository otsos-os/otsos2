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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type volatile as qualifier for MMIO

$define %func lapic_read as inline u32 with args u32
$define %func lapic_write as inline void with args u32, u32
$define %func lapic_rdmsr as inline u64 with args u32
$define %func lapic_wrmsr as inline void with args u32, u64
$define %func lapic_init as function with args void
$define %func lapic_eoi as procedure with args void
$define %func lapic_timer_set as procedure with args u32, int
$define %func lapic_timer_stop as procedure with args void
$define %func lapic_timer_set_count as procedure with args u32
$define %func lapic_timer_get_count as function with args void
$define %func lapic_timer_calibrate as function with args void
$define %func lapic_get_id as function with args void
$define %func lapic_is_enabled as function with args void

*/

/* !SPACE!

$space %internal lapic_read, lapic_write, lapic_rdmsr, lapic_wrmsr
$space %export lapic_init, lapic_eoi
$space %export lapic_timer_set, lapic_timer_stop
$space %export lapic_timer_set_count, lapic_timer_get_count
$space %export lapic_timer_calibrate
$space %export lapic_get_id, lapic_is_enabled

*/

#include <kernel/interrupts/apic/lapic.h>
#include <kernel/drivers/timer.h>
#include <kernel/other/config.h>
#include <mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static u64	lapic_base;
static int	lapic_enabled;
static u64	lapic_timer_freq;

static inline u32
lapic_read(u32 offset)
{
	return (*(volatile u32 *)((u8 *)lapic_base + offset));
}

static inline void
lapic_write(u32 offset, u32 value)
{
	*(volatile u32 *)((u8 *)lapic_base + offset) = value;
}

static inline u64
lapic_rdmsr(u32 msr)
{
	u32	lo, hi;

	__asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return (((u64)hi << 32) | lo);
}

static inline void
lapic_wrmsr(u32 msr, u64 value)
{
	u32	lo, hi;

	lo = (u32)value;
	hi = (u32)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

int
lapic_init(void)
{
	u64	apic_msr;
	u64	apic_phys;
	u64	vaddr;
	u32	svr;

	apic_msr = lapic_rdmsr(IA32_APIC_BASE);
	if (!(apic_msr & IA32_APIC_BASE_ENABLE)) {
		apic_msr |= IA32_APIC_BASE_ENABLE;
		lapic_wrmsr(IA32_APIC_BASE, apic_msr);
	}
	apic_phys = apic_msr & 0xFFFFF000ULL;
	vaddr = APIC_MMIO_VBASE + apic_phys;
	lapic_base = vaddr;
	pmap_enter(vaddr, apic_phys, PTE_RW | PTE_PCD | PTE_PWT);

	svr = lapic_read(LAPIC_SVR);
	lapic_write(LAPIC_SVR,
	    (svr & ~0xFFU) | 0xFF | LAPIC_SVR_ENABLE);

	lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK |
	    APIC_TIMER_VECTOR);
	lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV16);

	lapic_write(LAPIC_DFR, 0xFFFFFFFF);
	lapic_write(LAPIC_LDR, (1U << lapic_get_id()));

	lapic_enabled = 1;
	printk("[LAPIC] id=%u ver=%u base=0x%p\n",
	    (u32)lapic_get_id(),
	    lapic_read(LAPIC_VERSION) & 0xFF,
	    (void *)apic_phys);
	return (0);
}
void
lapic_enable(void)
{
	u64	apic_msr;
	u32	svr;

	apic_msr = lapic_rdmsr(IA32_APIC_BASE);
	if (!(apic_msr & IA32_APIC_BASE_ENABLE)) {
		apic_msr |= IA32_APIC_BASE_ENABLE;
		lapic_wrmsr(IA32_APIC_BASE, apic_msr);
	}
	svr = lapic_read(LAPIC_SVR);
	lapic_write(LAPIC_SVR, (svr & ~0xFFU) | 0xFF |
	    LAPIC_SVR_ENABLE);
	if (!lapic_enabled) {
		lapic_enabled = 1;
	}
}

void
lapic_icr_send(u32 vector, u8 apic_id)
{
	u32	icr_high;

	icr_high = ((u32)apic_id) << 24;
	lapic_write(LAPIC_ICR_HI, icr_high);
	lapic_write(LAPIC_ICR_LO, vector);
	while ((lapic_read(LAPIC_ICR_LO) & LAPIC_ICR_PENDING) != 0) {
		__asm__ volatile("pause");
	}
}

u64
lapic_timer_get_freq(void)
{
	return (lapic_timer_freq);
}

void
lapic_timer_init_ap(void)
{
	u32	hz;
	u64	freq;
	u32	count;

	freq = lapic_timer_freq;
	hz = timer_get_frequency();
	if (freq == 0 || hz == 0) {
		printk("[LAPIC] AP timer doesnt started and no freq\n");
		return;
	}

	lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV128);
	count = (u32)(freq / hz);
	if (count == 0) {
		count = 1;
	}
	lapic_timer_set(APIC_TIMER_VECTOR, 1);
	lapic_timer_set_count(count);
	printk("[LAPIC] AP timer started count=%u\n", count);
}

void
lapic_eoi(void)
{
	lapic_write(LAPIC_EOI, 0);
}

void
lapic_timer_set(u32 vector, int periodic)
{
	u32	val;

	val = vector;
	if (periodic)
		val |= LAPIC_LVT_TIMER_PERIODIC;
	lapic_write(LAPIC_LVT_TIMER, val);
}

void
lapic_timer_stop(void)
{
	lapic_write(LAPIC_LVT_TIMER, LAPIC_LVT_MASK |
	    APIC_TIMER_VECTOR);
	lapic_write(LAPIC_TIMER_ICR, 0);
}

void
lapic_timer_set_count(u32 count)
{
	lapic_write(LAPIC_TIMER_ICR, count);
}

u32
lapic_timer_get_count(void)
{
	return (lapic_read(LAPIC_TIMER_CCR));
}

static u64
lapic_timer_calibrate_read(void *arg)
{

	(void)arg;
	return ((u64)lapic_read(LAPIC_TIMER_CCR));
}

u64
lapic_timer_calibrate(void)
{
	struct timer_calibrate	calib;
	const char		*calib_name;
	u64			freq;

	if (!timer_is_initialized()) {
		printk("[LAPIC] timer not running, cannot "
		    "calibrate\n");
		return (0);
	}

	calib_name = config_get_string("timer", "timer_to_calibrate",
	    "pit");
	printk("[LAPIC] calibrating via %s timer...\n", calib_name);

	lapic_write(LAPIC_TIMER_DCR, LAPIC_TIMER_DIV128);
	lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

	calib.read_count = lapic_timer_calibrate_read;
	calib.arg = NULL;
	freq = timer_calibrate(&calib, 50, 128);

	if (freq == 0) {
		printk("[LAPIC] calibration failed\n");
		return (0);
	}

	lapic_timer_freq = freq;
	printk("[LAPIC] calibrated freq=%u Hz\n", (u32)freq);
	return (freq);
}

u8
lapic_get_id(void)
{
	return ((u8)(lapic_read(LAPIC_ID) >> 24));
}

int
lapic_is_enabled(void)
{
	return (lapic_enabled);
}
