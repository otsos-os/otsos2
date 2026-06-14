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

#ifndef API_H
#define API_H

#include <kernel/interrupts/idt.h>
#include <kernel/api/errno.h>
#include <mlibc/mlibc.h>

#define MAX_HANDLES 32
#define MAX_DATA_OBJECTS 64

struct process;

typedef struct {
  int used;
  int flags;
  int object_index;
} api_handle_t;

typedef struct {
  int used;
  int refcount;
  int type;
  char path[256];
  u32 offset;
  int flags;
  void *pipe;
} api_object_t;

#define API_OPEN_READ 0x0001
#define API_OPEN_WRITE 0x0002
#define API_OPEN_RW (API_OPEN_READ | API_OPEN_WRITE)
#define API_OPEN_CREATE 0x0040
#define API_OPEN_TRUNC 0x0200
#define API_OPEN_APPEND 0x0400

#define API_SEEK_SET 0
#define API_SEEK_CUR 1
#define API_SEEK_END 2

#define API_MAP_READ 0x1
#define API_MAP_WRITE 0x2
#define API_MAP_EXEC 0x4

#define API_MAP_PRIVATE 0x02
#define API_MAP_FIXED 0x10
#define API_MAP_ANON 0x20

#define API_CLONE_VM 0x00000100
#define API_CLONE_THREAD 0x00010000

#define API_OBJECT_FILE 0
#define API_OBJECT_PIPE 1
#define MMAP_BASE 0x0000001000000000ULL
#define MMAP_LIMIT 0x00007FFF00000000ULL

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
  u8 buffer[PIPE_BUF_SIZE];
  u32 read_pos;
  u32 write_pos;
  u32 size;
  int readers;
  int writers;
} pipe_t;

struct api_sysinfo {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

int api_term_read(void *buf, u32 count);
int api_term_write(const void *buf, u32 count);
int api_data_read(int handle, void *buf, u32 count);
int api_info(struct api_sysinfo *buf);
void api_info_fill(struct api_sysinfo *buf);
int api_data_write(int handle, const void *buf, u32 count);
int api_data_open(const char *path, int flags);
int api_data_close(int handle);
long api_data_seek(int handle, long offset, int whence);
int api_proc_wait(int *status);
int api_data_pipe(int handles[2]);
long api_proc_clone(u64 flags, u64 child_stack, u64 ptid, registers_t *regs);
u64 api_mem_map(const void *uargs);
int api_proc_fork(registers_t *regs);

int pipe_read(pipe_t *p, void *buf, u32 count);
int pipe_write(pipe_t *p, const void *buf, u32 count);
int api_proc_spawn(const char *path, const char *const *argv,
                   const char *const *envp, registers_t *regs);
void api_init(void);
void api_init_process(struct process *proc);
void api_copy_handles(struct process *dst, const struct process *src);
void api_release_handles(struct process *proc);
api_handle_t *api_get_handle_table(void);
api_object_t *api_get_object_table(void);
int api_alloc_object(void);
void api_release_object(int index);

#endif
