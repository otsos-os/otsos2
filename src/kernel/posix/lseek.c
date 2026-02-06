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

long sys_lseek(int fd, long offset, int whence) {
  file_descriptor_t *fd_table = posix_get_fd_table();
  open_file_t *oft = posix_get_open_file_table();

  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }
  if (!fd_table[fd].used) {
    return -1;
  }

  int of_index = fd_table[fd].of_index;
  if (of_index < 0 || of_index >= MAX_OPEN_FILES) {
    /* stdio or invalid: not seekable */
    return -1;
  }
  if (!oft[of_index].used) {
    return -1;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(oft[of_index].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return -1;
  }

  long long new_off = 0;
  switch (whence) {
  case SEEK_SET:
    new_off = (long long)offset;
    break;
  case SEEK_CUR:
    new_off = (long long)oft[of_index].offset + (long long)offset;
    break;
  case SEEK_END:
    new_off = (long long)entry.size + (long long)offset;
    break;
  default:
    return -1;
  }

  if (new_off < 0) {
    return -1;
  }

  oft[of_index].offset = (u32)new_off;
  return (long)oft[of_index].offset;
}
