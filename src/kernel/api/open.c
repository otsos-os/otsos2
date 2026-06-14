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
#include <kernel/api/api.h>
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

static int api_find_free_handle(void) {
  api_handle_t *handles = api_get_handle_table();
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (!handles[i].used) {
      return i;
    }
  }
  return -API_ERR_HANDLES_FULL;
}

static int api_flags_valid(int flags) {
  if ((flags & API_OPEN_RW) == 0) {
    return 0;
  }

  if ((flags & (API_OPEN_CREATE | API_OPEN_TRUNC | API_OPEN_APPEND)) &&
      !(flags & API_OPEN_WRITE)) {
    return 0;
  }

  return 1;
}

static int path_is_old_dev_namespace(const char *path) {
  return path && path[0] == '/' && path[1] == 'd' && path[2] == 'e' &&
         path[3] == 'v' && (path[4] == '\0' || path[4] == '/');
}

int api_data_open(const char *path, int flags) {
  api_handle_t *handles = api_get_handle_table();
  api_object_t *objects = api_get_object_table();

  char *kpath = copy_user_path(path);
  if (!kpath || kpath[0] == 0) {
    if (kpath) {
      kfree(kpath);
    }
    return -API_ERR_BAD_ADDR;
  }

  if (!api_flags_valid(flags)) {
    kfree(kpath);
    return -API_ERR_BAD_VALUE;
  }

  if (path_is_old_dev_namespace(kpath)) {
    kfree(kpath);
    return -API_ERR_NO_DEVICE;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("API OPEN: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    kfree(kpath);
    return -API_ERR_IO;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  int exists =
      (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) == 0);

  if (!exists) {
    if (!(flags & API_OPEN_CREATE)) {
      kfree(kpath);
      return -API_ERR_NOT_FOUND;
    }
    if (chainfs_write_file(kpath, (const u8 *)"", 0) != 0) {
      kfree(kpath);
      return -API_ERR_IO;
    }
    exists =
        (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) == 0);
    if (!exists) {
      kfree(kpath);
      return -API_ERR_IO;
    }
  } else if (entry.type == CHAINFS_TYPE_DIR) {
    kfree(kpath);
    return -API_ERR_IS_DIR;
  } else if (flags & API_OPEN_TRUNC) {
    if (chainfs_write_file(kpath, (const u8 *)"", 0) != 0) {
      kfree(kpath);
      return -API_ERR_IO;
    }
    if (chainfs_find_file(kpath, &entry, &entry_block, &entry_offset) != 0) {
      kfree(kpath);
      return -API_ERR_IO;
    }
  }

  int handle = api_find_free_handle();
  if (handle < 0) {
    kfree(kpath);
    return handle;
  }

  int object_index = api_alloc_object();
  if (object_index < 0) {
    kfree(kpath);
    return object_index;
  }

  objects[object_index].flags = flags;
  objects[object_index].offset = (flags & API_OPEN_APPEND) ? entry.size : 0;

  memset(objects[object_index].path, 0, sizeof(objects[object_index].path));
  int path_len = strlen(kpath);
  if (path_len >= (int)sizeof(objects[object_index].path)) {
    path_len = (int)sizeof(objects[object_index].path) - 1;
  }
  memcpy(objects[object_index].path, kpath, path_len);
  kfree(kpath);

  handles[handle].used = 1;
  handles[handle].flags = flags;
  handles[handle].object_index = object_index;

  return handle;
}
