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

#include <kernel/gdt.h>
#include <kernel/mmu.h>
#include <kernel/panic.h>
#include <kernel/process.h>
#include <lib/com1.h>
#include <mlibc/memory.h>

process_t process_table[MAX_PROCESSES];
u32 next_pid = 1;
static process_t *current_process = NULL;

void process_init(void) {
  com1_printf("[PROC] Initializing process subsystem...\n");

  memset(process_table, 0, sizeof(process_table));
  next_pid = 1;
  current_process = NULL;

  com1_printf("[PROC] Process table initialized (%d slots)\n", MAX_PROCESSES);
}

process_t *alloc_process(void) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i].state == PROC_STATE_UNUSED) {
      return &process_table[i];
    }
  }
  return NULL;
}

process_t *process_create_kernel(const char *name, void (*entry)(void)) {
  process_t *proc = alloc_process();
  if (!proc) {
    com1_printf("[PROC] Error: No free process slots\n");
    return NULL;
  }

  u8 *kstack = (u8 *)kmalloc_aligned(KERNEL_STACK_SIZE, 16);
  if (!kstack) {
    com1_printf("[PROC] Error: Failed to allocate kernel stack\n");
    return NULL;
  }
  memset(kstack, 0, KERNEL_STACK_SIZE);

  
  proc->pid = next_pid++;
  proc->ppid = 0; 
  proc->state = PROC_STATE_EMBRYO;

  int i;
  for (i = 0; i < PROCESS_NAME_LEN - 1 && name[i]; i++) {
    proc->name[i] = name[i];
  }
  proc->name[i] = '\0';

  proc->cr3 = mmu_read_cr3();
  proc->entry_point = (u64)entry;

  
  proc->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);
  proc->user_stack = 0; 

  memset(&proc->context, 0, sizeof(cpu_context_t));
  proc->context.rip = (u64)entry;
  proc->context.cs = KERNEL_CS;
  proc->context.rflags = 0x202; 
  proc->context.rsp = proc->kernel_stack;
  proc->context.ss = KERNEL_DS;

  proc->exit_code = 0;
  proc->next = NULL;

  proc->state = PROC_STATE_RUNNABLE;

  com1_printf("[PROC] Created kernel process '%s' (PID %d) entry=%p\n",
              proc->name, proc->pid, (void *)proc->entry_point);

  return proc;
}

process_t *process_get(u32 pid) {
  for (int i = 0; i < MAX_PROCESSES; i++) {
    if (process_table[i].state != PROC_STATE_UNUSED &&
        process_table[i].pid == pid) {
      return &process_table[i];
    }
  }
  return NULL;
}

process_t *process_current(void) { return current_process; }

void process_set_current(process_t *proc) {
  current_process = proc;
  if (proc) {
    proc->state = PROC_STATE_RUNNING;
    tss_set_rsp0(proc->kernel_stack);
  }
}

void process_exit(int code) {
  if (!current_process) {
    com1_printf("[PROC] Error: No current process to exit\n");
    return;
  }

  com1_printf("[PROC] Process '%s' (PID %d) exited with code %d\n",
              current_process->name, current_process->pid, code);

  if (current_process->pid == 1) {
    panic("Init process terminated! (PID 1 exited with code %d)", code);
  }

  current_process->exit_code = code;
  current_process->state = PROC_STATE_ZOMBIE;


  while (1) {
    __asm__ volatile("hlt");
  }
}

void process_dump(process_t *proc) {
  if (!proc) {
    com1_printf("[PROC] NULL process\n");
    return;
  }

  const char *state_names[] = {"UNUSED",  "EMBRYO",   "RUNNABLE",
                               "RUNNING", "SLEEPING", "ZOMBIE"};

  com1_printf("=== Process Dump ===\n");
  com1_printf("  PID: %d, PPID: %d\n", proc->pid, proc->ppid);
  com1_printf("  Name: %s\n", proc->name);
  com1_printf("  State: %s\n", state_names[proc->state]);
  com1_printf("  Entry: %p\n", (void *)proc->entry_point);
  com1_printf("  CR3: %p\n", (void *)proc->cr3);
  com1_printf("  Kernel Stack: %p\n", (void *)proc->kernel_stack);
  com1_printf("  User Stack: %p\n", (void *)proc->user_stack);
  com1_printf("  Context RIP: %p\n", (void *)proc->context.rip);
  com1_printf("  Context RSP: %p\n", (void *)proc->context.rsp);
  com1_printf("====================\n");
}

process_t *process_create(const char *name, void *elf_data, u64 elf_size) {
  extern process_t *userspace_load_elf(const char *name, void *elf_data,
                                       u64 elf_size);
  return userspace_load_elf(name, elf_data, elf_size);
}

int process_kill(u32 pid) {
  if (pid == 1) {
    return -1; 
  }

  process_t *proc = process_get(pid);
  if (!proc) {
    return -1;
  }

  if (proc == current_process) {
    process_exit(-1);
    return 0;
  }


  if (proc->kernel_stack) {
    void *kstack_base = (void *)(proc->kernel_stack - KERNEL_STACK_SIZE);
    kfree(kstack_base);
  }

  memset(proc, 0, sizeof(process_t));
  proc->state = PROC_STATE_UNUSED;

  return 0;
}
