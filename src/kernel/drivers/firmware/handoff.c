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

$define %type fw_handoff_t as raw boot-time facts handed over by the loader
$define %type multiboot2_info_t as multiboot2 information structure
$define %type multiboot2_tag_t as one multiboot2 information tag

$define %func fw_handoff_parse as procedure with args multiboot2_info_t *, fw_handoff_t *
$define %func fw_handoff as function with args void

*/

/* !SPACE!

$space %internal fw_handoff_parse
$space %export fw_handoff

*/

#include <kernel/drivers/firmware/firmware.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/multiboot2.h>
#include <mlibc/mlibc.h>

#define	MB2_EFI64_PTR_OFF	8
#define	MB2_SMBIOS_MAJOR_OFF	8
#define	MB2_SMBIOS_MINOR_OFF	9
#define	MB2_SMBIOS_ANCHOR_OFF	16
#define	MB2_ACPI_RSDP_OFF	8
#define	FW_SMBIOS_ANCHOR_MIN	24

static fw_handoff_t	fw_handoff_state;
static int		fw_handoff_ready;

static void
fw_handoff_parse(multiboot2_info_t *mb, fw_handoff_t *out)
{
	multiboot2_tag_t	*tag;


	tag = multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_EFI64);
	if (tag != NULL && tag->size >= MB2_EFI64_PTR_OFF + 8) {
		out->efi_system_table =
		    *(u64 *)((u8 *)tag + MB2_EFI64_PTR_OFF);
	}

	tag = multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_ACPI_NEW);
	if (tag == NULL) {
		tag = multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_ACPI_OLD);
	}
	if (tag != NULL && tag->size > MB2_ACPI_RSDP_OFF) {
		out->acpi_rsdp = (u64)((u8 *)tag + MB2_ACPI_RSDP_OFF);
	}


	tag = multiboot2_find_tag(mb, MULTIBOOT2_TAG_TYPE_SMBIOS);
	if (tag != NULL && tag->size >=
	    MB2_SMBIOS_ANCHOR_OFF + FW_SMBIOS_ANCHOR_MIN) {
		out->smbios_entry = (const void *)((u8 *)tag +
		    MB2_SMBIOS_ANCHOR_OFF);
		out->smbios_size = tag->size - MB2_SMBIOS_ANCHOR_OFF;
		out->smbios_major = *((u8 *)tag + MB2_SMBIOS_MAJOR_OFF);
		out->smbios_minor = *((u8 *)tag + MB2_SMBIOS_MINOR_OFF);
	}
}


const fw_handoff_t *
fw_handoff(void)
{
	const newbus_bootinfo_t	*boot;

	if (fw_handoff_ready) {
		return (&fw_handoff_state);
	}
	boot = newbus_get_bootinfo();
	if (boot == NULL || boot->mb2 == NULL) {
		return (NULL);
	}
	memset(&fw_handoff_state, 0, sizeof(fw_handoff_state));
	fw_handoff_parse((multiboot2_info_t *)boot->mb2, &fw_handoff_state);
	fw_handoff_ready = 1;
	return (&fw_handoff_state);
}
