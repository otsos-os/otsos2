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
$define %type int as 32 bit signed
$define %type acpi_madt_t as packed struct with MADT header
$define %type acpi_madt_entry_header_t as packed struct with entry type and length
$define %type acpi_madt_local_apic_t as packed struct with local APIC entry
$define %type acpi_madt_io_apic_t as packed struct with I/O APIC entry
$define %type acpi_fadt_t as packed struct with FADT fields
$define %type acpi_hpet_t as packed struct with HPET fields
$define %type acpi_sdt_header_t as packed struct with common SDT header

$define %func acpi_madt_foreach as function with args u8, void (*)(acpi_madt_entry_header_t *, void *), void *
$define %func count_cpu_cb as procedure with args acpi_madt_entry_header_t *, void *
$define %func acpi_get_cpu_count as function with args void
$define %func get_ioapic_cb as procedure with args acpi_madt_entry_header_t *, void *
$define %func acpi_get_ioapic_address as function with args void
$define %func acpi_get_local_apic_address as function with args void
$define %func acpi_has_dual_pic as function with args void
$define %func acpi_dump_tables as procedure with args void

*/

/* !SPACE!

$space %internal count_cpu_cb, get_ioapic_cb
$space %export acpi_madt_foreach, acpi_get_cpu_count
$space %export acpi_get_ioapic_address, acpi_get_local_apic_address
$space %export acpi_has_dual_pic, acpi_dump_tables

*/

#include <kernel/drivers/acpi/acpi.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

int
acpi_madt_foreach(u8 entry_type,
    void (*callback)(acpi_madt_entry_header_t *, void *), void *ctx)
{
	acpi_madt_t			*madt;
	u8				*ptr, *end;
	int				count;
	acpi_madt_entry_header_t	*entry;

	madt = acpi_get_madt();
	if (!madt) {
		return (-1);
	}

	ptr = (u8 *)madt + sizeof(acpi_madt_t);
	end = (u8 *)madt + madt->header.length;
	count = 0;

	while (ptr < end) {
		entry = (acpi_madt_entry_header_t *)ptr;

		if (entry->length == 0) {
			break;
		}

		if (entry->type == entry_type) {
			if (callback) {
				callback(entry, ctx);
			}
			count++;
		}

		ptr += entry->length;
	}

	return (count);
}

static void
count_cpu_cb(acpi_madt_entry_header_t *entry, void *ctx)
{
	acpi_madt_local_apic_t	*lapic;
	int			*count;

	lapic = (acpi_madt_local_apic_t *)entry;
	count = (int *)ctx;

	if (lapic->flags & 1) {
		(*count)++;
	}
}

int
acpi_get_cpu_count(void)
{
	int	count, res;

	count = 0;
	res = acpi_madt_foreach(ACPI_MADT_LOCAL_APIC,
	    count_cpu_cb, &count);
	if (res < 0) {
		return (-1);
	}
	return (count);
}

static void
get_ioapic_cb(acpi_madt_entry_header_t *entry, void *ctx)
{
	acpi_madt_io_apic_t	*ioapic;
	u32			*addr;

	ioapic = (acpi_madt_io_apic_t *)entry;
	addr = (u32 *)ctx;

	if (*addr == 0) {
		*addr = ioapic->io_apic_address;
	}
}

u32
acpi_get_ioapic_address(void)
{
	u32	addr;

	addr = 0;
	acpi_madt_foreach(ACPI_MADT_IO_APIC, get_ioapic_cb, &addr);
	return (addr);
}

u32
acpi_get_local_apic_address(void)
{
	acpi_madt_t	*madt;

	madt = acpi_get_madt();
	if (!madt) {
		return (0);
	}
	return (madt->local_apic_address);
}

int
acpi_has_dual_pic(void)
{
	acpi_madt_t	*madt;

	madt = acpi_get_madt();
	if (!madt) {
		return (0);
	}
	return ((madt->flags & 1) ? 1 : 0);
}

void
acpi_dump_tables(void)
{
	acpi_fadt_t		*fadt;
	acpi_madt_t		*madt;
	acpi_sdt_header_t	*hpet, *mcfg;
	int			cpus;
	u32			ioapic;

	if (!acpi_is_initialized()) {
		com1_printf("[ACPI] not initialized\n");
		return;
	}

	com1_printf("[ACPI] ===== Table Dump =====\n");
	com1_printf("[ACPI] revision: %u\n", acpi_get_revision());

	fadt = acpi_get_fadt();
	if (fadt) {
		com1_printf("[ACPI] FADT:\n");
		com1_printf("[ACPI]   SCI interrupt   : %u\n",
		    fadt->sci_interrupt);
		com1_printf("[ACPI]   SMI command port: 0x%x\n",
		    fadt->smi_command_port);
		com1_printf("[ACPI]   PM1a event      : 0x%x\n",
		    fadt->pm1a_event_block);
		com1_printf("[ACPI]   PM1a control    : 0x%x\n",
		    fadt->pm1a_control_block);
		com1_printf("[ACPI]   PM1b control    : 0x%x\n",
		    fadt->pm1b_control_block);
		com1_printf("[ACPI]   PM timer        : 0x%x\n",
		    fadt->pm_timer_block);
		com1_printf("[ACPI]   flags           : 0x%x\n",
		    fadt->flags);
		com1_printf("[ACPI]   century reg     : %u\n",
		    fadt->century);
		com1_printf("[ACPI]   boot arch flags : 0x%x\n",
		    fadt->boot_arch_flags);

		if (fadt->flags & ACPI_FADT_RESET_REG_SUP) {
			com1_printf("[ACPI]   reset reg addr  : "
			    "0x%x (space=%u, val=0x%x)\n",
			    (u32)fadt->reset_reg.address,
			    fadt->reset_reg.address_space,
			    fadt->reset_value);
		}
	}

	madt = acpi_get_madt();
	if (madt) {
		com1_printf("[ACPI] MADT:\n");
		com1_printf("[ACPI]   local APIC addr : 0x%x\n",
		    madt->local_apic_address);
		com1_printf("[ACPI]   flags           : 0x%x\n",
		    madt->flags);

		cpus = acpi_get_cpu_count();
		com1_printf("[ACPI]   CPUs (enabled)  : %d\n",
		    cpus);

		ioapic = acpi_get_ioapic_address();
		if (ioapic) {
			com1_printf("[ACPI]   I/O APIC addr   : "
			    "0x%x\n", ioapic);
		}
	}

	hpet = acpi_find_table("HPET");
	if (hpet) {
		acpi_hpet_t	*h;

		h = (acpi_hpet_t *)hpet;
		com1_printf("[ACPI] HPET found at 0x%x\n",
		    (u32)h->address.address);
	}

	mcfg = acpi_find_table("MCFG");
	if (mcfg) {
		com1_printf("[ACPI] MCFG (PCIe) found, "
		    "len=%u\n", mcfg->length);
	}

	com1_printf("[ACPI] ===== End Dump =====\n");
}
