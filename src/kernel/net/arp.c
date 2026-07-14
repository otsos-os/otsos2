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

$define %func arp_cache_init as procedure with args void
$define %func arp_input as function with args net_iface_t *, const u8 *, u16
$define %func arp_resolve as function with args net_iface_t *, u32, u8 *
$define %func arp_lookup as function with args u32
$define %func arp_send_request as function with args net_iface_t *, u32
$define %func arp_send_reply as function with args net_iface_t *, arp_header_t *, u32
$define %func arp_cache_update as procedure with args u32, const u8 *

*/

/* !SPACE!

$space %internal arp_send_request, arp_send_reply, arp_cache_update
$space %export arp_cache_init, arp_input, arp_resolve, arp_lookup

*/

#include <kernel/net/arp.h>
#include <kernel/net/ethernet.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static arp_cache_entry_t	g_arp_cache[ARP_CACHE_SIZE];
static int			g_arp_cache_next;

void
arp_cache_init(void)
{
	int	i;

	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		memset(&g_arp_cache[i], 0, sizeof(g_arp_cache[i]));
	}
	g_arp_cache_next = 0;
}

static void
arp_cache_update(u32 ip, const u8 *mac)
{
	arp_cache_entry_t	*entry;
	int			i;

	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		entry = &g_arp_cache[i];
		if (entry->valid && entry->ip == ip) {
			memcpy(entry->mac, mac, ETHERNET_ADDR_LEN);
			return;
		}
	}

	entry = &g_arp_cache[g_arp_cache_next];
	entry->ip = ip;
	memcpy(entry->mac, mac, ETHERNET_ADDR_LEN);
	entry->valid = 1;
	g_arp_cache_next = (g_arp_cache_next + 1) % ARP_CACHE_SIZE;
}

arp_cache_entry_t *
arp_lookup(u32 ip)
{
	int	i;

	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		if (g_arp_cache[i].valid &&
		    g_arp_cache[i].ip == ip) {
			return (&g_arp_cache[i]);
		}
	}
	return (NULL);
}

int
arp_resolve(net_iface_t *iface, u32 ip, u8 *mac_out)
{
	arp_cache_entry_t	*entry;

	entry = arp_lookup(ip);
	if (entry) {
		memcpy(mac_out, entry->mac, ETHERNET_ADDR_LEN);
		return (0);
	}

	return (-1);
}

int
arp_send_request(net_iface_t *iface, u32 target_ip)
{
	arp_header_t	arp;
	u8		bcast[ETHERNET_ADDR_LEN];

	memset(bcast, 0xFF, ETHERNET_ADDR_LEN);
	memset(&arp, 0, sizeof(arp));
	arp.htype = __builtin_bswap16(ARP_HTYPE_ETHERNET);
	arp.ptype = __builtin_bswap16(ARP_PTYPE_IPV4);
	arp.hlen = ARP_HLEN_ETHERNET;
	arp.plen = ARP_PLEN_IPV4;
	arp.oper = __builtin_bswap16(ARP_OP_REQUEST);
	memcpy(arp.sha, iface->ndev->mac, ETHERNET_ADDR_LEN);
	arp.spa = iface->ip_addr;
	arp.tpa = target_ip;

	return (ethernet_output(iface, bcast, ETHERTYPE_ARP,
	    (const u8 *)&arp, sizeof(arp)));
}

static int
arp_send_reply(net_iface_t *iface, arp_header_t *req, u32 src_mapped_ip)
{
	arp_header_t	arp;
	memset(&arp, 0, sizeof(arp));
	arp.htype = __builtin_bswap16(ARP_HTYPE_ETHERNET);
	arp.ptype = __builtin_bswap16(ARP_PTYPE_IPV4);
	arp.hlen = ARP_HLEN_ETHERNET;
	arp.plen = ARP_PLEN_IPV4;
	arp.oper = __builtin_bswap16(ARP_OP_REPLY);
	memcpy(arp.sha, iface->ndev->mac, ETHERNET_ADDR_LEN);
	arp.spa = src_mapped_ip;
	memcpy(arp.tha, req->sha, ETHERNET_ADDR_LEN);
	arp.tpa = req->spa;
	return (ethernet_output(iface, req->sha, ETHERTYPE_ARP,
	    (const u8 *)&arp, sizeof(arp)));
}

int
arp_input(net_iface_t *iface, const u8 *data, u16 len)
{
	const arp_header_t	*arp;

	if (!iface || !data || len < sizeof(arp_header_t)) {
		return (-1);
	}

	arp = (const arp_header_t *)data;

	if (__builtin_bswap16(arp->htype) != ARP_HTYPE_ETHERNET ||
	    __builtin_bswap16(arp->ptype) != ARP_PTYPE_IPV4) {
		return (0);
	}

	if (arp->tpa != iface->ip_addr && iface->ip_addr != 0) {
		return (0);
	}

	arp_cache_update(arp->spa, arp->sha);

	if (__builtin_bswap16(arp->oper) == ARP_OP_REQUEST) {
		if (arp->tpa == iface->ip_addr) {
			arp_send_reply(iface,
			    (arp_header_t *)arp, arp->tpa);
		}
	}

	return (0);
}
