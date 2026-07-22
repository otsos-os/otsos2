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

#include <kernel/api/posix/posix.h>
#include <kernel/api/api.h>
#include <kernel/api/auxv.h>
#include <kernel/api/session.h>
#include <kernel/console/terminal.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <kernel/gdt.h>
#include <kernel/cm/cm.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/signal.h>
#include <kernel/useraddr.h>
#include <kernel/interrupts/idt.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_map.h>
#include <userland/elf.h>
#include <userland/userspace.h>

extern void	futex_wake_all(u64 uaddr);
extern int	futex_wait(u64 uaddr, u32 expected_val);
extern int	futex_wake(u64 uaddr, u32 max_waiters);

extern void pmap_destroy_page_tables_only(u64 cr3);

#define EXECVE_MAX_ARGS	64
#define EXECVE_MAX_ENVP	64
#define EXECVE_MAX_STR	256

static char *
copy_user_string(const char *user, int max_len)
{
	char	*kbuf;
	int	len;

	if (!user || !is_user_address(user, 1)) {
		return (NULL);
	}

	len = 0;
	while (len < max_len) {
		if (!is_user_address(user + len, 1)) {
			return (NULL);
		}
		if (user[len] == '\0') {
			break;
		}
		len++;
	}

	if (len >= max_len) {
		return (NULL);
	}

	kbuf = (char *)kmem_calloc(len + 1, 1);
	if (!kbuf) {
		return (NULL);
	}

	memcpy(kbuf, user, len);
	kbuf[len] = '\0';
	return (kbuf);
}

static int
copy_user_string_array(const char *const *user, char ***out, int max_count)
{
	char	**arr;
	int	count;

	if (!user) {
		*out = NULL;
		return (0);
	}

	if (!is_user_address(user, sizeof(char *))) {
		return (-POSIX_EFAULT);
	}

	arr = (char **)kmem_calloc(max_count + 1, sizeof(char *));
	if (!arr) {
		return (-POSIX_ENOMEM);
	}

	count = 0;
	while (count < max_count) {
		if (!is_user_address(&user[count], sizeof(char *))) {
			break;
		}
		if (user[count] == NULL) {
			break;
		}
		char	*copy;

		copy = copy_user_string(user[count], EXECVE_MAX_STR);
		if (!copy) {
			int	i;
			for (i = 0; i < count; i++) {
				kmem_free(arr[i]);
			}
			kmem_free(arr);
			return (-POSIX_EFAULT);
		}
		arr[count++] = copy;
	}

	arr[count] = NULL;
	*out = arr;
	return (count);
}

static void
free_string_array(char **arr)
{
	int	i;

	if (!arr) {
		return;
	}
	for (i = 0; arr[i]; i++) {
		kmem_free(arr[i]);
	}
	kmem_free(arr);
}

static u64
allocate_execve_stack(process_t *proc)
{
	if (vm_map_create_user_stack(proc) != 0) {
		return (0);
	}
	return (USER_STACK_BASE);
}

#define EXECVE_AUXV_COUNT	8

static int
build_execve_stack(char **argv, int argc, char **envp, int envc,
    const auxv_desc_t *aux, u64 *out_rsp, u64 *out_argv, u64 *out_envp)
{
	u64	sp;
	u64	stack_min;
	u64	*argv_ptrs;
	u64	*envp_ptrs;
	u64	at_random_addr;
	u64	words, total;
	int	i;

	sp = USER_STACK_BASE & ~0xFULL;
	stack_min = USER_STACK_TOP;

	argv_ptrs = (u64 *)kmem_calloc(argc ? argc : 1, sizeof(u64));
	envp_ptrs = (u64 *)kmem_calloc(envc ? envc : 1, sizeof(u64));
	if (!argv_ptrs || !envp_ptrs) {
		kmem_free(argv_ptrs);
		kmem_free(envp_ptrs);
		return (-POSIX_ENOMEM);
	}

	for (i = envc - 1; i >= 0; i--) {
		size_t	len;
		len = strlen(envp[i]) + 1;
		if (sp < stack_min + len) {
			kmem_free(argv_ptrs);
			kmem_free(envp_ptrs);
			return (-POSIX_ENOMEM);
		}
		sp -= len;
		memcpy((void *)sp, envp[i], len);
		envp_ptrs[i] = sp;
	}

	for (i = argc - 1; i >= 0; i--) {
		size_t	len;
		len = strlen(argv[i]) + 1;
		if (sp < stack_min + len) {
			kmem_free(argv_ptrs);
			kmem_free(envp_ptrs);
			return (-POSIX_ENOMEM);
		}
		sp -= len;
		memcpy((void *)sp, argv[i], len);
		argv_ptrs[i] = sp;
	}

	sp &= ~0xFULL;
	if (sp < stack_min + 16) {
		kmem_free(argv_ptrs);
		kmem_free(envp_ptrs);
		return (-POSIX_ENOMEM);
	}
	sp -= 16;
	at_random_addr = sp;
	if (crypto_rng_bytes((u8 *)sp, 16) != 0) {
		memset((void *)sp, 0, 16);
	}

	sp &= ~0xFULL;

	words = 1 + (u64)argc + 1 + (u64)envc + 1 + 2 * EXECVE_AUXV_COUNT;
	total = words * 8;
	if (sp < stack_min + total + 8) {
		kmem_free(argv_ptrs);
		kmem_free(envp_ptrs);
		return (-POSIX_ENOMEM);
	}
	if (((sp - total) & 0xFULL) != 0) {
		sp -= 8;
	}

	// high adress first
	sp -= 8; *(u64 *)sp = 0;//at_null.a_val
	sp -= 8; *(u64 *)sp = AT_NULL;//at_null.a_type

	{
		u64	aux_pairs[EXECVE_AUXV_COUNT - 1][2] = {
			{AT_PHDR, aux->at_phdr},
			{AT_PHENT, aux->at_phent},
			{AT_PHNUM, aux->at_phnum},
			{AT_ENTRY, aux->at_entry},
			{AT_BASE, aux->at_base},
			{AT_PAGESZ, aux->at_pagesz},
			{AT_RANDOM, at_random_addr},
		};
		for (i = EXECVE_AUXV_COUNT - 2; i >= 0; i--) {
			sp -= 8; *(u64 *)sp = aux_pairs[i][1];
			sp -= 8; *(u64 *)sp = aux_pairs[i][0];
		}
	}

	sp -= 8;
	*(u64 *)sp = 0;
	for (i = envc - 1; i >= 0; i--) {
		sp -= 8;
		*(u64 *)sp = envp_ptrs[i];
	}

	*out_envp = sp;

	sp -= 8;
	*(u64 *)sp = 0;
	for (i = argc - 1; i >= 0; i--) {
		sp -= 8;
		*(u64 *)sp = argv_ptrs[i];
	}

	*out_argv = sp;

	sp -= 8;
	*(u64 *)sp = (u64)argc;

	*out_rsp = sp;

	kmem_free(argv_ptrs);
	kmem_free(envp_ptrs);
	return (0);
}

static void
copy_process_name(char *dst, const char *path)
{
	const char	*base;
	int		i;

	base = path;
	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == '/') {
			base = path + i + 1;
		}
	}

	memset(dst, 0, PROCESS_NAME_LEN);
	for (i = 0; i < PROCESS_NAME_LEN - 1 && base[i] != '\0'; i++) {
		dst[i] = base[i];
	}
}


static int
execve_read_file(const char *path, u8 **out_buf, u32 *out_size)
{
	vnode_t		*vn;
	posix_stat_t	st;
	u32		size, total;
	u8		*buf;

	vn = NULL;
	if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
		return (-POSIX_ENOENT);
	}
	if (!vnode_can_exec(vn)) {
		vnode_release(vn);
		return (-POSIX_EACCES);
	}
	if (vn->type == VDIR) {
		vnode_release(vn);
		return (-POSIX_EISDIR);
	}
	if (vnode_stat(vn, &st) != 0 || st.st_size == 0) {
		vnode_release(vn);
		return (-POSIX_ENOEXEC);
	}

	size = (u32)st.st_size;
	buf = (u8 *)kmem_calloc(size, 1);
	if (!buf) {
		vnode_release(vn);
		return (-POSIX_ENOMEM);
	}

	{
		int nr = vnode_read(vn, buf, size, 0);
		if (nr < 0) {
			vnode_release(vn);
			kmem_free(buf);
			return (-POSIX_EIO);
		}
		total = (u32)nr;
	}
	vnode_release(vn);

	if (total != size) {
		kmem_free(buf);
		return (-POSIX_EIO);
	}

	*out_buf = buf;
	*out_size = size;
	return (0);
}

s64
posix_getpid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	proc = process_current();
	if (!proc) {
		return (0);
	}
	return ((s64)proc->pid);
}

s64
posix_getppid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	proc = process_current();
	if (!proc) {
		return (0);
	}
	return ((s64)proc->ppid);
}

s64
posix_getuid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;

  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
  (void)regs;

  proc = process_current();
  if (!proc) {
    return (0);
  }
  return ((s64)proc->uid);
}

s64
posix_getgid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;

  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
  (void)regs;

  proc = process_current();
  if (!proc) {
    return (0);
  }
  return ((s64)proc->gid);
}

s64
posix_geteuid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;

  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
  (void)regs;

  proc = process_current();
  if (!proc) {
    return (0);
  }
  return ((s64)proc->euid);
}

s64
posix_getegid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;

  (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
  (void)regs;

  proc = process_current();
  if (!proc) {
    return (0);
  }
  return ((s64)proc->egid);
}

s64
posix_setuid(u64 uid_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;
  u32		uid;

  (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

  proc = process_current();
  if (!proc) {
    return (-POSIX_EFAULT);
  }

  uid = (u32)uid_u;

  if (proc->euid == 0) {
    proc->uid = uid;
    proc->euid = uid;
    proc->suid = uid;
    return (0);
  }
  if (uid == proc->uid || uid == proc->suid) {
    proc->euid = uid;
    return (0);
  }
  return (-POSIX_EPERM);
}

s64
posix_setgid(u64 gid_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  struct process	*proc;
  u32		gid;

  (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

  proc = process_current();
  if (!proc) {
    return (-POSIX_EFAULT);
  }

  gid = (u32)gid_u;

  if (proc->egid == 0) {
    proc->gid = gid;
    proc->egid = gid;
    proc->sgid = gid;
    return (0);
  }
  if (gid == proc->gid || gid == proc->sgid) {
    proc->egid = gid;
    return (0);
  }
  return (-POSIX_EPERM);
}

s64
posix_setpgid(u64 pid_u, u64 pgid_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	u32		pid;
	u32		pgid;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	pid = (u32)pid_u;
	pgid = (u32)pgid_u;

	if (pid == 0) {
		proc = process_current();
	} else {
		proc = process_get(pid);
	}
	if (!proc) {
		return (-POSIX_ESRCH);
	}
	if (pgid == 0) {
		pgid = proc->pid;
	}
	/* session change is not allowed */
	if (proc->sid != proc->pid && pgid != proc->sid) {
		return (-POSIX_EPERM);
	}
	proc->pgid = pgid;
	return (0);
}

s64
posix_getpgrp(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_ESRCH);
	}
	return ((s64)proc->pgid);
}

s64
posix_getsid(u64 pid_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	if (pid_u == 0) {
		proc = process_current();
	} else {
		proc = process_get((u32)pid_u);
	}
	if (!proc) {
		return (-POSIX_ESRCH);
	}
	return ((s64)proc->sid);
}

s64
posix_setsid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	int		sid;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	sid = api_session_setsid();
	if (sid < 0) {
		if (sid == -API_ERR_BAD_VALUE || sid == -API_ERR_NO_PROC) {
			return (-POSIX_ESRCH);
		}
		if (sid == -API_ERR_PERM) {
			return (-POSIX_EPERM);
		}
		return (-POSIX_EPERM);
	}
	return ((s64)sid);
}

s64
posix_getpgid(u64 pid_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	if (pid_u == 0) {
		proc = process_current();
	} else {
		proc = process_get((u32)pid_u);
	}
	if (!proc) {
		return (-POSIX_ESRCH);
	}
	return ((s64)proc->pgid);
}

s64
posix_exit(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	struct thread	*td;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (0);
	}

	td = thread_current();
	if (!td) {
		posix_cleanup_process(proc);
		process_exit((int)code);
		return (0);
	}

	/* If this is the last alive thread, exit the whole process */
	if (thread_count_alive(proc) <= 1) {
		posix_cleanup_process(proc);
		process_exit((int)code);
		return (0);
	}

	/* Otherwise just exit this thread */
	thread_exit((int)code);
	return (0);
}

s64
posix_exit_group(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (0);
	}

	posix_cleanup_process(proc);
	process_exit((int)code);
	return (0);
}

s64
posix_wait4(u64 pid_u, u64 status_u, u64 options, u64 rusage_u,
    u64 a5, u64 a6, registers_t *regs)
{
	struct process	*current;
	process_t	*child;
	thread_t	*child_td;
	u64		old_cr3;
	int		*status;
	int		attempt, i, reaped_pid;

	(void)pid_u; (void)options; (void)rusage_u; (void)a5; (void)a6;
	(void)regs;

	current = process_current();
	if (!current) {
		return (-POSIX_ECHILD);
	}

	status = (int *)status_u;

	for (attempt = 0; attempt < 2; attempt++) {
		for (i = 0; i < MAX_PROCESSES; i++) {
			child = &process_table[i];
			if (child->pid == 0) {
				continue;
			}
			child_td = child->main_thread;
			if (!child_td ||
			    child_td->state != PROC_STATE_ZOMBIE) {
				continue;
			}
			if (child->ppid != current->pid) {
				continue;
			}

			if (status && is_user_address(status,
			    sizeof(int))) {
				*status = child->exit_code;
			}

			if (child_td->running_cpu >= 0) {
				reaped_pid = (int)child->pid;
				if (current->controlling_tty >= 0) {
					terminal_set_pgrp(current->controlling_tty,
					    current->pgid);
				}
				terminal_set_pgrp(terminal_get_active(),
				    current->pgid);
				child->ppid = 0;
				return ((s64)reaped_pid);
			}

			if (child->owns_address_space && child->cr3) {
				old_cr3 = pmap_get_cr3();
				pmap_load(child->cr3);
				vm_map_free_all(child);
				pmap_load(old_cr3);
				pmap_destroy(child->cr3);
				child->cr3 = 0;
				child->owns_address_space = 0;
			}

			thread_destroy(child_td);

			reaped_pid = (int)child->pid;
			memset(child, 0, sizeof(process_t));
			if (current->controlling_tty >= 0) {
				terminal_set_pgrp(current->controlling_tty,
				    current->pgid);
			}
			terminal_set_pgrp(terminal_get_active(),
			    current->pgid);
			return ((s64)reaped_pid);
		}

		if (attempt == 0) {
			proc_sleep((void *)current);
		}
	}

	return (-POSIX_ECHILD);
}

s64
posix_fork(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*parent;
	struct process	*child;
	struct thread	*parent_td;
	struct thread	*child_td;
	u64		child_cr3;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

	parent = process_current();
	if (!parent || !regs) {
		return (-POSIX_EFAULT);
	}

	parent_td = thread_current();
	if (!parent_td) {
		return (-POSIX_EFAULT);
	}

	child = alloc_process();
	if (!child) {
		return (-POSIX_EAGAIN);
	}
	memset(child, 0, sizeof(process_t));

	child_cr3 = pmap_clone(parent->cr3);
	if (!child_cr3) {
		memset(child, 0, sizeof(process_t));
		return (-POSIX_ENOMEM);
	}

	child->pid = next_pid++;
	child->ppid = parent->pid;
	child->cr3 = child_cr3;
	child->entry_point = parent->entry_point;

	{
		int	i;
		for (i = 0; i < PROCESS_NAME_LEN - 1 &&
		    parent->name[i] != '\0'; i++) {
			child->name[i] = parent->name[i];
		}
		child->name[i] = '\0';
	}

	child->user_stack = parent->user_stack;
	child->exit_code = 0;
	child->owns_address_space = 1;
	child->mmap_base = parent->mmap_base;
	child->brk_min = parent->brk_min;
	child->brk = parent->brk;
  child->kusr_auth = parent->kusr_auth;
  child->uid = parent->uid;
  child->gid = parent->gid;
  child->euid = parent->euid;
  child->egid = parent->egid;
  child->suid = parent->suid;
  child->sgid = parent->sgid;
  child->sid = parent->sid;
  child->pgid = parent->pgid;
	child->is_session_leader = 0;
	child->controlling_tty = parent->controlling_tty;

	vm_map_fork(parent, child);
	api_copy_handles(child, parent);
	posix_copy_fds(child, parent);

	/* Create thread for child process */
	child_td = thread_create(child, parent->entry_point,
	    parent->user_stack, USER_CS, USER_DS);
	if (!child_td) {
		pmap_destroy(child_cr3);
		memset(child, 0, sizeof(process_t));
		return (-POSIX_ENOMEM);
	}

	child->main_thread = child_td;
	child->cur_thread = child_td;

	/* Copy parent's context, set return value to 0 */
	process_save_context(parent, regs);
	child_td->context = parent_td->context;
	thread_copy_fpu_context(child_td, parent_td);
	child_td->context.rax = 0;
	child_td->fs_base = parent_td->fs_base;

	return ((s64)child->pid);
}

s64
posix_execve(u64 path_u, u64 argv_u, u64 envp_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	process_t	new_as;
	char		*kpath;
	char		**kargv;
	char		**kenvp;
	int		argc, envc;
	u8		*elf_buf;
	u32		elf_size;
	vnode_t		*vn;
	posix_stat_t	st;
	u64		new_cr3, old_cr3;
	u64		entry_point;
	u64		user_stack, new_rsp, argv_addr, envp_addr;
	u64		data_start, data_end;
	auxv_desc_t	aux;
	int		i;

	(void)a4; (void)a5; (void)a6;

	proc = process_current();
	if (!proc || !regs) {
		return (-POSIX_EFAULT);
	}

	kpath = copy_user_string((const char *)path_u, 256);
	if (!kpath) {
		return (-POSIX_EFAULT);
	}

	kargv = NULL;
	kenvp = NULL;
	argc = copy_user_string_array((const char *const *)argv_u, &kargv,
	    EXECVE_MAX_ARGS);
	if (argc < 0) {
		kmem_free(kpath);
		return ((s64)argc);
	}
	envc = copy_user_string_array((const char *const *)envp_u, &kenvp,
	    EXECVE_MAX_ENVP);
	if (envc < 0) {
		free_string_array(kargv);
		kmem_free(kpath);
		return ((s64)envc);
	}

	vn = NULL;
	if (vfs_resolve(kpath, &vn) != 0 || vn == NULL) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOENT);
	}
	if (!vnode_can_exec(vn)) {
		vnode_release(vn);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_EACCES);
	}

	if (vn->type == VDIR) {
		vnode_release(vn);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_EISDIR);
	}

	if (vnode_stat(vn, &st) != 0) {
		vnode_release(vn);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_EIO);
	}

	if (st.st_size == 0) {
		vnode_release(vn);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOEXEC);
	}

	elf_size = (u32)st.st_size;
	elf_buf = (u8 *)kmem_calloc(elf_size, 1);
	if (!elf_buf) {
		vnode_release(vn);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOMEM);
	}

	{
		u32	total;
		int	nr;

		nr = vnode_read(vn, elf_buf, elf_size, 0);
		if (nr < 0) {
			vnode_release(vn);
			kmem_free(elf_buf);
			free_string_array(kargv);
			free_string_array(kenvp);
			kmem_free(kpath);
			return (-POSIX_EIO);
		}
		total = (u32)nr;

		vnode_release(vn);

		if (total != elf_size) {
			kmem_free(elf_buf);
			free_string_array(kargv);
			free_string_array(kenvp);
			kmem_free(kpath);
			return (-POSIX_EIO);
		}
	}

	data_start = 0;
	data_end = 0;
	memset(&new_as, 0, sizeof(new_as));

	new_cr3 = pmap_create();
	if (!new_cr3) {
		kmem_free(elf_buf);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOMEM);
	}

	old_cr3 = pmap_get_cr3();
	new_as.cr3 = new_cr3;
	new_as.owns_address_space = 1;
	new_as.mmap_base = MMAP_BASE;
	pmap_load(new_cr3);

	{
		elf_loadinfo_t	li;

		entry_point = elf_load_full(elf_buf, elf_size, &li);
		if (entry_point == 0) {
			kmem_free(elf_buf);
			pmap_load(old_cr3);
			pmap_destroy(new_cr3);
			free_string_array(kargv);
			free_string_array(kenvp);
			kmem_free(kpath);
			return (-POSIX_ENOEXEC);
		}

		data_start = li.data_start;
		data_end = li.data_end;
		if (data_end == 0 && li.load_addr_max != 0) {
			data_end = (li.load_addr_max + PAGE_SIZE - 1)
			    & ~(PAGE_SIZE - 1);
			data_start = 0;
		}

		aux.at_phdr = li.phdr_vaddr;
		aux.at_phent = li.phent;
		aux.at_phnum = li.phnum;
		aux.at_entry = li.entry;
		aux.at_base = 0;
		aux.at_pagesz = PAGE_SIZE;

		if (li.interp_off != 0 && li.interp_len != 0) {
			char	interp_path[256];
			u64	ilen;
			u8	*interp_buf;
			u32	interp_size;
			u64	interp_entry;
			int	ierr;

			ilen = li.interp_len;
			if (ilen > sizeof(interp_path)) {
				ilen = sizeof(interp_path);
			}
			memcpy(interp_path, (char *)elf_buf + li.interp_off,
			    ilen);
			interp_path[ilen - 1] = '\0';
			kmem_free(elf_buf);
			elf_buf = NULL;

			interp_buf = NULL;
			interp_size = 0;
			ierr = execve_read_file(interp_path, &interp_buf,
			    &interp_size);
			if (ierr < 0) {
				pmap_load(old_cr3);
				pmap_destroy(new_cr3);
				free_string_array(kargv);
				free_string_array(kenvp);
				kmem_free(kpath);
				return ((s64)ierr);
			}

			interp_entry = elf_load_interp(interp_buf, interp_size,
			    ELF_INTERP_BASE);
			kmem_free(interp_buf);
			if (interp_entry == 0) {
				pmap_load(old_cr3);
				pmap_destroy(new_cr3);
				free_string_array(kargv);
				free_string_array(kenvp);
				kmem_free(kpath);
				return (-POSIX_ENOEXEC);
			}

			aux.at_base = ELF_INTERP_BASE;
			entry_point = interp_entry;
		} else {
			kmem_free(elf_buf);
			elf_buf = NULL;
		}
	}

	user_stack = allocate_execve_stack(&new_as);
	if (user_stack == 0) {
		pmap_load(old_cr3);
		pmap_destroy(new_cr3);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOMEM);
	}

	{
		int	err;
		err = build_execve_stack(kargv, argc, kenvp, envc,
		    &aux, &new_rsp, &argv_addr, &envp_addr);
		if (err < 0) {
			vm_map_free_all(&new_as);
			pmap_load(old_cr3);
			pmap_destroy(new_cr3);
			free_string_array(kargv);
			free_string_array(kenvp);
			kmem_free(kpath);
			return ((s64)err);
		}
	}

	free_string_array(kargv);
	free_string_array(kenvp);

	if (data_end != 0) {
		register_data_bss(&new_as, data_start, data_end);
	}

	pmap_load(old_cr3);

	if (proc->owns_address_space && proc->cr3) {
		u64	dead_cr3;

		dead_cr3 = proc->cr3;
		vm_map_free_all(proc);
		pmap_load(new_cr3);
		pmap_destroy(dead_cr3);
	} else {
		pmap_load(new_cr3);
	}

	proc->vma_list = new_as.vma_list;
	new_as.vma_list = NULL;
	proc->cr3 = new_cr3;
	proc->entry_point = entry_point;
	proc->user_stack = user_stack;
	proc->owns_address_space = 1;
  proc->mmap_base = MMAP_BASE;
  proc->kusr_auth = 0;
  proc->suid = proc->euid;
  proc->sgid = proc->egid;

  copy_process_name(proc->name, kpath);
	kmem_free(kpath);

	for (i = 0; i < MAX_POSIX_FDS; i++) {
		if (proc->posix_fds[i].used &&
		    proc->posix_fds[i].cloexec) {
			if (proc->posix_fds[i].vnode) {
				vnode_release(proc->posix_fds[i].vnode);
			}
			proc->posix_fds[i].used = 0;
			proc->posix_fds[i].vnode = NULL;
		}
	}

	proc->sigmask = 0;
	proc->sigpending = 0;
	for (i = 0; i < MAX_POSIX_SIGS; i++) {
		proc->sigaction[i].handler = 0;
		proc->sigaction[i].mask = 0;
		proc->sigaction[i].flags = 0;
		proc->sigaction[i].restorer = 0;
	}

	regs->rip = entry_point;
	regs->rsp = new_rsp;
	regs->rdi = (u64)argc;
	regs->rsi = argv_addr;
	regs->rdx = envp_addr;
	regs->rax = 0;
	regs->rbx = 0;
	regs->rcx = 0;
	regs->rbp = 0;
	regs->r8 = 0;
	regs->r9 = 0;
	regs->r10 = 0;
	regs->r11 = 0x202;
	regs->r12 = 0;
	regs->r13 = 0;
	regs->r14 = 0;
	regs->r15 = 0;

	if (data_end != 0) {
		proc->brk_min = data_end;
		proc->brk = data_end;
	}

	return (0);
}

s64
posix_kill(u64 pid_u, u64 sig_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*target;
	u32		pid;
	int		sig;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	pid = (u32)pid_u;
	sig = (int)sig_u;

	if (sig == 0) {
		if (process_get(pid) == NULL) {
			return (-POSIX_ESRCH);
		}
		return (0);
	}

	if (sig < 1 || sig > MAX_POSIX_SIGS) {
		return (-POSIX_EINVAL);
	}

	target = process_get(pid);
	if (!target) {
		return (-POSIX_ESRCH);
	}

	return ((s64)process_send_signal(pid, sig));
}

s64
posix_uname(u64 buf_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	posix_utsname_t	*buf;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	if (!is_user_address((void *)buf_u, sizeof(posix_utsname_t))) {
		return (-POSIX_EFAULT);
	}

	buf = (posix_utsname_t *)buf_u;
	memset(buf, 0, sizeof(posix_utsname_t));

	cm_get_string_default("SYSTEM", "Os", "SysName",
	    buf->sysname, sizeof(buf->sysname), "otsos2");
	cm_get_string_default("SYSTEM", "Os", "NodeName",
	    buf->nodename, sizeof(buf->nodename), "localhost");
	cm_get_string_default("SYSTEM", "Os", "Release",
	    buf->release, sizeof(buf->release), "0.0.0");
	cm_get_string_default("SYSTEM", "Os", "Version",
	    buf->version, sizeof(buf->version), "unknown");
	cm_get_string_default("SYSTEM", "Os", "Machine",
	    buf->machine, sizeof(buf->machine), "x86_64");
	cm_get_string_default("SYSTEM", "Os", "DomainName",
	    buf->domainname, sizeof(buf->domainname), "localdomain");

	return (0);
}

static volatile int	nanosleep_channel;

s64
posix_nanosleep(u64 req_u, u64 rem_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	return (posix_clock_nanosleep(POSIX_CLOCK_REALTIME, 0, req_u, rem_u,
	    a5, a6, regs));
}

s64
posix_clone(u64 flags_u, u64 stack, u64 ptid, u64 ctid, u64 tls,
    u64 a6, registers_t *regs)
{
	struct process	*parent;
	struct thread	*parent_td;
	u64		flags;

	(void)a6;
	(void)tls;	/* TODO: CLONE_SETTLS */

	parent = process_current();
	if (!parent || !regs) {
		return (-POSIX_EFAULT);
	}

	parent_td = thread_current();
	if (!parent_td) {
		return (-POSIX_EFAULT);
	}

	flags = flags_u;

	/* Thread creation: CLONE_VM | CLONE_THREAD */
	if ((flags & POSIX_CLONE_THREAD) && (flags & POSIX_CLONE_VM)) {
		struct thread	*new_td;

		if (!stack) {
			return (-POSIX_EINVAL);
		}

		new_td = thread_create(parent,
		    parent_td->context.rip,
		    stack & ~0xFULL,
		    USER_CS, USER_DS);
		if (!new_td) {
			return (-POSIX_ENOMEM);
		}

		thread_save_context(parent_td, regs);
		new_td->context = parent_td->context;
		thread_copy_fpu_context(new_td, parent_td);
		new_td->context.rax = 0;
		new_td->context.rsp = stack & ~0xFULL;

		/* CLONE_PARENT_SETTID: write child TID to ptid */
		if ((flags & POSIX_CLONE_PARENT_SETTID) && ptid) {
			if (is_user_address((void *)ptid,
			    sizeof(u32))) {
				*(u32 *)ptid = new_td->tid;
			}
		}

		/* CLONE_CHILD_SETTID: write child TID to ctid
		 * (in child's address space — same as parent for threads) */
		if ((flags & POSIX_CLONE_CHILD_SETTID) && ctid) {
			if (is_user_address((void *)ctid,
			    sizeof(u32))) {
				*(u32 *)ctid = new_td->tid;
			}
		}

		/* CLONE_CHILD_CLEARTID: set tid_address for futex wake
		 * on thread exit */
		if (flags & POSIX_CLONE_CHILD_CLEARTID) {
			new_td->tid_address = ctid;
		}

		printk("[POSIX clone] new thread tid=%d "
		    "PID %d\n", new_td->tid, parent->pid);

		return ((s64)new_td->tid);
	}

	/* Full process fork */
	if (flags & (POSIX_CLONE_VM | POSIX_CLONE_THREAD)) {
		return (-POSIX_EINVAL);
	}

	/* Fall back to fork semantics */
	return (posix_fork(0, 0, 0, 0, 0, 0, regs));
}

s64
posix_gettid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct thread	*td;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	td = thread_current();
	if (!td) {
		return (0);
	}
	return ((s64)td->tid);
}

s64
posix_futex(u64 uaddr_u, u64 op_u, u64 val_u, u64 timeout, u64 uaddr2,
    u64 val3, registers_t *regs)
{
	int		op;
	u32		val;
	u64		uaddr;

	(void)timeout; (void)uaddr2; (void)val3; (void)regs;

	uaddr = uaddr_u;
	op = (int)op_u;
	val = (u32)val_u;

	if (!is_user_address((void *)uaddr, sizeof(u32))) {
		return (-POSIX_EFAULT);
	}

	switch (op) {
	case FUTEX_WAIT:
		return ((s64)futex_wait(uaddr, val));

	case FUTEX_WAKE:
		return ((s64)futex_wake(uaddr, val));

	default:
		printk("[POSIX futex] unsupported op=%d\n",
		    op);
		return (-POSIX_ENOSYS);
	}
}

s64
posix_set_tid_address(u64 tidptr_u, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct thread	*td;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	td = thread_current();
	if (!td) {
		return (-POSIX_EFAULT);
	}

	td->tid_address = tidptr_u;

	/* Write our TID to the address so userspace can check
	 * if the thread is still alive */
	if (tidptr_u && is_user_address((void *)tidptr_u,
	    sizeof(u32))) {
		u64	flags;

		flags = pmap_extract_flags(tidptr_u);
		if (!(flags & PTE_PRESENT) || (flags & PTE_COW) ||
		    !(flags & PTE_RW)) {
			if (vm_map_fault(process_current(), tidptr_u, 0x3)
			    != 0 && vm_cow_fault(tidptr_u, 0x3) != 0) {
				return (-POSIX_EFAULT);
			}
		}
		*(u32 *)tidptr_u = td->tid;
	}

	return ((s64)td->tid);
}

#define	MSR_FS_BASE	0xC0000100

static inline u64
posix_rdmsr(u32 msr)
{
	u32	low, high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (((u64)high << 32) | low);
}

static inline void
posix_wrmsr(u32 msr, u64 value)
{
	u32	low, high;

	low = (u32)value;
	high = (u32)(value >> 32);
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

#define	ARCH_SET_FS	0x1002
#define	ARCH_GET_FS	0x1003
#define	ARCH_SET_GS	0x1001
#define	ARCH_GET_GS	0x1004

s64
posix_arch_prctl(u64 code, u64 addr, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct thread	*td;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	td = thread_current();
	if (!td) {
		return (-POSIX_EFAULT);
	}

	switch (code) {
	case ARCH_SET_FS:
		posix_wrmsr(MSR_FS_BASE, addr);
		td->fs_base = addr;
		return (0);

	case ARCH_GET_FS:
		if (!is_user_address((void *)addr, sizeof(u64))) {
			return (-POSIX_EFAULT);
		}
		*(u64 *)addr = posix_rdmsr(MSR_FS_BASE);
		return (0);

	case ARCH_SET_GS:
		posix_wrmsr(0xC0000101, addr);
		return (0);

	case ARCH_GET_GS:
		if (!is_user_address((void *)addr, sizeof(u64))) {
			return (-POSIX_EFAULT);
		}
		*(u64 *)addr = posix_rdmsr(0xC0000101);
		return (0);

	default:
		return (-POSIX_EINVAL);
	}
}
s64
posix_getrandom(u64 buf_u, u64 buflen, u64 flags, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	(void)a4; (void)a5; (void)a6; (void)regs;

	if (flags & ~3) {
		return (-POSIX_EINVAL);
	}
	if (buflen == 0) {
		return (0);
	}
	if (!is_user_address((void *)buf_u, buflen)) {
		return (-POSIX_EFAULT);
	}

	return ((s64)api_sys_random((u8 *)buf_u, (u32)buflen));
}

s64
posix_time(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	return ((s64)time_second);
}

s64
posix_gettimeofday(u64 tv_u, u64 tz_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct bintime	bt;
	struct {
		long	tv_sec;
		long	tv_usec;
	} *tv;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	bintime(&bt);

	if (tv_u != 0) {
		tv = (void *)tv_u;
		if (!is_user_address(tv, sizeof(*tv))) {
			return (-POSIX_EFAULT);
		}
		tv->tv_sec = (long)bt.sec;
		tv->tv_usec = (long)bintime_frac_to_usec(bt.frac);
	}

	(void)tz_u;
	return (0);
}

s64
posix_clock_gettime(u64 clock_id, u64 tp_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct bintime	bt;
	struct {
		long	tv_sec;
		long	tv_nsec;
	} *tp;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	if (clock_id == POSIX_CLOCK_REALTIME) {
		bintime(&bt);
	} else if (clock_id == POSIX_CLOCK_MONOTONIC) {
		binuptime(&bt);
	} else {
		return (-POSIX_EINVAL);
	}

	if (tp_u == 0) {
		return (-POSIX_EFAULT);
	}
	tp = (void *)tp_u;
	if (!is_user_address(tp, sizeof(*tp))) {
		return (-POSIX_EFAULT);
	}
	tp->tv_sec = (long)bt.sec;
	tp->tv_nsec = (long)bintime_frac_to_nsec(bt.frac);

	return (0);
}

s64
posix_clock_nanosleep(u64 clock_id, u64 flags, u64 req_u, u64 rem_u,
    u64 a5, u64 a6, registers_t *regs)
{
	struct {
		long	tv_sec;
		long	tv_nsec;
	} *req;
	struct thread	*td;
	u64		ns;
	u64		ms;
	u64		target_ticks;

	(void)rem_u; (void)a5; (void)a6; (void)regs;

	if (clock_id != POSIX_CLOCK_REALTIME &&
	    clock_id != POSIX_CLOCK_MONOTONIC) {
		return (-POSIX_EINVAL);
	}

	if (req_u == 0) {
		return (-POSIX_EFAULT);
	}
	req = (void *)req_u;
	if (!is_user_address(req, sizeof(*req))) {
		return (-POSIX_EFAULT);
	}

	if (req->tv_sec < 0 || req->tv_nsec < 0 ||
	    (u64)req->tv_nsec >= NSEC_PER_SEC) {
		return (-POSIX_EINVAL);
	}

	ns = (u64)req->tv_sec * NSEC_PER_SEC + (u64)req->tv_nsec;
	ms = ns / 1000000ULL;
	if (ms == 0) {
		return (0);
	}

	td = thread_current();
	if (!td) {
		return (-POSIX_EINTR);
	}

	if (flags & POSIX_CLOCK_TIMER_ABSTIME) {
		u64	boot_epoch;

		boot_epoch = time_second - time_uptime;
		if ((u64)req->tv_sec < boot_epoch) {
			return (0);
		}
		ns = ((u64)req->tv_sec - boot_epoch) * NSEC_PER_SEC +
		    (u64)req->tv_nsec;
		ms = ns / 1000000ULL;
	}

	target_ticks = timer_get_ticks() + ms;
	if (target_ticks < timer_get_ticks()) {
		target_ticks = timer_get_ticks();
	}

	td->sleep_target_ticks = target_ticks;
	proc_sleep((void *)&nanosleep_channel);

	return (0);
}

s64
posix_times(u64 buf_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct {
		long	tms_utime;
		long	tms_stime;
		long	tms_cutime;
		long	tms_cstime;
	} *buf;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	if (buf_u == 0) {
		return (0);
	}
	buf = (void *)buf_u;
	if (!is_user_address(buf, sizeof(*buf))) {
		return (-POSIX_EFAULT);
	}
	buf->tms_utime = 0;
	buf->tms_stime = 0;
	buf->tms_cutime = 0;
	buf->tms_cstime = 0;
	return (0);
}
