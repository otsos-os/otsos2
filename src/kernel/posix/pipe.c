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

#include <kernel/posix/posix.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/memory.h>

static int posix_find_free_fd(void) {
  file_descriptor_t *fd_table = posix_get_fd_table();
  for (int i = 3; i < MAX_FDS; i++) {
    if (!fd_table[i].used) {
      return i;
    }
  }
  return -EMFILE;
}

int pipe_read(pipe_t *p, void *buf, u32 count) {
  if (!p || !buf || count == 0) {
    return 0;
  }
  if (p->size == 0) {
    if (p->writers == 0) {
      return 0;
    }
    return 0;
  }

  u32 to_read = count;
  if (to_read > p->size) {
    to_read = p->size;
  }

  u8 *out = (u8 *)buf;
  for (u32 i = 0; i < to_read; i++) {
    out[i] = p->buffer[p->read_pos];
    p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
  }
  p->size -= to_read;
  return (int)to_read;
}

int pipe_write(pipe_t *p, const void *buf, u32 count) {
  if (!p || !buf || count == 0) {
    return 0;
  }
  if (p->readers == 0) {
    return -EPIPE;
  }

  u32 space = PIPE_BUF_SIZE - p->size;
  if (space == 0) {
    return 0;
  }

  u32 to_write = count;
  if (to_write > space) {
    to_write = space;
  }

  const u8 *in = (const u8 *)buf;
  for (u32 i = 0; i < to_write; i++) {
    p->buffer[p->write_pos] = in[i];
    p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
  }
  p->size += to_write;
  return (int)to_write;
}

int sys_pipe(int fds[2]) {
  if (!is_user_address(fds, sizeof(int) * 2)) {
    return -EFAULT;
  }

  file_descriptor_t *fd_table = posix_get_fd_table();
  open_file_t *oft = posix_get_open_file_table();

  int fd_read = posix_find_free_fd();
  if (fd_read < 0) {
    return -EMFILE;
  }
  fd_table[fd_read].used = 1;
  int fd_write = posix_find_free_fd();
  if (fd_write < 0) {
    fd_table[fd_read].used = 0;
    return -EMFILE;
  }
  fd_table[fd_read].used = 0;

  int of_read = posix_alloc_open_file();
  if (of_read < 0) {
    return -ENFILE;
  }
  int of_write = posix_alloc_open_file();
  if (of_write < 0) {
    posix_release_open_file(of_read);
    return -ENFILE;
  }

  pipe_t *p = (pipe_t *)kmalloc(sizeof(pipe_t));
  if (!p) {
    posix_release_open_file(of_read);
    posix_release_open_file(of_write);
    return -ENOMEM;
  }
  memset(p, 0, sizeof(pipe_t));
  p->readers = 1;
  p->writers = 1;

  oft[of_read].type = OFT_TYPE_PIPE;
  oft[of_read].pipe = p;
  oft[of_read].flags = O_RDONLY;

  oft[of_write].type = OFT_TYPE_PIPE;
  oft[of_write].pipe = p;
  oft[of_write].flags = O_WRONLY;

  fd_table[fd_read].used = 1;
  fd_table[fd_read].flags = O_RDONLY;
  fd_table[fd_read].of_index = of_read;

  fd_table[fd_write].used = 1;
  fd_table[fd_write].flags = O_WRONLY;
  fd_table[fd_write].of_index = of_write;

  fds[0] = fd_read;
  fds[1] = fd_write;
  return 0;
}
