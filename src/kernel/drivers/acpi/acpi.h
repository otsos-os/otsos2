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

#ifndef ACPI_H
#define ACPI_H

#include <mlibc/mlibc.h>

/* ── RSDP (Root System Description Pointer) ─────────────────────────── */

/* ACPI 1.0 RSDP — 20 bytes */
typedef struct {
  char signature[8]; /* "RSD PTR " */
  u8 checksum;
  char oem_id[6];
  u8 revision;      /* 0 = ACPI 1.0,  2 = ACPI 2.0+ */
  u32 rsdt_address; /* physical address of RSDT */
} __attribute__((packed)) acpi_rsdp_v1_t;

/* ACPI 2.0+ RSDP — extends v1 to 36 bytes */
typedef struct {
  acpi_rsdp_v1_t v1;
  u32 length;       /* total length of RSDP (36) */
  u64 xsdt_address; /* physical address of XSDT */
  u8 extended_checksum;
  u8 reserved[3];
} __attribute__((packed)) acpi_rsdp_v2_t;

/* ── Generic SDT Header (common to every ACPI table) ───────────────── */
typedef struct {
  char signature[4];
  u32 length;
  u8 revision;
  u8 checksum;
  char oem_id[6];
  char oem_table_id[8];
  u32 oem_revision;
  u32 creator_id;
  u32 creator_revision;
} __attribute__((packed)) acpi_sdt_header_t;

/* ── RSDT (Root System Description Table, ACPI 1.0) ─────────────────── */
typedef struct {
  acpi_sdt_header_t header;
  u32 entries[]; /* array of 32-bit physical pointers to SDTs */
} __attribute__((packed)) acpi_rsdt_t;

/* ── XSDT (Extended SDT, ACPI 2.0+) ─────────────────────────────────── */
typedef struct {
  acpi_sdt_header_t header;
  u64 entries[]; /* array of 64-bit physical pointers to SDTs */
} __attribute__((packed)) acpi_xsdt_t;

/* ── Generic Address Structure (GAS) ─────────────────────────────────── */
typedef struct {
  u8 address_space; /* 0 = system memory, 1 = system I/O */
  u8 bit_width;
  u8 bit_offset;
  u8 access_size; /* 0=undefined, 1=byte, 2=word, 3=dword, 4=qword */
  u64 address;
} __attribute__((packed)) acpi_gas_t;

/* Address space IDs */
#define ACPI_GAS_SYSTEM_MEMORY 0
#define ACPI_GAS_SYSTEM_IO 1

/* ── FADT (Fixed ACPI Description Table) ─────────────────────────────── */
typedef struct {
  acpi_sdt_header_t header;

  u32 firmware_ctrl; /* physical address of FACS */
  u32 dsdt;          /* physical address of DSDT */

  u8 reserved1;            /* used in ACPI 1.0 for interrupt model */
  u8 preferred_pm_profile; /* 0=unspec, 1=desktop, 2=mobile, ... */
  u16 sci_interrupt;       /* SCI interrupt number */
  u32 smi_command_port;    /* SMI command port */
  u8 acpi_enable;          /* value to write to smi_command to enable ACPI */
  u8 acpi_disable;         /* value to write to smi_command to disable ACPI */
  u8 s4bios_req;
  u8 pstate_control;

  u32 pm1a_event_block;
  u32 pm1b_event_block;
  u32 pm1a_control_block;
  u32 pm1b_control_block;
  u32 pm2_control_block;
  u32 pm_timer_block;
  u32 gpe0_block;
  u32 gpe1_block;

  u8 pm1_event_length;
  u8 pm1_control_length;
  u8 pm2_control_length;
  u8 pm_timer_length;
  u8 gpe0_length;
  u8 gpe1_length;
  u8 gpe1_base;
  u8 cstate_control;

  u16 worst_c2_latency;
  u16 worst_c3_latency;
  u16 flush_size;
  u16 flush_stride;

  u8 duty_offset;
  u8 duty_width;

  u8 day_alarm;
  u8 month_alarm;
  u8 century; /* RTC century register index (CMOS) */

  u16 boot_arch_flags; /* IA-PC boot architecture flags */
  u8 reserved2;
  u32 flags;

  acpi_gas_t reset_reg;
  u8 reset_value;
  u16 arm_boot_arch;
  u8 fadt_minor_version;

  /* ACPI 2.0+ extended fields */
  u64 x_firmware_ctrl;
  u64 x_dsdt;

  acpi_gas_t x_pm1a_event_block;
  acpi_gas_t x_pm1b_event_block;
  acpi_gas_t x_pm1a_control_block;
  acpi_gas_t x_pm1b_control_block;
  acpi_gas_t x_pm2_control_block;
  acpi_gas_t x_pm_timer_block;
  acpi_gas_t x_gpe0_block;
  acpi_gas_t x_gpe1_block;
} __attribute__((packed)) acpi_fadt_t;

/* FADT flags */
#define ACPI_FADT_WBINVD (1 << 0)
#define ACPI_FADT_WBINVD_FLUSH (1 << 1)
#define ACPI_FADT_C1_SUPPORTED (1 << 2)
#define ACPI_FADT_C2_MP_SUPPORTED (1 << 3)
#define ACPI_FADT_POWER_BUTTON (1 << 4)
#define ACPI_FADT_SLEEP_BUTTON (1 << 5)
#define ACPI_FADT_FIXED_RTC (1 << 6)
#define ACPI_FADT_S4_RTC_WAKE (1 << 7)
#define ACPI_FADT_TMR_VAL_EXT (1 << 8) /* PM timer is 32-bit */
#define ACPI_FADT_DCK_CAP (1 << 9)
#define ACPI_FADT_RESET_REG_SUP (1 << 10)
#define ACPI_FADT_SEALED_CASE (1 << 11)
#define ACPI_FADT_HEADLESS (1 << 12)
#define ACPI_FADT_HW_REDUCED (1 << 20)

/* Boot architecture flags */
#define ACPI_BOOT_LEGACY_DEVICES (1 << 0)
#define ACPI_BOOT_8042 (1 << 1)

/* ── MADT (Multiple APIC Description Table) ──────────────────────────── */
typedef struct {
  acpi_sdt_header_t header;
  u32 local_apic_address;
  u32 flags; /* bit 0 = dual 8259 installed */
} __attribute__((packed)) acpi_madt_t;

/* MADT entry header */
typedef struct {
  u8 type;
  u8 length;
} __attribute__((packed)) acpi_madt_entry_header_t;

/* MADT entry types */
#define ACPI_MADT_LOCAL_APIC 0
#define ACPI_MADT_IO_APIC 1
#define ACPI_MADT_INT_SRC_OVERRIDE 2
#define ACPI_MADT_NMI_SOURCE 3
#define ACPI_MADT_LOCAL_APIC_NMI 4
#define ACPI_MADT_LOCAL_APIC_ADDR 5
#define ACPI_MADT_LOCAL_X2APIC 9

/* Local APIC entry */
typedef struct {
  acpi_madt_entry_header_t header;
  u8 acpi_processor_id;
  u8 apic_id;
  u32 flags; /* bit 0 = enabled */
} __attribute__((packed)) acpi_madt_local_apic_t;

/* I/O APIC entry */
typedef struct {
  acpi_madt_entry_header_t header;
  u8 io_apic_id;
  u8 reserved;
  u32 io_apic_address;
  u32 global_system_interrupt_base;
} __attribute__((packed)) acpi_madt_io_apic_t;

/* Interrupt Source Override entry */
typedef struct {
  acpi_madt_entry_header_t header;
  u8 bus_source;
  u8 irq_source;
  u32 global_system_interrupt;
  u16 flags;
} __attribute__((packed)) acpi_madt_int_override_t;

/* ── HPET (High-Precision Event Timer) ───────────────────────────────── */
typedef struct {
  acpi_sdt_header_t header;
  u8 hardware_rev_id;
  u8 comparator_count_and_flags; /* bits[4:0] = count, bit 5 = 64-bit */
  u16 pci_vendor_id;
  acpi_gas_t address;
  u8 hpet_number;
  u16 minimum_tick;
  u8 page_protection;
} __attribute__((packed)) acpi_hpet_t;

/* ── MCFG (PCI Express memory-mapped config) ─────────────────────────── */
typedef struct {
  u64 base_address;
  u16 segment_group;
  u8 start_bus;
  u8 end_bus;
  u32 reserved;
} __attribute__((packed)) acpi_mcfg_entry_t;

typedef struct {
  acpi_sdt_header_t header;
  u64 reserved;
  /* followed by acpi_mcfg_entry_t entries[] */
} __attribute__((packed)) acpi_mcfg_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * acpi_init_from_rsdp — initialise ACPI from an RSDP pointer.
 *   @rsdp  pointer to RSDP (typically from multiboot2 tag or BIOS scan)
 *   returns 0 on success, -1 on failure
 */
int acpi_init_from_rsdp(void *rsdp);

/*
 * acpi_init_from_multiboot2 — initialise ACPI from multiboot2 info struct.
 *   attempts to locate ACPI_NEW (XSDT) tag first, falls back to ACPI_OLD
 * (RSDT). returns 0 on success, -1 on failure
 */
int acpi_init_from_multiboot2(void *mb2_info);

/*
 * acpi_find_table — search for an ACPI table by its 4-char signature.
 *   e.g. "FACP" for FADT, "APIC" for MADT, "HPET" for HPET.
 *   returns a pointer to the table header, or NULL if not found.
 */
acpi_sdt_header_t *acpi_find_table(const char *signature);

/*
 * acpi_get_fadt — return cached pointer to FADT, or NULL.
 */
acpi_fadt_t *acpi_get_fadt(void);

/*
 * acpi_get_madt — return cached pointer to MADT, or NULL.
 */
acpi_madt_t *acpi_get_madt(void);

/*
 * acpi_is_initialized — 1 if ACPI was successfully parsed, 0 otherwise.
 */
int acpi_is_initialized(void);

/*
 * acpi_get_revision — return ACPI revision (0 = 1.0, 2 = 2.0+).
 */
int acpi_get_revision(void);

/*
 * acpi_validate_checksum — validate the checksum of an ACPI table.
 *   returns 0 if valid, -1 if not.
 */
int acpi_validate_checksum(acpi_sdt_header_t *header);

#endif
