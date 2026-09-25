/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type ldisk_info_t as identity and geometry of one block device
$define %type ldisk_entry_t as one enumerated block device with its handle path
$define %type ldisk_dev_t as an opened block device
$define %type ldisk_part_t as one GPT partition request and its placement
$define %type ldisk_fs_t as which filesystem writer to address on a target

$define %func ldisk_enumerate as function with args ldisk_entry_t *, int, int *
$define %func ldisk_find_slice as function with args const char *, uint64_t, ldisk_entry_t *
$define %func ldisk_open as function with args const char *, ldisk_dev_t *
$define %func ldisk_close as procedure with args ldisk_dev_t *
$define %func ldisk_refresh as function with args ldisk_dev_t *
$define %func ldisk_probe_fs as function with args ldisk_dev_t *
$define %func ldisk_flush as function with args ldisk_dev_t *
$define %func ldisk_rescan as function with args ldisk_dev_t *
$define %func ldisk_gpt_begin as function with args ldisk_dev_t *
$define %func ldisk_gpt_add as function with args ldisk_dev_t *, ldisk_part_t *
$define %func ldisk_gpt_commit as function with args ldisk_dev_t *
$define %func ldisk_mkfs_chainfs as function with args ldisk_dev_t *, uint64_t, uint32_t
$define %func ldisk_mkfs_fat32 as function with args ldisk_dev_t *, const char *
$define %func ldisk_mkdir as function with args ldisk_dev_t *, ldisk_fs_t, const char *
$define %func ldisk_write as function with args ldisk_dev_t *, ldisk_fs_t, const char *, const void *, uint64_t, uint64_t, uint64_t
$define %func ldisk_truncate as function with args ldisk_dev_t *, const char *, uint64_t
$define %func ldisk_read as function with args ldisk_dev_t *, const char *, void *, uint64_t, uint64_t *
$define %func ldisk_boot_install as function with args ldisk_dev_t *, const void *, uint64_t, const void *, uint64_t, uint64_t
$define %func ldisk_chunk_max as function with args void
$define %func ldisk_boot_sector as function with args void
$define %func ldisk_stage2_max as function with args void

*/

/* !SPACE!

$space %export ldisk_info_t, ldisk_entry_t, ldisk_dev_t, ldisk_part_t
$space %export ldisk_fs_t
$space %export ldisk_enumerate, ldisk_find_slice
$space %export ldisk_open, ldisk_close, ldisk_refresh, ldisk_probe_fs
$space %export ldisk_flush, ldisk_rescan
$space %export ldisk_gpt_begin, ldisk_gpt_add, ldisk_gpt_commit
$space %export ldisk_mkfs_chainfs, ldisk_mkfs_fat32
$space %export ldisk_mkdir, ldisk_write, ldisk_truncate, ldisk_read
$space %export ldisk_boot_install
$space %export ldisk_chunk_max, ldisk_boot_sector, ldisk_stage2_max

*/

#ifndef LIBDISK_DISK_H
#define LIBDISK_DISK_H

#include <kernel/api/disk_abi.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>


#define LDISK_NAME_MAX		DIOC_NAME_MAX
#define LDISK_MODEL_MAX		DIOC_MODEL_MAX
#define LDISK_PART_NAME_MAX	DIOC_PART_NAME_MAX
#define LDISK_FS_PATH_MAX	DIOC_PATH_MAX
#define LDISK_PATH_MAX		256
#define LDISK_DEVS_MAX		64
#define LDISK_F_READONLY	DIOC_INFO_F_READONLY
#define LDISK_F_NOFLUSH		DIOC_INFO_F_NOFLUSH
#define LDISK_F_SLICE		DIOC_INFO_F_SLICE
#define LDISK_F_CHAINFS		DIOC_INFO_F_CHAINFS
#define LDISK_F_FAT		DIOC_INFO_F_FAT
#define LDISK_F_ROOT		DIOC_INFO_F_ROOT
#define LDISK_PART_ESP		DIOC_GPT_KIND_ESP
#define LDISK_PART_BIOS		DIOC_GPT_KIND_BIOS
#define LDISK_PART_ROOT		DIOC_GPT_KIND_ROOT
#define LDISK_PART_REST		(~0ULL)

typedef enum ldisk_fs {
	LDISK_FS_CHAINFS = 0,
	LDISK_FS_FAT32 = 1
} ldisk_fs_t;

typedef struct ldisk_info {
	char		name[LDISK_NAME_MAX];
	char		parent[LDISK_NAME_MAX];
	char		model[LDISK_MODEL_MAX];
	uint64_t	total_sectors;
	uint64_t	base_lba;
	uint32_t	sector_size;
	uint32_t	max_io_sectors;
	uint32_t	type;
	uint32_t	flags;
	uint32_t	index;
} ldisk_info_t;


typedef struct ldisk_entry {
	ldisk_info_t	info;
	char		path[LDISK_PATH_MAX];
} ldisk_entry_t;

typedef struct ldisk_dev {
	ldisk_info_t	info;
	char		path[LDISK_PATH_MAX];
	int		handle;
} ldisk_dev_t;

typedef struct ldisk_part {
	char		name[LDISK_PART_NAME_MAX];
	uint32_t	kind;
	uint64_t	size_sectors;
	uint32_t	index;
	uint64_t	first_lba;
	uint64_t	last_lba;
} ldisk_part_t;

int		ldisk_enumerate(ldisk_entry_t *out, int max, int *count);
int		ldisk_find_slice(const char *parent_name, uint64_t first_lba,
		    ldisk_entry_t *out);

int		ldisk_open(const char *entity_path, ldisk_dev_t *dev);
void		ldisk_close(ldisk_dev_t *dev);
int		ldisk_refresh(ldisk_dev_t *dev);

int		ldisk_probe_fs(ldisk_dev_t *dev);
int		ldisk_flush(ldisk_dev_t *dev);
int		ldisk_rescan(ldisk_dev_t *dev);

int		ldisk_gpt_begin(ldisk_dev_t *dev);
int		ldisk_gpt_add(ldisk_dev_t *dev, ldisk_part_t *part);
int		ldisk_gpt_commit(ldisk_dev_t *dev);

int		ldisk_mkfs_chainfs(ldisk_dev_t *dev, uint64_t total_blocks,
		    uint32_t max_files);
int		ldisk_mkfs_fat32(ldisk_dev_t *dev, const char *label);

int		ldisk_mkdir(ldisk_dev_t *dev, ldisk_fs_t fs, const char *path);
int		ldisk_write(ldisk_dev_t *dev, ldisk_fs_t fs, const char *path,
		    const void *data, uint64_t size, uint64_t offset,
		    uint64_t total);
int		ldisk_truncate(ldisk_dev_t *dev, const char *path,
		    uint64_t size);
int		ldisk_read(ldisk_dev_t *dev, const char *path, void *buf,
		    uint64_t size, uint64_t *got);

int		ldisk_boot_install(ldisk_dev_t *dev, const void *stage1,
		    uint64_t stage1_size, const void *stage2,
		    uint64_t stage2_size, uint64_t stage2_lba);

uint64_t	ldisk_chunk_max(void);
uint64_t	ldisk_boot_sector(void);
uint64_t	ldisk_stage2_max(void);

#endif
