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
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type char as 8 bit signed
$define %type vnode_t as struct with VFS vnode state
$define %type posix_fd_t as POSIX file descriptor table entry
$define %type unix_sock_t as AF_UNIX socket state
$define %type net_endpoint_t as native IPv4 network endpoint
$define %type net_endpoint_addr_t as native IPv4 endpoint address
$define %type posix_socket_t as POSIX socket wrapper over UNIX or IPv4
$define %type posix_sockaddr_in_t as Linux sockaddr_in layout
$define %type posix_msghdr_t as Linux x86_64 msghdr layout

$define %func posix_net_ret as function with args int
$define %func posix_socket_from_vnode as function with args vnode_t *
$define %func posix_socket_from_fd as function with args int, posix_fd_t **
$define %func posix_socket_alloc as function with args int, int, int
$define %func posix_socket_free as procedure with args posix_socket_t *
$define %func posix_socket_vnode as function with args posix_socket_t *
$define %func posix_socket_install as function with args socket, flags
$define %func posix_socket_uninstall as procedure with args int
$define %func posix_sockaddr_in_from_user as function with args user addr
$define %func posix_sockaddr_in_to_user as function with args inet addr
$define %func posix_sockaddr_un_from_user as function with args user addr
$define %func posix_sockaddr_un_to_user as function with args UNIX addr
$define %func posix_socket_open_unix as function with args domain tuple
$define %func posix_socket_open_inet as function with args domain tuple
$define %func posix_socket as function with args Linux socket tuple
$define %func posix_bind as function with args socket, address
$define %func posix_connect as function with args socket, address
$define %func posix_listen as function with args socket, backlog
$define %func posix_accept_common as function with args socket address flags
$define %func posix_accept as function with args socket, address
$define %func posix_accept4 as function with args socket, address, flags
$define %func posix_socket_send_data as function with args socket message
$define %func posix_socket_recv_data as function with args socket message
$define %func posix_sendto as function with args socket, buffer, address
$define %func posix_recvfrom as function with args socket, buffer, address
$define %func posix_sendmsg as function with args socket, msghdr
$define %func posix_recvmsg as function with args socket, msghdr
$define %func posix_shutdown as function with args socket, how
$define %func posix_getname_common as function with args socket, address
$define %func posix_getsockname as function with args socket, address
$define %func posix_getpeername as function with args socket, address
$define %func posix_socketpair as function with args domain, type, sv
$define %func posix_setsockopt as function with args socket, option
$define %func posix_copy_sockopt_int as function with args option, value
$define %func posix_getsockopt as function with args socket, option
$define %func posix_socket_read as function with args vnode, buffer
$define %func posix_socket_write as function with args vnode, buffer
$define %func posix_socket_close as procedure with args vnode_t *
$define %func posix_socket_hold as procedure with args vnode_t *
$define %func posix_socket_set_nonblock as procedure with args vnode_t *, int
$define %func posix_socket_fd_status as function with args vnode_t *
$define %func posix_socket_fd_readable as function with args vnode_t *
$define %func posix_socket_fd_writable as function with args vnode_t *

*/

/* !SPACE!

$space %internal posix_net_ret, posix_socket_from_vnode
$space %internal posix_socket_from_fd, posix_socket_alloc
$space %internal posix_socket_free, posix_socket_vnode
$space %internal posix_socket_install, posix_socket_uninstall
$space %internal posix_sockaddr_in_from_user
$space %internal posix_sockaddr_in_to_user
$space %internal posix_sockaddr_un_from_user
$space %internal posix_sockaddr_un_to_user
$space %internal posix_socket_open_unix, posix_socket_open_inet
$space %internal posix_socket_send_data, posix_socket_recv_data
$space %internal posix_accept_common, posix_getname_common
$space %internal posix_copy_sockopt_int
$space %export posix_socket, posix_bind, posix_connect
$space %export posix_listen, posix_accept, posix_accept4
$space %export posix_sendto, posix_recvfrom, posix_sendmsg, posix_recvmsg
$space %export posix_shutdown, posix_getsockname, posix_getpeername
$space %export posix_socketpair, posix_setsockopt, posix_getsockopt
$space %export posix_socket_read, posix_socket_write
$space %export posix_socket_close, posix_socket_hold
$space %export posix_socket_set_nonblock
$space %export posix_socket_fd_status
$space %export posix_socket_fd_readable, posix_socket_fd_writable

*/

#include <kernel/api/errno.h>
#include <kernel/api/posix/posix.h>
#include <kernel/api/posix/posix_socket.h>
#include <kernel/api/api.h>
#include <kernel/drivers/net/unix_sock.h>
#include <kernel/entity/entity.h>
#include <kernel/net/endpoint.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	POSIX_SOCKET_MAGIC	0x50534f43u
#define	POSIX_SOCKET_UNIX	1
#define	POSIX_SOCKET_INET	2
#define	POSIX_SOCKET_ERR_NONE	0

#define	POSIX_MSG_SEND_MASK	(MSG_DONTWAIT | MSG_NOSIGNAL | MSG_MORE)
#define	POSIX_MSG_RECV_MASK	(MSG_DONTWAIT | MSG_TRUNC)

typedef struct posix_sockaddr_in {
	u16	sin_family;
	u16	sin_port;
	u32	sin_addr;
	u8	sin_zero[8];
} __attribute__((packed)) posix_sockaddr_in_t;

typedef struct posix_msghdr {
	void			*msg_name;
	socklen_t		msg_namelen;
	struct posix_iovec	*msg_iov;
	int			msg_iovlen;
	int			pad1;
	void			*msg_control;
	socklen_t		msg_controllen;
	int			pad2;
	int			msg_flags;
} posix_msghdr_t;

typedef struct posix_socket {
	u32		magic;
	int		kind;
	int		domain;
	int		type;
	int		protocol;
	int		refcount;
	int		so_error;
	int		reuseaddr;
	int		broadcast;
	int		keepalive;
	int		nosigpipe;
	int		listening;
	int		shut_rd;
	int		shut_wr;
	union {
		unix_sock_t	*local;
		net_endpoint_t	*inet;
	} u;
} posix_socket_t;

static s64
posix_net_ret(int ret)
{
	int	err;

	if (ret >= 0) {
		return ((s64)ret);
	}

	err = -ret;
	switch (err) {
	case API_ERR_BAD_HANDLE:
		return (-POSIX_EBADF);
	case API_ERR_BAD_ADDR:
		return (-POSIX_EFAULT);
	case API_ERR_NO_MEMORY:
	case API_ERR_NOMEM:
		return (-POSIX_ENOMEM);
	case API_ERR_RETRY:
		return (-POSIX_EAGAIN);
	case API_ERR_BUSY:
		return (-POSIX_EADDRINUSE);
	case API_ERR_EXISTS:
		return (-POSIX_EADDRINUSE);
	case API_ERR_BAD_VALUE:
	case API_ERR_INVAL:
		return (-POSIX_EINVAL);
	case API_ERR_NOT_SUPPORTED:
		return (-POSIX_EOPNOTSUPP);
	case API_ERR_NO_DEVICE_ADDR:
		return (-POSIX_EADDRNOTAVAIL);
	case API_ERR_NO_DEVICE:
		return (-POSIX_ENETUNREACH);
	case API_ERR_TOO_BIG:
		return (-POSIX_EMSGSIZE);
	case API_ERR_PIPE_CLOSED:
		return (-POSIX_EPIPE);
	case API_ERR_TIMED_OUT:
		return (-POSIX_ETIMEDOUT);
	case API_ERR_NOT_FOUND:
		return (-POSIX_ENOTCONN);
	case API_ERR_IO:
		return (-POSIX_ECONNRESET);
	default:
		return (-POSIX_EIO);
	}
}

static posix_socket_t *
posix_socket_from_vnode(vnode_t *vn)
{
	posix_socket_t	*sock;

	if (!vn || vn->type != VSOCK || !vn->data) {
		return (NULL);
	}
	sock = (posix_socket_t *)vn->data;
	if (sock->magic != POSIX_SOCKET_MAGIC) {
		return (NULL);
	}
	return (sock);
}

static posix_socket_t *
posix_socket_from_fd(int fd, posix_fd_t **out_pfd)
{
	struct process	*proc;
	posix_fd_t	*pfd;

	if (out_pfd) {
		*out_pfd = NULL;
	}
	proc = process_current();
	if (!proc) {
		return (NULL);
	}
	pfd = posix_get_fd(proc, fd);
	if (!pfd || !pfd->vnode) {
		return (NULL);
	}
	if (pfd->vnode->type != VSOCK) {
		return (NULL);
	}
	if (out_pfd) {
		*out_pfd = pfd;
	}
	return (posix_socket_from_vnode(pfd->vnode));
}

static posix_socket_t *
posix_socket_alloc(int kind, int domain, int type, int protocol)
{
	posix_socket_t	*sock;

	sock = (posix_socket_t *)kmem_calloc(1, sizeof(*sock));
	if (!sock) {
		return (NULL);
	}
	sock->magic = POSIX_SOCKET_MAGIC;
	sock->kind = kind;
	sock->domain = domain;
	sock->type = type;
	sock->protocol = protocol;
	sock->refcount = 1;
	sock->so_error = POSIX_SOCKET_ERR_NONE;
	return (sock);
}

static void
posix_socket_free(posix_socket_t *sock)
{
	if (!sock || sock->magic != POSIX_SOCKET_MAGIC) {
		return;
	}
	if (sock->kind == POSIX_SOCKET_UNIX && sock->u.local) {
		unix_sock_put(sock->u.local);
		sock->u.local = NULL;
	} else if (sock->kind == POSIX_SOCKET_INET && sock->u.inet) {
		net_endpoint_close(sock->u.inet);
		sock->u.inet = NULL;
	}
	sock->magic = 0;
	kmem_free(sock);
}

static vnode_t *
posix_socket_vnode(posix_socket_t *sock)
{
	vnode_t	*vn;

	vn = vnode_alloc(VSOCK, "socket");
	if (!vn) {
		return (NULL);
	}

	vn->data = sock;
	vn->data_owned = 0;
	vn->mode = POSIX_S_IFSOCK | 0600;
	vn->read_fn = NULL;
	vn->write_fn = NULL;
	vn->stat_fn = NULL;
	vn->readdir_fn = NULL;
	vn->ioctl_fn = NULL;
	vn->readlink_fn = NULL;
	return (vn);
}

static int
posix_socket_install(posix_socket_t *sock, int flags, int cloexec)
{
	struct process	*proc;
	vnode_t		*vn;
	int		fd;

	proc = process_current();
	if (!proc) {
		return (-POSIX_EFAULT);
	}

	fd = posix_alloc_fd(proc);
	if (fd < 0) {
		return (fd);
	}

	vn = posix_socket_vnode(sock);
	if (!vn) {
		proc->posix_fds[fd].used = 0;
		return (-POSIX_ENOMEM);
	}

	proc->posix_fds[fd].used = 1;
	proc->posix_fds[fd].cloexec = cloexec ? 1 : 0;
	proc->posix_fds[fd].flags = POSIX_O_RDWR | flags;
	proc->posix_fds[fd].offset = 0;
	proc->posix_fds[fd].vnode = vn;
	{
		entity_id_t	id;

		id = entity_io_create_raw(ENTITY_ARCH_FILE, 0);
		if (id == 0) {
			proc->posix_fds[fd].used = 0;
			proc->posix_fds[fd].vnode = NULL;
			vnode_release(vn);
			return (-POSIX_ENOMEM);
		}
		entity_io_set_ptr(id, ENTITY_IO_PTR_BACKING, vn);
		entity_io_set_ptr(id, ENTITY_IO_PTR_PATH, NULL);
		entity_io_set_i32(id, ENTITY_IO_I32_OFFSET, 0);
		entity_io_set_i32(id, ENTITY_IO_I32_FLAGS,
		    POSIX_O_RDWR);
		proc->posix_fds[fd].entity = (u64)id;
	}
	return (fd);
}

static void
posix_socket_uninstall(int fd)
{
	struct process	*proc;
	posix_fd_t	*pfd;

	proc = process_current();
	if (!proc) {
		return;
	}
	pfd = posix_get_fd(proc, fd);
	if (!pfd) {
		return;
	}
	if (pfd->vnode) {
		if (pfd->vnode->type == VSOCK) {
			posix_socket_close(pfd->vnode);
		}
	}
	if (pfd->entity != 0) {
		entity_release((entity_id_t)pfd->entity);
		pfd->entity = 0;
	} else if (pfd->vnode) {
		vnode_release(pfd->vnode);
	}
	pfd->used = 0;
	pfd->cloexec = 0;
	pfd->flags = 0;
	pfd->offset = 0;
	pfd->vnode = NULL;
	pfd->entity = 0;
}

static int
posix_sockaddr_in_from_user(u64 addr_u, u64 addrlen_u,
    net_endpoint_addr_t *addr)
{
	posix_sockaddr_in_t	sin;
	u64			need;

	need = sizeof(posix_sockaddr_in_t);
	if (!addr_u || !addr) {
		return (-POSIX_EFAULT);
	}
	if (addrlen_u < need) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)addr_u, need) ||
	    !user_range_fault_in((void *)addr_u, need, 0)) {
		return (-POSIX_EFAULT);
	}

	memset(&sin, 0, sizeof(sin));
	memcpy(&sin, (void *)addr_u, sizeof(sin));
	if (sin.sin_family != AF_INET) {
		return (-POSIX_EAFNOSUPPORT);
	}

	addr->family = NET_ENDPOINT_ADDR_IP4;
	addr->port = __builtin_bswap16(sin.sin_port);
	addr->ip = __builtin_bswap32(sin.sin_addr);
	addr->ifindex = NET_ENDPOINT_IF_AUTO;
	return (0);
}

static int
posix_sockaddr_in_to_user(const net_endpoint_addr_t *addr, u64 addr_u,
    socklen_t *addrlen)
{
	posix_sockaddr_in_t	sin;
	socklen_t		ulen;

	if (!addr || !addrlen) {
		return (0);
	}
	if (!addr_u) {
		return (-POSIX_EFAULT);
	}
	ulen = *addrlen;
	if (ulen > sizeof(sin)) {
		ulen = sizeof(sin);
	}
	if (!is_user_address((void *)addr_u, ulen) ||
	    !user_range_fault_in((void *)addr_u, ulen, 1)) {
		return (-POSIX_EFAULT);
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = __builtin_bswap16(addr->port);
	sin.sin_addr = __builtin_bswap32(addr->ip);
	if (ulen != 0) {
		memcpy((void *)addr_u, &sin, (unsigned long)ulen);
	}
	*addrlen = sizeof(sin);
	return (0);
}

static int
posix_sockaddr_un_from_user(u64 addr_u, u64 addrlen_u,
    struct sockaddr_un *sun)
{
	u64	copy_len;

	if (!addr_u || !sun) {
		return (-POSIX_EFAULT);
	}
	if (addrlen_u < 2) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)addr_u, addrlen_u) ||
	    !user_range_fault_in((void *)addr_u, addrlen_u, 0)) {
		return (-POSIX_EFAULT);
	}

	copy_len = addrlen_u;
	if (copy_len > sizeof(*sun)) {
		copy_len = sizeof(*sun);
	}
	memset(sun, 0, sizeof(*sun));
	memcpy(sun, (void *)addr_u, (unsigned long)copy_len);
	if (sun->sun_family != AF_UNIX) {
		return (-POSIX_EAFNOSUPPORT);
	}
	return (0);
}

static int
posix_sockaddr_un_to_user(unix_sock_t *local, u64 addr_u,
    socklen_t *addrlen)
{
	struct sockaddr_un	sun;
	socklen_t		ulen;
	int			path_len;
	int			ret;

	if (!local || !addrlen) {
		return (0);
	}
	if (!addr_u) {
		return (-POSIX_EFAULT);
	}

	ulen = *addrlen;
	if (ulen > sizeof(sun)) {
		ulen = sizeof(sun);
	}
	if (!is_user_address((void *)addr_u, ulen) ||
	    !user_range_fault_in((void *)addr_u, ulen, 1)) {
		return (-POSIX_EFAULT);
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	path_len = sizeof(sun.sun_path);
	ret = unix_sock_getsockname(local, sun.sun_path, &path_len);
	if (ret < 0) {
		return (ret);
	}
	if (ulen != 0) {
		memcpy((void *)addr_u, &sun, (unsigned long)ulen);
	}
	*addrlen = sizeof(sun);
	return (0);
}

static int
posix_socket_open_unix(int domain, int type, int protocol, int flags,
    int cloexec)
{
	posix_socket_t	*sock;
	unix_sock_t	*local;
	int		fd;

	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		return (-POSIX_ESOCKTNOSUPPORT);
	}

	local = unix_sock_alloc(domain, type, protocol);
	if (!local) {
		return (-POSIX_ENOMEM);
	}

	sock = posix_socket_alloc(POSIX_SOCKET_UNIX, domain, type,
	    protocol);
	if (!sock) {
		unix_sock_put(local);
		return (-POSIX_ENOMEM);
	}
	sock->u.local = local;

	fd = posix_socket_install(sock,
	    (flags & SOCK_NONBLOCK) ? POSIX_O_NONBLOCK : 0, cloexec);
	if (fd < 0) {
		posix_socket_free(sock);
		return (fd);
	}
	return (fd);
}

static int
posix_socket_open_inet(int domain, int type, int protocol, int flags,
    int cloexec)
{
	posix_socket_t	*sock;
	net_endpoint_t	*ep;
	u32		ep_flags;
	int		ep_proto, ep_mode;
	int		fd;

	ep_proto = 0;
	ep_mode = 0;
	if (type == SOCK_STREAM) {
		if (protocol != 0 && protocol != IPPROTO_TCP) {
			return (-POSIX_EPROTONOSUPPORT);
		}
		ep_proto = NET_ENDPOINT_PROTO_TCP;
		ep_mode = NET_ENDPOINT_MODE_STREAM;
		protocol = IPPROTO_TCP;
	} else if (type == SOCK_DGRAM) {
		if (protocol != 0 && protocol != IPPROTO_UDP) {
			return (-POSIX_EPROTONOSUPPORT);
		}
		ep_proto = NET_ENDPOINT_PROTO_UDP;
		ep_mode = NET_ENDPOINT_MODE_DGRAM;
		protocol = IPPROTO_UDP;
	} else {
		return (-POSIX_ESOCKTNOSUPPORT);
	}

	ep_flags = 0;
	if (flags & SOCK_NONBLOCK) {
		ep_flags |= NET_ENDPOINT_FLAG_NONBLOCK;
	}
	ep = net_endpoint_open(ep_proto, ep_mode, ep_flags);
	if (!ep) {
		return (-POSIX_ENOMEM);
	}

	sock = posix_socket_alloc(POSIX_SOCKET_INET, domain, type,
	    protocol);
	if (!sock) {
		net_endpoint_close(ep);
		return (-POSIX_ENOMEM);
	}
	sock->u.inet = ep;

	fd = posix_socket_install(sock,
	    (flags & SOCK_NONBLOCK) ? POSIX_O_NONBLOCK : 0, cloexec);
	if (fd < 0) {
		posix_socket_free(sock);
		return (fd);
	}
	return (fd);
}

s64
posix_socket(u64 domain_u, u64 type_u, u64 protocol_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	int	domain, type, protocol;
	int	flags, cloexec;

	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	domain = (int)domain_u;
	type = (int)type_u;
	protocol = (int)protocol_u;
	flags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	cloexec = (flags & SOCK_CLOEXEC) ? 1 : 0;

	if (domain == AF_INET) {
		return ((s64)posix_socket_open_inet(domain, type,
		    protocol, flags, cloexec));
	}
	if (domain == AF_UNIX || domain == AF_LOCAL ||
	    domain == AF_UNSPEC) {
		return ((s64)posix_socket_open_unix(domain, type,
		    protocol, flags, cloexec));
	}
	return (-POSIX_EAFNOSUPPORT);
}

s64
posix_bind(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	net_endpoint_addr_t	addr;
	struct sockaddr_un	sun;
	posix_socket_t		*sock;
	int			ret;

	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		ret = posix_sockaddr_in_from_user(addr_u, addrlen_u, &addr);
		if (ret != 0) {
			return ((s64)ret);
		}
		ret = net_endpoint_bind(sock->u.inet, &addr);
		return (posix_net_ret(ret));
	}

	ret = posix_sockaddr_un_from_user(addr_u, addrlen_u, &sun);
	if (ret != 0) {
		return ((s64)ret);
	}
	if (sun.sun_path[0] == '\0') {
		return (-POSIX_EINVAL);
	}
	return ((s64)unix_sock_bind(sock->u.local, sun.sun_path));
}

s64
posix_connect(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	net_endpoint_addr_t	addr;
	struct sockaddr_un	sun;
	posix_socket_t		*sock;
	unix_sock_t		*target;
	int			ret;

	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		ret = posix_sockaddr_in_from_user(addr_u, addrlen_u, &addr);
		if (ret != 0) {
			return ((s64)ret);
		}
		ret = net_endpoint_connect(sock->u.inet, &addr);
		if (ret == -API_ERR_RETRY) {
			return (-POSIX_EINPROGRESS);
		}
		return (posix_net_ret(ret));
	}

	ret = posix_sockaddr_un_from_user(addr_u, addrlen_u, &sun);
	if (ret != 0) {
		return ((s64)ret);
	}
	target = unix_sock_find_by_path(sun.sun_path);
	if (!target) {
		return (-POSIX_ECONNREFUSED);
	}
	return ((s64)unix_sock_connect_stream(sock->u.local, target));
}

s64
posix_listen(u64 sockfd_u, u64 backlog_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	posix_socket_t	*sock;
	int		ret;

	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		ret = net_endpoint_listen(sock->u.inet, (int)backlog_u);
		if (ret == 0) {
			sock->listening = 1;
		}
		return (posix_net_ret(ret));
	}
	ret = unix_sock_listen(sock->u.local);
	if (ret == 0) {
		sock->listening = 1;
	}
	return ((s64)ret);
}

static s64
posix_accept_common(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 flags_u)
{
	net_endpoint_addr_t	inet_addr;
	posix_socket_t		*sock;
	posix_socket_t		*child_sock;
	net_endpoint_t		*child_ep;
	unix_sock_t		*child_unix;
	posix_fd_t		*pfd;
	socklen_t		ulen;
	u32			msg_flags;
	int			ret, fd, fd_flags, cloexec;

	pfd = NULL;
	sock = posix_socket_from_fd((int)sockfd_u, &pfd);
	if (!sock || !pfd) {
		return (-POSIX_EBADF);
	}
	if (flags_u & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) {
		return (-POSIX_EINVAL);
	}
	if (!addr_u) {
		addrlen_u = 0;
	} else if (!addrlen_u) {
		return (-POSIX_EFAULT);
	}
	if (addrlen_u) {
		if (!is_user_address((void *)addrlen_u, sizeof(ulen)) ||
		    !user_range_fault_in((void *)addrlen_u,
		    sizeof(ulen), 1)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));
	}

	msg_flags = 0;
	if ((pfd->flags & POSIX_O_NONBLOCK) ||
	    (flags_u & SOCK_NONBLOCK)) {
		msg_flags |= NET_ENDPOINT_MSG_NONBLOCK;
	}
	fd_flags = (flags_u & SOCK_NONBLOCK) ? POSIX_O_NONBLOCK : 0;
	cloexec = (flags_u & SOCK_CLOEXEC) ? 1 : 0;

	if (sock->kind == POSIX_SOCKET_INET) {
		child_ep = NULL;
		ret = net_endpoint_accept(sock->u.inet, &child_ep,
		    addr_u ? &inet_addr : NULL, msg_flags);
		if (ret != 0) {
			return (posix_net_ret(ret));
		}
		child_sock = posix_socket_alloc(POSIX_SOCKET_INET,
		    AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (!child_sock) {
			net_endpoint_close(child_ep);
			return (-POSIX_ENOMEM);
		}
		child_sock->u.inet = child_ep;
		fd = posix_socket_install(child_sock, fd_flags, cloexec);
		if (fd < 0) {
			posix_socket_free(child_sock);
			return ((s64)fd);
		}
		if (addr_u && addrlen_u) {
			ret = posix_sockaddr_in_to_user(&inet_addr, addr_u,
			    &ulen);
			if (ret != 0) {
				posix_socket_uninstall(fd);
				return ((s64)ret);
			}
			memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
		}
		return ((s64)fd);
	}

	child_unix = unix_sock_accept_dequeue(sock->u.local,
	    msg_flags ? 1 : 0);
	if (!child_unix) {
		return (msg_flags ? -POSIX_EAGAIN : -POSIX_EINVAL);
	}
	child_sock = posix_socket_alloc(POSIX_SOCKET_UNIX, AF_UNIX,
	    sock->type, sock->protocol);
	if (!child_sock) {
		unix_sock_put(child_unix);
		return (-POSIX_ENOMEM);
	}
	child_sock->u.local = child_unix;
	fd = posix_socket_install(child_sock, fd_flags, cloexec);
	if (fd < 0) {
		posix_socket_free(child_sock);
		return ((s64)fd);
	}
	if (addr_u && addrlen_u) {
		ret = posix_sockaddr_un_to_user(child_unix->peer ?
		    child_unix->peer : child_unix, addr_u, &ulen);
		if (ret != 0) {
			posix_socket_uninstall(fd);
			return ((s64)ret);
		}
		memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
	}
	return ((s64)fd);
}

s64
posix_accept(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;
	return (posix_accept_common(sockfd_u, addr_u, addrlen_u, 0));
}

s64
posix_accept4(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 flags_u,
    u64 a5, u64 a6, registers_t *regs)
{
	(void)a5;
	(void)a6;
	(void)regs;
	return (posix_accept_common(sockfd_u, addr_u, addrlen_u, flags_u));
}

static s64
posix_socket_send_data(posix_socket_t *sock, const void *buf, u32 len,
    int flags, u64 dest_addr_u, u64 addrlen_u, int nonblock)
{
	net_endpoint_addr_t	addr;
	struct sockaddr_un	sun;
	const net_endpoint_addr_t *addrp;
	u32			msg_flags;
	int			ret;
	u8			empty;

	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (flags & ~POSIX_MSG_SEND_MASK) {
		return (-POSIX_EOPNOTSUPP);
	}
	if (len != 0 && !buf) {
		return (-POSIX_EFAULT);
	}
	empty = 0;
	if (len == 0 && !buf) {
		buf = &empty;
	}
	if (sock->shut_wr) {
		return (-POSIX_EPIPE);
	}

	if (sock->kind == POSIX_SOCKET_UNIX) {
		if (sock->type == SOCK_STREAM) {
			if (dest_addr_u != 0) {
				return (-POSIX_EISCONN);
			}
			return ((s64)unix_sock_stream_write(sock->u.local,
			    buf, len, nonblock || (flags & MSG_DONTWAIT)));
		}
		if (!dest_addr_u) {
			if (!sock->u.local->peer) {
				return (-POSIX_EDESTADDRREQ);
			}
			return ((s64)unix_sock_dgram_sendto(sock->u.local,
			    buf, len, sock->u.local->peer->bound_path));
		}
		ret = posix_sockaddr_un_from_user(dest_addr_u,
		    addrlen_u, &sun);
		if (ret != 0) {
			return ((s64)ret);
		}
		if (sun.sun_path[0] == '\0') {
			return (-POSIX_EDESTADDRREQ);
		}
		return ((s64)unix_sock_dgram_sendto(sock->u.local, buf,
		    len, sun.sun_path));
	}

	msg_flags = 0;
	if (nonblock || (flags & MSG_DONTWAIT)) {
		msg_flags |= NET_ENDPOINT_MSG_NONBLOCK;
	}

	addrp = NULL;
	if (dest_addr_u) {
		ret = posix_sockaddr_in_from_user(dest_addr_u,
		    addrlen_u, &addr);
		if (ret != 0) {
			return ((s64)ret);
		}
		if (sock->type == SOCK_STREAM) {
			return (-POSIX_EISCONN);
		}
		addrp = &addr;
	}

	ret = net_endpoint_send(sock->u.inet, (const u8 *)buf, len,
	    addrp, msg_flags);
	if (ret == -API_ERR_BAD_VALUE && sock->type == SOCK_DGRAM &&
	    !addrp) {
		return (-POSIX_EDESTADDRREQ);
	}
	if (ret < 0) {
		sock->so_error = (int)-posix_net_ret(ret);
	}
	return (posix_net_ret(ret));
}

static s64
posix_socket_recv_data(posix_socket_t *sock, void *buf, u32 len,
    int flags, u64 src_addr_u, socklen_t *addrlen)
{
	net_endpoint_addr_t	addr;
	u32			msg_flags, out_flags;
	int			ret;
	char			from_path[108];
	u32			from_len;

	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (flags & ~POSIX_MSG_RECV_MASK) {
		return (-POSIX_EOPNOTSUPP);
	}
	if (len != 0 && !buf) {
		return (-POSIX_EFAULT);
	}
	if (sock->shut_rd) {
		return (0);
	}

	if (sock->kind == POSIX_SOCKET_UNIX) {
		if (sock->type == SOCK_STREAM) {
			return ((s64)unix_sock_stream_read(sock->u.local,
			    buf, len, flags & MSG_DONTWAIT));
		}
		from_len = sizeof(from_path);
		ret = unix_sock_dgram_recvfrom(sock->u.local, buf, len,
		    from_path, &from_len, flags & MSG_DONTWAIT);
		if (ret < 0) {
			return ((s64)ret);
		}
		if (src_addr_u && addrlen) {
			struct sockaddr_un	sun;
			socklen_t		ulen;

			ulen = *addrlen;
			if (ulen > sizeof(sun)) {
				ulen = sizeof(sun);
			}
			if (!is_user_address((void *)src_addr_u, ulen) ||
			    !user_range_fault_in((void *)src_addr_u,
			    ulen, 1)) {
				return (-POSIX_EFAULT);
			}
			memset(&sun, 0, sizeof(sun));
			sun.sun_family = AF_UNIX;
			if (from_path[0]) {
				strcpy(sun.sun_path, from_path);
			}
			if (ulen != 0) {
				memcpy((void *)src_addr_u, &sun,
				    (unsigned long)ulen);
			}
			*addrlen = sizeof(sun);
		}
		return ((s64)ret);
	}

	msg_flags = 0;
	if (flags & MSG_DONTWAIT) {
		msg_flags |= NET_ENDPOINT_MSG_NONBLOCK;
	}
	out_flags = 0;
	ret = net_endpoint_recv(sock->u.inet, (u8 *)buf, len,
	    src_addr_u ? &addr : NULL, msg_flags, &out_flags);
	if (ret < 0) {
		sock->so_error = (int)-posix_net_ret(ret);
		return (posix_net_ret(ret));
	}
	if (src_addr_u && addrlen) {
		ret = posix_sockaddr_in_to_user(&addr, src_addr_u,
		    addrlen);
		if (ret != 0) {
			return ((s64)ret);
		}
	}
	(void)out_flags;
	return ((s64)ret);
}

s64
posix_sendto(u64 sockfd_u, u64 buf_u, u64 len_u, u64 flags_u,
    u64 dest_addr_u, u64 addrlen_u, registers_t *regs)
{
	posix_socket_t	*sock;
	posix_fd_t	*pfd;
	const void	*buf;
	u32		len;

	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, &pfd);
	if (!sock || !pfd) {
		return (-POSIX_EBADF);
	}
	if (len_u > 0xFFFFFFFFu) {
		return (-POSIX_EMSGSIZE);
	}
	len = (u32)len_u;
	buf = (const void *)buf_u;
	if (len != 0 && (!is_user_address(buf, len) ||
	    !user_range_fault_in(buf, len, 0))) {
		return (-POSIX_EFAULT);
	}
	return (posix_socket_send_data(sock, buf, len, (int)flags_u,
	    dest_addr_u, addrlen_u,
	    (pfd->flags & POSIX_O_NONBLOCK) ? 1 : 0));
}

s64
posix_recvfrom(u64 sockfd_u, u64 buf_u, u64 len_u, u64 flags_u,
    u64 src_addr_u, u64 addrlen_u, registers_t *regs)
{
	posix_socket_t	*sock;
	posix_fd_t	*pfd;
	socklen_t	ulen;
	void		*buf;
	s64		ret;
	u32		len;

	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, &pfd);
	if (!sock || !pfd) {
		return (-POSIX_EBADF);
	}
	if (len_u > 0xFFFFFFFFu) {
		return (-POSIX_EMSGSIZE);
	}
	len = (u32)len_u;
	buf = (void *)buf_u;
	if (len != 0 && (!is_user_address(buf, len) ||
	    !user_range_fault_in(buf, len, 1))) {
		return (-POSIX_EFAULT);
	}

	if (!src_addr_u) {
		flags_u |= (pfd->flags & POSIX_O_NONBLOCK) ?
		    MSG_DONTWAIT : 0;
		return (posix_socket_recv_data(sock, buf, len,
		    (int)flags_u, 0, NULL));
	}
	if (!addrlen_u) {
		return (-POSIX_EFAULT);
	}
	if (!is_user_address((void *)addrlen_u, sizeof(ulen)) ||
	    !user_range_fault_in((void *)addrlen_u, sizeof(ulen), 1)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));
	flags_u |= (pfd->flags & POSIX_O_NONBLOCK) ? MSG_DONTWAIT : 0;
	ret = posix_socket_recv_data(sock, buf, len, (int)flags_u,
	    src_addr_u, &ulen);
	if (ret >= 0) {
		memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
	}
	return (ret);
}

s64
posix_sendmsg(u64 sockfd_u, u64 msg_u, u64 flags_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	posix_msghdr_t		msg;
	struct posix_iovec	iov;
	posix_socket_t		*sock;
	posix_fd_t		*pfd;

	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, &pfd);
	if (!sock || !pfd) {
		return (-POSIX_EBADF);
	}
	if (!msg_u || !is_user_address((void *)msg_u, sizeof(msg)) ||
	    !user_range_fault_in((void *)msg_u, sizeof(msg), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&msg, (void *)msg_u, sizeof(msg));
	if (msg.msg_iovlen != 1 || !msg.msg_iov) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address(msg.msg_iov, sizeof(iov)) ||
	    !user_range_fault_in(msg.msg_iov, sizeof(iov), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&iov, msg.msg_iov, sizeof(iov));
	if (iov.iov_len > 0xFFFFFFFFu) {
		return (-POSIX_EMSGSIZE);
	}
	if (iov.iov_len != 0 && (!is_user_address(iov.iov_base,
	    iov.iov_len) || !user_range_fault_in(iov.iov_base,
	    iov.iov_len, 0))) {
		return (-POSIX_EFAULT);
	}
	flags_u |= (pfd->flags & POSIX_O_NONBLOCK) ? MSG_DONTWAIT : 0;
	return (posix_socket_send_data(sock, iov.iov_base,
	    (u32)iov.iov_len, (int)flags_u, (u64)msg.msg_name,
	    (u64)msg.msg_namelen, 0));
}

s64
posix_recvmsg(u64 sockfd_u, u64 msg_u, u64 flags_u, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	posix_msghdr_t		msg;
	struct posix_iovec	iov;
	posix_socket_t		*sock;
	posix_fd_t		*pfd;
	socklen_t		namelen;
	s64			ret;

	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, &pfd);
	if (!sock || !pfd) {
		return (-POSIX_EBADF);
	}
	if (!msg_u || !is_user_address((void *)msg_u, sizeof(msg)) ||
	    !user_range_fault_in((void *)msg_u, sizeof(msg), 1)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&msg, (void *)msg_u, sizeof(msg));
	if (msg.msg_iovlen != 1 || !msg.msg_iov) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address(msg.msg_iov, sizeof(iov)) ||
	    !user_range_fault_in(msg.msg_iov, sizeof(iov), 0)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&iov, msg.msg_iov, sizeof(iov));
	if (iov.iov_len > 0xFFFFFFFFu) {
		return (-POSIX_EMSGSIZE);
	}
	if (iov.iov_len != 0 && (!is_user_address(iov.iov_base,
	    iov.iov_len) || !user_range_fault_in(iov.iov_base,
	    iov.iov_len, 1))) {
		return (-POSIX_EFAULT);
	}

	namelen = msg.msg_namelen;
	flags_u |= (pfd->flags & POSIX_O_NONBLOCK) ? MSG_DONTWAIT : 0;
	ret = posix_socket_recv_data(sock, iov.iov_base,
	    (u32)iov.iov_len, (int)flags_u, (u64)msg.msg_name,
	    msg.msg_name ? &namelen : NULL);
	if (ret >= 0) {
		msg.msg_namelen = namelen;
		msg.msg_flags = 0;
		memcpy((void *)msg_u, &msg, sizeof(msg));
	}
	return (ret);
}

s64
posix_shutdown(u64 sockfd_u, u64 how_u, u64 a3, u64 a4, u64 a5,
    u64 a6, registers_t *regs)
{
	posix_socket_t	*sock;
	int		how;

	(void)a3;
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	how = (int)how_u;
	if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR) {
		return (-POSIX_EINVAL);
	}
	if (sock->kind == POSIX_SOCKET_UNIX) {
		unix_sock_shutdown(sock->u.local, how);
		return (0);
	}
	if (how == SHUT_RD || how == SHUT_RDWR) {
		sock->shut_rd = 1;
	}
	if (how == SHUT_WR || how == SHUT_RDWR) {
		sock->shut_wr = 1;
		if (sock->type == SOCK_STREAM) {
			net_endpoint_close(sock->u.inet);
		}
	}
	return (0);
}

static s64
posix_getname_common(u64 sockfd_u, u64 addr_u, u64 addrlen_u, int peer)
{
	net_endpoint_addr_t	addr;
	posix_socket_t		*sock;
	socklen_t		ulen;
	int			ret;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	if (!addr_u || !addrlen_u) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)addrlen_u, sizeof(ulen)) ||
	    !user_range_fault_in((void *)addrlen_u, sizeof(ulen), 1)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&ulen, (void *)addrlen_u, sizeof(ulen));

	if (sock->kind == POSIX_SOCKET_INET) {
		if (peer) {
			ret = net_endpoint_get_peer(sock->u.inet, &addr);
			if (ret != 0) {
				return (posix_net_ret(ret));
			}
		} else {
			net_endpoint_get_local(sock->u.inet, &addr);
		}
		ret = posix_sockaddr_in_to_user(&addr, addr_u, &ulen);
	} else {
		if (peer && !sock->u.local->peer) {
			return (-POSIX_ENOTCONN);
		}
		ret = posix_sockaddr_un_to_user(peer ?
		    sock->u.local->peer : sock->u.local, addr_u, &ulen);
	}
	if (ret != 0) {
		return ((s64)ret);
	}
	memcpy((void *)addrlen_u, &ulen, sizeof(ulen));
	return (0);
}

s64
posix_getsockname(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4,
    u64 a5, u64 a6, registers_t *regs)
{
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;
	return (posix_getname_common(sockfd_u, addr_u, addrlen_u, 0));
}

s64
posix_getpeername(u64 sockfd_u, u64 addr_u, u64 addrlen_u, u64 a4,
    u64 a5, u64 a6, registers_t *regs)
{
	(void)a4;
	(void)a5;
	(void)a6;
	(void)regs;
	return (posix_getname_common(sockfd_u, addr_u, addrlen_u, 1));
}

s64
posix_socketpair(u64 domain_u, u64 type_u, u64 protocol_u, u64 sv_u,
    u64 a5, u64 a6, registers_t *regs)
{
	posix_socket_t	*a_sock, *b_sock;
	unix_sock_t	*a_unix, *b_unix;
	int		*sv;
	int		domain, type, protocol;
	int		flags, fd_flags, cloexec;
	int		fd_a, fd_b;

	(void)a5;
	(void)a6;
	(void)regs;

	domain = (int)domain_u;
	type = (int)type_u;
	protocol = (int)protocol_u;
	flags = type & (SOCK_NONBLOCK | SOCK_CLOEXEC);
	type &= ~(SOCK_NONBLOCK | SOCK_CLOEXEC);
	fd_flags = (flags & SOCK_NONBLOCK) ? POSIX_O_NONBLOCK : 0;
	cloexec = (flags & SOCK_CLOEXEC) ? 1 : 0;

	if (domain != AF_UNIX && domain != AF_LOCAL &&
	    domain != AF_UNSPEC) {
		return (-POSIX_EAFNOSUPPORT);
	}
	if (type != SOCK_STREAM && type != SOCK_DGRAM) {
		return (-POSIX_ESOCKTNOSUPPORT);
	}

	sv = (int *)sv_u;
	if (!sv || !is_user_address(sv, sizeof(int) * 2) ||
	    !user_range_fault_in(sv, sizeof(int) * 2, 1)) {
		return (-POSIX_EFAULT);
	}

	a_unix = unix_sock_alloc(domain, type, protocol);
	b_unix = unix_sock_alloc(domain, type, protocol);
	if (!a_unix || !b_unix) {
		if (a_unix) {
			unix_sock_put(a_unix);
		}
		if (b_unix) {
			unix_sock_put(b_unix);
		}
		return (-POSIX_ENOMEM);
	}
	unix_sock_pair(a_unix, b_unix);

	a_sock = posix_socket_alloc(POSIX_SOCKET_UNIX, domain, type,
	    protocol);
	b_sock = posix_socket_alloc(POSIX_SOCKET_UNIX, domain, type,
	    protocol);
	if (!a_sock || !b_sock) {
		if (a_sock) {
			posix_socket_free(a_sock);
		}
		if (b_sock) {
			posix_socket_free(b_sock);
		}
		unix_sock_put(a_unix);
		unix_sock_put(b_unix);
		return (-POSIX_ENOMEM);
	}
	a_sock->u.local = a_unix;
	b_sock->u.local = b_unix;

	fd_a = posix_socket_install(a_sock, fd_flags, cloexec);
	if (fd_a < 0) {
		posix_socket_free(a_sock);
		posix_socket_free(b_sock);
		return ((s64)fd_a);
	}
	fd_b = posix_socket_install(b_sock, fd_flags, cloexec);
	if (fd_b < 0) {
		posix_socket_uninstall(fd_a);
		posix_socket_free(b_sock);
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
	posix_socket_t	*sock;
	int		level, optname, val;

	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	level = (int)level_u;
	optname = (int)optname_u;
	val = 0;
	if (optlen_u >= sizeof(int)) {
		if (!optval_u || !is_user_address((void *)optval_u,
		    sizeof(int)) || !user_range_fault_in((void *)optval_u,
		    sizeof(int), 0)) {
			return (-POSIX_EFAULT);
		}
		memcpy(&val, (void *)optval_u, sizeof(val));
	}

	if (level == SOL_SOCKET) {
		switch (optname) {
		case SO_REUSEADDR:
			sock->reuseaddr = val ? 1 : 0;
			return (0);
		case SO_BROADCAST:
			sock->broadcast = val ? 1 : 0;
			return (0);
		case SO_KEEPALIVE:
			sock->keepalive = val ? 1 : 0;
			return (0);
		case SO_NOSIGPIPE:
			sock->nosigpipe = val ? 1 : 0;
			return (0);
		case SO_SNDBUF:
		case SO_RCVBUF:
		case SO_LINGER:
			return (0);
		default:
			return (-POSIX_ENOPROTOOPT);
		}
	}
	if (level == IPPROTO_TCP && optname == TCP_NODELAY &&
	    sock->kind == POSIX_SOCKET_INET && sock->type == SOCK_STREAM) {
		return (0);
	}
	return (-POSIX_ENOPROTOOPT);
}

static int
posix_copy_sockopt_int(u64 optval_u, u64 optlen_u, int val)
{
	socklen_t	len;

	if (!optval_u || !optlen_u) {
		return (-POSIX_EINVAL);
	}
	if (!is_user_address((void *)optlen_u, sizeof(len)) ||
	    !user_range_fault_in((void *)optlen_u, sizeof(len), 1)) {
		return (-POSIX_EFAULT);
	}
	memcpy(&len, (void *)optlen_u, sizeof(len));
	if (len > sizeof(val)) {
		len = sizeof(val);
	}
	if (!is_user_address((void *)optval_u, len) ||
	    !user_range_fault_in((void *)optval_u, len, 1)) {
		return (-POSIX_EFAULT);
	}
	if (len != 0) {
		memcpy((void *)optval_u, &val, (unsigned long)len);
	}
	len = sizeof(val);
	memcpy((void *)optlen_u, &len, sizeof(len));
	return (0);
}

s64
posix_getsockopt(u64 sockfd_u, u64 level_u, u64 optname_u, u64 optval_u,
    u64 optlen_u, u64 a6, registers_t *regs)
{
	posix_socket_t	*sock;
	int		level, optname, val;

	(void)a6;
	(void)regs;

	sock = posix_socket_from_fd((int)sockfd_u, NULL);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	level = (int)level_u;
	optname = (int)optname_u;

	if (level == SOL_SOCKET) {
		switch (optname) {
		case SO_TYPE:
			val = sock->type;
			break;
		case SO_ERROR:
			val = sock->so_error;
			sock->so_error = 0;
			break;
		case SO_DOMAIN:
			val = sock->domain;
			break;
		case SO_PROTOCOL:
			val = sock->protocol;
			break;
		case SO_ACCEPTCONN:
			val = sock->listening;
			break;
		case SO_REUSEADDR:
			val = sock->reuseaddr;
			break;
		case SO_BROADCAST:
			val = sock->broadcast;
			break;
		case SO_KEEPALIVE:
			val = sock->keepalive;
			break;
		case SO_SNDBUF:
			val = 8192;
			break;
		case SO_RCVBUF:
			val = 8192;
			break;
		default:
			return (-POSIX_ENOPROTOOPT);
		}
		return ((s64)posix_copy_sockopt_int(optval_u,
		    optlen_u, val));
	}
	if (level == IPPROTO_TCP && optname == TCP_NODELAY &&
	    sock->kind == POSIX_SOCKET_INET && sock->type == SOCK_STREAM) {
		return ((s64)posix_copy_sockopt_int(optval_u,
		    optlen_u, 0));
	}
	return (-POSIX_ENOPROTOOPT);
}

int
posix_socket_read(vnode_t *vn, void *buf, u32 count, int nonblock)
{
	posix_socket_t	*sock;
	s64		ret;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	ret = posix_socket_recv_data(sock, buf, count,
	    nonblock ? MSG_DONTWAIT : 0, 0, NULL);
	return ((int)ret);
}

int
posix_socket_write(vnode_t *vn, const void *buf, u32 count, int nonblock)
{
	posix_socket_t	*sock;
	s64		ret;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return (-POSIX_EBADF);
	}
	ret = posix_socket_send_data(sock, buf, count,
	    nonblock ? MSG_DONTWAIT : 0, 0, 0, nonblock);
	return ((int)ret);
}

void
posix_socket_close(vnode_t *vn)
{
	posix_socket_t	*sock;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return;
	}
	if (sock->refcount > 0) {
		sock->refcount--;
	}
	if (sock->refcount == 0) {
		vn->data = NULL;
		posix_socket_free(sock);
	}
}

void
posix_socket_hold(vnode_t *vn)
{
	posix_socket_t	*sock;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return;
	}
	sock->refcount++;
}

void
posix_socket_set_nonblock(vnode_t *vn, int nonblock)
{
	posix_socket_t	*sock;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return;
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		net_endpoint_set_nonblock(sock->u.inet, nonblock);
	}
}

short
posix_socket_fd_status(vnode_t *vn)
{
	posix_socket_t	*sock;
	short		status;

	status = 0;
	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return (POSIX_POLLNVAL);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		if (!sock->u.inet) {
			return (POSIX_POLLNVAL);
		}
		if (sock->so_error != 0) {
			status |= POSIX_POLLERR;
		}
		return (status);
	}
	if (!sock->u.local || sock->u.local->state == UNIX_SOCK_FREE) {
		return (POSIX_POLLNVAL);
	}
	if (sock->u.local->error != 0) {
		status |= POSIX_POLLERR;
	}
	if (sock->u.local->state == UNIX_SOCK_CLOSED) {
		status |= POSIX_POLLHUP;
	}
	if (sock->u.local->type == SOCK_STREAM && sock->u.local->peer &&
	    sock->u.local->peer->state == UNIX_SOCK_CLOSED) {
		status |= POSIX_POLLHUP;
	}
	return (status);
}

int
posix_socket_fd_readable(vnode_t *vn)
{
	posix_socket_t	*sock;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return (0);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		if (sock->shut_rd) {
			return (1);
		}
		return (net_endpoint_readable(sock->u.inet));
	}
	if (!sock->u.local || sock->u.local->state == UNIX_SOCK_FREE) {
		return (0);
	}
	if (sock->u.local->state == UNIX_SOCK_LISTENING) {
		return (sock->u.local->accept_count > 0);
	}
	if (sock->u.local->shut_rd ||
	    sock->u.local->state == UNIX_SOCK_CLOSED) {
		return (1);
	}
	if (sock->u.local->type == SOCK_DGRAM) {
		return (sock->u.local->msg_count > 0);
	}
	if (sock->u.local->stream_count > 0) {
		return (1);
	}
	if (!sock->u.local->peer ||
	    sock->u.local->peer->state == UNIX_SOCK_FREE ||
	    sock->u.local->peer->state == UNIX_SOCK_CLOSED ||
	    sock->u.local->peer->shut_wr) {
		return (1);
	}
	return (0);
}

int
posix_socket_fd_writable(vnode_t *vn)
{
	posix_socket_t	*sock;
	int		next;

	sock = posix_socket_from_vnode(vn);
	if (!sock) {
		return (0);
	}
	if (sock->kind == POSIX_SOCKET_INET) {
		if (sock->shut_wr) {
			return (0);
		}
		return (net_endpoint_writable(sock->u.inet));
	}
	if (!sock->u.local || sock->u.local->state == UNIX_SOCK_FREE ||
	    sock->u.local->shut_wr) {
		return (0);
	}
	if (sock->u.local->type == SOCK_DGRAM) {
		return (sock->u.local->state != UNIX_SOCK_CLOSED);
	}
	if (!sock->u.local->peer ||
	    sock->u.local->peer->state == UNIX_SOCK_FREE ||
	    sock->u.local->peer->state == UNIX_SOCK_CLOSED) {
		return (0);
	}
	next = (sock->u.local->peer->stream_head + 1) %
	    UNIX_SOCK_BUF_SIZE;
	return (next != sock->u.local->peer->stream_tail);
}
