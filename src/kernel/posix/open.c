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
#include <kernel/posix/posix.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

static int posix_find_free_fd(void) {
  for (int i = 3; i < MAX_FDS; i++) {
    if (!fd_table[i].used) {
      return i;
    }
  }
  return -1;
}

static int posix_flags_valid(int flags) {
  if ((flags & O_RDWR) == 0) {
    return 0;
  }

  if ((flags & (O_CREAT | O_TRUNC | O_APPEND)) && !(flags & O_WRONLY)) {
    return 0;
  }

  return 1;
}

int sys_open(const char *path, int flags) {
  if (path == NULL || path[0] == 0) {
    return -1;
  }

  if (!posix_flags_valid(flags)) {
    return -1;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("POSIX OPEN: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    return -1;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  int exists =
      (chainfs_find_file(path, &entry, &entry_block, &entry_offset) == 0);

  if (!exists) {
    if (!(flags & O_CREAT)) {
      return -1;
    }
    if (chainfs_write_file(path, (const u8 *)"", 0) != 0) {
      return -1;
    }
    exists = (chainfs_find_file(path, &entry, &entry_block, &entry_offset) == 0);
    if (!exists) {
      return -1;
    }
  } else if (flags & O_TRUNC) {
    if (chainfs_write_file(path, (const u8 *)"", 0) != 0) {
      return -1;
    }
    if (chainfs_find_file(path, &entry, &entry_block, &entry_offset) != 0) {
      return -1;
    }
  }

  int fd = posix_find_free_fd();
  if (fd < 0) {
    return -1;
  }

  fd_table[fd].used = 1;
  fd_table[fd].flags = flags;
  fd_table[fd].offset = (flags & O_APPEND) ? entry.size : 0;

  memset(fd_table[fd].path, 0, sizeof(fd_table[fd].path));
  int path_len = strlen(path);
  if (path_len >= (int)sizeof(fd_table[fd].path)) {
    path_len = (int)sizeof(fd_table[fd].path) - 1;
  }
  memcpy(fd_table[fd].path, path, path_len);

  return fd;
}
