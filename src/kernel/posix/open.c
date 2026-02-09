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
#include <kernel/drivers/tty.h>
#include <kernel/posix/posix.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

static char *copy_user_path(const char *path) {
  if (!path) {
    return NULL;
  }
  if (!is_user_address(path, 1)) {
    return NULL;
  }
  int len = 0;
  while (len < 255) {
    if (!is_user_address(path + len, 1)) {
      return NULL;
    }
    if (path[len] == '\0') {
      break;
    }
    len++;
  }
  if (len == 255) {
    return NULL;
  }
  char *buf = (char *)kcalloc(len + 1, 1);
  if (!buf) {
    return NULL;
  }
  memcpy(buf, path, len);
  buf[len] = '\0';
  return buf;
}

static int posix_find_free_fd(void) {
  file_descriptor_t *fd_table = posix_get_fd_table();
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

static int posix_open_device(const char *kpath, int flags,
                             const chainfs_file_entry_t *entry) {
  u16 major = (u16)entry->reserved[0] | ((u16)entry->reserved[1] << 8);
  u16 minor = (u16)entry->reserved[2] | ((u16)entry->reserved[3] << 8);

  int of_type = -1;
  if (major == TTY_DEVICE_MAJOR) {
    (void)minor;
    of_type = OFT_TYPE_TTY;
  }

  if (of_type < 0) {
    return -1;
  }

  int fd = posix_find_free_fd();
  if (fd < 0) {
    return -1;
  }

  int of_index = posix_alloc_open_file();
  if (of_index < 0) {
    return -1;
  }

  open_file_t *oft = posix_get_open_file_table();
  oft[of_index].type = of_type;
  oft[of_index].flags = flags;
  oft[of_index].offset = 0;
  memset(oft[of_index].path, 0, sizeof(oft[of_index].path));
  int path_len = strlen(kpath);
  if (path_len >= (int)sizeof(oft[of_index].path)) {
    path_len = (int)sizeof(oft[of_index].path) - 1;
  }
  memcpy(oft[of_index].path, kpath, path_len);

  file_descriptor_t *fd_table = posix_get_fd_table();
  fd_table[fd].used = 1;
  fd_table[fd].flags = flags;
  fd_table[fd].of_index = of_index;

  return fd;
}

int sys_open(const char *path, int flags) {
  file_descriptor_t *fd_table = posix_get_fd_table();
  open_file_t *oft = posix_get_open_file_table();

  char *kpath = copy_user_path(path);
  if (!kpath || kpath[0] == 0) {
    if (kpath) {
      kfree(kpath);
    }
    return -1;
  }

  if (!posix_flags_valid(flags)) {
    kfree(kpath);
    return -1;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("POSIX OPEN: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    kfree(kpath);
    return -1;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  int exists =
      (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) == 0);

  if (!exists) {
    if (!(flags & O_CREAT)) {
      kfree(kpath);
      return -1;
    }
    if (chainfs_write_file(kpath, (const u8 *)"", 0) != 0) {
      kfree(kpath);
      return -1;
    }
    exists =
        (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) == 0);
    if (!exists) {
      kfree(kpath);
      return -1;
    }
  } else if (entry.type == CHAINFS_TYPE_DEV) {
    int fd = posix_open_device(kpath, flags, &entry);
    kfree(kpath);
    return fd;
  } else if (entry.type == CHAINFS_TYPE_DIR) {
    kfree(kpath);
    return -1;
  } else if (flags & O_TRUNC) {
    if (chainfs_write_file(kpath, (const u8 *)"", 0) != 0) {
      kfree(kpath);
      return -1;
    }
    if (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) != 0) {
      kfree(kpath);
      return -1;
    }
  }

  int fd = posix_find_free_fd();
  if (fd < 0) {
    kfree(kpath);
    return -1;
  }

  int of_index = posix_alloc_open_file();
  if (of_index < 0) {
    kfree(kpath);
    return -1;
  }

  oft[of_index].flags = flags;
  oft[of_index].offset = (flags & O_APPEND) ? entry.size : 0;

  memset(oft[of_index].path, 0, sizeof(oft[of_index].path));
  int path_len = strlen(kpath);
  if (path_len >= (int)sizeof(oft[of_index].path)) {
    path_len = (int)sizeof(oft[of_index].path) - 1;
  }
  memcpy(oft[of_index].path, kpath, path_len);
  kfree(kpath);

  fd_table[fd].used = 1;
  fd_table[fd].flags = flags;
  fd_table[fd].of_index = of_index;

  return fd;
}
