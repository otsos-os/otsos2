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
$define %type arp_header_t as packed struct with ARP fields
$define %type arp_cache_entry_t as struct with IP->MAC mapping

$define %func arp_init as function with args void
$define %func arp_cache_init as procedure with args void
$define %func arp_input as function with args net_iface_t *, const u8 *, u16
$define %func arp_resolve as function with args net_iface_t *, u32, u8 *
$define %func arp_lookup as function with args net_iface_t *, u32
$define %func arp_send_request as function with args net_iface_t *, u32
$define %func arp_hold_packet as function with args net_iface_t *, u32, u16, const u8 *, u16

*/

/* !SPACE!

$space %export arp_cache_init, arp_input, arp_resolve, arp_lookup
$space %export arp_send_request, arp_hold_packet

*/

#ifndef NET_ARP_H
#define NET_ARP_H

#include <kernel/net/net.h>
#include <kernel/net/ethernet.h>
#include <mlibc/mlibc.h>

#define	ARP_HTYPE_ETHERNET	1
#define	ARP_PTYPE_IPV4		0x0800
#define	ARP_HLEN_ETHERNET	6
#define	ARP_PLEN_IPV4		4

#define	ARP_OP_REQUEST		1
#define	ARP_OP_REPLY		2

#define	ARP_CACHE_SIZE		16
#define	ARP_ENTRY_TTL_SECS	120
#define	ARP_PENDING_SLOTS	4
#define	ARP_PENDING_BUFSZ	ETHERNET_MTU

typedef struct {
	u16	htype;
	u16	ptype;
	u8	hlen;
	u8	plen;
	u16	oper;
	u8	sha[ETHERNET_ADDR_LEN];
	u32	spa;
	u8	tha[ETHERNET_ADDR_LEN];
	u32	tpa;
} __attribute__((packed)) arp_header_t;

typedef struct {
	net_iface_t	*iface;
	u64	created;
	u32	ip;
	u8	mac[ETHERNET_ADDR_LEN];
	int	valid;
} arp_cache_entry_t;

typedef struct {
	net_iface_t	*iface;
	u32	ip;
	u16	ethertype;
	u16	len;
	u8	data[ARP_PENDING_BUFSZ];
	int	valid;
} arp_pending_t;

void	arp_cache_init(void);
int	arp_input(net_iface_t *iface, const u8 *data, u16 len);
int	arp_resolve(net_iface_t *iface, u32 ip, u8 *mac_out);
arp_cache_entry_t *arp_lookup(net_iface_t *iface, u32 ip);
int	arp_send_request(net_iface_t *iface, u32 target_ip);
int	arp_hold_packet(net_iface_t *iface, u32 ip, u16 ethertype,
    const u8 *data, u16 len);

#endif
