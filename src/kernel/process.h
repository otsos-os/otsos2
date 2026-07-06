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

#include <kernel/api/api.h>
#include <kernel/api/posix/posix.h>
#include <kernel/thread.h>
#include <mlibc/mlibc.h>

struct vm_object;

#define PERSONALITY_OTSOS	0
#define PERSONALITY_POSIX	1

#define MAX_PROCESSES 64
#define PROCESS_NAME_LEN 32
#define USER_STACK_SIZE (64 * 1024)   /* 64 KB user stack */
#define KERNEL_STACK_SIZE (16 * 1024) // 16 kb kheap for thread

/* Virtual Memory Area — tracks a mapped region of a process's address space. */
typedef struct vma {
  u64 start;           /* page-aligned virtual address */
  u64 end;             /* page-aligned end (exclusive) */
  u32 prot;            /* API_MAP_READ / WRITE / EXEC */
  u32 flags;           /* API_MAP_ANON / MAP_GEM / MAP_PRIVATE etc */
  u32 gem_handle;      /* GEM handle if MAP_GEM, else 0 */
  u64 object_offset;   /* offset into vm_object */
  struct vm_object *object;
  struct vma *next;    /* singly-linked list, sorted by start */
} vma_t;

/* Process Control Block (PCB) */
typedef struct process {
  u32 pid;                     /* Process ID */
  u32 ppid;                    /* Parent Process ID */
  char name[PROCESS_NAME_LEN]; /* Process name */

  /* Memory */
  u64 cr3;         /* Page table root */
  u64 entry_point; /* Entry point address */

  /* Stack */
  u64 user_stack;   /* User stack top */

	thread_t	*main_thread; 
	thread_t	*cur_thread;
	thread_t	*thread_list;	/* linked list of all threads in this proc */
	int		thread_count;	/* number of alive threads */
	int		preferred_cpu;	/* scheduler placement target, -1 for any CPU */
	int		last_cpu;	/* last CPU that ran this process */

  /* Exit status */
  int exit_code;

  /* Address space ownership */
  int owns_address_space;

  /* KUSR privilege */
  int kusr_auth;
  u32 uid;
  u32 gid;
  u32 euid;
  u32 egid;
  u32 suid;
  u32 sgid;

  /* mmap */
  u64 mmap_base;
  vma_t *vma_list;    /* sorted list of virtual memory areas */

  /* File descriptors */
  api_handle_t handles[MAX_HANDLES];

	/* POSIX personality state */
	int		personality;
	posix_fd_t	posix_fds[MAX_POSIX_FDS];
	posix_sigaction_t	sigaction[MAX_POSIX_SIGS];
	u64		sigmask;
	u64		sigpending;
	u64		brk;
	u64		brk_min;

	/* Session / process group / controlling terminal */
	u32		sid;
	u32		pgid;
	int		controlling_tty;	/* terminal index, -1 if none */
	int		is_session_leader;
} process_t;

/* Initialize process subsystem */
void process_init(void);

/* Create a new process from ELF in memory */
process_t *process_create(const char *name, void *elf_data, u64 elf_size);

/* Create a new kernel-mode process (for testing without ELF) */
process_t *process_create_kernel(const char *name, void (*entry)(void));

/* Get process by PID */
process_t *process_get(u32 pid);

process_t *process_current(void);

void process_set_current(process_t *proc);

/* Exit current process */
void process_exit(int code);
int process_kill(u32 pid);
int process_send_signal(u32 pid, int sig);

void process_yield(void);

/* Debug: dump process info */
void process_dump(process_t *proc);

void process_save_context(process_t *proc, registers_t *regs);

/* Internal: find free process slot */
process_t *alloc_process(void);
int process_is_initialized(void);

/* Sleep / wake — defined in event.c, used by process and event subsystem */
void proc_sleep(void *channel);
void proc_wakeup(void *channel);
void proc_wakeup_one(void *channel);
static inline int proc_has_privilege(const process_t *proc)
{
	return (proc != NULL && (proc->kusr_auth || proc->euid == 0));
}

/* Global process data */
extern process_t process_table[MAX_PROCESSES];
extern u32 next_pid;

#endif
