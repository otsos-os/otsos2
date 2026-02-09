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

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/mmu.h>

chainfs_t g_chainfs;
u64 g_chainfs_phys = 0;
#define ENTRIES_PER_BLOCK (CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t))

int chainfs_init(disk_t *disk) {
  g_chainfs.disk = disk;
  com1_printf("ChainFS: Initializing... (g_chainfs at %p, disk: %s)\n",
              &g_chainfs, disk ? disk->name : "NULL");

  disk_read(g_chainfs.disk, 0, g_chainfs.sector_buffer);

  chainfs_superblock_t *sb = (chainfs_superblock_t *)g_chainfs.sector_buffer;

  if (sb->magic != CHAINFS_MAGIC) {
    com1_printf("ChainFS: Invalid magic number 0x%x, expected 0x%x\n",
                sb->magic, CHAINFS_MAGIC);
    return -1;
  }

  g_chainfs.superblock = *sb;

  g_chainfs.data_area_start = 1 + g_chainfs.superblock.file_table_block_count +
                              g_chainfs.superblock.block_map_block_count;

  // Set current directory to root
  g_chainfs.current_dir_block = g_chainfs.superblock.root_dir_block;
  g_chainfs_phys = mmu_virt_to_phys((u64)&g_chainfs);
  com1_printf("[CHAINFS] g_chainfs phys=%p\n", (void *)g_chainfs_phys);

  com1_printf("ChainFS: Initialized successfully\n");
  com1_printf("  Total blocks: %u\n", g_chainfs.superblock.block_count);
  com1_printf("  File table blocks: %u\n",
              g_chainfs.superblock.file_table_block_count);
  com1_printf("  Block map blocks: %u\n",
              g_chainfs.superblock.block_map_block_count);
  com1_printf("  Data area start: %u\n", g_chainfs.data_area_start);
  com1_printf("  Root directory block: %u\n",
              g_chainfs.superblock.root_dir_block);

  return 0;
}

int chainfs_format(u32 total_blocks, u32 max_files) {
  com1_printf("ChainFS: Formatting disk with %u blocks, %u max files\n",
              total_blocks, max_files);

  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);
  u32 file_table_blocks =
      (max_files + entries_per_block - 1) / entries_per_block;

  u32 data_blocks = total_blocks - 1 - file_table_blocks;
  u32 map_entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
  u32 block_map_blocks =
      (data_blocks + map_entries_per_block - 1) / map_entries_per_block;

  data_blocks = total_blocks - 1 - file_table_blocks - block_map_blocks;

  chainfs_superblock_t sb = {0};
  sb.magic = CHAINFS_MAGIC;
  sb.block_count = total_blocks;
  sb.file_table_block_count = file_table_blocks;
  sb.block_map_block_count = block_map_blocks;
  sb.total_files = max_files;
  sb.root_dir_block = 0;

  for (int i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
    g_chainfs.sector_buffer[i] = 0;
  }
  *((chainfs_superblock_t *)g_chainfs.sector_buffer) = sb;
  disk_write(g_chainfs.disk, 0, g_chainfs.sector_buffer);

  // Clear file table
  for (int i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
    g_chainfs.sector_buffer[i] = 0;
  }

  // Create root directory entry in first file table block
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;
  entries[0].status = 1;
  entries[0].type = CHAINFS_TYPE_DIR;
  entries[0].name[0] = '/';
  entries[0].name[1] = 0;
  entries[0].size = 0;
  entries[0].start_block = 0; // Root directory doesn't need data blocks
  entries[0].parent_block = 0xFFFFFFFF; // Root has no parent

  disk_write(g_chainfs.disk, 1, g_chainfs.sector_buffer);

  // Clear remaining file table blocks
  for (int i = 0; i < CHAINFS_BLOCK_SIZE; i++) {
    g_chainfs.sector_buffer[i] = 0;
  }

  for (u32 block = 2; block < 1 + file_table_blocks; block++) {
    disk_write(g_chainfs.disk, block, g_chainfs.sector_buffer);
  }

  // Initialize block map
  u32 *block_map = (u32 *)g_chainfs.sector_buffer;
  for (u32 i = 0; i < map_entries_per_block; i++) {
    block_map[i] = CHAINFS_FREE_BLOCK;
  }

  for (u32 block = 1 + file_table_blocks;
       block < 1 + file_table_blocks + block_map_blocks; block++) {
    disk_write(g_chainfs.disk, block, g_chainfs.sector_buffer);
  }

  com1_printf("ChainFS: Format complete\n");

  return chainfs_init(g_chainfs.disk);
}

int chainfs_find_file(const char *filename, chainfs_file_entry_t *entry,
                      u32 *entry_block, u32 *entry_offset) {
  // If filename contains path separators, use path resolution
  if (strchr(filename, '/') != 0) {
    return chainfs_resolve_path(filename, entry, entry_block, entry_offset);
  }

  // Simple filename - search in current directory
  return chainfs_find_in_directory(g_chainfs.current_dir_block, filename, entry,
                                   entry_block, entry_offset);
}

int chainfs_find_free_file_entry(u32 *entry_block, u32 *entry_offset) {
  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    return -2; // Special error code for uninitialized FS
  }
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

  for (u32 block = 1; block < 1 + g_chainfs.superblock.file_table_block_count;
       block++) {
    disk_read(g_chainfs.disk, block, g_chainfs.sector_buffer);
    chainfs_file_entry_t *entries =
        (chainfs_file_entry_t *)g_chainfs.sector_buffer;

    for (u32 i = 0; i < entries_per_block; i++) {
      if (entries[i].status == 0) {
        *entry_block = block;
        *entry_offset = i;
        return 0;
      }
    }
  }

  return -1;
}

int chainfs_read_block_map_entry(u32 block_index, u32 *next_block) {
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
  u32 map_block = block_index / entries_per_block;
  u32 map_offset = block_index % entries_per_block;

  if (map_block >= g_chainfs.superblock.block_map_block_count) {
    return -1;
  }

  u32 sector = 1 + g_chainfs.superblock.file_table_block_count + map_block;
  disk_read(g_chainfs.disk, sector, g_chainfs.sector_buffer);

  u32 *map_entries = (u32 *)g_chainfs.sector_buffer;
  *next_block = map_entries[map_offset];

  return 0;
}

int chainfs_write_block_map_entry(u32 block_index, u32 next_block) {
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
  u32 map_block = block_index / entries_per_block;
  u32 map_offset = block_index % entries_per_block;

  if (map_block >= g_chainfs.superblock.block_map_block_count) {
    return -1;
  }

  u32 sector = 1 + g_chainfs.superblock.file_table_block_count + map_block;
  disk_read(g_chainfs.disk, sector, g_chainfs.sector_buffer);

  u32 *map_entries = (u32 *)g_chainfs.sector_buffer;
  map_entries[map_offset] = next_block;

  disk_write(g_chainfs.disk, sector, g_chainfs.sector_buffer);

  return 0;
}

int chainfs_find_free_blocks(u32 count, u32 *blocks) {
  u32 found = 0;
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(u32);
  u32 total_data_blocks =
      g_chainfs.superblock.block_count - g_chainfs.data_area_start;

  for (u32 i = 0; i < total_data_blocks && found < count; i++) {
    u32 next_block;
    if (chainfs_read_block_map_entry(i, &next_block) == 0) {
      if (next_block == CHAINFS_FREE_BLOCK) {
        blocks[found++] = i;
      }
    }
  }

  return (found == count) ? 0 : -1;
}

void chainfs_free_block_chain(u32 start_block) {
  u32 current_block = start_block;

  while (current_block != CHAINFS_EOF_MARKER) {
    u32 next_block;
    if (chainfs_read_block_map_entry(current_block, &next_block) != 0) {
      break;
    }

    chainfs_write_block_map_entry(current_block, CHAINFS_FREE_BLOCK);
    current_block = next_block;
  }
}

int chainfs_read_file(const char *filename, u8 *buffer, u32 buffer_size,
                      u32 *bytes_read) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;

  if (chainfs_find_file(filename, &entry, &entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: File '%s' not found\n", filename);
    return -1;
  }

  u32 remaining = entry.size;
  u32 copied = 0;
  u32 current_block = entry.start_block;

  while (remaining > 0 && current_block != CHAINFS_EOF_MARKER &&
         copied < buffer_size) {
    u32 real_sector = g_chainfs.data_area_start + current_block;
    disk_read(g_chainfs.disk, real_sector, g_chainfs.sector_buffer);

    u32 to_copy = remaining;
    if (to_copy > CHAINFS_BLOCK_SIZE) {
      to_copy = CHAINFS_BLOCK_SIZE;
    }
    if (copied + to_copy > buffer_size) {
      to_copy = buffer_size - copied;
    }

    for (u32 i = 0; i < to_copy; i++) {
      buffer[copied + i] = g_chainfs.sector_buffer[i];
    }

    copied += to_copy;
    remaining -= to_copy;

    if (remaining > 0) {
      u32 next_block;
      if (chainfs_read_block_map_entry(current_block, &next_block) != 0) {
        break;
      }
      current_block = next_block;
    }
  }

  *bytes_read = copied;
  com1_printf("ChainFS: Read %u bytes from '%s'\n", copied, filename);
  return 0;
}

int chainfs_read_file_range(const char *filename, u8 *buffer, u32 buffer_size,
                            u32 offset, u32 *bytes_read) {
  if (bytes_read == NULL || buffer == NULL) {
    return -1;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(filename, &entry, &entry_block, &entry_offset) != 0) {
    return -1;
  }

  if (offset >= entry.size) {
    *bytes_read = 0;
    return 0;
  }

  u32 remaining = entry.size - offset;
  if (remaining > buffer_size) {
    remaining = buffer_size;
  }

  u32 block_skip = offset / CHAINFS_BLOCK_SIZE;
  u32 intra_offset = offset % CHAINFS_BLOCK_SIZE;
  u32 current_block = entry.start_block;

  for (u32 i = 0; i < block_skip; i++) {
    u32 next_block;
    if (chainfs_read_block_map_entry(current_block, &next_block) != 0) {
      return -1;
    }
    if (next_block == CHAINFS_EOF_MARKER) {
      *bytes_read = 0;
      return 0;
    }
    current_block = next_block;
  }

  u32 copied = 0;
  while (remaining > 0) {
    u32 real_sector = g_chainfs.data_area_start + current_block;
    disk_read(g_chainfs.disk, real_sector, g_chainfs.sector_buffer);

    u32 to_copy = CHAINFS_BLOCK_SIZE - intra_offset;
    if (to_copy > remaining) {
      to_copy = remaining;
    }

    for (u32 i = 0; i < to_copy; i++) {
      buffer[copied + i] = g_chainfs.sector_buffer[intra_offset + i];
    }

    copied += to_copy;
    remaining -= to_copy;
    intra_offset = 0;

    if (remaining == 0) {
      break;
    }

    u32 next_block;
    if (chainfs_read_block_map_entry(current_block, &next_block) != 0) {
      return -1;
    }
    if (next_block == CHAINFS_EOF_MARKER) {
      break;
    }
    current_block = next_block;
  }

  *bytes_read = copied;
  com1_printf("ChainFS: Read %u bytes from '%s' (offset %u)\n", copied,
              filename, offset);
  return 0;
}

int chainfs_write_file(const char *filename, const u8 *data, u32 size) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  int file_exists =
      (chainfs_find_file(filename, &entry, &entry_block, &entry_offset) == 0);

  u32 parent_block = g_chainfs.current_dir_block;
  char file_name[32];

  // Extract directory and filename
  if (strchr(filename, '/') != 0) {
    // Path contains directories
    char parent_path[CHAINFS_MAX_PATH];
    int path_len = strlen(filename);
    int last_slash = -1;

    for (int i = path_len - 1; i >= 0; i--) {
      if (filename[i] == '/') {
        last_slash = i;
        break;
      }
    }

    if (last_slash == -1) {
      strcpy(file_name, filename);
    } else if (last_slash == 0) {
      parent_block = g_chainfs.superblock.root_dir_block;
      strcpy(file_name, filename + 1);
    } else {
      for (int i = 0; i < last_slash; i++) {
        parent_path[i] = filename[i];
      }
      parent_path[last_slash] = 0;
      strcpy(file_name, filename + last_slash + 1);

      chainfs_file_entry_t parent_entry;
      u32 parent_entry_block, parent_entry_offset;
      if (chainfs_resolve_path(parent_path, &parent_entry, &parent_entry_block,
                               &parent_entry_offset) != 0) {
        com1_printf("ChainFS: Parent directory not found: %s\n", parent_path);
        return -1;
      }
      if (parent_entry.type != CHAINFS_TYPE_DIR) {
        com1_printf("ChainFS: Parent is not a directory: %s\n", parent_path);
        return -1;
      }
      parent_block = (parent_entry_block - 1) * ENTRIES_PER_BLOCK +
                     parent_entry_offset;
    }
  } else {
    strcpy(file_name, filename);
  }

  if (file_exists) {
    chainfs_free_block_chain(entry.start_block);
  } else {
    int res = chainfs_find_free_file_entry(&entry_block, &entry_offset);
    if (res == -2) {
      com1_printf("ChainFS: Filesystem not initialized! Please run 'format'\n");
      return -2;
    } else if (res != 0) {
      com1_printf(
          "ChainFS: No free file entries (disk full or too many files)\n");
      return -1;
    }
  }

  u32 blocks_needed = (size + CHAINFS_BLOCK_SIZE - 1) / CHAINFS_BLOCK_SIZE;
  if (blocks_needed == 0) {
    blocks_needed = 1;
  }

  u32 *allocated_blocks = (u32 *)kmalloc(blocks_needed * sizeof(u32));
  if (!allocated_blocks) {
    com1_printf("ChainFS: Memory allocation failed\n");
    return -1;
  }

  if (chainfs_find_free_blocks(blocks_needed, allocated_blocks) != 0) {
    com1_printf("ChainFS: Not enough free blocks\n");
    kfree(allocated_blocks);
    return -1;
  }

  u32 remaining = size;
  u32 data_offset = 0;

  for (u32 i = 0; i < blocks_needed; i++) {
    for (int j = 0; j < CHAINFS_BLOCK_SIZE; j++) {
      g_chainfs.sector_buffer[j] = 0;
    }

    u32 to_copy = remaining;
    if (to_copy > CHAINFS_BLOCK_SIZE) {
      to_copy = CHAINFS_BLOCK_SIZE;
    }

    for (u32 j = 0; j < to_copy; j++) {
      g_chainfs.sector_buffer[j] = data[data_offset + j];
    }

    u32 real_sector = g_chainfs.data_area_start + allocated_blocks[i];
    disk_write(g_chainfs.disk, real_sector, g_chainfs.sector_buffer);

    u32 next_block =
        (i + 1 < blocks_needed) ? allocated_blocks[i + 1] : CHAINFS_EOF_MARKER;
    chainfs_write_block_map_entry(allocated_blocks[i], next_block);

    remaining -= to_copy;
    data_offset += to_copy;
  }

  disk_read(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;

  entries[entry_offset].status = 1;
  entries[entry_offset].type = CHAINFS_TYPE_FILE;
  for (int i = 0; i < 30; i++) {
    entries[entry_offset].name[i] = 0;
  }

  int name_len = strlen(file_name);
  if (name_len > 29) {
    name_len = 29;
  }
  for (int i = 0; i < name_len; i++) {
    entries[entry_offset].name[i] = file_name[i];
  }

  entries[entry_offset].size = size;
  entries[entry_offset].start_block = allocated_blocks[0];
  entries[entry_offset].parent_block = parent_block;

  disk_write(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);

  kfree(allocated_blocks);

  com1_printf("ChainFS: Wrote %u bytes to '%s' using %u blocks\n", size,
              filename, blocks_needed);
  return 0;
}

int chainfs_delete_file(const char *filename) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;

  if (chainfs_find_file(filename, &entry, &entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: File '%s' not found\n", filename);
    return -1;
  }

  chainfs_free_block_chain(entry.start_block);

  disk_read(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;
  entries[entry_offset].status = 0;
  disk_write(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);

  com1_printf("ChainFS: Deleted file '%s'\n", filename);
  return 0;
}

int chainfs_get_file_list(chainfs_file_entry_t *files, u32 max_files,
                          u32 *file_count) {
  return chainfs_list_dir("", files, max_files, file_count);
}

static int read_entry_by_index(u32 index, chainfs_file_entry_t *entry,
                               u32 *block, u32 *offset) {
  if (index >= g_chainfs.superblock.total_files)
    return -1;

  u32 b = 1 + (index / ENTRIES_PER_BLOCK);
  u32 o = index % ENTRIES_PER_BLOCK;

  disk_read(g_chainfs.disk, b, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;
  *entry = entries[o];
  if (block)
    *block = b;
  if (offset)
    *offset = o;
  return 0;
}

// Helper function to split path into components
static int split_path(const char *path, char components[][32],
                      int max_components) {
  int count = 0;
  int start = 0;
  int len = strlen(path);

  // Skip leading slash
  if (len > 0 && path[0] == '/') {
    start = 1;
  }

  for (int i = start; i <= len && count < max_components; i++) {
    if (path[i] == '/' || path[i] == 0) {
      int comp_len = i - start;
      if (comp_len > 0 && comp_len < 31) {
        for (int j = 0; j < comp_len; j++) {
          components[count][j] = path[start + j];
        }
        components[count][comp_len] = 0;
        count++;
      }
      start = i + 1;
    }
  }

  return count;
}

// Find entry in specific directory
int chainfs_find_in_directory(u32 dir_block, const char *name,
                              chainfs_file_entry_t *entry, u32 *entry_block,
                              u32 *entry_offset) {
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);

  for (u32 block = 1; block < 1 + g_chainfs.superblock.file_table_block_count;
       block++) {
    disk_read(g_chainfs.disk, block, g_chainfs.sector_buffer);
    chainfs_file_entry_t *entries =
        (chainfs_file_entry_t *)g_chainfs.sector_buffer;

    for (u32 i = 0; i < entries_per_block; i++) {
      if (entries[i].status == 1 && entries[i].parent_block == dir_block &&
          strcmp(entries[i].name, name) == 0) {
        *entry = entries[i];
        *entry_block = block;
        *entry_offset = i;
        return 0;
      }
    }
  }

  return -1;
}

// Resolve full path to file/directory entry
int chainfs_resolve_path(const char *path, chainfs_file_entry_t *entry,
                         u32 *entry_block, u32 *entry_offset) {
  char components[16][32];
  int comp_count = split_path(path, components, 16);

  // Start from root or current directory
  u32 current_block = (path[0] == '/') ? g_chainfs.superblock.root_dir_block
                                       : g_chainfs.current_dir_block;

  // If path is just "/" return root directory
  if (comp_count == 0 && path[0] == '/') {
    u32 root_idx = g_chainfs.superblock.root_dir_block;
    u32 root_block = 1 + (root_idx / ENTRIES_PER_BLOCK);
    u32 root_offset = root_idx % ENTRIES_PER_BLOCK;
    disk_read(g_chainfs.disk, root_block, g_chainfs.sector_buffer);
    chainfs_file_entry_t *entries =
        (chainfs_file_entry_t *)g_chainfs.sector_buffer;
    *entry = entries[root_offset];
    *entry_block = root_block;
    *entry_offset = root_offset;
    return 0;
  }

  // Traverse path components
  for (int i = 0; i < comp_count; i++) {
    chainfs_file_entry_t found_entry;
    u32 found_block, found_offset;

    if (chainfs_find_in_directory(current_block, components[i], &found_entry,
                                  &found_block, &found_offset) != 0) {
      return -1; // Component not found
    }

    if (i == comp_count - 1) {
      // Last component - return it
      *entry = found_entry;
      *entry_block = found_block;
      *entry_offset = found_offset;
      return 0;
    } else {
      // Intermediate component - must be directory
      if (found_entry.type != CHAINFS_TYPE_DIR) {
        return -1; // Not a directory
      }
      current_block = (found_block - 1) * ENTRIES_PER_BLOCK + found_offset;
    }
  }

  return -1;
}

// Create directory
int chainfs_mkdir(const char *path) {
  // Find parent directory
  char parent_path[CHAINFS_MAX_PATH];
  char dir_name[32];

  // Extract parent path and directory name
  int path_len = strlen(path);
  int last_slash = -1;

  for (int i = path_len - 1; i >= 0; i--) {
    if (path[i] == '/') {
      last_slash = i;
      break;
    }
  }

  if (last_slash == -1) {
    // No slash - create in current directory
    parent_path[0] = 0;
    strcpy(dir_name, path);
  } else if (last_slash == 0) {
    // Root directory
    parent_path[0] = '/';
    parent_path[1] = 0;
    strcpy(dir_name, path + 1);
  } else {
    // Copy parent path
    for (int i = 0; i < last_slash; i++) {
      parent_path[i] = path[i];
    }
    parent_path[last_slash] = 0;
    strcpy(dir_name, path + last_slash + 1);
  }

  // Find parent directory
  chainfs_file_entry_t parent_entry;
  u32 parent_block, parent_offset;

  if (parent_path[0] == 0) {
    // Use current directory
    parent_block = g_chainfs.current_dir_block;
  } else {
    if (chainfs_resolve_path(parent_path, &parent_entry, &parent_block,
                             &parent_offset) != 0) {
      com1_printf("ChainFS: Parent directory not found: %s\n", parent_path);
      return -1;
    }
    if (parent_entry.type != CHAINFS_TYPE_DIR) {
      com1_printf("ChainFS: Parent is not a directory: %s\n", parent_path);
      return -1;
    }
    parent_block =
        (parent_block - 1) * ENTRIES_PER_BLOCK + parent_offset;
  }

  // Check if directory already exists
  chainfs_file_entry_t existing_entry;
  u32 existing_block, existing_offset;
  if (chainfs_find_in_directory(parent_block, dir_name, &existing_entry,
                                &existing_block, &existing_offset) == 0) {
    com1_printf("ChainFS: Directory already exists: %s\n", path);
    return -1;
  }

  // Find free file entry
  u32 entry_block, entry_offset;
  if (chainfs_find_free_file_entry(&entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: No free file entries\n");
    return -1;
  }

  // Create directory entry
  disk_read(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;

  entries[entry_offset].status = 1;
  entries[entry_offset].type = CHAINFS_TYPE_DIR;
  strcpy(entries[entry_offset].name, dir_name);
  entries[entry_offset].size = 0;
  entries[entry_offset].start_block = 0; // Directories don't need data blocks
  entries[entry_offset].parent_block = parent_block;

  disk_write(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);

  com1_printf("ChainFS: Created directory: %s\n", path);
  return 0;
}

int chainfs_mknod(const char *path, u16 major, u16 minor) {
  char parent_path[CHAINFS_MAX_PATH];
  char node_name[32];

  int path_len = strlen(path);
  int last_slash = -1;

  for (int i = path_len - 1; i >= 0; i--) {
    if (path[i] == '/') {
      last_slash = i;
      break;
    }
  }

  if (last_slash == -1) {
    parent_path[0] = 0;
    strcpy(node_name, path);
  } else if (last_slash == 0) {
    parent_path[0] = '/';
    parent_path[1] = 0;
    strcpy(node_name, path + 1);
  } else {
    for (int i = 0; i < last_slash; i++) {
      parent_path[i] = path[i];
    }
    parent_path[last_slash] = 0;
    strcpy(node_name, path + last_slash + 1);
  }

  chainfs_file_entry_t parent_entry;
  u32 parent_block, parent_offset;

  if (parent_path[0] == 0) {
    parent_block = g_chainfs.current_dir_block;
  } else {
    if (chainfs_resolve_path(parent_path, &parent_entry, &parent_block,
                             &parent_offset) != 0) {
      com1_printf("ChainFS: Parent directory not found: %s\n", parent_path);
      return -1;
    }
    if (parent_entry.type != CHAINFS_TYPE_DIR) {
      com1_printf("ChainFS: Parent is not a directory: %s\n", parent_path);
      return -1;
    }
    parent_block =
        (parent_block - 1) * ENTRIES_PER_BLOCK + parent_offset;
  }

  chainfs_file_entry_t existing_entry;
  u32 existing_block, existing_offset;
  if (chainfs_find_in_directory(parent_block, node_name, &existing_entry,
                                &existing_block, &existing_offset) == 0) {
    com1_printf("ChainFS: Node already exists: %s\n", path);
    return -1;
  }

  u32 entry_block, entry_offset;
  if (chainfs_find_free_file_entry(&entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: No free file entries\n");
    return -1;
  }

  disk_read(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;

  entries[entry_offset].status = 1;
  entries[entry_offset].type = CHAINFS_TYPE_DEV;
  strcpy(entries[entry_offset].name, node_name);
  entries[entry_offset].size = 0;
  entries[entry_offset].start_block = CHAINFS_EOF_MARKER;
  entries[entry_offset].parent_block = parent_block;
  memset(entries[entry_offset].reserved, 0,
         sizeof(entries[entry_offset].reserved));
  entries[entry_offset].reserved[0] = (u8)(major & 0xFF);
  entries[entry_offset].reserved[1] = (u8)((major >> 8) & 0xFF);
  entries[entry_offset].reserved[2] = (u8)(minor & 0xFF);
  entries[entry_offset].reserved[3] = (u8)((minor >> 8) & 0xFF);

  disk_write(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);

  com1_printf("ChainFS: Created device node: %s (%u:%u)\n", path, major, minor);
  return 0;
}

// Change directory
int chainfs_chdir(const char *path) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;

  if (chainfs_resolve_path(path, &entry, &entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: Directory not found: %s\n", path);
    return -1;
  }

  if (entry.type != CHAINFS_TYPE_DIR) {
    com1_printf("ChainFS: Not a directory: %s\n", path);
    return -1;
  }

  g_chainfs.current_dir_block =
      (entry_block - 1) * ENTRIES_PER_BLOCK + entry_offset;
  com1_printf("ChainFS: Changed directory to: %s\n", path);
  return 0;
}

// List directory contents
int chainfs_list_dir(const char *path, chainfs_file_entry_t *files,
                     u32 max_files, u32 *file_count) {
  chainfs_file_entry_t dir_entry;
  u32 dir_block, dir_offset;

  // If path is empty, use current directory
  if (path[0] == 0) {
    dir_block = g_chainfs.current_dir_block;
  } else {
    if (chainfs_resolve_path(path, &dir_entry, &dir_block, &dir_offset) != 0) {
      com1_printf("ChainFS: Directory not found: %s\n", path);
      return -1;
    }

    if (dir_entry.type != CHAINFS_TYPE_DIR) {
      com1_printf("ChainFS: Not a directory: %s\n", path);
      return -1;
    }
    dir_block = (dir_block - 1) * ENTRIES_PER_BLOCK + dir_offset;
  }

  // Find all entries with this directory as parent
  u32 entries_per_block = CHAINFS_BLOCK_SIZE / sizeof(chainfs_file_entry_t);
  u32 found = 0;

  for (u32 block = 1; block < 1 + g_chainfs.superblock.file_table_block_count &&
                      found < max_files;
       block++) {
    disk_read(g_chainfs.disk, block, g_chainfs.sector_buffer);
    chainfs_file_entry_t *entries =
        (chainfs_file_entry_t *)g_chainfs.sector_buffer;

    for (u32 i = 0; i < entries_per_block && found < max_files; i++) {
      if (entries[i].status == 1 && entries[i].parent_block == dir_block) {
        files[found] = entries[i];
        found++;
      }
    }
  }

  *file_count = found;
  return 0;
}

// Get current path as string
char *chainfs_get_current_path(char *buffer, u32 buffer_size) {
  if (g_chainfs.current_dir_block == g_chainfs.superblock.root_dir_block) {
    if (buffer_size >= 2) {
      buffer[0] = '/';
      buffer[1] = 0;
      return buffer;
    }
    return 0;
  }

  // Build path by traversing up to root
  char temp_path[CHAINFS_MAX_PATH];
  temp_path[0] = 0;

  u32 current_idx = g_chainfs.current_dir_block;

  while (current_idx != g_chainfs.superblock.root_dir_block &&
         current_idx != 0xFFFFFFFF) {
    chainfs_file_entry_t entry;
    if (read_entry_by_index(current_idx, &entry, NULL, NULL) != 0)
      break;

    char new_name[CHAINFS_MAX_PATH];
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
    return buffer;
  }

  return 0;
}

// Remove directory (must be empty)
int chainfs_rmdir(const char *path) {
  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;

  if (chainfs_resolve_path(path, &entry, &entry_block, &entry_offset) != 0) {
    com1_printf("ChainFS: Directory not found: %s\n", path);
    return -1;
  }

  if (entry.type != CHAINFS_TYPE_DIR) {
    com1_printf("ChainFS: Not a directory: %s\n", path);
    return -1;
  }

  if (entry_block == g_chainfs.superblock.root_dir_block) {
    com1_printf("ChainFS: Cannot remove root directory\n");
    return -1;
  }

  // Check if directory is empty
  chainfs_file_entry_t files[1];
  u32 file_count;
  if (chainfs_list_dir(path, files, 1, &file_count) == 0 && file_count > 0) {
    com1_printf("ChainFS: Directory not empty: %s\n", path);
    return -1;
  }

  // Remove directory entry
  disk_read(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);
  chainfs_file_entry_t *entries =
      (chainfs_file_entry_t *)g_chainfs.sector_buffer;
  entries[entry_offset].status = 0;
  disk_write(g_chainfs.disk, entry_block, g_chainfs.sector_buffer);

  com1_printf("ChainFS: Removed directory: %s\n", path);
  return 0;
}
