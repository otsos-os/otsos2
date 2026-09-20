/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "blockdev.h"

#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/disk/gpt.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/fat32/fat32.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	BLOCKDEV_MAX		DISK_MAX
#define	BLOCKDEV_BOUNCE_SECTORS	64

typedef struct blockdev_slot {
	disk_t		*disk;
	device_t	dev;
	int		used;
	int		pad;
} blockdev_slot_t;

static blockdev_slot_t	blockdev_slots[BLOCKDEV_MAX];
static u8		*blockdev_bounce;
static u32		blockdev_bounce_bytes;

static int
blockdev_privileged(void)
{
	process_t	*proc;

	proc = process_current();
	if (proc == NULL) {
		return (1);
	}
	return (proc_has_privilege(proc));
}

static disk_t *
blockdev_lookup(device_t dev)
{
	int	i;

	for (i = 0; i < BLOCKDEV_MAX; i++) {
		if (blockdev_slots[i].used && blockdev_slots[i].dev == dev) {
			return (blockdev_slots[i].disk);
		}
	}
	return (NULL);
}

static u8 *
blockdev_get_bounce(u32 sector_size)
{
	u32	want;

	want = sector_size * BLOCKDEV_BOUNCE_SECTORS;
	if (blockdev_bounce != NULL && blockdev_bounce_bytes >= want) {
		return (blockdev_bounce);
	}
	if (blockdev_bounce != NULL) {
		kmem_free(blockdev_bounce);
		blockdev_bounce = NULL;
		blockdev_bounce_bytes = 0;
	}
	blockdev_bounce = kmem_alloc(want);
	if (blockdev_bounce == NULL) {
		return (NULL);
	}
	blockdev_bounce_bytes = want;
	return (blockdev_bounce);
}

static int
blockdev_range_ok(const disk_t *disk, u64 offset, u64 count, u64 *lba,
    u32 *nsectors)
{
	u64	end_sector;
	u64	sectors;

	if (disk->sector_size == 0) {
		return (-1);
	}
	if ((offset % disk->sector_size) != 0 ||
	    (count % disk->sector_size) != 0) {
		return (-1);
	}
	sectors = count / disk->sector_size;
	if (sectors == 0 || sectors > BLOCKDEV_BOUNCE_SECTORS) {
		return (-1);
	}
	*lba = offset / disk->sector_size;
	end_sector = *lba + sectors;
	if (end_sector > disk->total_sectors || end_sector < *lba) {
		return (-1);
	}
	*nsectors = (u32)sectors;
	return (0);
}

static int
blockdev_read(device_t dev, void *buf, u64 count, u64 offset)
{
	disk_t	*disk;
	u8	*bounce;
	u64	lba;
	u32	nsectors;
	int	error;

	if (!blockdev_privileged()) {
		return (-1);
	}
	disk = blockdev_lookup(dev);
	if (disk == NULL || buf == NULL) {
		return (-1);
	}
	if (blockdev_range_ok(disk, offset, count, &lba, &nsectors) != 0) {
		return (-1);
	}
	bounce = blockdev_get_bounce(disk->sector_size);
	if (bounce == NULL) {
		return (-1);
	}
	error = bio_read(disk, lba, nsectors, bounce);
	if (error != BIO_STATUS_OK) {
		memset(buf, 0, (unsigned long)count);
		return (-1);
	}
	memcpy(buf, bounce, (unsigned long)count);
	return ((int)count);
}

static int
blockdev_write(device_t dev, const void *buf, u64 count, u64 offset)
{
	disk_t	*disk;
	u8	*bounce;
	u64	lba;
	u32	nsectors;
	int	error;

	if (!blockdev_privileged()) {
		return (-1);
	}
	disk = blockdev_lookup(dev);
	if (disk == NULL || buf == NULL) {
		return (-1);
	}
	if ((disk->flags & DISK_F_READONLY) != 0) {
		return (-1);
	}
	if (blockdev_range_ok(disk, offset, count, &lba, &nsectors) != 0) {
		return (-1);
	}
	bounce = blockdev_get_bounce(disk->sector_size);
	if (bounce == NULL) {
		return (-1);
	}
	memcpy(bounce, buf, (unsigned long)count);
	error = bio_write(disk, lba, nsectors, bounce);
	if (error != BIO_STATUS_OK) {
		return (-1);
	}
	return ((int)count);
}


static void
blockdev_fill_info(const disk_t *disk, dioc_info_t *info)
{
	const disk_t	*root;

	memset(info, 0, sizeof(*info));
	strncpy(info->name, disk->name, DISK_NAME_MAX - 1);
	if (disk->parent != NULL) {
		strncpy(info->parent, disk->parent->name, DISK_NAME_MAX - 1);
	}
	info->total_sectors = disk->total_sectors;
	info->base_lba = disk->base_lba;
	info->sector_size = disk->sector_size;
	info->max_io_sectors = disk->max_io_sectors;
	info->type = (u32)disk->type;
	info->index = (u32)disk->index;

	if ((disk->flags & DISK_F_READONLY) != 0) {
		info->flags |= DIOC_INFO_F_READONLY;
	}
	if ((disk->flags & DISK_F_NO_FLUSH) != 0) {
		info->flags |= DIOC_INFO_F_NOFLUSH;
	}
	if ((disk->flags & DISK_F_SLICE) != 0) {
		info->flags |= DIOC_INFO_F_SLICE;
	}

	root = chainfs_root_disk();
	if (root != NULL && (disk == root ||
	    (root->parent != NULL && disk == root->parent))) {
		info->flags |= DIOC_INFO_F_ROOT;
	}
}

static int
blockdev_probe_fs(const disk_t *disk, dioc_info_t *info)
{
	u8	*probe;

	blockdev_fill_info(disk, info);
	if (disk->sector_size == 0) {
		return (0);
	}
	probe = kmem_alloc(disk->sector_size);
	if (probe == NULL) {
		return (0);
	}
	if (bio_read((disk_t *)disk, 0, 1, probe) == BIO_STATUS_OK) {
		if (chainfs_probe_sector(probe) == 0) {
			info->flags |= DIOC_INFO_F_CHAINFS;
		}
		if (fat32_probe_sector(probe) == 0) {
			info->flags |= DIOC_INFO_F_FAT;
		}
	}
	kmem_free(probe);
	return (0);
}

static int
blockdev_copyin(void *dst, const void *src, u64 len)
{
	if (src == NULL || len == 0) {
		return (-1);
	}
	if (process_current() != NULL) {
		if (!is_user_address(src, (size_t)len) ||
		    !user_range_fault_in(src, (size_t)len, 0)) {
			return (-1);
		}
	}
	memcpy(dst, src, (unsigned long)len);
	return (0);
}

static int
blockdev_copyout(void *dst, const void *src, u64 len)
{
	if (dst == NULL || len == 0) {
		return (-1);
	}
	if (process_current() != NULL) {
		if (!is_user_address(dst, (size_t)len) ||
		    !user_range_fault_in(dst, (size_t)len, 1)) {
			return (-1);
		}
	}
	memcpy(dst, src, (unsigned long)len);
	return (0);
}

static int
blockdev_file_arg(void *arg, dioc_file_t *out)
{
	if (blockdev_copyin(out, arg, sizeof(*out)) != 0) {
		return (-1);
	}
	out->path[DIOC_PATH_MAX - 1] = '\0';
	if (out->path[0] == '\0') {
		return (-1);
	}
	return (0);
}

static u8 *
blockdev_file_payload(const dioc_file_t *req)
{
	u8	*buf;

	if (req->size == 0) {
		return (NULL);
	}
	if (req->size > DIOC_FILE_MAX) {
		return (NULL);
	}
	buf = kmem_alloc((u32)req->size);
	if (buf == NULL) {
		return (NULL);
	}
	if (blockdev_copyin(buf, (const void *)req->data, req->size) != 0) {
		kmem_free(buf);
		return (NULL);
	}
	return (buf);
}

static int
blockdev_cfs_write(disk_t *disk, void *arg)
{
	chainfs_t	*ctx;
	dioc_file_t	req;
	u8		*buf;
	u64		end;
	int		ret;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	if (req.size > DIOC_FILE_MAX || req.total > 0xFFFFFFFFULL) {
		return (-1);
	}
	end = req.offset + req.size;
	if (end < req.offset || end > req.total) {
		return (-1);
	}
	if (req.size == 0) {
		buf = NULL;
	} else {
		buf = blockdev_file_payload(&req);
		if (buf == NULL) {
			return (-1);
		}
	}
	ctx = kmem_alloc(sizeof(*ctx));
	if (ctx == NULL) {
		if (buf != NULL) {
			kmem_free(buf);
		}
		return (-1);
	}
	if (chainfs_ctx_enter(ctx, disk) != 0) {
		kmem_free(ctx);
		if (buf != NULL) {
			kmem_free(buf);
		}
		return (-1);
	}

	if (req.offset == 0) {
		ret = chainfs_write_file(req.path, buf, (u32)req.size,
		    (u32)req.total);
	} else {
		ret = chainfs_write_file_range(req.path, buf, (u32)req.size,
		    (u32)req.offset);
	}
	chainfs_ctx_leave(ctx);
	kmem_free(ctx);
	if (buf != NULL) {
		kmem_free(buf);
	}
	return (ret);
}

static int
blockdev_cfs_trunc(disk_t *disk, void *arg)
{
	chainfs_t	*ctx;
	dioc_file_t	req;
	int		ret;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	if (req.total > 0xFFFFFFFFULL) {
		return (-1);
	}
	ctx = kmem_alloc(sizeof(*ctx));
	if (ctx == NULL) {
		return (-1);
	}
	if (chainfs_ctx_enter(ctx, disk) != 0) {
		kmem_free(ctx);
		return (-1);
	}
	ret = chainfs_truncate(req.path, (u32)req.total);
	chainfs_ctx_leave(ctx);
	kmem_free(ctx);
	return (ret == 0 ? 0 : -1);
}

static int
blockdev_cfs_read(disk_t *disk, void *arg)
{
	chainfs_t	*ctx;
	dioc_file_t	req;
	u8		*buf;
	u32		got;
	int		ret;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	if (req.size == 0 || req.size > DIOC_FILE_MAX) {
		return (-1);
	}
	buf = kmem_alloc((u32)req.size);
	if (buf == NULL) {
		return (-1);
	}
	ctx = kmem_alloc(sizeof(*ctx));
	if (ctx == NULL) {
		kmem_free(buf);
		return (-1);
	}
	if (chainfs_ctx_enter(ctx, disk) != 0) {
		kmem_free(ctx);
		kmem_free(buf);
		return (-1);
	}
	got = 0;
	ret = chainfs_read_file(req.path, buf, (u32)req.size, &got);
	chainfs_ctx_leave(ctx);
	kmem_free(ctx);
	if (ret != 0) {
		kmem_free(buf);
		return (-1);
	}
	if (blockdev_copyout((void *)req.data, buf, got) != 0) {
		kmem_free(buf);
		return (-1);
	}
	kmem_free(buf);
	req.done = got;
	if (blockdev_copyout(arg, &req, sizeof(req)) != 0) {
		return (-1);
	}
	return ((int)got);
}

static int
blockdev_cfs_mkdir(disk_t *disk, void *arg)
{
	chainfs_t	*ctx;
	dioc_file_t	req;
	int		ret;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	ctx = kmem_alloc(sizeof(*ctx));
	if (ctx == NULL) {
		return (-1);
	}
	if (chainfs_ctx_enter(ctx, disk) != 0) {
		kmem_free(ctx);
		return (-1);
	}
	ret = chainfs_mkdir(req.path);
	chainfs_ctx_leave(ctx);
	kmem_free(ctx);
	if (ret == 0 || ret == -API_ERR_EXISTS) {
		return (0);
	}
	return (-1);
}

static int
blockdev_fat_write(disk_t *disk, void *arg)
{
	dioc_file_t	req;
	u8		*buf;
	int		ret;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	buf = blockdev_file_payload(&req);
	if (buf == NULL) {
		return (-1);
	}
	ret = fat32_write_file(disk, req.path, buf, (u32)req.size);
	kmem_free(buf);
	return (ret < 0 ? -1 : 0);
}

static int
blockdev_fat_mkdir(disk_t *disk, void *arg)
{
	dioc_file_t	req;

	if (blockdev_file_arg(arg, &req) != 0) {
		return (-1);
	}
	return (fat32_mkdir(disk, req.path));
}


static int
blockdev_bootinst_check(const disk_t *disk, const dioc_bootinst_t *req,
    const u8 *stage1)
{
	u32	i;

	if (disk->sector_size != DIOC_BOOT_SECTOR) {
		printk("[BLOCKDEV] %s: BIOS boot needs %u byte sectors, "
		    "have %u\n", disk->name, DIOC_BOOT_SECTOR,
		    disk->sector_size);
		return (-1);
	}
	if ((disk->flags & DISK_F_SLICE) != 0) {
		printk("[BLOCKDEV] %s: boot block goes on the whole disk, "
		    "not a slice\n", disk->name);
		return (-1);
	}
	if ((disk->flags & DISK_F_READONLY) != 0) {
		printk("[BLOCKDEV] %s: read-only\n", disk->name);
		return (-1);
	}
	if (req->stage1_size != DIOC_BOOT_SECTOR) {
		printk("[BLOCKDEV] %s: stage1 must be %u bytes, got %lu\n",
		    disk->name, DIOC_BOOT_SECTOR,
		    (unsigned long)req->stage1_size);
		return (-1);
	}
	if (req->stage2_size == 0 ||
	    req->stage2_size > DIOC_BOOT_STAGE2_MAX) {
		printk("[BLOCKDEV] %s: stage2 size %lu out of range\n",
		    disk->name, (unsigned long)req->stage2_size);
		return (-1);
	}

	if (*(const u32 *)(stage1 + DIOC_BOOT_PARAM_OFF) !=
	    DIOC_BOOT_PARAM_MAGIC) {
		printk("[BLOCKDEV] %s: stage1 has no parameter block\n",
		    disk->name);
		return (-1);
	}

	for (i = DIOC_BOOT_CODE_MAX; i < DIOC_BOOT_SIG_OFF; i++) {
		if (stage1[i] != 0) {
			printk("[BLOCKDEV] %s: stage1 occupies byte %u, "
			    "past the %u byte limit\n", disk->name, i,
			    DIOC_BOOT_CODE_MAX);
			return (-1);
		}
	}
	if (stage1[DIOC_BOOT_SIG_OFF] != 0x55 ||
	    stage1[DIOC_BOOT_SIG_OFF + 1] != 0xAA) {
		printk("[BLOCKDEV] %s: stage1 lacks the 55AA signature\n",
		    disk->name);
		return (-1);
	}
	return (0);
}


static int
blockdev_bootinst_stage1(disk_t *disk, const dioc_bootinst_t *req,
    const u8 *stage1)
{
	u8	*mbr;
	u32	sectors;

	mbr = kmem_alloc(disk->sector_size);
	if (mbr == NULL) {
		return (-1);
	}
	if (bio_read(disk, 0, 1, mbr) != BIO_STATUS_OK) {
		printk("[BLOCKDEV] %s: cannot read LBA 0\n", disk->name);
		kmem_free(mbr);
		return (-1);
	}
	if (mbr[DIOC_BOOT_SIG_OFF] != 0x55 ||
	    mbr[DIOC_BOOT_SIG_OFF + 1] != 0xAA) {
		printk("[BLOCKDEV] %s: no protective MBR, commit the GPT "
		    "first\n", disk->name);
		kmem_free(mbr);
		return (-1);
	}

	memcpy(mbr, stage1, DIOC_BOOT_CODE_MAX);
	sectors = (u32)((req->stage2_size + DIOC_BOOT_SECTOR - 1) /
	    DIOC_BOOT_SECTOR);
	*(u32 *)(mbr + DIOC_BOOT_PARAM_OFF + 4) = (u32)req->stage2_lba;
	*(u32 *)(mbr + DIOC_BOOT_PARAM_OFF + 8) =
	    (u32)(req->stage2_lba >> 32);
	*(u16 *)(mbr + DIOC_BOOT_PARAM_OFF + 12) = (u16)sectors;

	if (bio_write(disk, 0, 1, mbr) != BIO_STATUS_OK) {
		printk("[BLOCKDEV] %s: cannot write LBA 0\n", disk->name);
		kmem_free(mbr);
		return (-1);
	}
	kmem_free(mbr);
	return (0);
}


static int
blockdev_bootinst_stage2(disk_t *disk, const dioc_bootinst_t *req,
    const u8 *stage2)
{
	u8	*buf;
	u64	last;
	u32	bytes;
	u32	sectors;

	sectors = (u32)((req->stage2_size + DIOC_BOOT_SECTOR - 1) /
	    DIOC_BOOT_SECTOR);
	bytes = sectors * DIOC_BOOT_SECTOR;

	if (req->stage2_lba == 0) {
		printk("[BLOCKDEV] %s: stage2 cannot live at LBA 0\n",
		    disk->name);
		return (-1);
	}
	last = req->stage2_lba + (u64)sectors;
	if (last < req->stage2_lba || last > disk->total_sectors) {
		printk("[BLOCKDEV] %s: stage2 range %lu+%u past the end\n",
		    disk->name, (unsigned long)req->stage2_lba, sectors);
		return (-1);
	}

	buf = kmem_alloc(bytes);
	if (buf == NULL) {
		return (-1);
	}
	memcpy(buf, stage2, (unsigned long)req->stage2_size);
	memset(buf + req->stage2_size, 0, bytes - (u32)req->stage2_size);

	if (bio_write(disk, req->stage2_lba, sectors, buf) != BIO_STATUS_OK) {
		printk("[BLOCKDEV] %s: cannot write stage2 at %lu\n",
		    disk->name, (unsigned long)req->stage2_lba);
		kmem_free(buf);
		return (-1);
	}
	kmem_free(buf);
	return (0);
}


static int
blockdev_bootinst(disk_t *disk, void *arg)
{
	dioc_bootinst_t	req;
	u8		*stage1;
	u8		*stage2;
	int		ret;

	if (arg == NULL) {
		return (-1);
	}
	if (blockdev_copyin(&req, arg, sizeof(req)) != 0) {
		return (-1);
	}
	if (req.stage1_size != DIOC_BOOT_SECTOR ||
	    req.stage2_size == 0 ||
	    req.stage2_size > DIOC_BOOT_STAGE2_MAX) {
		printk("[BLOCKDEV] %s: bad boot image sizes\n", disk->name);
		return (-1);
	}

	stage1 = kmem_alloc((u32)req.stage1_size);
	if (stage1 == NULL) {
		return (-1);
	}
	stage2 = kmem_alloc((u32)req.stage2_size);
	if (stage2 == NULL) {
		kmem_free(stage1);
		return (-1);
	}
	if (blockdev_copyin(stage1, (const void *)req.stage1,
	    req.stage1_size) != 0 ||
	    blockdev_copyin(stage2, (const void *)req.stage2,
	    req.stage2_size) != 0) {
		kmem_free(stage2);
		kmem_free(stage1);
		return (-1);
	}

	ret = blockdev_bootinst_check(disk, &req, stage1);
	if (ret == 0) {
		ret = blockdev_bootinst_stage2(disk, &req, stage2);
	}
	if (ret == 0) {
		ret = blockdev_bootinst_stage1(disk, &req, stage1);
	}
	kmem_free(stage2);
	kmem_free(stage1);

	if (ret != 0) {
		return (-1);
	}
	if ((disk->flags & DISK_F_NO_FLUSH) == 0) {
		(void)bio_flush(disk);
	}
	printk("[BLOCKDEV] %s: boot block installed, stage2 at %lu\n",
	    disk->name, (unsigned long)req.stage2_lba);
	return (0);
}

static int
blockdev_ioctl(device_t dev, u64 cmd, void *arg)
{
	dioc_mkfs_chainfs_t	mkfs;
	dioc_mkfs_fat32_t	fat;
	dioc_gpt_add_t		add;
	dioc_info_t		info;
	disk_t			*disk;

	if (!blockdev_privileged()) {
		return (-1);
	}
	disk = blockdev_lookup(dev);
	if (disk == NULL) {
		return (-1);
	}

	switch (cmd) {
	case DIOC_GETINFO:
		if (arg == NULL) {
			return (-1);
		}
		blockdev_fill_info(disk, &info);
		return (blockdev_copyout(arg, &info, sizeof(info)));
	case DIOC_PROBEFS:
		if (arg == NULL) {
			return (-1);
		}
		if (blockdev_probe_fs(disk, &info) != 0) {
			return (-1);
		}
		return (blockdev_copyout(arg, &info, sizeof(info)));
	case DIOC_FLUSH:
		if ((disk->flags & DISK_F_NO_FLUSH) != 0) {
			return (0);
		}
		return (bio_flush(disk) == BIO_STATUS_OK ? 0 : -1);
	case DIOC_RESCAN:
		return (gpt_rescan(disk));
	case DIOC_MKFS_CHAINFS:
		if (arg == NULL) {
			return (-1);
		}
		if (blockdev_copyin(&mkfs, arg, sizeof(mkfs)) != 0) {
			return (-1);
		}
		return (chainfs_format_disk(disk, mkfs.total_blocks,
		    mkfs.max_files));
	case DIOC_MKFS_FAT32:
		if (arg == NULL) {
			return (-1);
		}
		if (blockdev_copyin(&fat, arg, sizeof(fat)) != 0) {
			return (-1);
		}
		fat.label[sizeof(fat.label) - 1] = '\0';
		return (fat32_format_disk(disk, fat.label));
	case DIOC_GPT_INIT:
		return (gpt_layout_init(disk));
	case DIOC_GPT_ADD:
		if (arg == NULL) {
			return (-1);
		}
		if (blockdev_copyin(&add, arg, sizeof(add)) != 0) {
			return (-1);
		}
		add.name[sizeof(add.name) - 1] = '\0';
		if (gpt_layout_add(disk, &add) != 0) {
			return (-1);
		}
		return (blockdev_copyout(arg, &add, sizeof(add)));
	case DIOC_GPT_COMMIT:
		return (gpt_layout_commit(disk));
	case DIOC_CFS_WRITE:
		return (blockdev_cfs_write(disk, arg));
	case DIOC_CFS_READ:
		return (blockdev_cfs_read(disk, arg));
	case DIOC_CFS_MKDIR:
		return (blockdev_cfs_mkdir(disk, arg));
	case DIOC_CFS_TRUNC:
		return (blockdev_cfs_trunc(disk, arg));
	case DIOC_FAT_WRITE:
		return (blockdev_fat_write(disk, arg));
	case DIOC_FAT_MKDIR:
		return (blockdev_fat_mkdir(disk, arg));
	case DIOC_BOOTINST:
		return (blockdev_bootinst(disk, arg));
	default:
		return (-1);
	}
}

static int
blockdev_stat(device_t dev, posix_stat_t *st)
{
	disk_t	*disk;

	disk = blockdev_lookup(dev);
	if (disk == NULL || st == NULL) {
		return (-1);
	}
	memset(st, 0, sizeof(*st));
	st->st_mode = POSIX_S_IFBLK | 0600;
	st->st_size = (s64)disk_capacity_bytes(disk);
	st->st_blksize = (s64)disk->sector_size;
	st->st_blocks = (s64)disk->total_sectors;
	st->st_nlink = 1;
	return (0);
}

static const newbus_interface_t blockdev_interface = {
	.name	= DIOC_IFACE_NAME,
	.read	= blockdev_read,
	.write	= blockdev_write,
	.ioctl	= blockdev_ioctl,
	.stat	= blockdev_stat,
};

void
blockdev_publish(disk_t *disk)
{
	int	i, free_slot;

	if (disk == NULL || disk->dev == NULL) {
		return;
	}
	free_slot = -1;
	for (i = 0; i < BLOCKDEV_MAX; i++) {
		if (blockdev_slots[i].used) {
			if (blockdev_slots[i].disk == disk) {
				return;
			}
			continue;
		}
		if (free_slot < 0) {
			free_slot = i;
		}
	}
	if (free_slot < 0) {
		drivers_log("[BLOCK] no slot for %s\n", disk->name);
		return;
	}

	blockdev_slots[free_slot].disk = disk;
	blockdev_slots[free_slot].dev = disk->dev;
	blockdev_slots[free_slot].used = 1;

	if (newbus_interface_register(disk->dev, &blockdev_interface) != 0) {
		blockdev_slots[free_slot].used = 0;
		blockdev_slots[free_slot].disk = NULL;
		blockdev_slots[free_slot].dev = NULL;
		drivers_log("[BLOCK] interface register failed for %s\n",
		    disk->name);
		return;
	}
	drivers_log("[BLOCK] %s exposed at /Entity/Interface/Driver/%s/block\n",
	    disk->name, device_get_nameunit(disk->dev));
}

void
blockdev_revoke(disk_t *disk)
{
	int	i;

	if (disk == NULL) {
		return;
	}
	for (i = 0; i < BLOCKDEV_MAX; i++) {
		if (!blockdev_slots[i].used ||
		    blockdev_slots[i].disk != disk) {
			continue;
		}
		if (blockdev_slots[i].dev != NULL) {
			(void)newbus_interface_unregister(
			    blockdev_slots[i].dev, &blockdev_interface);
		}
		blockdev_slots[i].used = 0;
		blockdev_slots[i].disk = NULL;
		blockdev_slots[i].dev = NULL;
		return;
	}
}
