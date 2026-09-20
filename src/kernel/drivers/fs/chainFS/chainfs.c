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
$define %type disk_t as struct with one registered block device and its geometry

$define %func cfs_sector_read as function with args u32, u8 *
$define %func cfs_sector_write as function with args u32, const u8 *
$define %func chainfs_init as function with args disk_t *
$define %func chainfs_root_disk as function with args void
$define %func chainfs_format as function with args u32, u32
$define %func chainfs_find_file as function with args const char *, chainfs_file_entry_t *, u32 *, u32 *
$define %func chainfs_find_free_file_entry as function with args u32 *, u32 *
$define %func chainfs_read_block_map_entry as function with args u32, u32 *
$define %func chainfs_write_block_map_entry as function with args u32, u32
$define %func chainfs_find_free_blocks as function with args u32, u32 *
$define %func chainfs_free_block_chain as procedure with args u32
$define %func chainfs_read_file as function with args const char *, u8 *, u32, u32 *
$define %func chainfs_read_file_range as function with args const char *, u8 *, u32, u32, u32 *
$define %func chainfs_write_file as function with args const char *, const u8 *, u32, u32
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
$define %func chainfs_list_dir_range as function with args const char *, u32, chainfs_file_entry_t *, u32, u32 *, u32 *
$define %func chainfs_get_current_path as function with args char *, u32
$define %func chainfs_rmdir as function with args const char *
$define %func chainfs_live_boot as function with args void

*/

/* !SPACE!

$space %internal cfs_sector_read, cfs_sector_write
$space %internal read_entry_by_index, split_path, chainfs_live_boot
$space %export chainfs_init, chainfs_root_disk, chainfs_format
$space %export chainfs_find_file
$space %export chainfs_find_free_file_entry, chainfs_read_block_map_entry
$space %export chainfs_write_block_map_entry, chainfs_find_free_blocks
$space %export chainfs_free_block_chain, chainfs_read_file
$space %export chainfs_read_file_range, chainfs_write_file
$space %export chainfs_link, chainfs_delete_file, chainfs_get_file_list
$space %export chainfs_find_in_directory, chainfs_resolve_path
$space %export chainfs_mkdir, chainfs_create_socket, chainfs_chdir
$space %export chainfs_list_dir, chainfs_list_dir_range
$space %export chainfs_get_current_path, chainfs_rmdir
$space %export g_chainfs, g_chainfs_phys

*/

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/disk/bio.h>
#include <kernel/drivers/fs/vfs/back/vfs_back.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/multiboot2.h>
#include <kernel/sync/sync.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>

chainfs_t	g_chainfs;
u64		g_chainfs_phys;
static int	chainfs_root_is_ram = 1;

static disk_t		*chainfs_root_dev;
static chainfs_t	*cfs = &g_chainfs;
static mtx_t		chainfs_ctx_lock;
static int		chainfs_ctx_lock_ready;

#define	ENTRIES_PER_BLOCK \
    (CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t))
#define	CHAINFS_BOOT_MAX_FILES	4096
#define	CFS_FORMAT_BATCH		64U
#define	CFS_SECTORS_PER_IO	1
#define	CFS_MAX_RUN_SECTORS	256U
#define	CFS_LIVE_MODULE		"init"

static int
cfs_sector_read(u32 sector, u8 *buffer)
{
	int	error;

	error = bio_read(cfs->disk, (u64)sector, CFS_SECTORS_PER_IO,
	    buffer);
	if (error != BIO_STATUS_OK) {
		drivers_log("[CHAINFS] read sector %u failed: %s\n", sector,
		    bio_status_name(error));
	}
	return (error);
}

static int
cfs_sector_write(u32 sector, const u8 *buffer)
{
	int	error;

	error = bio_write(cfs->disk, (u64)sector, CFS_SECTORS_PER_IO,
	    buffer);
	if (error != BIO_STATUS_OK) {
		drivers_log("[CHAINFS] write sector %u failed: %s\n", sector,
		    bio_status_name(error));
	}
	return (error);
}

static int	cfs_split_parent(const char *path, u32 *parent_block,
		    char *leaf_out);

static int
cfs_io_limit(void)
{
	u32	limit;

	limit = cfs->disk != NULL ? cfs->disk->max_io_sectors : 1;
	if (limit == 0) {
		limit = 1;
	}
	if (limit > CFS_MAX_RUN_SECTORS) {
		limit = CFS_MAX_RUN_SECTORS;
	}
	return ((int)limit);
}

static int
cfs_sectors_read(u32 sector, u32 count, u8 *buffer)
{
	u32	chunk, limit;
	int	error;

	limit = (u32)cfs_io_limit();
	while (count > 0) {
		chunk = count > limit ? limit : count;
		error = bio_read(cfs->disk, (u64)sector, chunk, buffer);
		if (error != BIO_STATUS_OK) {
			drivers_log("[CHAINFS] read %u+%u failed: %s\n",
			    sector, chunk, bio_status_name(error));
			return (error);
		}
		buffer += (u64)chunk * CHAINFS_BLOCK_SIZE;
		sector += chunk;
		count -= chunk;
	}
	return (BIO_STATUS_OK);
}

static int
cfs_sectors_write(u32 sector, u32 count, const u8 *buffer)
{
	u32	chunk, limit;
	int	error;

	limit = (u32)cfs_io_limit();
	while (count > 0) {
		chunk = count > limit ? limit : count;
		error = bio_write(cfs->disk, (u64)sector, chunk, buffer);
		if (error != BIO_STATUS_OK) {
			drivers_log("[CHAINFS] write %u+%u failed: %s\n",
			    sector, chunk, bio_status_name(error));
			return (error);
		}
		buffer += (u64)chunk * CHAINFS_BLOCK_SIZE;
		sector += chunk;
		count -= chunk;
	}
	return (BIO_STATUS_OK);
}

static void
cfs_map_invalidate(void)
{
	cfs->map_cached = CHAINFS_MAP_NONE;
	cfs->map_dirty = 0;
	cfs->seek_entry_block = CHAINFS_MAP_NONE;
}

static int
cfs_map_flush(void)
{
	u32	sector;
	int	error;

	if (cfs->map_cached == CHAINFS_MAP_NONE || !cfs->map_dirty) {
		return (0);
	}
	sector = 1 + cfs->superblock.file_table_block_count +
	    cfs->map_cached;
	error = cfs_sector_write(sector, cfs->map_buffer);
	if (error != BIO_STATUS_OK) {
		return (-API_ERR_IO);
	}
	cfs->map_dirty = 0;
	return (0);
}

static int
cfs_map_load(u32 map_block)
{
	u32	sector;

	if (map_block >= cfs->superblock.block_map_block_count) {
		return (-API_ERR_BAD_VALUE);
	}
	if (cfs->map_cached == map_block) {
		return (0);
	}
	if (cfs_map_flush() != 0) {
		return (-API_ERR_IO);
	}
	sector = 1 + cfs->superblock.file_table_block_count + map_block;
	if (cfs_sector_read(sector, cfs->map_buffer) != BIO_STATUS_OK) {
		cfs_map_invalidate();
		return (-API_ERR_IO);
	}
	cfs->map_cached = map_block;
	cfs->map_dirty = 0;
	return (0);
}

int
chainfs_sync(void)
{
	int	ret;

	ret = cfs_map_flush();
	if (cfs->disk != NULL) {
		(void)bio_flush(cfs->disk);
	}
	return (ret);
}

int
chainfs_init(disk_t *disk)
{
	chainfs_superblock_t	*sb;

	if (!disk) {
		drivers_log("ChainFS: init failed, disk is NULL\n");
		return (-1);
	}
	cfs->disk = disk;
	cfs_map_invalidate();
	drivers_log("ChainFS: Initializing... "
	    "(ctx at %p, disk: %s)\n",
	    cfs, disk ? disk->name : "NULL");

	cfs_sector_read(0, cfs->sector_buffer);

	sb = (chainfs_superblock_t *)cfs->sector_buffer;

	if (sb->magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: Invalid magic number "
		    "0x%x, expected 0x%x\n",
		    sb->magic, CHAINFS_MAGIC);
		return (-1);
	}

	cfs->superblock = *sb;

	cfs->data_area_start = 1 +
	    cfs->superblock.file_table_block_count +
	    cfs->superblock.block_map_block_count;

	cfs->current_dir_block =
	    cfs->superblock.root_dir_block;
	cfs->alloc_hint = 0;
	cfs_map_invalidate();
	if (cfs == &g_chainfs) {
		g_chainfs_phys = pmap_extract((u64)&g_chainfs);
		drivers_log("[CHAINFS] g_chainfs phys=%p\n",
		    (void *)g_chainfs_phys);
	}

	drivers_log("ChainFS: Initialized successfully\n");
	drivers_log("  Total blocks: %u\n",
	    cfs->superblock.block_count);
	drivers_log("  File table blocks: %u\n",
	    cfs->superblock.file_table_block_count);
	drivers_log("  Block map blocks: %u\n",
	    cfs->superblock.block_map_block_count);
	drivers_log("  Data area start: %u\n",
	    cfs->data_area_start);
	drivers_log("  Root directory block: %u\n",
	    cfs->superblock.root_dir_block);

	return (0);
}

int
chainfs_probe_sector(const void *sector)
{
	const chainfs_superblock_t	*sb;

	if (sector == NULL) {
		return (-1);
	}
	sb = (const chainfs_superblock_t *)sector;
	if (sb->magic != CHAINFS_MAGIC) {
		return (-1);
	}
	if (sb->block_count < 8) {
		return (-1);
	}
	if (sb->file_table_block_count == 0 ||
	    sb->block_map_block_count == 0) {
		return (-1);
	}
	if (1 + sb->file_table_block_count + sb->block_map_block_count >=
	    sb->block_count) {
		return (-1);
	}
	return (0);
}

static int
cfs_fill_range(disk_t *disk, u64 lba, u64 count, const u8 *pattern)
{
	u8	*buf;
	u32	batch;
	u32	chunk;
	u32	i;

	if (count == 0) {
		return (BIO_STATUS_OK);
	}
	batch = disk->max_io_sectors;
	if (batch > CFS_FORMAT_BATCH) {
		batch = CFS_FORMAT_BATCH;
	}
	if (batch == 0) {
		batch = 1;
	}
	buf = kmem_alloc(batch * CHAINFS_BLOCK_SIZE);
	while (buf == NULL && batch > 1) {
		batch /= 2;
		buf = kmem_alloc(batch * CHAINFS_BLOCK_SIZE);
	}
	if (buf == NULL) {
		return (BIO_STATUS_NOMEM);
	}
	for (i = 0; i < batch; i++) {
		if (pattern != NULL) {
			memcpy(buf + (u64)i * CHAINFS_BLOCK_SIZE, pattern,
			    CHAINFS_BLOCK_SIZE);
		} else {
			memset(buf + (u64)i * CHAINFS_BLOCK_SIZE, 0,
			    CHAINFS_BLOCK_SIZE);
		}
	}
	while (count > 0) {
		int	error;

		chunk = count > batch ? batch : (u32)count;
		error = bio_write(disk, lba, chunk, buf);
		if (error != BIO_STATUS_OK) {
			kmem_free(buf);
			return (error);
		}
		lba += chunk;
		count -= chunk;
	}
	kmem_free(buf);
	return (BIO_STATUS_OK);
}

int
chainfs_format_disk(disk_t *disk, u64 total_blocks, u32 max_files)
{
	u32	entries_per_block, file_table_blocks;
	u32	data_blocks, map_entries_per_block;
	u32	block_map_blocks;
	chainfs_superblock_t	sb;
	chainfs_file_entry_t	*entries;
	u32	*block_map;
	u8	*buffer;
	u32	blocks;
	u32	i;
	int	error;

	if (disk == NULL) {
		drivers_log("ChainFS: format failed, disk is NULL\n");
		return (-1);
	}
	if (disk->sector_size != CHAINFS_BLOCK_SIZE) {
		drivers_log("ChainFS: %s sector size %u unsupported\n",
		    disk->name, disk->sector_size);
		return (-1);
	}
	if ((disk->flags & DISK_F_READONLY) != 0) {
		drivers_log("ChainFS: %s is read-only\n", disk->name);
		return (-1);
	}
	if (total_blocks == 0 || total_blocks > disk->total_sectors) {
		total_blocks = disk->total_sectors;
	}
	if (total_blocks > 0xFFFFFFFFULL) {
		total_blocks = 0xFFFFFFFFULL;
	}
	blocks = (u32)total_blocks;
	if (max_files == 0) {
		max_files = CHAINFS_BOOT_MAX_FILES;
	}
	if (blocks < 8) {
		drivers_log("ChainFS: format failed, invalid params "
		    "blocks=%u files=%u\n", blocks, max_files);
		return (-1);
	}

	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);
	file_table_blocks =
	    (max_files + entries_per_block - 1) / entries_per_block;
	if (file_table_blocks >= (blocks - 2)) {
		drivers_log("ChainFS: format failed, file table too "
		    "large (%u blocks)\n", file_table_blocks);
		return (-1);
	}

	data_blocks = blocks - 1 - file_table_blocks;
	map_entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	block_map_blocks =
	    (data_blocks + map_entries_per_block - 1) /
	    map_entries_per_block;
	if (1 + file_table_blocks + block_map_blocks >= blocks) {
		drivers_log("ChainFS: format failed, no data area left\n");
		return (-1);
	}

	buffer = kmem_alloc(CHAINFS_BLOCK_SIZE);
	if (buffer == NULL) {
		return (-1);
	}

	drivers_log("ChainFS: formatting %s with %u blocks, %u max files\n",
	    disk->name, blocks, max_files);

	memset(&sb, 0, sizeof(sb));
	sb.magic = CHAINFS_MAGIC;
	sb.block_count = blocks;
	sb.file_table_block_count = file_table_blocks;
	sb.block_map_block_count = block_map_blocks;
	sb.total_files = max_files;
	sb.root_dir_block = 0;

	memset(buffer, 0, CHAINFS_BLOCK_SIZE);
	*((chainfs_superblock_t *)buffer) = sb;
	error = bio_write(disk, 0, 1, buffer);
	if (error != BIO_STATUS_OK) {
		goto fail;
	}

	memset(buffer, 0, CHAINFS_BLOCK_SIZE);
	entries = (chainfs_file_entry_t *)buffer;
	entries[0].status = 1;
	entries[0].type = CHAINFS_TYPE_DIR;
	entries[0].name[0] = '/';
	entries[0].name[1] = 0;
	entries[0].size = 0;
	entries[0].start_block = 0;
	entries[0].parent_block = 0xFFFFFFFF;
	entries[0].nlink = 1;
	error = bio_write(disk, 1, 1, buffer);
	if (error != BIO_STATUS_OK) {
		goto fail;
	}

	error = cfs_fill_range(disk, 2, file_table_blocks - 1, NULL);
	if (error != BIO_STATUS_OK) {
		goto fail;
	}

	memset(buffer, 0, CHAINFS_BLOCK_SIZE);
	block_map = (u32 *)buffer;
	for (i = 0; i < map_entries_per_block; i++) {
		block_map[i] = CHAINFS_FREE_BLOCK;
	}
	error = cfs_fill_range(disk, 1 + file_table_blocks, block_map_blocks,
	    buffer);
	if (error != BIO_STATUS_OK) {
		goto fail;
	}

	kmem_free(buffer);
	if ((disk->flags & DISK_F_NO_FLUSH) == 0) {
		(void)bio_flush(disk);
	}
	drivers_log("ChainFS: format of %s complete\n", disk->name);
	return (0);

fail:
	kmem_free(buffer);
	drivers_log("ChainFS: format of %s failed: %s\n", disk->name,
	    bio_status_name(error));
	return (-1);
}

int
chainfs_format(u32 total_blocks, u32 max_files)
{
	if (cfs->disk == NULL) {
		drivers_log("ChainFS: format failed, disk is NULL\n");
		return (-1);
	}
	if (chainfs_format_disk(cfs->disk, total_blocks, max_files) != 0) {
		return (-1);
	}
	return (chainfs_init(cfs->disk));
}

int
chainfs_find_file(const char *filename, chainfs_file_entry_t *entry,
    u32 *entry_block, u32 *entry_offset)
{
	if (!filename || filename[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strchr(filename, '/') != 0) {
		return (chainfs_resolve_path(filename, entry,
		    entry_block, entry_offset));
	}

	return (chainfs_find_in_directory(
	    cfs->current_dir_block, filename, entry,
	    entry_block, entry_offset));
}

int
chainfs_find_free_file_entry(u32 *entry_block, u32 *entry_offset)
{
	u32			entries_per_block, block, i;
	chainfs_file_entry_t	*entries;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

	for (block = 1;
	    block < 1 + cfs->superblock.file_table_block_count;
	    block++) {
		cfs_sector_read(block,
		    cfs->sector_buffer);
		entries = (chainfs_file_entry_t *)
		    cfs->sector_buffer;

		for (i = 0; i < entries_per_block; i++) {
			if (entries[i].status == 0) {
				*entry_block = block;
				*entry_offset = i;
				return (0);
			}
		}
	}

	return (-API_ERR_NO_SPACE);
}

int
chainfs_read_block_map_entry(u32 block_index, u32 *next_block)
{
	const u32	*map_entries;
	u32		entries_per_block, map_block, map_offset;
	int		ret;

	if (next_block == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	map_block = block_index / entries_per_block;
	map_offset = block_index % entries_per_block;

	ret = cfs_map_load(map_block);
	if (ret != 0) {
		return (ret);
	}

	map_entries = (const u32 *)cfs->map_buffer;
	*next_block = map_entries[map_offset];

	return (0);
}

int
chainfs_write_block_map_entry(u32 block_index, u32 next_block)
{
	u32	*map_entries;
	u32	entries_per_block, map_block, map_offset;
	int	ret;

	entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
	map_block = block_index / entries_per_block;
	map_offset = block_index % entries_per_block;

	ret = cfs_map_load(map_block);
	if (ret != 0) {
		return (ret);
	}

	map_entries = (u32 *)cfs->map_buffer;
	map_entries[map_offset] = next_block;
	cfs->map_dirty = 1;

	return (0);
}

int
chainfs_find_free_blocks(u32 count, u32 *blocks)
{
	u32	found, total_data_blocks;
	u32	i, next_block, scanned;

	if (blocks == NULL || count == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	found = 0;
	if (cfs->superblock.block_count <= cfs->data_area_start) {
		return (-API_ERR_NO_SPACE);
	}
	total_data_blocks = cfs->superblock.block_count -
	    cfs->data_area_start;
	if (cfs->alloc_hint >= total_data_blocks) {
		cfs->alloc_hint = 0;
	}

	scanned = 0;
	i = cfs->alloc_hint;
	while (scanned < total_data_blocks && found < count) {
		if (chainfs_read_block_map_entry(i, &next_block) == 0 &&
		    next_block == CHAINFS_FREE_BLOCK) {
			blocks[found++] = i;
		}
		scanned++;
		i++;
		if (i >= total_data_blocks) {
			i = 0;
		}
	}
	cfs->alloc_hint = i;

	return (found == count) ? 0 : -API_ERR_NO_SPACE;
}

void
chainfs_free_block_chain(u32 start_block)
{
	u32	current_block, next_block;

	current_block = start_block;
	cfs->seek_entry_block = CHAINFS_MAP_NONE;

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
	u32			real_sector, to_copy, next_block;
	u32			run, full, last_block;
	int			ret;

	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		drivers_log("ChainFS: File '%s' not found\n",
		    filename);
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}

	if (buffer == NULL || bytes_read == NULL) {
		return (-API_ERR_BAD_VALUE);
	}

	remaining = entry.size;
	if (remaining > buffer_size) {
		remaining = buffer_size;
	}
	copied = 0;
	current_block = entry.start_block;

	while (remaining > 0 && current_block != CHAINFS_EOF_MARKER) {
		run = 1;
		last_block = current_block;
		while ((u64)run * CHAINFS_BLOCK_SIZE < remaining) {
			if (chainfs_read_block_map_entry(last_block,
			    &next_block) != 0) {
				return (-API_ERR_IO);
			}
			if (next_block == CHAINFS_EOF_MARKER ||
			    next_block != last_block + 1) {
				break;
			}
			last_block = next_block;
			run++;
		}

		real_sector = cfs->data_area_start + current_block;
		full = remaining / CHAINFS_BLOCK_SIZE;
		if (full > run) {
			full = run;
		}
		if (full > 0) {
			if (cfs_sectors_read(real_sector, full,
			    buffer + copied) != BIO_STATUS_OK) {
				return (-API_ERR_IO);
			}
			copied += full * CHAINFS_BLOCK_SIZE;
			remaining -= full * CHAINFS_BLOCK_SIZE;
		}
		if (full < run && remaining > 0) {
			if (cfs_sector_read(real_sector + full,
			    cfs->sector_buffer) != BIO_STATUS_OK) {
				return (-API_ERR_IO);
			}
			to_copy = remaining;
			memcpy(buffer + copied, cfs->sector_buffer,
			    to_copy);
			copied += to_copy;
			remaining -= to_copy;
			full++;
		}
		if (remaining == 0) {
			break;
		}

		last_block = current_block + (full - 1);
		if (chainfs_read_block_map_entry(last_block,
		    &next_block) != 0) {
			return (-API_ERR_IO);
		}
		current_block = next_block;
	}

	*bytes_read = copied;
	/*drivers_log("ChainFS: Read %u bytes from '%s'\n",
	    copied, filename);*/
	return (0);
}

int
chainfs_truncate(const char *filename, u32 size)
{
	chainfs_file_entry_t	entry;
	chainfs_file_entry_t	*entries;
	u32			entry_block, entry_offset;
	u32			parent_block, blocks_needed;
	u32			*allocated_blocks;
	u32			i, next_block, name_len;
	char			file_name[32];
	int			file_exists, ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	if (filename == NULL) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = cfs_split_parent(filename, &parent_block, file_name);
	if (ret != 0) {
		return (ret);
	}

	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	file_exists = (ret == 0);
	if (file_exists) {
		if (entry.type == CHAINFS_TYPE_DIR) {
			return (-API_ERR_IS_DIR);
		}
		chainfs_free_block_chain(entry.start_block);
	} else {
		ret = chainfs_find_free_file_entry(&entry_block,
		    &entry_offset);
		if (ret != 0) {
			return (ret);
		}
	}

	blocks_needed = (size + CHAINFS_BLOCK_SIZE - 1) /
	    CHAINFS_BLOCK_SIZE;
	if (blocks_needed == 0) {
		blocks_needed = 1;
	}

	allocated_blocks = (u32 *)kmem_alloc(
	    (u64)blocks_needed * sizeof(u32));
	if (allocated_blocks == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	if (chainfs_find_free_blocks(blocks_needed,
	    allocated_blocks) != 0) {
		kmem_free(allocated_blocks);
		return (-API_ERR_NO_SPACE);
	}

	for (i = 0; i < blocks_needed; i++) {
		next_block = (i + 1 < blocks_needed) ?
		    allocated_blocks[i + 1] : CHAINFS_EOF_MARKER;
		if (chainfs_write_block_map_entry(allocated_blocks[i],
		    next_block) != 0) {
			kmem_free(allocated_blocks);
			return (-API_ERR_IO);
		}
	}

	if (cfs_sector_read(entry_block, cfs->sector_buffer) !=
	    BIO_STATUS_OK) {
		kmem_free(allocated_blocks);
		return (-API_ERR_IO);
	}
	entries = (chainfs_file_entry_t *)cfs->sector_buffer;
	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_FILE;
	for (i = 0; i < 30; i++) {
		entries[entry_offset].name[i] = 0;
	}
	name_len = strlen(file_name);
	for (i = 0; i < name_len; i++) {
		entries[entry_offset].name[i] = file_name[i];
	}
	entries[entry_offset].size = size;
	entries[entry_offset].start_block = allocated_blocks[0];
	entries[entry_offset].parent_block = parent_block;
	if (!file_exists) {
		entries[entry_offset].nlink = 1;
	}
	if (cfs_sector_write(entry_block, cfs->sector_buffer) !=
	    BIO_STATUS_OK) {
		kmem_free(allocated_blocks);
		return (-API_ERR_IO);
	}

	kmem_free(allocated_blocks);
	cfs->seek_entry_block = CHAINFS_MAP_NONE;
	(void)cfs_map_flush();
	return (0);
}

static int
cfs_seek_block(u32 entry_block, u32 entry_offset, u32 start_block,
    u32 block_index, u32 *out_block)
{
	u32	current, next, walked;

	if (out_block == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	current = start_block;
	walked = 0;
	if (cfs->seek_entry_block == entry_block &&
	    cfs->seek_entry_offset == entry_offset &&
	    cfs->seek_block_index <= block_index) {
		current = cfs->seek_block;
		walked = cfs->seek_block_index;
	}
	while (walked < block_index) {
		if (current == CHAINFS_EOF_MARKER) {
			return (-API_ERR_BAD_VALUE);
		}
		if (chainfs_read_block_map_entry(current, &next) != 0) {
			return (-API_ERR_IO);
		}
		current = next;
		walked++;
	}
	if (current == CHAINFS_EOF_MARKER) {
		return (-API_ERR_BAD_VALUE);
	}
	cfs->seek_entry_block = entry_block;
	cfs->seek_entry_offset = entry_offset;
	cfs->seek_block_index = walked;
	cfs->seek_block = current;
	*out_block = current;
	return (0);
}

int
chainfs_write_file_range(const char *filename, const u8 *data, u32 size,
    u32 offset)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			block_index, intra, current_block;
	u32			remaining, done, run, chunk, next_block;
	u32			real_sector, last_block;
	int			ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	if (filename == NULL || (data == NULL && size != 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	if (size == 0) {
		return (0);
	}

	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}
	if (offset >= entry.size || size > entry.size - offset) {
		return (-API_ERR_BAD_VALUE);
	}

	block_index = offset / CHAINFS_BLOCK_SIZE;
	intra = offset % CHAINFS_BLOCK_SIZE;
	ret = cfs_seek_block(entry_block, entry_offset, entry.start_block,
	    block_index, &current_block);
	if (ret != 0) {
		return (ret);
	}

	remaining = size;
	done = 0;

	if (intra != 0) {
		real_sector = cfs->data_area_start + current_block;
		if (cfs_sector_read(real_sector, cfs->sector_buffer) !=
		    BIO_STATUS_OK) {
			return (-API_ERR_IO);
		}
		chunk = CHAINFS_BLOCK_SIZE - intra;
		if (chunk > remaining) {
			chunk = remaining;
		}
		memcpy(cfs->sector_buffer + intra, data, chunk);
		if (cfs_sector_write(real_sector, cfs->sector_buffer) !=
		    BIO_STATUS_OK) {
			return (-API_ERR_IO);
		}
		done += chunk;
		remaining -= chunk;
		if (remaining == 0) {
			(void)cfs_map_flush();
			return (0);
		}
		if (chainfs_read_block_map_entry(current_block,
		    &next_block) != 0) {
			return (-API_ERR_IO);
		}
		current_block = next_block;
		block_index++;
		if (current_block == CHAINFS_EOF_MARKER) {
			return (-API_ERR_BAD_VALUE);
		}
		cfs->seek_block_index = block_index;
		cfs->seek_block = current_block;
	}

	while (remaining > 0) {
		run = 1;
		last_block = current_block;
		while ((u64)run * CHAINFS_BLOCK_SIZE < remaining) {
			if (chainfs_read_block_map_entry(last_block,
			    &next_block) != 0) {
				return (-API_ERR_IO);
			}
			if (next_block == CHAINFS_EOF_MARKER ||
			    next_block != last_block + 1) {
				break;
			}
			last_block = next_block;
			run++;
		}

		real_sector = cfs->data_area_start + current_block;
		chunk = remaining / CHAINFS_BLOCK_SIZE;
		if (chunk > run) {
			chunk = run;
		}
		if (chunk > 0) {
			if (cfs_sectors_write(real_sector, chunk,
			    data + done) != BIO_STATUS_OK) {
				return (-API_ERR_IO);
			}
			done += chunk * CHAINFS_BLOCK_SIZE;
			remaining -= chunk * CHAINFS_BLOCK_SIZE;
		}
		if (chunk < run && remaining > 0) {
			if (cfs_sector_read(real_sector + chunk,
			    cfs->sector_buffer) != BIO_STATUS_OK) {
				return (-API_ERR_IO);
			}
			memcpy(cfs->sector_buffer, data + done, remaining);
			if (cfs_sector_write(real_sector + chunk,
			    cfs->sector_buffer) != BIO_STATUS_OK) {
				return (-API_ERR_IO);
			}
			done += remaining;
			remaining = 0;
			chunk++;
		}

		block_index += chunk;
		last_block = current_block + (chunk - 1);
		if (remaining == 0) {
			cfs->seek_block_index = block_index - 1;
			cfs->seek_block = last_block;
			break;
		}
		if (chainfs_read_block_map_entry(last_block,
		    &next_block) != 0) {
			return (-API_ERR_IO);
		}
		if (next_block == CHAINFS_EOF_MARKER) {
			return (-API_ERR_BAD_VALUE);
		}
		current_block = next_block;
		cfs->seek_block_index = block_index;
		cfs->seek_block = current_block;
	}

	(void)cfs_map_flush();
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
	int			ret;

	if (bytes_read == NULL || buffer == NULL) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
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
			return (-API_ERR_IO);
		}
		if (next_block == CHAINFS_EOF_MARKER) {
			*bytes_read = 0;
			return (0);
		}
		current_block = next_block;
	}

	copied = 0;
	while (remaining > 0) {
		real_sector = cfs->data_area_start +
		    current_block;
		cfs_sector_read(real_sector,
		    cfs->sector_buffer);

		to_copy = CHAINFS_BLOCK_SIZE - intra_offset;
		if (to_copy > remaining) {
			to_copy = remaining;
		}

			memcpy(buffer + copied,
		    cfs->sector_buffer + intra_offset,
		    to_copy);

		copied += to_copy;
		remaining -= to_copy;
		intra_offset = 0;

		if (remaining == 0) {
			break;
		}

		if (chainfs_read_block_map_entry(current_block,
		    &next_block) != 0) {
			return (-API_ERR_IO);
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

static int
cfs_split_parent(const char *path, u32 *parent_block, char *leaf_out)
{
	chainfs_file_entry_t	parent_entry;
	char			parent_path[CHAINFS_MAX_PATH];
	const char		*leaf;
	u32			parent_entry_block, parent_entry_offset;
	int			path_len, last_slash, i;
	int			ret;

	if (path == NULL || parent_block == NULL || leaf_out == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	*parent_block = cfs->current_dir_block;
	leaf = path;

	if (strchr(path, '/') != 0) {
		path_len = strlen(path);
		if (path_len >= (int)sizeof(parent_path)) {
			return (-API_ERR_TOO_BIG);
		}
		last_slash = -1;
		if (path[0] == '/') {
			last_slash = 0;
		}
		for (i = path_len - 1; i > 0; i--) {
			if (path[i] == '/') {
				last_slash = i;
				break;
			}
		}
		if (last_slash == 0) {
			*parent_block = cfs->superblock.root_dir_block;
			leaf = path + 1;
		} else if (last_slash > 0) {
			for (i = 0; i < last_slash; i++) {
				parent_path[i] = path[i];
			}
			parent_path[last_slash] = 0;
			leaf = path + last_slash + 1;

			ret = chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset);
			if (ret != 0) {
				drivers_log("ChainFS: Parent directory "
				    "not found: %s\n", parent_path);
				return (ret);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				drivers_log("ChainFS: Parent is not "
				    "a directory: %s\n", parent_path);
				return (-API_ERR_NOT_DIR);
			}
			*parent_block = (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK + parent_entry_offset;
		}
	}
	if (leaf[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen(leaf) > 29) {
		return (-API_ERR_TOO_BIG);
	}
	strcpy(leaf_out, leaf);
	return (0);
}

int
chainfs_write_file(const char *filename, const u8 *data, u32 size,
    u32 alloc_size)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			parent_block, blocks_needed;
	u32			*allocated_blocks, remaining, data_offset;
	u32			real_sector, next_block, to_copy;
	u32			name_len, i, j, run, full;
	char			file_name[32];
	int			file_exists, ret;
	chainfs_file_entry_t	*entries;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: write failed, filesystem "
		    "not initialized\n");
		return (-API_ERR_IO);
	}
	if (!filename || (!data && size != 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	if (alloc_size < size) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	file_exists = (ret == 0);

	ret = cfs_split_parent(filename, &parent_block, file_name);
	if (ret != 0) {
		return (ret);
	}

	if (file_exists) {
		if (entry.type == CHAINFS_TYPE_DIR) {
			return (-API_ERR_IS_DIR);
		}
		chainfs_free_block_chain(entry.start_block);
		cfs->seek_entry_block = CHAINFS_MAP_NONE;
	} else {
		ret = chainfs_find_free_file_entry(&entry_block,
		    &entry_offset);
		if (ret == -API_ERR_IO) {
			drivers_log("ChainFS: Filesystem not "
			    "initialized!\n");
			return (ret);
		} else if (ret != 0) {
			drivers_log("ChainFS: No free file entries "
			    "(disk full or too many files)\n");
			return (ret);
		}
	}

	blocks_needed = (alloc_size + CHAINFS_BLOCK_SIZE - 1) /
	    CHAINFS_BLOCK_SIZE;
	if (blocks_needed == 0) {
		blocks_needed = 1;
	}

	allocated_blocks = (u32 *)kmem_alloc(
	    blocks_needed * sizeof(u32));
	if (!allocated_blocks) {
		drivers_log("ChainFS: Memory allocation failed\n");
		return (-API_ERR_NO_MEMORY);
	}

	if (chainfs_find_free_blocks(blocks_needed,
	    allocated_blocks) != 0) {
		drivers_log("ChainFS: Not enough free blocks\n");
		kmem_free(allocated_blocks);
		return (-API_ERR_NO_SPACE);
	}

	remaining = size;
	data_offset = 0;

	i = 0;
	while (i < blocks_needed) {
		run = 1;
		while (i + run < blocks_needed &&
		    allocated_blocks[i + run] ==
		    allocated_blocks[i + run - 1] + 1) {
			run++;
		}

		full = remaining / CHAINFS_BLOCK_SIZE;
		if (full > run) {
			full = run;
		}
		real_sector = cfs->data_area_start + allocated_blocks[i];
		if (full > 0) {
			if (cfs_sectors_write(real_sector, full,
			    data + data_offset) != BIO_STATUS_OK) {
				kmem_free(allocated_blocks);
				return (-API_ERR_IO);
			}
			data_offset += full * CHAINFS_BLOCK_SIZE;
			remaining -= full * CHAINFS_BLOCK_SIZE;
		}
		if (full < run) {
			for (j = 0; j < CHAINFS_BLOCK_SIZE; j++) {
				cfs->sector_buffer[j] = 0;
			}
			to_copy = remaining;
			if (to_copy > CHAINFS_BLOCK_SIZE) {
				to_copy = CHAINFS_BLOCK_SIZE;
			}
			if (to_copy > 0) {
				memcpy(cfs->sector_buffer,
				    data + data_offset, to_copy);
			}
			if (cfs_sector_write(real_sector + full,
			    cfs->sector_buffer) != BIO_STATUS_OK) {
				kmem_free(allocated_blocks);
				return (-API_ERR_IO);
			}
			data_offset += to_copy;
			remaining -= to_copy;
			full++;
		}

		for (j = 0; j < full; j++) {
			next_block = (i + j + 1 < blocks_needed) ?
			    allocated_blocks[i + j + 1] :
			    CHAINFS_EOF_MARKER;
			chainfs_write_block_map_entry(
			    allocated_blocks[i + j], next_block);
		}
		i += full;
	}
	if (cfs_map_flush() != 0) {
		kmem_free(allocated_blocks);
		return (-API_ERR_IO);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_FILE;
	for (i = 0; i < 30; i++) {
		entries[entry_offset].name[i] = 0;
	}

	name_len = strlen(file_name);
	for (i = 0; i < name_len; i++) {
		entries[entry_offset].name[i] = file_name[i];
	}

	entries[entry_offset].size = alloc_size;
	entries[entry_offset].start_block = allocated_blocks[0];
	entries[entry_offset].parent_block = parent_block;
	if (!file_exists) {
		entries[entry_offset].nlink = 1;
	}

	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	kmem_free(allocated_blocks);

	/*drivers_log("ChainFS: Wrote %u bytes to '%s' using "
	    "%u blocks\n", size, filename, blocks_needed);*/
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
	const char		*leaf;
	chainfs_file_entry_t	*entries;
	int			ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	if (!target || !linkpath) {
		return (-API_ERR_BAD_VALUE);
	}

	target_len = strlen(target);
	if (target_len == 0 || target_len >= 256) {
		return (-API_ERR_BAD_VALUE);
	}

	parent_block = cfs->current_dir_block;
	leaf = linkpath;

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
			    cfs->superblock.root_dir_block;
			leaf = linkpath + 1;
		} else {
			for (i = 0; i < last_slash; i++) {
				parent_path[i] = linkpath[i];
			}
			parent_path[last_slash] = '\0';
			leaf = linkpath + last_slash + 1;

			ret = chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset);
			if (ret != 0) {
				return (ret);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				return (-API_ERR_NOT_DIR);
			}
			parent_block =
			    (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK + parent_entry_offset;
		}
	}
	if (leaf[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen(leaf) > 29) {
		return (-API_ERR_TOO_BIG);
	}
	strcpy(file_name, leaf);
	if (chainfs_find_in_directory(parent_block, file_name,
	    &entry, &entry_block, &entry_offset) == 0) {
		return (-API_ERR_EXISTS);
	}

	ret = chainfs_find_free_file_entry(&entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

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
			cfs_sector_write(entry_block,
			    cfs->sector_buffer);
			return (-API_ERR_NO_MEMORY);
		}

		if (chainfs_find_free_blocks(blocks_needed,
		    allocated_blocks) != 0) {
			kmem_free(allocated_blocks);
			cfs_sector_write(entry_block,
			    cfs->sector_buffer);
			return (-API_ERR_NO_SPACE);
		}

		remaining = target_len;
		data_offset = 0;

		for (i = 0; i < blocks_needed; i++) {
			for (j = 0; j < CHAINFS_BLOCK_SIZE; j++) {
				cfs->sector_buffer[j] = 0;
			}

			to_copy = remaining;
			if (to_copy > CHAINFS_BLOCK_SIZE) {
				to_copy = CHAINFS_BLOCK_SIZE;
			}

			for (j = 0; j < to_copy; j++) {
				cfs->sector_buffer[j] =
				    (u8)target[data_offset + j];
			}

			real_sector = cfs->data_area_start +
			    allocated_blocks[i];
			cfs_sector_write(real_sector,
			    cfs->sector_buffer);

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

	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	(void)cfs_map_flush();
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
	const char		*leaf;
	chainfs_file_entry_t	*entries;
	int			ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	if (!oldpath || !newpath) {
		return (-API_ERR_BAD_VALUE);
	}

	ret = chainfs_find_file(oldpath, &old_entry, &old_block,
	    &old_offset);
	if (ret != 0) {
		return (ret);
	}

	if (old_entry.type == CHAINFS_TYPE_DIR) {
		return (-API_ERR_IS_DIR);
	}

	parent_block = cfs->current_dir_block;
	leaf = newpath;

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
			    cfs->superblock.root_dir_block;
			leaf = newpath + 1;
		} else {
			for (i = 0; i < last_slash; i++) {
				parent_path[i] = newpath[i];
			}
			parent_path[last_slash] = '\0';
			leaf = newpath + last_slash + 1;

			ret = chainfs_resolve_path(parent_path,
			    &parent_entry, &parent_entry_block,
			    &parent_entry_offset);
			if (ret != 0) {
				return (ret);
			}
			if (parent_entry.type != CHAINFS_TYPE_DIR) {
				return (-API_ERR_NOT_DIR);
			}
			parent_block =
			    (parent_entry_block - 1) *
			    ENTRIES_PER_BLOCK + parent_entry_offset;
		}
	}
	if (leaf[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen(leaf) > 29) {
		return (-API_ERR_TOO_BIG);
	}
	strcpy(file_name, leaf);
	if (chainfs_find_in_directory(parent_block, file_name,
	    &parent_entry, &parent_entry_block,
	    &parent_entry_offset) == 0) {
		return (-API_ERR_EXISTS);
	}

	ret = chainfs_find_free_file_entry(&entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}

	cfs_sector_read(old_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;
	entries[old_offset].nlink++;
	cfs_sector_write(old_block,
	    cfs->sector_buffer);

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

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

	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	(void)cfs_map_flush();
	return (0);
}

int
chainfs_readlink(const char *path, char *buf, u32 bufsize)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			to_read;
	u32			bytes_read;
	int			ret;

	ret = chainfs_find_file(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}

	if (entry.type != CHAINFS_TYPE_SYMLINK) {
		return (-API_ERR_BAD_VALUE);
	}

	if (bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
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
	ret = chainfs_read_file_range(path, (u8 *)buf, to_read,
	    0, &bytes_read);
	if (ret != 0) {
		return (ret);
	}

	buf[bytes_read] = '\0';
	return ((int)bytes_read);
}

int
chainfs_delete_file(const char *filename)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u32			start_block;
	chainfs_file_entry_t	*entries;
	int			ret;

	ret = chainfs_find_file(filename, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		drivers_log("ChainFS: File '%s' not found\n",
		    filename);
		return (ret);
	}
	if (entry.type == CHAINFS_TYPE_DIR) {
		drivers_log("ChainFS: '%s' is a directory\n",
		    filename);
		return (-API_ERR_IS_DIR);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

	if (entries[entry_offset].nlink > 1) {
		entries[entry_offset].nlink--;
		cfs_sector_write(entry_block,
		    cfs->sector_buffer);
	} else {
		start_block = entry.start_block;
		entries[entry_offset].status = 0;
		cfs_sector_write(entry_block,
		    cfs->sector_buffer);
		chainfs_free_block_chain(start_block);
	}

	drivers_log("ChainFS: Deleted file '%s'\n", filename);
	(void)cfs_map_flush();
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

	if (index >= cfs->superblock.total_files) {
		return (-1);
	}

	b = 1 + (index / ENTRIES_PER_BLOCK);
	o = index % ENTRIES_PER_BLOCK;

	cfs_sector_read(b, cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;
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

	if (!name || name[0] == '\0' || !entry || !entry_block ||
	    !entry_offset) {
		return (-API_ERR_BAD_VALUE);
	}

	entries_per_block =
	    CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

	for (block = 1;
	    block < 1 + cfs->superblock.file_table_block_count;
	    block++) {
		cfs_sector_read(block,
		    cfs->sector_buffer);
		entries = (chainfs_file_entry_t *)
		    cfs->sector_buffer;

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

	return (-API_ERR_NOT_FOUND);
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
	int			ret;

	if (!path || !entry || !entry_block || !entry_offset ||
	    path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	comp_count = split_path(path, components, 16);

	current_block = (path[0] == '/') ?
	    cfs->superblock.root_dir_block :
	    cfs->current_dir_block;

	if (comp_count == 0 && path[0] == '/') {
		root_idx = cfs->superblock.root_dir_block;
		root_block = 1 + (root_idx / ENTRIES_PER_BLOCK);
		root_offset = root_idx % ENTRIES_PER_BLOCK;
		cfs_sector_read(root_block,
		    cfs->sector_buffer);
		entries = (chainfs_file_entry_t *)
		    cfs->sector_buffer;
		*entry = entries[root_offset];
		*entry_block = root_block;
		*entry_offset = root_offset;
		return (0);
	}

	for (i = 0; i < comp_count; i++) {
		ret = chainfs_find_in_directory(current_block,
		    components[i], &found_entry, &found_block,
		    &found_offset);
		if (ret != 0) {
			return (ret);
		}

		if (i == comp_count - 1) {
			*entry = found_entry;
			*entry_block = found_block;
			*entry_offset = found_offset;
			return (0);
		} else {
			if (found_entry.type != CHAINFS_TYPE_DIR) {
				return (-API_ERR_NOT_DIR);
			}
			current_block = (found_block - 1) *
			    ENTRIES_PER_BLOCK + found_offset;
		}
	}

	return (-API_ERR_NOT_FOUND);
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
	const char		*leaf;
	chainfs_file_entry_t	*entries;
	int			ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		drivers_log("ChainFS: mkdir failed, filesystem "
		    "not initialized\n");
		return (-API_ERR_IO);
	}
	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	path_len = strlen(path);
	last_slash = -1;
	leaf = path;

	for (i = path_len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			last_slash = i;
			break;
		}
	}

	if (last_slash == -1) {
		parent_path[0] = 0;
		leaf = path;
	} else if (last_slash == 0) {
		parent_path[0] = '/';
		parent_path[1] = 0;
		leaf = path + 1;
	} else {
		for (i = 0; i < last_slash; i++) {
			parent_path[i] = path[i];
		}
		parent_path[last_slash] = 0;
		leaf = path + last_slash + 1;
	}

	if (leaf[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen(leaf) > 29) {
		return (-API_ERR_TOO_BIG);
	}
	strcpy(dir_name, leaf);

	if (parent_path[0] == 0) {
		parent_block = cfs->current_dir_block;
	} else {
		ret = chainfs_resolve_path(parent_path,
		    &parent_entry, &parent_block,
		    &parent_offset);
		if (ret != 0) {
			drivers_log("ChainFS: Parent directory "
			    "not found: %s\n", parent_path);
			return (ret);
		}
		if (parent_entry.type != CHAINFS_TYPE_DIR) {
			drivers_log("ChainFS: Parent is not "
			    "a directory: %s\n", parent_path);
			return (-API_ERR_NOT_DIR);
		}
		parent_block = (parent_block - 1) *
		    ENTRIES_PER_BLOCK + parent_offset;
	}

	if (chainfs_find_in_directory(parent_block, dir_name,
	    &existing_entry, &existing_block,
	    &existing_offset) == 0) {
		drivers_log("ChainFS: Directory already exists: "
		    "%s\n", path);
		return (-API_ERR_EXISTS);
	}
	ret = chainfs_find_free_file_entry(&entry_block,
	    &entry_offset);
	if (ret != 0) {
		drivers_log("ChainFS: No free file entries\n");
		return (ret);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_DIR;
	strcpy(entries[entry_offset].name, dir_name);
	entries[entry_offset].size = 0;
	entries[entry_offset].start_block = 0;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	drivers_log("ChainFS: Created directory: %s\n", path);
	(void)cfs_map_flush();
	return (0);
}

int
chainfs_chdir(const char *path)
{
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	int			ret;

	ret = chainfs_resolve_path(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		drivers_log("ChainFS: Directory not found: %s\n",
		    path);
		return (ret);
	}

	if (entry.type != CHAINFS_TYPE_DIR) {
		drivers_log("ChainFS: Not a directory: %s\n",
		    path);
		return (-API_ERR_NOT_DIR);
	}

	cfs->current_dir_block =
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
	return (chainfs_list_dir_range(path, 0, files, max_files,
	    file_count, NULL));
}

int
chainfs_list_dir_range(const char *path, u32 start,
    chainfs_file_entry_t *files, u32 max_files, u32 *file_count,
    u32 *total_count)
{
	chainfs_file_entry_t	dir_entry;
	chainfs_file_entry_t	sector_entries[ENTRIES_PER_BLOCK];
	u32			dir_block, dir_offset, entries_per_block;
	u32			found, seen, block, i;
	chainfs_file_entry_t	*entries;
	int			ret;

	if (!path || !file_count || (max_files != 0 && !files)) {
		return (-API_ERR_BAD_VALUE);
	}

	if (path[0] == 0) {
		dir_block = cfs->current_dir_block;
	} else {
		ret = chainfs_resolve_path(path, &dir_entry,
		    &dir_block, &dir_offset);
		if (ret != 0) {
			drivers_log("ChainFS: Directory not "
			    "found: %s\n", path);
			return (ret);
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
	seen = 0;
	if (max_files == 0 && total_count == NULL) {
		*file_count = 0;
		return (0);
	}

	for (block = 1;
	    block < 1 + cfs->superblock.file_table_block_count;
	    block++) {
		if (total_count == NULL && max_files != 0 &&
		    found >= max_files) {
			break;
		}

		cfs_sector_read(block, (u8 *)sector_entries);
		entries = sector_entries;

		for (i = 0; i < entries_per_block; i++) {
			if (entries[i].status == 1 &&
			    entries[i].parent_block == dir_block) {
				if (seen >= start && found < max_files) {
					files[found] = entries[i];
					found++;
				}
				seen++;
			}
		}
	}

	*file_count = found;
	if (total_count) {
		*total_count = seen;
	}
	return (0);
}

char *
chainfs_get_current_path(char *buffer, u32 buffer_size)
{
	char	temp_path[CHAINFS_MAX_PATH];
	u32	current_idx;
	chainfs_file_entry_t	entry;

	if (cfs->current_dir_block ==
	    cfs->superblock.root_dir_block) {
		if (buffer_size >= 2) {
			buffer[0] = '/';
			buffer[1] = 0;
			return (buffer);
		}
		return (0);
	}

	temp_path[0] = 0;
	current_idx = cfs->current_dir_block;

	while (current_idx !=
	    cfs->superblock.root_dir_block &&
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
	const char		*leaf;
	chainfs_file_entry_t	*entries;
	int			ret;

	if (cfs->superblock.magic != CHAINFS_MAGIC) {
		return (-API_ERR_IO);
	}
	if (!path || path[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}

	path_len = strlen(path);
	last_slash = -1;
	leaf = path;

	for (i = path_len - 1; i >= 0; i--) {
		if (path[i] == '/') {
			last_slash = i;
			break;
		}
	}

	if (last_slash == -1) {
		parent_path[0] = 0;
		leaf = path;
	} else if (last_slash == 0) {
		parent_path[0] = '/';
		parent_path[1] = 0;
		leaf = path + 1;
	} else {
		for (i = 0; i < last_slash; i++) {
			parent_path[i] = path[i];
		}
		parent_path[last_slash] = 0;
		leaf = path + last_slash + 1;
	}

	if (leaf[0] == '\0') {
		return (-API_ERR_BAD_VALUE);
	}
	if (strlen(leaf) > 29) {
		return (-API_ERR_TOO_BIG);
	}
	strcpy(sock_name, leaf);

	if (parent_path[0] == 0) {
		parent_block = cfs->current_dir_block;
	} else {
		ret = chainfs_resolve_path(parent_path, &parent_entry,
		    &parent_block, &parent_offset);
		if (ret != 0) {
			return (ret);
		}
		if (parent_entry.type != CHAINFS_TYPE_DIR) {
			return (-API_ERR_NOT_DIR);
		}
		parent_block = (parent_block - 1) *
		    ENTRIES_PER_BLOCK + parent_offset;
	}

	if (chainfs_find_in_directory(parent_block, sock_name,
	    &existing_entry, &existing_block, &existing_offset) == 0) {
		return (-API_ERR_EXISTS);
	}
	ret = chainfs_find_free_file_entry(&entry_block,
	    &entry_offset);
	if (ret != 0) {
		return (ret);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;

	entries[entry_offset].status = 1;
	entries[entry_offset].type = CHAINFS_TYPE_SOCK;
	strcpy(entries[entry_offset].name, sock_name);
	entries[entry_offset].size = 0;
	entries[entry_offset].start_block = 0;
	entries[entry_offset].parent_block = parent_block;
	entries[entry_offset].nlink = 1;

	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	(void)cfs_map_flush();
	return (0);
}

int
chainfs_rmdir(const char *path)
{
	chainfs_file_entry_t	entry, files[1];
	u32			entry_block, entry_offset, file_count;
	chainfs_file_entry_t	*entries;
	int			ret;

	ret = chainfs_resolve_path(path, &entry, &entry_block,
	    &entry_offset);
	if (ret != 0) {
		drivers_log("ChainFS: Directory not found: %s\n",
		    path);
		return (ret);
	}

	if (entry.type != CHAINFS_TYPE_DIR) {
		drivers_log("ChainFS: Not a directory: %s\n",
		    path);
		return (-API_ERR_NOT_DIR);
	}

	if ((entry_block - 1) * ENTRIES_PER_BLOCK + entry_offset ==
	    cfs->superblock.root_dir_block) {
		drivers_log("ChainFS: Cannot remove root "
		    "directory\n");
		return (-API_ERR_BUSY);
	}

	if (chainfs_list_dir(path, files, 1, &file_count) == 0 &&
	    file_count > 0) {
		drivers_log("ChainFS: Directory not empty: %s\n",
		    path);
		return (-API_ERR_NOT_EMPTY);
	}

	cfs_sector_read(entry_block,
	    cfs->sector_buffer);
	entries = (chainfs_file_entry_t *)
	    cfs->sector_buffer;
	entries[entry_offset].status = 0;
	cfs_sector_write(entry_block,
	    cfs->sector_buffer);

	drivers_log("ChainFS: Removed directory: %s\n", path);
	(void)cfs_map_flush();
	return (0);
}

static void
chainfs_root_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "chainfs_root", 0) == NULL) {
		device_add_child(parent, "chainfs_root", 0);
	}
}

static int
chainfs_root_probe(device_t dev)
{
	(void)dev;
	return (disk_count() > 0 ? 0 : -1);
}

static int
chainfs_live_boot(void)
{
	const newbus_bootinfo_t	*bi;
	void			*mod;
	u32			sz;

	bi = newbus_get_bootinfo();
	if (bi == NULL || bi->mb2 == NULL) {
		return (0);
	}
	mod = NULL;
	sz = 0;
	if (multiboot2_find_module((multiboot2_info_t *)bi->mb2,
	    CFS_LIVE_MODULE, &mod, &sz) != 0) {
		return (0);
	}
	return (mod != NULL && sz > 0);
}

static disk_t *
chainfs_pick_root(void)
{
	disk_t	*disk;
	disk_t	*fallback;
	u8	*probe;
	int	count;
	int	live;
	int	i;

	fallback = NULL;
	count = disk_count();
	live = chainfs_live_boot();
	probe = kmem_alloc(CHAINFS_BLOCK_SIZE);
	if (probe == NULL) {
		return (disk_get(0));
	}
	for (i = 0; i < count; i++) {
		disk = disk_get(i);
		if (disk == NULL || disk->sector_size != CHAINFS_BLOCK_SIZE) {
			continue;
		}
		if (disk->type == DISK_TYPE_RAM) {
			if (fallback == NULL) {
				fallback = disk;
			}
			continue;
		}
		if (live) {
			continue;
		}
		if (bio_read(disk, 0, 1, probe) != BIO_STATUS_OK) {
			continue;
		}
		if (chainfs_probe_sector(probe) != 0) {
			continue;
		}
		drivers_log("[CHAINFS] installed root found on %s\n",
		    disk->name);
		kmem_free(probe);
		return (disk);
	}
	kmem_free(probe);
	if (fallback != NULL) {
		return (fallback);
	}
	return (disk_get(0));
}

static int
chainfs_root_attach(device_t dev)
{
	const vfs_back_ops_t	*ops;
	disk_t			*disk;
	u64			format_blocks;
	int			ret;

	(void)dev;
	if (!chainfs_ctx_lock_ready) {
		mtx_init(&chainfs_ctx_lock, "chainfs_ctx", 0);
		chainfs_ctx_lock_ready = 1;
	}
	disk = chainfs_pick_root();
	if (disk == NULL) {
		return (-1);
	}
	ops = vfs_chainfs_back_ops();
	if (ops == NULL) {
		return (-1);
	}
	(void)vfs_back_register_ops(ops);
	ret = chainfs_init(disk);
	if (ret != 0) {
		if (disk->type != DISK_TYPE_RAM) {
			drivers_log("[CHAINFS] no filesystem on %s, refusing "
			    "to format a real disk\n", disk->name);
			return (-1);
		}
		format_blocks = 64;
		if (disk->total_sectors > 0) {
			format_blocks = disk->total_sectors;
		}
		drivers_log("[CHAINFS] init failed on %s, formatting ram "
		    "disk\n", disk->name);
		cfs->disk = disk;
		if (chainfs_format(format_blocks,
		    CHAINFS_BOOT_MAX_FILES) != 0) {
			return (-1);
		}
		ret = chainfs_init(disk);
		if (ret != 0) {
			return (-1);
		}
	}
	chainfs_root_is_ram = (disk->type == DISK_TYPE_RAM) ? 1 : 0;
	chainfs_root_dev = disk;
	if (ops->init != NULL && ops->init() != 0) {
		return (-1);
	}
	ret = vfs_back_mount("/", ops);
	if (ret == -API_ERR_EXISTS) {
		return (0);
	}
	return (ret);
}

int
chainfs_root_is_ramdisk(void)
{
	return (chainfs_root_is_ram);
}

disk_t *
chainfs_root_disk(void)
{
	return (chainfs_root_dev);
}

int
chainfs_ctx_enter(chainfs_t *ctx, disk_t *disk)
{
	if (ctx == NULL || disk == NULL || ctx == &g_chainfs) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!chainfs_ctx_lock_ready) {
		return (-API_ERR_NODEV);
	}
	mtx_lock(&chainfs_ctx_lock);
	if (cfs != &g_chainfs) {
		mtx_unlock(&chainfs_ctx_lock);
		return (-API_ERR_BUSY);
	}
	memset(ctx, 0, sizeof(*ctx));
	cfs = ctx;
	if (chainfs_init(disk) != 0) {
		cfs = &g_chainfs;
		mtx_unlock(&chainfs_ctx_lock);
		return (-API_ERR_IO);
	}
	return (0);
}

void
chainfs_ctx_leave(chainfs_t *ctx)
{
	if (cfs != ctx) {
		return;
	}
	(void)chainfs_sync();
	cfs = &g_chainfs;
	mtx_unlock(&chainfs_ctx_lock);
}

static devclass_t chainfs_root_devclass = {
	.name		= "chainfs",
	.maxunit	= 1,
};

static driver_t chainfs_root_driver = {
	.name		= "chainfs_root",
	.identify	= chainfs_root_identify,
	.probe		= chainfs_root_probe,
	.attach		= chainfs_root_attach,
};

PSEUDO_DRIVER_MODULE(chainfs_root, chainfs_root_driver,
    chainfs_root_devclass, NEWBUS_PASS_FILESYSTEM, NEWBUS_ORDER_MIDDLE);
