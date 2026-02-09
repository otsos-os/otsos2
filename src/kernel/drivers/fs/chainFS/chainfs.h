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

#ifndef CHAINFS_H
#define CHAINFS_H

#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define CHAINFS_MAGIC 0xCAFEBABE
#define CHAINFS_BLOCK_SIZE 512
#define CHAINFS_MAX_FILENAME 31
#define CHAINFS_EOF_MARKER 0xFFFFFFFF
#define CHAINFS_FREE_BLOCK 0x00000000
#define CHAINFS_MAX_PATH 256

#define CHAINFS_TYPE_FILE 0
#define CHAINFS_TYPE_DIR 1
#define CHAINFS_TYPE_DEV 2

typedef struct {
  u32 magic; // 0xCAFEBABE
  u32 block_count;
  u32 file_table_block_count; // File Table (M)
  u32 block_map_block_count;  // Block Map (N)
  u32 total_files;
  u32 root_dir_block; // Root directory entry block
  u8 padding[488];
} __attribute__((packed)) chainfs_superblock_t;

typedef struct {
  u8 status;     // 0 = free, 1 = exists
  u8 type;       // 0 = file, 1 = directory
  char name[30]; // Reduced by 1 to make room for type
  u32 size;
  u32 start_block;
  u32 parent_block; // Parent directory entry block (0 for root)
  u8 reserved[16];  // Reduced reserved space
} __attribute__((packed)) chainfs_file_entry_t;

#include <kernel/drivers/disk/disk.h>

typedef struct {
  chainfs_superblock_t superblock;
  u32 data_area_start;
    u32 current_dir_block;        // Current directory for relative paths
  u8 sector_buffer[CHAINFS_BLOCK_SIZE];
  disk_t *disk;
} chainfs_t;

extern chainfs_t g_chainfs;
extern u64 g_chainfs_phys;

int chainfs_init(disk_t *disk);
int chainfs_format(u32 total_blocks, u32 max_files);
int chainfs_read_file(const char *filename, u8 *buffer, u32 buffer_size,
                      u32 *bytes_read);
int chainfs_read_file_range(const char *filename, u8 *buffer, u32 buffer_size,
                            u32 offset, u32 *bytes_read);
int chainfs_write_file(const char *filename, const u8 *data, u32 size);
int chainfs_delete_file(const char *filename);
int chainfs_get_file_list(chainfs_file_entry_t *files, u32 max_files,
                          u32 *file_count);

// Directory operations
int chainfs_mkdir(const char *path);
int chainfs_rmdir(const char *path);
int chainfs_chdir(const char *path);
int chainfs_list_dir(const char *path, chainfs_file_entry_t *files,
                     u32 max_files, u32 *file_count);
char *chainfs_get_current_path(char *buffer, u32 buffer_size);
int chainfs_mknod(const char *path, u16 major, u16 minor);

// Path resolution
int chainfs_resolve_path(const char *path, chainfs_file_entry_t *entry,
                         u32 *entry_block, u32 *entry_offset);
int chainfs_find_in_directory(u32 dir_block, const char *name,
                              chainfs_file_entry_t *entry, u32 *entry_block,
                              u32 *entry_offset);

int chainfs_find_file(const char *filename, chainfs_file_entry_t *entry,
                      u32 *entry_block, u32 *entry_offset);
int chainfs_find_free_file_entry(u32 *entry_block, u32 *entry_offset);
int chainfs_find_free_blocks(u32 count, u32 *blocks);
int chainfs_read_block_map_entry(u32 block_index, u32 *next_block);
int chainfs_write_block_map_entry(u32 block_index, u32 next_block);
void chainfs_free_block_chain(u32 start_block);

#endif
