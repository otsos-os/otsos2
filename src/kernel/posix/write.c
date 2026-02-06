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
#include <kernel/drivers/vga.h>
#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

file_descriptor_t fd_table[MAX_FDS];

void posix_init() {
  com1_printf("[POSIX] Initializing FD table at %p\n", fd_table);
  memset(fd_table, 0, sizeof(fd_table));
  fd_table[STDIN_FILENO].used = 1;
  fd_table[STDIN_FILENO].flags = O_RDONLY;
  fd_table[STDOUT_FILENO].used = 1;
  fd_table[STDOUT_FILENO].flags = O_WRONLY;
  fd_table[STDERR_FILENO].used = 1;
  fd_table[STDERR_FILENO].flags = O_WRONLY;
  
}

int sys_write(int fd, const void *buf, u32 count) {
  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  if (!fd_table[fd].used) {
    return -1;
  }

  if (count == 0) {
    return 0;
  }

  if (!is_user_address(buf, count)) {
    process_t *proc = process_current();
    if (proc && (proc->context.cs & 3) == 3) {
      process_exit(-1);
    }
    return -1;
  }

  const char *data = (const char *)buf;

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    for (u32 i = 0; i < count; i++) {
      vga_putc(data[i]);
      com1_write_byte(data[i]);
    }
    return count;
  }

  if (!(fd_table[fd].flags & O_WRONLY)) {
    return -1;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("POSIX WRITE: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    return -1;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(fd_table[fd].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return -1;
  }

  if (fd_table[fd].flags & O_APPEND) {
    fd_table[fd].offset = entry.size;
  }

  u32 offset = fd_table[fd].offset;
  u32 end_pos = offset + count;
  u32 new_size = (end_pos > entry.size) ? end_pos : entry.size;

  u8 *new_data = (u8 *)kcalloc(new_size, 1);
  if (!new_data) {
    return -1;
  }

  if (entry.size > 0) {
    u32 bytes_read = 0;
    if (chainfs_read_file(fd_table[fd].path, new_data, entry.size,
                          &bytes_read) != 0) {
      kfree(new_data);
      return -1;
    }
  }

  memcpy(new_data + offset, buf, count);

  int result = chainfs_write_file(fd_table[fd].path, new_data, new_size);
  kfree(new_data);

  if (result == 0) {
    fd_table[fd].offset = offset + count;
    return count;
  }

  return -1;
}
