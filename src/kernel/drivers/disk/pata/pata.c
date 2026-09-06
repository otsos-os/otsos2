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
$define %type u64 as 64 bit unsigned
$define %type disk_t as struct with one registered block device and its geometry
$define %type bio_t as struct with one block request, its status and callback
$define %type pata_dummy_area as struct with buffer and guard arrays

$define %func pata_wait_not_bsy as function with args u32
$define %func pata_wait_drq as function with args u32
$define %func pata_guard_init as procedure with args void
$define %func pata_guard_check as procedure with args const char *
$define %func debug_chainfs_overlap as procedure with args const void *, u32, const char *
$define %func debug_chainfs_magic_change as procedure with args u32, const char *
$define %func pata_identify as procedure with args void
$define %func pata_error_status as function with args void
$define %func pata_read_sector as function with args u32, u8 *
$define %func pata_write_sector as function with args u32, u8 *
$define %func pata_cache_flush as function with args void
$define %func pata_submit as function with args disk_t *, bio_t *

$const PATA_LBA28_MAX as highest sector addressable through the LBA28 registers
$const PATA_MAX_IO_SECTORS as ceiling on sectors in one PATA request
$const PATA_PIO_TIMEOUT as poll budget for one BSY or DRQ transition

*/

/* !SPACE!

$space %internal pata_wait_not_bsy, pata_wait_drq
$space %internal pata_guard_init, pata_guard_check
$space %internal debug_chainfs_overlap, debug_chainfs_magic_change
$space %internal pata_error_status
$space %internal pata_read_sector, pata_write_sector
$space %internal pata_cache_flush, pata_submit
$space %internal pata_identify
*/
#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#define	PATA_LBA28_MAX		0x0FFFFFFFULL
#define	PATA_MAX_IO_SECTORS	256
#define	PATA_PIO_TIMEOUT	1000000
#define	PATA_FALLBACK_SECTORS	20480

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

static int
pata_error_status(void)
{
	u8	err;

	err = inb(IDE_ERROR);
	if ((err & IDE_ERROR_UNC) != 0) {
		return (BIO_STATUS_MEDIUM);
	}
	return (BIO_STATUS_IOERR);
}

static int
pata_read_sector(u32 lba, u8 *buffer)
{
	u32	magic_before;
	u8	status;

	memset(buffer, 0, PATA_SECTOR_SIZE);

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] read(%u): BSY timeout before cmd\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}

	outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
	outb(IDE_FEATURES, 0x00);
	outb(IDE_SEC_COUNT, 1);
	outb(IDE_LBA_LOW, (u8)lba);
	outb(IDE_LBA_MID, (u8)(lba >> 8));
	outb(IDE_LBA_HIGH, (u8)(lba >> 16));
	outb(IDE_COMMAND, IDE_CMD_READ);

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] read(%u): BSY timeout after cmd\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}
	if (pata_wait_drq(PATA_PIO_TIMEOUT) != 0) {
		status = inb(IDE_STATUS);
		if ((status & IDE_STATUS_ERR) != 0) {
			drivers_log("[PATA] read(%u): err status=0x%x "
			    "error=0x%x\n", lba, status, inb(IDE_ERROR));
			return (pata_error_status());
		}
		drivers_log("[PATA] read(%u): DRQ timeout\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}

	debug_chainfs_overlap(buffer, PATA_SECTOR_WORDS, "pata_read_sector");
	magic_before = g_chainfs.superblock.magic;
	insw(IDE_DATA, buffer, PATA_SECTOR_WORDS);
	debug_chainfs_magic_change(magic_before, "pata_read_sector");

	return (BIO_STATUS_OK);
}

static int
pata_write_sector(u32 lba, u8 *buffer)
{
	u8	status;

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] write(%u): BSY timeout before cmd\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}

	outb(IDE_DRIVE_SEL, 0xE0 | ((lba >> 24) & 0x0F));
	outb(IDE_FEATURES, 0x00);
	outb(IDE_SEC_COUNT, 1);
	outb(IDE_LBA_LOW, (u8)lba);
	outb(IDE_LBA_MID, (u8)(lba >> 8));
	outb(IDE_LBA_HIGH, (u8)(lba >> 16));
	outb(IDE_COMMAND, IDE_CMD_WRITE);

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] write(%u): BSY timeout after cmd\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}
	if (pata_wait_drq(PATA_PIO_TIMEOUT) != 0) {
		status = inb(IDE_STATUS);
		if ((status & IDE_STATUS_ERR) != 0) {
			drivers_log("[PATA] write(%u): err status=0x%x "
			    "error=0x%x\n", lba, status, inb(IDE_ERROR));
			return (pata_error_status());
		}
		drivers_log("[PATA] write(%u): DRQ timeout\n", lba);
		return (BIO_STATUS_TIMEOUT);
	}

	outsw(IDE_DATA, buffer, PATA_SECTOR_WORDS);


	return (BIO_STATUS_OK);
}


static int
pata_cache_flush(void)
{
	u8	status;

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] flush: BSY timeout before cmd\n");
		return (BIO_STATUS_TIMEOUT);
	}

	outb(IDE_DRIVE_SEL, 0xE0);
	outb(IDE_COMMAND, IDE_CMD_FLUSH_CACHE);

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("[PATA] flush: BSY timeout after cmd\n");
		return (BIO_STATUS_TIMEOUT);
	}

	status = inb(IDE_STATUS);
	if ((status & IDE_STATUS_ERR) != 0) {
		drivers_log("[PATA] flush: err status=0x%x error=0x%x\n",
		    status, inb(IDE_ERROR));
		return (pata_error_status());
	}
	return (BIO_STATUS_OK);
}


static int
pata_submit(disk_t *self, bio_t *bio)
{
	u8	*buf;
	u32	done;
	int	status, flush;

	(void)self;

	if (bio->cmd == BIO_FLUSH) {
		bio_done(bio, pata_cache_flush(), 0);
		return (0);
	}


	if (bio->lba + (u64)bio->nsectors > PATA_LBA28_MAX + 1ULL) {
		drivers_log("[PATA] request beyond LBA28: lba=%u n=%u\n",
		    (u32)bio->lba, bio->nsectors);
		bio_done(bio, BIO_STATUS_INVAL, bio->nsectors);
		return (0);
	}

	buf = (u8 *)bio->buf;
	status = BIO_STATUS_OK;

	for (done = 0; done < bio->nsectors; done++) {
		if (bio->cmd == BIO_READ) {
			status = pata_read_sector((u32)(bio->lba + done),
			    buf + (done * PATA_SECTOR_SIZE));
		} else {
			status = pata_write_sector((u32)(bio->lba + done),
			    buf + (done * PATA_SECTOR_SIZE));
		}

		if (status != BIO_STATUS_OK) {
			break;
		}
	}

	if (bio->cmd == BIO_WRITE && done > 0) {
		flush = pata_cache_flush();
		if (status == BIO_STATUS_OK) {
			status = flush;
		}
	}

	bio_done(bio, status, bio->nsectors - done);
	return (0);
}

static const disk_ops_t pata_ops = {
	.submit		= pata_submit,
	.timeout	= NULL,
};

static void
pata_identify(void)
{
	u64	capacity;
	u32	magic_before;
	u8	status;

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

	if (pata_wait_not_bsy(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("PATA: Identify timeout/error "
		    "(BSY)\n");
		return;
	}

	if (inb(IDE_LBA_MID) != 0 || inb(IDE_LBA_HIGH) != 0) {
		drivers_log("PATA: Device is not ATA/PATA "
		    "(ATAPI or unsupported)\n");
		return;
	}

	if (pata_wait_drq(PATA_PIO_TIMEOUT) != 0) {
		drivers_log("PATA: Identify timeout/error "
		    "(DRQ)\n");
		return;
	}

	magic_before = g_chainfs.superblock.magic;
	debug_chainfs_overlap(pata_dummy_area.buffer, PATA_SECTOR_WORDS,
	    "pata_identify");
	pata_guard_init();
	insw(IDE_DATA, pata_dummy_area.buffer, PATA_SECTOR_WORDS);
	pata_guard_check("identify");
	debug_chainfs_magic_change(magic_before, "pata_identify");

	drivers_log("PATA: Drive identified successfully.\n");
	if (pata_registered) {
		return;
	}

	capacity = ((u64)pata_dummy_area.buffer[61] << 16) |
	    (u64)pata_dummy_area.buffer[60];
	if (capacity == 0) {
		drivers_log("[PATA] IDENTIFY reported zero capacity, "
		    "assuming %u sectors\n", PATA_FALLBACK_SECTORS);
		capacity = PATA_FALLBACK_SECTORS;
	}
	if (capacity > PATA_LBA28_MAX + 1ULL) {
		drivers_log("[PATA] capacity clamped to LBA28 limit, "
		    "%u sectors unreachable\n",
		    (u32)(capacity - (PATA_LBA28_MAX + 1ULL)));
		capacity = PATA_LBA28_MAX + 1ULL;
	}

	memset(&pata_disk, 0, sizeof(pata_disk));
	strcpy(pata_disk.name, "pata0");
	pata_disk.type = DISK_TYPE_PATA;
	pata_disk.sector_size = PATA_SECTOR_SIZE;
	pata_disk.total_sectors = capacity;
	pata_disk.max_io_sectors = PATA_MAX_IO_SECTORS;
	pata_disk.ops = &pata_ops;
	pata_disk.private_data = NULL;

	if (disk_register(&pata_disk) >= 0) {
		pata_registered = 1;
	}
}

static void
pata_identify_newbus(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "pata", 0) == NULL) {
		device_add_child(parent, "pata", 0);
	}
}

static int
pata_probe_newbus(device_t dev)
{
	(void)dev;
	if (inb(IDE_STATUS) == 0xFF) {
		return (-1);
	}
	return (60);
}

static int
pata_attach_newbus(device_t dev)
{
	(void)dev;
	pata_identify();
	return (0);
}

static devclass_t pata_devclass = {
	.name		= "disk",
	.maxunit	= 8,
};

static driver_t pata_driver = {
	.name		= "pata",
	.identify	= pata_identify_newbus,
	.probe		= pata_probe_newbus,
	.attach		= pata_attach_newbus,
};

ISA_DRIVER_MODULE(pata, pata_driver, pata_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_MIDDLE);
