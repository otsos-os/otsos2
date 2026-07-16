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
$define %func arp_lookup as function with args net_iface_t *, u32
$define %func arp_send_request as function with args net_iface_t *, u32
$define %func arp_send_reply as function with args net_iface_t *, arp_header_t *, u32
$define %func arp_cache_update as procedure with args net_iface_t *, u32, const u8 *
$define %func arp_hold_packet as function with args net_iface_t *, u32, u16, const u8 *, u16
$define %func arp_flush_pending as procedure with args net_iface_t *, u32
$define %func arp_entry_expired as function with args const arp_cache_entry_t *
$define %func arp_announce as function with args net_iface_t *
$define %func arp_tick as procedure with args void

*/

/* !SPACE!

$space %internal arp_send_reply, arp_cache_update
$space %internal arp_flush_pending, arp_entry_expired
$space %export arp_cache_init, arp_input, arp_resolve, arp_lookup
$space %export arp_send_request, arp_hold_packet, arp_tick

*/

#include <kernel/net/arp.h>
#include <kernel/net/ethernet.h>
#include <kernel/drivers/timer.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static arp_cache_entry_t	g_arp_cache[ARP_CACHE_SIZE];
static arp_pending_t		g_arp_pending[ARP_PENDING_SLOTS];
static int			g_arp_cache_next;

void
arp_cache_init(void)
{
	int	i;

	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		memset(&g_arp_cache[i], 0, sizeof(g_arp_cache[i]));
	}
	for (i = 0; i < ARP_PENDING_SLOTS; i++) {
		memset(&g_arp_pending[i], 0, sizeof(g_arp_pending[i]));
	}
	g_arp_cache_next = 0;
}
static int
arp_entry_expired(const arp_cache_entry_t *entry)
{
	u64	freq, age;

	if (!timer_is_initialized()) {
		return (0);
	}
	freq = timer_get_frequency();
	if (freq == 0) {
		return (0);
	}
	age = timer_get_ticks() - entry->created;
	return (age > (u64)ARP_ENTRY_TTL_SECS * freq);
}

static void
arp_cache_update(net_iface_t *iface, u32 ip, const u8 *mac)
{
	arp_cache_entry_t	*entry;
	u64			now;
	int			i;

	now = timer_is_initialized() ? timer_get_ticks() : 0;
	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		entry = &g_arp_cache[i];
		if (entry->valid && entry->iface == iface &&
		    entry->ip == ip) {
			memcpy(entry->mac, mac, ETHERNET_ADDR_LEN);
			entry->created = now;
			return;
		}
	}

	entry = &g_arp_cache[g_arp_cache_next];
	entry->iface = iface;
	entry->ip = ip;
	entry->created = now;
	memcpy(entry->mac, mac, ETHERNET_ADDR_LEN);
	entry->valid = 1;
	g_arp_cache_next = (g_arp_cache_next + 1) % ARP_CACHE_SIZE;
}

arp_cache_entry_t *
arp_lookup(net_iface_t *iface, u32 ip)
{
	arp_cache_entry_t	*entry;
	int			i;

	for (i = 0; i < ARP_CACHE_SIZE; i++) {
		entry = &g_arp_cache[i];
		if (entry->valid && entry->iface == iface &&
		    entry->ip == ip) {
			if (arp_entry_expired(entry)) {
				entry->valid = 0;
				return (NULL);
			}
			return (entry);
		}
	}
	return (NULL);
}

int
arp_resolve(net_iface_t *iface, u32 ip, u8 *mac_out)
{
	arp_cache_entry_t	*entry;

	if (!iface || !mac_out) {
		return (-1);
	}
	entry = arp_lookup(iface, ip);
	if (entry) {
		memcpy(mac_out, entry->mac, ETHERNET_ADDR_LEN);
		return (0);
	}

	return (-1);
}
int
arp_hold_packet(net_iface_t *iface, u32 ip, u16 ethertype,
    const u8 *data, u16 len)
{
	arp_pending_t	*slot;
	u64		now, freq, retry_ticks;
	int		i, free_idx, same_slot, need_request;

	if (!iface || !data || len == 0 || len > ARP_PENDING_BUFSZ) {
		return (-1);
	}
	now = 0;
	freq = 0;
	if (timer_is_initialized()) {
		now = timer_get_ticks();
		freq = timer_get_frequency();
	}
	free_idx = -1;
	for (i = 0; i < ARP_PENDING_SLOTS; i++) {
		slot = &g_arp_pending[i];
		if (slot->valid && slot->iface == iface &&
		    slot->ip == ip) {
			free_idx = i;
			break;
		}
		if (!slot->valid && free_idx < 0) {
			free_idx = i;
		}
	}
	if (free_idx < 0) {
		free_idx = 0;
	}

	slot = &g_arp_pending[free_idx];
	same_slot = slot->valid && slot->iface == iface &&
	    slot->ip == ip;
	if (!same_slot) {
		memset(slot, 0, sizeof(*slot));
		slot->created = now;
	} else if (slot->created == 0) {
		slot->created = now;
	}

	slot->iface = iface;
	slot->ip = ip;
	slot->ethertype = ethertype;
	slot->len = len;
	memcpy(slot->data, data, len);
	slot->valid = 1;

	retry_ticks = (u64)ARP_PENDING_RETRY_SECS * freq;
	need_request = slot->last_sent == 0;
	if (!need_request && retry_ticks != 0 &&
	    now - slot->last_sent >= retry_ticks) {
		need_request = 1;
	}
	if (need_request) {
		if (slot->retries >= ARP_PENDING_MAX_RETRIES) {
			slot->valid = 0;
			return (-1);
		}
		if (arp_send_request(iface, ip) != 0) {
			slot->valid = 0;
			return (-1);
		}
		slot->last_sent = now ? now : 1;
		slot->retries++;
	}
	return (0);
}

static void
arp_flush_pending(net_iface_t *iface, u32 ip)
{
	arp_pending_t	*slot;
	u8		mac[ETHERNET_ADDR_LEN];
	int		i;

	if (arp_resolve(iface, ip, mac) != 0) {
		return;
	}
	for (i = 0; i < ARP_PENDING_SLOTS; i++) {
		slot = &g_arp_pending[i];
		if (!slot->valid || slot->iface != iface || slot->ip != ip) {
			continue;
		}
		slot->valid = 0;
		ethernet_output(iface, mac, slot->ethertype,
		    slot->data, slot->len);
	}
}

int
arp_send_request(net_iface_t *iface, u32 target_ip)
{
	arp_header_t	arp;
	u8		bcast[ETHERNET_ADDR_LEN];

	if (!iface || !iface->ndev || iface->ip_addr == 0 ||
	    target_ip == 0) {
		return (-1);
	}
	memset(bcast, 0xFF, ETHERNET_ADDR_LEN);
	memset(&arp, 0, sizeof(arp));
	arp.htype = __builtin_bswap16(ARP_HTYPE_ETHERNET);
	arp.ptype = __builtin_bswap16(ARP_PTYPE_IPV4);
	arp.hlen = ARP_HLEN_ETHERNET;
	arp.plen = ARP_PLEN_IPV4;
	arp.oper = __builtin_bswap16(ARP_OP_REQUEST);
	memcpy(arp.sha, iface->ndev->mac, ETHERNET_ADDR_LEN);
	arp.spa = __builtin_bswap32(iface->ip_addr);
	arp.tpa = __builtin_bswap32(target_ip);

	return (ethernet_output(iface, bcast, ETHERTYPE_ARP,
	    (const u8 *)&arp, sizeof(arp)));
}

int
arp_announce(net_iface_t *iface)
{
	if (!iface || iface->ip_addr == 0) {
		return (-1);
	}

	return (arp_send_request(iface, iface->ip_addr));
}

void
arp_tick(void)
{
	arp_pending_t	*slot;
	u64		now, freq, retry_ticks, ttl_ticks;
	int		i;

	if (!timer_is_initialized()) {
		return;
	}
	freq = timer_get_frequency();
	if (freq == 0) {
		return;
	}

	now = timer_get_ticks();
	retry_ticks = (u64)ARP_PENDING_RETRY_SECS * freq;
	ttl_ticks = (u64)ARP_PENDING_TTL_SECS * freq;
	if (retry_ticks == 0) {
		retry_ticks = 1;
	}
	if (ttl_ticks == 0) {
		ttl_ticks = 1;
	}

	for (i = 0; i < ARP_PENDING_SLOTS; i++) {
		slot = &g_arp_pending[i];
		if (!slot->valid) {
			continue;
		}
		if (slot->created != 0 &&
		    now - slot->created >= ttl_ticks) {
			slot->valid = 0;
			continue;
		}
		if (slot->retries >= ARP_PENDING_MAX_RETRIES) {
			slot->valid = 0;
			continue;
		}
		if (slot->last_sent != 0 &&
		    now - slot->last_sent < retry_ticks) {
			continue;
		}
		if (arp_send_request(slot->iface, slot->ip) != 0) {
			slot->valid = 0;
			continue;
		}
		slot->last_sent = now;
		slot->retries++;
	}
}

static int
arp_send_reply(net_iface_t *iface, arp_header_t *req, u32 src_mapped_ip)
{
	arp_header_t	arp;

	if (!iface || !iface->ndev || !req) {
		return (-1);
	}
	memset(&arp, 0, sizeof(arp));
	arp.htype = __builtin_bswap16(ARP_HTYPE_ETHERNET);
	arp.ptype = __builtin_bswap16(ARP_PTYPE_IPV4);
	arp.hlen = ARP_HLEN_ETHERNET;
	arp.plen = ARP_PLEN_IPV4;
	arp.oper = __builtin_bswap16(ARP_OP_REPLY);
	memcpy(arp.sha, iface->ndev->mac, ETHERNET_ADDR_LEN);
	arp.spa = __builtin_bswap32(src_mapped_ip);
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
	    __builtin_bswap16(arp->ptype) != ARP_PTYPE_IPV4 ||
	    arp->hlen != ARP_HLEN_ETHERNET ||
	    arp->plen != ARP_PLEN_IPV4) {
		return (0);
	}

	if (iface->ip_addr == 0 ||
	    __builtin_bswap32(arp->tpa) != iface->ip_addr) {
		return (0);
	}
	if (__builtin_bswap16(arp->oper) != ARP_OP_REQUEST &&
	    __builtin_bswap16(arp->oper) != ARP_OP_REPLY) {
		return (0);
	}

	arp_cache_update(iface, __builtin_bswap32(arp->spa), arp->sha);
	arp_flush_pending(iface, __builtin_bswap32(arp->spa));

	if (__builtin_bswap16(arp->oper) == ARP_OP_REQUEST) {
		if (__builtin_bswap32(arp->tpa) == iface->ip_addr) {
			arp_send_reply(iface,
			    (arp_header_t *)arp, iface->ip_addr);
		}
	}

	return (0);
}
