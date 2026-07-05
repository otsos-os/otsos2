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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

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

$space %internal lapic_read, lapic_write
$space %export lapic_init, lapic_eoi
$space %export lapic_timer_set, lapic_timer_stop
$space %export lapic_timer_set_count, lapic_timer_get_count
$space %export lapic_timer_calibrate
$space %export lapic_get_id, lapic_is_enabled

*/

#ifndef KERNEL_INTERRUPTS_APIC_LAPIC_H
#define KERNEL_INTERRUPTS_APIC_LAPIC_H
#include <mlibc/mlibc.h>
#define	APIC_MMIO_VBASE		0xFFFF800000000000ULL
#define	LAPIC_ID		0x0020
#define	LAPIC_VERSION		0x0030
#define	LAPIC_TPR		0x0080
#define	LAPIC_APR		0x0090
#define	LAPIC_PPR		0x00A0
#define	LAPIC_EOI		0x00B0
#define	LAPIC_RRD		0x00C0
#define	LAPIC_LDR		0x00D0
#define	LAPIC_DFR		0x00E0
#define	LAPIC_SVR		0x00F0
#define	LAPIC_ISR_BASE		0x0100
#define	LAPIC_TMR_BASE		0x0180
#define	LAPIC_IRR_BASE		0x0200
#define	LAPIC_ESR		0x0280
#define	LAPIC_LVT_CMCI		0x02F0
#define	LAPIC_ICR_LO		0x0300
#define	LAPIC_ICR_HI		0x0310
#define	LAPIC_LVT_TIMER		0x0320
#define	LAPIC_LVT_THERMAL	0x0330
#define	LAPIC_LVT_PMC		0x0340
#define	LAPIC_LVT_LINT0		0x0350
#define	LAPIC_LVT_LINT1		0x0360
#define	LAPIC_LVT_ERROR		0x0370
#define	LAPIC_TIMER_ICR		0x0380
#define	LAPIC_TIMER_CCR		0x0390
#define	LAPIC_TIMER_DCR		0x03E0
#define	LAPIC_SVR_ENABLE		(1U << 8)
#define	LAPIC_SVR_FOCUS			(1U << 9)
#define	LAPIC_SVR_EOI_BROADCAST		(1U << 12)
#define	LAPIC_LVT_MASK			(1U << 16)
#define	LAPIC_LVT_TIMER_PERIODIC	(1U << 17)
#define	LAPIC_LVT_TIMER_TSC_DEADLINE	(1U << 18)
#define	LAPIC_TIMER_DIV1		0x0B
#define	LAPIC_TIMER_DIV2		0x00
#define	LAPIC_TIMER_DIV4		0x01
#define	LAPIC_TIMER_DIV8		0x02
#define	LAPIC_TIMER_DIV16		0x03
#define	LAPIC_TIMER_DIV32		0x08
#define	LAPIC_TIMER_DIV64		0x09
#define	LAPIC_TIMER_DIV128		0x0A
#define	LAPIC_ICR_FIXED			(0U << 8)
#define	LAPIC_ICR_INIT			(5U << 8)
#define	LAPIC_ICR_STARTUP		(6U << 8)
#define	LAPIC_ICR_ASSERT		(1U << 14)
#define	LAPIC_ICR_PENDING		(1U << 12)
#define	IA32_APIC_BASE			0x1B
#define	IA32_APIC_BASE_BSP		(1U << 8)
#define	IA32_APIC_BASE_X2APIC		(1U << 10)
#define	IA32_APIC_BASE_ENABLE		(1U << 11)
#define	APIC_TIMER_VECTOR		0x30

int	lapic_init(void);
void	lapic_eoi(void);
void	lapic_timer_set(u32 vector, int periodic);
void	lapic_timer_stop(void);
void	lapic_timer_set_count(u32 count);
u32	lapic_timer_get_count(void);
u64	lapic_timer_calibrate(void);
u8	lapic_get_id(void);
int	lapic_is_enabled(void);
#endif
