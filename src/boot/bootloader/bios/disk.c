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

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed

$define %func bios_read_sectors as function with args u32, u32, u32
$define %func bios_disk_read as function with args u32, u32, void *
$define %func bios_disk_select as procedure with args u32
$define %func bios_disk_current as function with args void
$define %func bios_disk_present as function with args u32

*/

/* !SPACE!

$space %export bios_disk_read, bios_disk_select, bios_disk_current
$space %export bios_disk_present

*/

#include <boot/bootloader/bios/bios.h>
#include <boot/bootloader/lib/string.h>

#define BIOS_BOUNCE_ADDR	0x00070000U
#define BIOS_READ_CHUNK		32U

extern int	bios_read_sectors(u32 lba, u32 sectors, u32 dst);
extern u8	bios_disk_drive;

void
bios_disk_select(u32 drive)
{
	bios_disk_drive = (u8)drive;
}

u32
bios_disk_current(void)
{
	return ((u32)bios_disk_drive);
}

int
bios_disk_present(u32 drive)
{
	u8	saved;
	int	rc;

	saved = bios_disk_drive;
	bios_disk_drive = (u8)drive;
	rc = bios_read_sectors(0, 1, BIOS_BOUNCE_ADDR);
	bios_disk_drive = saved;
	return (rc == 0 ? 0 : -1);
}

int
bios_disk_read(u32 lba, u32 sectors, void *dst)
{
	u8	*out;
	u32	chunk, max_chunk;
	u32	bytes;
	int	rc;

	out = (u8 *)dst;
	max_chunk = BIOS_READ_CHUNK;
	while (sectors > 0) {
		chunk = sectors;
		if (chunk > max_chunk) {
			chunk = max_chunk;
		}
		rc = bios_read_sectors(lba, chunk, BIOS_BOUNCE_ADDR);
		if (rc != 0 && chunk > 1) {
			max_chunk = 1;
			chunk = 1;
			rc = bios_read_sectors(lba, chunk, BIOS_BOUNCE_ADDR);
		}
		if (rc != 0) {
			return (rc);
		}
		bytes = chunk * BIOS_SECTOR_SIZE;
		bl_memcpy(out, (const void *)BIOS_BOUNCE_ADDR, bytes);
		out += bytes;
		lba += chunk;
		sectors -= chunk;
	}
	return (0);
}
