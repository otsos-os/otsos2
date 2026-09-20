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
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type cfsr_read_fn as callback reading sectors from a boot device
$define %type cfsr_superblock_t as ChainFS on-disk superblock
$define %type cfsr_entry_t as ChainFS on-disk file table entry
$define %type cfsr_volume_t as mounted read-only ChainFS volume

$define %func cfsr_probe as function with args const void *
$define %func cfsr_mount as function with args cfsr_volume_t *, cfsr_read_fn, void *, u64, u32
$define %func cfsr_lookup as function with args cfsr_volume_t *, const char *, cfsr_entry_t *
$define %func cfsr_read as function with args cfsr_volume_t *, const cfsr_entry_t *, void *, u32, u32 *
$define %func cfsr_dir_entry as function with args cfsr_volume_t *, const char *, u32, cfsr_entry_t *

*/

/* !SPACE!

$space %export cfsr_read_fn, cfsr_superblock_t, cfsr_entry_t
$space %export cfsr_volume_t
$space %export cfsr_probe, cfsr_mount, cfsr_lookup, cfsr_read
$space %export cfsr_dir_entry

*/

#ifndef BOOTLOADER_CFSREAD_H
#define BOOTLOADER_CFSREAD_H

#include <boot/bootloader/lib/types.h>

#define CFSR_BLOCK_SIZE		512U
#define CFSR_MAGIC		0xCAFEBABEU
#define CFSR_EOF_MARKER		0xFFFFFFFFU
#define CFSR_FREE_BLOCK		0x00000000U
#define CFSR_MAP_NONE		0xFFFFFFFFU
#define CFSR_TYPE_FILE		0
#define CFSR_TYPE_DIR		1
#define CFSR_TYPE_SYMLINK	2
#define CFSR_MAX_DEPTH		16U
#define CFSR_NAME_MAX		30U

typedef int	(*cfsr_read_fn)(void *ctx, u64 lba, u32 count, void *dst);

typedef struct {
	u32	magic;
	u32	block_count;
	u32	file_table_block_count;
	u32	block_map_block_count;
	u32	total_files;
	u32	root_dir_block;
	u8	padding[488];
} __attribute__((packed)) cfsr_superblock_t;

typedef struct {
	u8	status;
	u8	type;
	char	name[30];
	u32	size;
	u32	start_block;
	u32	parent_block;
	u32	nlink;
	u8	reserved[12];
} __attribute__((packed)) cfsr_entry_t;

typedef struct {
	cfsr_read_fn	read;
	void		*ctx;
	u64		base_lba;
	u32		block_count;
	u32		file_table_blocks;
	u32		block_map_blocks;
	u32		root_dir_block;
	u32		data_area_start;
	u32		map_cached;
	u32		max_run;
	u8		sector[CFSR_BLOCK_SIZE];
	u8		map[CFSR_BLOCK_SIZE];
} cfsr_volume_t;

int	cfsr_probe(const void *sector);
int	cfsr_mount(cfsr_volume_t *vol, cfsr_read_fn read, void *ctx,
	    u64 base_lba, u32 max_run);
int	cfsr_lookup(cfsr_volume_t *vol, const char *path, cfsr_entry_t *out);
int	cfsr_read(cfsr_volume_t *vol, const cfsr_entry_t *entry, void *dst,
	    u32 limit, u32 *out_size);
int	cfsr_dir_entry(cfsr_volume_t *vol, const char *path, u32 index,
	    cfsr_entry_t *out);

#endif
