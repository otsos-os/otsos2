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
#include <kernel/api/api.h>
#include <kernel/signal.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/fs/devfs/devfs.h>
#include <kernel/console/terminal.h>
#include <kernel/console/pty.h>
#include <kernel/drivers/timer.h>
#include <kernel/other/restrict.h>
#include <kernel/process.h>
#include <kernel/thread.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static int	posix_poll_wait_channel;

#define	POSIX_GETDENTS_BATCH	16

void
posix_poll_notify(void)
{
	proc_wakeup(&posix_poll_wait_channel);
}

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

static s64
posix_vfs_ret(int ret)
{
	int	err;

	if (ret >= 0) {
		return ((s64)ret);
	}

	err = -ret;
	switch (err) {
	case API_ERR_PERM:
		return (-POSIX_EPERM);
	case API_ERR_ACCESS:
		return (-POSIX_EACCES);
	case API_ERR_NOT_FOUND:
		return (-POSIX_ENOENT);
	case API_ERR_BAD_ADDR:
		return (-POSIX_EFAULT);
	case API_ERR_NO_MEMORY:
	case API_ERR_NOMEM:
		return (-POSIX_ENOMEM);
	case API_ERR_BUSY:
		return (-POSIX_EBUSY);
	case API_ERR_EXISTS:
		return (-POSIX_EEXIST);
	case API_ERR_NODEV:
		return (-POSIX_ENODEV);
	case API_ERR_NOT_DIR:
		return (-POSIX_ENOTDIR);
	case API_ERR_IS_DIR:
		return (-POSIX_EISDIR);
	case API_ERR_NOT_SUPPORTED:
		return (-POSIX_ENOSYS);
	case API_ERR_TOO_BIG:
		return (-POSIX_ENAMETOOLONG);
	case API_ERR_FILE_TOO_BIG:
		return (-POSIX_EFBIG);
	case API_ERR_HANDLES_FULL:
		return (-POSIX_EMFILE);
	case API_ERR_OBJECTS_FULL:
		return (-POSIX_ENOSPC);
	case API_ERR_READ_ONLY:
		return (-POSIX_EROFS);
	case API_ERR_NO_SPACE:
		return (-POSIX_ENOSPC);
	case API_ERR_PIPE_CLOSED:
		return (-POSIX_EPIPE);
	case API_ERR_IO:
		return (-POSIX_EIO);
	case API_ERR_CROSS_DEVICE:
		return (-POSIX_EXDEV);
	case API_ERR_NOT_EMPTY:
		return (-POSIX_ENOTEMPTY);
	case API_ERR_BAD_VALUE:
	case API_ERR_INVAL:
	default:
		return (-POSIX_EINVAL);
	}
}

static u8
posix_dtype_from_vtype(int type)
{
	switch (type) {
	case VREG:
		return (POSIX_DT_REG);
	case VDIR:
		return (POSIX_DT_DIR);
	case VCHR:
		return (POSIX_DT_CHR);
	case VPIPE:
		return (POSIX_DT_FIFO);
	case VLNK:
		return (POSIX_DT_LNK);
	case VSOCK:
		return (POSIX_DT_SOCK);
	default:
		return (POSIX_DT_UNKNOWN);
	}
}

static int
posix_check_perm(vnode_t *vn, int access)
{
  struct process	*proc;
  int			mode;

  if (!vn || !access) {
    return (0);
  }

  proc = process_current();
  if (!proc) {
    return (-POSIX_EACCES);
  }

  if (proc->euid == 0) {
    return (0);
  }

  if (proc->euid == vn->uid) {
    mode = (vn->mode >> 6) & 7;
    if ((access & 4) && !(mode & 4)) return (-POSIX_EACCES);
    if ((access & 2) && !(mode & 2)) return (-POSIX_EACCES);
    if ((access & 1) && !(mode & 1)) return (-POSIX_EACCES);
    return (0);
  }

  if (proc->egid == vn->gid) {
    mode = (vn->mode >> 3) & 7;
    if ((access & 4) && !(mode & 4)) return (-POSIX_EACCES);
    if ((access & 2) && !(mode & 2)) return (-POSIX_EACCES);
    if ((access & 1) && !(mode & 1)) return (-POSIX_EACCES);
    return (0);
  }

  mode = vn->mode & 7;
  if ((access & 4) && !(mode & 4)) return (-POSIX_EACCES);
  if ((access & 2) && !(mode & 2)) return (-POSIX_EACCES);
  if ((access & 1) && !(mode & 1)) return (-POSIX_EACCES);
  return (0);
}

static s64
posix_do_open(const char *path, int posix_flags, u64 mode)
{
	struct process	*proc;
	vnode_t		*vn;
	int		fd;
	int		exists;
	int		perm;
	int		ret;
	int		want;

	(void)mode;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (path[0] == '\0') {
		return (-POSIX_ENOENT);
	}

	if (restrict_kusr_check(path)) {
		return (-POSIX_EACCES);
	}

	vn = NULL;
	if (posix_flags & POSIX_O_NOFOLLOW) {
		ret = vfs_resolve_nofollow(path, &vn);
	} else {
		ret = vfs_resolve(path, &vn);
	}
	if (ret == 0 && vn != NULL) {
		exists = 1;
		if ((posix_flags & POSIX_O_NOFOLLOW) &&
		    vn->type == VLNK) {
			vnode_release(vn);
			return (-POSIX_ELOOP);
		}
		if (vn->type == VDIR) {
			/*
			 * Directories may be opened read-only (for
			 * getdents64 / opendir).  Reject write/open
			 * attempts as EISDIR.
			 */
			if ((posix_flags & POSIX_O_WRONLY) ||
			    (posix_flags & POSIX_O_RDWR)) {
				vnode_release(vn);
				return (-POSIX_EISDIR);
			}
		}
	} else {
		if (ret != 0 && ret != -API_ERR_NOT_FOUND) {
			return (posix_vfs_ret(ret));
		}
		exists = 0;
	}

	if (vn != NULL && vn->type == VCHR && strcmp(vn->name, "ptmx") == 0) {
		vnode_t		*master_vn;

		vnode_release(vn);
		ret = pty_open_master(&master_vn);
		if (ret != 0) {
			return ((s64)ret);
		}
		vn = master_vn;
	}

	if (!exists) {
		if (!(posix_flags & POSIX_O_CREAT)) {
			return (-POSIX_ENOENT);
		}

		if (posix_flags & POSIX_O_DIRECTORY) {
			return (-POSIX_ENOTDIR);
		}

		ret = vfs_create_file(path);
		if (ret != 0) {
			return (posix_vfs_ret(ret));
		}

		ret = vfs_resolve(path, &vn);
		if (ret != 0) {
			return (posix_vfs_ret(ret));
		}
		if (vn == NULL) {
			return (-POSIX_EIO);
		}

		vn->uid = proc->euid;
		vn->gid = proc->egid;
	} else {
		if ((posix_flags & POSIX_O_CREAT) &&
		    (posix_flags & POSIX_O_EXCL)) {
			vnode_release(vn);
			return (-POSIX_EEXIST);
		}

		if ((posix_flags & POSIX_O_DIRECTORY) &&
		    vn->type != VDIR) {
			vnode_release(vn);
			return (-POSIX_ENOTDIR);
		}
	}

	want = 0;
	if (posix_flags & POSIX_O_RDWR) {
		want = 6;
	} else if (posix_flags & POSIX_O_WRONLY) {
		want = 2;
	} else {
		want = 4;
	}

	perm = posix_check_perm(vn, want);
	if (perm != 0) {
		vnode_release(vn);
		return ((s64)perm);
	}

	if (posix_flags & POSIX_O_TRUNC) {
		if (vn->type != VCHR) {
			ret = vfs_truncate(path, 0);
			if (ret != 0) {
				vnode_release(vn);
				return (posix_vfs_ret(ret));
			}
		}
		vn->size = 0;
	}

	fd = posix_alloc_fd(proc);
	if (fd < 0) {
		vnode_release(vn);
		return ((s64)fd);
	}

	proc->posix_fds[fd].used = 1;
	proc->posix_fds[fd].cloexec =
	    (posix_flags & POSIX_O_CLOEXEC) ? 1 : 0;
	proc->posix_fds[fd].flags = posix_flags & ~(POSIX_O_CREAT |
	    POSIX_O_EXCL | POSIX_O_CLOEXEC | POSIX_O_DIRECTORY |
	    POSIX_O_NOFOLLOW | POSIX_O_LARGEFILE);
	proc->posix_fds[fd].offset =
	    (posix_flags & POSIX_O_APPEND) ? vn->size : 0;
	proc->posix_fds[fd].vnode = vn;

	return ((s64)fd);
}

static char *
build_dir_path(vnode_t *dir_vn, const char *name)
{
	const char	*base;
	size_t		base_len;
	size_t		name_len;
	size_t		total_len;
	char		*resolved;

	if (!dir_vn || dir_vn->type != VDIR) {
		return (NULL);
	}

	if (dir_vn->data) {
		base = (const char *)dir_vn->data;
	} else if (dir_vn->readdir_fn == devfs_root_readdir) {
		base = "/dev";
	} else {
		return (NULL);
	}

	base_len = strlen(base);
	name_len = strlen(name);
	total_len = base_len + name_len + 2;
	if (total_len > 256) {
		return (NULL);
	}

	resolved = (char *)kmem_calloc(total_len, 1);
	if (!resolved) {
		return (NULL);
	}

	memcpy(resolved, base, base_len);
	if (base_len > 0 && base[base_len - 1] != '/') {
		resolved[base_len] = '/';
		memcpy(resolved + base_len + 1, name, name_len + 1);
	} else {
		memcpy(resolved + base_len, name, name_len + 1);
	}

	return (resolved);
}

s64
posix_open(u64 path_u, u64 flags, u64 mode, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char		*path;
	s64		ret;

	(void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	ret = posix_do_open(path, (int)flags, mode);
	kmem_free(path);

	return (ret);
}

s64
posix_openat(u64 dirfd_u, u64 path_u, u64 flags, u64 mode, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	char		*path;
	char		*resolved;
	s64		ret;
	int		dirfd;

	(void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	if (path[0] == '\0') {
		kmem_free(path);
		return (-POSIX_ENOENT);
	}

	dirfd = (int)dirfd_u;

	if (path[0] != '/' && dirfd != POSIX_AT_FDCWD) {
		pfd = posix_get_fd(proc, dirfd);
		if (!pfd) {
			kmem_free(path);
			return (-POSIX_EBADF);
		}

		if (!pfd->vnode || pfd->vnode->type != VDIR) {
			kmem_free(path);
			return (-POSIX_ENOTDIR);
		}

		resolved = build_dir_path(pfd->vnode, path);
		kmem_free(path);
		if (!resolved) {
			return (-POSIX_ENOTDIR);
		}

		ret = posix_do_open(resolved, (int)flags, mode);
		kmem_free(resolved);
		return (ret);
	}

	ret = posix_do_open(path, (int)flags, mode);
	kmem_free(path);
	return (ret);
}

s64
posix_close(u64 fd_u, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	pipe_t		*pipe;
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
		if (pfd->vnode->type == VPIPE && pfd->vnode->data) {
			pipe = (pipe_t *)pfd->vnode->data;
			if (pfd->flags & POSIX_O_WRONLY) {
				if (pipe->writers > 0) {
					pipe->writers--;
				}
			} else if (pipe->readers > 0) {
				pipe->readers--;
			}
			proc_wakeup((void *)pipe);
			posix_poll_notify();
		}
		if (pfd->vnode->type == VSOCK)
			posix_socket_close(pfd->vnode);
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
	pipe_t		*pipe;
	s64		ret;
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
	if (!user_range_fault_in(buf, count, 1)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL) {
		return (-POSIX_EBADF);
	}

	if (pfd->vnode->ioctl_fn == terminal_ioctl_vnode) {
		n = terminal_read_vnode(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0, 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else if (pfd->vnode->ioctl_fn == pty_master_ioctl) {
		n = pty_master_read(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else if (pfd->vnode->ioctl_fn == pty_slave_ioctl) {
		n = pty_slave_read(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else if (pfd->vnode->type == VPIPE) {
		pipe = (pipe_t *)pfd->vnode->data;
		if (!pipe) {
			return (-POSIX_EBADF);
		}
		if ((pfd->flags & POSIX_O_NONBLOCK) && pipe->size == 0 &&
		    pipe->writers > 0) {
			return (-POSIX_EAGAIN);
		}
		n = pipe_read(pipe, buf, (u32)count);
		if (n < 0) {
			ret = posix_vfs_ret(n);
			return (ret);
		}
	} else if (pfd->vnode->type == VSOCK) {
		n = posix_socket_read(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else {
		n = vnode_read(pfd->vnode, buf, count, pfd->offset);
		if (n < 0) {
			ret = posix_vfs_ret(n);
			return (ret);
		}
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
	pipe_t		*pipe;
	s64		ret;
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
	if (!user_range_fault_in(buf, count, 0)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL) {
		return (-POSIX_EBADF);
	}

	if (pfd->flags & POSIX_O_APPEND) {
		pfd->offset = pfd->vnode->size;
	}

	if (pfd->vnode->ioctl_fn == terminal_ioctl_vnode) {
		n = terminal_write_vnode(pfd->vnode, buf, (u32)count);
		if (n < 0) {
			return (-POSIX_EIO);
		}
	} else if (pfd->vnode->ioctl_fn == pty_master_ioctl) {
		n = pty_master_write(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else if (pfd->vnode->ioctl_fn == pty_slave_ioctl) {
		n = pty_slave_write(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else if (pfd->vnode->type == VPIPE) {
		pipe = (pipe_t *)pfd->vnode->data;
		if (!pipe) {
			return (-POSIX_EBADF);
		}
		if (pipe->readers == 0) {
			return (-POSIX_EPIPE);
		}
		if ((pfd->flags & POSIX_O_NONBLOCK) &&
		    pipe->size >= PIPE_BUF_SIZE) {
			return (-POSIX_EAGAIN);
		}
		n = pipe_write(pipe, buf, (u32)count);
		if (n < 0) {
			ret = posix_vfs_ret(n);
			return (ret);
		}
	} else if (pfd->vnode->type == VSOCK) {
		n = posix_socket_write(pfd->vnode, buf, (u32)count,
		    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		if (n < 0) {
			return ((s64)n);
		}
	} else {
		n = vnode_write(pfd->vnode, buf, count, pfd->offset);
		if (n < 0) {
			ret = posix_vfs_ret(n);
			return (ret);
		}
	}

	pfd->offset += (u64)n;
	return ((s64)n);
}

s64
posix_readv(u64 fd_u, u64 iov_u, u64 iovcnt_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct posix_iovec	*iov;
	u64			total;
	u64			i;
	s64			n;

	(void)a4; (void)a5; (void)a6;

	if (iovcnt_u == 0) {
		return (0);
	}
	if (iovcnt_u > 1024) {
		return (-POSIX_EINVAL);
	}

	iov = (struct posix_iovec *)iov_u;
	if (!is_user_address(iov, iovcnt_u * sizeof(struct posix_iovec))) {
		return (-POSIX_EFAULT);
	}
	if (!user_range_fault_in(iov,
	    iovcnt_u * sizeof(struct posix_iovec), 0)) {
		return (-POSIX_EFAULT);
	}

	total = 0;
	for (i = 0; i < iovcnt_u; i++) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		n = posix_read(fd_u, (u64)iov[i].iov_base,
		    iov[i].iov_len, 0, 0, 0, regs);
		if (n < 0) {
			if (total == 0) {
				return (n);
			}
			break;
		}
		total += (u64)n;
		if ((u64)n < iov[i].iov_len) {
			break;
		}
	}
	return ((s64)total);
}

s64
posix_writev(u64 fd_u, u64 iov_u, u64 iovcnt_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct posix_iovec	*iov;
	u64			total;
	u64			i;
	s64			n;

	(void)a4; (void)a5; (void)a6;

	if (iovcnt_u == 0) {
		return (0);
	}
	if (iovcnt_u > 1024) {
		return (-POSIX_EINVAL);
	}

	iov = (struct posix_iovec *)iov_u;
	if (!is_user_address(iov, iovcnt_u * sizeof(struct posix_iovec))) {
		return (-POSIX_EFAULT);
	}
	if (!user_range_fault_in(iov,
	    iovcnt_u * sizeof(struct posix_iovec), 0)) {
		return (-POSIX_EFAULT);
	}

	total = 0;
	for (i = 0; i < iovcnt_u; i++) {
		if (iov[i].iov_len == 0) {
			continue;
		}
		n = posix_write(fd_u, (u64)iov[i].iov_base,
		    iov[i].iov_len, 0, 0, 0, regs);
		if (n < 0) {
			if (total == 0) {
				return (n);
			}
			break;
		}
		total += (u64)n;
		if ((u64)n < iov[i].iov_len) {
			break;
		}
	}
	return ((s64)total);
}

static int
posix_poll_terminal_readable(vnode_t *vn)
{
	int	idx;

	idx = (int)(unsigned long)vn->data;
	if (idx == -1) {
		idx = terminal_get_active();
	}
	terminal_input_poll();
	return (terminal_read_available(idx) > 0);
}

static int
posix_poll_pipe_readable(pipe_t *p)
{
	if (!p) {
		return (0);
	}
	return (p->size > 0 || p->writers == 0);
}

static int
posix_poll_pipe_writable(pipe_t *p)
{
	if (!p) {
		return (0);
	}
	return (p->readers > 0 && p->size < PIPE_BUF_SIZE);
}

static short
posix_poll_fd_status(posix_fd_t *pfd)
{
	vnode_t		*vn;
	pipe_t		*p;
	short		 status;

	status = 0;
	vn = pfd->vnode;
	if (!vn) {
		return (POSIX_POLLNVAL);
	}

	if (vn->type == VPIPE) {
		p = (pipe_t *)vn->data;
		if (!p) {
			return (POSIX_POLLERR);
		}
		if ((pfd->flags & POSIX_O_WRONLY) && p->readers == 0) {
			status |= POSIX_POLLERR;
		}
		if (!(pfd->flags & POSIX_O_WRONLY) && p->writers == 0) {
			status |= POSIX_POLLHUP;
		}
		return (status);
	}

	if (vn->type == VSOCK) {
		return (posix_socket_fd_status(vn));
	}

	return (0);
}

static int
posix_poll_fd_readable(posix_fd_t *pfd)
{
	vnode_t		*vn;
	pipe_t		*p;

	vn = pfd->vnode;
	if (!vn) {
		return (0);
	}
	if (vn->ioctl_fn == terminal_ioctl_vnode) {
		return (posix_poll_terminal_readable(vn));
	}
	if (vn->ioctl_fn == pty_master_ioctl ||
	    vn->ioctl_fn == pty_slave_ioctl) {
		return (pty_read_available(vn) > 0);
	}
	if (vn->type == VPIPE) {
		p = (pipe_t *)vn->data;
		return (posix_poll_pipe_readable(p));
	}
	if (vn->type == VSOCK) {
		return (posix_socket_fd_readable(vn));
	}
	if (vn->type == VREG || vn->type == VDIR || vn->read_fn) {
		return (1);
	}
	return (0);
}

static int
posix_poll_fd_writable(posix_fd_t *pfd)
{
	vnode_t		*vn;
	pipe_t		*p;

	vn = pfd->vnode;
	if (!vn) {
		return (0);
	}
	if (vn->ioctl_fn == terminal_ioctl_vnode) {
		return (1);
	}
	if (vn->ioctl_fn == pty_master_ioctl ||
	    vn->ioctl_fn == pty_slave_ioctl) {
		return (pty_write_available(vn) > 0);
	}
	if (vn->type == VPIPE) {
		p = (pipe_t *)vn->data;
		return (posix_poll_pipe_writable(p));
	}
	if (vn->type == VSOCK) {
		return (posix_socket_fd_writable(vn));
	}
	if (vn->type == VREG || vn->write_fn) {
		return (1);
	}
	return (0);
}

static short
posix_poll_revents(struct process *proc, posix_pollfd_t *pollfd)
{
	posix_fd_t	*pfd;
	short		 revents;
	short		 read_events;
	short		 write_events;

	if (pollfd->fd < 0) {
		return (0);
	}

	pfd = posix_get_fd(proc, pollfd->fd);
	if (!pfd) {
		return (POSIX_POLLNVAL);
	}

	revents = posix_poll_fd_status(pfd);
	if (revents & POSIX_POLLNVAL) {
		return (revents);
	}

	read_events = POSIX_POLLIN | POSIX_POLLRDNORM;
	write_events = POSIX_POLLOUT | POSIX_POLLWRNORM;

	if ((pollfd->events & read_events) &&
	    posix_poll_fd_readable(pfd)) {
		revents |= pollfd->events & read_events;
	}
	if ((pollfd->events & write_events) &&
	    posix_poll_fd_writable(pfd)) {
		revents |= pollfd->events & write_events;
	}

	return (revents);
}

static u64
posix_poll_timeout_ticks(int timeout)
{
	u64	ticks;
	u64	freq;

	if (timeout <= 0) {
		return (0);
	}
	freq = timer_get_frequency();
	if (freq == 0) {
		return ((u64)timeout);
	}
	ticks = ((u64)timeout * freq + 999ULL) / 1000ULL;
	if (ticks == 0) {
		ticks = 1;
	}
	return (ticks);
}

s64
posix_poll(u64 fds_u, u64 nfds_u, u64 timeout_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct process		*proc;
	thread_t		*td;
	posix_pollfd_t		*fds;
	u64			 timeout_ticks;
	u64			 start_ticks;
	u64			 now_ticks;
	u64			 elapsed;
	u64			 remaining;
	int			 timeout;
	int			 nfds;
	int			 ready;
	int			 i;

	(void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	if (nfds_u > 1024) {
		return (-POSIX_EINVAL);
	}

	nfds = (int)nfds_u;
	fds = (posix_pollfd_t *)fds_u;
	if (nfds > 0) {
		if (!is_user_address(fds, sizeof(*fds) * nfds_u)) {
			return (-POSIX_EFAULT);
		}
		if (!user_range_fault_in(fds, sizeof(*fds) * nfds_u, 1)) {
			return (-POSIX_EFAULT);
		}
	} else {
		fds = NULL;
	}

	timeout = (int)timeout_u;
	if (timeout < -1) {
		return (-POSIX_EINVAL);
	}

	timeout_ticks = posix_poll_timeout_ticks(timeout);
	start_ticks = timer_get_ticks();
	td = thread_current();

	while (1) {
		ready = 0;
		for (i = 0; i < nfds; i++) {
			fds[i].revents = posix_poll_revents(proc, &fds[i]);
			if (fds[i].revents != 0) {
				ready++;
			}
		}

		if (ready > 0) {
			if (td) {
				td->sleep_target_ticks = 0;
			}
			return ((s64)ready);
		}

		if (timeout == 0) {
			return (0);
		}

		if (proc->sigpending & ~proc->sigmask) {
			if (td) {
				td->sleep_target_ticks = 0;
			}
			return (-POSIX_EINTR);
		}

		if (timeout > 0) {
			now_ticks = timer_get_ticks();
			elapsed = now_ticks - start_ticks;
			if (elapsed >= timeout_ticks) {
				if (td) {
					td->sleep_target_ticks = 0;
				}
				return (0);
			}
			remaining = timeout_ticks - elapsed;
			if (td) {
				td->sleep_target_ticks = now_ticks + remaining;
			}
		}

		proc_sleep(&posix_poll_wait_channel);
		if (td) {
			td->sleep_target_ticks = 0;
		}
	}
}

typedef struct {
	s64	tv_sec;
	s64	tv_usec;
} posix_timeval_t;

typedef struct {
	s64	tv_sec;
	s64	tv_nsec;
} posix_timespec_t;

typedef struct {
	u64	sigmask;
	u64	sigsetsize;
} posix_pselect_sigmask_t;

static int
posix_fdset_words(int nfds)
{
	return ((nfds + 63) / 64);
}

static int
posix_fdset_bytes(int nfds)
{
	return (posix_fdset_words(nfds) * (int)sizeof(u64));
}

static void
posix_fdset_zero(posix_fdset_t *set)
{
	memset(set, 0, sizeof(*set));
}

static int
posix_fdset_isset(posix_fdset_t *set, int fd)
{
	return ((set->bits[fd / 64] & (1ULL << (fd % 64))) != 0);
}

static void
posix_fdset_set(posix_fdset_t *set, int fd)
{
	set->bits[fd / 64] |= (1ULL << (fd % 64));
}

static s64
posix_fdset_copyin(u64 set_u, posix_fdset_t *set, int nfds)
{
	int	bytes;

	posix_fdset_zero(set);
	bytes = posix_fdset_bytes(nfds);
	if (set_u == 0 || bytes == 0) {
		return (0);
	}
	if (!is_user_address((void *)set_u, (size_t)bytes) ||
	    !user_range_fault_in((void *)set_u, (size_t)bytes, 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(set->bits, (void *)set_u, (size_t)bytes);
	return (0);
}

static s64
posix_fdset_copyout(u64 set_u, posix_fdset_t *set, int nfds)
{
	int	bytes;

	bytes = posix_fdset_bytes(nfds);
	if (set_u == 0 || bytes == 0) {
		return (0);
	}
	if (!is_user_address((void *)set_u, (size_t)bytes) ||
	    !user_range_fault_in((void *)set_u, (size_t)bytes, 1)) {
		return (-POSIX_EFAULT);
	}
	memcpy((void *)set_u, set->bits, (size_t)bytes);
	return (0);
}

static int
posix_select_any(posix_fdset_t *readfds, posix_fdset_t *writefds,
    posix_fdset_t *exceptfds, int fd)
{
	if (posix_fdset_isset(readfds, fd)) {
		return (1);
	}
	if (posix_fdset_isset(writefds, fd)) {
		return (1);
	}
	if (posix_fdset_isset(exceptfds, fd)) {
		return (1);
	}
	return (0);
}

static s64
posix_select_scan(struct process *proc, int nfds, posix_fdset_t *readfds,
    posix_fdset_t *writefds, posix_fdset_t *exceptfds,
    posix_fdset_t *readout, posix_fdset_t *writeout,
    posix_fdset_t *exceptout)
{
	posix_fd_t	*pfd;
	short		status;
	int		ready;
	int		fd;

	posix_fdset_zero(readout);
	posix_fdset_zero(writeout);
	posix_fdset_zero(exceptout);
	ready = 0;

	for (fd = 0; fd < nfds; fd++) {
		if (!posix_select_any(readfds, writefds, exceptfds, fd)) {
			continue;
		}

		pfd = posix_get_fd(proc, fd);
		if (!pfd) {
			return (-POSIX_EBADF);
		}

		status = posix_poll_fd_status(pfd);
		if (status & POSIX_POLLNVAL) {
			return (-POSIX_EBADF);
		}

		if (posix_fdset_isset(readfds, fd) &&
		    ((status & (POSIX_POLLERR | POSIX_POLLHUP)) ||
		    posix_poll_fd_readable(pfd))) {
			posix_fdset_set(readout, fd);
			ready++;
		}
		if (posix_fdset_isset(writefds, fd) &&
		    ((status & POSIX_POLLERR) ||
		    posix_poll_fd_writable(pfd))) {
			posix_fdset_set(writeout, fd);
			ready++;
		}
		if (posix_fdset_isset(exceptfds, fd) &&
		    (status & POSIX_POLLPRI)) {
			posix_fdset_set(exceptout, fd);
			ready++;
		}
	}

	return ((s64)ready);
}

static u64
posix_timeout_ms_to_ticks(u64 ms)
{
	u64	freq;
	u64	ticks;

	if (ms == 0) {
		return (0);
	}
	freq = timer_get_frequency();
	if (freq == 0) {
		return (ms);
	}
	if (ms > (0xffffffffffffffffULL - 999ULL) / freq) {
		return (0xffffffffffffffffULL);
	}
	ticks = (ms * freq + 999ULL) / 1000ULL;
	if (ticks == 0) {
		ticks = 1;
	}
	return (ticks);
}

static s64
posix_select_timeval_timeout(u64 timeout_u, int *has_timeout, u64 *ticks)
{
	posix_timeval_t	tv;
	u64		ms;

	*has_timeout = 0;
	*ticks = 0;
	if (timeout_u == 0) {
		return (0);
	}
	if (!is_user_address((void *)timeout_u, sizeof(tv)) ||
	    !user_range_fault_in((void *)timeout_u, sizeof(tv), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&tv, (void *)timeout_u, sizeof(tv));
	if (tv.tv_sec < 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000) {
		return (-POSIX_EINVAL);
	}
	ms = (u64)tv.tv_sec * 1000ULL;
	ms += ((u64)tv.tv_usec + 999ULL) / 1000ULL;
	*has_timeout = 1;
	*ticks = posix_timeout_ms_to_ticks(ms);
	return (0);
}

static s64
posix_select_timespec_timeout(u64 timeout_u, int *has_timeout, u64 *ticks)
{
	posix_timespec_t	ts;
	u64		ms;

	*has_timeout = 0;
	*ticks = 0;
	if (timeout_u == 0) {
		return (0);
	}
	if (!is_user_address((void *)timeout_u, sizeof(ts)) ||
	    !user_range_fault_in((void *)timeout_u, sizeof(ts), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&ts, (void *)timeout_u, sizeof(ts));
	if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
		return (-POSIX_EINVAL);
	}
	ms = (u64)ts.tv_sec * 1000ULL;
	ms += ((u64)ts.tv_nsec + 999999ULL) / 1000000ULL;
	*has_timeout = 1;
	*ticks = posix_timeout_ms_to_ticks(ms);
	return (0);
}

static s64
posix_select_wait(struct process *proc, int nfds, posix_fdset_t *readfds,
    posix_fdset_t *writefds, posix_fdset_t *exceptfds,
    posix_fdset_t *readout, posix_fdset_t *writeout,
    posix_fdset_t *exceptout, int has_timeout, u64 timeout_ticks)
{
	thread_t	*td;
	u64	start_ticks;
	u64	now_ticks;
	u64	elapsed;
	u64	remaining;
	s64	ret;

	td = thread_current();
	start_ticks = timer_get_ticks();

	while (1) {
		ret = posix_select_scan(proc, nfds, readfds, writefds,
		    exceptfds, readout, writeout, exceptout);
		if (ret != 0) {
			if (td) {
				td->sleep_target_ticks = 0;
			}
			return (ret);
		}

		if (has_timeout && timeout_ticks == 0) {
			return (0);
		}

		if (proc->sigpending & ~proc->sigmask) {
			if (td) {
				td->sleep_target_ticks = 0;
			}
			return (-POSIX_EINTR);
		}

		if (has_timeout) {
			now_ticks = timer_get_ticks();
			elapsed = now_ticks - start_ticks;
			if (elapsed >= timeout_ticks) {
				if (td) {
					td->sleep_target_ticks = 0;
				}
				return (0);
			}
			remaining = timeout_ticks - elapsed;
			if (td) {
				td->sleep_target_ticks = now_ticks + remaining;
			}
		}

		proc_sleep(&posix_poll_wait_channel);
		if (td) {
			td->sleep_target_ticks = 0;
		}
	}
}

static void
posix_select_mask_fixup(struct process *proc)
{
	if (!proc) {
		return;
	}
	proc->sigmask &= ~(1ULL << (SIGKILL - 1));
	proc->sigmask &= ~(1ULL << (SIGSTOP - 1));
}

static s64
posix_pselect_apply_mask(struct process *proc, u64 sigmask_u, u64 *oldmask,
    int *changed)
{
	posix_pselect_sigmask_t	data;
	u64			newmask;

	*changed = 0;
	*oldmask = 0;
	if (sigmask_u == 0) {
		return (0);
	}
	if (!is_user_address((void *)sigmask_u, sizeof(data)) ||
	    !user_range_fault_in((void *)sigmask_u, sizeof(data), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&data, (void *)sigmask_u, sizeof(data));
	if (data.sigmask == 0) {
		return (0);
	}
	if (data.sigsetsize != 8) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)data.sigmask, sizeof(newmask)) ||
	    !user_range_fault_in((void *)data.sigmask, sizeof(newmask), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&newmask, (void *)data.sigmask, sizeof(newmask));

	*oldmask = proc->sigmask;
	proc->sigmask = newmask;
	posix_select_mask_fixup(proc);
	*changed = 1;
	return (0);
}

static s64
posix_select_common(u64 nfds_u, u64 readfds_u, u64 writefds_u,
    u64 exceptfds_u, int has_timeout, u64 timeout_ticks)
{
	struct process	*proc;
	posix_fdset_t	readfds, writefds, exceptfds;
	posix_fdset_t	readout, writeout, exceptout;
	s64		ret;
	int		nfds;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}
	if (nfds_u > MAX_POSIX_FDS) {
		return (-POSIX_EINVAL);
	}
	nfds = (int)nfds_u;
	if (nfds < 0) {
		return (-POSIX_EINVAL);
	}

	ret = posix_fdset_copyin(readfds_u, &readfds, nfds);
	if (ret != 0) {
		return (ret);
	}
	ret = posix_fdset_copyin(writefds_u, &writefds, nfds);
	if (ret != 0) {
		return (ret);
	}
	ret = posix_fdset_copyin(exceptfds_u, &exceptfds, nfds);
	if (ret != 0) {
		return (ret);
	}

	ret = posix_select_wait(proc, nfds, &readfds, &writefds, &exceptfds,
	    &readout, &writeout, &exceptout, has_timeout, timeout_ticks);
	if (ret < 0) {
		return (ret);
	}

	if (posix_fdset_copyout(readfds_u, &readout, nfds) != 0) {
		return (-POSIX_EFAULT);
	}
	if (posix_fdset_copyout(writefds_u, &writeout, nfds) != 0) {
		return (-POSIX_EFAULT);
	}
	if (posix_fdset_copyout(exceptfds_u, &exceptout, nfds) != 0) {
		return (-POSIX_EFAULT);
	}
	return (ret);
}

s64
posix_select(u64 nfds, u64 readfds, u64 writefds, u64 exceptfds,
    u64 timeout, u64 a6, registers_t *regs)
{
	u64	timeout_ticks;
	s64	ret;
	int	has_timeout;

	(void)a6; (void)regs;

	ret = posix_select_timeval_timeout(timeout, &has_timeout,
	    &timeout_ticks);
	if (ret != 0) {
		return (ret);
	}
	return (posix_select_common(nfds, readfds, writefds, exceptfds,
	    has_timeout, timeout_ticks));
}

s64
posix_pselect6(u64 nfds, u64 readfds, u64 writefds, u64 exceptfds,
    u64 timeout, u64 sigmask, registers_t *regs)
{
	struct process	*proc;
	u64		timeout_ticks;
	u64		oldmask;
	s64		ret;
	int		has_timeout;
	int		mask_changed;

	(void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}
	ret = posix_select_timespec_timeout(timeout, &has_timeout,
	    &timeout_ticks);
	if (ret != 0) {
		return (ret);
	}
	ret = posix_pselect_apply_mask(proc, sigmask, &oldmask,
	    &mask_changed);
	if (ret != 0) {
		return (ret);
	}
	ret = posix_select_common(nfds, readfds, writefds, exceptfds,
	    has_timeout, timeout_ticks);
	if (mask_changed) {
		proc->sigmask = oldmask;
		posix_select_mask_fixup(proc);
	}
	return (ret);
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

	if ((pfd->vnode->type == VCHR &&
	    strcmp(pfd->vnode->name, "fb0") != 0) ||
	    pfd->vnode->type == VPIPE) {
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
	if (!user_range_fault_in(buf, count, 1)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL || pfd->vnode->type == VPIPE ||
	    (pfd->vnode->type == VCHR &&
	    strcmp(pfd->vnode->name, "fb0") != 0)) {
		return (-POSIX_ESPIPE);
	}

	n = vnode_read(pfd->vnode, buf, count, pos_u);
	if (n < 0) {
		return (posix_vfs_ret(n));
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
	if (!user_range_fault_in(buf, count, 0)) {
		return (-POSIX_EFAULT);
	}

	if (pfd->vnode == NULL || pfd->vnode->type == VPIPE ||
	    (pfd->vnode->type == VCHR &&
	    strcmp(pfd->vnode->name, "fb0") != 0)) {
		return (-POSIX_ESPIPE);
	}

	n = vnode_write(pfd->vnode, buf, count, pos_u);
	if (n < 0) {
		return (posix_vfs_ret(n));
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
	int		ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	if (!is_user_address((void *)buf_u, sizeof(posix_stat_t))) {
		kmem_free(path);
		return (-POSIX_EFAULT);
	}

	ret = vfs_resolve(path, &vn);
	if (ret != 0) {
		kmem_free(path);
		return (posix_vfs_ret(ret));
	}
	if (vn == NULL) {
		kmem_free(path);
		return (-POSIX_ENOENT);
	}

	st = (posix_stat_t *)buf_u;
	ret = vnode_stat(vn, st);
	if (ret != 0) {
		vnode_release(vn);
		kmem_free(path);
		return (posix_vfs_ret(ret));
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
	int		ret;

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
	ret = vnode_stat(pfd->vnode, st);
	if (ret != 0) {
		return (posix_vfs_ret(ret));
	}

	return (0);
}

s64
posix_lstat(u64 path_u, u64 buf_u, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
	char		*path;
	vnode_t		*vn;
	posix_stat_t	*st;
	int		ret;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	path = copy_user_string((const char *)path_u, 256);
	if (!path) {
		return (-POSIX_EFAULT);
	}

	if (!is_user_address((void *)buf_u, sizeof(posix_stat_t))) {
		kmem_free(path);
		return (-POSIX_EFAULT);
	}

	ret = vfs_resolve_nofollow(path, &vn);
	if (ret != 0) {
		kmem_free(path);
		return (posix_vfs_ret(ret));
	}
	if (vn == NULL) {
		kmem_free(path);
		return (-POSIX_ENOENT);
	}

	st = (posix_stat_t *)buf_u;
	ret = vnode_stat(vn, st);
	if (ret != 0) {
		vnode_release(vn);
		kmem_free(path);
		return (posix_vfs_ret(ret));
	}

	vnode_release(vn);
	kmem_free(path);
	return (0);
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
	if (!user_range_fault_in(pipefd, sizeof(int) * 2, 1)) {
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
		if (pfd->vnode->type == VSOCK)
			posix_socket_hold(pfd->vnode);
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
			if (new_pfd->vnode->type == VSOCK)
				posix_socket_close(new_pfd->vnode);
			vnode_release(new_pfd->vnode);
		}
	}

	*new_pfd = *old_pfd;
	new_pfd->cloexec = 0;
	if (old_pfd->vnode) {
		if (old_pfd->vnode->type == VSOCK)
			posix_socket_hold(old_pfd->vnode);
		vnode_acquire(old_pfd->vnode);
	}

	return ((s64)newfd);
}
static void
dup3_vsock_cleanup(vnode_t *vn)
{
	if (vn && vn->type == VSOCK)
		posix_socket_close(vn);
}
static void
dup3_vsock_hold(vnode_t *vn)
{
	if (vn && vn->type == VSOCK)
		posix_socket_hold(vn);
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
			dup3_vsock_cleanup(new_pfd->vnode);
			vnode_release(new_pfd->vnode);
		}
	}

	*new_pfd = *old_pfd;
	new_pfd->cloexec = (flags & POSIX_O_CLOEXEC) ? 1 : 0;
	if (old_pfd->vnode) {
		dup3_vsock_hold(old_pfd->vnode);
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
			if (pfd->vnode->type == VSOCK)
				posix_socket_hold(pfd->vnode);
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
		if (pfd->vnode && pfd->vnode->type == VSOCK) {
			posix_socket_set_nonblock(pfd->vnode,
			    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0);
		}
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
	s64		ret;
	int		fd;

	(void)a4; (void)a5; (void)a6; (void)regs;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	fd = (int)fd_u;
	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return (-POSIX_EBADF);
	}

	if (pfd->vnode && pfd->vnode->type == VCHR &&
	    strcmp(pfd->vnode->name, "fb0") == 0) {
		return (vnode_ioctl(pfd->vnode, cmd_u, (void *)arg_u));
	}

	switch ((int)cmd_u) {
	case POSIX_TIOCGWINSZ:
	case POSIX_TIOCSWINSZ:
	case POSIX_TCGETS:
	case POSIX_TCSETS:
	case POSIX_TCSETSW:
	case POSIX_TCSETSF:
	case POSIX_TCFLSH:
	case POSIX_FIONREAD:
	case POSIX_TIOCGPGRP:
	case POSIX_TIOCSPGRP:
	case POSIX_TIOCGSID:
	case POSIX_TIOCSCTTY:
	case POSIX_TIOCGPTN:
		ret = vnode_ioctl(pfd->vnode, cmd_u, (void *)arg_u);
		break;
	default:
		ret = -POSIX_ENOTTY;
		break;
	}

	return (ret);
}

s64
posix_access(u64 path_u, u64 mode, u64 a3, u64 a4, u64 a5, u64 a6,
    registers_t *regs)
{
  char		*path;
  vnode_t		*vn;
  int			perm;
  int			ret;
  int			want;

  (void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

  path = copy_user_string((const char *)path_u, 256);
  if (!path) {
    return (-POSIX_EFAULT);
  }

  if (restrict_kusr_check(path)) {
    kmem_free(path);
    return (-POSIX_EACCES);
  }

  ret = vfs_resolve(path, &vn);
  if (ret != 0) {
    kmem_free(path);
    return (posix_vfs_ret(ret));
  }
  if (vn == NULL) {
    kmem_free(path);
    return (-POSIX_ENOENT);
  }

  want = (int)mode & 7;
  perm = posix_check_perm(vn, want);
  vnode_release(vn);
  kmem_free(path);
  return ((s64)perm);
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
	vfs_dirent_t	entries[POSIX_GETDENTS_BATCH];
	struct process	*proc;
	posix_dirent64_t	*de;
	posix_fd_t	*pfd;
	u8		*buf;
	u32		idx, batch_count, batch_index, consumed, want;
	u32		name_len;
	u64		pos;
	int		ret;
	u16		reclen;

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
	if (!user_range_fault_in((void *)buf_u, count, 1)) {
		return (-POSIX_EFAULT);
	}

	buf = (u8 *)buf_u;
	pos = 0;
	idx = (u32)pfd->offset;

	while (pos + POSIX_DIRENT64_NAME_OFF + 1 <= count) {
		want = POSIX_GETDENTS_BATCH;
		batch_count = 0;
		ret = vnode_listdir(pfd->vnode, idx, entries, want,
		    &batch_count);
		if (ret != 0) {
			return (posix_vfs_ret(ret));
		}
		if (batch_count == 0) {
			break;
		}
		if (batch_count > want) {
			batch_count = want;
		}

		consumed = 0;
		for (batch_index = 0; batch_index < batch_count;
		    batch_index++) {
			name_len = strlen(entries[batch_index].name);
			reclen = (u16)((POSIX_DIRENT64_NAME_OFF +
			    name_len + 1 + 7) & ~7);

			if (pos + reclen > count) {
				break;
			}

			de = (posix_dirent64_t *)(buf + pos);
			de->d_ino = idx + consumed + 1;
			de->d_off = (s64)(idx + consumed + 1);
			de->d_reclen = reclen;
			de->d_type =
			    posix_dtype_from_vtype(entries[batch_index].type);
			memcpy(de->d_name, entries[batch_index].name,
			    name_len);
			de->d_name[name_len] = '\0';

			pos += reclen;
			consumed++;
		}

		idx += consumed;
		if (consumed < batch_count) {
			break;
		}
	}

	pfd->offset = (u64)idx;
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
