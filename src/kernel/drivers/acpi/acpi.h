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
$define %type acpi_gas_t as packed struct with Generic Address Structure
$define %type acpi_fadt_t as packed struct with FADT fields
$define %type acpi_madt_t as packed struct with MADT header
$define %type acpi_madt_entry_header_t as packed struct with entry type and length
$define %type acpi_madt_local_apic_t as packed struct with local APIC entry
$define %type acpi_madt_io_apic_t as packed struct with I/O APIC entry
$define %type acpi_madt_int_override_t as packed struct with interrupt override
$define %type acpi_hpet_t as packed struct with HPET fields
$define %type acpi_mcfg_entry_t as packed struct with MCFG entry
$define %type acpi_mcfg_t as packed struct with MCFG header

$define %func acpi_init_from_rsdp as function with args void *
$define %func acpi_init_from_multiboot2 as function with args void *
$define %func acpi_find_table as function with args const char *
$define %func acpi_get_fadt as function with args void
$define %func acpi_get_madt as function with args void
$define %func acpi_is_initialized as function with args void
$define %func acpi_get_revision as function with args void
$define %func acpi_validate_checksum as function with args acpi_sdt_header_t *
$define %func acpi_madt_foreach as function with args u8, void (*)(acpi_madt_entry_header_t *, void *), void *
$define %func acpi_get_cpu_count as function with args void
$define %func acpi_get_ioapic_address as function with args void
$define %func acpi_get_local_apic_address as function with args void
$define %func acpi_has_dual_pic as function with args void
$define %func acpi_dump_tables as procedure with args void

*/

/* !SPACE!

$space %export acpi_init_from_rsdp, acpi_init_from_multiboot2
$space %export acpi_find_table, acpi_get_fadt, acpi_get_madt
$space %export acpi_is_initialized, acpi_get_revision
$space %export acpi_validate_checksum
$space %export acpi_madt_foreach, acpi_get_cpu_count
$space %export acpi_get_ioapic_address, acpi_get_local_apic_address
$space %export acpi_has_dual_pic, acpi_dump_tables

*/

#ifndef ACPI_H
#define ACPI_H

#include <mlibc/mlibc.h>

typedef struct {
	char	signature[8];
	u8	checksum;
	char	oem_id[6];
	u8	revision;
	u32	rsdt_address;
} __attribute__((packed)) acpi_rsdp_v1_t;

typedef struct {
	acpi_rsdp_v1_t	v1;
	u32		length;
	u64		xsdt_address;
	u8		extended_checksum;
	u8		reserved[3];
} __attribute__((packed)) acpi_rsdp_v2_t;

typedef struct {
	char	signature[4];
	u32	length;
	u8	revision;
	u8	checksum;
	char	oem_id[6];
	char	oem_table_id[8];
	u32	oem_revision;
	u32	creator_id;
	u32	creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

typedef struct {
	acpi_sdt_header_t	header;
	u32			entries[];
} __attribute__((packed)) acpi_rsdt_t;

typedef struct {
	acpi_sdt_header_t	header;
	u64			entries[];
} __attribute__((packed)) acpi_xsdt_t;

typedef struct {
	u8	address_space;
	u8	bit_width;
	u8	bit_offset;
	u8	access_size;
	u64	address;
} __attribute__((packed)) acpi_gas_t;

#define	ACPI_GAS_SYSTEM_MEMORY	0
#define	ACPI_GAS_SYSTEM_IO	1

typedef struct {
	acpi_sdt_header_t	header;
	u32			firmware_ctrl;
	u32			dsdt;
	u8			reserved1;
	u8			preferred_pm_profile;
	u16			sci_interrupt;
	u32			smi_command_port;
	u8			acpi_enable;
	u8			acpi_disable;
	u8			s4bios_req;
	u8			pstate_control;
	u32			pm1a_event_block;
	u32			pm1b_event_block;
	u32			pm1a_control_block;
	u32			pm1b_control_block;
	u32			pm2_control_block;
	u32			pm_timer_block;
	u32			gpe0_block;
	u32			gpe1_block;
	u8			pm1_event_length;
	u8			pm1_control_length;
	u8			pm2_control_length;
	u8			pm_timer_length;
	u8			gpe0_length;
	u8			gpe1_length;
	u8			gpe1_base;
	u8			cstate_control;
	u16			worst_c2_latency;
	u16			worst_c3_latency;
	u16			flush_size;
	u16			flush_stride;
	u8			duty_offset;
	u8			duty_width;
	u8			day_alarm;
	u8			month_alarm;
	u8			century;
	u16			boot_arch_flags;
	u8			reserved2;
	u32			flags;
	acpi_gas_t		reset_reg;
	u8			reset_value;
	u16			arm_boot_arch;
	u8			fadt_minor_version;
	u64			x_firmware_ctrl;
	u64			x_dsdt;
	acpi_gas_t		x_pm1a_event_block;
	acpi_gas_t		x_pm1b_event_block;
	acpi_gas_t		x_pm1a_control_block;
	acpi_gas_t		x_pm1b_control_block;
	acpi_gas_t		x_pm2_control_block;
	acpi_gas_t		x_pm_timer_block;
	acpi_gas_t		x_gpe0_block;
	acpi_gas_t		x_gpe1_block;
} __attribute__((packed)) acpi_fadt_t;

#define	ACPI_FADT_WBINVD		(1 << 0)
#define	ACPI_FADT_WBINVD_FLUSH		(1 << 1)
#define	ACPI_FADT_C1_SUPPORTED		(1 << 2)
#define	ACPI_FADT_C2_MP_SUPPORTED	(1 << 3)
#define	ACPI_FADT_POWER_BUTTON		(1 << 4)
#define	ACPI_FADT_SLEEP_BUTTON		(1 << 5)
#define	ACPI_FADT_FIXED_RTC		(1 << 6)
#define	ACPI_FADT_S4_RTC_WAKE		(1 << 7)
#define	ACPI_FADT_TMR_VAL_EXT		(1 << 8)
#define	ACPI_FADT_DCK_CAP		(1 << 9)
#define	ACPI_FADT_RESET_REG_SUP		(1 << 10)
#define	ACPI_FADT_SEALED_CASE		(1 << 11)
#define	ACPI_FADT_HEADLESS		(1 << 12)
#define	ACPI_FADT_HW_REDUCED		(1 << 20)

#define	ACPI_BOOT_LEGACY_DEVICES	(1 << 0)
#define	ACPI_BOOT_8042			(1 << 1)

typedef struct {
	acpi_sdt_header_t	header;
	u32			local_apic_address;
	u32			flags;
} __attribute__((packed)) acpi_madt_t;

typedef struct {
	u8	type;
	u8	length;
} __attribute__((packed)) acpi_madt_entry_header_t;

#define	ACPI_MADT_LOCAL_APIC		0
#define	ACPI_MADT_IO_APIC		1
#define	ACPI_MADT_INT_SRC_OVERRIDE	2
#define	ACPI_MADT_NMI_SOURCE		3
#define	ACPI_MADT_LOCAL_APIC_NMI	4
#define	ACPI_MADT_LOCAL_APIC_ADDR	5
#define	ACPI_MADT_LOCAL_X2APIC		9

typedef struct {
	acpi_madt_entry_header_t	header;
	u8				acpi_processor_id;
	u8				apic_id;
	u32				flags;
} __attribute__((packed)) acpi_madt_local_apic_t;

typedef struct {
	acpi_madt_entry_header_t	header;
	u8				io_apic_id;
	u8				reserved;
	u32				io_apic_address;
	u32				global_system_interrupt_base;
} __attribute__((packed)) acpi_madt_io_apic_t;

typedef struct {
	acpi_madt_entry_header_t	header;
	u8				bus_source;
	u8				irq_source;
	u32				global_system_interrupt;
	u16				flags;
} __attribute__((packed)) acpi_madt_int_override_t;

typedef struct {
	acpi_sdt_header_t	header;
	u8			hardware_rev_id;
	u8			comparator_count_and_flags;
	u16			pci_vendor_id;
	acpi_gas_t		address;
	u8			hpet_number;
	u16			minimum_tick;
	u8			page_protection;
} __attribute__((packed)) acpi_hpet_t;

typedef struct {
	u64	base_address;
	u16	segment_group;
	u8	start_bus;
	u8	end_bus;
	u32	reserved;
} __attribute__((packed)) acpi_mcfg_entry_t;

typedef struct {
	acpi_sdt_header_t	header;
	u64			reserved;
} __attribute__((packed)) acpi_mcfg_t;

int			acpi_init_from_rsdp(void *rsdp);
int			acpi_init_from_multiboot2(void *mb2_info);
acpi_sdt_header_t	*acpi_find_table(const char *signature);
acpi_fadt_t		*acpi_get_fadt(void);
acpi_madt_t		*acpi_get_madt(void);
int			acpi_is_initialized(void);
int			acpi_get_revision(void);
int			acpi_validate_checksum(acpi_sdt_header_t *header);

int	acpi_madt_foreach(u8 entry_type,
    void (*callback)(acpi_madt_entry_header_t *, void *), void *ctx);
int	acpi_get_cpu_count(void);
u32	acpi_get_ioapic_address(void);
u32	acpi_get_local_apic_address(void);
int	acpi_has_dual_pic(void);
void	acpi_dump_tables(void);

#endif
