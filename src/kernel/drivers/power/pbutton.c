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

$define %type u16 as 16 bit unsigned
$define %type int as 32 bit signed

$define %func pbutton_handle_event as function with args u16
$define %func power_button_init as function with args void
$define %func power_button_poll as procedure with args void
$define %func power_button_is_initialized as function with args void

*/

/* !SPACE!

$space %internal pbutton_handle_event
$space %export power_button_init, power_button_poll
$space %export power_button_is_initialized

*/

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/power/pbutton.h>
#include <kernel/drivers/power/power.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	PM1_STS_PWRBTN	(1 << 8)

static int	g_pbutton_initialized;
static int	g_pbutton_shutdown_in_progress;
static u16	g_pm1a_event;
static u16	g_pm1b_event;

static int
pbutton_handle_event(u16 pm1_event_port)
{
	u16	status;

	if (pm1_event_port == 0) {
		return (0);
	}

	status = inw(pm1_event_port);
	if ((status & PM1_STS_PWRBTN) == 0) {
		return (0);
	}

	outw(pm1_event_port, PM1_STS_PWRBTN);

	if (!g_pbutton_shutdown_in_progress) {
		g_pbutton_shutdown_in_progress = 1;
		drivers_log("[PWRBTN] Power button pressed, "
		    "shutting down...\n");
		power_controller_shutdown();
	}

	return (1);
}

int
power_button_init(void)
{
	acpi_fadt_t	*fadt;

	g_pbutton_initialized = 0;
	g_pbutton_shutdown_in_progress = 0;
	g_pm1a_event = 0;
	g_pm1b_event = 0;

	if (!acpi_is_initialized()) {
		return (-1);
	}

	fadt = acpi_get_fadt();
	if (!fadt) {
		return (-1);
	}

	g_pm1a_event = (u16)fadt->pm1a_event_block;
	g_pm1b_event = (u16)fadt->pm1b_event_block;

	if (g_pm1a_event == 0 && g_pm1b_event == 0) {
		return (-1);
	}

	g_pbutton_initialized = 1;
	drivers_log("[PWRBTN] initialized: PM1a_EVT=0x%x "
	    "PM1b_EVT=0x%x\n", g_pm1a_event, g_pm1b_event);
	return (0);
}

void
power_button_poll(void)
{
	if (!g_pbutton_initialized || g_pbutton_shutdown_in_progress) {
		return;
	}

	if (pbutton_handle_event(g_pm1a_event)) {
		return;
	}
	(void)pbutton_handle_event(g_pm1b_event);
}

int
power_button_is_initialized(void)
{
	return (g_pbutton_initialized);
}
