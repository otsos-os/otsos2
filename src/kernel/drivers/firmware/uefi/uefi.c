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

$define %func uefi_present as function with args const fw_handoff_t *
$define %func uefi_revision as function with args u64
$define %func uefi_systab_vendor as function with args u64, char *, u32
$define %func uefi_detect as function with args const fw_handoff_t *, fw_desc_t *
$define %func uefi_identify as procedure with args driver_t *, device_t
$define %func uefi_probe as function with args device_t
$define %func uefi_attach as function with args device_t

*/

/* !SPACE!

$space %internal uefi_present, uefi_revision, uefi_systab_vendor, uefi_detect
$space %internal uefi_identify, uefi_probe, uefi_attach

*/

#include <kernel/drivers/firmware/firmware.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>

#define	UEFI_SYSTAB_SIGNATURE	0x5453595320494249ULL
#define	UEFI_SYSTAB_REV_OFF	8
#define	UEFI_SYSTAB_VENDOR_OFF	16
#define	UEFI_IDENTITY_LIMIT	0x100000000ULL
#define	UEFI_VENDOR_DEFAULT	"UEFI"

static int
uefi_present(const fw_handoff_t *h)
{
	return (h != NULL && h->efi_system_table != 0);
}

static u32
uefi_revision(u64 systab)
{
	const u64	*hdr;

	if (systab == 0 || systab >= UEFI_IDENTITY_LIMIT) {
		return (0);
	}
	hdr = (const u64 *)systab;
	if (*hdr != UEFI_SYSTAB_SIGNATURE) {
		return (0);
	}
	return (*(const u32 *)((const u8 *)systab + UEFI_SYSTAB_REV_OFF));
}

static int
uefi_systab_vendor(u64 systab, char *out_vendor, u32 max_len)
{
	const u16	*wstr;
	u64		vendor_ptr;
	u32		i;

	if (systab == 0 || systab >= UEFI_IDENTITY_LIMIT ||
	    out_vendor == NULL || max_len == 0) {
		return (-1);
	}
	vendor_ptr = *(const u64 *)((const u8 *)systab +
	    UEFI_SYSTAB_VENDOR_OFF);
	if (vendor_ptr == 0 || vendor_ptr >= UEFI_IDENTITY_LIMIT) {
		return (-1);
	}
	wstr = (const u16 *)(unsigned long)vendor_ptr;
	i = 0;
	while (i + 1 < max_len && wstr[i] != 0) {
		out_vendor[i] = (char)(wstr[i] & 0x7f);
		i++;
	}
	out_vendor[i] = '\0';
	return (i > 0 ? 0 : -1);
}

static int
uefi_detect(const fw_handoff_t *h, fw_desc_t *out)
{
	if (!uefi_present(h) || out == NULL) {
		return (-1);
	}
	memset(out, 0, sizeof(*out));
	out->origin = FW_ORIGIN_UEFI;
	out->efi.system_table = h->efi_system_table;
	out->efi.revision = uefi_revision(h->efi_system_table);

	if (h->acpi_rsdp != 0 &&
	    fw_rsdp_checksum_ok((const void *)h->acpi_rsdp)) {
		out->acpi_rsdp = h->acpi_rsdp;
	}
	if (h->smbios_entry != NULL) {
		(void)fw_smbios_parse(h->smbios_entry, h->smbios_size,
		    &out->smbios);
	}

	if (fw_smbios_bios_vendor(&out->smbios, out->vendor,
	    sizeof(out->vendor)) != 0) {
		if (uefi_systab_vendor(h->efi_system_table, out->vendor,
		    sizeof(out->vendor)) != 0) {
			strncpy(out->vendor, UEFI_VENDOR_DEFAULT,
			    sizeof(out->vendor) - 1);
		}
	}
	return (0);
}

static const fw_provider_t uefi_provider = {
	.name = "uefi",
	.origin = FW_ORIGIN_UEFI,
	.present = uefi_present,
	.detect = uefi_detect,
};


static void
uefi_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (!uefi_present(fw_handoff())) {
		return;
	}
	if (device_find_child(parent, "efi", 0) == NULL) {
		device_add_child(parent, "efi", 0);
	}
}

static int
uefi_probe(device_t dev)
{
	(void)dev;
	if (!uefi_present(fw_handoff())) {
		return (-1);
	}
	return (100);
}


static int
uefi_attach(device_t dev)
{
	fw_desc_t	desc;

	if (uefi_detect(fw_handoff(), &desc) != 0) {
		return (-1);
	}
	return (fw_bind(dev, &uefi_provider, &desc));
}

static driver_t uefi_driver = {
	.name = "efi",
	.identify = uefi_identify,
	.probe = uefi_probe,
	.attach = uefi_attach,
};

static devclass_t uefi_devclass = {
	.name = "efi",
	.maxunit = 1,
};

FIRMWARE_DRIVER_MODULE(efi, uefi_driver, uefi_devclass,
    NEWBUS_PASS_FIRMWARE, NEWBUS_ORDER_FIRST);
