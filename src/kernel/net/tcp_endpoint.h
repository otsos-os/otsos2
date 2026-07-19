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
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_tcp_init as procedure with args void
$define %func net_endpoint_tcp_connect as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_tcp_listen as function with args net_endpoint_t *, int
$define %func net_endpoint_tcp_accept as function with args net_endpoint_t *, net_endpoint_t **, net_endpoint_addr_t *, u32
$define %func net_endpoint_tcp_send_user as function with args net_endpoint_t *, const u8 *, u32, u32
$define %func net_endpoint_tcp_recv_user as function with args net_endpoint_t *, u8 *, u32, net_endpoint_addr_t *, u32, u32 *
$define %func net_endpoint_tcp_readable as function with args net_endpoint_t *
$define %func net_endpoint_tcp_writable as function with args net_endpoint_t *
$define %func net_endpoint_tcp_pending_bytes as function with args net_endpoint_t *
$define %func net_endpoint_tcp_write_space as function with args net_endpoint_t *
$define %func net_endpoint_tcp_begin_close as function with args net_endpoint_t *
$define %func net_endpoint_tcp_drop_children as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_input as function with args net_iface_t *, u32, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16
$define %func net_endpoint_tcp_tick as procedure with args void

*/

/* !SPACE!

$space %export net_endpoint_tcp_init, net_endpoint_tcp_connect
$space %export net_endpoint_tcp_listen, net_endpoint_tcp_accept
$space %export net_endpoint_tcp_send_user, net_endpoint_tcp_recv_user
$space %export net_endpoint_tcp_readable, net_endpoint_tcp_writable
$space %export net_endpoint_tcp_pending_bytes
$space %export net_endpoint_tcp_write_space
$space %export net_endpoint_tcp_begin_close
$space %export net_endpoint_tcp_drop_children
$space %export net_endpoint_tcp_input, net_endpoint_tcp_tick

*/

#ifndef NET_TCP_ENDPOINT_H
#define NET_TCP_ENDPOINT_H

#include <kernel/net/endpoint.h>
#include <mlibc/mlibc.h>

void	net_endpoint_tcp_init(void);
int	net_endpoint_tcp_connect(net_endpoint_t *ep,
    const net_endpoint_addr_t *addr);
int	net_endpoint_tcp_listen(net_endpoint_t *ep, int backlog);
int	net_endpoint_tcp_accept(net_endpoint_t *ep, net_endpoint_t **out_ep,
    net_endpoint_addr_t *addr, u32 flags);
int	net_endpoint_tcp_send_user(net_endpoint_t *ep, const u8 *data,
    u32 len, u32 flags);
int	net_endpoint_tcp_recv_user(net_endpoint_t *ep, u8 *buf, u32 len,
    net_endpoint_addr_t *addr, u32 flags, u32 *out_flags);
int	net_endpoint_tcp_readable(net_endpoint_t *ep);
int	net_endpoint_tcp_writable(net_endpoint_t *ep);
u32	net_endpoint_tcp_pending_bytes(net_endpoint_t *ep);
u32	net_endpoint_tcp_write_space(net_endpoint_t *ep);
int	net_endpoint_tcp_begin_close(net_endpoint_t *ep);
void	net_endpoint_tcp_drop_children(net_endpoint_t *parent);
int	net_endpoint_tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, u32 seq, u32 ack, u16 flags,
    u16 window, const u8 *data, u16 len);
void	net_endpoint_tcp_tick(void);

#endif
