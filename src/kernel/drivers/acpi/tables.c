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

/*
 * tables.c — ACPI table lookup and validation utilities.
 *
 * Provides helpers on top of the core acpi.c for iterating MADT entries,
 * counting CPUs, and extracting commonly needed information from the
 * parsed ACPI tables.
 */

#include <kernel/drivers/acpi/acpi.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/* ── MADT iteration ──────────────────────────────────────────────────── */

/*
 * Iterate over all MADT entries of a given type and invoke a callback
 * for each one.  The callback receives the entry pointer and a user context.
 *
 * Returns the number of matching entries found, or -1 if MADT is absent.
 */
int acpi_madt_foreach(u8 entry_type,
                      void (*callback)(acpi_madt_entry_header_t *, void *),
                      void *ctx) {
  acpi_madt_t *madt = acpi_get_madt();
  if (!madt)
    return -1;

  u8 *ptr = (u8 *)madt + sizeof(acpi_madt_t);
  u8 *end = (u8 *)madt + madt->header.length;
  int count = 0;

  while (ptr < end) {
    acpi_madt_entry_header_t *entry = (acpi_madt_entry_header_t *)ptr;

    if (entry->length == 0)
      break; /* safety: prevent infinite loop */

    if (entry->type == entry_type) {
      if (callback)
        callback(entry, ctx);
      count++;
    }

    ptr += entry->length;
  }

  return count;
}

/* ── CPU enumeration ─────────────────────────────────────────────────── */

/*
 * Count the number of usable CPUs by iterating Local APIC entries
 * in the MADT.  Only entries with the "enabled" flag set are counted.
 */
static void count_cpu_cb(acpi_madt_entry_header_t *entry, void *ctx) {
  acpi_madt_local_apic_t *lapic = (acpi_madt_local_apic_t *)entry;
  int *count = (int *)ctx;

  if (lapic->flags & 1) { /* bit 0 = processor enabled */
    (*count)++;
  }
}

int acpi_get_cpu_count(void) {
  int count = 0;
  int res = acpi_madt_foreach(ACPI_MADT_LOCAL_APIC, count_cpu_cb, &count);
  if (res < 0)
    return -1;
  return count;
}

/* ── I/O APIC information ────────────────────────────────────────────── */

static void get_ioapic_cb(acpi_madt_entry_header_t *entry, void *ctx) {
  acpi_madt_io_apic_t *ioapic = (acpi_madt_io_apic_t *)entry;
  u32 *addr = (u32 *)ctx;

  /* Return the address of the first I/O APIC we find */
  if (*addr == 0) {
    *addr = ioapic->io_apic_address;
  }
}

/*
 * Get the base address of the first I/O APIC.
 * Returns 0 if not found.
 */
u32 acpi_get_ioapic_address(void) {
  u32 addr = 0;
  acpi_madt_foreach(ACPI_MADT_IO_APIC, get_ioapic_cb, &addr);
  return addr;
}

/* ── Local APIC address ──────────────────────────────────────────────── */

/*
 * Get the local APIC base address from the MADT header.
 * Returns 0 if MADT is absent.
 */
u32 acpi_get_local_apic_address(void) {
  acpi_madt_t *madt = acpi_get_madt();
  if (!madt)
    return 0;
  return madt->local_apic_address;
}

/* ── Dual PIC detection ──────────────────────────────────────────────── */

/*
 * Returns 1 if the system has dual 8259 PICs installed (indicated by
 * MADT flags bit 0).
 */
int acpi_has_dual_pic(void) {
  acpi_madt_t *madt = acpi_get_madt();
  if (!madt)
    return 0;
  return (madt->flags & 1) ? 1 : 0;
}

/* ── ACPI table dump (debug) ──────────────────────────────────────────── */

/*
 * Print a summary of all detected ACPI tables to COM1 for debugging.
 */
void acpi_dump_tables(void) {
  if (!acpi_is_initialized()) {
    com1_printf("[ACPI] not initialized\n");
    return;
  }

  com1_printf("[ACPI] ===== Table Dump =====\n");
  com1_printf("[ACPI] revision: %u\n", acpi_get_revision());

  acpi_fadt_t *fadt = acpi_get_fadt();
  if (fadt) {
    com1_printf("[ACPI] FADT:\n");
    com1_printf("[ACPI]   SCI interrupt   : %u\n", fadt->sci_interrupt);
    com1_printf("[ACPI]   SMI command port: 0x%x\n", fadt->smi_command_port);
    com1_printf("[ACPI]   PM1a event      : 0x%x\n", fadt->pm1a_event_block);
    com1_printf("[ACPI]   PM1a control    : 0x%x\n", fadt->pm1a_control_block);
    com1_printf("[ACPI]   PM1b control    : 0x%x\n", fadt->pm1b_control_block);
    com1_printf("[ACPI]   PM timer        : 0x%x\n", fadt->pm_timer_block);
    com1_printf("[ACPI]   flags           : 0x%x\n", fadt->flags);
    com1_printf("[ACPI]   century reg     : %u\n", fadt->century);
    com1_printf("[ACPI]   boot arch flags : 0x%x\n", fadt->boot_arch_flags);

    if (fadt->flags & ACPI_FADT_RESET_REG_SUP) {
      com1_printf("[ACPI]   reset reg addr  : 0x%x (space=%u, val=0x%x)\n",
                  (u32)fadt->reset_reg.address, fadt->reset_reg.address_space,
                  fadt->reset_value);
    }
  }

  acpi_madt_t *madt = acpi_get_madt();
  if (madt) {
    com1_printf("[ACPI] MADT:\n");
    com1_printf("[ACPI]   local APIC addr : 0x%x\n", madt->local_apic_address);
    com1_printf("[ACPI]   flags           : 0x%x\n", madt->flags);

    int cpus = acpi_get_cpu_count();
    com1_printf("[ACPI]   CPUs (enabled)  : %d\n", cpus);

    u32 ioapic = acpi_get_ioapic_address();
    if (ioapic) {
      com1_printf("[ACPI]   I/O APIC addr   : 0x%x\n", ioapic);
    }
  }

  /* Look for HPET */
  acpi_sdt_header_t *hpet = acpi_find_table("HPET");
  if (hpet) {
    acpi_hpet_t *h = (acpi_hpet_t *)hpet;
    com1_printf("[ACPI] HPET found at 0x%x\n", (u32)h->address.address);
  }

  /* Look for MCFG (PCIe) */
  acpi_sdt_header_t *mcfg = acpi_find_table("MCFG");
  if (mcfg) {
    com1_printf("[ACPI] MCFG (PCIe) found, len=%u\n", mcfg->length);
  }

  com1_printf("[ACPI] ===== End Dump =====\n");
}
