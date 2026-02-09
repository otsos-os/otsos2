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
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

int sys_read(int fd, void *buf, u32 count) {
  file_descriptor_t *fd_table = posix_get_fd_table();
  open_file_t *oft = posix_get_open_file_table();

  if (fd < 0 || fd >= MAX_FDS) {
    com1_printf("[DEBUG] sys_read: invalid fd %d\n", fd);
    return -1;
  }

  if (!fd_table[fd].used) {
    return -1;
  }

  if (count == 0) {
    return 0;
  }

  if (!is_user_address(buf, count)) {
    com1_printf("[DEBUG] sys_read: invalid user buffer %p (%d)\n", buf,
                (int)count);
    process_t *proc = process_current();
    if (proc && (proc->context.cs & 3) == 3) {
      process_exit(-1);
    }
    return -1;
  }

  if (fd == STDIN_FILENO) {
    return tty_read(buf, count);
  }

  int of_index = fd_table[fd].of_index;
  if (of_index >= 0 && of_index < MAX_OPEN_FILES && oft[of_index].used &&
      oft[of_index].type == OFT_TYPE_TTY) {
    if (!(fd_table[fd].flags & O_RDONLY)) {
      return -1;
    }
    return tty_read(buf, count);
  }

  if (!(fd_table[fd].flags & O_RDONLY)) {
    return -1;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    return -1;
  }

  if (of_index < 0 || of_index >= MAX_OPEN_FILES || !oft[of_index].used) {
    return -1;
  }

  if (oft[of_index].type == OFT_TYPE_PIPE) {
    return pipe_read((pipe_t *)oft[of_index].pipe, buf, count);
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(oft[of_index].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return -1;
  }

  if (oft[of_index].offset >= entry.size) {
    return 0;
  }

  u32 to_read = count;
  u32 remaining = entry.size - oft[of_index].offset;
  if (to_read > remaining) {
    to_read = remaining;
  }

  u32 bytes_read = 0;
  int res = chainfs_read_file_range(oft[of_index].path, (u8 *)buf, to_read,
                                    oft[of_index].offset, &bytes_read);

  if (res == 0) {
    oft[of_index].offset += bytes_read;
    return bytes_read;
  }

  return -1;
}
