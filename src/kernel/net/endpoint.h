/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type int as 32 bit signed
$define %type net_endpoint_t as opaque native network endpoint
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_init as procedure with args void
$define %func net_endpoint_open as function with args int, int, u32
$define %func net_endpoint_close as procedure with args net_endpoint_t *
$define %func net_endpoint_set_nonblock as procedure with args endpoint, int
$define %func net_endpoint_bind as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_connect as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_listen as function with args net_endpoint_t *, int
$define %func net_endpoint_accept as function with args net_endpoint_t *, net_endpoint_t **, net_endpoint_addr_t *, u32
$define %func net_endpoint_send as function with args net_endpoint_t *, const u8 *, u32, const net_endpoint_addr_t *, u32
$define %func net_endpoint_recv as function with args net_endpoint_t *, u8 *, u32, net_endpoint_addr_t *, u32, u32 *
$define %func net_endpoint_get_local as procedure with args net_endpoint_t *, net_endpoint_addr_t *
$define %func net_endpoint_get_peer as function with args net_endpoint_t *, net_endpoint_addr_t *
$define %func net_endpoint_get_state as function with args net_endpoint_t *, u32 *, u32 *
$define %func net_endpoint_readable as function with args net_endpoint_t *
$define %func net_endpoint_writable as function with args net_endpoint_t *
$define %func net_endpoint_pending_bytes as function with args net_endpoint_t *
$define %func net_endpoint_write_space as function with args net_endpoint_t *
$define %func net_endpoint_udp_input as function with args net_iface_t *, u32, u32, u16, u16, const u8 *, u16
$define %func net_endpoint_tcp_input as function with args net_iface_t *, u32, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16, const u8 *, u16
$define %func net_endpoint_tick as procedure with args void

*/

/* !SPACE!

$space %export net_endpoint_init, net_endpoint_open, net_endpoint_close
$space %export net_endpoint_set_nonblock
$space %export net_endpoint_bind, net_endpoint_connect
$space %export net_endpoint_listen, net_endpoint_accept
$space %export net_endpoint_send, net_endpoint_recv
$space %export net_endpoint_get_local, net_endpoint_get_peer
$space %export net_endpoint_get_state
$space %export net_endpoint_readable, net_endpoint_writable
$space %export net_endpoint_pending_bytes, net_endpoint_write_space
$space %export net_endpoint_udp_input, net_endpoint_tcp_input
$space %export net_endpoint_tick

*/

#ifndef NET_ENDPOINT_H
#define NET_ENDPOINT_H

#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	NET_ENDPOINT_ADDR_IP4		1
#define	NET_ENDPOINT_PROTO_UDP		1
#define	NET_ENDPOINT_PROTO_TCP		2
#define	NET_ENDPOINT_MODE_DGRAM		1
#define	NET_ENDPOINT_MODE_STREAM		2
#define	NET_ENDPOINT_FLAG_NONBLOCK	0x00000001
#define	NET_ENDPOINT_MSG_NONBLOCK	0x00000001
#define	NET_ENDPOINT_MSG_TRUNC		0x00000002
#define	NET_ENDPOINT_IF_AUTO		(-1)

/*
 * Abstract connection state reported by net_endpoint_get_state().  Kept here
 * rather than reusing the API_NET_STATE_* values so the net layer does not have
 * to include api.h; api/net.c maps between them and must be updated in step
 * with this list.
 */
#define	NET_ENDPOINT_STATE_CLOSED	0
#define	NET_ENDPOINT_STATE_LISTEN	1
#define	NET_ENDPOINT_STATE_CONNECTING	2
#define	NET_ENDPOINT_STATE_CONNECTED	3
#define	NET_ENDPOINT_STATE_PEER_CLOSED	4
#define	NET_ENDPOINT_STATE_CLOSING	5

typedef struct net_endpoint net_endpoint_t;

typedef struct {
	u32	ip;
	u16	port;
	u16	family;
	int	ifindex;
} net_endpoint_addr_t;

void	net_endpoint_init(void);
net_endpoint_t *net_endpoint_open(int proto, int mode, u32 flags);
void	net_endpoint_close(net_endpoint_t *ep);
void	net_endpoint_set_nonblock(net_endpoint_t *ep, int nonblock);
int	net_endpoint_bind(net_endpoint_t *ep,
    const net_endpoint_addr_t *addr);
int	net_endpoint_connect(net_endpoint_t *ep,
    const net_endpoint_addr_t *addr);
int	net_endpoint_listen(net_endpoint_t *ep, int backlog);
int	net_endpoint_accept(net_endpoint_t *ep, net_endpoint_t **out_ep,
    net_endpoint_addr_t *addr, u32 flags);
int	net_endpoint_send(net_endpoint_t *ep, const u8 *data, u32 len,
    const net_endpoint_addr_t *addr, u32 flags);
int	net_endpoint_recv(net_endpoint_t *ep, u8 *buf, u32 len,
    net_endpoint_addr_t *addr, u32 flags, u32 *out_flags);
void	net_endpoint_get_local(net_endpoint_t *ep,
    net_endpoint_addr_t *addr);
int	net_endpoint_get_peer(net_endpoint_t *ep,
    net_endpoint_addr_t *addr);
int	net_endpoint_get_state(net_endpoint_t *ep, u32 *out_state,
    u32 *out_error);
int	net_endpoint_readable(net_endpoint_t *ep);
int	net_endpoint_writable(net_endpoint_t *ep);
u32	net_endpoint_pending_bytes(net_endpoint_t *ep);
u32	net_endpoint_write_space(net_endpoint_t *ep);
int	net_endpoint_udp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, const u8 *data, u16 len);
int	net_endpoint_tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, u32 seq, u32 ack, u16 flags,
    u16 window, const u8 *opts, u16 opt_len, const u8 *data, u16 len);
void	net_endpoint_tick(void);

#endif
