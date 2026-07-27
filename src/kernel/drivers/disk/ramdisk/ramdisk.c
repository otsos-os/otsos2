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
$define %type int as 32 bit signed
$define %type disk_t as struct with name, type, sector_size, sectors, ops

$define %func ramdisk_block_ptr as function with args ramdisk_priv_t *, u32, int
$define %func ramdisk_read_sector as procedure with args disk_t *, u32, u8 *
$define %func ramdisk_write_sector as procedure with args disk_t *, u32, u8 *
$define %func ramdisk_init as procedure with args void *, u32

*/

/* !SPACE!

$space %internal ramdisk_block_ptr, ramdisk_read_sector, ramdisk_write_sector
$space %export ramdisk_init

*/

#include "ramdisk.h"
#include "../disk.h"
#include <kernel/drivers/newbus/newbus.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#define BLOCK_SHIFT		12
#define BLOCK_SIZE		(1 << BLOCK_SHIFT)
#define SECTORS_PER_BLOCK	(BLOCK_SIZE / 512)
#define RAMDISK_GROW_BYTES	(96U * 1024U * 1024U)

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

static void
ramdisk_read_sector(disk_t *self, u32 lba, u8 *buffer)
{
	ramdisk_priv_t	*priv;
	u8		*block;
	u32		bi, off;

	priv = (ramdisk_priv_t *)self->private_data;
	if (!priv || !priv->pool) {
		return;
	}
	bi = lba / SECTORS_PER_BLOCK;

	if (bi >= priv->max_blocks) {
		memset(buffer, 0, self->sector_size);
		return;
	}

	block = ramdisk_block_ptr(priv, bi, 0);
	if (!block) {
		memset(buffer, 0, self->sector_size);
		return;
	}

	off = (lba % SECTORS_PER_BLOCK) * self->sector_size;
	memcpy(buffer, block + off, self->sector_size);
}

static void
ramdisk_write_sector(disk_t *self, u32 lba, u8 *buffer)
{
	ramdisk_priv_t	*priv;
	u8		*block;
	u32		bi, off;

	priv = (ramdisk_priv_t *)self->private_data;
	if (!priv || !priv->pool) {
		return;
	}
	bi = lba / SECTORS_PER_BLOCK;

	if (bi >= priv->max_blocks) {
		drivers_log("[RAMDISK] Write at lba=%u exceeds "
		    "capacity (%u blocks)\n", lba, priv->max_blocks);
		return;
	}

	block = ramdisk_block_ptr(priv, bi, 1);
	if (!block) {
		drivers_log("[RAMDISK] Failed to grow at lba=%u "
		    "(block %u)\n", lba, bi);
		return;
	}

	if (bi >= priv->touched_blocks) {
		priv->touched_blocks = bi + 1;
	}

	off = (lba % SECTORS_PER_BLOCK) * self->sector_size;
	memcpy(block + off, buffer, self->sector_size);
}

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

	strcpy(ram_disk.name, "ramdisk0");
	ram_disk.type = DISK_TYPE_RAM;
	ram_disk.sector_size = 512;
	ram_disk.total_sectors = priv->max_blocks *
	    SECTORS_PER_BLOCK;
	ram_disk.private_data = priv;
	ram_disk.read_sector = ramdisk_read_sector;
	ram_disk.write_sector = ramdisk_write_sector;

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
