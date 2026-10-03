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
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type net_iface_t as struct with logical network interface state
$define %type ipv4_reasm_frag_t as one received fragment with byte range
$define %type ipv4_reasm_slot_t as reassembly accumulator for one datagram

$define %func ipv4_reasm_init as procedure with args void
$define %func ipv4_reasm_input as function with args net_iface_t *, const u8 *, u16, u16, u32, u32, u32, u8, u16, u16, const u8 **, u16 *, u8 **
$define %func ipv4_reasm_tick as procedure with args void
$define %func ipv4_reasm_evict as procedure with args net_iface_t *

*/

/* !SPACE!

$space %internal ipv4_reasm_find_slot, ipv4_reasm_free_slot
$space %internal ipv4_reasm_record_fragment
$space %internal ipv4_reasm_coverage, ipv4_reasm_try_complete
$space %export ipv4_reasm_init, ipv4_reasm_input, ipv4_reasm_tick
$space %export ipv4_reasm_evict

*/

#include <kernel/net/reassembly.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/icmp.h>
#include <kernel/drivers/timer.h>
#include <kernel/time.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>

#define	IPV4_REASM_SLOTS		8
#define	IPV4_REASM_FRAG_BUF		2048
#define	IPV4_MF				(1u << 13)
#define	IPV4_DF				(1u << 14)


typedef struct {
	u32	off;
	u32	len;
	u32	stash_off;
} ipv4_reasm_frag_t;

typedef struct {
	net_iface_t	*iface;
	u8		*heap;
	u8		proto;
	u32		id;
	u32		src_ip;
	u32		dst_ip;
	u32		total_data;
	u32		hdr_len;
	u32		stash_used;
	u16		fragment_count;
	u64		last_rx_ms;
	u8		closed;
	u8		used;
	u32		pad0;
	ipv4_reasm_frag_t frag[IPV4_REASM_MAX_FRAGMENTS];
	u8		stash[IPV4_REASM_FRAG_BUF];
	u8		inline_base[IPV4_REASM_INLINE_MAX];
} ipv4_reasm_slot_t;

static ipv4_reasm_slot_t	g_slots[IPV4_REASM_SLOTS];
static u64			g_now_ms;
static int			g_initialized;

static u64
ipv4_reasm_now_ms(void)
{
	struct timespec	ts;

	if (!timer_is_initialized()) {
		return (g_now_ms);
	}
	nanouptime(&ts);
	return ((u64)ts.tv_sec * 1000ULL +
	    (u64)(ts.tv_nsec / 1000000L));
}

void
ipv4_reasm_init(void)
{
	int	i;

	for (i = 0; i < IPV4_REASM_SLOTS; i++) {
		memset(&g_slots[i], 0, sizeof(g_slots[i]));
	}
	g_now_ms = ipv4_reasm_now_ms();
	g_initialized = 1;
}

static void
ipv4_reasm_free_slot(ipv4_reasm_slot_t *slot)
{
	if (slot->heap) {
		kmem_free(slot->heap);
	}
	memset(slot, 0, sizeof(*slot));
}

static ipv4_reasm_slot_t *
ipv4_reasm_find_slot(net_iface_t *iface, u32 id, u32 src_ip, u32 dst_ip,
    u8 proto)
{
	ipv4_reasm_slot_t	*slot;
	ipv4_reasm_slot_t	*oldest;
	u64			oldest_ms;
	int			i;

	oldest = NULL;
	oldest_ms = 0;
	for (i = 0; i < IPV4_REASM_SLOTS; i++) {
		slot = &g_slots[i];
		if (!slot->used) {
			continue;
		}
		if (slot->iface == iface && slot->id == id &&
		    slot->src_ip == src_ip && slot->dst_ip == dst_ip &&
		    slot->proto == proto) {
			return (slot);
		}
		if (!oldest || slot->last_rx_ms < oldest_ms) {
			oldest = slot;
			oldest_ms = slot->last_rx_ms;
		}
	}

	for (i = 0; i < IPV4_REASM_SLOTS; i++) {
		if (!g_slots[i].used) {
			slot = &g_slots[i];
			memset(slot, 0, sizeof(*slot));
			slot->used = 1;
			return (slot);
		}
	}

	slot = oldest;
	ipv4_reasm_free_slot(slot);
	slot->used = 1;
	return (slot);
}


static int
ipv4_reasm_record_fragment(ipv4_reasm_slot_t *slot, u32 off, u32 len,
    const u8 *data)
{
	ipv4_reasm_frag_t	*frag;

	if (len == 0 || off + len < off) {
		return (0);
	}
	if (slot->fragment_count >= IPV4_REASM_MAX_FRAGMENTS) {
		return (0);
	}
	if (slot->stash_used + len > sizeof(slot->stash)) {
		return (0);
	}
	frag = &slot->frag[slot->fragment_count];
	frag->off = off;
	frag->len = len;
	frag->stash_off = slot->stash_used;
	memcpy(slot->stash + slot->stash_used, data, len);
	slot->stash_used += len;
	slot->fragment_count++;
	return (1);
}


static int
ipv4_reasm_coverage(ipv4_reasm_slot_t *slot)
{
	ipv4_reasm_frag_t	*frag;
	u32			cursor, cap, seg_end;
	int			i, j;

	for (i = 1; i < slot->fragment_count; i++) {
		frag = &slot->frag[i];
		j = i - 1;
		while (j >= 0 && slot->frag[j].off > frag->off) {
			slot->frag[j + 1] = slot->frag[j];
			j--;
		}
		slot->frag[j + 1] = *frag;
	}

	cursor = 0;
	cap = slot->total_data;
	for (i = 0; i < slot->fragment_count; i++) {
		seg_end = slot->frag[i].off + slot->frag[i].len;
		if (slot->frag[i].off > cursor) {
			return (0);
		}
		if (seg_end > cursor) {
			cursor = seg_end;
		}
		if (cursor >= cap) {
			return (1);
		}
	}
	return (cursor >= cap);
}


static int
ipv4_reasm_try_complete(ipv4_reasm_slot_t *slot, const u8 **out_datagram,
    u16 *out_len, u8 **out_buf)
{
	ipv4_reasm_frag_t	*frag;
	u8			*base;
	u32			cap;
	u32			i, j;

	if (!slot->closed || slot->total_data == 0) {
		return (0);
	}
	if (!ipv4_reasm_coverage(slot)) {
		return (0);
	}

	if (slot->total_data <= sizeof(slot->inline_base)) {
		base = slot->inline_base;
		cap = sizeof(slot->inline_base);
		*out_buf = NULL;
	} else {
		if (!slot->heap) {
			return (0);
		}
		base = slot->heap;
		cap = slot->total_data;
		*out_buf = base;
	}

	memset(base, 0, cap);
	for (i = 0; i < slot->fragment_count; i++) {
		frag = &slot->frag[i];
		for (j = 0; j < frag->len && frag->off + j < cap; j++) {
			if (base[frag->off + j] == 0) {
				base[frag->off + j] =
				    slot->stash[frag->stash_off + j];
			}
		}
	}

	*out_datagram = base;
	*out_len = (u16)slot->total_data;
	return (1);
}

int
ipv4_reasm_input(net_iface_t *iface, const u8 *data, u16 hdr_len,
    u16 payload_len, u32 id, u32 src_ip, u32 dst_ip, u8 proto,
    u16 frag_flags, u16 frag_off, const u8 **out_datagram,
    u16 *out_len, u8 **out_buf)
{
	ipv4_reasm_slot_t	*slot;
	u32			offset_bytes;
	u32			expected_total;
	int			completed;

	if (!out_datagram || !out_len || !out_buf) {
		return (-1);
	}
	*out_datagram = NULL;
	*out_len = 0;
	*out_buf = NULL;
	if (!g_initialized) {
		ipv4_reasm_init();
	}
	if (!data || hdr_len == 0) {
		return (-1);
	}

	offset_bytes = (u32)frag_off * 8u;

	if (frag_off == 0 && !(frag_flags & IPV4_MF)) {
		*out_datagram = data + hdr_len;
		*out_len = payload_len;
		*out_buf = NULL;
		return (1);
	}

	slot = ipv4_reasm_find_slot(iface, id, src_ip, dst_ip, proto);
	slot->iface = iface;
	slot->id = id;
	slot->src_ip = src_ip;
	slot->dst_ip = dst_ip;
	slot->proto = proto;
	slot->hdr_len = hdr_len;
	slot->last_rx_ms = ipv4_reasm_now_ms();

	if (!ipv4_reasm_record_fragment(slot, offset_bytes, payload_len,
	    data + hdr_len)) {
		ipv4_reasm_free_slot(slot);
		return (-1);
	}


	if (!(frag_flags & IPV4_MF)) {
		expected_total = offset_bytes + payload_len;
		if (!slot->closed) {
			slot->total_data = expected_total;
			slot->closed = 1;
		} else if (expected_total != slot->total_data) {
			ipv4_reasm_free_slot(slot);
			return (-1);
		}

		if (slot->total_data > sizeof(slot->inline_base) &&
		    !slot->heap) {
			slot->heap = kmem_alloc(slot->total_data);
			if (!slot->heap) {
				ipv4_reasm_free_slot(slot);
				return (-1);
			}
			memset(slot->heap, 0, slot->total_data);
		}
	} else if (!slot->closed && slot->total_data == 0 &&
	    slot->fragment_count >= 2) {

	}

	if (slot->closed) {
		completed = ipv4_reasm_try_complete(slot, out_datagram,
		    out_len, out_buf);
		if (completed) {
			ipv4_reasm_free_slot(slot);
			return (1);
		}
	}

	return (0);
}

void
ipv4_reasm_tick(void)
{
	ipv4_reasm_slot_t	*slot;
	u64			now;
	int			i;

	if (!g_initialized) {
		return;
	}
	now = ipv4_reasm_now_ms();
	g_now_ms = now;
	for (i = 0; i < IPV4_REASM_SLOTS; i++) {
		slot = &g_slots[i];
		if (!slot->used) {
			continue;
		}
		if (now - slot->last_rx_ms >= IPV4_REASM_TIMEOUT_MS) {
			ipv4_reasm_free_slot(slot);
		}
	}
}

void
ipv4_reasm_evict(net_iface_t *iface)
{
	ipv4_reasm_slot_t	*slot;
	int			i;

	if (!g_initialized) {
		return;
	}
	for (i = 0; i < IPV4_REASM_SLOTS; i++) {
		slot = &g_slots[i];
		if (slot->used && slot->iface == iface) {
			ipv4_reasm_free_slot(slot);
		}
	}
}
