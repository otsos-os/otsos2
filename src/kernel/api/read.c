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
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

int api_term_read(void *buf, u32 count) {
  if (count == 0) {
    return 0;
  }

  if (!is_user_address(buf, count)) {
    com1_printf("[DEBUG] api_term_read: invalid user buffer %p (%d)\n", buf,
                (int)count);
    thread_t *td = thread_current();
    if (td && (td->context.cs & 3) == 3) {
      process_exit(-1);
    }
    return -API_ERR_BAD_ADDR;
  }

  return tty_read(buf, count);
}

int api_data_read(int handle, void *buf, u32 count) {
  api_handle_t *handles = api_get_handle_table();
  api_object_t *objects = api_get_object_table();

  if (handle < 0 || handle >= MAX_HANDLES) {
    com1_printf("[DEBUG] api_data_read: invalid handle %d\n", handle);
    return -API_ERR_BAD_HANDLE;
  }

  if (!handles[handle].used) {
    return -API_ERR_BAD_HANDLE;
  }

  if (count == 0) {
    return 0;
  }

  if (!is_user_address(buf, count)) {
    com1_printf("[DEBUG] api_data_read: invalid user buffer %p (%d)\n", buf,
                (int)count);
    thread_t *td = thread_current();
    if (td && (td->context.cs & 3) == 3) {
      process_exit(-1);
    }
    return -API_ERR_BAD_ADDR;
  }

  if (!(handles[handle].flags & API_OPEN_READ)) {
    return -API_ERR_BAD_HANDLE;
  }

  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    return -API_ERR_IO;
  }

  int object_index = handles[handle].object_index;
  if (object_index < 0 || object_index >= MAX_DATA_OBJECTS ||
      !objects[object_index].used) {
    return -API_ERR_BAD_HANDLE;
  }

  if (objects[object_index].type == API_OBJECT_PIPE) {
    return pipe_read((pipe_t *)objects[object_index].pipe, buf, count);
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;
  if (chainfs_find_file(objects[object_index].path, &entry, &entry_block,
                        &entry_offset) != 0) {
    return -API_ERR_NOT_FOUND;
  }

  if (objects[object_index].offset >= entry.size) {
    return 0;
  }

  u32 to_read = count;
  u32 remaining = entry.size - objects[object_index].offset;
  if (to_read > remaining) {
    to_read = remaining;
  }

  u32 bytes_read = 0;
  int res = chainfs_read_file_range(objects[object_index].path, (u8 *)buf,
                                    to_read, objects[object_index].offset,
                                    &bytes_read);

  if (res == 0) {
    objects[object_index].offset += bytes_read;
    return bytes_read;
  }

  return -API_ERR_IO;
}
