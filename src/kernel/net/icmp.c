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
$define %type icmp_header_t as packed struct with ICMP type/code/checksum

$define %func icmp_input as function with args net_iface_t *, u32, const u8 *, u16
$define %func icmp_checksum as function with args const void *, int

*/

/* !SPACE!

$space %internal icmp_checksum
$space %export icmp_input

*/

#include <kernel/net/icmp.h>
#include <kernel/net/ipv4.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static u16
icmp_checksum(const void *buf, int len)
{
	const u16	*p;
	u32		sum;
	int		i;

	p = (const u16 *)buf;
	sum = 0;
	for (i = 0; i < len / 2; i++) {
		sum += __builtin_bswap16(p[i]);
	}
	if (len & 1) {
		sum += (u16)((const u8 *)buf)[len - 1] << 8;
	}
	while (sum >> 16) {
		sum = (sum & 0xFFFF) + (sum >> 16);
	}
	return ((u16)(~sum & 0xFFFF));
}

int
icmp_input(net_iface_t *iface, u32 src_ip,
    const u8 *data, u16 len)
{
	const icmp_header_t	*icmp;
	u8			reply[256];
	icmp_header_t		*rep;
	u16			reply_len;

	if (!iface || !data || len < sizeof(icmp_header_t)) {
		return (-1);
	}

	icmp = (const icmp_header_t *)data;
	if (icmp_checksum(data, len) != 0) {
		return (0);
	}

	if (icmp->type == ICMP_TYPE_ECHO_REQUEST &&
	    icmp->code == 0) {
		reply_len = len > sizeof(reply) - sizeof(icmp_header_t) ?
		    sizeof(reply) : len;

		memset(reply, 0, sizeof(reply));
		rep = (icmp_header_t *)reply;
		rep->type = ICMP_TYPE_ECHO_REPLY;
		rep->code = 0;
		rep->id = icmp->id;
		rep->seq = icmp->seq;

		memcpy(reply + sizeof(icmp_header_t),
		    data + sizeof(icmp_header_t),
		    reply_len - sizeof(icmp_header_t));

		rep->checksum = __builtin_bswap16(
		    icmp_checksum(reply, reply_len));

		ipv4_output(iface, src_ip, IPV4_PROTO_ICMP,
		    reply, reply_len);

		drivers_log("[ICMP] echo reply to "
		    "%d.%d.%d.%d (seq=%u)\n",
		    (src_ip >> 24) & 0xFF,
		    (src_ip >> 16) & 0xFF,
		    (src_ip >> 8) & 0xFF,
		    src_ip & 0xFF,
		    __builtin_bswap16(icmp->seq));
	}

	return (0);
}
