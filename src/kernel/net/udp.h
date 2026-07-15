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
$define %type net_iface_t as struct with network interface state
$define %type udp_header_t as packed struct with UDP src/dst port + length + checksum
$define %type udp_rx_handler_t as callback for a delivered UDP datagram
$define %type udp_binding_t as struct with bound port and receive handler

$define %func udp_input as function with args net_iface_t *, u32, const u8 *, u16
$define %func udp_output as function with args net_iface_t *, u32, u16, u16, const u8 *, u16
$define %func udp_bind as function with args u16, udp_rx_handler_t, void *
$define %func udp_unbind as procedure with args u16
$define %func udp_checksum as function with args u32, u32, const u8 *, u16

*/

/* !SPACE!

$space %export udp_input, udp_output, udp_bind, udp_unbind, udp_checksum

*/

#ifndef NET_UDP_H
#define NET_UDP_H

#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	UDP_HEADER_LEN		8
#define	UDP_MAX_BINDINGS	16

typedef struct {
	u16	src_port;
	u16	dst_port;
	u16	length;
	u16	checksum;
} __attribute__((packed)) udp_header_t;
typedef void (*udp_rx_handler_t)(net_iface_t *iface, u32 src_ip,
    u16 src_port, u16 dst_port, const u8 *data, u16 len, void *ctx);

typedef struct {
	udp_rx_handler_t	handler;
	void			*ctx;
	u16			port;
	int			valid;
} udp_binding_t;

int	udp_input(net_iface_t *iface, u32 src_ip,
    const u8 *data, u16 len);
int	udp_output(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, const u8 *data, u16 len);
int	udp_bind(u16 port, udp_rx_handler_t handler, void *ctx);
void	udp_unbind(u16 port);
u16	udp_checksum(u32 src_ip, u32 dst_ip, const u8 *segment, u16 len);

#endif
