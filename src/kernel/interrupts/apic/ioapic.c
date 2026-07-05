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
$define %func ioapic_find_address as function with args void
$define %func ioapic_set_irq as procedure with args u8, u8, u8, int, int
$define %func ioapic_mask_irq as procedure with args u8
$define %func ioapic_unmask_irq as procedure with args u8
$define %func ioapic_is_initialized as function with args void

*/

/* !SPACE!

$space %internal ioapic_read, ioapic_write, ioapic_find_address
$space %export ioapic_init, ioapic_set_irq
$space %export ioapic_mask_irq, ioapic_unmask_irq
$space %export ioapic_is_initialized

*/

#include <kernel/interrupts/apic/ioapic.h>
#include <kernel/interrupts/apic/lapic.h>
#include <kernel/drivers/acpi/acpi.h>
#include <mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static u64	ioapic_base;
static u8	ioapic_gsi_base;
static int	ioapic_ready;

static inline u32
ioapic_read(u8 reg)
{
	*(volatile u32 *)((u8 *)ioapic_base + IOAPIC_IOREGSEL) = reg;
	return (*(volatile u32 *)((u8 *)ioapic_base + IOAPIC_IOWIN));
}

static inline void
ioapic_write(u8 reg, u32 value)
{
	*(volatile u32 *)((u8 *)ioapic_base + IOAPIC_IOREGSEL) = reg;
	*(volatile u32 *)((u8 *)ioapic_base + IOAPIC_IOWIN) = value;
}

static u64
ioapic_find_address(void)
{
	acpi_madt_t		*madt;
	acpi_madt_entry_header_t *entry;
	u8			*end;
	u32			first_addr;

	madt = acpi_get_madt();
	if (madt == NULL)
		return (0);
	end = (u8 *)madt + madt->header.length;
	first_addr = 0;
	entry = (acpi_madt_entry_header_t *)(madt + 1);
	while ((u8 *)entry < end) {
		if (entry->type == ACPI_MADT_IO_APIC) {
			acpi_madt_io_apic_t *ioapic;

			ioapic = (acpi_madt_io_apic_t *)entry;
			if (first_addr == 0) {
				first_addr =
				    ioapic->io_apic_address;
				ioapic_gsi_base = (u8)ioapic->
				    global_system_interrupt_base;
			}
		}
		if (entry->length == 0)
			break;
		entry = (acpi_madt_entry_header_t *)((u8 *)entry +
		    entry->length);
	}
	return ((u64)first_addr);
}

int
ioapic_init(void)
{
	u64	ioapic_phys;
	u64	vaddr;
	u32	ver;
	u8	max_redir;
	u8	i;

	ioapic_phys = ioapic_find_address();
	if (ioapic_phys == 0) {
		printk("[IOAPIC] no I/O APIC found in MADT\n");
		return (-1);
	}
	vaddr = APIC_MMIO_VBASE + ioapic_phys;
	ioapic_base = vaddr;
	pmap_enter(vaddr, ioapic_phys, PTE_RW | PTE_PCD | PTE_PWT);

	ver = ioapic_read(1);
	max_redir = (u8)((ver >> 16) & 0xFF);
	for (i = 0; i <= max_redir &&
	    i < IOAPIC_IRQ_ENTRY_COUNT; i++)
		ioapic_mask_irq(i);
	ioapic_ready = 1;
	printk("[IOAPIC] addr=0x%p gsi_base=%u max_redir=%u\n",
	    (void *)ioapic_phys, (u32)ioapic_gsi_base,
	    (u32)max_redir);
	return (0);
}

void
ioapic_set_irq(u8 irq, u8 vector, u8 dest, int level,
    int active_low)
{
	u32	low, high;
	u8	reg;

	if (!ioapic_ready || irq >= IOAPIC_IRQ_ENTRY_COUNT)
		return;
	reg = 0x10 + irq * 2;
	low = vector & IOAPIC_RTE_VECTOR_MASK;
	if (level)
		low |= IOAPIC_RTE_LEVEL;
	if (active_low)
		low |= IOAPIC_RTE_ACTIVE_LOW;
	high = ((u32)dest) << 24;
	ioapic_write(reg, low);
	ioapic_write((u8)(reg + 1), high);
}

void
ioapic_mask_irq(u8 irq)
{
	u32	low;
	u8	reg;

	if (!ioapic_ready || irq >= IOAPIC_IRQ_ENTRY_COUNT)
		return;
	reg = 0x10 + irq * 2;
	low = ioapic_read(reg);
	ioapic_write(reg, low | IOAPIC_RTE_MASK);
}

void
ioapic_unmask_irq(u8 irq)
{
	u32	low;
	u8	reg;

	if (!ioapic_ready || irq >= IOAPIC_IRQ_ENTRY_COUNT)
		return;
	reg = 0x10 + irq * 2;
	low = ioapic_read(reg);
	ioapic_write(reg, low & ~IOAPIC_RTE_MASK);
}

int
ioapic_is_initialized(void)
{
	return (ioapic_ready);
}
