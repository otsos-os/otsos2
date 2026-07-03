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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type disk_t as struct with name, type, sector_size, sectors, ops
$define %type pata_dummy_area as struct with buffer and guard arrays

$define %func pata_wait_not_bsy as function with args u32
$define %func pata_wait_drq as function with args u32
$define %func pata_guard_init as procedure with args void
$define %func pata_guard_check as procedure with args const char *
$define %func debug_chainfs_overlap as procedure with args const void *, u32, const char *
$define %func debug_chainfs_magic_change as procedure with args u32, const char *
$define %func pata_identify as procedure with args u16 *
$define %func pata_disk_read as procedure with args disk_t *, u32, u8 *
$define %func pata_disk_write as procedure with args disk_t *, u32, u8 *
$define %func pata_read_sector as procedure with args u32, u8 *
$define %func pata_write_sector as procedure with args u32, u8 *

*/

/* !SPACE!

$space %internal pata_wait_not_bsy, pata_wait_drq
$space %internal pata_guard_init, pata_guard_check
$space %internal debug_chainfs_overlap, debug_chainfs_magic_change
$space %internal pata_disk_read, pata_disk_write
$space %export pata_identify, pata_read_sector, pata_write_sector

*/

#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int	pata_registered;

static int
pata_wait_not_bsy(u32 timeout)
{
	u8	status;

	while (timeout--) {
		status = inb(IDE_STATUS);
		if (status == 0 || status == 0xFF) {
			return (-1);
		}
		if ((status & IDE_STATUS_BSY) == 0) {
			return (0);
		}
	}
	return (-1);
}

static int
pata_wait_drq(u32 timeout)
{
	u8	status;

	while (timeout--) {
		status = inb(IDE_STATUS);
		if (status == 0 || status == 0xFF) {
			return (-1);
		}
		if (status & IDE_STATUS_ERR) {
			return (-1);
		}
		if ((status & IDE_STATUS_BSY) == 0 &&
		    (status & IDE_STATUS_DRQ)) {
			return (0);
		}
	}
	return (-1);
}

struct pata_dummy_area {
	u16	buffer[256];
	u16	guard[16];
};

static struct pata_dummy_area	pata_dummy_area;
static disk_t			pata_disk;

static void
pata_guard_init(void)
{
	u32	i;

	for (i = 0; i < 16; ++i) {
		pata_dummy_area.guard[i] =
		    (u16)(0xBEEF ^ (i * 0x1111));
	}
}

static void
pata_guard_check(const char *where)
{
	u32	i;
	u16	expected;

	for (i = 0; i < 16; ++i) {
		expected = (u16)(0xBEEF ^ (i * 0x1111));
		if (pata_dummy_area.guard[i] != expected) {
			drivers_log("[PATA] dummy guard corrupted "
			    "at %s idx=%u val=0x%x exp=0x%x\n",
			    where, i, pata_dummy_area.guard[i],
			    expected);
			return;
		}
	}
}

static void
debug_chainfs_overlap(const void *buffer, u32 words, const char *op)
{
	const u8	*start, *end, *magic;

	start = (const u8 *)buffer;
	end = start + (words * 2);
	magic = (const u8 *)&g_chainfs.superblock.magic;

	if (start <= magic && magic < end) {
		drivers_log("[CHAINFS] magic overlap in %s: "
		    "buf=%p words=%u\n", op, buffer, words);
	}
}

static void
debug_chainfs_magic_change(u32 before, const char *op)
{
	u32	after;

	after = g_chainfs.superblock.magic;
	if (after != before) {
		drivers_log("[CHAINFS] magic changed during %s: "
		    "old=0x%x new=0x%x ra=%p\n", before, after,
		    op, __builtin_return_address(0));
	}
}

void
pata_identify(u16 *target_buf)
{
	u8	status;
	u32	magic_before;
	int	i;
	const char	*name;

	drivers_log("PATA: Identifying drive...\n");
	outb(IDE_DRIVE_SEL, 0xA0);
	outb(IDE_SEC_COUNT, 0);
	outb(IDE_LBA_LOW, 0);
	outb(IDE_LBA_MID, 0);
	outb(IDE_LBA_HIGH, 0);
	outb(IDE_COMMAND, IDE_CMD_IDENTIFY);

	status = inb(IDE_STATUS);
	if (status == 0 || status == 0xFF) {
		drivers_log("PATA: No drive found.\n");
		return;
	}

	if (pata_wait_not_bsy(1000000) != 0) {
		drivers_log("PATA: Identify timeout/error "
		    "(BSY)\n");
		return;
	}

	if (inb(IDE_LBA_MID) != 0 || inb(IDE_LBA_HIGH) != 0) {
		drivers_log("PATA: Device is not ATA/PATA "
		    "(ATAPI or unsupported)\n");
		return;
	}

	if (pata_wait_drq(1000000) != 0) {
		drivers_log("PATA: Identify timeout/error "
		    "(DRQ)\n");
		return;
	}

	magic_before = g_chainfs.superblock.magic;
	if (target_buf) {
		debug_chainfs_overlap(target_buf, 256,
		    "pata_identify");
		insw(IDE_DATA, target_buf, 256);
	} else {
		debug_chainfs_overlap(pata_dummy_area.buffer,
		    256, "pata_identify");
		pata_guard_init();
		insw(IDE_DATA, pata_dummy_area.buffer, 256);
		pata_guard_check("identify");
	}
	debug_chainfs_magic_change(magic_before, "pata_identify");

	drivers_log("PATA: Drive identified successfully.\n");
	if (pata_registered) {
		return;
	}

	pata_disk.type = DISK_TYPE_PATA;
	pata_disk.sector_size = 512;

	{
		u32	capa;
		u16	*buf;

		buf = (u16 *)pata_dummy_area.buffer;
		capa = ((u32)buf[61] << 16) | (u32)buf[60];
		if (capa == 0) {
			capa = 20480;
		}
		pata_disk.total_sectors = capa;
		drivers_log("[PATA] capacity: %u sectors (%u MB)\n",
		    capa, capa / 2048);
	}

	{
		void pata_disk_read(struct disk *self, u32 lba,
		    u8 *buffer);
		void pata_disk_write(struct disk *self, u32 lba,
		    u8 *buffer);

		pata_disk.read_sector = pata_disk_read;
		pata_disk.write_sector = pata_disk_write;
	}

	i = 0;
	name = "pata0";
	while (name[i]) {
		pata_disk.name[i] = name[i];
		i++;
	}
	pata_disk.name[i] = 0;

	if (disk_register(&pata_disk) >= 0) {
		pata_registered = 1;
	}
}

void
pata_disk_read(struct disk *self, u32 lba, u8 *buffer)
{
	pata_read_sector(lba, buffer);
}

void
pata_disk_write(struct disk *self, u32 lba, u8 *buffer)
{
	pata_write_sector(lba, buffer);
}

void
pata_read_sector(u32 lba, u8 *buffer)
{
	u32	magic_before;

	if (pata_wait_not_bsy(1000000) != 0) {
		drivers_log("[PATA] read_sector(%u): BSY timeout\n", lba);
		if (buffer) {
			memset(buffer, 0, 512);
		}
		return;
	}

	outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
	outb(IDE_FEATURES, 0x00);
	outb(IDE_SEC_COUNT, 1);
	outb(IDE_LBA_LOW, (u8)lba);
	outb(IDE_LBA_MID, (u8)(lba >> 8));
	outb(IDE_LBA_HIGH, (u8)(lba >> 16));
	outb(IDE_COMMAND, IDE_CMD_READ);

	if (pata_wait_not_bsy(1000000) != 0) {
		drivers_log("[PATA] read_sector(%u): BSY timeout "
		    "after cmd\n", lba);
		if (buffer) {
			memset(buffer, 0, 512);
		}
		return;
	}
	if (pata_wait_drq(1000000) != 0) {
		drivers_log("[PATA] read_sector(%u): DRQ timeout\n",
		    lba);
		if (buffer) {
			memset(buffer, 0, 512);
		}
		return;
	}

	debug_chainfs_overlap(buffer, 256, "pata_read_sector");
	magic_before = g_chainfs.superblock.magic;
	insw(IDE_DATA, buffer, 256);
	debug_chainfs_magic_change(magic_before,
	    "pata_read_sector");
}

void
pata_write_sector(u32 lba, u8 *buffer)
{
	if (pata_wait_not_bsy(1000000) != 0) {
		return;
	}

	outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
	outb(IDE_FEATURES, 0x00);
	outb(IDE_SEC_COUNT, 1);
	outb(IDE_LBA_LOW, (u8)lba);
	outb(IDE_LBA_MID, (u8)(lba >> 8));
	outb(IDE_LBA_HIGH, (u8)(lba >> 16));
	outb(IDE_COMMAND, IDE_CMD_WRITE);

	if (pata_wait_not_bsy(1000000) != 0 ||
	    pata_wait_drq(1000000) != 0) {
		return;
	}

	outsw(IDE_DATA, buffer, 256);

	outb(IDE_COMMAND, 0xE7);
	(void)pata_wait_not_bsy(1000000);
}
