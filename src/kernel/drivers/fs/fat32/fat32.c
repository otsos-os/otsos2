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
#include "fat32.h"

#include <kernel/drivers/disk/bio.h>
#include <kernel/time.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	FAT32_DIRENT_PER_SECTOR	(FAT32_SECTOR_SIZE / sizeof(fat32_dirent_t))
#define	FAT32_MAX_DIR_SECTORS	65536
#define	FAT32_MAX_CHAIN		(1U << 22)

static u16
fat32_rd16(const u8 *p)
{
	return ((u16)p[0] | ((u16)p[1] << 8));
}

static u32
fat32_rd32(const u8 *p)
{
	return ((u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) |
	    ((u32)p[3] << 24));
}

int
fat32_probe_sector(const void *sector)
{
	const u8	*p;
	u16		bps;
	u8		spc;

	if (sector == NULL) {
		return (-1);
	}
	p = (const u8 *)sector;
	if (p[510] != 0x55 || p[511] != 0xAA) {
		return (-1);
	}
	bps = fat32_rd16(p + 11);
	if (bps != 512 && bps != 1024 && bps != 2048 && bps != 4096) {
		return (-1);
	}
	spc = p[13];
	if (spc == 0 || (spc & (spc - 1)) != 0) {
		return (-1);
	}
	if (fat32_rd16(p + 17) != 0 || fat32_rd16(p + 22) != 0) {
		return (-1);
	}
	if (fat32_rd32(p + 36) == 0) {
		return (-1);
	}
	if (memcmp(p + 82, "FAT32   ", 8) != 0) {
		return (-1);
	}
	return (0);
}

int
fat32_mount(disk_t *disk, fat32_volume_t *vol)
{
	u8	sector[FAT32_SECTOR_SIZE];
	u32	total_sectors;
	u32	data_sectors;

	if (disk == NULL || vol == NULL) {
		return (-1);
	}
	if (disk->sector_size != FAT32_SECTOR_SIZE) {
		return (-1);
	}
	if (bio_read(disk, 0, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	if (fat32_probe_sector(sector) != 0) {
		return (-1);
	}
	memset(vol, 0, sizeof(*vol));
	vol->disk = disk;
	vol->sectors_per_cluster = sector[13];
	vol->num_fats = sector[16];
	vol->fat_lba = fat32_rd16(sector + 14);
	vol->fat_sectors = fat32_rd32(sector + 36);
	vol->root_cluster = fat32_rd32(sector + 44);
	vol->fsinfo_lba = fat32_rd16(sector + 48);
	total_sectors = fat32_rd32(sector + 32);

	if (vol->num_fats == 0 || vol->num_fats > 2 ||
	    vol->fat_sectors == 0 || vol->fat_lba == 0 ||
	    vol->root_cluster < 2) {
		return (-1);
	}
	vol->data_lba = vol->fat_lba + vol->num_fats * vol->fat_sectors;
	if (total_sectors <= vol->data_lba ||
	    total_sectors > disk->total_sectors) {
		return (-1);
	}
	data_sectors = total_sectors - vol->data_lba;
	vol->cluster_count = data_sectors / vol->sectors_per_cluster;
	vol->bytes_per_cluster = vol->sectors_per_cluster * FAT32_SECTOR_SIZE;
	if (vol->cluster_count < 16 || vol->root_cluster >
	    vol->cluster_count + 1) {
		return (-1);
	}
	return (0);
}

static u64
fat32_cluster_lba(const fat32_volume_t *vol, u32 cluster)
{
	return ((u64)vol->data_lba +
	    (u64)(cluster - 2) * vol->sectors_per_cluster);
}

static int
fat32_fat_get(const fat32_volume_t *vol, u32 cluster, u32 *value)
{
	u8	sector[FAT32_SECTOR_SIZE];
	u32	offset;
	u64	lba;

	if (cluster < 2 || cluster > vol->cluster_count + 1) {
		return (-1);
	}
	offset = cluster * 4;
	lba = (u64)vol->fat_lba + offset / FAT32_SECTOR_SIZE;
	if (bio_read(vol->disk, lba, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	*value = fat32_rd32(sector + (offset % FAT32_SECTOR_SIZE)) & FAT32_MASK;
	return (0);
}

static int
fat32_fat_set(const fat32_volume_t *vol, u32 cluster, u32 value)
{
	u8	sector[FAT32_SECTOR_SIZE];
	u32	offset;
	u32	i;
	u64	lba;

	if (cluster < 2 || cluster > vol->cluster_count + 1) {
		return (-1);
	}
	offset = cluster * 4;
	for (i = 0; i < vol->num_fats; i++) {
		lba = (u64)vol->fat_lba + (u64)i * vol->fat_sectors +
		    offset / FAT32_SECTOR_SIZE;
		if (bio_read(vol->disk, lba, 1, sector) != BIO_STATUS_OK) {
			return (-1);
		}
		*(u32 *)(sector + (offset % FAT32_SECTOR_SIZE)) =
		    value & FAT32_MASK;
		if (bio_write(vol->disk, lba, 1, sector) != BIO_STATUS_OK) {
			return (-1);
		}
	}
	return (0);
}

static int
fat32_cluster_zero(const fat32_volume_t *vol, u32 cluster)
{
	u8	sector[FAT32_SECTOR_SIZE];
	u32	i;
	u64	lba;

	memset(sector, 0, sizeof(sector));
	lba = fat32_cluster_lba(vol, cluster);
	for (i = 0; i < vol->sectors_per_cluster; i++) {
		if (bio_write(vol->disk, lba + i, 1, sector) != BIO_STATUS_OK) {
			return (-1);
		}
	}
	return (0);
}

static int
fat32_alloc_cluster(const fat32_volume_t *vol, u32 *out)
{
	u32	cluster;
	u32	value;

	for (cluster = 2; cluster <= vol->cluster_count + 1; cluster++) {
		if (fat32_fat_get(vol, cluster, &value) != 0) {
			return (-1);
		}
		if (value != 0) {
			continue;
		}
		if (fat32_fat_set(vol, cluster, FAT32_MASK) != 0) {
			return (-1);
		}
		*out = cluster;
		return (0);
	}
	return (-1);
}

static void
fat32_free_chain(const fat32_volume_t *vol, u32 cluster)
{
	u32	next;
	u32	guard;

	guard = 0;
	while (cluster >= 2 && cluster < FAT32_EOC &&
	    guard++ < FAT32_MAX_CHAIN) {
		if (fat32_fat_get(vol, cluster, &next) != 0) {
			return;
		}
		if (fat32_fat_set(vol, cluster, 0) != 0) {
			return;
		}
		cluster = next;
	}
}

static int
fat32_chain_extend(const fat32_volume_t *vol, u32 last, u32 *out)
{
	u32	cluster;

	if (fat32_alloc_cluster(vol, &cluster) != 0) {
		return (-1);
	}
	if (fat32_cluster_zero(vol, cluster) != 0) {
		return (-1);
	}
	if (last >= 2 && fat32_fat_set(vol, last, cluster) != 0) {
		return (-1);
	}
	*out = cluster;
	return (0);
}

static char
fat32_upper(char c)
{
	if (c >= 'a' && c <= 'z') {
		return ((char)(c - 'a' + 'A'));
	}
	return (c);
}

static int
fat32_char_ok(char c)
{
	static const char	bad[] = "\"*+,./:;<=>?[\\]|";

	if ((unsigned char)c < 0x20) {
		return (0);
	}
	if (strchr(bad, c) != NULL) {
		return (0);
	}
	return (1);
}

static int
fat32_name83(const char *name, u32 len, u8 *out)
{
	u32	i, n, dot;
	char	c;

	if (len == 0 || len > 12) {
		return (-1);
	}
	memset(out, ' ', 11);
	dot = len;
	for (i = 0; i < len; i++) {
		if (name[i] == '.') {
			if (dot != len) {
				return (-1);
			}
			dot = i;
		}
	}
	if (dot == 0 || dot > 8) {
		return (-1);
	}
	if (dot != len && (len - dot - 1) > 3) {
		return (-1);
	}
	for (i = 0; i < dot; i++) {
		c = fat32_upper(name[i]);
		if (!fat32_char_ok(c)) {
			return (-1);
		}
		out[i] = (u8)c;
	}
	if (dot == len) {
		return (0);
	}
	n = 0;
	for (i = dot + 1; i < len; i++) {
		c = fat32_upper(name[i]);
		if (!fat32_char_ok(c)) {
			return (-1);
		}
		out[8 + n] = (u8)c;
		n++;
	}
	return (0);
}

typedef struct fat32_slot {
	u64	lba;
	u32	offset;
} fat32_slot_t;

static int
fat32_dir_lookup(const fat32_volume_t *vol, u32 dir_cluster, const u8 *name83,
    fat32_dirent_t *out, fat32_slot_t *slot)
{
	u8		sector[FAT32_SECTOR_SIZE];
	fat32_dirent_t	*ent;
	u32		cluster;
	u32		i, k;
	u32		guard;
	u64		lba;

	cluster = dir_cluster;
	guard = 0;
	while (cluster >= 2 && cluster < FAT32_EOC &&
	    guard++ < FAT32_MAX_CHAIN) {
		lba = fat32_cluster_lba(vol, cluster);
		for (i = 0; i < vol->sectors_per_cluster; i++) {
			if (bio_read(vol->disk, lba + i, 1, sector) !=
			    BIO_STATUS_OK) {
				return (-1);
			}
			ent = (fat32_dirent_t *)sector;
			for (k = 0; k < FAT32_DIRENT_PER_SECTOR; k++) {
				if (ent[k].name[0] == 0x00) {
					return (-1);
				}
				if (ent[k].name[0] == 0xE5) {
					continue;
				}
				if ((ent[k].attr & FAT32_ATTR_VOLUME_ID) != 0 &&
				    (ent[k].attr & FAT32_ATTR_DIRECTORY) == 0) {
					continue;
				}
				if (memcmp(ent[k].name, name83, 11) != 0) {
					continue;
				}
				if (out != NULL) {
					*out = ent[k];
				}
				if (slot != NULL) {
					slot->lba = lba + i;
					slot->offset = k *
					    sizeof(fat32_dirent_t);
				}
				return (0);
			}
		}
		if (fat32_fat_get(vol, cluster, &cluster) != 0) {
			return (-1);
		}
	}
	return (-1);
}

static int
fat32_dir_alloc_slot(const fat32_volume_t *vol, u32 dir_cluster,
    fat32_slot_t *slot)
{
	u8		sector[FAT32_SECTOR_SIZE];
	fat32_dirent_t	*ent;
	u32		cluster, last;
	u32		i, k;
	u32		guard;
	u64		lba;

	cluster = dir_cluster;
	last = dir_cluster;
	guard = 0;
	while (cluster >= 2 && cluster < FAT32_EOC &&
	    guard++ < FAT32_MAX_CHAIN) {
		lba = fat32_cluster_lba(vol, cluster);
		for (i = 0; i < vol->sectors_per_cluster; i++) {
			if (bio_read(vol->disk, lba + i, 1, sector) !=
			    BIO_STATUS_OK) {
				return (-1);
			}
			ent = (fat32_dirent_t *)sector;
			for (k = 0; k < FAT32_DIRENT_PER_SECTOR; k++) {
				if (ent[k].name[0] != 0x00 &&
				    ent[k].name[0] != 0xE5) {
					continue;
				}
				slot->lba = lba + i;
				slot->offset = k * sizeof(fat32_dirent_t);
				return (0);
			}
		}
		last = cluster;
		if (fat32_fat_get(vol, cluster, &cluster) != 0) {
			return (-1);
		}
	}
	if (fat32_chain_extend(vol, last, &cluster) != 0) {
		return (-1);
	}
	slot->lba = fat32_cluster_lba(vol, cluster);
	slot->offset = 0;
	return (0);
}

static int
fat32_slot_write(const fat32_volume_t *vol, const fat32_slot_t *slot,
    const fat32_dirent_t *ent)
{
	u8	sector[FAT32_SECTOR_SIZE];

	if (bio_read(vol->disk, slot->lba, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	memcpy(sector + slot->offset, ent, sizeof(*ent));
	return (bio_write(vol->disk, slot->lba, 1, sector) == BIO_STATUS_OK ?
	    0 : -1);
}

static void
fat32_now(u16 *date, u16 *time_of_day)
{
	struct calendar_time	ct;
	struct timespec		ts;
	u32			year;

	getnanotime(&ts);
	calendar_from_epoch(ts.tv_sec, &ct);
	year = ct.year < FAT32_EPOCH_YEAR ? FAT32_EPOCH_YEAR : ct.year;
	if (year - FAT32_EPOCH_YEAR > 127) {
		year = FAT32_EPOCH_YEAR + 127;
	}
	*date = (u16)(((year - FAT32_EPOCH_YEAR) << 9) | (ct.month << 5) |
	    ct.mday);
	*time_of_day = (u16)((ct.hour << 11) | (ct.min << 5) | (ct.sec / 2));
}

static void
fat32_dirent_init(fat32_dirent_t *ent, const u8 *name83, u8 attr, u32 cluster,
    u32 size)
{
	u16	date;
	u16	tod;

	fat32_now(&date, &tod);
	memset(ent, 0, sizeof(*ent));
	memcpy(ent->name, name83, 11);
	ent->attr = attr;
	ent->cluster_lo = (u16)(cluster & 0xFFFF);
	ent->cluster_hi = (u16)((cluster >> 16) & 0xFFFF);
	ent->size = size;
	ent->create_date = date;
	ent->create_time = tod;
	ent->write_date = date;
	ent->write_time = tod;
	ent->access_date = date;
}

static int
fat32_dir_create(const fat32_volume_t *vol, u32 parent_cluster,
    const u8 *name83, u32 *out_cluster)
{
	fat32_dirent_t	ent;
	fat32_dirent_t	dots[2];
	fat32_slot_t	slot;
	u8		sector[FAT32_SECTOR_SIZE];
	u32		cluster;
	u16		date;
	u16		tod;

	if (fat32_alloc_cluster(vol, &cluster) != 0) {
		return (-1);
	}
	if (fat32_cluster_zero(vol, cluster) != 0) {
		return (-1);
	}

	memset(dots, 0, sizeof(dots));
	fat32_now(&date, &tod);
	memset(dots[0].name, ' ', 11);
	dots[0].name[0] = '.';
	dots[0].attr = FAT32_ATTR_DIRECTORY;
	dots[0].create_date = date;
	dots[0].create_time = tod;
	dots[0].write_date = date;
	dots[0].write_time = tod;
	dots[0].access_date = date;
	dots[0].cluster_lo = (u16)(cluster & 0xFFFF);
	dots[0].cluster_hi = (u16)((cluster >> 16) & 0xFFFF);
	memset(dots[1].name, ' ', 11);
	dots[1].name[0] = '.';
	dots[1].name[1] = '.';
	dots[1].attr = FAT32_ATTR_DIRECTORY;
	dots[1].create_date = date;
	dots[1].create_time = tod;
	dots[1].write_date = date;
	dots[1].write_time = tod;
	dots[1].access_date = date;
	if (parent_cluster != vol->root_cluster) {
		dots[1].cluster_lo = (u16)(parent_cluster & 0xFFFF);
		dots[1].cluster_hi = (u16)((parent_cluster >> 16) & 0xFFFF);
	}
	memset(sector, 0, sizeof(sector));
	memcpy(sector, dots, sizeof(dots));
	if (bio_write(vol->disk, fat32_cluster_lba(vol, cluster), 1, sector) !=
	    BIO_STATUS_OK) {
		return (-1);
	}

	if (fat32_dir_alloc_slot(vol, parent_cluster, &slot) != 0) {
		return (-1);
	}
	fat32_dirent_init(&ent, name83, FAT32_ATTR_DIRECTORY, cluster, 0);
	if (fat32_slot_write(vol, &slot, &ent) != 0) {
		return (-1);
	}
	*out_cluster = cluster;
	return (0);
}

static int
fat32_walk(const fat32_volume_t *vol, const char *path, int create_dirs,
    u32 *dir_cluster, const char **leaf, u32 *leaf_len)
{
	const char	*p;
	const char	*start;
	fat32_dirent_t	ent;
	u8		name83[11];
	u32		cluster;
	u32		len;
	u32		depth;

	if (path == NULL) {
		return (-1);
	}
	p = path;
	while (*p == '/' || *p == '\\') {
		p++;
	}
	if (*p == '\0') {
		return (-1);
	}
	cluster = vol->root_cluster;
	depth = 0;
	for (;;) {
		start = p;
		while (*p != '\0' && *p != '/' && *p != '\\') {
			p++;
		}
		len = (u32)(p - start);
		if (len == 0) {
			return (-1);
		}
		if (*p == '\0') {
			*dir_cluster = cluster;
			*leaf = start;
			*leaf_len = len;
			return (0);
		}
		if (++depth > 16) {
			return (-1);
		}
		if (fat32_name83(start, len, name83) != 0) {
			return (-1);
		}
		if (fat32_dir_lookup(vol, cluster, name83, &ent, NULL) == 0) {
			if ((ent.attr & FAT32_ATTR_DIRECTORY) == 0) {
				return (-1);
			}
			cluster = ((u32)ent.cluster_hi << 16) | ent.cluster_lo;
			if (cluster < 2 || cluster > vol->cluster_count + 1) {
				return (-1);
			}
		} else if (create_dirs) {
			if (fat32_dir_create(vol, cluster, name83,
			    &cluster) != 0) {
				return (-1);
			}
		} else {
			return (-1);
		}
		while (*p == '/' || *p == '\\') {
			p++;
		}
		if (*p == '\0') {
			return (-1);
		}
	}
}

static int
fat32_fsinfo_update(const fat32_volume_t *vol)
{
	u8	sector[FAT32_SECTOR_SIZE];

	if (vol->fsinfo_lba == 0) {
		return (0);
	}
	if (bio_read(vol->disk, vol->fsinfo_lba, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	if (fat32_rd32(sector) != 0x41615252U) {
		return (0);
	}
	*(u32 *)(sector + 488) = 0xFFFFFFFFU;
	*(u32 *)(sector + 492) = 0xFFFFFFFFU;
	return (bio_write(vol->disk, vol->fsinfo_lba, 1, sector) ==
	    BIO_STATUS_OK ? 0 : -1);
}

int
fat32_mkdir(disk_t *disk, const char *path)
{
	fat32_volume_t	vol;
	fat32_dirent_t	ent;
	const char	*leaf;
	u8		name83[11];
	u32		dir_cluster;
	u32		leaf_len;
	u32		cluster;

	if (fat32_mount(disk, &vol) != 0) {
		return (-1);
	}
	if (fat32_walk(&vol, path, 1, &dir_cluster, &leaf, &leaf_len) != 0) {
		return (-1);
	}
	if (fat32_name83(leaf, leaf_len, name83) != 0) {
		return (-1);
	}
	if (fat32_dir_lookup(&vol, dir_cluster, name83, &ent, NULL) == 0) {
		return ((ent.attr & FAT32_ATTR_DIRECTORY) != 0 ? 0 : -1);
	}
	if (fat32_dir_create(&vol, dir_cluster, name83, &cluster) != 0) {
		return (-1);
	}
	(void)fat32_fsinfo_update(&vol);
	if ((disk->flags & DISK_F_NO_FLUSH) == 0) {
		(void)bio_flush(disk);
	}
	return (0);
}

int
fat32_write_file(disk_t *disk, const char *path, const void *data, u32 size)
{
	fat32_volume_t	vol;
	fat32_dirent_t	ent;
	fat32_slot_t	slot;
	const char	*leaf;
	const u8	*src;
	u8		sector[FAT32_SECTOR_SIZE];
	u8		name83[11];
	u32		dir_cluster;
	u32		leaf_len;
	u32		cluster, prev;
	u32		remaining;
	u32		i;
	u64		lba;

	if (disk == NULL || data == NULL) {
		return (-1);
	}
	if (fat32_mount(disk, &vol) != 0) {
		return (-1);
	}
	if (fat32_walk(&vol, path, 1, &dir_cluster, &leaf, &leaf_len) != 0) {
		return (-1);
	}
	if (fat32_name83(leaf, leaf_len, name83) != 0) {
		return (-1);
	}

	if (fat32_dir_lookup(&vol, dir_cluster, name83, &ent, &slot) == 0) {
		if ((ent.attr & FAT32_ATTR_DIRECTORY) != 0) {
			return (-1);
		}
		cluster = ((u32)ent.cluster_hi << 16) | ent.cluster_lo;
		if (cluster >= 2) {
			fat32_free_chain(&vol, cluster);
		}
	} else {
		if (fat32_dir_alloc_slot(&vol, dir_cluster, &slot) != 0) {
			return (-1);
		}
	}

	src = (const u8 *)data;
	remaining = size;
	cluster = 0;
	prev = 0;
	while (remaining > 0) {
		u32	chunk;

		if (fat32_chain_extend(&vol, prev, &chunk) != 0) {
			if (cluster >= 2) {
				fat32_free_chain(&vol, cluster);
			}
			return (-1);
		}
		if (cluster == 0) {
			cluster = chunk;
		}
		prev = chunk;
		lba = fat32_cluster_lba(&vol, chunk);
		for (i = 0; i < vol.sectors_per_cluster && remaining > 0; i++) {
			u32	n;

			n = remaining > FAT32_SECTOR_SIZE ?
			    FAT32_SECTOR_SIZE : remaining;
			memset(sector, 0, sizeof(sector));
			memcpy(sector, src, n);
			if (bio_write(vol.disk, lba + i, 1, sector) !=
			    BIO_STATUS_OK) {
				fat32_free_chain(&vol, cluster);
				return (-1);
			}
			src += n;
			remaining -= n;
		}
	}

	fat32_dirent_init(&ent, name83, FAT32_ATTR_ARCHIVE,
	    cluster >= 2 ? cluster : 0, size);
	if (fat32_slot_write(&vol, &slot, &ent) != 0) {
		if (cluster >= 2) {
			fat32_free_chain(&vol, cluster);
		}
		return (-1);
	}
	(void)fat32_fsinfo_update(&vol);
	if ((disk->flags & DISK_F_NO_FLUSH) == 0) {
		(void)bio_flush(disk);
	}
	return ((int)size);
}

int
fat32_read_file(disk_t *disk, const char *path, void *buf, u32 size,
    u32 *out_size)
{
	fat32_volume_t	vol;
	fat32_dirent_t	ent;
	const char	*leaf;
	u8		sector[FAT32_SECTOR_SIZE];
	u8		name83[11];
	u8		*dst;
	u32		dir_cluster;
	u32		leaf_len;
	u32		cluster;
	u32		remaining;
	u32		guard;
	u32		i;
	u64		lba;

	if (disk == NULL || buf == NULL) {
		return (-1);
	}
	if (fat32_mount(disk, &vol) != 0) {
		return (-1);
	}
	if (fat32_walk(&vol, path, 0, &dir_cluster, &leaf, &leaf_len) != 0) {
		return (-1);
	}
	if (fat32_name83(leaf, leaf_len, name83) != 0) {
		return (-1);
	}
	if (fat32_dir_lookup(&vol, dir_cluster, name83, &ent, NULL) != 0) {
		return (-1);
	}
	if ((ent.attr & FAT32_ATTR_DIRECTORY) != 0) {
		return (-1);
	}
	if (ent.size > size) {
		return (-1);
	}
	dst = (u8 *)buf;
	remaining = ent.size;
	cluster = ((u32)ent.cluster_hi << 16) | ent.cluster_lo;
	guard = 0;
	while (remaining > 0) {
		if (cluster < 2 || cluster >= FAT32_EOC ||
		    guard++ > FAT32_MAX_CHAIN) {
			memset(buf, 0, size);
			return (-1);
		}
		lba = fat32_cluster_lba(&vol, cluster);
		for (i = 0; i < vol.sectors_per_cluster && remaining > 0; i++) {
			u32	n;

			if (bio_read(vol.disk, lba + i, 1, sector) !=
			    BIO_STATUS_OK) {
				memset(buf, 0, size);
				return (-1);
			}
			n = remaining > FAT32_SECTOR_SIZE ?
			    FAT32_SECTOR_SIZE : remaining;
			memcpy(dst, sector, n);
			dst += n;
			remaining -= n;
		}
		if (fat32_fat_get(&vol, cluster, &cluster) != 0) {
			memset(buf, 0, size);
			return (-1);
		}
	}
	if (out_size != NULL) {
		*out_size = ent.size;
	}
	return ((int)ent.size);
}

static int
fat32_zero_range(disk_t *disk, u64 lba, u64 count)
{
	u8	*buf;
	u32	batch;
	u32	chunk;

	if (count == 0) {
		return (0);
	}
	batch = disk->max_io_sectors;
	if (batch > FAT32_ZERO_BATCH) {
		batch = FAT32_ZERO_BATCH;
	}
	if (batch == 0) {
		batch = 1;
	}
	buf = kmem_alloc(batch * FAT32_SECTOR_SIZE);
	while (buf == NULL && batch > 1) {
		batch /= 2;
		buf = kmem_alloc(batch * FAT32_SECTOR_SIZE);
	}
	if (buf == NULL) {
		return (-1);
	}
	memset(buf, 0, batch * FAT32_SECTOR_SIZE);
	while (count > 0) {
		chunk = count > batch ? batch : (u32)count;
		if (bio_write(disk, lba, chunk, buf) != BIO_STATUS_OK) {
			kmem_free(buf);
			return (-1);
		}
		lba += chunk;
		count -= chunk;
	}
	kmem_free(buf);
	return (0);
}

static u32
fat32_fat_size(u64 total_sectors, u32 reserved, u32 spc, u32 num_fats)
{
	u64	tmp1;
	u64	tmp2;

	tmp1 = total_sectors - reserved;
	tmp2 = ((u64)FAT32_SECTOR_SIZE * spc) / 4 + num_fats;
	return ((u32)((tmp1 + (tmp2 - 1)) / tmp2));
}

static u32
fat32_pick_spc(u64 total_sectors)
{
	static const u32	candidates[] = { 1, 2, 4, 8, 16, 32, 64 };
	u32			best;
	u32			i;
	u64			clusters;

	best = 0;
	for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
		clusters = total_sectors / candidates[i];
		if (clusters < FAT32_MIN_CLUSTERS + 16) {
			break;
		}
		best = candidates[i];
	}
	return (best);
}

int
fat32_format_disk(disk_t *disk, const char *label)
{
	u8	sector[FAT32_SECTOR_SIZE];
	u8	*bpb;
	u64	total_sectors;
	u32	spc;
	u32	reserved;
	u32	fat_size;
	u32	data_sectors;
	u32	clusters;
	u32	i;

	if (disk == NULL) {
		return (-1);
	}
	if (disk->sector_size != FAT32_SECTOR_SIZE) {
		drivers_log("[FAT32] %s sector size %u unsupported\n",
		    disk->name, disk->sector_size);
		return (-1);
	}
	if ((disk->flags & DISK_F_READONLY) != 0) {
		return (-1);
	}
	total_sectors = disk->total_sectors;
	if (total_sectors > 0xFFFFFFFFULL) {
		total_sectors = 0xFFFFFFFFULL;
	}
	spc = fat32_pick_spc(total_sectors);
	if (spc == 0) {
		drivers_log("[FAT32] %s too small for FAT32 (%llu sectors, "
		    "need %u clusters)\n", disk->name,
		    (unsigned long long)total_sectors, FAT32_MIN_CLUSTERS);
		return (-1);
	}
	reserved = 32;
	fat_size = fat32_fat_size(total_sectors, reserved, spc, 2);
	if (fat_size == 0 ||
	    (u64)reserved + 2ULL * fat_size >= total_sectors) {
		return (-1);
	}
	data_sectors = (u32)total_sectors - reserved - 2 * fat_size;
	clusters = data_sectors / spc;
	if (clusters < FAT32_MIN_CLUSTERS) {
		drivers_log("[FAT32] %s yields only %u clusters\n", disk->name,
		    clusters);
		return (-1);
	}

	memset(sector, 0, sizeof(sector));
	bpb = sector;
	bpb[0] = 0xEB;
	bpb[1] = 0x58;
	bpb[2] = 0x90;
	memcpy(bpb + 3, "OTSOS2  ", 8);
	*(u16 *)(bpb + 11) = FAT32_SECTOR_SIZE;
	bpb[13] = (u8)spc;
	*(u16 *)(bpb + 14) = (u16)reserved;
	bpb[16] = 2;
	*(u16 *)(bpb + 17) = 0;
	*(u16 *)(bpb + 19) = 0;
	bpb[21] = 0xF8;
	*(u16 *)(bpb + 22) = 0;
	*(u16 *)(bpb + 24) = 63;
	*(u16 *)(bpb + 26) = 255;
	*(u32 *)(bpb + 28) = 0;
	*(u32 *)(bpb + 32) = (u32)total_sectors;
	*(u32 *)(bpb + 36) = fat_size;
	*(u16 *)(bpb + 40) = 0;
	*(u16 *)(bpb + 42) = 0;
	*(u32 *)(bpb + 44) = 2;
	*(u16 *)(bpb + 48) = 1;
	*(u16 *)(bpb + 50) = 6;
	bpb[64] = 0x80;
	bpb[66] = 0x29;
	*(u32 *)(bpb + 67) = 0x4F54534FU;
	memset(bpb + 71, ' ', 11);
	if (label != NULL) {
		for (i = 0; i < 11 && label[i] != '\0'; i++) {
			bpb[71 + i] = (u8)fat32_upper(label[i]);
		}
	} else {
		memcpy(bpb + 71, "OTSOS ESP  ", 11);
	}
	memcpy(bpb + 82, "FAT32   ", 8);
	bpb[510] = 0x55;
	bpb[511] = 0xAA;

	if (bio_write(disk, 0, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	if (bio_write(disk, 6, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}

	memset(sector, 0, sizeof(sector));
	*(u32 *)(sector + 0) = 0x41615252U;
	*(u32 *)(sector + 484) = 0x61417272U;
	*(u32 *)(sector + 488) = clusters - 1;
	*(u32 *)(sector + 492) = 2;
	sector[510] = 0x55;
	sector[511] = 0xAA;
	if (bio_write(disk, 1, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}
	if (bio_write(disk, 7, 1, sector) != BIO_STATUS_OK) {
		return (-1);
	}

	if (fat32_zero_range(disk, 2, 4) != 0) {
		return (-1);
	}
	if (fat32_zero_range(disk, 8, reserved - 8) != 0) {
		return (-1);
	}
	if (fat32_zero_range(disk, reserved, 2ULL * fat_size) != 0) {
		return (-1);
	}

	*(u32 *)(sector + 0) = 0x0FFFFFF8U;
	*(u32 *)(sector + 4) = 0xFFFFFFFFU;
	*(u32 *)(sector + 8) = 0x0FFFFFFFU;
	for (i = 0; i < 2; i++) {
		if (bio_write(disk, reserved + (u64)i * fat_size, 1, sector) !=
		    BIO_STATUS_OK) {
			return (-1);
		}
	}

	if (fat32_zero_range(disk, reserved + 2ULL * fat_size, spc) != 0) {
		return (-1);
	}

	if ((disk->flags & DISK_F_NO_FLUSH) == 0) {
		(void)bio_flush(disk);
	}
	drivers_log("[FAT32] %s formatted: %u clusters, spc %u, fat %u "
	    "sectors\n", disk->name, clusters, spc, fat_size);
	return (0);
}
