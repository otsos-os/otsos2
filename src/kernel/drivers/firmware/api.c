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
$define %type fwioc_info_t as firmware summary returned to userspace
$define %type fwioc_smbios_t as SMBIOS location returned to userspace
$define %type newbus_interface_t as named driver I/O interface table

$define %func fw_abi_origin as function with args fw_origin_t
$define %func fw_abi_caps as function with args u32
$define %func fw_copyout as function with args void *, const void *, u32
$define %func fw_format_info as function with args const fw_desc_t *, char *, size_t
$define %func fw_read as function with args device_t, void *, u64, u64
$define %func fw_ioctl_info as function with args const fw_desc_t *, void *
$define %func fw_ioctl_smbios as function with args const fw_desc_t *, void *
$define %func fw_ioctl as function with args device_t, u64, void *
$define %func fw_entity_attach as function with args device_t

*/

/* !SPACE!

$space %internal fw_abi_origin, fw_abi_caps, fw_copyout
$space %internal fw_format_info, fw_read
$space %internal fw_ioctl_info, fw_ioctl_smbios, fw_ioctl
$space %export fw_entity_attach

*/

#include <kernel/api/firmware_abi.h>
#include <kernel/drivers/firmware/firmware.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

static fwioc_u32
fw_abi_origin(fw_origin_t origin)
{
	switch (origin) {
	case FW_ORIGIN_BIOS:
		return (FWIOC_ORIGIN_BIOS);
	case FW_ORIGIN_UEFI:
		return (FWIOC_ORIGIN_UEFI);
	default:
		return (FWIOC_ORIGIN_UNKNOWN);
	}
}

static fwioc_u32
fw_abi_caps(u32 caps)
{
	fwioc_u32	out;

	out = 0;
	if (caps & FW_CAP_ACPI) {
		out |= FWIOC_CAP_ACPI;
	}
	if (caps & FW_CAP_SMBIOS) {
		out |= FWIOC_CAP_SMBIOS;
	}
	if (caps & FW_CAP_EFI_SYSTAB) {
		out |= FWIOC_CAP_EFI_SYSTAB;
	}
	return (out);
}

static int
fw_copyout(void *dst, const void *src, u32 len)
{
	if (dst == NULL) {
		return (-1);
	}
	if (process_current() != NULL) {
		if (!is_user_address(dst, (size_t)len) ||
		    !user_range_fault_in(dst, (size_t)len, 1)) {
			return (-1);
		}
	}
	memcpy(dst, src, (unsigned long)len);
	return (0);
}

static int
fw_ioctl_info(const fw_desc_t *fw, void *arg)
{
	fwioc_info_t	info;

	memset(&info, 0, sizeof(info));
	info.origin = fw_abi_origin(fw->origin);
	info.caps = fw_abi_caps(fw->caps);
	info.acpi_rsdp = (fwioc_u64)fw->acpi_rsdp;
	info.efi_system_table = (fwioc_u64)fw->efi.system_table;
	info.efi_revision = (fwioc_u32)fw->efi.revision;
	info.smbios_major = (fwioc_u32)fw->smbios.major;
	info.smbios_minor = (fwioc_u32)fw->smbios.minor;
	strncpy(info.vendor, fw->vendor, sizeof(info.vendor) - 1);
	return (fw_copyout(arg, &info, (u32)sizeof(info)));
}

static int
fw_ioctl_smbios(const fw_desc_t *fw, void *arg)
{
	fwioc_smbios_t	sm;

	if ((fw->caps & FW_CAP_SMBIOS) == 0) {
		return (-1);
	}
	memset(&sm, 0, sizeof(sm));
	sm.entry = (fwioc_u64)fw->smbios.entry;
	sm.table = (fwioc_u64)fw->smbios.table;
	sm.table_length = (fwioc_u32)fw->smbios.table_length;
	sm.structures = (fwioc_u32)fw->smbios.structures;
	sm.major = (fwioc_u32)fw->smbios.major;
	sm.minor = (fwioc_u32)fw->smbios.minor;
	return (fw_copyout(arg, &sm, (u32)sizeof(sm)));
}


static size_t
fw_format_info(const fw_desc_t *fw, char *buf, size_t max_size)
{
	int	len, total;

	total = 0;
	len = snprintf(buf + total, max_size - total,
	    "origin: %s\nvendor: %s\n",
	    fw_origin_name(fw->origin),
	    fw->vendor[0] != '\0' ? fw->vendor : "unknown");
	if (len > 0) {
		total += len;
	}
	if (fw->origin == FW_ORIGIN_UEFI) {
		len = snprintf(buf + total, max_size - total,
		    "revision: %u.%u\nsystab: 0x%llx\n",
		    (unsigned int)(fw->efi.revision >> 16),
		    (unsigned int)(fw->efi.revision & 0xffff),
		    (unsigned long long)fw->efi.system_table);
		if (len > 0) {
			total += len;
		}
	}
	if (fw->caps & FW_CAP_SMBIOS) {
		len = snprintf(buf + total, max_size - total,
		    "smbios: %u.%u (entry: 0x%llx, table: 0x%llx)\n",
		    (unsigned int)fw->smbios.major,
		    (unsigned int)fw->smbios.minor,
		    (unsigned long long)fw->smbios.entry,
		    (unsigned long long)fw->smbios.table);
		if (len > 0) {
			total += len;
		}
	}
	if (fw->caps & FW_CAP_ACPI) {
		len = snprintf(buf + total, max_size - total,
		    "acpi_rsdp: 0x%llx\n",
		    (unsigned long long)fw->acpi_rsdp);
		if (len > 0) {
			total += len;
		}
	}
	return ((size_t)total);
}

static int
fw_read(device_t dev, void *buf, u64 count, u64 offset)
{
	const fw_desc_t	*fw;
	char		text[512];
	size_t		len;

	(void)dev;
	fw = fw_desc();
	if (fw == NULL || buf == NULL || count == 0) {
		return (-1);
	}
	len = fw_format_info(fw, text, sizeof(text));
	if (offset >= len) {
		return (0);
	}
	if (count > len - offset) {
		count = len - offset;
	}
	memcpy(buf, text + offset, (unsigned long)count);
	return ((int)count);
}

static int
fw_ioctl(device_t dev, u64 cmd, void *arg)
{
	const fw_desc_t	*fw;

	(void)dev;
	fw = fw_desc();
	if (fw == NULL || arg == NULL) {
		return (-1);
	}
	switch (cmd) {
	case FWIOC_GETINFO:
		return (fw_ioctl_info(fw, arg));
	case FWIOC_SMBIOS_GET:
		return (fw_ioctl_smbios(fw, arg));
	default:
		return (-1);
	}
}


static const newbus_interface_t fw_interface = {
	.name = FWIOC_IFACE_NAME,
	.read = fw_read,
	.write = NULL,
	.ioctl = fw_ioctl,
	.stat = NULL,
};

int
fw_entity_attach(device_t dev)
{
	if (dev == NULL) {
		return (-1);
	}
	return (newbus_interface_register(dev, &fw_interface));
}
