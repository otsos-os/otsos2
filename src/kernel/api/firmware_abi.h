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

$define %type fwioc_u32 as ABI stable 32 bit unsigned
$define %type fwioc_u64 as ABI stable 64 bit unsigned
$define %type fwioc_info_t as firmware summary returned to userspace
$define %type fwioc_smbios_t as SMBIOS location returned to userspace

*/

/* !SPACE!

$space %export fwioc_info_t, fwioc_smbios_t

*/

#ifndef _KERNEL_API_FIRMWARE_ABI_H_
#define _KERNEL_API_FIRMWARE_ABI_H_

typedef unsigned int		fwioc_u32;
typedef unsigned long long	fwioc_u64;

#define	FWIOC_IFACE_NAME	"firmware"
#define	FWIOC_GETINFO		0x4601u
#define	FWIOC_SMBIOS_GET	0x4602u
#define	FWIOC_ORIGIN_UNKNOWN	0u
#define	FWIOC_ORIGIN_BIOS	1u
#define	FWIOC_ORIGIN_UEFI	2u
#define	FWIOC_CAP_ACPI		0x0001u
#define	FWIOC_CAP_SMBIOS	0x0002u
#define	FWIOC_CAP_EFI_SYSTAB	0x0004u
#define	FWIOC_VENDOR_MAX	64

typedef struct fwioc_info {
	fwioc_u32	origin;
	fwioc_u32	caps;
	fwioc_u64	acpi_rsdp;
	fwioc_u64	efi_system_table;
	fwioc_u32	efi_revision;
	fwioc_u32	smbios_major;
	fwioc_u32	smbios_minor;
	fwioc_u32	reserved;
	char		vendor[FWIOC_VENDOR_MAX];
} fwioc_info_t;


typedef struct fwioc_smbios {
	fwioc_u64	entry;
	fwioc_u64	table;
	fwioc_u32	table_length;
	fwioc_u32	structures;
	fwioc_u32	major;
	fwioc_u32	minor;
} fwioc_smbios_t;

#endif
