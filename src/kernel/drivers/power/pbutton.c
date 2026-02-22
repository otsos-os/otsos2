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
#include <kernel/drivers/power/pbutton.h>
#include <kernel/drivers/power/power.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define PM1_STS_PWRBTN (1 << 8)

static int g_pbutton_initialized = 0;
static int g_pbutton_shutdown_in_progress = 0;
static u16 g_pm1a_event = 0;
static u16 g_pm1b_event = 0;

static int pbutton_handle_event(u16 pm1_event_port) {
  if (pm1_event_port == 0) {
    return 0;
  }

  u16 status = inw(pm1_event_port);
  if ((status & PM1_STS_PWRBTN) == 0) {
    return 0;
  }

  outw(pm1_event_port, PM1_STS_PWRBTN);

  if (!g_pbutton_shutdown_in_progress) {
    g_pbutton_shutdown_in_progress = 1;
    com1_printf("[PWRBTN] Power button pressed, shutting down...\n");
    power_controller_shutdown();
  }

  return 1;
}

int power_button_init(void) {
  g_pbutton_initialized = 0;
  g_pbutton_shutdown_in_progress = 0;
  g_pm1a_event = 0;
  g_pm1b_event = 0;

  if (!acpi_is_initialized()) {
    return -1;
  }

  acpi_fadt_t *fadt = acpi_get_fadt();
  if (!fadt) {
    return -1;
  }

  g_pm1a_event = (u16)fadt->pm1a_event_block;
  g_pm1b_event = (u16)fadt->pm1b_event_block;

  if (g_pm1a_event == 0 && g_pm1b_event == 0) {
    return -1;
  }

  g_pbutton_initialized = 1;
  com1_printf("[PWRBTN] initialized: PM1a_EVT=0x%x PM1b_EVT=0x%x\n",
              g_pm1a_event, g_pm1b_event);
  return 0;
}

void power_button_poll(void) {
  if (!g_pbutton_initialized || g_pbutton_shutdown_in_progress) {
    return;
  }

  if (pbutton_handle_event(g_pm1a_event)) {
    return;
  }
  (void)pbutton_handle_event(g_pm1b_event);
}

int power_button_is_initialized(void) { return g_pbutton_initialized; }
