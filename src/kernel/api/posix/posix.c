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
#include <kernel/api/posix/posix_socket.h>
#include <kernel/api/signal.h>
#include <kernel/console/terminal.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static u32	posix_debug_last_pid;
static int	posix_debug_syscalls_left;

#define POSIX_REG_R8		0
#define POSIX_REG_R9		1
#define POSIX_REG_R10		2
#define POSIX_REG_R11		3
#define POSIX_REG_R12		4
#define POSIX_REG_R13		5
#define POSIX_REG_R14		6
#define POSIX_REG_R15		7
#define POSIX_REG_RDI		8
#define POSIX_REG_RSI		9
#define POSIX_REG_RBP		10
#define POSIX_REG_RBX		11
#define POSIX_REG_RDX		12
#define POSIX_REG_RAX		13
#define POSIX_REG_RCX		14
#define POSIX_REG_RSP		15
#define POSIX_REG_RIP		16
#define POSIX_REG_EFL		17
#define POSIX_REG_CSGSFS	18
#define POSIX_REG_ERR		19
#define POSIX_REG_TRAPNO	20
#define POSIX_REG_OLDMASK	21

static int
posix_debug_python_proc(struct process *proc)
{
	if (!proc) {
		return (0);
	}
	return (strcmp(proc->name, "python") == 0);
}

static const char *
posix_debug_syscall_name(u64 num)
{
	switch (num) {
	case SYS_read:
		return ("read");
	case SYS_write:
		return ("write");
	case SYS_open:
		return ("open");
	case SYS_openat:
		return ("openat");
	case SYS_close:
		return ("close");
	case SYS_stat:
		return ("stat");
	case SYS_fstat:
		return ("fstat");
	case SYS_lstat:
		return ("lstat");
	case SYS_poll:
		return ("poll");
	case SYS_select:
		return ("select");
	case SYS_lseek:
		return ("lseek");
	case SYS_mmap:
		return ("mmap");
	case SYS_mremap:
		return ("mremap");
	case SYS_mprotect:
		return ("mprotect");
	case SYS_munmap:
		return ("munmap");
	case SYS_brk:
		return ("brk");
	case SYS_rt_sigaction:
		return ("rt_sigaction");
	case SYS_rt_sigprocmask:
		return ("rt_sigprocmask");
	case SYS_ioctl:
		return ("ioctl");
	case SYS_access:
		return ("access");
	case SYS_fcntl:
		return ("fcntl");
	case SYS_getcwd:
		return ("getcwd");
	case SYS_getuid:
		return ("getuid");
	case SYS_geteuid:
		return ("geteuid");
	case SYS_getgid:
		return ("getgid");
	case SYS_getegid:
		return ("getegid");
	case SYS_getdents64:
		return ("getdents64");
	case SYS_futex:
		return ("futex");
	case SYS_getrandom:
		return ("getrandom");
	case SYS_clock_gettime:
		return ("clock_gettime");
	case SYS_pselect6:
		return ("pselect6");
	default:
		return ("?");
	}
}

static void
posix_debug_syscall_trace(struct process *proc, u64 num, s64 ret,
    u64 a1, u64 a2, u64 a3)
{
	if (!posix_debug_python_proc(proc)) {
		return;
	}
	if (posix_debug_last_pid != proc->pid) {
		posix_debug_last_pid = proc->pid;
		posix_debug_syscalls_left = 512;
		printk("[PYSYS] trace start pid=%d\n", (int)proc->pid);
	}
	if (posix_debug_syscalls_left <= 0) {
		return;
	}
	posix_debug_syscalls_left--;
	printk("[PYSYS] %s(%d) ret=%d a1=%x a2=%x a3=%x left=%d\n",
	    posix_debug_syscall_name(num), (int)num, (int)ret,
	    (u32)a1, (u32)a2, (u32)a3, posix_debug_syscalls_left);
}

s64	posix_read(u64 fd, u64 buf, u64 count, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_write(u64 fd, u64 buf, u64 count, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_readv(u64 fd, u64 iov, u64 iovcnt, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_writev(u64 fd, u64 iov, u64 iovcnt, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_open(u64 path, u64 flags, u64 mode, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_openat(u64 dirfd, u64 path, u64 flags, u64 mode, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_close(u64 fd, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_stat(u64 path, u64 buf, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_fstat(u64 fd, u64 buf, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_lstat(u64 path, u64 buf, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_poll(u64 fds, u64 nfds, u64 timeout, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_select(u64 nfds, u64 readfds, u64 writefds, u64 exceptfds,
	    u64 timeout, u64 a6, registers_t *regs);
s64	posix_pselect6(u64 nfds, u64 readfds, u64 writefds, u64 exceptfds,
	    u64 timeout, u64 sigmask, registers_t *regs);
s64	posix_lseek(u64 fd, u64 offset, u64 whence, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_mmap(u64 addr, u64 length, u64 prot, u64 flags, u64 fd,
	    u64 offset, registers_t *regs);
s64	posix_mremap(u64 old_addr, u64 old_size, u64 new_size, u64 flags,
	    u64 new_addr, u64 a6, registers_t *regs);
s64	posix_mprotect(u64 addr, u64 length, u64 prot, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_munmap(u64 addr, u64 length, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_brk(u64 addr, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_rt_sigaction(u64 signum, u64 act, u64 oldact, u64 sigsetsize,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_rt_sigprocmask(u64 how, u64 set, u64 oldset, u64 sigsetsize,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_rt_sigreturn(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_ioctl(u64 fd, u64 cmd, u64 arg, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_pread64(u64 fd, u64 buf, u64 count, u64 pos, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_pwrite64(u64 fd, u64 buf, u64 count, u64 pos, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_access(u64 path, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_pipe(u64 pipefd, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_pipe2(u64 pipefd, u64 flags, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_shmget(u64 key, u64 size, u64 shmflg, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_shmat(u64 shmid, u64 shmaddr, u64 shmflg, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_shmdt(u64 shmaddr, u64 a2, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_shmctl(u64 shmid, u64 cmd, u64 buf, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_dup(u64 fd, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_dup2(u64 oldfd, u64 newfd, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_dup3(u64 oldfd, u64 newfd, u64 flags, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_nanosleep(u64 req, u64 rem, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_fork(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_execve(u64 path, u64 argv, u64 envp, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_exit(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_wait4(u64 pid, u64 status, u64 options, u64 rusage,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_kill(u64 pid, u64 sig, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_uname(u64 buf, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_fcntl(u64 fd, u64 cmd, u64 arg, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_flock(u64 fd, u64 op, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_mount(u64 source, u64 target, u64 fstype, u64 flags, u64 data,
	    u64 a6, registers_t *regs);
s64	posix_umount2(u64 target, u64 flags, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_getcwd(u64 buf, u64 size, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_chdir(u64 path, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_rename(u64 oldpath, u64 newpath, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_mkdir(u64 path, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_rmdir(u64 path, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_creat(u64 path, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_link(u64 oldpath, u64 newpath, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_unlink(u64 path, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_symlink(u64 target, u64 linkpath, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_readlink(u64 path, u64 buf, u64 bufsize, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_chmod(u64 path, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_fchmod(u64 fd, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_getpid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_getppid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
	    registers_t *regs);
s64	posix_getuid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_setuid(u64 uid, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_getgid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_setgid(u64 gid, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_geteuid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_getegid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_getdents64(u64 fd, u64 buf, u64 count, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_clone(u64 flags, u64 stack, u64 ptid, u64 ctid, u64 tls,
    u64 a6, registers_t *regs);
s64	posix_gettid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_futex(u64 uaddr, u64 op, u64 val, u64 timeout, u64 uaddr2,
    u64 val3, registers_t *regs);
s64	posix_set_tid_address(u64 tidptr, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_exit_group(u64 code, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs);
s64	posix_arch_prctl(u64 code, u64 addr, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_socket(u64 domain, u64 type, u64 protocol, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_bind(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_listen(u64 sockfd, u64 backlog, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_accept(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_accept4(u64 sockfd, u64 addr, u64 addrlen, u64 flags, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_connect(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_sendto(u64 sockfd, u64 buf, u64 len, u64 flags, u64 dest_addr,
    u64 addrlen, registers_t *regs);
s64	posix_recvfrom(u64 sockfd, u64 buf, u64 len, u64 flags, u64 src_addr,
    u64 addrlen, registers_t *regs);
s64	posix_sendmsg(u64 sockfd, u64 msg, u64 flags, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_recvmsg(u64 sockfd, u64 msg, u64 flags, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_shutdown(u64 sockfd, u64 how, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_getsockname(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_getpeername(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_socketpair(u64 domain, u64 type, u64 protocol, u64 sv, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_setsockopt(u64 sockfd, u64 level, u64 optname, u64 optval,
    u64 optlen, u64 a6, registers_t *regs);
s64	posix_getsockopt(u64 sockfd, u64 level, u64 optname, u64 optval,
    u64 optlen, u64 a6, registers_t *regs);

void
posix_syscall_handler(registers_t *regs)
{
	u64	num;
	u64	a1, a2, a3, a4, a5, a6;
	s64	ret;

	if (!regs) {
		return;
	}

	num = regs->rax;
	a1 = regs->rdi;
	a2 = regs->rsi;
	a3 = regs->rdx;
	a4 = regs->r10;
	a5 = regs->r8;
	a6 = regs->r9;

	ret = -POSIX_ENOSYS;

	switch (num) {
	case SYS_read:
		ret = posix_read(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_write:
		ret = posix_write(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_readv:
		ret = posix_readv(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_writev:
		ret = posix_writev(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_open:
		ret = posix_open(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_openat:
		ret = posix_openat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_close:
		ret = posix_close(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_stat:
		ret = posix_stat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_fstat:
		ret = posix_fstat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_lstat:
		ret = posix_lstat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_poll:
		ret = posix_poll(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_select:
		ret = posix_select(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_pselect6:
		ret = posix_pselect6(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_lseek:
		ret = posix_lseek(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_mmap:
		ret = posix_mmap(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_mremap:
		ret = posix_mremap(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_mprotect:
		ret = posix_mprotect(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_munmap:
		ret = posix_munmap(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_brk:
		ret = posix_brk(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_rt_sigaction:
		ret = posix_rt_sigaction(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_rt_sigprocmask:
		ret = posix_rt_sigprocmask(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_rt_sigreturn:
		ret = posix_rt_sigreturn(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_ioctl:
		ret = posix_ioctl(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_pread64:
		ret = posix_pread64(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_pwrite64:
		ret = posix_pwrite64(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_access:
		ret = posix_access(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_pipe:
		ret = posix_pipe(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_pipe2:
		ret = posix_pipe2(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_shmget:
		ret = posix_shmget(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_shmat:
		ret = posix_shmat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_shmctl:
		ret = posix_shmctl(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_shmdt:
		ret = posix_shmdt(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_dup:
		ret = posix_dup(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_dup2:
		ret = posix_dup2(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_dup3:
		ret = posix_dup3(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_nanosleep:
		ret = posix_nanosleep(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_time:
		ret = posix_time(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_gettimeofday:
		ret = posix_gettimeofday(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_clock_gettime:
		ret = posix_clock_gettime(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_clock_nanosleep:
		ret = posix_clock_nanosleep(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_times:
		ret = posix_times(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getpid:
		ret = posix_getpid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_fork:
		ret = posix_fork(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_execve:
		ret = posix_execve(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_exit:
		ret = posix_exit(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_wait4:
		ret = posix_wait4(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_kill:
		ret = posix_kill(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_uname:
		ret = posix_uname(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_fcntl:
		ret = posix_fcntl(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_flock:
		ret = posix_flock(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_mount:
		ret = posix_mount(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_umount2:
		ret = posix_umount2(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getcwd:
		ret = posix_getcwd(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_chdir:
		ret = posix_chdir(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_rename:
		ret = posix_rename(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_mkdir:
		ret = posix_mkdir(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_rmdir:
		ret = posix_rmdir(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_creat:
		ret = posix_creat(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_link:
		ret = posix_link(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_unlink:
		ret = posix_unlink(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_symlink:
		ret = posix_symlink(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_readlink:
		ret = posix_readlink(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_chmod:
		ret = posix_chmod(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_fchmod:
		ret = posix_fchmod(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getppid:
		ret = posix_getppid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getuid:
		ret = posix_getuid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_setuid:
		ret = posix_setuid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getgid:
		ret = posix_getgid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_setgid:
		ret = posix_setgid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_geteuid:
		ret = posix_geteuid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getegid:
		ret = posix_getegid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_setpgid:
		ret = posix_setpgid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getpgrp:
		ret = posix_getpgrp(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getsid:
		ret = posix_getsid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_setsid:
		ret = posix_setsid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getpgid:
		ret = posix_getpgid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getdents64:
		ret = posix_getdents64(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_clone:
		ret = posix_clone(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_gettid:
		ret = posix_gettid(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_futex:
		ret = posix_futex(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_set_tid_address:
		ret = posix_set_tid_address(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_exit_group:
		ret = posix_exit_group(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_arch_prctl:
		ret = posix_arch_prctl(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getrandom:
		ret = posix_getrandom(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_socket:
		ret = posix_socket(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_bind:
		ret = posix_bind(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_listen:
		ret = posix_listen(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_accept:
		ret = posix_accept(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_accept4:
		ret = posix_accept4(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_connect:
		ret = posix_connect(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_sendto:
		ret = posix_sendto(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_recvfrom:
		ret = posix_recvfrom(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_sendmsg:
		ret = posix_sendmsg(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_recvmsg:
		ret = posix_recvmsg(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_shutdown:
		ret = posix_shutdown(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getsockname:
		ret = posix_getsockname(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getpeername:
		ret = posix_getpeername(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_socketpair:
		ret = posix_socketpair(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_setsockopt:
		ret = posix_setsockopt(a1, a2, a3, a4, a5, a6, regs);
		break;
	case SYS_getsockopt:
		ret = posix_getsockopt(a1, a2, a3, a4, a5, a6, regs);
		break;
	default:
		printk("[POSIX] unknown syscall: %d\n", (int)num);
		ret = -POSIX_ENOSYS;
		break;
	}

	posix_debug_syscall_trace(process_current(), num, ret, a1, a2, a3);
	regs->rax = (u64)ret;
}

void
posix_init_process(struct process *proc)
{
	int	i;
	int	tty;

	if (!proc) {
		return;
	}

	for (i = 0; i < MAX_POSIX_FDS; i++) {
		proc->posix_fds[i].used = 0;
		proc->posix_fds[i].cloexec = 0;
		proc->posix_fds[i].flags = 0;
		proc->posix_fds[i].offset = 0;
		proc->posix_fds[i].vnode = NULL;
	}

	for (i = 0; i < MAX_POSIX_SIGS; i++) {
		proc->sigaction[i].handler = 0;
		proc->sigaction[i].mask = 0;
		proc->sigaction[i].flags = 0;
		proc->sigaction[i].restorer = 0;
	}

	proc->sigmask = 0;
	proc->sigpending = 0;
	proc->brk = 0;
	proc->personality = 0;
	proc->sid = proc->pid;
	proc->pgid = proc->pid;
	proc->is_session_leader = 1;
	tty = terminal_get_active();
	proc->controlling_tty = tty;

	terminal_set_session(tty, proc->sid);
	terminal_set_pgrp(tty, proc->pgid);
}

void
posix_copy_fds(struct process *dst, const struct process *src)
{
	int	i;

	if (!dst || !src) {
		return;
	}

	for (i = 0; i < MAX_POSIX_FDS; i++) {
		dst->posix_fds[i] = src->posix_fds[i];
		if (src->posix_fds[i].used && src->posix_fds[i].vnode) {
			if (src->posix_fds[i].vnode->type == VSOCK)
				posix_socket_hold(src->posix_fds[i].vnode);
			vnode_acquire(src->posix_fds[i].vnode);
		}
	}

	for (i = 0; i < MAX_POSIX_SIGS; i++) {
		dst->sigaction[i] = src->sigaction[i];
	}

	dst->sigmask = src->sigmask;
	dst->sigpending = 0;
	dst->brk = src->brk;
	dst->personality = src->personality;
}

void
posix_cleanup_process(struct process *proc)
{
	int	i;

	if (!proc) {
		return;
	}

	for (i = 0; i < MAX_POSIX_FDS; i++) {
		if (proc->posix_fds[i].used && proc->posix_fds[i].vnode) {
			if (proc->posix_fds[i].vnode->type == VSOCK)
				posix_socket_close(proc->posix_fds[i].vnode);
			vnode_release(proc->posix_fds[i].vnode);
		}
		proc->posix_fds[i].used = 0;
		proc->posix_fds[i].vnode = NULL;
	}

	for (i = 0; i < MAX_POSIX_SIGS; i++) {
		proc->sigaction[i].handler = 0;
	}

	proc->sigmask = 0;
	proc->sigpending = 0;
}

void
posix_setup_stdio(struct process *proc)
{
	vnode_t		*vn_in, *vn_out, *vn_err;

	if (!proc) {
		return;
	}

	if (proc->posix_fds[0].used) {
		return;
	}

	vn_in = devfs_lookup("/dev/tty");
	vn_out = devfs_lookup("/dev/tty");
	vn_err = devfs_lookup("/dev/tty");

	if (!vn_in || !vn_out || !vn_err) {
		if (vn_in) vnode_release(vn_in);
		if (vn_out) vnode_release(vn_out);
		if (vn_err) vnode_release(vn_err);
		return;
	}

	proc->posix_fds[0].used = 1;
	proc->posix_fds[0].cloexec = 0;
	proc->posix_fds[0].flags = POSIX_O_RDONLY;
	proc->posix_fds[0].offset = 0;
	proc->posix_fds[0].vnode = vn_in;

	proc->posix_fds[1].used = 1;
	proc->posix_fds[1].cloexec = 0;
	proc->posix_fds[1].flags = POSIX_O_WRONLY;
	proc->posix_fds[1].offset = 0;
	proc->posix_fds[1].vnode = vn_out;

	proc->posix_fds[2].used = 1;
	proc->posix_fds[2].cloexec = 0;
	proc->posix_fds[2].flags = POSIX_O_WRONLY;
	proc->posix_fds[2].offset = 0;
	proc->posix_fds[2].vnode = vn_err;

	if (proc->controlling_tty >= 0) {
		terminal_set_pgrp(proc->controlling_tty, proc->pgid);
	}
}

int
posix_alloc_fd(struct process *proc)
{
	int	i;

	if (!proc) {
		return (-1);
	}

	for (i = 0; i < MAX_POSIX_FDS; i++) {
		if (!proc->posix_fds[i].used) {
			return (i);
		}
	}

	return (-POSIX_EMFILE);
}

posix_fd_t *
posix_get_fd(struct process *proc, int fd)
{
	if (!proc || fd < 0 || fd >= MAX_POSIX_FDS) {
		return (NULL);
	}

	if (!proc->posix_fds[fd].used) {
		return (NULL);
	}

	return (&proc->posix_fds[fd]);
}

int
posix_signal_pending(struct process *proc)
{
	return (signal_pending(proc));
}

static u64
posix_signal_user_frame_sp(u64 rsp)
{
	rsp -= 128;
	rsp -= sizeof(posix_rt_sigframe_t);
	rsp &= ~15ULL;
	return (rsp);
}

static void
posix_signal_fill_frame(posix_rt_sigframe_t *frame,
    posix_rt_sigframe_t *user_frame, registers_t *regs, int sig, u64 oldmask)
{
	memset(frame, 0, sizeof(*frame));

	frame->info.signo = sig;
	frame->uc.mcontext.gregs[POSIX_REG_R8] = regs->r8;
	frame->uc.mcontext.gregs[POSIX_REG_R9] = regs->r9;
	frame->uc.mcontext.gregs[POSIX_REG_R10] = regs->r10;
	frame->uc.mcontext.gregs[POSIX_REG_R11] = regs->r11;
	frame->uc.mcontext.gregs[POSIX_REG_R12] = regs->r12;
	frame->uc.mcontext.gregs[POSIX_REG_R13] = regs->r13;
	frame->uc.mcontext.gregs[POSIX_REG_R14] = regs->r14;
	frame->uc.mcontext.gregs[POSIX_REG_R15] = regs->r15;
	frame->uc.mcontext.gregs[POSIX_REG_RDI] = regs->rdi;
	frame->uc.mcontext.gregs[POSIX_REG_RSI] = regs->rsi;
	frame->uc.mcontext.gregs[POSIX_REG_RBP] = regs->rbp;
	frame->uc.mcontext.gregs[POSIX_REG_RBX] = regs->rbx;
	frame->uc.mcontext.gregs[POSIX_REG_RDX] = regs->rdx;
	frame->uc.mcontext.gregs[POSIX_REG_RAX] = regs->rax;
	frame->uc.mcontext.gregs[POSIX_REG_RCX] = regs->rcx;
	frame->uc.mcontext.gregs[POSIX_REG_RSP] = regs->rsp;
	frame->uc.mcontext.gregs[POSIX_REG_RIP] = regs->rip;
	frame->uc.mcontext.gregs[POSIX_REG_EFL] = regs->rflags;
	frame->uc.mcontext.gregs[POSIX_REG_CSGSFS] = regs->cs & 0xffff;
	frame->uc.mcontext.gregs[POSIX_REG_ERR] = regs->err_code;
	frame->uc.mcontext.gregs[POSIX_REG_TRAPNO] = regs->int_no;
	frame->uc.mcontext.gregs[POSIX_REG_OLDMASK] = oldmask;
	frame->uc.mcontext.fpregs = (u64)&user_frame->uc.fpregs_mem[0];
	frame->uc.sigmask[0] = oldmask;
}

static void
posix_signal_terminate_badframe(struct process *proc)
{
	if (proc) {
		posix_cleanup_process(proc);
	}
	process_exit(128 + SIGSEGV);
}

static void
posix_signal_mask_fixup(struct process *proc)
{
	if (!proc) {
		return;
	}
	proc->sigmask &= ~(1ULL << (SIGKILL - 1));
	proc->sigmask &= ~(1ULL << (SIGSTOP - 1));
}

void
posix_signal_deliver(struct process *proc, registers_t *regs)
{
	posix_rt_sigframe_t	frame;
	posix_rt_sigframe_t	*user_frame;
	posix_sigaction_t	*act;
	u64	pending;
	u64	mask;
	u64	frame_sp;
	u64	ret_sp;
	u64	oldmask;
	u64	handler;
	u64	flags;
	u64	action_mask;
	u64	restorer;
	int	sig;
	int	dfl;

	if (!proc || !regs) {
		return;
	}

	pending = proc->sigpending & ~proc->sigmask;
	if (pending == 0) {
		return;
	}

	for (sig = 1; sig <= MAX_POSIX_SIGS; sig++) {
		mask = (1ULL << (sig - 1));
		if (pending & mask) {
			act = &proc->sigaction[sig - 1];
			handler = act->handler;
			flags = act->flags;
			action_mask = act->mask;
			restorer = act->restorer;
			if (handler == POSIX_SIG_IGN) {
				proc->sigpending &= ~mask;
				return;
			}
			if (handler == POSIX_SIG_DFL) {
				dfl = posix_signal_default(sig);
				if (dfl == SIG_DFL_TERMINATE) {
					proc->sigpending &= ~mask;
					posix_cleanup_process(proc);
					process_exit(128 + sig);
					return;
				}
				if (dfl == SIG_DFL_STOP) {
					proc->sigpending &= ~mask;
					continue;
				}
				proc->sigpending &= ~mask;
				continue;
			}

			if (restorer == 0) {
				proc->sigpending &= ~mask;
				posix_signal_terminate_badframe(proc);
				return;
			}

			frame_sp = posix_signal_user_frame_sp(regs->rsp);
			ret_sp = frame_sp - sizeof(u64);
			if (!is_user_address((void *)ret_sp,
			    sizeof(u64) + sizeof(frame))) {
				proc->sigpending &= ~mask;
				posix_signal_terminate_badframe(proc);
				return;
			}
			if (!user_range_fault_in((void *)ret_sp,
			    sizeof(u64) + sizeof(frame), 1)) {
				proc->sigpending &= ~mask;
				posix_signal_terminate_badframe(proc);
				return;
			}

			user_frame = (posix_rt_sigframe_t *)frame_sp;
			oldmask = proc->sigmask;
			posix_signal_fill_frame(&frame, user_frame, regs, sig,
			    oldmask);

			memcpy(user_frame, &frame, sizeof(frame));
			memcpy((void *)ret_sp, &restorer, sizeof(restorer));

			proc->sigmask |= action_mask;
			if (!(flags & POSIX_SA_NODEFER)) {
				proc->sigmask |= mask;
			}
			posix_signal_mask_fixup(proc);
			proc->sigpending &= ~mask;
			if (flags & POSIX_SA_RESETHAND) {
				act->handler = POSIX_SIG_DFL;
				act->flags = 0;
				act->restorer = 0;
				act->mask = 0;
			}

			if (posix_debug_python_proc(proc)) {
				printk("[PYDBG] signal deliver sig=%d "
				    "handler=%p restorer=%p rsp=%p\n", sig,
				    (void *)handler, (void *)restorer,
				    (void *)ret_sp);
			}

			regs->rip = handler;
			regs->rsp = ret_sp;
			regs->rdi = (u64)sig;
			if (flags & POSIX_SA_SIGINFO) {
				regs->rsi = (u64)&user_frame->info;
				regs->rdx = (u64)&user_frame->uc;
			} else {
				regs->rsi = 0;
				regs->rdx = 0;
			}
			return;
		}
	}
}

int
posix_signal_default(int sig)
{
	switch (sig) {
	case SIGCHLD:
	case SIGCONT:
	case SIGURG:
	case SIGWINCH:
		return (SIG_DFL_IGNORE);
	case SIGSTOP:
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		return (SIG_DFL_STOP);
	default:
		return (SIG_DFL_TERMINATE);
	}
}
