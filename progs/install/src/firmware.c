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

$define %type fwioc_info_t as firmware summary returned to userspace

$define %func inst_fw_path as function with args char *, unsigned long
$define %func inst_fw_origin as function with args void

*/

/* !SPACE!

$space %internal inst_fw_path
$space %export inst_fw_origin

*/

#include <native.h>
#include <stdio.h>
#include <string.h>

#include <kernel/api/firmware_abi.h>

#include "inst.h"

#define	INST_FW_ROOT		"/Entity/Interface/Driver"
#define	INST_FW_MAX_UNITS	32
#define	INST_FW_MAX_IFACES	16

static int
inst_fw_path(char *out, unsigned long size)
{
	struct api_entity_entry	units[INST_FW_MAX_UNITS];
	struct api_entity_entry	ifaces[INST_FW_MAX_IFACES];
	char			dir[192];
	int			nunits, nifaces;
	int			i, j;

	nunits = entityList(INST_FW_ROOT, units, INST_FW_MAX_UNITS);
	if (nunits <= 0) {
		return (-1);
	}
	for (i = 0; i < nunits && i < INST_FW_MAX_UNITS; i++) {
		snprintf(dir, sizeof(dir), "%s/%s", INST_FW_ROOT,
		    units[i].name);
		nifaces = entityList(dir, ifaces, INST_FW_MAX_IFACES);
		if (nifaces <= 0) {
			continue;
		}
		for (j = 0; j < nifaces && j < INST_FW_MAX_IFACES; j++) {
			if (strcmp(ifaces[j].name, FWIOC_IFACE_NAME) != 0) {
				continue;
			}
			snprintf(out, size, "%s/%s", dir, ifaces[j].name);
			return (0);
		}
	}
	return (-1);
}


unsigned int
inst_fw_origin(void)
{
	fwioc_info_t	info;
	char		path[256];
	int		handle;
	int		ret;

	if (inst_fw_path(path, sizeof(path)) != 0) {
		return (FWIOC_ORIGIN_UNKNOWN);
	}

	handle = entityOpen(path, ENTITY_ACCESS_READ | ENTITY_ACCESS_WRITE);
	if (handle < 0) {
		return (FWIOC_ORIGIN_UNKNOWN);
	}
	memset(&info, 0, sizeof(info));
	ret = entityIoctl(handle, FWIOC_GETINFO, &info);
	entityClose(handle);
	if (ret != 0) {
		return (FWIOC_ORIGIN_UNKNOWN);
	}
	return (info.origin);
}
