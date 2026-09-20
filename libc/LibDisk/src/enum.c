/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
$define %type char as 8 bit signed
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_entry_t as one enumerated block device
$define %type api_entity_entry as kernel entity namespace listing entry

$define %func ldisk_iface_path as function with args char *, size_t, const char *
$define %func ldisk_probe_entry as function with args const char *, ldisk_entry_t *
$define %func ldisk_enumerate as function with args ldisk_entry_t *, int, int *
$define %func ldisk_find_slice as function with args const char *, uint64_t, ldisk_entry_t *

*/

/* !SPACE!

$space %internal ldisk_iface_path, ldisk_probe_entry
$space %export ldisk_enumerate, ldisk_find_slice

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk_int.h"

#define LDISK_NS_MAX		64

static int
ldisk_iface_path(char *dst, size_t size, const char *devunit)
{
	int	len;

	len = snprintf(dst, size, "%s/%s/%s", LDISK_NS_ROOT, devunit,
	    LDISK_IFACE_NAME);
	if (len < 0 || (size_t)len + 1 > size) {
		errno = E2BIG;
		return (-1);
	}
	return (0);
}

static int
ldisk_probe_entry(const char *path, ldisk_entry_t *out)
{
	dioc_info_t	raw;
	int		handle;
	int		ret;

	handle = entityOpen(path, ENTITY_ACCESS_READ | ENTITY_ACCESS_WRITE);
	if (handle < 0) {
		return (-1);
	}
	memset(&raw, 0, sizeof(raw));
	ret = entityIoctl(handle, DIOC_GETINFO, &raw);
	(void)entityClose(handle);
	if (ret != 0) {
		return (-1);
	}

	memset(out, 0, sizeof(*out));
	ldisk_copy_info(&out->info, &raw);
	if (ldisk_str_copy(out->path, sizeof(out->path), path) != 0) {
		return (-1);
	}
	return (0);
}


int
ldisk_enumerate(ldisk_entry_t *out, int max, int *count)
{
	struct api_entity_entry	devs[LDISK_NS_MAX];
	struct api_entity_entry	ifaces[LDISK_NS_MAX];
	char			path[LDISK_PATH_MAX];
	char			base[LDISK_PATH_MAX];
	int			found;
	int			ndevs;
	int			nifaces;
	int			i, j;
	int			len;

	if (out == NULL || count == NULL || max <= 0) {
		errno = EINVAL;
		return (-1);
	}
	*count = 0;
	found = 0;

	ndevs = entityList(LDISK_NS_ROOT, devs, LDISK_NS_MAX);
	if (ndevs < 0) {
		return (-1);
	}

	for (i = 0; i < ndevs && found < max; i++) {
		if (devs[i].name[0] == '\0') {
			continue;
		}
		len = snprintf(base, sizeof(base), "%s/%s", LDISK_NS_ROOT,
		    devs[i].name);
		if (len < 0 || (size_t)len + 1 > sizeof(base)) {
			continue;
		}
		nifaces = entityList(base, ifaces, LDISK_NS_MAX);
		if (nifaces <= 0) {
			continue;
		}
		for (j = 0; j < nifaces; j++) {
			if (strcmp(ifaces[j].name, LDISK_IFACE_NAME) != 0) {
				continue;
			}
			if (ldisk_iface_path(path, sizeof(path),
			    devs[i].name) != 0) {
				break;
			}
			if (ldisk_probe_entry(path, &out[found]) == 0) {
				found++;
			}
			break;
		}
	}

	*count = found;
	return (0);
}


int
ldisk_find_slice(const char *parent_name, uint64_t first_lba,
    ldisk_entry_t *out)
{
	ldisk_entry_t	*entries;
	int		count;
	int		i;
	int		ret;

	if (parent_name == NULL || out == NULL) {
		errno = EINVAL;
		return (-1);
	}

	entries = malloc(sizeof(*entries) * LDISK_DEVS_MAX);
	if (entries == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	ret = ldisk_enumerate(entries, LDISK_DEVS_MAX, &count);
	if (ret != 0) {
		free(entries);
		return (-1);
	}

	for (i = 0; i < count; i++) {
		if ((entries[i].info.flags & LDISK_F_SLICE) == 0) {
			continue;
		}
		if (entries[i].info.base_lba != first_lba) {
			continue;
		}
		if (strcmp(entries[i].info.parent, parent_name) != 0) {
			continue;
		}
		*out = entries[i];
		free(entries);
		return (0);
	}

	free(entries);
	memset(out, 0, sizeof(*out));
	errno = ENOENT;
	return (-1);
}
