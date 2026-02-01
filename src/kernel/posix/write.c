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
#include <lib/com1.h>

file_descriptor_t fd_table[MAX_FDS];

void posix_init() {
  fd_table[STDIN_FILENO].used = 1;
  fd_table[STDOUT_FILENO].used = 1;
  fd_table[STDERR_FILENO].used = 1;
}

int sys_write(int fd, const void *buf, u32 count) {
  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  if (!fd_table[fd].used) {
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

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("POSIX WRITE: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    return -1;
  }

  int result = chainfs_write_file(fd_table[fd].path, (const u8 *)buf, count);

  if (result == 0) {
    fd_table[fd].offset += count;
    return count;
  }

  return -1;
}
