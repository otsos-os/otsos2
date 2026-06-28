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

$define %type int as 32 bit signed
$define %type power_info_t as struct with PM register info and flags
$define %type shutdown_hook_fn as function pointer for pre-shutdown callbacks

$define %func power_register_shutdown_hook as function with args shutdown_hook_fn
$define %func run_shutdown_hooks as procedure with args void
$define %func power_controller_shutdown as procedure with args void
$define %func power_controller_reboot as procedure with args void
$define %func power_controller_status as procedure with args void

*/

/* !SPACE!

$space %export power_register_shutdown_hook
$space %export power_controller_shutdown, power_controller_reboot
$space %export power_controller_status
$space %internal run_shutdown_hooks

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/power/power.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define	MAX_SHUTDOWN_HOOKS	16

typedef void (*shutdown_hook_fn)(void);

static shutdown_hook_fn	g_shutdown_hooks[MAX_SHUTDOWN_HOOKS];
static int		g_shutdown_hook_count;

int
power_register_shutdown_hook(shutdown_hook_fn fn)
{
	if (!fn) {
		return (-1);
	}
	if (g_shutdown_hook_count >= MAX_SHUTDOWN_HOOKS) {
		com1_printf("[POWER] shutdown hook table full\n");
		return (-1);
	}
	g_shutdown_hooks[g_shutdown_hook_count++] = fn;
	return (0);
}

static void
run_shutdown_hooks(void)
{
	int	i;

	com1_printf("[POWER] running %d shutdown hooks...\n",
	    g_shutdown_hook_count);
	for (i = 0; i < g_shutdown_hook_count; i++) {
		if (g_shutdown_hooks[i]) {
			g_shutdown_hooks[i]();
		}
	}
	com1_printf("[POWER] all hooks completed\n");
}

void
power_controller_shutdown(void)
{
	com1_printf("[POWER] === SYSTEM SHUTDOWN INITIATED ===\n");
	run_shutdown_hooks();
	power_shutdown();
	__builtin_unreachable();
}

void
power_controller_reboot(void)
{
	com1_printf("[POWER] === SYSTEM REBOOT INITIATED ===\n");
	run_shutdown_hooks();
	power_reboot();
	__builtin_unreachable();
}

void
power_controller_status(void)
{
	power_info_t	info;

	info = power_get_info();

	com1_printf("[POWER] === Status ===\n");
	com1_printf("[POWER]   initialized    : %s\n",
	    info.initialized ? "yes" : "no");
	com1_printf("[POWER]   ACPI available : %s\n",
	    info.acpi_available ? "yes" : "no");
	com1_printf("[POWER]   reset reg      : %s\n",
	    info.reset_reg_available ? "yes" : "no");
	com1_printf("[POWER]   PM1a control   : 0x%x\n",
	    info.pm1a_control);
	com1_printf("[POWER]   PM1b control   : 0x%x\n",
	    info.pm1b_control);
	com1_printf("[POWER]   SLP_TYP S5     : a=%u b=%u\n",
	    info.slp_typa_s5, info.slp_typb_s5);
	com1_printf("[POWER]   SMI cmd port   : 0x%x\n",
	    info.smi_command_port);
	com1_printf("[POWER]   shutdown hooks : %d\n",
	    g_shutdown_hook_count);
}
