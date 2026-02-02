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
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/vga.h>
#include <kernel/posix/posix.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

int sys_read(int fd, void *buf, u32 count) {
  if (fd < 0 || fd >= MAX_FDS) {
    return -1;
  }

  if (!fd_table[fd].used) {
    return -1;
  }

  char *data = (char *)buf;

  if (fd == STDIN_FILENO) {
    __asm__ volatile("sti");
    u32 i = 0;
    while (i < count) {
      char c;
      scanf("%c", &c);

      data[i] = c;

      vga_putc(c);
      com1_write_byte(c);

      i++;
      if (c == '\n') {
        break;
      }
    }
    return i;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    return -1;
  }

  u32 bytes_read = 0;
  int res = chainfs_read_file(fd_table[fd].path, (u8 *)buf, count, &bytes_read);

  if (res == 0) {
    fd_table[fd].offset += bytes_read;
    return bytes_read;
  }

  return -1;
}
