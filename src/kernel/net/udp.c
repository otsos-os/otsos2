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
$define %type udp_binding_t as struct with bound port and receive handler

$define %func udp_init as procedure with args void
$define %func udp_input as function with args net_iface_t *, u32, u32, const u8 *, u16, const u8 *, u16
$define %func udp_output as function with args net_iface_t *, u32, u16, u16, const u8 *, u16
$define %func udp_bind as function with args u16, udp_rx_handler_t, void *
$define %func udp_unbind as procedure with args u16
$define %func udp_checksum as function with args u32, u32, const u8 *, u16
$define %func udp_find_binding as function with args u16

*/

/* !SPACE!

$space %internal udp_find_binding
$space %export udp_init, udp_input, udp_output, udp_bind, udp_unbind, udp_checksum

*/

#include <kernel/net/udp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/icmp.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/endpoint.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static udp_binding_t	g_udp_bindings[UDP_MAX_BINDINGS];
static int		g_udp_initialized;

void
udp_init(void)
{
	int	i;

	if (g_udp_initialized) {
		return;
	}
	for (i = 0; i < UDP_MAX_BINDINGS; i++) {
		memset(&g_udp_bindings[i], 0, sizeof(g_udp_bindings[i]));
	}
	g_udp_initialized = 1;
}
u16
udp_checksum(u32 src_ip, u32 dst_ip, const u8 *segment, u16 len)
{
	u32	sum;
	u16	i;
	sum = 0;
	sum += (src_ip >> 16) & 0xFFFF;
	sum += src_ip & 0xFFFF;
	sum += (dst_ip >> 16) & 0xFFFF;
	sum += dst_ip & 0xFFFF;
	sum += IPV4_PROTO_UDP;
	sum += len;

	for (i = 0; i + 1 < len; i += 2) {
		sum += ((u16)segment[i] << 8) | segment[i + 1];
	}
	if (len & 1) {
		sum += (u16)segment[len - 1] << 8;
	}
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return ((u16)(~sum & 0xFFFF));
}

static udp_binding_t *
udp_find_binding(u16 port)
{
	int	i;

	for (i = 0; i < UDP_MAX_BINDINGS; i++) {
		if (g_udp_bindings[i].valid &&
		    g_udp_bindings[i].port == port) {
			return (&g_udp_bindings[i]);
		}
	}
	return (NULL);
}

int
udp_bind(u16 port, udp_rx_handler_t handler, void *ctx)
{
	udp_binding_t	*slot;
	int		i, free_idx;

	if (port == 0 || !handler) {
		return (-1);
	}
	free_idx = -1;
	for (i = 0; i < UDP_MAX_BINDINGS; i++) {
		if (g_udp_bindings[i].valid &&
		    g_udp_bindings[i].port == port) {
			return (-1);
		}
		if (!g_udp_bindings[i].valid && free_idx < 0) {
			free_idx = i;
		}
	}
	if (free_idx < 0) {
		return (-1);
	}

	slot = &g_udp_bindings[free_idx];
	slot->handler = handler;
	slot->ctx = ctx;
	slot->port = port;
	slot->valid = 1;
	return (0);
}

void
udp_unbind(u16 port)
{
	udp_binding_t	*slot;

	slot = udp_find_binding(port);
	if (slot) {
		memset(slot, 0, sizeof(*slot));
	}
}

int
udp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    const u8 *data, u16 len, const u8 *ip_packet, u16 ip_len)
{
	const udp_header_t	*udp;
	udp_binding_t		*binding;
	u16			udp_len, dst_port, src_port;

	if (!iface || !data || len < UDP_HEADER_LEN) {
		return (-1);
	}

	udp = (const udp_header_t *)data;
	udp_len = __builtin_bswap16(udp->length);
	if (udp_len < UDP_HEADER_LEN || udp_len > len) {
		return (-1);
	}

	if (udp->checksum != 0) {
		if (udp_checksum(src_ip, dst_ip, data, udp_len) != 0) {
			return (0);
		}
	}

	src_port = __builtin_bswap16(udp->src_port);
	dst_port = __builtin_bswap16(udp->dst_port);

	if (net_endpoint_udp_input(iface, src_ip, dst_ip,
	    src_port, dst_port, data + UDP_HEADER_LEN,
	    (u16)(udp_len - UDP_HEADER_LEN))) {
		return (0);
	}

	binding = udp_find_binding(dst_port);
	if (!binding) {
		drivers_log("[UDP] port %d -> %d (%d bytes), no listener\n",
		    src_port, dst_port, udp_len - UDP_HEADER_LEN);
		if (dst_ip == iface->ip_addr && ip_packet && ip_len != 0) {
			icmp_send_unreachable(iface, src_ip,
			    ICMP_CODE_PORT_UNREACH, ip_packet, ip_len);
		}
		return (0);
	}

	binding->handler(iface, src_ip, src_port, dst_port,
	    data + UDP_HEADER_LEN, (u16)(udp_len - UDP_HEADER_LEN),
	    binding->ctx);
	return (0);
}

int
udp_output(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, const u8 *data, u16 len)
{
	u8			segment[ETHERNET_MTU];
	udp_header_t		*udp;
	u16			udp_len, csum;

	if (!iface || !data || src_port == 0 || dst_port == 0) {
		return (-1);
	}
	udp_len = (u16)(UDP_HEADER_LEN + len);
	if (udp_len > sizeof(segment) ||
	    udp_len > ETHERNET_MTU - sizeof(ipv4_header_t)) {
		return (-1);
	}

	memset(segment, 0, sizeof(segment));
	udp = (udp_header_t *)segment;
	udp->src_port = __builtin_bswap16(src_port);
	udp->dst_port = __builtin_bswap16(dst_port);
	udp->length = __builtin_bswap16(udp_len);
	udp->checksum = 0;
	memcpy(segment + UDP_HEADER_LEN, data, len);

	csum = udp_checksum(iface->ip_addr, dst_ip, segment, udp_len);
	udp->checksum = __builtin_bswap16(csum == 0 ? 0xFFFF : csum);
	return (ipv4_output(iface, dst_ip, IPV4_PROTO_UDP,
	    segment, udp_len));
}
