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
 * power.c — Power management: shutdown, reboot, ACPI enable.
 *
 * Uses ACPI FADT to obtain PM1a/PM1b control registers and the reset
 * register.  Falls back to PS/2 keyboard controller reset or triple-fault
 * if ACPI is unavailable.
 */

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/power/power.h>
#include <kernel/panic.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/* ── PM1 control register bits ───────────────────────────────────────── */

/*
 * SLP_EN is bit 13 in PM1_CNT.
 * SLP_TYP is bits [12:10] in PM1_CNT.
 */
#define PM1_SLP_EN (1 << 13)
#define PM1_SLP_TYP_MASK (7 << 10)

/* ── PS/2 keyboard controller reset ──────────────────────────────────── */
#define KB_CTRL_PORT 0x64
#define KB_RESET_CMD 0xFE

/* ── internal state ──────────────────────────────────────────────────── */

static power_info_t g_power = {0};

/* ── DSDT S5 object parsing ──────────────────────────────────────────── */

/*
 * Try to find the \_S5 object in the DSDT to determine the SLP_TYP values
 * for S5 (soft-off).  This is a simplified byte-pattern search, not a
 * full AML interpreter.
 *
 * The \_S5_ object typically looks like:
 *   08 5F 53 35 5F 12 ...   (NameOp, _S5_, PackageOp, ...)
 *
 * The SLP_TYP values follow the PackageOp header.
 */
static int parse_s5_from_dsdt(acpi_fadt_t *fadt) {
  if (!fadt)
    return -1;

  /* Get DSDT address (prefer extended if ACPI 2.0+) */
  u64 dsdt_addr = 0;
  if (acpi_get_revision() >= 2 && fadt->x_dsdt) {
    dsdt_addr = fadt->x_dsdt;
  } else {
    dsdt_addr = (u64)fadt->dsdt;
  }

  if (!dsdt_addr) {
    com1_printf("[POWER] no DSDT found\n");
    return -1;
  }

  acpi_sdt_header_t *dsdt = (acpi_sdt_header_t *)dsdt_addr;

  /* Validate DSDT signature */
  if (dsdt->signature[0] != 'D' || dsdt->signature[1] != 'S' ||
      dsdt->signature[2] != 'D' || dsdt->signature[3] != 'T') {
    com1_printf("[POWER] invalid DSDT signature\n");
    return -1;
  }

  com1_printf("[POWER] DSDT at %p, length %u\n", (void *)dsdt_addr,
              dsdt->length);

  /*
   * Search for the byte sequence: "_S5_"
   * In AML: 0x5F 0x53 0x35 0x5F
   */
  u8 *aml = (u8 *)dsdt + sizeof(acpi_sdt_header_t);
  u32 aml_length = dsdt->length - sizeof(acpi_sdt_header_t);

  for (u32 i = 0; i < aml_length - 4; i++) {
    if (aml[i] == '_' && aml[i + 1] == 'S' && aml[i + 2] == '5' &&
        aml[i + 3] == '_') {

      com1_printf("[POWER] found _S5_ at DSDT offset %u\n", i);

      /*
       * The preceding byte should be NameOp (0x08) or it could be
       * part of a scope path.  Skip to the Package data.
       *
       * Typical encoding after _S5_:
       *   12 0A 04 0A <SLP_TYPa> 0A <SLP_TYPb> ...
       *   (PackageOp, PkgLength, NumElements, BytePrefix, value, ...)
       */
      u8 *s5_ptr = aml + i + 4; /* skip past "_S5_" */

      /* Check for PackageOp (0x12) */
      if (*s5_ptr == 0x12) {
        s5_ptr++;

        /* Skip package length (simplified: assume single-byte PkgLength) */
        u8 pkg_len = *s5_ptr;
        s5_ptr++;

        /* Skip number of elements */
        s5_ptr++;

        /* Read SLP_TYPa: may be a BytePrefix (0x0A) followed by value,
         * or a raw byte if the encoding is compact */
        if (*s5_ptr == 0x0A) {
          s5_ptr++;
        }
        g_power.slp_typa_s5 = *s5_ptr;
        s5_ptr++;

        /* Read SLP_TYPb */
        if (*s5_ptr == 0x0A) {
          s5_ptr++;
        }
        g_power.slp_typb_s5 = *s5_ptr;

        com1_printf("[POWER] S5 SLP_TYP: a=%u, b=%u\n", g_power.slp_typa_s5,
                    g_power.slp_typb_s5);
        (void)pkg_len;
        return 0;
      }
    }
  }

  com1_printf("[POWER] _S5_ object not found in DSDT\n");
  return -1;
}

/* ── Public API ──────────────────────────────────────────────────────── */

int power_init(void) {
  memset(&g_power, 0, sizeof(g_power));

  if (!acpi_is_initialized()) {
    com1_printf("[POWER] ACPI not available, limited power management\n");
    g_power.initialized = 1;
    return -1;
  }

  acpi_fadt_t *fadt = acpi_get_fadt();
  if (!fadt) {
    com1_printf("[POWER] FADT not found, limited power management\n");
    g_power.initialized = 1;
    return -1;
  }

  /* Extract PM register addresses from FADT */
  g_power.pm1a_control = (u16)fadt->pm1a_control_block;
  g_power.pm1b_control = (u16)fadt->pm1b_control_block;
  g_power.smi_command_port = fadt->smi_command_port;
  g_power.acpi_enable_value = fadt->acpi_enable;
  g_power.acpi_available = 1;

  /* Check if FADT reset register is supported */
  if (fadt->flags & ACPI_FADT_RESET_REG_SUP) {
    g_power.reset_reg_available = 1;
    com1_printf("[POWER] ACPI reset register available at 0x%x\n",
                (u32)fadt->reset_reg.address);
  }

  /* Parse DSDT to find S5 sleep type values for shutdown */
  parse_s5_from_dsdt(fadt);

  g_power.initialized = 1;

  com1_printf("[POWER] initialized: PM1a=0x%x PM1b=0x%x SLP_TYP_S5=%u/%u\n",
              g_power.pm1a_control, g_power.pm1b_control, g_power.slp_typa_s5,
              g_power.slp_typb_s5);

  return 0;
}

void power_shutdown(void) {
  com1_printf("[POWER] shutting down...\n");

  __asm__ volatile("cli"); /* disable interrupts for safety */

  /* Method 1: ACPI S5 soft-off via PM1 control registers */
  if (g_power.acpi_available && g_power.pm1a_control) {
    u16 slp_typa = (g_power.slp_typa_s5 << 10) | PM1_SLP_EN;
    com1_printf("[POWER] writing 0x%x to PM1a control (0x%x)\n", slp_typa,
                g_power.pm1a_control);
    outw(g_power.pm1a_control, slp_typa);

    if (g_power.pm1b_control) {
      u16 slp_typb = (g_power.slp_typb_s5 << 10) | PM1_SLP_EN;
      outw(g_power.pm1b_control, slp_typb);
    }

    /* If we reach here, ACPI shutdown didn't work immediately.
     * Give it a moment. */
    for (volatile int i = 0; i < 10000000; i++) {
      __asm__ volatile("pause");
    }
  }

  panic("[POWER] ACPI S5 shutdown failed, system cannot power off and please sent the logs com1 (or tty0) issue in github\n");
}

void power_reboot(void) {
  com1_printf("[POWER] rebooting...\n");

  __asm__ volatile("cli");

  /* Method 1: ACPI reset register (if available) */
  if (g_power.acpi_available && g_power.reset_reg_available) {
    acpi_fadt_t *fadt = acpi_get_fadt();
    if (fadt) {
      if (fadt->reset_reg.address_space == ACPI_GAS_SYSTEM_IO) {
        com1_printf("[POWER] ACPI reset via I/O port 0x%x = 0x%x\n",
                    (u32)fadt->reset_reg.address, fadt->reset_value);
        outb((u16)fadt->reset_reg.address, fadt->reset_value);
      } else if (fadt->reset_reg.address_space == ACPI_GAS_SYSTEM_MEMORY) {
        com1_printf("[POWER] ACPI reset via MMIO 0x%x = 0x%x\n",
                    (u32)fadt->reset_reg.address, fadt->reset_value);
        volatile u8 *reg = (volatile u8 *)fadt->reset_reg.address;
        *reg = fadt->reset_value;
      }

      /* Wait a bit for the reset to take effect */
      for (volatile int i = 0; i < 10000000; i++) {
        __asm__ volatile("pause");
      }
    }
  }

  /* Method 2: PS/2 keyboard controller reset (pulse CPU reset line) */
  com1_printf("[POWER] trying PS/2 keyboard controller reset\n");

  /* Wait for the keyboard controller input buffer to be clear */
  u8 good = 0x02;
  while (good & 0x02) {
    good = inb(KB_CTRL_PORT);
  }
  outb(KB_CTRL_PORT, KB_RESET_CMD);

  /* Wait a bit */
  for (volatile int i = 0; i < 10000000; i++) {
    __asm__ volatile("pause");
  }

  /* Method 3: Triple-fault — load a zero-length IDT and trigger interrupt */
  com1_printf("[POWER] triple-faulting...\n");
  struct {
    u16 limit;
    u64 base;
  } __attribute__((packed)) null_idt = {0, 0};
  __asm__ volatile("lidt %0" : : "m"(null_idt));
  __asm__ volatile("int $0x03");

  /* Should never reach here */
  while (1) {
    __asm__ volatile("hlt");
  }
}

int power_acpi_enable(void) {
  if (!g_power.acpi_available || !g_power.smi_command_port) {
    com1_printf("[POWER] SMI command port not available\n");
    return -1;
  }

  /* Check if ACPI is already enabled by reading PM1a control */
  u16 pm1a = inw(g_power.pm1a_control);
  if (pm1a & 1) { /* SCI_EN bit = bit 0 */
    com1_printf("[POWER] ACPI already enabled\n");
    return 0;
  }

  com1_printf("[POWER] enabling ACPI via SMI cmd port 0x%x, value 0x%x\n",
              g_power.smi_command_port, g_power.acpi_enable_value);
  outb((u16)g_power.smi_command_port, g_power.acpi_enable_value);

  /* Poll for SCI_EN bit to be set (with timeout) */
  for (int i = 0; i < 3000; i++) {
    pm1a = inw(g_power.pm1a_control);
    if (pm1a & 1) {
      com1_printf("[POWER] ACPI mode enabled successfully\n");
      return 0;
    }
    /* Simple delay */
    for (volatile int j = 0; j < 10000; j++) {
      __asm__ volatile("pause");
    }
  }

  panic("[POWER] failed to enable ACPI mode (timeout)\n");
  return -1;
}

power_info_t power_get_info(void) { return g_power; }
int power_is_initialized(void) { return g_power.initialized; }
