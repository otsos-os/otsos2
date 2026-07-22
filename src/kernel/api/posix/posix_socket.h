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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type short as 16 bit signed
$define %type vnode_t as struct with VFS vnode state
$define %type registers_t as struct with CPU register snapshot

$define %func posix_socket as function with args Linux socket tuple
$define %func posix_bind as function with args socket, address
$define %func posix_connect as function with args socket, address
$define %func posix_listen as function with args socket, backlog
$define %func posix_accept as function with args socket, address
$define %func posix_accept4 as function with args socket, address, flags
$define %func posix_sendto as function with args socket, buffer, address
$define %func posix_recvfrom as function with args socket, buffer, address
$define %func posix_sendmsg as function with args socket, msghdr
$define %func posix_recvmsg as function with args socket, msghdr
$define %func posix_shutdown as function with args socket, how
$define %func posix_getsockname as function with args socket, address
$define %func posix_getpeername as function with args socket, address
$define %func posix_socketpair as function with args domain, type, sv
$define %func posix_setsockopt as function with args socket, option
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

#ifndef KERNEL_API_POSIX_SOCKET_H
#define KERNEL_API_POSIX_SOCKET_H

#include <kernel/interrupts/idt.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/net/unix_sock.h>
#include <mlibc/mlibc.h>

#define	AF_INET		2

#define	IPPROTO_IP	0
#define	IPPROTO_TCP	6
#define	IPPROTO_UDP	17

#define	SOCK_NONBLOCK	04000
#define	SOCK_CLOEXEC	02000000

#define	SO_NOSIGPIPE	0x0800
#define	SO_REUSEADDR	2
#define	SO_BROADCAST	6
#define	SO_SNDBUF	7
#define	SO_RCVBUF	8
#define	SO_KEEPALIVE	9
#define	SO_LINGER	13
#define	SO_ACCEPTCONN	30
#define	SO_PROTOCOL	38
#define	SO_DOMAIN	39

#define	TCP_NODELAY	1

#define	MSG_TRUNC	0x0020
#define	MSG_DONTWAIT	0x0040
#define	MSG_NOSIGNAL	0x4000
#define	MSG_MORE	0x8000

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
void	posix_socket_set_nonblock(vnode_t *vn, int nonblock);
short	posix_socket_fd_status(vnode_t *vn);
int	posix_socket_fd_readable(vnode_t *vn);
int	posix_socket_fd_writable(vnode_t *vn);

#endif
