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
$define %func ipv4_output_src as function with args net_iface_t *, u32, u32, u8, const u8 *, u16
$define %func ipv4_output as function with args net_iface_t *, u32, u8, const u8 *, u16
$define %func ipv4_checksum as function with args const void *, int
$define %func ipv4_get_icmp_unreach_sent as function with args void
$define %func ipv4_get_frag_dropped as function with args void

*/

/* !SPACE!

$space %export ipv4_input, ipv4_output_src, ipv4_output, ipv4_checksum
$space %export ipv4_get_icmp_unreach_sent, ipv4_get_frag_dropped

*/

#ifndef NET_IPV4_H
#define NET_IPV4_H

#include <kernel/net/net.h>
#include <mlibc/mlibc.h>

#define	IPV4_VERSION		4
#define	IPV4_IHL_MIN		5
#define	IPV4_TTL_DEFAULT	64

#define	IPV4_PROTO_ICMP		1
#define	IPV4_PROTO_TCP		6
#define	IPV4_PROTO_UDP		17

typedef struct {
	u8	ver_ihl;
	u8	dscp_ecn;
	u16	total_len;
	u16	id;
	u16	flags_frag;
	u8	ttl;
	u8	protocol;
	u16	checksum;
	u32	src;
	u32	dst;
} __attribute__((packed)) ipv4_header_t;

typedef struct {
	ipv4_header_t	hdr;
	u8		options[];
} __attribute__((packed)) ipv4_packet_t;

int	ipv4_input(net_iface_t *iface, const u8 *src_mac,
    const u8 *data, u16 len);
int	ipv4_output_src(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u8 protocol, const u8 *data, u16 len);
int	ipv4_output(net_iface_t *iface, u32 dst_ip, u8 protocol,
    const u8 *data, u16 len);
u16	ipv4_checksum(const void *buf, int len);
int	ipv4_get_icmp_unreach_sent(void);
int	ipv4_get_frag_dropped(void);

#endif
