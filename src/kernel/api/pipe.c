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

#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/memory.h>

static int api_find_free_handle(void) {
  api_handle_t *handles = api_get_handle_table();
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (!handles[i].used) {
      return i;
    }
  }
  return -API_ERR_HANDLES_FULL;
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
    return -API_ERR_PIPE_CLOSED;
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

int api_data_pipe(int handles_out[2]) {
  if (!is_user_address(handles_out, sizeof(int) * 2)) {
    return -API_ERR_BAD_ADDR;
  }

  api_handle_t *handles = api_get_handle_table();
  api_object_t *objects = api_get_object_table();

  int handle_read = api_find_free_handle();
  if (handle_read < 0) {
    return handle_read;
  }
  handles[handle_read].used = 1;
  int handle_write = api_find_free_handle();
  if (handle_write < 0) {
    handles[handle_read].used = 0;
    return handle_write;
  }
  handles[handle_read].used = 0;

  int of_read = api_alloc_object();
  if (of_read < 0) {
    return of_read;
  }
  int of_write = api_alloc_object();
  if (of_write < 0) {
    api_release_object(of_read);
    return of_write;
  }

  pipe_t *p = (pipe_t *)kmalloc(sizeof(pipe_t));
  if (!p) {
    api_release_object(of_read);
    api_release_object(of_write);
    return -API_ERR_NO_MEMORY;
  }
  memset(p, 0, sizeof(pipe_t));
  p->readers = 1;
  p->writers = 1;

  objects[of_read].type = API_OBJECT_PIPE;
  objects[of_read].pipe = p;
  objects[of_read].flags = API_OPEN_READ;

  objects[of_write].type = API_OBJECT_PIPE;
  objects[of_write].pipe = p;
  objects[of_write].flags = API_OPEN_WRITE;

  handles[handle_read].used = 1;
  handles[handle_read].flags = API_OPEN_READ;
  handles[handle_read].object_index = of_read;

  handles[handle_write].used = 1;
  handles[handle_write].flags = API_OPEN_WRITE;
  handles[handle_write].object_index = of_write;

  handles_out[0] = handle_read;
  handles_out[1] = handle_write;
  return 0;
}
