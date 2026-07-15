#ifndef KERNEL_API_POSIX_SOCKET_H
#define KERNEL_API_POSIX_SOCKET_H

#include <kernel/interrupts/idt.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/net/unix_sock.h>
#include <mlibc/mlibc.h>

#define	SOCK_NONBLOCK	04000
#define	SOCK_CLOEXEC	02000000

#define	SO_NOSIGPIPE	0x0800
#define	MSG_NOSIGNAL	0x4000

struct iovec {
	void	*iov_base;
	size_t	iov_len;
};

struct msghdr {
	void		*msg_name;
	socklen_t	msg_namelen;
	struct iovec	*msg_iov;
	size_t		msg_iovlen;
	void		*msg_control;
	size_t		msg_controllen;
	int		msg_flags;
};

s64	posix_socket(u64 domain, u64 type, u64 protocol, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_bind(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_connect(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_listen(u64 sockfd, u64 backlog, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_accept(u64 sockfd, u64 addr, u64 addrlen, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_accept4(u64 sockfd, u64 addr, u64 addrlen, u64 flags,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_sendto(u64 sockfd, u64 buf, u64 len, u64 flags,
	    u64 dest_addr, u64 addrlen, registers_t *regs);
s64	posix_recvfrom(u64 sockfd, u64 buf, u64 len, u64 flags,
	    u64 src_addr, u64 addrlen, registers_t *regs);
s64	posix_sendmsg(u64 sockfd, u64 msg, u64 flags, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_recvmsg(u64 sockfd, u64 msg, u64 flags, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_shutdown(u64 sockfd, u64 how, u64 a3, u64 a4, u64 a5,
	    u64 a6, registers_t *regs);
s64	posix_getsockname(u64 sockfd, u64 addr, u64 addrlen, u64 a4,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_getpeername(u64 sockfd, u64 addr, u64 addrlen, u64 a4,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_socketpair(u64 domain, u64 type, u64 protocol, u64 sv,
	    u64 a5, u64 a6, registers_t *regs);
s64	posix_setsockopt(u64 sockfd, u64 level, u64 optname, u64 optval,
	    u64 optlen, u64 a6, registers_t *regs);
s64	posix_getsockopt(u64 sockfd, u64 level, u64 optname, u64 optval,
	    u64 optlen, u64 a6, registers_t *regs);

int	posix_socket_read(vnode_t *vn, void *buf, u32 count, int nonblock);
int	posix_socket_write(vnode_t *vn, const void *buf, u32 count,
	    int nonblock);
void	posix_socket_close(vnode_t *vn);
void	posix_socket_hold(vnode_t *vn);

#endif
