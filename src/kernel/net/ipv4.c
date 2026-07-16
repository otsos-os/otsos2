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
$define %type ipv4_header_t as packed struct with IPv4 fields

$define %func ipv4_input as function with args net_iface_t *, const u8 *, const u8 *, u16
$define %func ipv4_output as function with args net_iface_t *, u32, u8, const u8 *, u16
$define %func ipv4_checksum as function with args const void *, int

*/

/* !SPACE!

$space %export ipv4_input, ipv4_output, ipv4_checksum

*/

#include <kernel/net/ipv4.h>
#include <kernel/net/icmp.h>
#include <kernel/net/udp.h>
#include <kernel/net/arp.h>
#include <kernel/net/ethernet.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int	g_icmp_unreach_sent;

u16
ipv4_checksum(const void *buf, int len)
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
ipv4_input(net_iface_t *iface, const u8 *src_mac,
    const u8 *data, u16 len)
{
	const ipv4_header_t	*ip;
	u16			header_len, total_len;
	u16			expected_checksum;
	u16			flags_frag;
	int			ret;

	(void)src_mac;
	if (!iface || !data || len < sizeof(ipv4_header_t)) {
		return (-1);
	}

	ip = (const ipv4_header_t *)data;

	if ((ip->ver_ihl >> 4) != IPV4_VERSION) {
		return (0);
	}

	header_len = (u16)((ip->ver_ihl & 0x0F) * 4u);
	if (header_len < sizeof(ipv4_header_t) || header_len > len) {
		return (-1);
	}

	total_len = __builtin_bswap16(ip->total_len);
	if (total_len < header_len || total_len > len) {
		return (-1);
	}

	expected_checksum = ipv4_checksum(ip, header_len);
	if (expected_checksum != 0) {
		return (0);
	}

	if (iface->ip_addr == 0 ||
	    (__builtin_bswap32(ip->dst) != iface->ip_addr &&
	    __builtin_bswap32(ip->dst) != 0xFFFFFFFF)) {
		return (0);
	}

	if (ip->ttl == 0) {
		return (0);
	}
	flags_frag = __builtin_bswap16(ip->flags_frag);
	if ((flags_frag & 0x3FFF) != 0) {
		return (0);
	}

	switch (ip->protocol) {
	case IPV4_PROTO_ICMP:
		ret = icmp_input(iface, __builtin_bswap32(ip->src),
		    data + header_len,
		    (u16)(total_len - header_len));
		break;

	case IPV4_PROTO_UDP:
		ret = udp_input(iface, __builtin_bswap32(ip->src),
		    data + header_len,
		    (u16)(total_len - header_len));
		break;

	case IPV4_PROTO_TCP:
		icmp_send_unreachable(iface,
		    __builtin_bswap32(ip->src),
		    ICMP_CODE_PROT_UNREACH, data, total_len);
		g_icmp_unreach_sent++;
		ret = 0;
		break;

	default:
		icmp_send_unreachable(iface,
		    __builtin_bswap32(ip->src),
		    ICMP_CODE_PROT_UNREACH, data, total_len);
		g_icmp_unreach_sent++;
		ret = 0;
		break;
	}

	return (ret);
}

int
ipv4_get_icmp_unreach_sent(void)
{
	return (g_icmp_unreach_sent);
}

int
ipv4_output(net_iface_t *iface, u32 dst_ip, u8 protocol,
    const u8 *data, u16 len)
{
	u8			packet[ETHERNET_MTU];
	ipv4_header_t		*ip;
	u16			total_len;
	static u16		packet_id;
	u32			next_hop;
	u8			dst_mac[ETHERNET_ADDR_LEN];

	if (!iface || !iface->ndev || !data || iface->ip_addr == 0 ||
	    dst_ip == 0) {
		return (-1);
	}
	total_len = (u16)(sizeof(ipv4_header_t) + len);
	if (total_len > ETHERNET_MTU) {
		return (-1);
	}

	memset(packet, 0, sizeof(packet));
	ip = (ipv4_header_t *)packet;
	ip->ver_ihl = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
	ip->total_len = __builtin_bswap16(total_len);
	ip->id = __builtin_bswap16(packet_id++);
	ip->ttl = IPV4_TTL_DEFAULT;
	ip->protocol = protocol;
	ip->src = __builtin_bswap32(iface->ip_addr);
	ip->dst = __builtin_bswap32(dst_ip);
	ip->checksum = __builtin_bswap16(
	    ipv4_checksum(ip, sizeof(ipv4_header_t)));
	memcpy(packet + sizeof(ipv4_header_t), data, len);

	if (dst_ip == 0xFFFFFFFF) {
		memset(dst_mac, 0xFF, ETHERNET_ADDR_LEN);
	} else {
		if (iface->netmask != 0 &&
		    (dst_ip & iface->netmask) !=
		    (iface->ip_addr & iface->netmask)) {
			if (iface->gw_addr == 0) {
				return (-1);
			}
			next_hop = iface->gw_addr;
		} else {
			next_hop = dst_ip;
		}
		if (arp_resolve(iface, next_hop, dst_mac) != 0) {
			/*
			 * MAC unknown: park the frame in the ARP pending
			 * queue and kick off resolution.  It is flushed
			 * when the reply arrives instead of being dropped.
			 */
			arp_hold_packet(iface, next_hop, ETHERTYPE_IPV4,
			    packet, total_len);
			arp_send_request(iface, next_hop);
			return (-1);
		}
	}

	return (ethernet_output(iface, dst_mac, ETHERTYPE_IPV4,
	    packet, total_len));
}
