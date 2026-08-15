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

$define %func ipv4_unconfigured_udp_bootstrap as function with args const u8 *, u16
$define %func ipv4_input as function with args net_iface_t *, const u8 *, const u8 *, u16
$define %func ipv4_output_src as function with args net_iface_t *, u32, u32, u8, const u8 *, u16
$define %func ipv4_output as function with args net_iface_t *, u32, u8, const u8 *, u16
$define %func ipv4_checksum as function with args const void *, int
$define %func ipv4_get_icmp_unreach_sent as function with args void
$define %func ipv4_get_frag_dropped as function with args void

*/

/* !SPACE!

$space %internal ipv4_unconfigured_udp_bootstrap
$space %export ipv4_input, ipv4_output_src, ipv4_output, ipv4_checksum
$space %export ipv4_get_icmp_unreach_sent, ipv4_get_frag_dropped

*/

#include <kernel/net/ipv4.h>
#include <kernel/net/icmp.h>
#include <kernel/net/udp.h>
#include <kernel/net/tcp.h>
#include <kernel/net/arp.h>
#include <kernel/net/ethernet.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>
#define	IPV4_BOOTPC_PORT	68

static int	g_ipv4_frag_dropped;

static int
ipv4_unconfigured_udp_bootstrap(const u8 *payload, u16 len)
{
	u16	dst_port;

	if (!payload || len < UDP_HEADER_LEN) {
		return (0);
	}
	dst_port = ((u16)payload[2] << 8) | payload[3];
	return (dst_port == IPV4_BOOTPC_PORT);
}

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
	u32			src_ip, dst_ip;
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

	src_ip = __builtin_bswap32(ip->src);
	dst_ip = __builtin_bswap32(ip->dst);

	if (dst_ip != 0xFFFFFFFF) {
		if (iface->ip_addr != 0 && dst_ip != iface->ip_addr) {
			return (0);
		}
		if (iface->ip_addr == 0 &&
		    (ip->protocol != IPV4_PROTO_UDP ||
		    !ipv4_unconfigured_udp_bootstrap(data + header_len,
		    (u16)(total_len - header_len)))) {
			return (0);
		}
	}

	if (ip->ttl == 0) {
		return (0);
	}
	flags_frag = __builtin_bswap16(ip->flags_frag);
	if ((flags_frag & 0x3FFF) != 0) {
		g_ipv4_frag_dropped++;
		return (0);
	}

	switch (ip->protocol) {
	case IPV4_PROTO_ICMP:
		ret = icmp_input(iface, src_ip, data + header_len,
		    (u16)(total_len - header_len));
		break;

	case IPV4_PROTO_UDP:
		ret = udp_input(iface, src_ip, dst_ip, data + header_len,
		    (u16)(total_len - header_len), data, total_len);
		break;

	case IPV4_PROTO_TCP:
		ret = tcp_input(iface, src_ip, dst_ip, data + header_len,
		    (u16)(total_len - header_len));
		break;

	default:
		if (dst_ip == iface->ip_addr) {
			icmp_send_unreachable(iface, src_ip,
			    ICMP_CODE_PROT_UNREACH, data, total_len);
		}
		ret = 0;
		break;
	}

	return (ret);
}

int
ipv4_get_icmp_unreach_sent(void)
{
	return (icmp_get_unreach_sent());
}

int
ipv4_get_frag_dropped(void)
{
	return (g_ipv4_frag_dropped);
}

int
ipv4_output_src(net_iface_t *iface, u32 src_ip, u32 dst_ip, u8 protocol,
    const u8 *data, u16 len)
{
	u8			packet[ETHERNET_MTU];
	ipv4_header_t		*ip;
	u16			total_len;
	static u16		packet_id;
	u32			next_hop;
	u8			dst_mac[ETHERNET_ADDR_LEN];

	if (!iface || !iface->ndev || !data || dst_ip == 0) {
		return (-1);
	}
	if (!net_stack_enabled() || !(iface->flags & NET_IFF_UP)) {
		return (-1);
	}
	if (src_ip == 0 && dst_ip != 0xFFFFFFFF) {
		return (-1);
	}
	total_len = (u16)(sizeof(ipv4_header_t) + len);
	if (total_len > ETHERNET_MTU || total_len > iface->ndev->mtu) {
		return (-1);
	}

	memset(packet, 0, sizeof(packet));
	ip = (ipv4_header_t *)packet;
	ip->ver_ihl = (IPV4_VERSION << 4) | IPV4_IHL_MIN;
	ip->total_len = __builtin_bswap16(total_len);
	ip->id = __builtin_bswap16(packet_id++);
	ip->ttl = net_default_ttl();
	ip->protocol = protocol;
	ip->src = __builtin_bswap32(src_ip);
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
			 * queue. ARP owns request retries and flush.
			 */
			if (arp_hold_packet(iface, next_hop, ETHERTYPE_IPV4,
			    packet, total_len) != 0) {
				return (-1);
			}
			return (NET_TX_PENDING);
		}
	}

	return (ethernet_output(iface, dst_mac, ETHERTYPE_IPV4,
	    packet, total_len));
}
int
ipv4_output(net_iface_t *iface, u32 dst_ip, u8 protocol,
    const u8 *data, u16 len)
{
	if (!iface || iface->ip_addr == 0) {
		return (-1);
	}
	return (ipv4_output_src(iface, iface->ip_addr, dst_ip,
	    protocol, data, len));
}
