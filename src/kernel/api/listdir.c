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
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

#define LISTDIR_MAX_FILES 128

int api_fs_listdir(const char *path, struct api_dirent *buf, u32 max_entries) {
  if (!buf || max_entries == 0) {
    return -API_ERR_BAD_VALUE;
  }
  if (!is_user_address(buf, max_entries * sizeof(struct api_dirent))) {
    return -API_ERR_BAD_ADDR;
  }

  const char *use_path = path;
  if (path) {
    if (!is_user_address(path, 1)) {
      return -API_ERR_BAD_ADDR;
    }
  }

  chainfs_file_entry_t entries[LISTDIR_MAX_FILES];
  u32 file_count = 0;

  const char *list_path = NULL;
  if (use_path && use_path[0] == '\0') {
    list_path = NULL;
  } else {
    list_path = use_path;
  }

  int ret = chainfs_list_dir(list_path, entries, LISTDIR_MAX_FILES, &file_count);
  if (ret != 0) {
    return ret;
  }

  u32 to_copy = file_count;
  if (to_copy > max_entries) {
    to_copy = max_entries;
  }

  for (u32 i = 0; i < to_copy; i++) {
    memset(buf[i].name, 0, sizeof(buf[i].name));
    int j = 0;
    while (j < 31 && entries[i].name[j] != '\0') {
      buf[i].name[j] = entries[i].name[j];
      j++;
    }
    buf[i].name[j] = '\0';
    buf[i].type = entries[i].type;
    buf[i].pad[0] = 0;
    buf[i].pad[1] = 0;
    buf[i].pad[2] = 0;
  }

  return (int)to_copy;
}
