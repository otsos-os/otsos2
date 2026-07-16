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
$define %type icmp_unreachable_t as packed struct with ICMP unreachable header

$define %func icmp_input as function with args net_iface_t *, u32, const u8 *, u16
$define %func icmp_send_unreachable as function with args net_iface_t *, u32, u8, const u8 *, u16
$define %func icmp_get_unreach_sent as function with args void

*/

/* !SPACE!

$space %export icmp_input, icmp_send_unreachable
$space %export icmp_get_unreach_sent

*/

#include <kernel/net/icmp.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/ethernet.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int	g_icmp_unreach_sent;

int
icmp_input(net_iface_t *iface, u32 src_ip,
    const u8 *data, u16 len)
{
	const icmp_header_t	*icmp;
	icmp_header_t		*rep;
	u8			reply[ETHERNET_MTU];
	int			ret;

	if (!iface || !data || len < sizeof(icmp_header_t) ||
	    len > ETHERNET_MTU) {
		return (-1);
	}

	icmp = (const icmp_header_t *)data;
	if (ipv4_checksum(data, len) != 0) {
		return (0);
	}

	if (icmp->type == ICMP_TYPE_ECHO_REQUEST &&
	    icmp->code == 0) {
		memset(reply, 0, len);
		rep = (icmp_header_t *)reply;
		rep->type = ICMP_TYPE_ECHO_REPLY;
		rep->code = 0;
		rep->id = icmp->id;
		rep->seq = icmp->seq;

		memcpy(reply + sizeof(icmp_header_t),
		    data + sizeof(icmp_header_t),
		    len - sizeof(icmp_header_t));

		rep->checksum = __builtin_bswap16(
		    ipv4_checksum(reply, len));

		ret = ipv4_output(iface, src_ip, IPV4_PROTO_ICMP,
		    reply, len);

		drivers_log("[ICMP] echo reply to "
		    "%d.%d.%d.%d (seq=%u) len=%u\n",
		    (src_ip >> 24) & 0xFF,
		    (src_ip >> 16) & 0xFF,
		    (src_ip >> 8) & 0xFF,
		    src_ip & 0xFF,
		    __builtin_bswap16(icmp->seq),
		    len);
		return (ret);
	}

	return (0);
}

int
icmp_get_unreach_sent(void)
{
	return (g_icmp_unreach_sent);
}

int
icmp_send_unreachable(net_iface_t *iface, u32 dst_ip,
    u8 code, const u8 *original_ip_packet, u16 original_len)
{
	icmp_unreachable_t	*unr;
	u8			pkt[sizeof(icmp_unreachable_t) +
				    sizeof(ipv4_header_t) + 8];
	u16			pkt_len, icmp_data_len;
	int			ret;

	if (!iface || !original_ip_packet || original_len == 0) {
		return (-1);
	}

	icmp_data_len = original_len;
	if (icmp_data_len > sizeof(ipv4_header_t) + 8) {
		icmp_data_len = sizeof(ipv4_header_t) + 8;
	}

	pkt_len = sizeof(icmp_unreachable_t) + icmp_data_len;
	if (pkt_len > ETHERNET_MTU) {
		return (-1);
	}

	memset(pkt, 0, pkt_len);
	unr = (icmp_unreachable_t *)pkt;
	unr->type = ICMP_TYPE_DEST_UNREACH;
	unr->code = code;
	unr->checksum = 0;
	unr->unused = 0;

	memcpy(pkt + sizeof(icmp_unreachable_t),
	    original_ip_packet, icmp_data_len);

	unr->checksum = __builtin_bswap16(
	    ipv4_checksum(pkt, pkt_len));

	ret = ipv4_output(iface, dst_ip, IPV4_PROTO_ICMP, pkt, pkt_len);
	if (ret == 0 || ret == NET_TX_PENDING) {
		g_icmp_unreach_sent++;
	}

	drivers_log("[ICMP] unreachable type=%u code=%u to "
	    "%d.%d.%d.%d\n",
	    ICMP_TYPE_DEST_UNREACH, code,
	    (dst_ip >> 24) & 0xFF,
	    (dst_ip >> 16) & 0xFF,
	    (dst_ip >> 8) & 0xFF,
	    dst_ip & 0xFF);

	return (ret);
}
