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
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type chainfs_superblock_t as packed struct with magic, block counts, root dir
$define %type chainfs_file_entry_t as packed struct with status, type, name, size, blocks
$define %type chainfs_t as struct with superblock, data area, dir, sector buffer, disk
$define %type disk_t as struct with name, type, sector_size, sectors, ops

$define %func chainfs_init as function with args disk_t *
$define %func chainfs_format as function with args u32, u32
$define %func chainfs_read_file as function with args const char *, u8 *, u32, u32 *
$define %func chainfs_read_file_range as function with args const char *, u8 *, u32, u32, u32 *
$define %func chainfs_write_file as function with args const char *, const u8 *, u32
$define %func chainfs_delete_file as function with args const char *
$define %func chainfs_get_file_list as function with args chainfs_file_entry_t *, u32, u32 *
$define %func chainfs_mkdir as function with args const char *
$define %func chainfs_rmdir as function with args const char *
$define %func chainfs_chdir as function with args const char *
$define %func chainfs_list_dir as function with args const char *, chainfs_file_entry_t *, u32, u32 *
$define %func chainfs_get_current_path as function with args char *, u32
$define %func chainfs_resolve_path as function with args const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_find_in_directory as function with args u32, const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_find_file as function with args const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_find_free_file_entry as function with args u32 *, u32 *
$define %func chainfs_find_free_blocks as function with args u32, u32 *
$define %func chainfs_read_block_map_entry as function with args u32, u32 *
$define %func chainfs_write_block_map_entry as function with args u32, u32
$define %func chainfs_free_block_chain as procedure with args u32

*/

/* !SPACE!

$space %export chainfs_init, chainfs_format, chainfs_read_file
$space %export chainfs_read_file_range, chainfs_write_file
$space %export chainfs_delete_file, chainfs_get_file_list
$space %export chainfs_mkdir, chainfs_rmdir, chainfs_chdir
$space %export chainfs_list_dir, chainfs_get_current_path
$space %export chainfs_resolve_path, chainfs_find_in_directory
$space %export chainfs_find_file, chainfs_find_free_file_entry
$space %export chainfs_find_free_blocks
$space %export chainfs_read_block_map_entry
$space %export chainfs_write_block_map_entry
$space %export chainfs_free_block_chain
$space %export g_chainfs, g_chainfs_phys

*/

#ifndef CHAINFS_H
#define CHAINFS_H

#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define	CHAINFS_MAGIC		0xCAFEBABE
#define	CHAINFS_BLOCK_SIZE	512
#define	CHAINFS_MAX_FILENAME	31
#define	CHAINFS_EOF_MARKER	0xFFFFFFFF
#define	CHAINFS_FREE_BLOCK	0x00000000
#define	CHAINFS_MAX_PATH	256

#define	CHAINFS_TYPE_FILE	0
#define	CHAINFS_TYPE_DIR	1

typedef struct {
	u32	magic;
	u32	block_count;
	u32	file_table_block_count;
	u32	block_map_block_count;
	u32	total_files;
	u32	root_dir_block;
	u8	padding[488];
} __attribute__((packed)) chainfs_superblock_t;

typedef struct {
	u8	status;
	u8	type;
	char	name[30];
	u32	size;
	u32	start_block;
	u32	parent_block;
	u8	reserved[16];
} __attribute__((packed)) chainfs_file_entry_t;

#include <kernel/drivers/disk/disk.h>

typedef struct {
	chainfs_superblock_t	superblock;
	u32			data_area_start;
	u32			current_dir_block;
	u8			sector_buffer[CHAINFS_BLOCK_SIZE];
	disk_t			*disk;
} chainfs_t;

extern chainfs_t	g_chainfs;
extern u64		g_chainfs_phys;

int	chainfs_init(disk_t *disk);
int	chainfs_format(u32 total_blocks, u32 max_files);
int	chainfs_read_file(const char *filename, u8 *buffer,
	    u32 buffer_size, u32 *bytes_read);
int	chainfs_read_file_range(const char *filename, u8 *buffer,
	    u32 buffer_size, u32 offset, u32 *bytes_read);
int	chainfs_write_file(const char *filename,
	    const u8 *data, u32 size);
int	chainfs_delete_file(const char *filename);
int	chainfs_get_file_list(chainfs_file_entry_t *files,
	    u32 max_files, u32 *file_count);

int	chainfs_mkdir(const char *path);
int	chainfs_rmdir(const char *path);
int	chainfs_chdir(const char *path);
int	chainfs_list_dir(const char *path,
	    chainfs_file_entry_t *files, u32 max_files,
	    u32 *file_count);
char	*chainfs_get_current_path(char *buffer, u32 buffer_size);

int	chainfs_resolve_path(const char *path,
	    chainfs_file_entry_t *entry, u32 *entry_block,
	    u32 *entry_offset);
int	chainfs_find_in_directory(u32 dir_block, const char *name,
	    chainfs_file_entry_t *entry, u32 *entry_block,
	    u32 *entry_offset);

int	chainfs_find_file(const char *filename,
	    chainfs_file_entry_t *entry, u32 *entry_block,
	    u32 *entry_offset);
int	chainfs_find_free_file_entry(u32 *entry_block,
	    u32 *entry_offset);
int	chainfs_find_free_blocks(u32 count, u32 *blocks);
int	chainfs_read_block_map_entry(u32 block_index,
	    u32 *next_block);
int	chainfs_write_block_map_entry(u32 block_index,
	    u32 next_block);
void	chainfs_free_block_chain(u32 start_block);

#endif
