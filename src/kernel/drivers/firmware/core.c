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

$define %type fw_desc_t as immutable description of the platform firmware
$define %type fw_provider_t as one firmware family backend
$define %type device_t as pointer to newbus device

$define %func fw_caps_of as function with args const fw_desc_t *
$define %func fw_bind as function with args device_t, provider, desc
$define %func fw_desc as function with args void
$define %func fw_origin_name as function with args fw_origin_t

*/

/* !SPACE!

$space %internal fw_caps_of
$space %export fw_bind, fw_desc, fw_origin_name

*/

#include <kernel/drivers/firmware/firmware.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>


static fw_desc_t		fw_bound_desc;
static const fw_provider_t	*fw_bound_provider;
static int			fw_bound;

static u32
fw_caps_of(const fw_desc_t *d)
{
	u32	caps;

	caps = 0;
	if (d->acpi_rsdp != 0) {
		caps |= FW_CAP_ACPI;
	}
	if (d->smbios.entry != 0 && d->smbios.table != 0) {
		caps |= FW_CAP_SMBIOS;
	}
	if (d->efi.system_table != 0) {
		caps |= FW_CAP_EFI_SYSTAB;
	}
	return (caps);
}


int
fw_bind(device_t dev, const fw_provider_t *prov, const fw_desc_t *desc)
{
	if (dev == NULL || prov == NULL || desc == NULL ||
	    desc->origin == FW_ORIGIN_UNKNOWN) {
		return (-1);
	}
	if (fw_bound) {
		drivers_log("[firmware] %s refused: already bound to %s\n",
		    prov->name, fw_bound_provider->name);
		return (-1);
	}

	fw_bound_desc = *desc;
	fw_bound_desc.caps = fw_caps_of(&fw_bound_desc);
	fw_bound_provider = prov;

	fw_bound = 1;

	drivers_log("[firmware] %s acpi=%s smbios=%s efi=%s\n",
	    fw_origin_name(fw_bound_desc.origin),
	    (fw_bound_desc.caps & FW_CAP_ACPI) ? "yes" : "no",
	    (fw_bound_desc.caps & FW_CAP_SMBIOS) ? "yes" : "no",
	    (fw_bound_desc.caps & FW_CAP_EFI_SYSTAB) ? "yes" : "no");

	return (fw_entity_attach(dev));
}


const fw_desc_t *
fw_desc(void)
{
	if (!fw_bound) {
		return (NULL);
	}
	return (&fw_bound_desc);
}

const char *
fw_origin_name(fw_origin_t origin)
{
	switch (origin) {
	case FW_ORIGIN_BIOS:
		return ("bios");
	case FW_ORIGIN_UEFI:
		return ("uefi");
	default:
		return ("unknown");
	}
}
