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
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>

static file_descriptor_t kernel_fd_table[MAX_FDS];
static open_file_t open_file_table[MAX_OPEN_FILES];

file_descriptor_t *posix_get_fd_table(void) {
  process_t *proc = process_current();
  if (proc) {
    return proc->fd_table;
  }
  return kernel_fd_table;
}

open_file_t *posix_get_open_file_table(void) { return open_file_table; }

int posix_alloc_open_file(void) {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!open_file_table[i].used) {
      open_file_table[i].used = 1;
      open_file_table[i].refcount = 1;
      open_file_table[i].offset = 0;
      open_file_table[i].flags = 0;
      open_file_table[i].type = OFT_TYPE_FILE;
      open_file_table[i].pipe = NULL;
      memset(open_file_table[i].path, 0, sizeof(open_file_table[i].path));
      return i;
    }
  }
  return -1;
}

void posix_release_open_file(int index) {
  if (index < 0 || index >= MAX_OPEN_FILES) {
    return;
  }
  if (!open_file_table[index].used) {
    return;
  }
  open_file_table[index].refcount--;
  if (open_file_table[index].refcount <= 0) {
    if (open_file_table[index].type == OFT_TYPE_PIPE &&
        open_file_table[index].pipe) {
      pipe_t *p = (pipe_t *)open_file_table[index].pipe;
      if (open_file_table[index].flags & O_WRONLY) {
        if (p->writers > 0) {
          p->writers--;
        }
      } else {
        if (p->readers > 0) {
          p->readers--;
        }
      }
      if (p->readers == 0 && p->writers == 0) {
        kfree(p);
      }
    }
    memset(&open_file_table[index], 0, sizeof(open_file_table[index]));
  }
}

void posix_init() {
  com1_printf("[POSIX] Initializing FD tables at %p\n", kernel_fd_table);
  memset(kernel_fd_table, 0, sizeof(kernel_fd_table));
  memset(open_file_table, 0, sizeof(open_file_table));

  kernel_fd_table[STDIN_FILENO].used = 1;
  kernel_fd_table[STDIN_FILENO].flags = O_RDONLY;
  kernel_fd_table[STDIN_FILENO].of_index = -1;

  kernel_fd_table[STDOUT_FILENO].used = 1;
  kernel_fd_table[STDOUT_FILENO].flags = O_WRONLY;
  kernel_fd_table[STDOUT_FILENO].of_index = -1;

  kernel_fd_table[STDERR_FILENO].used = 1;
  kernel_fd_table[STDERR_FILENO].flags = O_WRONLY;
  kernel_fd_table[STDERR_FILENO].of_index = -1;
}

void posix_init_process(struct process *proc) {
  if (!proc) {
    return;
  }
  memcpy(proc->fd_table, kernel_fd_table, sizeof(kernel_fd_table));
}

void posix_copy_fds(struct process *dst, const struct process *src) {
  if (!dst || !src) {
    return;
  }
  memcpy(dst->fd_table, src->fd_table, sizeof(dst->fd_table));
  for (int i = 0; i < MAX_FDS; i++) {
    if (!dst->fd_table[i].used) {
      continue;
    }
    int of_index = dst->fd_table[i].of_index;
    if (of_index >= 0 && of_index < MAX_OPEN_FILES &&
        open_file_table[of_index].used) {
      open_file_table[of_index].refcount++;
    }
  }
}

void posix_release_fds(struct process *proc) {
  if (!proc) {
    return;
  }
  for (int i = 0; i < MAX_FDS; i++) {
    if (!proc->fd_table[i].used) {
      continue;
    }
    int of_index = proc->fd_table[i].of_index;
    if (of_index >= 0) {
      posix_release_open_file(of_index);
    }
    proc->fd_table[i].used = 0;
    proc->fd_table[i].flags = 0;
    proc->fd_table[i].of_index = -1;
  }
}

int sys_write(int fd, const void *buf, u32 count) {
  file_descriptor_t *fd_table = posix_get_fd_table();
  open_file_t *oft = posix_get_open_file_table();

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

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    return tty_write(buf, count);
  }

  int of_index = fd_table[fd].of_index;
  if (of_index >= 0 && of_index < MAX_OPEN_FILES && oft[of_index].used &&
      oft[of_index].type == OFT_TYPE_TTY) {
    if (!(fd_table[fd].flags & O_WRONLY)) {
      return -1;
    }
    return tty_write(buf, count);
  }

  if (!(fd_table[fd].flags & O_WRONLY)) {
    return -1;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    com1_printf("POSIX WRITE: ChainFS not initialized or corrupted magic: %x\n",
                g_chainfs.superblock.magic);
    return -1;
  }

  if (of_index < 0 || of_index >= MAX_OPEN_FILES || !oft[of_index].used) {
    return -1;
  }

  if (oft[of_index].type == OFT_TYPE_PIPE) {
    return pipe_write((pipe_t *)oft[of_index].pipe, buf, count);
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(oft[of_index].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return -1;
  }

  if (fd_table[fd].flags & O_APPEND) {
    oft[of_index].offset = entry.size;
  }

  u32 offset = oft[of_index].offset;
  u32 end_pos = offset + count;
  u32 new_size = (end_pos > entry.size) ? end_pos : entry.size;

  u8 *new_data = (u8 *)kcalloc(new_size, 1);
  if (!new_data) {
    return -1;
  }

  if (entry.size > 0) {
    u32 bytes_read = 0;
    if (chainfs_read_file(oft[of_index].path, new_data, entry.size,
                          &bytes_read) != 0) {
      kfree(new_data);
      return -1;
    }
  }

  memcpy(new_data + offset, buf, count);

  int result = chainfs_write_file(oft[of_index].path, new_data, new_size);
  kfree(new_data);

  if (result == 0) {
    oft[of_index].offset = offset + count;
    return count;
  }

  return -1;
}
