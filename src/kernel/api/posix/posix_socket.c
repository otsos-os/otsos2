#include <kernel/api/posix/posix.h>
#include <kernel/api/posix/posix_socket.h>
#include <kernel/drivers/net/unix_sock.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

static unix_sock_t *
sock_from_fd(int fd)
{
	struct process	*proc;
	posix_fd_t	*pfd;
	proc = process_current();
	if (!proc)
		return (NULL);

	pfd = posix_get_fd(proc, fd);
	if (!pfd || !pfd->vnode)
		return (NULL);
	if (pfd->vnode->type != VSOCK)
		return (NULL);

	return ((unix_sock_t *)pfd->vnode->data);
}

static vnode_t *
sock_create_vnode(unix_sock_t *s)
{
	vnode_t	*vn;

	vn = vnode_alloc(VSOCK, "socket");
	if (!vn)
		return (NULL);

	vn->data = s;
	vn->data_owned = 0;
	vn->mode = POSIX_S_IFSOCK | 0600;
	vn->read_fn = unix_sock_vnode_read;
	vn->write_fn = unix_sock_vnode_write;
	vn->stat_fn = unix_sock_vnode_stat;
	vn->readdir_fn = NULL;
	vn->ioctl_fn = NULL;
	vn->readlink_fn = NULL;
	return (vn);
}

static int
sock_fd_install(unix_sock_t *s)
{
	struct process	*proc;
	vnode_t		*vn;
	int		fd;

	proc = process_current();
	if (!proc)
		return (-POSIX_EFAULT);

	fd = posix_alloc_fd(proc);
	if (fd < 0)
		return (fd);

	vn = sock_create_vnode(s);
	if (!vn) {
		proc->posix_fds[fd].used = 0;
		return (-POSIX_ENOMEM);
	}

	proc->posix_fds[fd].used = 1;
	proc->posix_fds[fd].cloexec = 0;
	proc->posix_fds[fd].flags = POSIX_O_RDWR;
	proc->posix_fds[fd].offset = 0;
	proc->posix_fds[fd].vnode = vn;
	return (fd);
}

s64
posix_socket(u64 domain_u, u64 type_u, u64 protocol_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	int		domain, type, protocol;
	unix_sock_t	*s;
	int		fd;

	(void)a4; (void)a5; (void)a6; (void)regs;

	domain = (int)domain_u;
	type = (int)type_u & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	protocol = (int)protocol_u;

	if (domain != AF_UNIX && domain != AF_UNSPEC)
		return (-POSIX_EAFNOSUPPORT);
	if (type != SOCK_STREAM && type != SOCK_DGRAM)
		return (-POSIX_EPROTONOSUPPORT);

	s = unix_sock_alloc(domain, type, protocol);
	if (!s)
		return (-POSIX_ENOMEM);

	fd = sock_fd_install(s);
	if (fd < 0) {
		unix_sock_put(s);
		return ((s64)fd);
	}
	return ((s64)fd);
}

s64
posix_bind(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	unix_sock_t		*s;
	struct sockaddr_un	 sun;
	int			addrlen;
	int			ret;

	(void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	addrlen = (int)addrlen_u;
	if (!addr_u || !is_user_address((void *)addr_u, addrlen))
		return (-POSIX_EFAULT);
	if (addrlen < 2)
		return (-POSIX_EINVAL);

	memset(&sun, 0, sizeof(sun));
	memcpy(&sun, (void *)addr_u,
	    (unsigned long)(addrlen < (int)sizeof(sun) ?
	    addrlen : (int)sizeof(sun)));

	if (sun.sun_family != AF_UNIX)
		return (-POSIX_EAFNOSUPPORT);
	if (sun.sun_path[0] == '\0')
		return (-POSIX_EINVAL);

	ret = unix_sock_bind(s, sun.sun_path);
	return ((s64)(ret < 0 ? ret : 0));
}

s64
posix_connect(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	unix_sock_t		*s, *target;
	struct sockaddr_un	 sun;
	int			addrlen;

	(void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	addrlen = (int)addrlen_u;
	if (!addr_u || !is_user_address((void *)addr_u, addrlen))
		return (-POSIX_EFAULT);
	if (addrlen < 2)
		return (-POSIX_EINVAL);

	memset(&sun, 0, sizeof(sun));
	memcpy(&sun, (void *)addr_u,
	    (unsigned long)(addrlen < (int)sizeof(sun) ?
	    addrlen : (int)sizeof(sun)));

	if (sun.sun_family != AF_UNIX)
		return (-POSIX_EAFNOSUPPORT);

	target = unix_sock_find_by_path(sun.sun_path);
	if (!target)
		return (-POSIX_ECONNREFUSED);

	return ((s64)unix_sock_connect_stream(s, target));
}

s64
posix_listen(u64 sockfd_u, u64 backlog_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	unix_sock_t	*s;

	(void)backlog_u; (void)a3; (void)a4; (void)a5; (void)a6;
	(void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	return ((s64)unix_sock_listen(s));
}

static int
sock_fill_addr(struct sockaddr_un *addr, unix_sock_t *s,
    struct sockaddr_un *user_addr, socklen_t *user_addrlen)
{
	if (!user_addr || !user_addrlen)
		return (0);

	socklen_t ulen;

	memcpy(&ulen, user_addrlen, sizeof(ulen));
	memset(addr, 0, sizeof(*addr));
	addr->sun_family = AF_UNIX;

	if (s->bound_path[0])
		strcpy(addr->sun_path, s->bound_path);

	if (ulen > (socklen_t)sizeof(*addr))
		ulen = sizeof(*addr);
	memcpy(user_addr, addr, (unsigned long)ulen);
	ulen = sizeof(*addr);
	memcpy(user_addrlen, &ulen, sizeof(ulen));

	return (0);
}

s64
posix_accept(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	unix_sock_t		*s, *n;
	int			fd;

	(void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	n = unix_sock_accept_dequeue(s, 0);
	if (!n)
		return (-POSIX_EINVAL);

	fd = sock_fd_install(n);
	if (fd < 0) {
		unix_sock_put(n);
		return ((s64)fd);
	}

	if (addr_u && addrlen_u) {
		struct sockaddr_un addr;
		sock_fill_addr(&addr, n->peer ? n->peer : n,
		    (struct sockaddr_un *)addr_u,
		    (socklen_t *)addrlen_u);
	}

	return ((s64)fd);
}

s64
posix_accept4(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 flags_u,
    u64 a5, u64 a6, registers_t *regs)
{
	(void)flags_u;
	return (posix_accept(sockfd_u, addr_u, addrlen_u,
	    flags_u, a5, a6, regs));
}

s64
posix_sendto(u64 sockfd_u, u64 buf_u, u64 len_u, u64 flags_u,
    u64 dest_addr_u, u64 addrlen_u, registers_t *regs)
{
	unix_sock_t	*s;
	const void	*buf;
	u32		len;

	(void)flags_u; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	buf = (const void *)buf_u;
	len = (u32)len_u;

	if (len > 0 && !is_user_address(buf, len))
		return (-POSIX_EFAULT);
	if (len == 0)
		return (0);

	if (unix_sock_get_type(s) == SOCK_STREAM) {
		int n;

		if (dest_addr_u != 0)
			return (-POSIX_EISCONN);

		n = unix_sock_stream_read(s, (void *)buf, len, 0);
		return ((s64)n);
	} else {
		struct sockaddr_un sun;

		if (!dest_addr_u || !addrlen_u)
			return (-POSIX_EDESTADDRREQ);

		int addrlen = (int)addrlen_u;
		if (!is_user_address((void *)dest_addr_u, addrlen))
			return (-POSIX_EFAULT);

		memset(&sun, 0, sizeof(sun));
		memcpy(&sun, (void *)dest_addr_u,
		    (unsigned long)(addrlen < (int)sizeof(sun) ?
		    addrlen : (int)sizeof(sun)));

		if (sun.sun_path[0] == '\0')
			return (-POSIX_EDESTADDRREQ);

		return ((s64)unix_sock_dgram_sendto(s, buf, len,
		    sun.sun_path));
	}
}

s64
posix_recvfrom(u64 sockfd_u, u64 buf_u, u64 len_u, u64 flags_u,
    u64 src_addr_u, u64 addrlen_u, registers_t *regs)
{
	unix_sock_t	*s;
	void		*buf;
	u32		len;

	(void)flags_u; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);
	buf = (void *)buf_u;
	len = (u32)len_u;
	if (len > 0 && !is_user_address(buf, len))
		return (-POSIX_EFAULT);
	if (len == 0)
		return (0);
	if (unix_sock_get_type(s) == SOCK_STREAM) {
		return ((s64)unix_sock_stream_read(s, buf, len, 0));
	} else {
		char		from_path[108];
		u32		from_len;
		int		n;

		from_len = sizeof(from_path);
		n = unix_sock_dgram_recvfrom(s, buf, len, from_path,
		    &from_len, 0);
		if (n < 0)
			return ((s64)n);

		if (src_addr_u && addrlen_u) {
			struct sockaddr_un sun;
			socklen_t ulen;

			memset(&sun, 0, sizeof(sun));
			sun.sun_family = AF_UNIX;
			if (from_path[0])
				strcpy(sun.sun_path, from_path);

			memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));
			if (ulen > (socklen_t)sizeof(sun))
				ulen = sizeof(sun);
			memcpy((void *)src_addr_u, &sun,
			    (unsigned long)ulen);
		}
		return ((s64)n);
	}
}

s64
posix_sendmsg(u64 sockfd_u, u64 msg_u, u64 flags_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct msghdr	msg;
	struct iovec	iov;
	void		*buf;

	(void)flags_u; (void)a4; (void)a5; (void)a6; (void)regs;

	if (!msg_u || !is_user_address((void *)msg_u, sizeof(msg)))
		return (-POSIX_EFAULT);

	memcpy(&msg, (void *)msg_u, sizeof(msg));
	if (msg.msg_iovlen != 1)
		return (-POSIX_EINVAL);
	if (!is_user_address(msg.msg_iov, sizeof(iov)))
		return (-POSIX_EFAULT);

	memcpy(&iov, msg.msg_iov, sizeof(iov));
	if (!iov.iov_base || iov.iov_len == 0)
		return (0);
	if (!is_user_address(iov.iov_base, iov.iov_len))
		return (-POSIX_EFAULT);

	buf = kmem_alloc(iov.iov_len);
	if (!buf)
		return (-POSIX_ENOMEM);

	memcpy(buf, iov.iov_base, iov.iov_len);
	s64 ret = posix_sendto(sockfd_u, (u64)buf, (u64)iov.iov_len,
	    0, (u64)msg.msg_name, (u64)msg.msg_namelen, regs);
	kmem_free(buf);
	return (ret);
}

s64
posix_recvmsg(u64 sockfd_u, u64 msg_u, u64 flags_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	struct msghdr	msg;
	struct iovec	iov;
	void		*buf;

	(void)flags_u; (void)a4; (void)a5; (void)a6; (void)regs;

	if (!msg_u || !is_user_address((void *)msg_u, sizeof(msg)))
		return (-POSIX_EFAULT);

	memcpy(&msg, (void *)msg_u, sizeof(msg));
	if (msg.msg_iovlen != 1)
		return (-POSIX_EINVAL);
	if (!is_user_address(msg.msg_iov, sizeof(iov)))
		return (-POSIX_EFAULT);

	memcpy(&iov, msg.msg_iov, sizeof(iov));
	if (!iov.iov_base || iov.iov_len == 0)
		return (0);
	if (!is_user_address(iov.iov_base, iov.iov_len))
		return (-POSIX_EFAULT);

	buf = kmem_alloc(iov.iov_len);
	if (!buf)
		return (-POSIX_ENOMEM);

	s64 ret = posix_recvfrom(sockfd_u, (u64)buf, (u64)iov.iov_len,
	    0, (u64)msg.msg_name,
	    (u64)(msg.msg_name ? sizeof(socklen_t) : 0), regs);

	if (ret > 0) {
		u32 copy = (u32)ret;
		if (copy > iov.iov_len)
			copy = iov.iov_len;
		memcpy(iov.iov_base, buf, copy);
	}

	kmem_free(buf);
	return (ret);
}

s64
posix_shutdown(u64 sockfd_u, u64 how_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	unix_sock_t	*s;

	(void)a3; (void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	unix_sock_shutdown(s, (int)how_u);
	return (0);
}

s64
posix_getsockname(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4,
    u64 a5, u64 a6, registers_t *regs)
{
	unix_sock_t	*s;

	(void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	if (!addr_u || !addrlen_u)
		return (-POSIX_EINVAL);
	if (!is_user_address((void *)addr_u, sizeof(struct sockaddr_un)) ||
	    !is_user_address((void *)addrlen_u, sizeof(socklen_t)))
		return (-POSIX_EFAULT);

	struct sockaddr_un sun;
	int path_len = (int)sizeof(sun.sun_path);
	int ret;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	ret = unix_sock_getsockname(s, sun.sun_path, &path_len);
	if (ret < 0)
		return ((s64)ret);

	socklen_t ulen;
	memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));
	if (ulen > (socklen_t)sizeof(sun))
		ulen = sizeof(sun);
	memcpy((void *)addr_u, &sun, (unsigned long)ulen);
	ulen = sizeof(sun);
	memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
	return (0);
}

s64
posix_getpeername(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4,
    u64 a5, u64 a6, registers_t *regs)
{
	unix_sock_t	*s;

	(void)a4; (void)a5; (void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	if (!addr_u || !addrlen_u)
		return (-POSIX_EINVAL);
	if (!is_user_address((void *)addr_u, sizeof(struct sockaddr_un)) ||
	    !is_user_address((void *)addrlen_u, sizeof(socklen_t)))
		return (-POSIX_EFAULT);

	struct sockaddr_un sun;
	int path_len = (int)sizeof(sun.sun_path);
	int ret;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	ret = unix_sock_getpeername(s, sun.sun_path, &path_len);
	if (ret < 0)
		return ((s64)ret);

	socklen_t ulen;
	memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));
	if (ulen > (socklen_t)sizeof(sun))
		ulen = sizeof(sun);
	memcpy((void *)addr_u, &sun, (unsigned long)ulen);
	ulen = sizeof(sun);
	memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
	return (0);
}

s64
posix_socketpair(u64 domain_u, u64 type_u, u64 protocol_u, u64 sv_u,
    u64 a5, u64 a6, registers_t *regs)
{
	int		domain, type, protocol;
	unix_sock_t	*a, *b;
	int		*sv;
	int		fd_a, fd_b;

	(void)a5; (void)a6; (void)regs;

	domain = (int)domain_u;
	type = (int)type_u & ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	protocol = (int)protocol_u;

	if (domain != AF_UNIX && domain != AF_UNSPEC)
		return (-POSIX_EAFNOSUPPORT);
	if (type != SOCK_STREAM && type != SOCK_DGRAM)
		return (-POSIX_EPROTONOSUPPORT);

	sv = (int *)sv_u;
	if (!sv || !is_user_address(sv, sizeof(int) * 2))
		return (-POSIX_EFAULT);

	a = unix_sock_alloc(domain, type, protocol);
	b = unix_sock_alloc(domain, type, protocol);
	if (!a || !b) {
		if (a) unix_sock_put(a);
		if (b) unix_sock_put(b);
		return (-POSIX_ENOMEM);
	}

	unix_sock_pair(a, b);

	fd_a = sock_fd_install(a);
	if (fd_a < 0) {
		unix_sock_pair(a, b); /* undo before cleanup */
		a->peer = NULL;
		b->peer = NULL;
		unix_sock_put(a);
		unix_sock_put(b);
		return ((s64)fd_a);
	}

	fd_b = sock_fd_install(b);
	if (fd_b < 0) {
		struct process *proc = process_current();
		posix_fd_t *pfd = proc ? posix_get_fd(proc, fd_a) : NULL;
		if (pfd) {
			pfd->used = 0;
			vnode_release(pfd->vnode);
		}
		a->peer = NULL;
		b->peer = NULL;
		unix_sock_put(a);
		unix_sock_put(b);
		return ((s64)fd_b);
	}

	sv[0] = fd_a;
	sv[1] = fd_b;
	return (0);
}

s64
posix_setsockopt(u64 sockfd_u, u64 level_u, u64 optname_u, u64 optval_u,
    u64 optlen_u, u64 a6, registers_t *regs)
{
	(void)a6; (void)regs;

	if (!sock_from_fd((int)sockfd_u))
		return (-POSIX_EBADF);

	if ((int)level_u != SOL_SOCKET)
		return (-POSIX_EOPNOTSUPP);

	switch ((int)optname_u) {
	case SO_NOSIGPIPE:
		return (0);
	default:
		return (-POSIX_EOPNOTSUPP);
	}
}

s64
posix_getsockopt(u64 sockfd_u, u64 level_u, u64 optname_u, u64 optval_u,
    u64 optlen_u, u64 a6, registers_t *regs)
{
	unix_sock_t	*s;
	int		val;
	socklen_t	len;

	(void)a6; (void)regs;

	s = sock_from_fd((int)sockfd_u);
	if (!s)
		return (-POSIX_EBADF);

	if ((int)level_u != SOL_SOCKET)
		return (-POSIX_EOPNOTSUPP);

	if (!optval_u || !optlen_u)
		return (-POSIX_EINVAL);
	if (!is_user_address((void *)optval_u, sizeof(int)) ||
	    !is_user_address((void *)optlen_u, sizeof(socklen_t)))
		return (-POSIX_EFAULT);

	memcpy(&len, (void *)optlen_u, sizeof(len));

	switch ((int)optname_u) {
	case SO_TYPE:
		val = unix_sock_get_type(s);
		break;
	case SO_ERROR:
		val = unix_sock_get_error(s);
		break;
	default:
		return (-POSIX_EOPNOTSUPP);
	}

	if (len > (socklen_t)sizeof(val))
		len = sizeof(val);
	memcpy((void *)optval_u, &val, (unsigned long)len);
	len = sizeof(val);
	memcpy((void *)optlen_u, &len, sizeof(len));
	return (0);
}

int
posix_socket_read(vnode_t *vn, void *buf, u32 count, int nonblock)
{
	unix_sock_t	*s;
	int		n;

	if (!vn || !vn->data)
		return (-POSIX_EBADF);

	s = (unix_sock_t *)vn->data;
	if (unix_sock_get_type(s) == SOCK_STREAM)
		n = unix_sock_stream_read(s, buf, count, nonblock);
	else
		n = unix_sock_dgram_recvfrom(s, buf, count, NULL, NULL,
		    nonblock);

	return ((s64)(n < 0 ? n : n));
}

int
posix_socket_write(vnode_t *vn, const void *buf, u32 count, int nonblock)
{
	unix_sock_t	*s;
	int		n;

	if (!vn || !vn->data)
		return (-POSIX_EBADF);

	s = (unix_sock_t *)vn->data;
	if (unix_sock_get_type(s) == SOCK_STREAM) {
		n = unix_sock_stream_write(s, buf, count, nonblock);
	} else {
		if (!s->peer)
			return (-POSIX_EPIPE);
		n = unix_sock_dgram_sendto(s, buf, count,
		    s->peer->bound_path);
	}

	return ((s64)(n < 0 ? n : n));
}

void
posix_socket_close(vnode_t *vn)
{
	unix_sock_vnode_close(vn);
}

void
posix_socket_hold(vnode_t *vn)
{
	unix_sock_vnode_hold(vn);
}
