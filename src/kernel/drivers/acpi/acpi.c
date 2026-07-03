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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type acpi_rsdp_v1_t as packed struct with RSDP v1 fields
$define %type acpi_rsdp_v2_t as packed struct extending v1 with XSDT addr
$define %type acpi_sdt_header_t as packed struct with common SDT header
$define %type acpi_rsdt_t as packed struct with RSDT header and entries
$define %type acpi_xsdt_t as packed struct with XSDT header and entries
$define %type acpi_fadt_t as packed struct with FADT fields
$define %type acpi_madt_t as packed struct with MADT header

$define %func validate_checksum as function with args const void *, u32
$define %func acpi_validate_checksum as function with args acpi_sdt_header_t *
$define %func sig_match as function with args const char *, const char *
$define %func validate_rsdp as function with args acpi_rsdp_v1_t *
$define %func scan_for_rsdp as function with args u64, u64
$define %func find_rsdp_bios as function with args void
$define %func parse_rsdt as procedure with args acpi_rsdt_t *
$define %func parse_xsdt as procedure with args acpi_xsdt_t *
$define %func acpi_init_from_rsdp as function with args void *
$define %func acpi_init_from_multiboot2 as function with args void *
$define %func acpi_find_table as function with args const char *
$define %func acpi_get_fadt as function with args void
$define %func acpi_get_madt as function with args void
$define %func acpi_is_initialized as function with args void
$define %func acpi_get_revision as function with args void

*/

/* !SPACE!

$space %internal validate_checksum, sig_match, validate_rsdp
$space %internal scan_for_rsdp, find_rsdp_bios
$space %internal parse_rsdt, parse_xsdt
$space %export acpi_validate_checksum, acpi_init_from_rsdp
$space %export acpi_init_from_multiboot2, acpi_find_table
$space %export acpi_get_fadt, acpi_get_madt
$space %export acpi_is_initialized, acpi_get_revision

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int			g_acpi_initialized;
static int			g_acpi_revision;
static acpi_rsdt_t		*g_rsdt;
static acpi_xsdt_t		*g_xsdt;
static acpi_fadt_t		*g_fadt;
static acpi_madt_t		*g_madt;

static int
validate_checksum(const void *ptr, u32 length)
{
	const u8	*bytes;
	u8		sum;
	u32		i;

	bytes = (const u8 *)ptr;
	sum = 0;
	for (i = 0; i < length; i++) {
		sum += bytes[i];
	}
	return (sum == 0) ? 0 : -1;
}

int
acpi_validate_checksum(acpi_sdt_header_t *header)
{
	if (!header) {
		return (-1);
	}
	return (validate_checksum(header, header->length));
}

static int
sig_match(const char *a, const char *b)
{
	return (a[0] == b[0] && a[1] == b[1] &&
	    a[2] == b[2] && a[3] == b[3]);
}

static int
validate_rsdp(acpi_rsdp_v1_t *rsdp)
{
	const char	*expected;
	int		i;

	expected = "RSD PTR ";
	for (i = 0; i < 8; i++) {
		if (rsdp->signature[i] != expected[i]) {
			return (-1);
		}
	}

	if (validate_checksum(rsdp, 20) != 0) {
		panic("[ACPI] RSDP v1 checksum failed\n");
		return (-1);
	}

	if (rsdp->revision >= 2) {
		acpi_rsdp_v2_t	*rsdp2;

		rsdp2 = (acpi_rsdp_v2_t *)rsdp;
		if (validate_checksum(rsdp2, rsdp2->length) != 0) {
			panic("[ACPI] RSDP v2 extended checksum "
			    "failed\n");
			return (-1);
		}
	}

	return (0);
}

static acpi_rsdp_v1_t *
scan_for_rsdp(u64 start, u64 end)
{
	u64			addr;
	acpi_rsdp_v1_t		*candidate;

	for (addr = start; addr < end; addr += 16) {
		candidate = (acpi_rsdp_v1_t *)addr;
		if (validate_rsdp(candidate) == 0) {
			return (candidate);
		}
	}
	return (NULL);
}

static acpi_rsdp_v1_t *
find_rsdp_bios(void)
{
	u16			ebda_seg;
	u64			ebda_addr;
	acpi_rsdp_v1_t		*rsdp;

	ebda_seg = *(u16 *)0x040E;
	ebda_addr = (u64)ebda_seg << 4;
	if (ebda_addr) {
		rsdp = scan_for_rsdp(ebda_addr, ebda_addr + 1024);
		if (rsdp) {
			return (rsdp);
		}
	}

	return (scan_for_rsdp(0x000E0000, 0x00100000));
}

static void
parse_rsdt(acpi_rsdt_t *rsdt)
{
	u32			num_entries, i;
	acpi_sdt_header_t	*sdt;

	if (!rsdt) {
		return;
	}

	if (acpi_validate_checksum(&rsdt->header) != 0) {
		panic("[ACPI] RSDT checksum failed\n");
		return;
	}

	num_entries = (rsdt->header.length -
	    sizeof(acpi_sdt_header_t)) / 4;
	drivers_log("[ACPI] RSDT has %u entries\n", num_entries);

	for (i = 0; i < num_entries; i++) {
		sdt = (acpi_sdt_header_t *)(u64)rsdt->entries[i];
		if (!sdt) {
			continue;
		}

		drivers_log("[ACPI]   table: %c%c%c%c  len=%u\n",
		    sdt->signature[0], sdt->signature[1],
		    sdt->signature[2], sdt->signature[3],
		    sdt->length);

		if (sig_match(sdt->signature, "FACP")) {
			g_fadt = (acpi_fadt_t *)sdt;
		} else if (sig_match(sdt->signature, "APIC")) {
			g_madt = (acpi_madt_t *)sdt;
		}
	}
}

static void
parse_xsdt(acpi_xsdt_t *xsdt)
{
	u32			num_entries, i;
	acpi_sdt_header_t	*sdt;

	if (!xsdt) {
		return;
	}

	if (acpi_validate_checksum(&xsdt->header) != 0) {
		panic("[ACPI] XSDT checksum failed\n");
		return;
	}

	num_entries = (xsdt->header.length -
	    sizeof(acpi_sdt_header_t)) / 8;
	drivers_log("[ACPI] XSDT has %u entries\n", num_entries);

	for (i = 0; i < num_entries; i++) {
		sdt = (acpi_sdt_header_t *)xsdt->entries[i];
		if (!sdt) {
			continue;
		}

		drivers_log("[ACPI]   table: %c%c%c%c  len=%u\n",
		    sdt->signature[0], sdt->signature[1],
		    sdt->signature[2], sdt->signature[3],
		    sdt->length);

		if (sig_match(sdt->signature, "FACP")) {
			g_fadt = (acpi_fadt_t *)sdt;
		} else if (sig_match(sdt->signature, "APIC")) {
			g_madt = (acpi_madt_t *)sdt;
		}
	}
}

int
acpi_init_from_rsdp(void *rsdp_ptr)
{
	acpi_rsdp_v1_t	*rsdp;

	rsdp = (acpi_rsdp_v1_t *)rsdp_ptr;

	if (validate_rsdp(rsdp) != 0) {
		panic("[ACPI] invalid RSDP at %p\n", rsdp_ptr);
		return (-1);
	}

	g_acpi_revision = rsdp->revision;

	drivers_log("[ACPI] RSDP found at %p, revision %u, "
	    "OEM: %.6s\n", rsdp_ptr, rsdp->revision, rsdp->oem_id);

	if (rsdp->revision >= 2) {
		acpi_rsdp_v2_t	*rsdp2;

		rsdp2 = (acpi_rsdp_v2_t *)rsdp;
		if (rsdp2->xsdt_address) {
			g_xsdt = (acpi_xsdt_t *)rsdp2->xsdt_address;
			drivers_log("[ACPI] using XSDT at %p\n",
			    (void *)rsdp2->xsdt_address);
			parse_xsdt(g_xsdt);
		} else {
			g_rsdt = (acpi_rsdt_t *)
			    (u64)rsdp->rsdt_address;
			drivers_log("[ACPI] XSDT null, using RSDT "
			    "at %p\n",
			    (void *)(u64)rsdp->rsdt_address);
			parse_rsdt(g_rsdt);
		}
	} else {
		g_rsdt = (acpi_rsdt_t *)(u64)rsdp->rsdt_address;
		drivers_log("[ACPI] using RSDT at %p\n",
		    (void *)(u64)rsdp->rsdt_address);
		parse_rsdt(g_rsdt);
	}

	if (g_fadt) {
		drivers_log("[ACPI] FADT found: SCI int=%u, "
		    "PM1a=0x%x, SMI cmd=0x%x\n",
		    g_fadt->sci_interrupt,
		    g_fadt->pm1a_control_block,
		    g_fadt->smi_command_port);
	}

	if (g_madt) {
		drivers_log("[ACPI] MADT found: local APIC at "
		    "0x%x, flags=0x%x\n",
		    g_madt->local_apic_address, g_madt->flags);
	}

	g_acpi_initialized = 1;
	return (0);
}

int
acpi_init_from_multiboot2(void *mb2_info)
{
	multiboot2_info_t	*mb;
	multiboot2_tag_t	*tag_new, *tag_old;
	acpi_rsdp_v1_t		*rsdp;

	mb = (multiboot2_info_t *)mb2_info;

	tag_new = multiboot2_find_tag(mb,
	    MULTIBOOT2_TAG_TYPE_ACPI_NEW);
	if (tag_new) {
		void	*rsdp_ptr;

		rsdp_ptr = (void *)((u8 *)tag_new + 8);
		drivers_log("[ACPI] found ACPI_NEW multiboot2 tag\n");
		return (acpi_init_from_rsdp(rsdp_ptr));
	}

	tag_old = multiboot2_find_tag(mb,
	    MULTIBOOT2_TAG_TYPE_ACPI_OLD);
	if (tag_old) {
		void	*rsdp_ptr;

		rsdp_ptr = (void *)((u8 *)tag_old + 8);
		drivers_log("[ACPI] found ACPI_OLD multiboot2 tag\n");
		return (acpi_init_from_rsdp(rsdp_ptr));
	}

	drivers_log("[ACPI] no multiboot2 ACPI tag, scanning "
	    "BIOS area...\n");
	rsdp = find_rsdp_bios();
	if (rsdp) {
		return (acpi_init_from_rsdp(rsdp));
	}

	panic("[ACPI] RSDP not found!\n");
	return (-1);
}

acpi_sdt_header_t *
acpi_find_table(const char *signature)
{
	u32			n, i;
	acpi_sdt_header_t	*sdt;

	if (!g_acpi_initialized) {
		return (NULL);
	}

	if (g_xsdt) {
		n = (g_xsdt->header.length -
		    sizeof(acpi_sdt_header_t)) / 8;
		for (i = 0; i < n; i++) {
			sdt = (acpi_sdt_header_t *)g_xsdt->entries[i];
			if (sdt && sig_match(sdt->signature,
			    signature)) {
				return (sdt);
			}
		}
	} else if (g_rsdt) {
		n = (g_rsdt->header.length -
		    sizeof(acpi_sdt_header_t)) / 4;
		for (i = 0; i < n; i++) {
			sdt = (acpi_sdt_header_t *)
			    (u64)g_rsdt->entries[i];
			if (sdt && sig_match(sdt->signature,
			    signature)) {
				return (sdt);
			}
		}
	}

	return (NULL);
}

acpi_fadt_t *
acpi_get_fadt(void)
{
	return (g_fadt);
}

acpi_madt_t *
acpi_get_madt(void)
{
	return (g_madt);
}

int
acpi_is_initialized(void)
{
	return (g_acpi_initialized);
}

int
acpi_get_revision(void)
{
	return (g_acpi_revision);
}
