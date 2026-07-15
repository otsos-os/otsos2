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
$define %func chainfs_find_file as function with args const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_find_free_file_entry as function with args u32 *, u32 *
$define %func chainfs_read_block_map_entry as function with args u32, u32 *
$define %func chainfs_write_block_map_entry as function with args u32, u32
$define %func chainfs_find_free_blocks as function with args u32, u32 *
$define %func chainfs_free_block_chain as procedure with args u32
$define %func chainfs_read_file as function with args const char *, u8 *, u32, u32 *
$define %func chainfs_read_file_range as function with args const char *, u8 *, u32, u32, u32 *
$define %func chainfs_write_file as function with args const char *, const u8 *, u32
$define %func chainfs_link as function with args const char *, const char *
$define %func chainfs_delete_file as function with args const char *
$define %func chainfs_get_file_list as function with args chainfs_file_entry_t *, u32, u32 *
$define %func read_entry_by_index as function with args u32, chainfs_file_entry_t *, u32 *, u32 *
$define %func split_path as function with args const char *, char[][32], int
$define %func chainfs_find_in_directory as function with args u32, const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_resolve_path as function with args const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_mkdir as function with args const char *
$define %func chainfs_create_socket as function with args const char *
$define %func chainfs_chdir as function with args const char *
$define %func chainfs_list_dir as function with args const char *, chainfs_file_entry_t *, u32, u32 *
$define %func chainfs_get_current_path as function with args char *, u32
$define %func chainfs_rmdir as function with args const char *

*/

/* !SPACE!

$space %internal read_entry_by_index, split_path
$space %export chainfs_init, chainfs_format, chainfs_find_file
$space %export chainfs_find_free_file_entry, chainfs_read_block_map_entry
$space %export chainfs_write_block_map_entry, chainfs_find_free_blocks
$space %export chainfs_free_block_chain, chainfs_read_file
$space %export chainfs_read_file_range, chainfs_write_file
$space %export chainfs_link, chainfs_delete_file, chainfs_get_file_list
$space %export chainfs_find_in_directory, chainfs_resolve_path
$space %export chainfs_mkdir, chainfs_create_socket, chainfs_chdir
$space %export chainfs_list_dir, chainfs_get_current_path, chainfs_rmdir
$space %export g_chainfs, g_chainfs_phys

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/api/errno.h>
#include <mm/vm/pmap.h>

chainfs_t	g_chainfs;
u64		g_chainfs_phys;

#define	ENTRIES_PER_BLOCK \
    (CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t))

int
chainfs_init(disk_t *disk)
{
	chainfs_superblock_t	*sb;

	if (!disk) {
		drivers_log("ChainFS: init failed, disk is NULL\n");
		return (-1);
	}
	g_chainfs.disk = disk;
	drivers_log("ChainFS: Initializing... "
	    "(g_chainfs at %p, disk: %s)\n",
	    &g_chainfs, disk ? disk->name : "NULL");

	disk_read(g_chainfs.disk, 0, g_chainfs.sector_buffer);

	sb = (chainfs_superblock_t *)g_chainfs.sector_buffer;

	if (sb->magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: Invalid magic number "
		    "0x%x, expected 0x%x\n",
		    sb->magic, CHAINFS_MAGIC);
		return (-1);
	}

	g_chainfs.superblock = *sb;

	g_chainfs.data_area_start = 1 +
	    g_chainfs.superblock.file_table_block_count +
	    g_chainfs.superblock.block_map_block_count;

	g_chainfs.current_dir_block =
	    g_chainfs.superblock.root_dir_block;
	g_chainfs_phys = pmap_extract((u64)&g_chainfs);
	drivers_log("[CHAINFS] g_chainfs phys=%p\n",
	    (void *)g_chainfs_phys);

	drivers_log("ChainFS: Initialized successfully\n");
	drivers_log("  Total blocks: %u\n",
	    g_chainfs.superblock.block_count);
	drivers_log("  File table blocks: %u\n",
	    g_chainfs.superblock.file_table_block_count);
	drivers_log("  Block map blocks: %u\n",
	    g_chainfs.superblock.block_map_block_count);
	drivers_log("  Data area start: %u\n",
	    g_chainfs.data_area_start);
	drivers_log("  Root directory block: %u\n",
	    g_chainfs.superblock.root_dir_block);

	return (0);
}

int
chainfs_format(u32 total_blocks, u32 max_files)
{
	u32	entries_per_block, file_table_blocks;
	u32	data_blocks, map_entries_per_block;
	u32	block_map_blocks;
	chainfs_superblock_t	sb;
	chainfs_file_entry_t	*entries;
	u32	*block_map;
	int	i;
	u32	block;

	if (!g_chainfs.disk) {
		drivers_log("ChainFS: format failed, disk is NULL\n");
		return (-1);
	}
	if (total_blocks < 8 || max_files == 0) {
		drivers_log("ChainFS: format failed, invalid params "
		    "blocks=%u files=%u\n", total_blocks, max_files);
		return (-1);
	}

	drivers_log("ChainFS: Formatting disk with %u blocks, "
	    "%u max files\n", total_blocks, max_files);

	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);
	file_table_blocks =
	    (max_files + entries_per_block - 1) / entries_per_block;
	if (file_table_blocks >= (total_blocks - 2)) {
		drivers_log("ChainFS: format failed, file table too "
		    "large (%u blocks)\n", file_table_blocks);
		return (-1);
	}

	data_blocks = total_blocks - 1 - file_table_blocks;
	map_entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	block_map_blocks =
	    (data_blocks + map_entries_per_block - 1) /
	    map_entries_per_block;
	if (1 + file_table_blocks + block_map_blocks >=
	    total_blocks) {
		drivers_log("ChainFS: format failed, no data area "
		    "left\n");
		return (-1);
	}

	data_blocks = total_blocks - 1 - file_table_blocks -
	    block_map_blocks;

	memset(&sb, 0, sizeof(sb));
	sb.magic = CHAINFS_MAGIC;
	sb.block_count = total_blocks;
	sb.file_table_block_count = file_table_blocks;
	sb.block_map_block_count = block_map_blocks;
	sb.total_files = max_files;
	sb.root_dir_block = 0;

	for (i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
		g_chainfs.sector_buffer[i] = 0;
	}
	*((chainfs_superblock_t *)g_chainfs.sector_buffer) = sb;
	disk_write(g_chainfs.disk, 0, g_chainfs.sector_buffer);

	for (i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
		g_chainfs.sector_buffer[i] = 0;
	}

	entries = (chainfs_file_entry_t *)g_chainfs.sector_buffer;
	entries[0].status = 1;
	entries[0].type = CHAINFS_TYPE_DIR;
	entries[0].name[0] = '/';
	entries[0].name[1] = 0;
	entries[0].size = 0;
	entries[0].start_block = 0;
	entries[0].parent_block = 0xFFFFFFFF;
	entries[0].nlink = 1;

	disk_write(g_chainfs.disk, 1, g_chainfs.sector_buffer);

	for (i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
		g_chainfs.sector_buffer[i] = 0;
	}

	for (block = 2; block < 1 + file_table_blocks; block++) {
		disk_write(g_chainfs.disk, block,
		    g_chainfs.sector_buffer);
	}

	block_map = (u32 *)g_chainfs.sector_buffer;
	for (i = 0; i < (int)map_entries_per_block; i++) {
		block_map[i] = CHAINFS_FREE_BLOCK;
	}

	for (block = 1 + file_table_blocks;
	    block < 1 + file_table_blocks + block_map_blocks;
	    block++) {
		disk_write(g_chainfs.disk, block,
		    g_chainfs.sector_buffer);
	}

	drivers_log("ChainFS: Format complete\n");

	return (chainfs_init(g_chainfs.disk));
}

int
chainfs_find_file(const char *filename, chainfs_file_entry_t *entry,
    u32 *entry_block, u32 *entry_offset)
{
	if (strchr(filename, '/') != 0) {
		return (chainfs_resolve_path(filename, entry,
		    entry_block, entry_offset));
	}

	return (chainfs_find_in_directory(
	    g_chainfs.current_dir_block, filename, entry,
	    entry_block, entry_offset));
}

int
chainfs_find_free_file_entry(u32 *entry_block, u32 *entry_offset)
{
	u32			entries_per_block, block, i;
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-2);
	}
	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

	for (block = 1;
	    block < 1 + g_chainfs.superblock.file_table_block_count;
	    block++) {
		disk_read(g_chainfs.disk, block,
		    g_chainfs.sector_buffer);
		entries = (chainfs_file_entry_t *)
		    g_chainfs.sector_buffer;

		for (i = 0; i < entries_per_block; i++) {
			if (entries[i].status == 0) {
				*entry_block = block;
				*entry_offset = i;
				return (0);
			}
		}
	}

	return (-1);
}

int
chainfs_read_block_map_entry(u32 block_index, u32 *next_block)
{
	u32	entries_per_block, map_block, map_offset, sector;
	u32	*map_entries;

	entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	map_block = block_index / entries_per_block;
	map_offset = block_index % entries_per_block;

	if (map_block >=
	    g_chainfs.superblock.block_map_block_count) {
		return (-1);
	}

	sector = 1 + g_chainfs.superblock.file_table_block_count +
	    map_block;
	disk_read(g_chainfs.disk, sector, g_chainfs.sector_buffer);

	map_entries = (u32 *)g_chainfs.sector_buffer;
	*next_block = map_entries[map_offset];

	return (0);
}

int
chainfs_write_block_map_entry(u32 block_index, u32 next_block)
{
	u32	entries_per_block, map_block, map_offset, sector;
	u32	*map_entries;

	entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	map_block = block_index / entries_per_block;
	map_offset = block_index % entries_per_block;

	if (map_block >=
	    g_chainfs.superblock.block_map_block_count) {
		return (-1);
	}

	sector = 1 + g_chainfs.superblock.file_table_block_count +
	    map_block;
	disk_read(g_chainfs.disk, sector, g_chainfs.sector_buffer);

	map_entries = (u32 *)g_chainfs.sector_buffer;
	map_entries[map_offset] = next_block;

	disk_write(g_chainfs.disk, sector,
	    g_chainfs.sector_buffer);

	return (0);
}

int
chainfs_find_free_blocks(u32 count, u32 *blocks)
{
	u32	found, entries_per_block, total_data_blocks;
	u32	i, next_block;

	found = 0;
	entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	total_data_blocks = g_chainfs.superblock.block_count -
	    g_chainfs.data_area_start;

	for (i = 0; i < total_data_blocks && found < count; i++) {
		if (chainfs_read_block_map_entry(i, &next_block)
		    == 0) {
			if (next_block == CHAINFS_FREE_BLOCK) {
				blocks[found++] = i;
			}
		}
	}

	return (found == count) ? 0 : -1;
}

void
chainfs_free_block_chain(u32 start_block)
{
	u32	current_block, next_block;

	current_block = start_block;

	while (current_block != CHAINFS_EOF_MARKER) {
		if (chainfs_read_block_map_entry(current_block,
		    &next_block) != 0) {
			break;
		}

		chainfs_write_block_map_entry(current_block,
		    CHAINFS_FREE_BLOCK);
		current_block = next_block;
	}
}

int
chainfs_read_file(const char *filename, u8 *buffer, u32 buffer_size,
    u32 *bytes_read)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			remaining, copied, current_block;
	u32			real_sector, to_copy, next_block, i;

	if (chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset) != 0) {
		drivers_log("ChainFS: File '%s' not found\n",
		    filename);
		return (-1);
	}

	remaining = entry.size;
	copied = 0;
	current_block = entry.start_block;

	while (remaining > 0 && current_block != CHAINFS_EOF_MARKER &&
	    copied < buffer_size) {
		real_sector = g_chainfs.data_area_start +
		    current_block;
		disk_read(g_chainfs.disk, real_sector,
		    g_chainfs.sector_buffer);

		to_copy = remaining;
		if (to_copy > CHAINFS_BLOCK_SIZE) {
			to_copy = CHAINFS_BLOCK_SIZE;
		}
		if (copied + to_copy > buffer_size) {
			to_copy = buffer_size - copied;
		}

		for (i = 0; i < to_copy; i++) {
			buffer[copied + i] =
			    g_chainfs.sector_buffer[i];
		}

		copied += to_copy;
		remaining -= to_copy;

		if (remaining > 0) {
			if (chainfs_read_block_map_entry(
			    current_block, &next_block) != 0) {
				break;
			}
			current_block = next_block;
		}
	}

	*bytes_read = copied;
	drivers_log("ChainFS: Read %u bytes from '%s'\n",
	    copied, filename);
	return (0);
}

int
chainfs_read_file_range(const char *filename, u8 *buffer,
    u32 buffer_size, u32 offset, u32 *bytes_read)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			remaining, block_skip, intra_offset;
	u32			current_block, copied, real_sector;
	u32			to_copy, next_block, i;

	if (bytes_read == NULL || buffer == NULL) {
		return (-1);
	}

	if (chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	if (offset >= entry.size) {
		*bytes_read = 0;
		return (0);
	}

	remaining = entry.size - offset;
	if (remaining > buffer_size) {
		remaining = buffer_size;
	}

	block_skip = offset / CHAINFS_BLOCK_SIZE;
	intra_offset = offset % CHAINFS_BLOCK_SIZE;
	current_block = entry.start_block;

	for (i = 0; i < block_skip; i++) {
		if (chainfs_read_block_map_entry(current_block,
		    &next_block) != 0) {
			return (-1);
		}
		if (next_block == CHAINFS_EOF_MARKER) {
			*bytes_read = 0;
			return (0);
		}
		current_block = next_block;
	}

	copied = 0;
	while (remaining > 0) {
		real_sector = g_chainfs.data_area_start +
		    current_block;
		disk_read(g_chainfs.disk, real_sector,
		    g_chainfs.sector_buffer);

		to_copy = CHAINFS_BLOCK_SIZE - intra_offset;
		if (to_copy > remaining) {
			to_copy = remaining;
		}

		for (i = 0; i < to_copy; i++) {
			buffer[copied + i] =
			    g_chainfs.sector_buffer[intra_offset + i];
		}

		copied += to_copy;
		remaining -= to_copy;
		intra_offset = 0;

		if (remaining == 0) {
			break;
		}

		if (chainfs_read_block_map_entry(current_block,
		    &next_block) != 0) {
			return (-1);
		}
		if (next_block == CHAINFS_EOF_MARKER) {
			break;
		}
		current_block = next_block;
	}

	*bytes_read = copied;
	/*drivers_log("ChainFS: Read %u bytes from '%s' "
	    "(offset %u)\n", copied, filename, offset);*/
	return (0);
}

int
chainfs_write_file(const char *filename, const u8 *data, u32 size)
{
	chainfs_file_entry_t	entry, parent_entry;
	u32			entry_block, entry_offset;
	u32			parent_entry_block, parent_entry_offset;
	u32			parent_block, blocks_needed;
	u32			*allocated_blocks, remaining, data_offset;
	u32			real_sector, next_block, to_copy;
	u32			name_len, i, j;
	char			file_name[32];
	char			parent_path[CHAINFS_MAX_PATH];
	int			path_len, last_slash;
	int			file_exists;
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: write failed, filesystem "
		    "not initialized\n");
		return (-1);
	}

	file_exists = (chainfs_find_file(filename, &entry,
	    &entry_block, &entry_offset) == 0);

	parent_block = g_chainfs.current_dir_block;

	if (strchr(filename, '/') != 0) {
		path_len = strlen(filename);
		last_slash = -1;

		for (i = path_len - 1; i > 0; i--) {
			if (filename[i] == '/') {
				last_slash = (int)i;
				break;
			}
		}

		if (last_slash == -1) {
			strcpy(file_name, filename);
		} else if (last_slash == 0) {
			parent_block =
			    g_chainfs.superblock.root_dir_block;
			strcpy(file_name, filename + 1);
		} else {
			for (i = 0; i < (u32)last_slash; i++) {
				parent_path[i] = filename[i];
			}
			parent_path[last_slash] = 0;
			strcpy(file_name, filename + last_slash + 1);

			if (chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset) != 0) {
				drivers_log("ChainFS: Parent directory "
				    "not found: %s\n", parent_path);
				return (-1);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				drivers_log("ChainFS: Parent is not "
				    "a directory: %s\n",
				    parent_path);
				return (-1);
			}
			parent_block =
			    (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK +
			    parent_entry_offset;
		}
	} else {
		strcpy(file_name, filename);
	}

	if (file_exists) {
		chainfs_free_block_chain(entry.start_block);
	} else {
		int	res;

		res = chainfs_find_free_file_entry(&entry_block,
		    &entry_offset);
		if (res == -2) {
			drivers_log("ChainFS: Filesystem not "
			    "initialized!\n");
			return (-2);
		} else if (res != 0) {
			drivers_log("ChainFS: No free file entries "
			    "(disk full or too many files)\n");
			return (-1);
		}
	}

	blocks_needed = (size + CHAINFS_BLOCK_SIZE - 1) /
	    CHAINFS_BLOCK_SIZE;
	if (blocks_needed == 0) {
		blocks_needed = 1;
	}

	allocated_blocks = (u32 *)kmem_alloc(
	    blocks_needed * sizeof(u32));
	if (!allocated_blocks) {
		drivers_log("ChainFS: Memory allocation failed\n");
		return (-1);
	}

	if (chainfs_find_free_blocks(blocks_needed,
	    allocated_blocks) != 0) {
		drivers_log("ChainFS: Not enough free blocks\n");
		kmem_free(allocated_blocks);
		return (-1);
	}

	remaining = size;
	data_offset = 0;

	for (i = 0; i < blocks_needed; i++) {
		for (j = 0; j < CHAINFS_BLOCK_SIZE; j++) {
			g_chainfs.sector_buffer[j] = 0;
		}

		to_copy = remaining;
		if (to_copy > CHAINFS_BLOCK_SIZE) {
			to_copy = CHAINFS_BLOCK_SIZE;
		}

		for (j = 0; j < to_copy; j++) {
			g_chainfs.sector_buffer[j] =
			    data[data_offset + j];
		}

		real_sector = g_chainfs.data_area_start +
		    allocated_blocks[i];
		disk_write(g_chainfs.disk, real_sector,
		    g_chainfs.sector_buffer);

		next_block = (i + 1 < blocks_needed) ?
		    allocated_blocks[i + 1] : CHAINFS_EOF_MARKER;
		chainfs_write_block_map_entry(allocated_blocks[i],
		    next_block);

		remaining -= to_copy;
		data_offset += to_copy;
	}

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_FILE;
	for (i = 0; i < 30; i++) {
		entries[entry_offset].name[i] = 0;
	}

	name_len = strlen(file_name);
	if (name_len > 29) {
		name_len = 29;
	}
	for (i = 0; i < name_len; i++) {
		entries[entry_offset].name[i] = file_name[i];
	}

	entries[entry_offset].size = size;
	entries[entry_offset].start_block = allocated_blocks[0];
	entries[entry_offset].parent_block = parent_block;
	if (!file_exists) {
		entries[entry_offset].nlink = 1;
	}

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	kmem_free(allocated_blocks);

	drivers_log("ChainFS: Wrote %u bytes to '%s' using "
	    "%u blocks\n", size, filename, blocks_needed);
	return (0);
}
int
chainfs_symlink(const char *target, const char *linkpath)
{
	chainfs_file_entry_t	entry, parent_entry;
	u32			entry_block, entry_offset;
	u32			parent_entry_block, parent_entry_offset;
	u32			parent_block;
	u32			i, name_len, path_len, last_slash;
	u32			target_len;
	char			file_name[32];
	char			parent_path[CHAINFS_MAX_PATH];
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	target_len = strlen(target);
	if (target_len == 0 || target_len >= 256) {
		return (-1);
	}

	parent_block = g_chainfs.current_dir_block;

	if (strchr(linkpath, '/') != 0) {
		path_len = strlen(linkpath);
		last_slash = 0;

		for (i = path_len - 1; i > 0; i--) {
			if (linkpath[i] == '/') {
				last_slash = i;
				break;
			}
		}

		if (last_slash == 0) {
			parent_block =
			    g_chainfs.superblock.root_dir_block;
			strcpy(file_name, linkpath + 1);
		} else {
			for (i = 0; i < last_slash; i++) {
				parent_path[i] = linkpath[i];
			}
			parent_path[last_slash] = '\0';
			strcpy(file_name, linkpath + last_slash + 1);

			if (chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset) != 0) {
				return (-1);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				return (-1);
			}
			parent_block =
			    (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK + parent_entry_offset;
		}
	} else {
		strcpy(file_name, linkpath);
	}

	if (chainfs_find_free_file_entry(&entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_SYMLINK;
	for (i = 0; i < 30; i++) {
		entries[entry_offset].name[i] = 0;
	}

	name_len = strlen(file_name);
	if (name_len > 29) {
		name_len = 29;
	}
	for (i = 0; i < name_len; i++) {
		entries[entry_offset].name[i] = file_name[i];
	}

	entries[entry_offset].size = target_len;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	if (target_len <= 12) {
		for (i = 0; i < target_len; i++) {
			entries[entry_offset].reserved[i] =
			    (u8)target[i];
		}
		entries[entry_offset].start_block =
		    CHAINFS_EOF_MARKER;
	} else {
		u32		blocks_needed;
		u32		*allocated_blocks;
		u32		remaining, data_offset, to_copy;
		u32		real_sector, next_block;
		u32		j;

		blocks_needed = (target_len + CHAINFS_BLOCK_SIZE -
		    1) / CHAINFS_BLOCK_SIZE;

		allocated_blocks = (u32 *)kmem_alloc(
		    blocks_needed * sizeof(u32));
		if (!allocated_blocks) {
			disk_write(g_chainfs.disk, entry_block,
			    g_chainfs.sector_buffer);
			return (-1);
		}

		if (chainfs_find_free_blocks(blocks_needed,
		    allocated_blocks) != 0) {
			kmem_free(allocated_blocks);
			disk_write(g_chainfs.disk, entry_block,
			    g_chainfs.sector_buffer);
			return (-1);
		}

		remaining = target_len;
		data_offset = 0;

		for (i = 0; i < blocks_needed; i++) {
			for (j = 0; j < CHAINFS_BLOCK_SIZE; j++) {
				g_chainfs.sector_buffer[j] = 0;
			}

			to_copy = remaining;
			if (to_copy > CHAINFS_BLOCK_SIZE) {
				to_copy = CHAINFS_BLOCK_SIZE;
			}

			for (j = 0; j < to_copy; j++) {
				g_chainfs.sector_buffer[j] =
				    (u8)target[data_offset + j];
			}

			real_sector = g_chainfs.data_area_start +
			    allocated_blocks[i];
			disk_write(g_chainfs.disk, real_sector,
			    g_chainfs.sector_buffer);

			next_block = (i + 1 < blocks_needed) ?
			    allocated_blocks[i + 1] :
			    CHAINFS_EOF_MARKER;
			chainfs_write_block_map_entry(
			    allocated_blocks[i], next_block);

			remaining -= to_copy;
			data_offset += to_copy;
		}

		entries[entry_offset].start_block =
		    allocated_blocks[0];
		kmem_free(allocated_blocks);
	}

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	return (0);
}

int
chainfs_link(const char *oldpath, const char *newpath)
{
	chainfs_file_entry_t	old_entry, parent_entry;
	u32			old_block, old_offset;
	u32			entry_block, entry_offset;
	u32			parent_entry_block, parent_entry_offset;
	u32			parent_block, i, path_len, last_slash;
	char			file_name[32];
	char			parent_path[CHAINFS_MAX_PATH];
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	if (chainfs_find_file(oldpath, &old_entry, &old_block,
	    &old_offset) != 0) {
		return (-1);
	}

	if (old_entry.type == CHAINFS_TYPE_DIR) {
		return (-1);
	}

	parent_block = g_chainfs.current_dir_block;

	if (strchr(newpath, '/') != 0) {
		path_len = strlen(newpath);
		last_slash = 0;

		for (i = path_len - 1; i > 0; i--) {
			if (newpath[i] == '/') {
				last_slash = i;
				break;
			}
		}

		if (last_slash == 0) {
			parent_block =
			    g_chainfs.superblock.root_dir_block;
			strcpy(file_name, newpath + 1);
		} else {
			for (i = 0; i < last_slash; i++) {
				parent_path[i] = newpath[i];
			}
			parent_path[last_slash] = '\0';
			strcpy(file_name, newpath + last_slash + 1);

			if (chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset) != 0) {
				return (-1);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				return (-1);
			}
			parent_block =
			    (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK + parent_entry_offset;
		}
	} else {
		strcpy(file_name, newpath);
	}

	if (chainfs_find_free_file_entry(&entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	disk_read(g_chainfs.disk, old_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;
	entries[old_offset].nlink++;
	disk_write(g_chainfs.disk, old_block,
	    g_chainfs.sector_buffer);

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = old_entry.type;
	for (i = 0; i < 30; i++) {
		entries[entry_offset].name[i] = 0;
	}
	for (i = 0; i < 29 && file_name[i] != '\0'; i++) {
		entries[entry_offset].name[i] = file_name[i];
	}
	entries[entry_offset].size = old_entry.size;
	entries[entry_offset].start_block = old_entry.start_block;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	return (0);
}

int
chainfs_readlink(const char *path, char *buf, u32 bufsize)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			to_read;
	u32			bytes_read;

	if (chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		return (-1);
	}

	if (entry.type != CHAINFS_TYPE_SYMLINK) {
		return (-1);
	}

	if (bufsize == 0) {
		return (-1);
	}

	if (entry.size <= 12 && entry.start_block ==
	    CHAINFS_EOF_MARKER) {
		u32	i;
		u32	copy_len;

		copy_len = entry.size;
		if (copy_len >= bufsize) {
			copy_len = bufsize - 1;
		}

		for (i = 0; i < copy_len; i++) {
			buf[i] = (char)entry.reserved[i];
		}
		buf[copy_len] = '\0';
		return ((int)copy_len);
	}

	to_read = entry.size;
	if (to_read >= bufsize) {
		to_read = bufsize - 1;
	}

	bytes_read = 0;
	if (chainfs_read_file_range(path, (u8 *)buf, to_read,
	    0, &bytes_read) != 0) {
		return (-1);
	}

	buf[bytes_read] = '\0';
	return ((int)bytes_read);
}

int
chainfs_delete_file(const char *filename)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	chainfs_file_entry_t	*entries;

	if (chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset) != 0) {
		drivers_log("ChainFS: File '%s' not found\n",
		    filename);
		return (-1);
	}

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	if (entries[entry_offset].nlink > 1) {
		entries[entry_offset].nlink--;
	} else {
		chainfs_free_block_chain(entry.start_block);
		entries[entry_offset].status = 0;
	}

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	drivers_log("ChainFS: Deleted file '%s'\n", filename);
	return (0);
}

int
chainfs_get_file_list(chainfs_file_entry_t *files, u32 max_files,
    u32 *file_count)
{
	return (chainfs_list_dir("", files, max_files,
	    file_count));
}

static int
read_entry_by_index(u32 index, chainfs_file_entry_t *entry,
    u32 *block, u32 *offset)
{
	u32			b, o;
	chainfs_file_entry_t	*entries;

	if (index >= g_chainfs.superblock.total_files) {
		return (-1);
	}

	b = 1 + (index / ENTRIES_PER_BLOCK);
	o = index % ENTRIES_PER_BLOCK;

	disk_read(g_chainfs.disk, b, g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;
	*entry = entries[o];
	if (block) {
		*block = b;
	}
	if (offset) {
		*offset = o;
	}
	return (0);
}

static int
split_path(const char *path, char components[][32], int max_components)
{
	int	count, start, len, i, j;

	count = 0;
	start = 0;
	len = strlen(path);

	if (len > 0 && path[0] == '/') {
		start = 1;
	}

	for (i = start; i <= len && count < max_components; i++) {
		if (path[i] == '/' || path[i] == 0) {
			int	comp_len;

			comp_len = i - start;
			if (comp_len > 0 && comp_len < 31) {
				for (j = 0; j < comp_len; j++) {
					components[count][j] =
					    path[start + j];
				}
				components[count][comp_len] = 0;
				count++;
			}
			start = i + 1;
		}
	}

	return (count);
}

int
chainfs_find_in_directory(u32 dir_block, const char *name,
    chainfs_file_entry_t *entry, u32 *entry_block, u32 *entry_offset)
{
	u32			entries_per_block, block, i;
	chainfs_file_entry_t	*entries;

	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

	for (block = 1;
	    block < 1 + g_chainfs.superblock.file_table_block_count;
	    block++) {
		disk_read(g_chainfs.disk, block,
		    g_chainfs.sector_buffer);
		entries = (chainfs_file_entry_t *)
		    g_chainfs.sector_buffer;

		for (i = 0; i < entries_per_block; i++) {
			if (entries[i].status == 1 &&
			    entries[i].parent_block == dir_block &&
			    strcmp(entries[i].name, name) == 0) {
				*entry = entries[i];
				*entry_block = block;
				*entry_offset = i;
				return (0);
			}
		}
	}

	return (-1);
}

int
chainfs_resolve_path(const char *path, chainfs_file_entry_t *entry,
    u32 *entry_block, u32 *entry_offset)
{
	char	components[16][32];
	int	comp_count, i;
	u32	current_block, root_idx, root_block, root_offset;
	u32	found_block, found_offset;
	chainfs_file_entry_t	found_entry;
	chainfs_file_entry_t	*entries;

	comp_count = split_path(path, components, 16);

	current_block = (path[0] == '/') ?
	    g_chainfs.superblock.root_dir_block :
	    g_chainfs.current_dir_block;

	if (comp_count == 0 && path[0] == '/') {
		root_idx = g_chainfs.superblock.root_dir_block;
		root_block = 1 + (root_idx / ENTRIES_PER_BLOCK);
		root_offset = root_idx % ENTRIES_PER_BLOCK;
		disk_read(g_chainfs.disk, root_block,
		    g_chainfs.sector_buffer);
		entries = (chainfs_file_entry_t *)
		    g_chainfs.sector_buffer;
		*entry = entries[root_offset];
		*entry_block = root_block;
		*entry_offset = root_offset;
		return (0);
	}

	for (i = 0; i < comp_count; i++) {
		if (chainfs_find_in_directory(current_block,
		    components[i], &found_entry, &found_block,
		    &found_offset) != 0) {
			return (-1);
		}

		if (i == comp_count - 1) {
			*entry = found_entry;
			*entry_block = found_block;
			*entry_offset = found_offset;
			return (0);
		} else {
			if (found_entry.type != CHAINFS_TYPE_DIR) {
				return (-1);
			}
			current_block = (found_block - 1) *
			    ENTRIES_PER_BLOCK + found_offset;
		}
	}

	return (-1);
}

int
chainfs_mkdir(const char *path)
{
	char			parent_path[CHAINFS_MAX_PATH];
	char			dir_name[32];
	int			path_len, last_slash, i;
	chainfs_file_entry_t	parent_entry, existing_entry;
	u32			parent_block, parent_offset;
	u32			existing_block, existing_offset;
	u32			entry_block, entry_offset;
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: mkdir failed, filesystem "
		    "not initialized\n");
		return (-1);
	}

	path_len = strlen(path);
	last_slash = -1;

	for (i = path_len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			last_slash = i;
			break;
		}
	}

	if (last_slash == -1) {
		parent_path[0] = 0;
		strcpy(dir_name, path);
	} else if (last_slash == 0) {
		parent_path[0] = '/';
		parent_path[1] = 0;
		strcpy(dir_name, path + 1);
	} else {
		for (i = 0; i < last_slash; i++) {
			parent_path[i] = path[i];
		}
		parent_path[last_slash] = 0;
		strcpy(dir_name, path + last_slash + 1);
	}

	if (parent_path[0] == 0) {
		parent_block = g_chainfs.current_dir_block;
	} else {
		if (chainfs_resolve_path(parent_path,
		    &parent_entry, &parent_block,
		    &parent_offset) != 0) {
			drivers_log("ChainFS: Parent directory "
			    "not found: %s\n", parent_path);
			return (-1);
		}
		if (parent_entry.type != CHAINFS_TYPE_DIR) {
			drivers_log("ChainFS: Parent is not "
			    "a directory: %s\n", parent_path);
			return (-1);
		}
		parent_block = (parent_block - 1) *
		    ENTRIES_PER_BLOCK + parent_offset;
	}

	if (chainfs_find_in_directory(parent_block, dir_name,
	    &existing_entry, &existing_block,
	    &existing_offset) == 0) {
		drivers_log("ChainFS: Directory already exists: "
		    "%s\n", path);
		return (-1);
	}

	if (chainfs_find_free_file_entry(&entry_block,
	    &entry_offset) != 0) {
		drivers_log("ChainFS: No free file entries\n");
		return (-1);
	}

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_DIR;
	strcpy(entries[entry_offset].name, dir_name);
	entries[entry_offset].size = 0;
	entries[entry_offset].start_block = 0;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	drivers_log("ChainFS: Created directory: %s\n", path);
	return (0);
}

int
chainfs_chdir(const char *path)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;

	if (chainfs_resolve_path(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		drivers_log("ChainFS: Directory not found: %s\n",
		    path);
		return (-API_ERR_NOT_FOUND);
	}

	if (entry.type != CHAINFS_TYPE_DIR) {
		drivers_log("ChainFS: Not a directory: %s\n",
		    path);
		return (-API_ERR_NOT_DIR);
	}

	g_chainfs.current_dir_block =
	    (entry_block - 1) * ENTRIES_PER_BLOCK +
	    entry_offset;
	drivers_log("ChainFS: Changed directory to: %s\n",
	    path);
	return (0);
}

int
chainfs_list_dir(const char *path, chainfs_file_entry_t *files,
    u32 max_files, u32 *file_count)
{
	chainfs_file_entry_t	dir_entry;
	u32			dir_block, dir_offset, entries_per_block;
	u32			found, block, i;
	chainfs_file_entry_t	*entries;

	if (path[0] == 0) {
		dir_block = g_chainfs.current_dir_block;
	} else {
		if (chainfs_resolve_path(path, &dir_entry,
		    &dir_block, &dir_offset) != 0) {
			drivers_log("ChainFS: Directory not "
			    "found: %s\n", path);
			return (-API_ERR_NOT_FOUND);
		}

		if (dir_entry.type != CHAINFS_TYPE_DIR) {
			drivers_log("ChainFS: Not a directory: "
			    "%s\n", path);
			return (-API_ERR_NOT_DIR);
		}
		dir_block = (dir_block - 1) *
		    ENTRIES_PER_BLOCK + dir_offset;
	}

	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);
	found = 0;

	for (block = 1;
	    block < 1 + g_chainfs.superblock.file_table_block_count &&
	    found < max_files;
	    block++) {
		disk_read(g_chainfs.disk, block,
		    g_chainfs.sector_buffer);
		entries = (chainfs_file_entry_t *)
		    g_chainfs.sector_buffer;

		for (i = 0; i < entries_per_block && found < max_files;
		    i++) {
			if (entries[i].status == 1 &&
			    entries[i].parent_block == dir_block) {
				files[found] = entries[i];
				found++;
			}
		}
	}

	*file_count = found;
	return (0);
}

char *
chainfs_get_current_path(char *buffer, u32 buffer_size)
{
	char	temp_path[CHAINFS_MAX_PATH];
	u32	current_idx;
	chainfs_file_entry_t	entry;

	if (g_chainfs.current_dir_block ==
	    g_chainfs.superblock.root_dir_block) {
		if (buffer_size >= 2) {
			buffer[0] = '/';
			buffer[1] = 0;
			return (buffer);
		}
		return (0);
	}

	temp_path[0] = 0;
	current_idx = g_chainfs.current_dir_block;

	while (current_idx !=
	    g_chainfs.superblock.root_dir_block &&
	    current_idx != 0xFFFFFFFF) {
		char	new_name[CHAINFS_MAX_PATH];

		if (read_entry_by_index(current_idx, &entry,
		    NULL, NULL) != 0) {
			break;
		}

		new_name[0] = '/';
		strcpy(new_name + 1, entry.name);
		strcat(new_name, temp_path);
		strcpy(temp_path, new_name);

		current_idx = entry.parent_block;
	}

	if (strlen(temp_path) == 0) {
		strcpy(temp_path, "/");
	}

	if (strlen(temp_path) < buffer_size) {
		strcpy(buffer, temp_path);
		return (buffer);
	}

	return (0);
}

int
chainfs_create_socket(const char *path)
{
	char			parent_path[CHAINFS_MAX_PATH];
	char			sock_name[32];
	int			path_len, last_slash, i;
	chainfs_file_entry_t	parent_entry, existing_entry;
	u32			parent_block, parent_offset;
	u32			existing_block, existing_offset;
	u32			entry_block, entry_offset;
	chainfs_file_entry_t	*entries;

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		return (-1);
	}

	path_len = strlen(path);
	last_slash = -1;

	for (i = path_len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			last_slash = i;
			break;
		}
	}

	if (last_slash == -1) {
		parent_path[0] = 0;
		strcpy(sock_name, path);
	} else if (last_slash == 0) {
		parent_path[0] = '/';
		parent_path[1] = 0;
		strcpy(sock_name, path + 1);
	} else {
		for (i = 0; i < last_slash; i++)
			parent_path[i] = path[i];
		parent_path[last_slash] = 0;
		strcpy(sock_name, path + last_slash + 1);
	}

	if (parent_path[0] == 0) {
		parent_block = g_chainfs.current_dir_block;
	} else {
		if (chainfs_resolve_path(parent_path, &parent_entry,
		    &parent_block, &parent_offset) != 0)
			return (-1);
		if (parent_entry.type != CHAINFS_TYPE_DIR)
			return (-1);
		parent_block = (parent_block - 1) *
		    ENTRIES_PER_BLOCK + parent_offset;
	}

	if (chainfs_find_in_directory(parent_block, sock_name,
	    &existing_entry, &existing_block, &existing_offset) == 0)
		return (-1);

	if (chainfs_find_free_file_entry(&entry_block,
	    &entry_offset) != 0)
		return (-1);

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_SOCK;
	strcpy(entries[entry_offset].name, sock_name);
	entries[entry_offset].size = 0;
	entries[entry_offset].start_block = 0;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	return (0);
}

int
chainfs_rmdir(const char *path)
{
	chainfs_file_entry_t	entry, files[1];
	u32			entry_block, entry_offset, file_count;
	chainfs_file_entry_t	*entries;

	if (chainfs_resolve_path(path, &entry, &entry_block,
	    &entry_offset) != 0) {
		drivers_log("ChainFS: Directory not found: %s\n",
		    path);
		return (-1);
	}

	if (entry.type != CHAINFS_TYPE_DIR) {
		drivers_log("ChainFS: Not a directory: %s\n",
		    path);
		return (-1);
	}

	if (entry_block ==
	    g_chainfs.superblock.root_dir_block) {
		drivers_log("ChainFS: Cannot remove root "
		    "directory\n");
		return (-1);
	}

	if (chainfs_list_dir(path, files, 1, &file_count) == 0 &&
	    file_count > 0) {
		drivers_log("ChainFS: Directory not empty: %s\n",
		    path);
		return (-1);
	}

	disk_read(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);
	entries = (chainfs_file_entry_t *)
	    g_chainfs.sector_buffer;
	entries[entry_offset].status = 0;
	disk_write(g_chainfs.disk, entry_block,
	    g_chainfs.sector_buffer);

	drivers_log("ChainFS: Removed directory: %s\n", path);
	return (0);
}
