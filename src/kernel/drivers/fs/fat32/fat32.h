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
/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type disk_t as registered block device
$define %type fat32_bpb_t as FAT32 boot parameter block on disk
$define %type fat32_dirent_t as FAT32 directory entry on disk
$define %type fat32_volume_t as mounted FAT32 volume geometry

$define %func fat32_probe_sector as function with args const void *
$define %func fat32_format_disk as function with args disk_t *, const char *
$define %func fat32_mount as function with args disk_t *, fat32_volume_t *
$define %func fat32_write_file as function with args disk_t *, const char *, const void *, u32
$define %func fat32_read_file as function with args disk_t *, const char *, void *, u32, u32 *
$define %func fat32_mkdir as function with args disk_t *, const char *

*/

/* !SPACE!

$space %export fat32_bpb_t, fat32_dirent_t, fat32_volume_t
$space %export fat32_probe_sector, fat32_format_disk, fat32_mount
$space %export fat32_write_file, fat32_read_file, fat32_mkdir

*/

#ifndef KERNEL_DRIVERS_FS_FAT32_FAT32_H
#define KERNEL_DRIVERS_FS_FAT32_FAT32_H

#include <kernel/drivers/disk/disk.h>

#define	FAT32_SECTOR_SIZE	512
#define	FAT32_MIN_CLUSTERS	65525
#define	FAT32_EOC		0x0FFFFFF8U
#define	FAT32_MASK		0x0FFFFFFFU
#define	FAT32_ATTR_READ_ONLY	0x01
#define	FAT32_ATTR_HIDDEN	0x02
#define	FAT32_ATTR_SYSTEM	0x04
#define	FAT32_ATTR_VOLUME_ID	0x08
#define	FAT32_ATTR_DIRECTORY	0x10
#define	FAT32_ATTR_ARCHIVE	0x20
#define	FAT32_MAX_PATH		128
#define	FAT32_EPOCH_YEAR	1980U
#define	FAT32_ZERO_BATCH	64U

typedef struct fat32_bpb {
	u8	jmp[3];
	u8	oem[8];
	u16	bytes_per_sector;
	u8	sectors_per_cluster;
	u16	reserved_sectors;
	u8	num_fats;
	u16	root_entries;
	u16	total_sectors16;
	u8	media;
	u16	fat_size16;
	u16	sectors_per_track;
	u16	num_heads;
	u32	hidden_sectors;
	u32	total_sectors32;
	u32	fat_size32;
	u16	ext_flags;
	u16	fs_version;
	u32	root_cluster;
	u16	fsinfo_sector;
	u16	backup_boot_sector;
	u8	reserved[12];
	u8	drive_number;
	u8	reserved1;
	u8	boot_signature;
	u32	volume_id;
	u8	volume_label[11];
	u8	fs_type[8];
} __attribute__((packed)) fat32_bpb_t;

typedef struct fat32_dirent {
	u8	name[11];
	u8	attr;
	u8	nt_reserved;
	u8	create_time_tenth;
	u16	create_time;
	u16	create_date;
	u16	access_date;
	u16	cluster_hi;
	u16	write_time;
	u16	write_date;
	u16	cluster_lo;
	u32	size;
} __attribute__((packed)) fat32_dirent_t;

typedef struct fat32_volume {
	disk_t	*disk;
	u32	fat_lba;
	u32	fat_sectors;
	u32	num_fats;
	u32	sectors_per_cluster;
	u32	data_lba;
	u32	root_cluster;
	u32	cluster_count;
	u32	fsinfo_lba;
	u32	bytes_per_cluster;
} fat32_volume_t;

int	fat32_probe_sector(const void *sector);
int	fat32_format_disk(disk_t *disk, const char *label);
int	fat32_mount(disk_t *disk, fat32_volume_t *vol);
int	fat32_write_file(disk_t *disk, const char *path, const void *data,
	    u32 size);
int	fat32_read_file(disk_t *disk, const char *path, void *buf, u32 size,
	    u32 *out_size);
int	fat32_mkdir(disk_t *disk, const char *path);

#endif
