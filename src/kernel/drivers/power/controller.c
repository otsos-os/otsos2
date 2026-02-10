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
 * controller.c — High-level power state controller.
 *
 * Orchestrates power state transitions by coordinating with other
 * kernel subsystems (filesystem sync, process termination, etc.)
 * before invoking the low-level power.c routines.
 */

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/power/power.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

/* ── Pre-shutdown hooks ──────────────────────────────────────────────── */

/*
 * Maximum number of registered shutdown hooks.
 * Hooks are called in order before shutdown/reboot.
 */
#define MAX_SHUTDOWN_HOOKS 16

typedef void (*shutdown_hook_fn)(void);

static shutdown_hook_fn g_shutdown_hooks[MAX_SHUTDOWN_HOOKS] = {0};
static int g_shutdown_hook_count = 0;

/*
 * Register a function to be called before shutdown/reboot.
 * Useful for flushing filesystem buffers, saving state, etc.
 *
 * Returns 0 on success, -1 if the hook table is full.
 */
int power_register_shutdown_hook(shutdown_hook_fn fn) {
  if (!fn)
    return -1;
  if (g_shutdown_hook_count >= MAX_SHUTDOWN_HOOKS) {
    com1_printf("[POWER] shutdown hook table full\n");
    return -1;
  }
  g_shutdown_hooks[g_shutdown_hook_count++] = fn;
  return 0;
}

/*
 * Execute all registered shutdown hooks.
 */
static void run_shutdown_hooks(void) {
  com1_printf("[POWER] running %d shutdown hooks...\n", g_shutdown_hook_count);
  for (int i = 0; i < g_shutdown_hook_count; i++) {
    if (g_shutdown_hooks[i]) {
      g_shutdown_hooks[i]();
    }
  }
  com1_printf("[POWER] all hooks completed\n");
}

/* ── Power controller API ────────────────────────────────────────────── */

/*
 * power_controller_shutdown — graceful shutdown sequence.
 *
 * 1. Log the shutdown intent
 * 2. Run all registered shutdown hooks (e.g. fs flush)
 * 3. Disable interrupts
 * 4. Call power_shutdown() (does not return)
 */
void power_controller_shutdown(void) {
  com1_printf("[POWER] === SYSTEM SHUTDOWN INITIATED ===\n");

  /* Step 1: run pre-shutdown hooks */
  run_shutdown_hooks();

  /* Step 2: actual shutdown */
  power_shutdown();

  /* Should never reach here */
  __builtin_unreachable();
}

/*
 * power_controller_reboot — graceful reboot sequence.
 *
 * 1. Log the reboot intent
 * 2. Run all registered shutdown hooks
 * 3. Call power_reboot() (does not return)
 */
void power_controller_reboot(void) {
  com1_printf("[POWER] === SYSTEM REBOOT INITIATED ===\n");

  /* Step 1: run pre-shutdown hooks */
  run_shutdown_hooks();

  /* Step 2: actual reboot */
  power_reboot();

  /* Should never reach here */
  __builtin_unreachable();
}

/*
 * power_controller_status — print current power subsystem status to COM1.
 */
void power_controller_status(void) {
  power_info_t info = power_get_info();

  com1_printf("[POWER] === Status ===\n");
  com1_printf("[POWER]   initialized    : %s\n",
              info.initialized ? "yes" : "no");
  com1_printf("[POWER]   ACPI available : %s\n",
              info.acpi_available ? "yes" : "no");
  com1_printf("[POWER]   reset reg      : %s\n",
              info.reset_reg_available ? "yes" : "no");
  com1_printf("[POWER]   PM1a control   : 0x%x\n", info.pm1a_control);
  com1_printf("[POWER]   PM1b control   : 0x%x\n", info.pm1b_control);
  com1_printf("[POWER]   SLP_TYP S5     : a=%u b=%u\n", info.slp_typa_s5,
              info.slp_typb_s5);
  com1_printf("[POWER]   SMI cmd port   : 0x%x\n", info.smi_command_port);
  com1_printf("[POWER]   shutdown hooks : %d\n", g_shutdown_hook_count);
}
