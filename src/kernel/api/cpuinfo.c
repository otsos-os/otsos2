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
#include <kernel/smp/smp.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>

int api_cpuinfo(struct api_cpuinfo *buf) {
  int cpu, cpus, reported_cpus, i;

  if (!is_user_address(buf, sizeof(struct api_cpuinfo))) {
    return -API_ERR_BAD_ADDR;
  }

  memset(buf, 0, sizeof(struct api_cpuinfo));

  reported_cpus = smp_cpu_count();
  if (reported_cpus <= 0) {
    reported_cpus = 1;
  }
  cpus = reported_cpus;
  if (cpus > API_CPUINFO_MAX_CPUS) {
    cpus = API_CPUINFO_MAX_CPUS;
  }

  buf->cpu_count = (u32)reported_cpus;
  buf->entry_count = (u32)cpus;

  for (cpu = 0; cpu < cpus; cpu++) {
    struct api_cpu_entry *entry;
    struct smp_cpu *smp_cpu;
    thread_t *td;
    process_t *proc;
    int j;
    entry = &buf->entries[cpu];
    smp_cpu = &smp_cpu_map[cpu];
    td = smp_cpu->current_thread;
    proc = td ? td->proc : NULL;
    entry->cpu_index = (u32)cpu;
    entry->lapic_id = smp_cpu->lapic_id;
    entry->present = (cpu == 0) ? 1 : smp_cpu->present;
    entry->online = (cpu == 0 || smp_ap_started((u8)cpu));
    entry->pid = proc ? proc->pid : 0;
    entry->tid = td ? td->tid : 0;
    entry->state = td ? (u32)td->state : 0;

    if (!proc) {
      continue;
    }
    for (j = 0; j < 31 && proc->name[j] != '\0'; j++) {
      entry->proc_name[j] = proc->name[j];
    }
    entry->proc_name[j] = '\0';
  }

  for (i = 0; i < MAX_PROCESSES; i++) {
    process_t *proc;
    int proc_cpu;

    proc = &process_table[i];
    if (proc->pid == 0) {
      continue;
    }
    proc_cpu = proc->last_cpu;
    if (proc_cpu < 0) {
      proc_cpu = proc->preferred_cpu;
    }
    if (proc_cpu < 0 || proc_cpu >= cpus) {
      continue;
    }
    if (buf->entries[proc_cpu].pid_count >= API_CPUINFO_MAX_PIDS) {
      continue;
    }
    buf->entries[proc_cpu].pids[buf->entries[proc_cpu].pid_count++] =
      proc->pid;
  }

  return 0;
}
