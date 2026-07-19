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
$define %type tcp_header_t as packed struct with TCP wire fields

$define %func tcp_input as function with args net_iface_t *, u32, u32, const u8 *, u16
$define %func tcp_output as function with args net_iface_t *, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16
$define %func tcp_checksum as function with args u32, u32, const u8 *, u16

*/

/* !SPACE!

$space %export tcp_input, tcp_output, tcp_checksum

*/

#include <kernel/net/tcp.h>
#include <kernel/net/endpoint.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <mlibc/mlibc.h>

u16
tcp_checksum(u32 src_ip, u32 dst_ip, const u8 *segment, u16 len)
{
	u32	sum;
	u16	i;

	sum = 0;
	sum += (src_ip >> 16) & 0xFFFF;
	sum += src_ip & 0xFFFF;
	sum += (dst_ip >> 16) & 0xFFFF;
	sum += dst_ip & 0xFFFF;
	sum += IPV4_PROTO_TCP;
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

int
tcp_output(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, u32 seq, u32 ack, u16 flags, u16 window,
    const u8 *data, u16 len)
{
	tcp_header_t	*tcp;
	u8		segment[ETHERNET_MTU];
	u16		tcp_len, csum;

	if (!iface || src_port == 0 || dst_port == 0 ||
	    (!data && len != 0)) {
		return (-1);
	}
	tcp_len = (u16)(TCP_HEADER_LEN + len);
	if (tcp_len > sizeof(segment) ||
	    tcp_len > ETHERNET_MTU - sizeof(ipv4_header_t)) {
		return (-1);
	}

	memset(segment, 0, sizeof(segment));
	tcp = (tcp_header_t *)segment;
	tcp->src_port = __builtin_bswap16(src_port);
	tcp->dst_port = __builtin_bswap16(dst_port);
	tcp->seq = __builtin_bswap32(seq);
	tcp->ack = __builtin_bswap32(ack);
	tcp->offset_flags = __builtin_bswap16(
	    (TCP_DATA_OFFSET_MIN << 12) | (flags & 0x01FF));
	tcp->window = __builtin_bswap16(window);
	tcp->checksum = 0;
	tcp->urgent = 0;
	if (len != 0) {
		memcpy(segment + TCP_HEADER_LEN, data, len);
	}

	csum = tcp_checksum(iface->ip_addr, dst_ip, segment, tcp_len);
	tcp->checksum = __builtin_bswap16(csum == 0 ? 0xFFFF : csum);
	return (ipv4_output(iface, dst_ip, IPV4_PROTO_TCP,
	    segment, tcp_len));
}

int
tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    const u8 *data, u16 len)
{
	const tcp_header_t	*tcp;
	const u8		*payload;
	u32			seq, ack;
	u16			src_port, dst_port;
	u16			offset_flags, header_len;
	u16			flags, window, payload_len;

	if (!iface || !data || len < TCP_HEADER_LEN) {
		return (-1);
	}

	tcp = (const tcp_header_t *)data;
	offset_flags = __builtin_bswap16(tcp->offset_flags);
	header_len = (u16)(((offset_flags >> 12) & 0x0F) * 4u);
	if (header_len < TCP_HEADER_LEN || header_len > len) {
		return (-1);
	}

	if (tcp_checksum(src_ip, dst_ip, data, len) != 0) {
		return (0);
	}

	src_port = __builtin_bswap16(tcp->src_port);
	dst_port = __builtin_bswap16(tcp->dst_port);
	seq = __builtin_bswap32(tcp->seq);
	ack = __builtin_bswap32(tcp->ack);
	flags = offset_flags & 0x01FF;
	window = __builtin_bswap16(tcp->window);
	payload = data + header_len;
	payload_len = (u16)(len - header_len);

	return (net_endpoint_tcp_input(iface, src_ip, dst_ip,
	    src_port, dst_port, seq, ack, flags, window,
	    payload, payload_len));
}
