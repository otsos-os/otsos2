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

#ifndef PROCESS_H
#define PROCESS_H

#include <mlibc/mlibc.h>

#define MAX_PROCESSES 64
#define PROCESS_NAME_LEN 32
#define USER_STACK_SIZE (64 * 1024)   /* 64 KB user stack */
#define KERNEL_STACK_SIZE (16 * 1024) /* 16 KB kernel stack per process */

/* Process states */
typedef enum {
  PROC_STATE_UNUSED = 0,
  PROC_STATE_EMBRYO,   /* Being created */
  PROC_STATE_RUNNABLE, /* Ready to run */
  PROC_STATE_RUNNING,  /* Currently running */
  PROC_STATE_SLEEPING, /* Waiting for event */
  PROC_STATE_ZOMBIE    /* Terminated, waiting for parent */
} process_state_t;

/* CPU context saved during context switch */
typedef struct {
  u64 r15, r14, r13, r12, r11, r10, r9, r8;
  u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
  u64 rip;
  u64 cs;
  u64 rflags;
  u64 rsp;
  u64 ss;
} __attribute__((packed)) cpu_context_t;

/* Process Control Block (PCB) */
typedef struct process {
  u32 pid;                     /* Process ID */
  u32 ppid;                    /* Parent Process ID */
  process_state_t state;       /* Current state */
  char name[PROCESS_NAME_LEN]; /* Process name */

  /* Memory */
  u64 cr3;         /* Page table root */
  u64 entry_point; /* Entry point address */

  /* Stack */
  u64 kernel_stack; /* Kernel stack top */
  u64 user_stack;   /* User stack top */

  /* Context */
  cpu_context_t context; /* Saved CPU state */

  /* Exit status */
  int exit_code;

  /* Links */
  struct process *next; /* For scheduler queue */
} process_t;

/* Initialize process subsystem */
void process_init(void);

/* Create a new process from ELF in memory */
process_t *process_create(const char *name, void *elf_data, u64 elf_size);

/* Create a new kernel-mode process (for testing without ELF) */
process_t *process_create_kernel(const char *name, void (*entry)(void));

/* Get process by PID */
process_t *process_get(u32 pid);

/* Get current running process */
process_t *process_current(void);

/* Set current running process */
void process_set_current(process_t *proc);

/* Exit current process */
void process_exit(int code);
int process_kill(u32 pid);

/* Switch to a process (used by scheduler) */
void process_switch(process_t *proc);

/* Yield CPU to another process */
void process_yield(void);

/* Debug: dump process info */
void process_dump(process_t *proc);

/* Internal: find free process slot */
process_t *alloc_process(void);

/* Global process data */
extern process_t process_table[MAX_PROCESSES];
extern u32 next_pid;

#endif
