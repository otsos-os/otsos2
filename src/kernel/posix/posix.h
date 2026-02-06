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

#ifndef POSIX_H
#define POSIX_H

#include <kernel/interrupts/idt.h>
#include <mlibc/mlibc.h>

#define MAX_FDS 32
#define MAX_OPEN_FILES 64

struct process;

typedef struct {
  int used;
  int flags;
  int of_index; /* index into open file table, -1 for stdio */
} file_descriptor_t;

typedef struct {
  int used;
  int refcount;
  char path[256];
  u32 offset;
  int flags;
} open_file_t;

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR (O_RDONLY | O_WRONLY)
#define O_CREAT 0x0040
#define O_TRUNC 0x0200
#define O_APPEND 0x0400

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int sys_read(int fd, void *buf, u32 count);
int sys_write(int fd, const void *buf, u32 count);
int sys_open(const char *path, int flags);
int sys_close(int fd);
long sys_lseek(int fd, long offset, int whence);
int sys_wait(int *status);
int sys_fork(registers_t *regs);
int sys_execve(const char *path, const char *const *argv,
               const char *const *envp, registers_t *regs);
void posix_init(void);
void posix_init_process(struct process *proc);
void posix_copy_fds(struct process *dst, const struct process *src);
void posix_release_fds(struct process *proc);
file_descriptor_t *posix_get_fd_table(void);
open_file_t *posix_get_open_file_table(void);
int posix_alloc_open_file(void);
void posix_release_open_file(int index);

#endif
