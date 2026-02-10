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

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/* ── internal state ──────────────────────────────────────────────────── */

static int g_acpi_initialized = 0;
static int g_acpi_revision = 0; /* 0 = ACPI 1.0,  2 = 2.0+ */

/* Root table pointer (RSDT or XSDT) */
static acpi_rsdt_t *g_rsdt = NULL;
static acpi_xsdt_t *g_xsdt = NULL;

/* Pre-cached important tables */
static acpi_fadt_t *g_fadt = NULL;
static acpi_madt_t *g_madt = NULL;

/* ── helpers ─────────────────────────────────────────────────────────── */

/*
 * Validate a contiguous block checksum (all bytes must sum to 0 mod 256).
 */
static int validate_checksum(const void *ptr, u32 length) {
  const u8 *bytes = (const u8 *)ptr;
  u8 sum = 0;
  for (u32 i = 0; i < length; i++) {
    sum += bytes[i];
  }
  return (sum == 0) ? 0 : -1;
}

int acpi_validate_checksum(acpi_sdt_header_t *header) {
  if (!header)
    return -1;
  return validate_checksum(header, header->length);
}

/*
 * Compare 4-byte signature (not null-terminated in ACPI tables).
 */
static int sig_match(const char *a, const char *b) {
  return a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3];
}

/* ── RSDP validation ────────────────────────────────────────────────── */

static int validate_rsdp(acpi_rsdp_v1_t *rsdp) {
  /* Check "RSD PTR " signature (8 bytes) */
  const char *expected = "RSD PTR ";
  for (int i = 0; i < 8; i++) {
    if (rsdp->signature[i] != expected[i]) {
      return -1;
    }
  }

  /* Validate v1 checksum (first 20 bytes) */
  if (validate_checksum(rsdp, 20) != 0) {
    panic("[ACPI] RSDP v1 checksum failed\n");
    return -1;
  }

  /* If ACPI 2.0+, also validate extended checksum */
  if (rsdp->revision >= 2) {
    acpi_rsdp_v2_t *rsdp2 = (acpi_rsdp_v2_t *)rsdp;
    if (validate_checksum(rsdp2, rsdp2->length) != 0) {
      panic("[ACPI] RSDP v2 extended checksum failed\n");
      return -1;
    }
  }

  return 0;
}

/* ── BIOS area scan for RSDP ────────────────────────────────────────── */

/*
 * Scan a physical memory range on 16-byte boundaries looking for "RSD PTR ".
 */
static acpi_rsdp_v1_t *scan_for_rsdp(u64 start, u64 end) {
  for (u64 addr = start; addr < end; addr += 16) {
    acpi_rsdp_v1_t *candidate = (acpi_rsdp_v1_t *)addr;
    if (validate_rsdp(candidate) == 0) {
      return candidate;
    }
  }
  return NULL;
}

/*
 * Find RSDP by scanning standard BIOS areas:
 *   1) EBDA (Extended BIOS Data Area) — first 1 KB
 *   2) 0x000E0000 — 0x000FFFFF (upper BIOS ROM area)
 */
static acpi_rsdp_v1_t *find_rsdp_bios(void) {
  /* Try EBDA first: segment stored at 0x040E */
  u16 ebda_seg = *(u16 *)0x040E;
  u64 ebda_addr = (u64)ebda_seg << 4;
  if (ebda_addr) {
    acpi_rsdp_v1_t *rsdp = scan_for_rsdp(ebda_addr, ebda_addr + 1024);
    if (rsdp)
      return rsdp;
  }

  /* Scan upper BIOS area */
  return scan_for_rsdp(0x000E0000, 0x00100000);
}

/* ── Parse RSDT/XSDT and cache tables ───────────────────────────────── */

static void parse_rsdt(acpi_rsdt_t *rsdt) {
  if (!rsdt)
    return;

  if (acpi_validate_checksum(&rsdt->header) != 0) {
    panic("[ACPI] RSDT checksum failed\n");
    return;
  }

  u32 num_entries = (rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
  com1_printf("[ACPI] RSDT has %u entries\n", num_entries);

  for (u32 i = 0; i < num_entries; i++) {
    acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)(u64)rsdt->entries[i];
    if (!sdt)
      continue;

    com1_printf("[ACPI]   table: %c%c%c%c  len=%u\n", sdt->signature[0],
                sdt->signature[1], sdt->signature[2], sdt->signature[3],
                sdt->length);

    /* Cache well-known tables */
    if (sig_match(sdt->signature, "FACP")) {
      g_fadt = (acpi_fadt_t *)sdt;
    } else if (sig_match(sdt->signature, "APIC")) {
      g_madt = (acpi_madt_t *)sdt;
    }
  }
}

static void parse_xsdt(acpi_xsdt_t *xsdt) {
  if (!xsdt)
    return;

  if (acpi_validate_checksum(&xsdt->header) != 0) {
    panic("[ACPI] XSDT checksum failed\n");
    return;
  }

  u32 num_entries = (xsdt->header.length - sizeof(acpi_sdt_header_t)) / 8;
  com1_printf("[ACPI] XSDT has %u entries\n", num_entries);

  for (u32 i = 0; i < num_entries; i++) {
    acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)xsdt->entries[i];
    if (!sdt)
      continue;

    com1_printf("[ACPI]   table: %c%c%c%c  len=%u\n", sdt->signature[0],
                sdt->signature[1], sdt->signature[2], sdt->signature[3],
                sdt->length);

    if (sig_match(sdt->signature, "FACP")) {
      g_fadt = (acpi_fadt_t *)sdt;
    } else if (sig_match(sdt->signature, "APIC")) {
      g_madt = (acpi_madt_t *)sdt;
    }
  }
}

/* ── Public API ──────────────────────────────────────────────────────── */

int acpi_init_from_rsdp(void *rsdp_ptr) {
  acpi_rsdp_v1_t *rsdp = (acpi_rsdp_v1_t *)rsdp_ptr;

  if (validate_rsdp(rsdp) != 0) {
    panic("[ACPI] invalid RSDP at %p\n", rsdp_ptr);
    return -1;
  }

  g_acpi_revision = rsdp->revision;

  com1_printf("[ACPI] RSDP found at %p, revision %u, OEM: %.6s\n", rsdp_ptr,
              rsdp->revision, rsdp->oem_id);

  if (rsdp->revision >= 2) {
    /* ACPI 2.0+ — prefer XSDT */
    acpi_rsdp_v2_t *rsdp2 = (acpi_rsdp_v2_t *)rsdp;
    if (rsdp2->xsdt_address) {
      g_xsdt = (acpi_xsdt_t *)rsdp2->xsdt_address;
      com1_printf("[ACPI] using XSDT at %p\n", (void *)rsdp2->xsdt_address);
      parse_xsdt(g_xsdt);
    } else {
      /* Fall back to RSDT */
      g_rsdt = (acpi_rsdt_t *)(u64)rsdp->rsdt_address;
      com1_printf("[ACPI] XSDT null, using RSDT at %p\n",
                  (void *)(u64)rsdp->rsdt_address);
      parse_rsdt(g_rsdt);
    }
  } else {
    /* ACPI 1.0 — only RSDT */
    g_rsdt = (acpi_rsdt_t *)(u64)rsdp->rsdt_address;
    com1_printf("[ACPI] using RSDT at %p\n", (void *)(u64)rsdp->rsdt_address);
    parse_rsdt(g_rsdt);
  }

  if (g_fadt) {
    com1_printf("[ACPI] FADT found: SCI int=%u, PM1a=0x%x, SMI cmd=0x%x\n",
                g_fadt->sci_interrupt, g_fadt->pm1a_control_block,
                g_fadt->smi_command_port);
  }

  if (g_madt) {
    com1_printf("[ACPI] MADT found: local APIC at 0x%x, flags=0x%x\n",
                g_madt->local_apic_address, g_madt->flags);
  }

  g_acpi_initialized = 1;
  return 0;
}

int acpi_init_from_multiboot2(void *mb2_info) {
  multiboot2_info_t *mb = (multiboot2_info_t *)mb2_info;

  /* Try ACPI_NEW (XSDT) tag first */
  multiboot2_tag_t *tag_new =
      multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_ACPI_NEW);
  if (tag_new) {
    /* RSDP data starts immediately after the 8-byte tag header */
    void *rsdp = (void *)((u8 *)tag_new + 8);
    com1_printf("[ACPI] found ACPI_NEW multiboot2 tag\n");
    return acpi_init_from_rsdp(rsdp);
  }

  /* Fall back to ACPI_OLD (RSDT) tag */
  multiboot2_tag_t *tag_old =
      multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_ACPI_OLD);
  if (tag_old) {
    void *rsdp = (void *)((u8 *)tag_old + 8);
    com1_printf("[ACPI] found ACPI_OLD multiboot2 tag\n");
    return acpi_init_from_rsdp(rsdp);
  }

  /* Last resort: scan BIOS memory */
  com1_printf("[ACPI] no multiboot2 ACPI tag, scanning BIOS area...\n");
  acpi_rsdp_v1_t *rsdp = find_rsdp_bios();
  if (rsdp) {
    return acpi_init_from_rsdp(rsdp);
  }

  panic("[ACPI] RSDP not found!\n");
  return -1;
}

acpi_sdt_header_t *acpi_find_table(const char *signature) {
  if (!g_acpi_initialized)
    return NULL;

  if (g_xsdt) {
    u32 n = (g_xsdt->header.length - sizeof(acpi_sdt_header_t)) / 8;
    for (u32 i = 0; i < n; i++) {
      acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)g_xsdt->entries[i];
      if (sdt && sig_match(sdt->signature, signature)) {
        return sdt;
      }
    }
  } else if (g_rsdt) {
    u32 n = (g_rsdt->header.length - sizeof(acpi_sdt_header_t)) / 4;
    for (u32 i = 0; i < n; i++) {
      acpi_sdt_header_t *sdt = (acpi_sdt_header_t *)(u64)g_rsdt->entries[i];
      if (sdt && sig_match(sdt->signature, signature)) {
        return sdt;
      }
    }
  }

  return NULL;
}

acpi_fadt_t *acpi_get_fadt(void) { return g_fadt; }
acpi_madt_t *acpi_get_madt(void) { return g_madt; }
int acpi_is_initialized(void) { return g_acpi_initialized; }
int acpi_get_revision(void) { return g_acpi_revision; }
