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
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_rx_t as queued UDP datagram
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_route as function with args u32, u32, int
$define %func net_endpoint_bind_conflict as function with args net_endpoint_t *, u32, u16
$define %func net_endpoint_alloc_port as function with args net_endpoint_t *, u32
$define %func net_endpoint_free as procedure with args net_endpoint_t *

*/

/* !SPACE!

$space %export net_endpoint_t, net_endpoint_rx_t
$space %export net_endpoint_route
$space %export net_endpoint_bind_conflict, net_endpoint_alloc_port
$space %export net_endpoint_free

*/

#ifndef NET_ENDPOINT_INTERNAL_H
#define NET_ENDPOINT_INTERNAL_H

#include <kernel/net/endpoint.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/tcp.h>
#include <kernel/net/udp.h>
#include <mlibc/mlibc.h>

#define	NET_ENDPOINT_MAX		32
#define	NET_ENDPOINT_RX_QUEUE		8
#define	NET_ENDPOINT_DGRAM_MAX		\
	(ETHERNET_MTU - sizeof(ipv4_header_t) - UDP_HEADER_LEN)
#define	NET_ENDPOINT_TCP_RX_SIZE		8192
#define	NET_ENDPOINT_TCP_TX_SIZE		\
	(ETHERNET_MTU - sizeof(ipv4_header_t) - TCP_HEADER_LEN)
#define	NET_ENDPOINT_TCP_ACCEPT_QUEUE	8
#define	NET_ENDPOINT_TCP_MAX_RETRIES	6
#define	NET_ENDPOINT_EPHEMERAL_FIRST	49152
#define	NET_ENDPOINT_EPHEMERAL_LAST	65535

#define	TCP_STATE_CLOSED		0
#define	TCP_STATE_LISTEN		1
#define	TCP_STATE_SYN_SENT		2
#define	TCP_STATE_SYN_RECEIVED		3
#define	TCP_STATE_ESTABLISHED		4
#define	TCP_STATE_CLOSE_WAIT		5
#define	TCP_STATE_FIN_WAIT_1		6
#define	TCP_STATE_FIN_WAIT_2		7
#define	TCP_STATE_CLOSING		8
#define	TCP_STATE_LAST_ACK		9
#define	TCP_STATE_TIME_WAIT		10

typedef struct {
	u8	data[NET_ENDPOINT_DGRAM_MAX];
	u32	src_ip;
	u32	dst_ip;
	u16	src_port;
	u16	dst_port;
	u16	len;
	int	ifindex;
} net_endpoint_rx_t;

struct net_endpoint {
	net_endpoint_rx_t	rx[NET_ENDPOINT_RX_QUEUE];
	u8			tcp_rx[NET_ENDPOINT_TCP_RX_SIZE];
	u8			tcp_tx[NET_ENDPOINT_TCP_TX_SIZE];
	int			tcp_accept_queue[NET_ENDPOINT_TCP_ACCEPT_QUEUE];
	u64			tcp_last_tx;
	u64			tcp_deadline;
	u32			flags;
	u32			local_ip;
	u32			peer_ip;
	u32			tcp_iss;
	u32			tcp_irs;
	u32			tcp_snd_una;
	u32			tcp_snd_nxt;
	u32			tcp_rcv_nxt;
	u32			tcp_peer_win;
	u32			tcp_rx_head;
	u32			tcp_rx_tail;
	u32			tcp_rx_count;
	u32			tcp_tx_seq;
	u32			tcp_tx_len;
	u32			rx_head;
	u32			rx_tail;
	u32			rx_count;
	u32			rx_bytes;
	u32			rx_drops;
	u16			local_port;
	u16			peer_port;
	u16			tcp_tx_flags;
	u16			tcp_accept_head;
	u16			tcp_accept_tail;
	u16			tcp_accept_count;
	u16			tcp_backlog;
	int			used;
	int			proto;
	int			mode;
	int			tcp_state;
	int			tcp_parent;
	int			tcp_retries;
	int			tcp_error;
	int			tcp_close_pending;
	int			tcp_orphan;
	int			ifindex;
	int			peer_ifindex;
};

extern net_endpoint_t	g_endpoints[NET_ENDPOINT_MAX];

net_iface_t *net_endpoint_route(u32 dst_ip, u32 local_ip, int ifindex);
int	net_endpoint_bind_conflict(net_endpoint_t *self, u32 ip, u16 port);
int	net_endpoint_alloc_port(net_endpoint_t *self, u32 ip);
void	net_endpoint_free(net_endpoint_t *ep);

#endif
