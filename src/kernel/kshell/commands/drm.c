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

$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type drm_driver_t as struct with DRM driver vtable

$define %func parse_nonneg_int as function with args const char *, int *
$define %func kshell_drm_switch_command as function with args int, char *[]

*/

/* !SPACE!

$space %internal parse_nonneg_int
$space %export kshell_drm_switch_command

*/

#include <kernel/drivers/video/drm/drm.h>
#include <kernel/drivers/tty.h>
#include <kernel/kshell/kshell.h>

static int
parse_nonneg_int(const char *s, int *ok)
{
	int	value, i;

	value = 0;
	i = 0;

	*ok = 0;
	if (!s || s[0] == '\0') {
		return (0);
	}

	while (s[i] != '\0') {
		if (s[i] < '0' || s[i] > '9') {
			return (0);
		}
		value = (value * 10) + (s[i] - '0');
		i++;
	}

	*ok = 1;
	return (value);
}

int
kshell_drm_switch_command(int argc, char *argv[])
{
	const drm_driver_t	*drv;
	int			ok, id, rc;

	if (argc != 2) {
		kshell_console_write("drm_switch: usage: "
		    "drm_switch <id>\n");
		return (-1);
	}

	ok = 0;
	id = parse_nonneg_int(argv[1], &ok);
	if (!ok) {
		kshell_console_write("drm_switch: invalid id\n");
		return (-1);
	}

	drv = drm_driver_get_by_index((u32)id);
	if (!drv) {
		kshell_console_write("drm_switch: no driver with "
		    "id ");
		kshell_console_write_int(id);
		kshell_console_write("\n");
		return (-1);
	}

	if (drv == drm_driver_get_selected()) {
		kshell_console_write("drm_switch: already active: ");
		kshell_console_write(drv->name ? drv->name :
		    "unnamed");
		kshell_console_write("\n");
		return (0);
	}

	kshell_console_write("drm_switch: switching to ");
	kshell_console_write(drv->name ? drv->name : "unnamed");
	kshell_console_write("...\n");

	rc = drm_reinit(drv, NULL);
	if (rc != 0) {
		kshell_console_write("drm_switch: failed (error ");
		kshell_console_write_int(rc);
		kshell_console_write(")\n");
		return (-1);
	}

	tty_reinit();

	kshell_console_write("drm_switch: active driver: ");
	kshell_console_write(drm_driver_get_selected_name());
	kshell_console_write(" (id=");
	kshell_console_write_int(
	    drm_driver_get_selected_index());
	kshell_console_write(")\n");

	return (0);
}
