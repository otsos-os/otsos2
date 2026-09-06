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

$define %type u32 as 32 bit unsigned
$define %type u8 as 8 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type disk_t as struct with one registered block device and its geometry
$define %type bio_t as struct with one block request, its status and callback

$define %func ramdisk_block_ptr as function with args ramdisk_priv_t *, u32, int
$define %func ramdisk_transfer as function with args disk_t *, bio_t *
$define %func ramdisk_submit as function with args disk_t *, bio_t *
$define %func ramdisk_init as procedure with args void *, u32

$const RAMDISK_MAX_IO_SECTORS as ceiling on sectors in one ramdisk request

*/

/* !SPACE!

$space %internal ramdisk_block_ptr, ramdisk_transfer, ramdisk_submit
$space %export ramdisk_init

*/

#include "ramdisk.h"
#include "../disk.h"
#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#define BLOCK_SHIFT		12
#define BLOCK_SIZE		(1 << BLOCK_SHIFT)
#define SECTORS_PER_BLOCK	(BLOCK_SIZE / 512)
#define RAMDISK_GROW_BYTES	(96U * 1024U * 1024U)

#define RAMDISK_MAX_IO_SECTORS	2048

typedef struct {
	u8	*pool;
	u8	**extra_blocks;
	u32	pool_blocks;
	u32	extra_block_count;
	u32	max_blocks;
	u32	touched_blocks;
} ramdisk_priv_t;

static disk_t		ram_disk;

static u8 *
ramdisk_block_ptr(ramdisk_priv_t *priv, u32 bi, int create)
{
	u8	*block;
	u32	extra_index;

	if (!priv) {
		return (NULL);
	}
	if (bi < priv->pool_blocks) {
		return (priv->pool + (bi << BLOCK_SHIFT));
	}
	if (bi >= priv->max_blocks || !priv->extra_blocks) {
		return (NULL);
	}
	extra_index = bi - priv->pool_blocks;
	if (extra_index >= priv->extra_block_count) {
		return (NULL);
	}
	block = priv->extra_blocks[extra_index];
	if (!block && create) {
		block = kmem_alloc(BLOCK_SIZE);
		if (!block) {
			return (NULL);
		}
		memset(block, 0, BLOCK_SIZE);
		priv->extra_blocks[extra_index] = block;
	}
	return (block);
}

static u32
ramdisk_transfer(disk_t *self, bio_t *bio)
{
	ramdisk_priv_t	*priv;
	u8		*block, *buf;
	u64		lba;
	u32		i, bi, off, writing;

	priv = (ramdisk_priv_t *)self->private_data;
	if (priv == NULL || priv->pool == NULL) {
		return (bio->nsectors);
	}
	buf = (u8 *)bio->buf;
	writing = (bio->cmd == BIO_WRITE) ? 1 : 0;

	for (i = 0; i < bio->nsectors; i++) {
		lba = bio->lba + (u64)i;
		bi = (u32)(lba / SECTORS_PER_BLOCK);

		if (bi >= priv->max_blocks) {
			if (writing != 0) {
				drivers_log("[RAMDISK] write at lba=%u past "
				    "capacity (%u blocks)\n", (u32)lba,
				    priv->max_blocks);
			}
			return (bio->nsectors - i);
		}

	
		block = ramdisk_block_ptr(priv, bi, (int)writing);
		off = (u32)(lba % SECTORS_PER_BLOCK) * self->sector_size;

		if (block == NULL) {
			if (writing != 0) {
				drivers_log("[RAMDISK] failed to grow at "
				    "lba=%u (block %u)\n", (u32)lba, bi);
				return (bio->nsectors - i);
			}
			memset(buf, 0, self->sector_size);
		} else if (writing != 0) {
			memcpy(block + off, buf, self->sector_size);
			if (bi >= priv->touched_blocks) {
				priv->touched_blocks = bi + 1;
			}
		} else {
			memcpy(buf, block + off, self->sector_size);
		}
		buf += self->sector_size;
	}
	return (0);
}

static int
ramdisk_submit(disk_t *self, bio_t *bio)
{
	u32	resid;

	if (self == NULL || bio == NULL) {
		return (-1);
	}

	switch (bio->cmd) {
	case BIO_READ:
	case BIO_WRITE:
		resid = ramdisk_transfer(self, bio);
		
		bio_done(bio, resid == 0 ? BIO_STATUS_OK : BIO_STATUS_IOERR,
		    resid);
		return (0);
	case BIO_FLUSH:

		bio_done(bio, BIO_STATUS_OK, 0);
		return (0);
	default:
		return (-1);
	}
}

static const disk_ops_t ramdisk_ops = {
	.submit		= ramdisk_submit,
	
	.timeout	= NULL,
};

void
ramdisk_init(void *pool, u32 pool_size)
{
	ramdisk_priv_t	*priv;
	u32		extra_bytes;

	priv = kmem_alloc(sizeof(ramdisk_priv_t));
	if (!priv) {
		drivers_log("[RAMDISK] Failed to allocate "
		    "private data\n");
		return;
	}

	priv->pool = (u8 *)pool;
	priv->pool_blocks = pool_size / BLOCK_SIZE;
	priv->extra_blocks = NULL;
	priv->extra_block_count = RAMDISK_GROW_BYTES / BLOCK_SIZE;
	priv->max_blocks = priv->pool_blocks;
	priv->touched_blocks = 0;
	if (priv->pool) {
		memset(priv->pool, 0, pool_size);
	}

	if (priv->extra_block_count > 0) {
		extra_bytes = priv->extra_block_count * sizeof(u8 *);
		priv->extra_blocks = kmem_alloc(extra_bytes);
		if (priv->extra_blocks) {
			memset(priv->extra_blocks, 0, extra_bytes);
			priv->max_blocks += priv->extra_block_count;
		} else {
			drivers_log("[RAMDISK] grow table allocation "
			    "failed\n");
			priv->extra_block_count = 0;
		}
	}

	memset(&ram_disk, 0, sizeof(ram_disk));
	strcpy(ram_disk.name, "ramdisk0");
	ram_disk.type = DISK_TYPE_RAM;
	ram_disk.sector_size = 512;
	ram_disk.total_sectors = (u64)priv->max_blocks *
	    SECTORS_PER_BLOCK;
	ram_disk.max_io_sectors = RAMDISK_MAX_IO_SECTORS;
	ram_disk.flags = DISK_F_NO_FLUSH;
	ram_disk.private_data = priv;
	ram_disk.ops = &ramdisk_ops;

	disk_register(&ram_disk);

	drivers_log("[RAMDISK] Initialized: pool %u bytes "
	    "(%u blocks), grow %u bytes (%u blocks), "
	    "%u sectors max\n",
	    pool_size, priv->pool_blocks, RAMDISK_GROW_BYTES,
	    priv->extra_block_count,
	    priv->max_blocks * SECTORS_PER_BLOCK);
}

static void
ramdisk_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "ramdisk", 0) == NULL) {
		device_add_child(parent, "ramdisk", 0);
	}
}

static int
ramdisk_probe(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot == NULL || boot->module_pool == NULL ||
	    boot->module_pool_size == 0) {
		return (-1);
	}
	return (80);
}

static int
ramdisk_attach(device_t dev)
{
	const newbus_bootinfo_t	*boot;

	(void)dev;
	boot = newbus_get_bootinfo();
	if (boot == NULL) {
		return (-1);
	}
	ramdisk_init(boot->module_pool, boot->module_pool_size);
	return (0);
}

static devclass_t ramdisk_devclass = {
	.name		= "disk",
	.maxunit	= 8,
};

static driver_t ramdisk_driver = {
	.name		= "ramdisk",
	.identify	= ramdisk_identify,
	.probe		= ramdisk_probe,
	.attach		= ramdisk_attach,
};

FIRMWARE_DRIVER_MODULE(ramdisk, ramdisk_driver, ramdisk_devclass,
    NEWBUS_PASS_STORAGE, NEWBUS_ORDER_FIRST);
