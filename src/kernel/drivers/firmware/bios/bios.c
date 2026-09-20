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

$define %type fw_provider_t as one firmware family backend
$define %type fw_desc_t as immutable description of the platform firmware
$define %type fw_handoff_t as raw boot-time facts handed over by the loader

$define %func bios_present as function with args const fw_handoff_t *
$define %func bios_detect as function with args const fw_handoff_t *, fw_desc_t *
$define %func bios_identify as procedure with args driver_t *, device_t
$define %func bios_probe as function with args device_t
$define %func bios_attach as function with args device_t

*/

/* !SPACE!

$space %internal bios_present, bios_detect
$space %internal bios_identify, bios_probe, bios_attach

*/

#include <kernel/drivers/firmware/bios/legacy.h>
#include <kernel/drivers/firmware/firmware.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>

#define	BIOS_VENDOR		"BIOS"


static int
bios_present(const fw_handoff_t *h)
{
	return (h != NULL && h->efi_system_table == 0);
}


static int
bios_detect(const fw_handoff_t *h, fw_desc_t *out)
{
	const void	*anchor;
	const void	*rsdp;

	if (!bios_present(h) || out == NULL) {
		return (-1);
	}
	memset(out, 0, sizeof(*out));
	out->origin = FW_ORIGIN_BIOS;
	strncpy(out->vendor, BIOS_VENDOR, sizeof(out->vendor) - 1);

	if (h->acpi_rsdp != 0 &&
	    fw_rsdp_checksum_ok((const void *)h->acpi_rsdp)) {
		out->acpi_rsdp = h->acpi_rsdp;
	} else {
		rsdp = bios_find_rsdp();
		if (rsdp != NULL) {
			out->acpi_rsdp = (u64)(unsigned long)rsdp;
		}
	}

	if (h->smbios_entry != NULL) {
		(void)fw_smbios_parse(h->smbios_entry, h->smbios_size,
		    &out->smbios);
	} else {
		anchor = bios_find_smbios();
		if (anchor != NULL) {

			(void)fw_smbios_parse(anchor, 64, &out->smbios);
		}
	}
	return (0);
}

static const fw_provider_t bios_provider = {
	.name = "bios",
	.origin = FW_ORIGIN_BIOS,
	.present = bios_present,
	.detect = bios_detect,
};

static void
bios_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (!bios_present(fw_handoff())) {
		return;
	}
	if (device_find_child(parent, "bios", 0) == NULL) {
		device_add_child(parent, "bios", 0);
	}
}

static int
bios_probe(device_t dev)
{
	(void)dev;
	if (!bios_present(fw_handoff())) {
		return (-1);
	}
	return (100);
}

static int
bios_attach(device_t dev)
{
	fw_desc_t	desc;

	if (bios_detect(fw_handoff(), &desc) != 0) {
		return (-1);
	}
	return (fw_bind(dev, &bios_provider, &desc));
}

static driver_t bios_driver = {
	.name = "bios",
	.identify = bios_identify,
	.probe = bios_probe,
	.attach = bios_attach,
};

static devclass_t bios_devclass = {
	.name = "bios",
	.maxunit = 1,
};


FIRMWARE_DRIVER_MODULE(bios, bios_driver, bios_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_FIRST);
