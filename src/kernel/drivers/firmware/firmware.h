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

$define %type fw_origin_t as which firmware family booted this kernel
$define %type fw_smbios_t as located SMBIOS entry point and table
$define %type fw_efi_t as located EFI system table
$define %type fw_desc_t as immutable description of the platform firmware
$define %type fw_handoff_t as raw boot-time facts handed over by the loader
$define %type fw_provider_t as one firmware family backend

$define %func fw_handoff as function with args void
$define %func fw_smbios_parse as function with args const void *, u32, fw_smbios_t *
$define %func fw_bind as function with args device_t, provider, desc
$define %func fw_desc as function with args void
$define %func fw_origin_name as function with args fw_origin_t
$define %func fw_entity_attach as function with args device_t
$define %func fw_rsdp_checksum_ok as function with args const void *

*/

/* !SPACE!

$space %export fw_handoff, fw_smbios_parse, fw_rsdp_checksum_ok
$space %export fw_bind, fw_desc, fw_origin_name, fw_entity_attach

*/

#ifndef _KERNEL_DRIVERS_FIRMWARE_H_
#define _KERNEL_DRIVERS_FIRMWARE_H_

#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/mlibc.h>

#define	FW_VENDOR_MAX		64

#define	FW_CAP_ACPI		0x0001u
#define	FW_CAP_SMBIOS		0x0002u
#define	FW_CAP_EFI_SYSTAB	0x0004u

typedef enum fw_origin {
	FW_ORIGIN_UNKNOWN	= 0,
	FW_ORIGIN_BIOS		= 1,
	FW_ORIGIN_UEFI		= 2,
} fw_origin_t;

typedef struct fw_smbios {
	u64	entry;
	u64	table;
	u32	table_length;
	u32	structures;
	u8	major;
	u8	minor;
} fw_smbios_t;

typedef struct fw_efi {
	u64	system_table;
	u32	revision;
} fw_efi_t;

typedef struct fw_desc {
	fw_origin_t	origin;
	u32		caps;
	u64		acpi_rsdp;
	fw_smbios_t	smbios;
	fw_efi_t	efi;
	char		vendor[FW_VENDOR_MAX];
} fw_desc_t;


typedef struct fw_handoff {
	u64		efi_system_table;
	u64		acpi_rsdp;
	const void	*smbios_entry;
	u32		smbios_size;
	u8		smbios_major;
	u8		smbios_minor;
} fw_handoff_t;

typedef struct fw_provider {
	const char	*name;
	fw_origin_t	origin;
	int		(*present)(const fw_handoff_t *h);
	int		(*detect)(const fw_handoff_t *h, fw_desc_t *out);
} fw_provider_t;

const fw_handoff_t	*fw_handoff(void);
int			fw_smbios_parse(const void *anchor, u32 size,
			    fw_smbios_t *out);
int			fw_rsdp_checksum_ok(const void *rsdp);
int			fw_bind(device_t dev, const fw_provider_t *prov,
			    const fw_desc_t *desc);
const fw_desc_t		*fw_desc(void);
const char		*fw_origin_name(fw_origin_t origin);
int			fw_entity_attach(device_t dev);

#endif
