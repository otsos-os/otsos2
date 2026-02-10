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

#ifndef POWER_H
#define POWER_H

#include <mlibc/mlibc.h>

/* ── Power states ────────────────────────────────────────────────────── */

typedef enum {
  POWER_STATE_S0 = 0, /* Working (fully on) */
  POWER_STATE_S1 = 1, /* Sleeping: CPU stops, power to CPU/RAM maintained */
  POWER_STATE_S3 = 3, /* Suspend-to-RAM (STR) */
  POWER_STATE_S4 = 4, /* Suspend-to-Disk (hibernate) */
  POWER_STATE_S5 = 5, /* Soft-off (shutdown) */
} power_state_t;

/* ── Shutdown/reboot methods ─────────────────────────────────────────── */

typedef enum {
  POWER_METHOD_ACPI,      /* ACPI PM1a/PM1b control registers */
  POWER_METHOD_KEYBOARD,  /* PS/2 keyboard controller reset (0xFE) */
  POWER_METHOD_TRIPLE,    /* Triple-fault reboot */
  POWER_METHOD_RESET_REG, /* ACPI FADT reset register */
} power_method_t;

/* ── Power subsystem status ──────────────────────────────────────────── */

typedef struct {
  int initialized;
  int acpi_available;      /* 1 if ACPI FADT has PM registers */
  int reset_reg_available; /* 1 if FADT reset register is valid */
  u16 pm1a_control;        /* PM1a control port from FADT */
  u16 pm1b_control;        /* PM1b control port from FADT */
  u16 slp_typa_s5;         /* SLP_TYP value for S5 on PM1a */
  u16 slp_typb_s5;         /* SLP_TYP value for S5 on PM1b */
  u32 smi_command_port;    /* SMI command port from FADT */
  u8 acpi_enable_value;    /* Value to send to enable ACPI mode */
} power_info_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * power_init — initialise the power management subsystem.
 *   Should be called after acpi_init.  Extracts PM register info from FADT.
 *   Returns 0 on success, -1 if ACPI FADT is not available.
 */
int power_init(void);

/*
 * power_shutdown — perform a system shutdown (S5 soft-off).
 *   Tries ACPI first, falls back to QEMU-specific method.
 *   This function does not return on success.
 */
void power_shutdown(void);

/*
 * power_reboot — perform a system reboot.
 *   Tries: ACPI reset register → PS/2 keyboard reset → triple fault.
 *   This function does not return on success.
 */
void power_reboot(void);

/*
 * power_acpi_enable — enable ACPI mode by writing to SMI command port.
 *   Required on some systems before PM registers can be used.
 *   Returns 0 on success, -1 if SMI command port is unavailable.
 */
int power_acpi_enable(void);

/*
 * power_get_info — get a snapshot of the power subsystem state.
 */
power_info_t power_get_info(void);

/*
 * power_is_initialized — returns 1 if power_init succeeded.
 */
int power_is_initialized(void);


int power_register_shutdown_hook(void (*fn)(void));
void power_controller_shutdown(void);
void power_controller_reboot(void);
void power_controller_status(void);

#endif
