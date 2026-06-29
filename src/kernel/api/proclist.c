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
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int api_proc_list(struct api_proc_info *buf, u32 max_entries) {
  if (!buf || max_entries == 0) {
    return -API_ERR_BAD_VALUE;
  }
  if (!is_user_address(buf, max_entries * sizeof(struct api_proc_info))) {
    return -API_ERR_BAD_ADDR;
  }

  u32 count = 0;
  for (int i = 0; i < MAX_PROCESSES && count < max_entries; i++) {
    process_t *proc = &process_table[i];
    if (proc->pid == 0) {
      continue;
    }

    thread_t *td = proc->main_thread;
    if (!td) {
      continue;
    }

    buf[count].pid = proc->pid;
    buf[count].ppid = proc->ppid;
    buf[count].state = (u32)td->state;

    memset(buf[count].name, 0, sizeof(buf[count].name));
    int j = 0;
    while (j < 31 && proc->name[j] != '\0') {
      buf[count].name[j] = proc->name[j];
      j++;
    }
    buf[count].name[j] = '\0';

    count++;
  }

  return (int)count;
}
