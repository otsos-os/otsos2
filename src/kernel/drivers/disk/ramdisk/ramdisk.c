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

$define %func ramdisk_read_sector as procedure with args disk_t *, u32, u8 *
$define %func ramdisk_write_sector as procedure with args disk_t *, u32, u8 *
$define %func ramdisk_init as procedure with args void *, u32

*/

/* !SPACE!

$space %internal ramdisk_read_sector, ramdisk_write_sector
$space %export ramdisk_init

*/

#include "ramdisk.h"
#include "../disk.h"
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#define BLOCK_SHIFT		12
#define BLOCK_SIZE		(1 << BLOCK_SHIFT)
#define SECTORS_PER_BLOCK	(BLOCK_SIZE / 512)

typedef struct {
	u8	*pool;
	u32	pool_blocks;
	u32	touched_blocks;
} ramdisk_priv_t;

static disk_t		ram_disk;

static void
ramdisk_read_sector(disk_t *self, u32 lba, u8 *buffer)
{
	ramdisk_priv_t	*priv;
	u32		bi, off;

	priv = (ramdisk_priv_t *)self->private_data;
	if (!priv || !priv->pool) {
		return;
	}
	bi = lba / SECTORS_PER_BLOCK;

	if (bi >= priv->pool_blocks) {
		memset(buffer, 0, self->sector_size);
		return;
	}

	if (bi >= priv->touched_blocks) {
		memset(buffer, 0, self->sector_size);
		return;
	}

	off = (lba % SECTORS_PER_BLOCK) * self->sector_size;
	memcpy(buffer, priv->pool + (bi << BLOCK_SHIFT) + off,
	    self->sector_size);
}

static void
ramdisk_write_sector(disk_t *self, u32 lba, u8 *buffer)
{
	ramdisk_priv_t	*priv;
	u32		bi, off;

	priv = (ramdisk_priv_t *)self->private_data;
	if (!priv || !priv->pool) {
		return;
	}
	bi = lba / SECTORS_PER_BLOCK;

	if (bi >= priv->pool_blocks) {
		drivers_log("[RAMDISK] Write at lba=%u exceeds "
		    "pool (%u blocks)\n", lba, priv->pool_blocks);
		return;
	}

	if (bi >= priv->touched_blocks) {
		priv->touched_blocks = bi + 1;
		self->total_sectors =
		    priv->touched_blocks * SECTORS_PER_BLOCK;
	}

	off = (lba % SECTORS_PER_BLOCK) * self->sector_size;
	memcpy(priv->pool + (bi << BLOCK_SHIFT) + off,
	    buffer, self->sector_size);
}

void
ramdisk_init(void *pool, u32 pool_size)
{
	ramdisk_priv_t	*priv;

	priv = kmem_alloc(sizeof(ramdisk_priv_t));
	if (!priv) {
		drivers_log("[RAMDISK] Failed to allocate "
		    "private data\n");
		return;
	}

	priv->pool = (u8 *)pool;
	priv->pool_blocks = pool_size / BLOCK_SIZE;
	priv->touched_blocks = 0;

	strcpy(ram_disk.name, "ramdisk0");
	ram_disk.type = DISK_TYPE_RAM;
	ram_disk.sector_size = 512;
	ram_disk.total_sectors = priv->pool_blocks *
	    SECTORS_PER_BLOCK;
	ram_disk.private_data = priv;
	ram_disk.read_sector = ramdisk_read_sector;
	ram_disk.write_sector = ramdisk_write_sector;

	disk_register(&ram_disk);

	drivers_log("[RAMDISK] Initialized: pool %u bytes "
	    "(%u blocks, %u sectors max)\n",
	    pool_size, priv->pool_blocks,
	    priv->pool_blocks * SECTORS_PER_BLOCK);
}
