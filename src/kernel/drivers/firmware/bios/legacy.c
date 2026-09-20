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

$define %func scan_rsdp as function with args u64, u64
$define %func scan_smbios as function with args u64, u64
$define %func bios_find_rsdp as function with args void
$define %func bios_find_smbios as function with args void

*/

/* !SPACE!

$space %internal scan_rsdp, scan_smbios
$space %export bios_find_rsdp, bios_find_smbios

*/

#include <kernel/drivers/firmware/bios/legacy.h>
#include <kernel/drivers/firmware/firmware.h>
#include <mlibc/mlibc.h>


#define	BIOS_EBDA_SEG_PTR	0x040E
#define	BIOS_EBDA_SCAN_BYTES	0x400
#define	BIOS_ROM_SCAN_START	0xE0000
#define	BIOS_ROM_SCAN_END	0x100000
#define	BIOS_SCAN_STRIDE	16
#define	BIOS_EBDA_MIN		0x1000
#define	BIOS_EBDA_MAX		0xA0000
#define	BIOS_SMBIOS_MIN		24


static const void *
scan_rsdp(u64 start, u64 end)
{
	u64	addr;

	for (addr = start; addr + 20 <= end; addr += BIOS_SCAN_STRIDE) {
		if (fw_rsdp_checksum_ok((const void *)addr)) {
			return ((const void *)addr);
		}
	}
	return (NULL);
}

static const void *
scan_smbios(u64 start, u64 end)
{
	fw_smbios_t	probe;
	u64		addr;

	for (addr = start; addr + BIOS_SMBIOS_MIN <= end;
	    addr += BIOS_SCAN_STRIDE) {
		if (fw_smbios_parse((const void *)addr,
		    (u32)(end - addr), &probe) == 0) {
			return ((const void *)addr);
		}
	}
	return (NULL);
}


const void *
bios_find_rsdp(void)
{
	const void	*found;
	u64		ebda;

	ebda = (u64)*(const u16 *)BIOS_EBDA_SEG_PTR << 4;
	if (ebda >= BIOS_EBDA_MIN && ebda < BIOS_EBDA_MAX) {
		found = scan_rsdp(ebda, ebda + BIOS_EBDA_SCAN_BYTES);
		if (found != NULL) {
			return (found);
		}
	}
	return (scan_rsdp(BIOS_ROM_SCAN_START, BIOS_ROM_SCAN_END));
}


const void *
bios_find_smbios(void)
{
	return (scan_smbios(BIOS_ROM_SCAN_START, BIOS_ROM_SCAN_END));
}
