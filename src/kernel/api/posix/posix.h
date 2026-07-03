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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s32 as 32 bit signed
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type posix_fd_t as struct with used, cloexec, flags, offset, vnode
$define %type posix_sigaction_t as struct with handler, mask, flags, restorer
$define %type posix_stat_t as packed struct with POSIX stat fields
$define %type posix_utsname_t as struct with uname fields
$define %type registers_t as struct with CPU register snapshot
$define %type vnode_t as struct with VFS vnode

$define %func posix_syscall_handler as procedure with args registers_t *
$define %func posix_init_process as procedure with args struct process *
$define %func posix_copy_fds as procedure with args struct process *, struct process *
$define %func posix_cleanup_process as procedure with args struct process *
$define %func posix_alloc_fd as function with args struct process *
$define %func posix_get_fd as function with args struct process *, int
$define %func posix_signal_pending as function with args struct process *
$define %func posix_signal_deliver as procedure with args struct process *, registers_t *

*/

/* !SPACE!

$space %export posix_syscall_handler
$space %export posix_init_process, posix_copy_fds, posix_cleanup_process
$space %export posix_alloc_fd, posix_get_fd
$space %export posix_signal_pending, posix_signal_deliver

*/

#ifndef POSIX_H
#define POSIX_H

#include <kernel/interrupts/idt.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/thread.h>
#include <kernel/time.h>
#include <mlibc/mlibc.h>

#define POSIX_ENOEXEC		8
#define POSIX_EBADF		9
#define POSIX_ECHILD		10
#define POSIX_EAGAIN		11
#define POSIX_ENOMEM		12
#define POSIX_EACCES		13
#define POSIX_EFAULT		14
#define POSIX_EBUSY		16
#define POSIX_EEXIST		17
#define POSIX_EXDEV		18
#define POSIX_ENODEV		19
#define POSIX_ENOTDIR		20
#define POSIX_EISDIR		21
#define POSIX_EINVAL		22
#define POSIX_EMFILE		24
#define POSIX_ENOTTY		25
#define POSIX_ENOSPC		28
#define POSIX_ESPIPE		29
#define POSIX_EROFS		30
#define POSIX_EPIPE		32
#define POSIX_ENOSYS		38
#define POSIX_ENOTEMPTY		39
#define POSIX_ELOOP		40
#define POSIX_ENAMETOOLONG	36
#define POSIX_ERANGE		34
#define POSIX_EOVERFLOW		75
#define POSIX_ESRCH		3
#define POSIX_EINTR		4
#define POSIX_EIO		5
#define POSIX_EPERM		1
#define POSIX_ENOENT		2

#define MAX_POSIX_FDS		256
#define MAX_POSIX_SIGS		64
#define POSIX_SIG_BLOCK		0
#define POSIX_SIG_UNBLOCK	1
#define POSIX_SIG_SETMASK	2

#define POSIX_O_RDONLY		0
#define POSIX_O_WRONLY		1
#define POSIX_O_RDWR		2
#define POSIX_O_CREAT		0100
#define POSIX_O_EXCL		0200
#define POSIX_O_TRUNC		01000
#define POSIX_O_APPEND		02000
#define POSIX_O_NONBLOCK	04000
#define POSIX_O_CLOEXEC		02000000
#define POSIX_O_DIRECTORY	0200000

#define POSIX_SEEK_SET		0
#define POSIX_SEEK_CUR		1
#define POSIX_SEEK_END		2

#define POSIX_F_DUPFD		0
#define POSIX_F_GETFD		1
#define POSIX_F_SETFD		2
#define POSIX_F_GETFL		3
#define POSIX_F_SETFL		4
#define POSIX_FD_CLOEXEC	1

#define POSIX_DT_UNKNOWN	0
#define POSIX_DT_FIFO		1
#define POSIX_DT_CHR		2
#define POSIX_DT_DIR		4
#define POSIX_DT_BLK		6
#define POSIX_DT_REG		8
#define POSIX_DT_LNK		10
#define POSIX_DT_SOCK		12

#define POSIX_TIOCGWINSZ	0x5413
#define POSIX_TIOCSWINSZ	0x5414
#define POSIX_TCGETS		0x5401
#define POSIX_TCSETS		0x5402
#define POSIX_TIOCGPGRP		0x540F
#define POSIX_TIOCSPGRP		0x5410
#define POSIX_TIOCGSID		0x5429
#define	POSIX_TIOCSCTTY		0x540E
#define	POSIX_TIOCGPTN		0x80045430


#define POSIX_MAP_READ		0x1
#define POSIX_MAP_WRITE		0x2
#define POSIX_MAP_EXEC		0x4

#define POSIX_MAP_PRIVATE	0x02
#define POSIX_MAP_SHARED	0x01
#define POSIX_MAP_FIXED		0x10
#define POSIX_MAP_ANON		0x20
#define POSIX_PROT_NONE		0
#define POSIX_PROT_READ		1
#define POSIX_PROT_WRITE	2
#define POSIX_PROT_EXEC		4

#define SYS_read		0
#define SYS_write		1
#define SYS_open		2
#define SYS_close		3
#define SYS_stat		4
#define SYS_fstat		5
#define SYS_lstat		6
#define SYS_poll		7
#define SYS_lseek		8
#define SYS_mmap		9
#define SYS_readv		19
#define SYS_writev		20
#define SYS_mprotect		10
#define SYS_munmap		11
#define SYS_brk		12
#define SYS_rt_sigaction	13
#define SYS_rt_sigprocmask	14
#define SYS_rt_sigreturn	15
#define SYS_ioctl		16
#define SYS_pread64		17
#define SYS_pwrite64		18
#define SYS_access		21
#define SYS_pipe		22
#define SYS_dup			32
#define SYS_dup2		33
#define SYS_nanosleep		35
#define SYS_getpid		39
#define SYS_fork		57
#define SYS_execve		59
#define SYS_exit		60
#define SYS_wait4		61
#define SYS_kill		62
#define SYS_uname		63
#define SYS_time		201
#define SYS_gettimeofday	96
#define SYS_clock_gettime	228
#define SYS_clock_nanosleep	230
#define SYS_times		100
#define SYS_fcntl		72
#define SYS_flock		73
#define SYS_getcwd		79
#define SYS_chdir		80
#define SYS_rename		82
#define SYS_mkdir		83
#define SYS_rmdir		84
#define SYS_creat		85
#define SYS_link		86
#define SYS_unlink		87
#define SYS_readlink		89
#define SYS_chmod		90
#define SYS_fchmod		91
#define SYS_chown		92
#define SYS_getuid		102
#define SYS_getgid		103
#define SYS_geteuid		108
#define SYS_getegid		109
#define SYS_getppid		110
#define SYS_setpgid		124
#define SYS_getpgrp		125
#define SYS_getsid		126
#define SYS_setsid		127
#define SYS_getpgid		128
#define SYS_getdents64		217
#define SYS_set_tid_address	218
#define SYS_exit_group		231
#define SYS_dup3		292
#define SYS_pipe2		293

#define SYS_clone		56
#define SYS_gettid		186
#define SYS_futex		202
#define SYS_arch_prctl		158
#define POSIX_CLOCK_REALTIME	0
#define POSIX_CLOCK_MONOTONIC	1
#define POSIX_CLOCK_TIMER_ABSTIME	1

/* futex operations */
#define FUTEX_WAIT		0
#define FUTEX_WAKE		1
#define FUTEX_FD		2
#define FUTEX_REQUEUE		3
#define FUTEX_CMP_REQUEUE	4
#define FUTEX_WAKE_OP		5
#define FUTEX_LOCK_PI		6
#define FUTEX_UNLOCK_PI		7
#define FUTEX_TRYLOCK_PI	8
#define FUTEX_WAIT_BITSET	9
#define FUTEX_WAKE_BITSET	10

/* clone flags (Linux-compatible) */
#define POSIX_CLONE_VM		0x00000100
#define POSIX_CLONE_FS		0x00000200
#define POSIX_CLONE_FILES	0x00000400
#define POSIX_CLONE_SIGHAND	0x00000800
#define POSIX_CLONE_THREAD	0x00010000
#define POSIX_CLONE_PARENT_SETTID	0x00100000
#define POSIX_CLONE_CHILD_CLEARTID	0x00200000
#define POSIX_CLONE_CHILD_SETTID	0x01000000
#define POSIX_CLONE_SETTLS		0x08000000
#define POSIX_CLONE_PARENT_SETTID2	0x10000000

struct process;

typedef struct posix_fd {
	int		used;
	int		cloexec;
	int		flags;
	u64		offset;
	vnode_t		*vnode;
} posix_fd_t;

typedef struct {
	u64		handler;
	u64		mask;
	u32		flags;
	u64		restorer;
} posix_sigaction_t;

typedef struct {
	char		sysname[65];
	char		nodename[65];
	char		release[65];
	char		version[65];
	char		machine[65];
	char		domainname[65];
} posix_utsname_t;

typedef struct {
	u64		d_ino;
	s64		d_off;
	u16		d_reclen;
	u8		d_type;
	char		d_name[256];
} posix_dirent64_t;

typedef struct {
	u64		addr;
	u64		length;
	u64		prot;
	u64		flags;
	u64		fd;
	u64		offset;
} posix_mmap_args_t;

void		posix_syscall_handler(registers_t *regs);
void		posix_init_process(struct process *proc);
void		posix_copy_fds(struct process *dst, const struct process *src);
void		posix_cleanup_process(struct process *proc);
void		posix_setup_stdio(struct process *proc);
int		posix_alloc_fd(struct process *proc);
posix_fd_t	*posix_get_fd(struct process *proc, int fd);
#define SIG_DFL_IGNORE		0
#define SIG_DFL_TERMINATE	1
#define SIG_DFL_STOP		2

int		posix_signal_pending(struct process *proc);
void		posix_signal_deliver(struct process *proc,
		    registers_t *regs);
int		posix_signal_default(int sig);

s64	posix_time(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_gettimeofday(u64 tv, u64 tz, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_clock_gettime(u64 clock_id, u64 tp, u64 a3, u64 a4,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_clock_nanosleep(u64 clock_id, u64 flags, u64 req,
	    u64 rem, u64 a5, u64 a6, registers_t *regs);
s64	posix_times(u64 buf, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);

s64	posix_setpgid(u64 pid_u, u64 pgid_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_getpgrp(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_getsid(u64 pid_u, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_setsid(u64 a1, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);
s64	posix_getpgid(u64 pid_u, u64 a2, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);

#endif

