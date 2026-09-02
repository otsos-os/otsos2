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
#include <kernel/entity/entity.h>
#include <mlibc/mlibc.h>
#include <mm/vm/vm_map.h>

struct vm_object;

#define PERSONALITY_OTSOS	0
#define PERSONALITY_POSIX	1

#define MAX_PROCESSES 256
#define PROCESS_NAME_LEN 32

#define	PROC_REC_CODE		0
#define	PROC_REC_FLAGS		1
#define	PROC_REC_PID		2
#define	PROC_REC_PPID		3
#define	PROC_REC_NOTIFY		4
#define	PROC_REC_TICK		0
#define	PROC_REC_PARENT_ID	1
#define	PROC_EXIT_EXITED	0x1
#define	PROC_EXIT_KILLED	0x2
#define	PROC_EXIT_REAPED	0x4
#define	PROC_NOTIFY_NONE	0x0
#define	PROC_NOTIFY_APC		0x1
#define USER_STACK_SIZE VM_MAP_STACK_INIT
#define USER_STACK_MAX_SIZE VM_MAP_STACK_MAX
#define USER_STACK_END VM_MAP_STACK_END
#define USER_STACK_BASE (VM_MAP_STACK_END - 16)
#define USER_STACK_TOP VM_MAP_STACK_TOP
#define USER_STACK_LIMIT VM_MAP_STACK_LIMIT
#define KERNEL_STACK_SIZE (16 * 1024) // 16 kb kheap for thread

/* Process Control Block (PCB) */
typedef struct process {
  u32 pid;                     /* Process ID */
  u32 ppid;                    /* Parent Process ID */
  u64 entity;                  /* entity registry id */
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

  u64 exit_upcall;
  int exit_upcall_special;

  /* Address space ownership */
  int owns_address_space;
  int resources_released;
  int reapable;

  /* KUSR privilege */
  int kusr_auth;
  u32 uid;
  u32 gid;
  u32 euid;
  u32 egid;
  u32 suid;
  u32 sgid;

  vm_map_t vm_map;

	/* Entity handle table (index-linked, global store) */
	int		entity_handle_count;
	int		entity_handle_head;

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
process_t *process_ref(u32 pid);
void process_unref(process_t *proc);
int process_has_reapable(void);

process_t *process_current(void);

void process_set_current(process_t *proc);

/* Exit current process */
void process_exit(int code);
void process_exit_signalled(int code);
int process_kill(u32 pid);
int process_send_signal(u32 pid, int sig);
int process_entity_attach(process_t *proc);
entity_id_t process_entity_of_pid(u32 pid);
entity_id_t process_record_find_child(entity_id_t parent, u32 want_pid);
int process_child_count(u32 pid);
void process_creation_abort(process_t *proc);
int process_record_read(entity_id_t id, int *code, int *flags, u32 *pid,
	    u32 *ppid);
int process_record_mark_reaped(entity_id_t id);
int process_record_consume(entity_id_t id);
int process_record_notify_mode(entity_id_t id);
int process_record_set_notify(entity_id_t id, int mode);
void process_reap(void);
void process_dump_records(void);

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
	return (proc != NULL && proc->kusr_auth);
}

/* Global process data */
/*
 * AoSoA: processes and their entity metadata live in one block so the
 * objects and metadata share the same allocation and cache lines.
 */
typedef struct process_block {
	entity_meta_block_t	meta;
	process_t		processes[MAX_PROCESSES];
} process_block_t;

extern process_block_t process_block;
#define process_table (process_block.processes)
extern u32 next_pid;

#endif
