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
$define %func tcp_output_opt as function with args net_iface_t *, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16, const u8 *, u16
$define %func tcp_checksum as function with args u32, u32, const u8 *, u16
$define %func tcp_opt_get_mss as function with args const u8 *, u16, u16 *

*/

/* !SPACE!

$space %export tcp_input, tcp_output, tcp_output_opt, tcp_checksum
$space %export tcp_opt_get_mss

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
tcp_opt_get_mss(const u8 *opts, u16 opt_len, u16 *out_mss)
{
	u16	pos, mss;
	u8	kind, len;

	if (!opts || !out_mss || opt_len == 0) {
		return (0);
	}
	if (opt_len > TCP_OPT_MAX_LEN) {
		opt_len = TCP_OPT_MAX_LEN;
	}

	pos = 0;
	/*
	 * Bounded walk: every branch below advances pos or returns, so a
	 * malformed option chain costs a rejected parse and never a hang.
	 */
	while (pos < opt_len) {
		kind = opts[pos];
		if (kind == TCP_OPT_END) {
			return (0);
		}
		if (kind == TCP_OPT_NOP) {
			pos++;
			continue;
		}
		if (pos + 1 >= opt_len) {
			return (0);
		}
		len = opts[pos + 1];
		/* A length under 2 would make this loop stand still. */
		if (len < 2 || pos + len > opt_len) {
			return (0);
		}
		if (kind == TCP_OPT_MSS && len == TCP_OPT_MSS_LEN) {
			mss = (u16)(((u16)opts[pos + 2] << 8) |
			    opts[pos + 3]);
			if (mss < TCP_MSS_MIN) {
				mss = TCP_MSS_MIN;
			}
			*out_mss = mss;
			return (1);
		}
		pos = (u16)(pos + len);
	}
	return (0);
}

int
tcp_output_opt(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, u32 seq, u32 ack, u16 flags, u16 window,
    const u8 *opts, u16 opt_len, const u8 *data, u16 len)
{
	tcp_header_t	*tcp;
	u8		segment[ETHERNET_MTU];
	u16		tcp_len, csum, pad_len, offset_words;

	if (!iface || src_port == 0 || dst_port == 0 ||
	    (!data && len != 0) || (!opts && opt_len != 0)) {
		return (-1);
	}
	if (opt_len > TCP_OPT_MAX_LEN) {
		return (-1);
	}
	/*
	 * The data offset counts 32-bit words, so the option area has to be
	 * rounded up with NOPs before the payload starts.
	 */
	pad_len = (u16)((4 - (opt_len & 3)) & 3);
	tcp_len = (u16)(TCP_HEADER_LEN + opt_len + pad_len + len);
	if (tcp_len > sizeof(segment) ||
	    tcp_len > ETHERNET_MTU - sizeof(ipv4_header_t)) {
		return (-1);
	}
	offset_words = (u16)((TCP_HEADER_LEN + opt_len + pad_len) / 4);

	memset(segment, 0, sizeof(segment));
	tcp = (tcp_header_t *)segment;
	tcp->src_port = __builtin_bswap16(src_port);
	tcp->dst_port = __builtin_bswap16(dst_port);
	tcp->seq = __builtin_bswap32(seq);
	tcp->ack = __builtin_bswap32(ack);
	tcp->offset_flags = __builtin_bswap16(
	    (offset_words << 12) | (flags & 0x01FF));
	tcp->window = __builtin_bswap16(window);
	tcp->checksum = 0;
	tcp->urgent = 0;
	if (opt_len != 0) {
		memcpy(segment + TCP_HEADER_LEN, opts, opt_len);
		/* Pad with NOP rather than END: harmless either way. */
		memset(segment + TCP_HEADER_LEN + opt_len, TCP_OPT_NOP,
		    pad_len);
	}
	if (len != 0) {
		memcpy(segment + TCP_HEADER_LEN + opt_len + pad_len,
		    data, len);
	}

	csum = tcp_checksum(iface->ip_addr, dst_ip, segment, tcp_len);
	tcp->checksum = __builtin_bswap16(csum == 0 ? 0xFFFF : csum);
	return (ipv4_output(iface, dst_ip, IPV4_PROTO_TCP,
	    segment, tcp_len));
}

int
tcp_output(net_iface_t *iface, u32 dst_ip, u16 src_port,
    u16 dst_port, u32 seq, u32 ack, u16 flags, u16 window,
    const u8 *data, u16 len)
{
	return (tcp_output_opt(iface, dst_ip, src_port, dst_port, seq,
	    ack, flags, window, NULL, 0, data, len));
}

int
tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    const u8 *data, u16 len)
{
	const tcp_header_t	*tcp;
	const u8		*payload;
	const u8		*opts;
	u32			seq, ack;
	u16			src_port, dst_port;
	u16			offset_flags, header_len;
	u16			flags, window, payload_len, opt_len;

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
	opts = data + TCP_HEADER_LEN;
	opt_len = (u16)(header_len - TCP_HEADER_LEN);
	payload = data + header_len;
	payload_len = (u16)(len - header_len);

	return (net_endpoint_tcp_input(iface, src_ip, dst_ip,
	    src_port, dst_port, seq, ack, flags, window,
	    opts, opt_len, payload, payload_len));
}
