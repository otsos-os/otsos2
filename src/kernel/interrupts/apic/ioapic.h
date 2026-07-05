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

$define %func ioapic_init as function with args void
$define %func ioapic_read as inline u32 with args u8
$define %func ioapic_write as inline void with args u8, u32
$define %func ioapic_set_irq as procedure with args u8, u8, u8, int, int
$define %func ioapic_mask_irq as procedure with args u8
$define %func ioapic_unmask_irq as procedure with args u8
$define %func ioapic_is_initialized as function with args void

*/

/* !SPACE!

$space %internal ioapic_read, ioapic_write
$space %export ioapic_init, ioapic_set_irq
$space %export ioapic_mask_irq, ioapic_unmask_irq
$space %export ioapic_is_initialized

*/

#ifndef KERNEL_INTERRUPTS_APIC_IOAPIC_H
#define KERNEL_INTERRUPTS_APIC_IOAPIC_H
#include <mlibc/mlibc.h>
#define	IOAPIC_IOREGSEL		0x00
#define	IOAPIC_IOWIN		0x10
#define	IOAPIC_IRQ_ENTRY_COUNT	24
#define	IOAPIC_RTE_VECTOR_MASK	0x000000FF
#define	IOAPIC_RTE_FIXED	(0U << 8)
#define	IOAPIC_RTE_LOWEST	(1U << 8)
#define	IOAPIC_RTE_INIT		(5U << 8)
#define	IOAPIC_RTE_EXTINT	(7U << 8)
#define	IOAPIC_RTE_LOGIC	(1U << 11)
#define	IOAPIC_RTE_ACTIVE_LOW	(1U << 13)
#define	IOAPIC_RTE_LEVEL	(1U << 15)
#define	IOAPIC_RTE_MASK		(1U << 16)
int	ioapic_init(void);
void	ioapic_set_irq(u8 irq, u8 vector, u8 dest, int level,
	    int active_low);
void	ioapic_mask_irq(u8 irq);
void	ioapic_unmask_irq(u8 irq);
int	ioapic_is_initialized(void);

#endif
