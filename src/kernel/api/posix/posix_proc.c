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
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/event/event.h>
#include <kernel/gdt.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/useraddr.h>
#include <kernel/interrupts/idt.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_map.h>
#include <userland/elf.h>
#include <userland/userspace.h>

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
allocate_execve_stack(void)
{
	u64	stack_pages;
	u64	stack_bottom;
	u64	i;

	stack_pages = (USER_STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
	stack_bottom = USER_STACK_TOP;

	for (i = 0; i < stack_pages; i++) {
		u64	page;

		page = vm_page_alloc_phys(0);
		if (!page) {
			u64	j;
			for (j = 0; j < i; j++) {
				u64	vaddr, paddr;
				vaddr = stack_bottom + (j * PAGE_SIZE);
				paddr = pmap_extract(vaddr);
				pmap_remove(vaddr);
				if (paddr) {
					vm_page_free_phys(paddr);
				}
			}
			return (0);
		}
		memset((void *)page, 0, PAGE_SIZE);
		pmap_enter(stack_bottom + (i * PAGE_SIZE), page,
		    PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX);
	}

	return (USER_STACK_BASE);
}

static int
build_execve_stack(char **argv, int argc, char **envp, int envc,
    u64 *out_rsp, u64 *out_argv, u64 *out_envp)
{
	u64	sp;
	u64	stack_min;
	u64	*argv_ptrs;
	u64	*envp_ptrs;
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

	if (sp < stack_min + (u64)(8 * (argc + envc + 3))) {
		kmem_free(argv_ptrs);
		kmem_free(envp_ptrs);
		return (-POSIX_ENOMEM);
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
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_getgid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_geteuid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_getegid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}

s64
posix_exit(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	posix_cleanup_process(process_current());
	process_exit((int)code);
	return (0);
}

s64
posix_wait4(u64 pid_u, u64 status_u, u64 options, u64 rusage_u,
    u64 a5, u64 a6, registers_t *regs)
{
	struct process	*current;
	int		*status;
	int		i, attempt;

	(void)pid_u; (void)options; (void)rusage_u; (void)a5; (void)a6;
	(void)regs;

	current = process_current();
	if (!current) {
		return (-POSIX_ECHILD);
	}

	status = (int *)status_u;

	for (attempt = 0; attempt < 2; attempt++) {
		for (i = 0; i < MAX_PROCESSES; i++) {
			process_t	*child;

			child = &process_table[i];
			if (child->state != PROC_STATE_ZOMBIE) {
				continue;
			}
			if (child->ppid != current->pid) {
				continue;
			}

			if (status && is_user_address(status,
			    sizeof(int))) {
				*status = child->exit_code;
			}

			if (child->owns_address_space && child->cr3) {
				u64	old_cr3;
				old_cr3 = pmap_get_cr3();
				pmap_load(child->cr3);
				vm_map_free_all(child);
				pmap_load(old_cr3);
				pmap_destroy(child->cr3);
				child->cr3 = 0;
				child->owns_address_space = 0;
			}

			if (child->kernel_stack) {
				u64	kstack_base;
				kstack_base = child->kernel_stack -
				    KERNEL_STACK_SIZE;
				kmem_free((void *)kstack_base);
			}

			{
				int	reaped_pid;
				reaped_pid = (int)child->pid;
				memset(child, 0, sizeof(process_t));
				child->state = PROC_STATE_UNUSED;
				return ((s64)reaped_pid);
			}
		}

		if (attempt == 0) {
			extern void proc_sleep(void *channel);
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
	u64		child_cr3;
	u8		*kstack;

	(void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;

	parent = process_current();
	if (!parent || !regs) {
		return (-POSIX_EFAULT);
	}

	child = alloc_process();
	if (!child) {
		return (-POSIX_EAGAIN);
	}

	child->state = PROC_STATE_EMBRYO;

	child_cr3 = pmap_clone(parent->cr3);
	if (!child_cr3) {
		memset(child, 0, sizeof(process_t));
		child->state = PROC_STATE_UNUSED;
		return (-POSIX_ENOMEM);
	}

	kstack = (u8 *)kmem_alloc_aligned(KERNEL_STACK_SIZE, 16);
	if (!kstack) {
		pmap_destroy(child_cr3);
		memset(child, 0, sizeof(process_t));
		child->state = PROC_STATE_UNUSED;
		return (-POSIX_ENOMEM);
	}
	memset(kstack, 0, KERNEL_STACK_SIZE);

	memset(child, 0, sizeof(process_t));

	child->pid = next_pid++;
	child->ppid = parent->pid;
	child->state = PROC_STATE_RUNNABLE;
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

	child->kernel_stack = (u64)(kstack + KERNEL_STACK_SIZE);
	child->user_stack = parent->user_stack;

	process_save_context(parent, regs);
	child->context = parent->context;
	child->context.rax = 0;

	child->exit_code = 0;
	child->owns_address_space = 1;
	child->mmap_base = parent->mmap_base;
	child->kusr_auth = parent->kusr_auth;

	vm_map_fork(parent, child);
	api_copy_handles(child, parent);
	posix_copy_fds(child, parent);

	child->next = NULL;

	return ((s64)child->pid);
}

s64
posix_execve(u64 path_u, u64 argv_u, u64 envp_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	char		*kpath;
	char		**kargv;
	char		**kenvp;
	int		argc, envc;
	u8		*elf_buf;
	u32		elf_size;
	chainfs_file_entry_t	entry;
	u32			entry_block, entry_offset;
	u64		new_cr3, old_cr3;
	u64		entry_point;
	u64		user_stack, new_rsp, argv_addr, envp_addr;
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

	if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_EIO);
	}

	if (chainfs_find_file(kpath, &entry, &entry_block,
	    &entry_offset) != 0) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOENT);
	}

	if (entry.type == CHAINFS_TYPE_DIR) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_EISDIR);
	}

	if (entry.size == 0) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOEXEC);
	}

	elf_buf = (u8 *)kmem_calloc(entry.size, 1);
	if (!elf_buf) {
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOMEM);
	}

	{
		u32	bytes_read;
		bytes_read = 0;
		if (chainfs_read_file(kpath, elf_buf, entry.size,
		    &bytes_read) != 0 || bytes_read != entry.size) {
			kmem_free(elf_buf);
			free_string_array(kargv);
			free_string_array(kenvp);
			kmem_free(kpath);
			return (-POSIX_EIO);
		}
	}
	elf_size = entry.size;

	new_cr3 = pmap_create();
	if (!new_cr3) {
		kmem_free(elf_buf);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOMEM);
	}

	old_cr3 = pmap_get_cr3();
	pmap_load(new_cr3);

	entry_point = elf_load(elf_buf, elf_size);
	kmem_free(elf_buf);
	if (entry_point == 0) {
		pmap_load(old_cr3);
		pmap_destroy(new_cr3);
		free_string_array(kargv);
		free_string_array(kenvp);
		kmem_free(kpath);
		return (-POSIX_ENOEXEC);
	}

	user_stack = allocate_execve_stack();
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
		    &new_rsp, &argv_addr, &envp_addr);
		if (err < 0) {
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

	pmap_load(old_cr3);

	if (proc->owns_address_space && proc->cr3) {
		vm_map_free_all(proc);
		pmap_destroy(proc->cr3);
	}

	proc->cr3 = new_cr3;
	proc->entry_point = entry_point;
	proc->user_stack = user_stack;
	proc->owns_address_space = 1;
	proc->mmap_base = MMAP_BASE;
	proc->kusr_auth = 0;

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

	pmap_load(new_cr3);

	com1_printf("[EXECVE] PID %d now running '%s' entry=%p\n",
	    proc->pid, proc->name, (void *)entry_point);

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

	if (sig == 9 || sig == 15) {
		return ((s64)process_send_signal(pid, sig));
	}

	target->sigpending |= (1ULL << (sig - 1));
	event_notify_signal(pid, sig);

	return (0);
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

	strcpy(buf->sysname, "otsos2");
	strcpy(buf->nodename, "localhost");
	strcpy(buf->release, "2.3.3");
	strcpy(buf->version, "otsos2-kernel-rev2");
	strcpy(buf->machine, "x86_64");
	strcpy(buf->domainname, "localdomain");

	return (0);
}

s64
posix_nanosleep(u64 req_u, u64 rem_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct {
		s64	tv_sec;
		s64	tv_nsec;
	} *req;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)rem_u; (void)regs;

	if (!is_user_address((void *)req_u, 16)) {
		return (-POSIX_EFAULT);
	}

	req = (void *)req_u;
	if (req->tv_sec > 0 || req->tv_nsec > 0) {
		extern void proc_sleep(void *channel);
		static volatile int	nanosleep_channel;
		proc_sleep((void *)&nanosleep_channel);
	}

	return (0);
}
