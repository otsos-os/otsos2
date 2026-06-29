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
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/fs/devtmpfs/devtmpfs.h>
#include <kernel/drivers/tty.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

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

s64
posix_open(u64 path_u, u64 flags, u64 mode, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	char		*path;
	vnode_t		*vn;
	int		fd;
	int		posix_flags;
	int		exists;

	(void)a4; (void)a5; (void)a6; (void)mode; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	posix_flags = (int)flags;

	vn = NULL;
	if (vfs_resolve(path, &vn) == 0 && vn != NULL) {
		exists = 1;
		if (vn->type == VDIR) {
			/*
			 * Directories may be opened read-only (for
			 * getdents64 / opendir).  Reject write/open
			 * attempts as EISDIR.
			 */
			if ((posix_flags & POSIX_O_WRONLY) ||
			    (posix_flags & POSIX_O_RDWR)) {
				vnode_release(vn);
				kmem_free(path);
				return (-POSIX_EISDIR);
			}
		}
	} else {
		exists = 0;
	}

	if (!exists) {
		if (!(posix_flags & POSIX_O_CREAT)) {
			kmem_free(path);
			return (-POSIX_ENOENT);
		}

		if (vfs_create_file(path) != 0) {
			kmem_free(path);
			return (-POSIX_EIO);
		}

		if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
			kmem_free(path);
			return (-POSIX_EIO);
		}
	}

	if (posix_flags & POSIX_O_TRUNC) {
		if (vn->write_fn) {
			vn->write_fn(vn, NULL, 0, 0);
		}
		vn->size = 0;
	}

	fd = posix_alloc_fd(proc);
	if (fd < 0) {
		vnode_release(vn);
		kmem_free(path);
		return ((s64)fd);
	}

	proc->posix_fds[fd].used = 1;
	proc->posix_fds[fd].cloexec =
	    (posix_flags & POSIX_O_CLOEXEC) ? 1 : 0;
	proc->posix_fds[fd].flags = posix_flags & ~(POSIX_O_CREAT |
	    POSIX_O_EXCL | POSIX_O_CLOEXEC);
	proc->posix_fds[fd].offset =
	    (posix_flags & POSIX_O_APPEND) ? vn->size : 0;
	proc->posix_fds[fd].vnode = vn;

	kmem_free(path);
	return ((s64)fd);
}

s64
posix_close(u64 fd_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (pfd->vnode) {
		vnode_release(pfd->vnode);
	}

	pfd->used = 0;
	pfd->vnode = NULL;
	pfd->offset = 0;
	pfd->flags = 0;
	pfd->cloexec = 0;

	return (0);
}

s64
posix_read(u64 fd_u, u64 buf_u, u64 count, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;
	void		*buf;
	int		n;

	(void)a4; (void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (count == 0) {
		return (0);
	}

	buf = (void *)buf_u;
	if (!is_user_address(buf, count)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL) {
		return (-POSIX_EBADF);
	}

	n = vnode_read(pfd->vnode, buf, count, pfd->offset);
	if (n < 0) {
		return (-POSIX_EIO);
	}

	pfd->offset += (u64)n;
	return ((s64)n);
}

s64
posix_write(u64 fd_u, u64 buf_u, u64 count, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;
	const void	*buf;
	int		n;

	(void)a4; (void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (count == 0) {
		return (0);
	}

	buf = (const void *)buf_u;
	if (!is_user_address(buf, count)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL) {
		return (-POSIX_EBADF);
	}

	if (pfd->flags & POSIX_O_APPEND) {
		pfd->offset = pfd->vnode->size;
	}

	n = vnode_write(pfd->vnode, buf, count, pfd->offset);
	if (n < 0) {
		return (-POSIX_EIO);
	}

	pfd->offset += (u64)n;
	return ((s64)n);
}

s64
posix_lseek(u64 fd_u, u64 offset_u, u64 whence_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;
	s64		offset;
	int		whence;
	s64		new_off;

	(void)a4; (void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (pfd->vnode == NULL) {
		return (-POSIX_EBADF);
	}

	if (pfd->vnode->type == VCHR || pfd->vnode->type == VPIPE) {
		return (-POSIX_ESPIPE);
	}

	offset = (s64)offset_u;
	whence = (int)whence_u;

	switch (whence) {
	case POSIX_SEEK_SET:
		new_off = offset;
		break;
	case POSIX_SEEK_CUR:
		new_off = (s64)pfd->offset + offset;
		break;
	case POSIX_SEEK_END:
		new_off = (s64)pfd->vnode->size + offset;
		break;
	default:
		return (-POSIX_EINVAL);
	}

	if (new_off < 0) {
		return (-POSIX_EINVAL);
	}

	pfd->offset = (u64)new_off;
	return (new_off);
}

s64
posix_pread64(u64 fd_u, u64 buf_u, u64 count, u64 pos_u, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;
	void		*buf;
	int		n;

	(void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (count == 0) {
		return (0);
	}

	buf = (void *)buf_u;
	if (!is_user_address(buf, count)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL || pfd->vnode->type == VCHR ||
	    pfd->vnode->type == VPIPE) {
		return (-POSIX_ESPIPE);
	}

	n = vnode_read(pfd->vnode, buf, count, pos_u);
	if (n < 0) {
		return (-POSIX_EIO);
	}

	return ((s64)n);
}

s64
posix_pwrite64(u64 fd_u, u64 buf_u, u64 count, u64 pos_u, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd;
	const void	*buf;
	int		n;

	(void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (count == 0) {
		return (0);
	}

	buf = (const void *)buf_u;
	if (!is_user_address(buf, count)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL || pfd->vnode->type == VCHR ||
	    pfd->vnode->type == VPIPE) {
		return (-POSIX_ESPIPE);
	}

	n = vnode_write(pfd->vnode, buf, count, pos_u);
	if (n < 0) {
		return (-POSIX_EIO);
	}

	return ((s64)n);
}

s64
posix_stat(u64 path_u, u64 buf_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char		*path;
	vnode_t		*vn;
	posix_stat_t	*st;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	if (!is_user_address((void *)buf_u, sizeof(posix_stat_t))) {
		kmem_free(path);
		return (-POSIX_EFAULT);
	}

	if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
		kmem_free(path);
		return (-POSIX_ENOENT);
	}

	st = (posix_stat_t *)buf_u;
	if (vnode_stat(vn, st) != 0) {
		vnode_release(vn);
		kmem_free(path);
		return (-POSIX_EIO);
	}

	vnode_release(vn);
	kmem_free(path);
	return (0);
}

s64
posix_fstat(u64 fd_u, u64 buf_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	posix_stat_t	*st;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, (int)fd_u);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (!is_user_address((void *)buf_u, sizeof(posix_stat_t))) {
		return (-POSIX_EFAULT);
	}

	st = (posix_stat_t *)buf_u;
	if (vnode_stat(pfd->vnode, st) != 0) {
		return (-POSIX_EIO);
	}

	return (0);
}

s64
posix_lstat(u64 path_u, u64 buf_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	return (posix_stat(path_u, buf_u, a3, a4, a5, a6, regs));
}

s64	posix_pipe2(u64 pipefd_u, u64 flags, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs);

s64
posix_pipe(u64 pipefd_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	return (posix_pipe2(pipefd_u, 0, a3, a4, a5, a6, regs));
}

s64
posix_pipe2(u64 pipefd_u, u64 flags_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	pipe_t		*p;
	vnode_t		*vn_read, *vn_write;
	int		fd_read, fd_write;
	int		*pipefd;
	int		flags;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pipefd = (int *)pipefd_u;
	if (!is_user_address(pipefd, sizeof(int) * 2)) {
		return (-POSIX_EFAULT);
	}

	flags = (int)flags_u;

	p = (pipe_t *)kmem_alloc(sizeof(pipe_t));
	if (!p) {
		return (-POSIX_ENOMEM);
	}

	memset(p, 0, sizeof(pipe_t));
	p->readers = 1;
	p->writers = 1;

	vn_read = vnode_alloc(VPIPE, "pipe");
	vn_write = vnode_alloc(VPIPE, "pipe");
	if (!vn_read || !vn_write) {
		if (vn_read) vnode_release(vn_read);
		if (vn_write) vnode_release(vn_write);
		kmem_free(p);
		return (-POSIX_ENOMEM);
	}

	vn_read->data = p;
	vn_write->data = p;
	vn_read->mode = POSIX_S_IFIFO | 0400;
	vn_write->mode = POSIX_S_IFIFO | 0200;

	fd_read = posix_alloc_fd(proc);
	if (fd_read < 0) {
		vnode_release(vn_read);
		vnode_release(vn_write);
		kmem_free(p);
		return ((s64)fd_read);
	}

	proc->posix_fds[fd_read].used = 1;
	proc->posix_fds[fd_read].cloexec = (flags & POSIX_O_CLOEXEC) ? 1 : 0;
	proc->posix_fds[fd_read].flags = POSIX_O_RDONLY;
	proc->posix_fds[fd_read].offset = 0;
	proc->posix_fds[fd_read].vnode = vn_read;

	fd_write = posix_alloc_fd(proc);
	if (fd_write < 0) {
		vnode_release(vn_read);
		vnode_release(vn_write);
		proc->posix_fds[fd_read].used = 0;
		kmem_free(p);
		return ((s64)fd_write);
	}

	proc->posix_fds[fd_write].used = 1;
	proc->posix_fds[fd_write].cloexec =
	    (flags & POSIX_O_CLOEXEC) ? 1 : 0;
	proc->posix_fds[fd_write].flags = POSIX_O_WRONLY;
	proc->posix_fds[fd_write].offset = 0;
	proc->posix_fds[fd_write].vnode = vn_write;

	pipefd[0] = fd_read;
	pipefd[1] = fd_write;

	return (0);
}

s64
posix_dup(u64 fd_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		new_fd;

	(void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, (int)fd_u);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	new_fd = posix_alloc_fd(proc);
	if (new_fd < 0) {
		return ((s64)new_fd);
	}

	proc->posix_fds[new_fd] = *pfd;
	proc->posix_fds[new_fd].cloexec = 0;
	if (pfd->vnode) {
		vnode_acquire(pfd->vnode);
	}

	return ((s64)new_fd);
}

s64
posix_dup2(u64 oldfd_u, u64 newfd_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*old_pfd, *new_pfd;
	int		oldfd, newfd;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	oldfd = (int)oldfd_u;
	newfd = (int)newfd_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	old_pfd = posix_get_fd(proc, oldfd);
	if (!old_pfd) {
		return (-POSIX_EBADF);
	}

	if (oldfd == newfd) {
		return ((s64)newfd);
	}

	if (newfd < 0 || newfd >= MAX_POSIX_FDS) {
		return (-POSIX_EBADF);
	}

	new_pfd = &proc->posix_fds[newfd];
	if (new_pfd->used) {
		if (new_pfd->vnode) {
			vnode_release(new_pfd->vnode);
		}
	}

	*new_pfd = *old_pfd;
	new_pfd->cloexec = 0;
	if (old_pfd->vnode) {
		vnode_acquire(old_pfd->vnode);
	}

	return ((s64)newfd);
}

s64
posix_dup3(u64 oldfd_u, u64 newfd_u, u64 flags_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*old_pfd, *new_pfd;
	int		oldfd, newfd, flags;

	(void)a4; (void)a5; (void)a6; (void)regs;

	oldfd = (int)oldfd_u;
	newfd = (int)newfd_u;
	flags = (int)flags_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (oldfd == newfd) {
		return (-POSIX_EINVAL);
	}

	old_pfd = posix_get_fd(proc, oldfd);
	if (!old_pfd) {
		return (-POSIX_EBADF);
	}

	if (newfd < 0 || newfd >= MAX_POSIX_FDS) {
		return (-POSIX_EBADF);
	}

	new_pfd = &proc->posix_fds[newfd];
	if (new_pfd->used) {
		if (new_pfd->vnode) {
			vnode_release(new_pfd->vnode);
		}
	}

	*new_pfd = *old_pfd;
	new_pfd->cloexec = (flags & POSIX_O_CLOEXEC) ? 1 : 0;
	if (old_pfd->vnode) {
		vnode_acquire(old_pfd->vnode);
	}

	return ((s64)newfd);
}

s64
posix_fcntl(u64 fd_u, u64 cmd_u, u64 arg_u, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	int		fd, cmd, arg, new_fd;

	(void)a4; (void)a5; (void)a6; (void)regs;

	fd = (int)fd_u;
	cmd = (int)cmd_u;
	arg = (int)arg_u;
	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	switch (cmd) {
	case POSIX_F_DUPFD:
		new_fd = posix_alloc_fd(proc);
		if (new_fd < 0) {
			return ((s64)new_fd);
		}
		proc->posix_fds[new_fd] = *pfd;
		proc->posix_fds[new_fd].cloexec = 0;
		if (pfd->vnode) {
			vnode_acquire(pfd->vnode);
		}
		return ((s64)new_fd);

	case POSIX_F_GETFD:
		return ((s64)(pfd->cloexec ? POSIX_FD_CLOEXEC : 0));

	case POSIX_F_SETFD:
		pfd->cloexec = (arg & POSIX_FD_CLOEXEC) ? 1 : 0;
		return (0);

	case POSIX_F_GETFL:
		return ((s64)pfd->flags);

	case POSIX_F_SETFL:
		pfd->flags = (pfd->flags & ~(POSIX_O_NONBLOCK |
		    POSIX_O_APPEND)) | (arg & (POSIX_O_NONBLOCK |
		    POSIX_O_APPEND));
		return (0);

	default:
		return (-POSIX_EINVAL);
	}
}

s64
posix_ioctl(u64 fd_u, u64 cmd_u, u64 arg_u, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;

	(void)a4; (void)a5; (void)a6; (void)arg_u; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, (int)fd_u);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	switch ((int)cmd_u) {
	case POSIX_TIOCGWINSZ:
		return (-POSIX_ENOTTY);
	case POSIX_TIOCSWINSZ:
		return (0);
	case POSIX_TCGETS:
	case POSIX_TCSETS:
		return (0);
	default:
		return (-POSIX_ENOTTY);
	}
}

s64
posix_access(u64 path_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char		*path;
	vnode_t		*vn;

	(void)mode; (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	if (vfs_resolve(path, &vn) != 0 || vn == NULL) {
		kmem_free(path);
		return (-POSIX_ENOENT);
	}

	vnode_release(vn);
	kmem_free(path);
	return (0);
}

s64
posix_creat(u64 path_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	return (posix_open(path_u,
	    POSIX_O_WRONLY | POSIX_O_CREAT | POSIX_O_TRUNC, mode,
	    a4, a5, a6, regs));
}

s64
posix_getdents64(u64 fd_u, u64 buf_u, u64 count, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	u8		*buf;
	u32		idx;
	u64		pos;
	int		type;
	char		name[32];

	(void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	pfd = posix_get_fd(proc, (int)fd_u);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (!pfd->vnode || pfd->vnode->type != VDIR) {
		return (-POSIX_ENOTDIR);
	}

	if (!is_user_address((void *)buf_u, count)) {
		return (-POSIX_EFAULT);
	}

	buf = (u8 *)buf_u;
	pos = 0;
	idx = 0;

	while (pos + sizeof(posix_dirent64_t) <= count) {
		memset(name, 0, sizeof(name));
		int	ret;

		type = POSIX_DT_UNKNOWN;
		ret = vnode_readdir(pfd->vnode, idx, name, &type);
		if (ret <= 0) {
			break;
		}

		posix_dirent64_t	*de;
		u32			name_len;
		u16			reclen;

		name_len = strlen(name);
		reclen = (u16)((sizeof(posix_dirent64_t) - 256 +
		    name_len + 1 + 7) & ~7);

		if (pos + reclen > count) {
			break;
		}

		de = (posix_dirent64_t *)(buf + pos);
		de->d_ino = idx + 1;
		de->d_off = (s64)(pos + reclen);
		de->d_reclen = reclen;
		de->d_type = (u8)type;
		memcpy(de->d_name, name, name_len);
		de->d_name[name_len] = '\0';

		pos += reclen;
		idx++;
	}

	return ((s64)pos);
}

s64
posix_flock(u64 fd, u64 op, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	(void)fd; (void)op; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;
	return (0);
}
