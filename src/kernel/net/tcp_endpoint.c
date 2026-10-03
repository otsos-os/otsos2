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
$define %type s32 as 32 bit signed
$define %type int as 32 bit signed
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state
$define %type net_endpoint_tcp_txseg_t as one in-flight TCP transmit segment

$define %func net_endpoint_seq_after as function with args u32, u32
$define %func net_endpoint_seq_after_eq as function with args u32, u32
$define %func net_endpoint_tcp_new_isn as function with args void
$define %func net_endpoint_tcp_now_ms as function with args void
$define %func net_endpoint_tcp_set_state as procedure with args net_endpoint_t *, int
$define %func net_endpoint_tcp_can_send as function with args net_endpoint_t *
$define %func net_endpoint_tcp_window as function with args net_endpoint_t *, int
$define %func net_endpoint_tcp_window_raw as function with args net_endpoint_t *
$define %func net_endpoint_tcp_peer_window as function with args net_endpoint_t *
$define %func net_endpoint_tcp_send as function with args net_endpoint_t *, u16, const u8 *, u16
$define %func net_endpoint_tcp_send_ack as function with args net_endpoint_t *
$define %func net_endpoint_tcp_send_pending as function with args net_endpoint_t *
$define %func net_endpoint_tcp_flush as function with args net_endpoint_t *
$define %func net_endpoint_tcp_flight as function with args net_endpoint_t *
$define %func net_endpoint_tcp_send_limit as function with args net_endpoint_t *
$define %func net_endpoint_tcp_queue as function with args net_endpoint_t *, const u8 *, u16, u16, int
$define %func net_endpoint_tcp_dequeue_acked as function with args net_endpoint_t *, u32
$define %func net_endpoint_tcp_rtt_sample as procedure with args net_endpoint_t *, u64
$define %func net_endpoint_tcp_cwnd_grow as procedure with args net_endpoint_t *, u32
$define %func net_endpoint_tcp_cwnd_loss as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_ack_tx as procedure with args net_endpoint_t *, u32
$define %func net_endpoint_tcp_rx_push as function with args net_endpoint_t *, const u8 *, u16
$define %func net_endpoint_tcp_rx_pop as function with args net_endpoint_t *, u8 *, u32
$define %func net_endpoint_tcp_iface_mss as function with args net_iface_t *
$define %func net_endpoint_tcp_note_window as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_window_update as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_ooo_store as function with args net_endpoint_t *, u32, const u8 *, u16
$define %func net_endpoint_tcp_ooo_drain as function with args net_endpoint_t *
$define %func net_endpoint_tcp_ooo_reset as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_find as function with args net_iface_t *, u32, u32, u16, u16
$define %func net_endpoint_tcp_find_listener as function with args net_iface_t *, u32, u16
$define %func net_endpoint_tcp_child as function with args net_endpoint_t *, net_iface_t *, u32, u32, u16, u16, u32, u16, const u8 *, u16
$define %func net_endpoint_tcp_queue_accept as function with args net_endpoint_t *
$define %func net_endpoint_tcp_drop as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_init as procedure with args void
$define %func net_endpoint_tcp_connect as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_tcp_listen as function with args net_endpoint_t *, int
$define %func net_endpoint_tcp_accept as function with args net_endpoint_t *, net_endpoint_t **, net_endpoint_addr_t *, u32
$define %func net_endpoint_tcp_send_user as function with args net_endpoint_t *, const u8 *, u32, u32
$define %func net_endpoint_tcp_recv_user as function with args net_endpoint_t *, u8 *, u32, net_endpoint_addr_t *, u32, u32 *
$define %func net_endpoint_tcp_readable as function with args net_endpoint_t *
$define %func net_endpoint_tcp_writable as function with args net_endpoint_t *
$define %func net_endpoint_tcp_pending_bytes as function with args net_endpoint_t *
$define %func net_endpoint_tcp_write_space as function with args net_endpoint_t *
$define %func net_endpoint_tcp_begin_close as function with args net_endpoint_t *
$define %func net_endpoint_tcp_drop_children as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_free as procedure with args net_endpoint_t *
$define %func net_endpoint_tcp_input as function with args net_iface_t *, u32, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16, const u8 *, u16
$define %func net_endpoint_tcp_tick as procedure with args void

*/

/* !SPACE!

$space %internal net_endpoint_seq_after, net_endpoint_seq_after_eq
$space %internal net_endpoint_tcp_new_isn, net_endpoint_tcp_now_ms
$space %internal net_endpoint_tcp_set_state, net_endpoint_tcp_can_send
$space %internal net_endpoint_tcp_window, net_endpoint_tcp_window_raw
$space %internal net_endpoint_tcp_peer_window, net_endpoint_tcp_send
$space %internal net_endpoint_tcp_send_ack, net_endpoint_tcp_send_pending
$space %internal net_endpoint_tcp_flush, net_endpoint_tcp_flight
$space %internal net_endpoint_tcp_send_limit, net_endpoint_tcp_queue
$space %internal net_endpoint_tcp_dequeue_acked, net_endpoint_tcp_rtt_sample
$space %internal net_endpoint_tcp_cwnd_grow, net_endpoint_tcp_cwnd_loss
$space %internal net_endpoint_tcp_ack_tx
$space %internal net_endpoint_tcp_rx_push, net_endpoint_tcp_rx_pop
$space %internal net_endpoint_tcp_iface_mss, net_endpoint_tcp_note_window
$space %internal net_endpoint_tcp_window_update
$space %internal net_endpoint_tcp_ooo_store, net_endpoint_tcp_ooo_drain
$space %internal net_endpoint_tcp_ooo_reset
$space %internal net_endpoint_tcp_find, net_endpoint_tcp_find_listener
$space %internal net_endpoint_tcp_child, net_endpoint_tcp_queue_accept
$space %internal net_endpoint_tcp_drop, net_endpoint_tcp_free
$space %export net_endpoint_tcp_init, net_endpoint_tcp_connect
$space %export net_endpoint_tcp_listen, net_endpoint_tcp_accept
$space %export net_endpoint_tcp_send_user, net_endpoint_tcp_recv_user
$space %export net_endpoint_tcp_readable, net_endpoint_tcp_writable
$space %export net_endpoint_tcp_pending_bytes
$space %export net_endpoint_tcp_write_space
$space %export net_endpoint_tcp_begin_close
$space %export net_endpoint_tcp_drop_children
$space %export net_endpoint_tcp_input, net_endpoint_tcp_tick
$space %export net_endpoint_tcp_alloc_buffers, net_endpoint_tcp_release

*/

#include <kernel/api/errno.h>
#include <kernel/crypto/rng/rng.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <kernel/net/endpoint_internal.h>
#include <kernel/net/endpoint_hash.h>
#include <kernel/net/tcp.h>
#include <kernel/net/tcp_endpoint.h>
#include <kernel/process.h>
#include <kernel/time.h>
#include <mlibc/mlibc.h>
#include <mm/kmem.h>

static u32	g_tcp_next_isn;

static void	net_endpoint_tcp_window_update(net_endpoint_t *ep);


static u32
net_endpoint_tcp_new_isn(void)
{
	struct timespec	ts;
	u64		now_ns;
	u64		entropy;
	u32		isn;

	now_ns = 0;
	if (timer_is_initialized()) {
		nanouptime(&ts);
		now_ns = (u64)ts.tv_sec * 1000000000ULL +
		    (u64)ts.tv_nsec;
	}
	entropy = crypto_rng_u64();
	isn = g_tcp_next_isn;
	g_tcp_next_isn = (u32)(isn + 0x10101 +
	    (u32)(now_ns >> 10));
	isn ^= (u32)(entropy >> 32);
	isn ^= (u32)entropy;
	return (isn);
}


static u64
net_endpoint_tcp_now_ms(void)
{
	struct timespec	ts;

	if (!timer_is_initialized()) {
		return (0);
	}
	nanouptime(&ts);
	return ((u64)ts.tv_sec * 1000ULL + (u64)(ts.tv_nsec / 1000000L));
}

static int
net_endpoint_seq_after(u32 a, u32 b)
{
	return ((s32)(a - b) > 0);
}

static int
net_endpoint_seq_after_eq(u32 a, u32 b)
{
	return ((s32)(a - b) >= 0);
}

static u64
net_endpoint_tcp_ticks(void)
{
	if (!timer_is_initialized()) {
		return (1);
	}
	return (timer_get_ticks());
}

int
net_endpoint_tcp_alloc_buffers(net_endpoint_t *ep)
{
	u8	*buf;
	u32	size;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (0);
	}
	if (ep->tcp_rx) {
		return (0);
	}

	buf = (u8 *)kmem_alloc(NET_ENDPOINT_TCP_RX_SIZE);
	if (buf) {
		ep->tcp_rx = buf;
		ep->tcp_rx_size = NET_ENDPOINT_TCP_RX_SIZE;
	} else {
		/*
		 * Degrade to a small window rather than refusing the
		 * connection: a slow socket beats an unopenable one.
		 */
		buf = (u8 *)kmem_alloc(NET_ENDPOINT_TCP_RX_MIN);
		if (!buf) {
			return (-1);
		}
		ep->tcp_rx = buf;
		ep->tcp_rx_size = NET_ENDPOINT_TCP_RX_MIN;
	}


	ep->tcp_txseg = (net_endpoint_tcp_txseg_t *)kmem_alloc(
	    sizeof(net_endpoint_tcp_txseg_t) * NET_ENDPOINT_TCP_SNDSLOTS);
	if (!ep->tcp_txseg) {
		kmem_free(ep->tcp_rx);
		ep->tcp_rx = NULL;
		ep->tcp_rx_size = 0;
		return (-1);
	}
	memset(ep->tcp_txseg, 0,
	    sizeof(net_endpoint_tcp_txseg_t) * NET_ENDPOINT_TCP_SNDSLOTS);


	size = ep->tcp_rx_size;
	ep->tcp_snd_wscale = 0;
	while (size > 65535 && ep->tcp_snd_wscale < NET_ENDPOINT_TCP_WSCALE_MAX) {
		ep->tcp_snd_wscale++;
		size >>= 1;
	}

	ep->tcp_rx_head = 0;
	ep->tcp_rx_tail = 0;
	ep->tcp_rx_count = 0;
	ep->tcp_last_adv_win = 0;
	return (0);
}

static void
net_endpoint_tcp_ooo_reset(net_endpoint_t *ep)
{
	if (!ep || !ep->tcp_ooo) {
		return;
	}
	memset(ep->tcp_ooo, 0, sizeof(*ep->tcp_ooo));
}

void
net_endpoint_tcp_release(net_endpoint_t *ep)
{
	if (!ep) {
		return;
	}
	/*
	 * Must run before the endpoint slot is memset by net_endpoint_free,
	 * otherwise both pointers are lost and the heap leaks per connection.
	 */
	if (ep->tcp_rx) {
		kmem_free(ep->tcp_rx);
		ep->tcp_rx = NULL;
	}
	if (ep->tcp_ooo) {
		kmem_free(ep->tcp_ooo);
		ep->tcp_ooo = NULL;
	}
	if (ep->tcp_txseg) {
		kmem_free(ep->tcp_txseg);
		ep->tcp_txseg = NULL;
	}
	ep->tcp_rx_size = 0;
	ep->tcp_rx_head = 0;
	ep->tcp_rx_tail = 0;
	ep->tcp_rx_count = 0;
	ep->tcp_last_adv_win = 0;
	ep->txseg_head = 0;
	ep->txseg_count = 0;
}

static u16
net_endpoint_tcp_iface_mss(net_iface_t *iface)
{
	u32	mtu;


	mtu = ETHERNET_MTU;
	if (iface && iface->ndev && iface->ndev->mtu != 0) {
		mtu = iface->ndev->mtu;
	}
	if (mtu <= sizeof(ipv4_header_t) + TCP_HEADER_LEN + TCP_MSS_MIN) {
		return (TCP_MSS_MIN);
	}
	return ((u16)(mtu - sizeof(ipv4_header_t) - TCP_HEADER_LEN));
}

static void
net_endpoint_tcp_set_state(net_endpoint_t *ep, int state)
{
	u64	wait;
	u32	freq;

	if (!ep) {
		return;
	}

	ep->tcp_state = state;
	ep->tcp_deadline = 0;
	if (state != TCP_STATE_TIME_WAIT &&
	    !(state == TCP_STATE_FIN_WAIT_2 && ep->tcp_orphan)) {
		return;
	}

	freq = timer_is_initialized() ? timer_get_frequency() : 0;
	wait = freq == 0 ? 200 : (u64)freq * 2;
	ep->tcp_deadline = net_endpoint_tcp_ticks() + wait;
}

static int
net_endpoint_tcp_can_send(net_endpoint_t *ep)
{
	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (0);
	}
	return (ep->tcp_state == TCP_STATE_ESTABLISHED ||
	    ep->tcp_state == TCP_STATE_CLOSE_WAIT);
}


static u32
net_endpoint_tcp_window_raw(net_endpoint_t *ep)
{
	u32	space;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (0);
	}
	if (!ep->tcp_rx || ep->tcp_rx_size == 0) {
		return (0);
	}
	space = ep->tcp_rx_size - ep->tcp_rx_count;
	return (space);
}

static u16
net_endpoint_tcp_window(net_endpoint_t *ep, int on_syn)
{
	u32	space;
	u32	wire;

	space = net_endpoint_tcp_window_raw(ep);
	if (on_syn) {
		wire = space;
	} else {
		wire = space >> ep->tcp_snd_wscale;
	}
	if (wire > 65535) {
		wire = 65535;
	}
	return ((u16)wire);
}


static void
net_endpoint_tcp_note_window(net_endpoint_t *ep)
{
	if (!ep) {
		return;
	}
	ep->tcp_last_adv_win = net_endpoint_tcp_window(ep, 0);
}


static u32
net_endpoint_tcp_peer_window(net_endpoint_t *ep)
{
	if (!ep) {
		return (0);
	}
	return ((u32)ep->tcp_peer_win << ep->tcp_rcv_wscale);
}

static int
net_endpoint_tcp_send(net_endpoint_t *ep, u16 flags,
    const u8 *data, u16 len)
{
	net_iface_t	*iface;
	u8		opts[TCP_OPT_MAX_LEN];
	u16		opt_len, mss;
	u32		seq, ack;
	int		ifindex, ret;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->peer_ip == 0 || ep->peer_port == 0) {
		return (-1);
	}

	ifindex = ep->peer_ifindex;
	if (ifindex == NET_ENDPOINT_IF_AUTO) {
		ifindex = ep->ifindex;
	}
	iface = net_endpoint_route(ep->peer_ip, ep->local_ip, ifindex);
	if (!iface) {
		return (-1);
	}
	if (ep->local_ip == 0) {
		ep->local_ip = iface->ip_addr;
	}
	if (ep->ifindex == NET_ENDPOINT_IF_AUTO) {
		ep->ifindex = iface->index;
	}


	opt_len = 0;
	if (flags & TCP_FLAG_SYN) {
		mss = net_endpoint_tcp_iface_mss(iface);
		opts[0] = TCP_OPT_MSS;
		opts[1] = TCP_OPT_MSS_LEN;
		opts[2] = (u8)(mss >> 8);
		opts[3] = (u8)(mss & 0xFF);
		opt_len = TCP_OPT_MSS_LEN;
		opts[opt_len++] = TCP_OPT_WSCALE;
		opts[opt_len++] = TCP_OPT_WSCALE_LEN;
		opts[opt_len++] = ep->tcp_snd_wscale;
	}

	seq = ep->tcp_tx_seq;
	ack = ep->tcp_rcv_nxt;

	ret = tcp_output_opt(iface, ep->peer_ip, ep->local_port,
	    ep->peer_port, seq, ack, flags,
	    net_endpoint_tcp_window(ep, (flags & TCP_FLAG_SYN) ? 1 : 0),
	    opt_len ? opts : NULL, opt_len, data, len);
	if (ret == 0 || ret == NET_TX_PENDING) {
		net_endpoint_tcp_note_window(ep);
	}
	return (ret);
}

static int
net_endpoint_tcp_send_ack(net_endpoint_t *ep)
{
	u32	seq;
	int	ret;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (-1);
	}
	seq = ep->tcp_tx_seq;
	ep->tcp_tx_seq = ep->tcp_snd_nxt;
	ret = net_endpoint_tcp_send(ep, TCP_FLAG_ACK, NULL, 0);
	ep->tcp_tx_seq = seq;
	return (ret);
}


static int
net_endpoint_tcp_send_pending(net_endpoint_t *ep)
{
	u16	flags;
	int	ret;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (-1);
	}
	flags = ep->tcp_tx_flags;
	if (ep->tcp_tx_len != 0) {
		flags |= TCP_FLAG_ACK;
	}
	if (flags == 0 && ep->tcp_tx_len == 0) {
		return (0);
	}


	ret = net_endpoint_tcp_send(ep, flags,
	    ep->tcp_tx_len ? ep->tcp_tx : NULL, (u16)ep->tcp_tx_len);
	if (ret == 0 || ret == NET_TX_PENDING) {
		ep->tcp_last_tx = timer_is_initialized() ?
		    timer_get_ticks() : 1;
		ep->tcp_last_rto_arm = net_endpoint_tcp_now_ms();
		return (0);
	}
	return (-1);
}


static u32
net_endpoint_tcp_flight(net_endpoint_t *ep)
{
	if (!ep) {
		return (0);
	}
	return (ep->tcp_snd_nxt - ep->tcp_snd_una);
}

static u32
net_endpoint_tcp_send_limit(net_endpoint_t *ep)
{
	u32	peer;
	u32	cwnd;

	peer = net_endpoint_tcp_peer_window(ep);
	cwnd = ep->tcp_cwnd;
	if (peer == 0) {
		return (0);
	}
	if (cwnd > peer) {
		return (peer);
	}
	return (cwnd);
}


static int
net_endpoint_tcp_flush(net_endpoint_t *ep)
{
	net_endpoint_tcp_txseg_t	*seg;
	u32				flight;
	u32				limit;
	u32				pewnd;
	int				ret;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    !ep->tcp_txseg) {
		return (-1);
	}

	while (ep->txseg_count != 0) {
		seg = &ep->tcp_txseg[ep->txseg_head];
		if (seg->in_flight) {
			break;
		}

		flight = net_endpoint_tcp_flight(ep);
		limit = net_endpoint_tcp_send_limit(ep);
		pewnd = net_endpoint_tcp_peer_window(ep);

		if (flight >= limit && !(pewnd == 0 && flight == 0)) {
			break;
		}

		ep->tcp_tx_seq = seg->seq;
		ret = net_endpoint_tcp_send(ep, seg->flags,
		    seg->len ? seg->data : NULL, seg->len);
		if (ret != 0 && ret != NET_TX_PENDING) {
			break;
		}
		seg->in_flight = 1;
		seg->sent = net_endpoint_tcp_now_ms();
		ep->tcp_last_tx = timer_is_initialized() ?
		    timer_get_ticks() : 1;
		ep->tcp_last_rto_arm = seg->sent;
		ep->tcp_rtseq_pending = 1;
	}
	return (0);
}


static int
net_endpoint_tcp_queue(net_endpoint_t *ep, const u8 *data,
    u16 len, u16 flags, int fin)
{
	net_endpoint_tcp_txseg_t	*seg;
	int				slot;

	if (!ep->tcp_txseg ||
	    ep->txseg_count >= NET_ENDPOINT_TCP_SNDSLOTS) {
		return (-1);
	}
	slot = (ep->txseg_head + ep->txseg_count) % NET_ENDPOINT_TCP_SNDSLOTS;
	seg = &ep->tcp_txseg[slot];
	memset(seg, 0, sizeof(*seg));
	seg->seq = ep->tcp_snd_nxt;
	seg->len = len;
	seg->flags = flags;
	seg->fin = fin;
	if (len != 0) {
		memcpy(seg->data, data, len);
	}
	ep->tcp_snd_nxt += len;
	if (fin) {
		ep->tcp_snd_nxt += 1;
	}
	ep->txseg_count++;
	return (slot);
}

static u32
net_endpoint_tcp_dequeue_acked(net_endpoint_t *ep, u32 ack)
{
	net_endpoint_tcp_txseg_t	*seg;
	u32				released;
	u32				seg_end;
	int				slot;

	if (!ep->tcp_txseg) {
		return (0);
	}
	released = 0;
	while (ep->txseg_count != 0) {
		slot = ep->txseg_head;
		seg = &ep->tcp_txseg[slot];
		if (!seg->in_flight) {
			break;
		}
		seg_end = seg->seq + seg->len + (seg->fin ? 1 : 0);
		if (!net_endpoint_seq_after_eq(ack, seg_end)) {
			break;
		}
		released += seg->len + (seg->fin ? 1 : 0);
		ep->txseg_head = (ep->txseg_head + 1) %
		    NET_ENDPOINT_TCP_SNDSLOTS;
		ep->txseg_count--;
		memset(seg, 0, sizeof(*seg));
	}
	return (released);
}

static void
net_endpoint_tcp_rtt_sample(net_endpoint_t *ep, u64 now_ms)
{
	u32	delta;
	u32	diff;
	u32	var;

	if (ep->tcp_rtseq_pending == 0 || ep->tcp_last_rto_arm == 0) {
		return;
	}
	delta = (u32)(now_ms - ep->tcp_last_rto_arm);
	ep->tcp_rtseq_pending = 0;
	ep->tcp_last_rto_arm = 0;

	if (ep->tcp_srtt_ms == 0) {
		ep->tcp_srtt_ms = delta;
		ep->tcp_rttvar_ms = delta / 2;
	} else {
		diff = delta > ep->tcp_srtt_ms ?
		    delta - ep->tcp_srtt_ms : ep->tcp_srtt_ms - delta;
		var = ep->tcp_rttvar_ms;
		ep->tcp_rttvar_ms = (var - (var >>
		    NET_ENDPOINT_TCP_SRTT_BETA)) +
		    (diff >> NET_ENDPOINT_TCP_SRTT_BETA);
		ep->tcp_srtt_ms = ep->tcp_srtt_ms -
		    (ep->tcp_srtt_ms >> NET_ENDPOINT_TCP_SRTT_ALPHA) +
		    (delta >> NET_ENDPOINT_TCP_SRTT_ALPHA);
	}

	ep->tcp_rto_ms = ep->tcp_srtt_ms +
	    (ep->tcp_rttvar_ms << NET_ENDPOINT_TCP_K);
	if (ep->tcp_rto_ms < NET_ENDPOINT_TCP_RTO_MIN_MS) {
		ep->tcp_rto_ms = NET_ENDPOINT_TCP_RTO_MIN_MS;
	}
	if (ep->tcp_rto_ms > NET_ENDPOINT_TCP_RTO_MAX_MS) {
		ep->tcp_rto_ms = NET_ENDPOINT_TCP_RTO_MAX_MS;
	}
}


static void
net_endpoint_tcp_cwnd_grow(net_endpoint_t *ep, u32 acked)
{
	u32	cwnd;
	u32	mss;

	if (acked == 0) {
		return;
	}
	cwnd = ep->tcp_cwnd;
	mss = ep->tcp_peer_mss ? ep->tcp_peer_mss : TCP_MSS_DEFAULT;
	if (cwnd < ep->tcp_ssthresh) {
		cwnd += mss;
		if (cwnd < ep->tcp_ssthresh + mss) {
			ep->tcp_cwnd = cwnd;
		} else {
			ep->tcp_cwnd = ep->tcp_ssthresh;
		}
	} else {
		cwnd += (mss * mss) / (cwnd ? cwnd : 1);
		ep->tcp_cwnd = cwnd;
	}
	if (ep->tcp_cwnd > NET_ENDPOINT_TCP_SNDBUF) {
		ep->tcp_cwnd = NET_ENDPOINT_TCP_SNDBUF;
	}
}


static void
net_endpoint_tcp_cwnd_loss(net_endpoint_t *ep)
{
	u32	cwnd;
	u32	mss;

	mss = ep->tcp_peer_mss ? ep->tcp_peer_mss : TCP_MSS_DEFAULT;
	cwnd = ep->tcp_cwnd;
	ep->tcp_ssthresh = (cwnd / 2) > (2 * mss) ? (cwnd / 2) : (2 * mss);
	ep->tcp_cwnd = ep->tcp_ssthresh;
	ep->tcp_recover = ep->tcp_snd_nxt;
}


static void
net_endpoint_tcp_ack_tx(net_endpoint_t *ep, u32 ack)
{
	u32	acked;
	u32	now_ms;
	u16	txflags;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return;
	}

	if (net_endpoint_seq_after(ack, ep->tcp_snd_nxt)) {
		return;
	}
	if (!net_endpoint_seq_after(ack, ep->tcp_snd_una)) {
		return;
	}

	now_ms = net_endpoint_tcp_now_ms();
	acked = ack - ep->tcp_snd_una;
	txflags = ep->tcp_tx_flags;
	ep->tcp_snd_una = ack;
	(void)net_endpoint_tcp_dequeue_acked(ep, ack);

	net_endpoint_tcp_rtt_sample(ep, now_ms);
	net_endpoint_tcp_cwnd_grow(ep, acked);


	if (ep->tcp_dupacks != 0 &&
	    net_endpoint_seq_after(ack, ep->tcp_recover)) {
		ep->tcp_dupacks = 0;
		ep->tcp_recover = 0;
	}

	if (ep->tcp_close_pending) {
		net_endpoint_tcp_begin_close(ep);
		return;
	}

	if (txflags != 0) {
		if (net_endpoint_seq_after_eq(ack, ep->tcp_snd_nxt)) {
			ep->tcp_tx_flags = 0;
			if (txflags & TCP_FLAG_FIN) {
				if (ep->tcp_state == TCP_STATE_FIN_WAIT_1) {
					net_endpoint_tcp_set_state(ep,
					    TCP_STATE_FIN_WAIT_2);
				} else if (ep->tcp_state == TCP_STATE_CLOSING) {
					net_endpoint_tcp_set_state(ep,
					    TCP_STATE_TIME_WAIT);
				} else if (ep->tcp_state == TCP_STATE_LAST_ACK) {
					net_endpoint_tcp_set_state(ep,
					    TCP_STATE_CLOSED);
				}
			}
		}
	}

	net_endpoint_tcp_flush(ep);
	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
}

static int
net_endpoint_tcp_rx_push(net_endpoint_t *ep, const u8 *data, u16 len)
{
	u32	space, first, to_copy;

	if (!ep || !data || len == 0) {
		return (0);
	}
	if (!ep->tcp_rx || ep->tcp_rx_size == 0) {
		return (0);
	}
	space = ep->tcp_rx_size - ep->tcp_rx_count;
	if (space == 0) {
		return (0);
	}
	to_copy = space;
	if (to_copy > len) {
		to_copy = len;
	}

	first = ep->tcp_rx_size - ep->tcp_rx_tail;
	if (first > to_copy) {
		first = to_copy;
	}
	memcpy(ep->tcp_rx + ep->tcp_rx_tail, data, first);
	if (to_copy > first) {
		memcpy(ep->tcp_rx, data + first, to_copy - first);
	}
	ep->tcp_rx_tail = (ep->tcp_rx_tail + to_copy) % ep->tcp_rx_size;
	ep->tcp_rx_count += to_copy;
	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
	return ((int)to_copy);
}

static int
net_endpoint_tcp_rx_pop(net_endpoint_t *ep, u8 *buf, u32 len)
{
	u32	to_copy, first;

	if (!ep || !buf || len == 0 || ep->tcp_rx_count == 0) {
		return (0);
	}
	if (!ep->tcp_rx || ep->tcp_rx_size == 0) {
		return (0);
	}
	to_copy = ep->tcp_rx_count;
	if (to_copy > len) {
		to_copy = len;
	}

	first = ep->tcp_rx_size - ep->tcp_rx_head;
	if (first > to_copy) {
		first = to_copy;
	}
	memcpy(buf, ep->tcp_rx + ep->tcp_rx_head, first);
	if (to_copy > first) {
		memcpy(buf + first, ep->tcp_rx, to_copy - first);
	}
	ep->tcp_rx_head = (ep->tcp_rx_head + to_copy) % ep->tcp_rx_size;
	ep->tcp_rx_count -= to_copy;
	/*
	 * Space just came free — tell the peer.  Without this the sender
	 * never learns the window reopened and the transfer stalls.
	 */
	net_endpoint_tcp_window_update(ep);
	event_notify_net_change(ep);
	return ((int)to_copy);
}

/*
 * Park a segment that arrived ahead of rcv_nxt.  Returns 1 when the
 * segment is held, 0 when it was dropped (no room, too large, or a
 * duplicate) — a dropped segment costs one peer retransmit, never
 * corruption, because rcv_nxt is untouched either way.
 */
static int
net_endpoint_tcp_ooo_store(net_endpoint_t *ep, u32 seq, const u8 *data,
    u16 len)
{
	int	i, free_slot;

	if (!ep || !data || len == 0 ||
	    len > NET_ENDPOINT_TCP_OOO_SEG_SIZE) {
		return (0);
	}
	/*
	 * Refuse to hold more than the receive ring could ever absorb;
	 * otherwise a peer streaming past a permanent hole would pin
	 * memory indefinitely.
	 */
	if (ep->tcp_rx_size == 0 ||
	    (u32)(seq - ep->tcp_rcv_nxt) >= ep->tcp_rx_size) {
		return (0);
	}

	if (!ep->tcp_ooo) {
		ep->tcp_ooo = (net_endpoint_tcp_ooo_t *)
		    kmem_alloc(sizeof(*ep->tcp_ooo));
		if (!ep->tcp_ooo) {
			return (0);
		}
		memset(ep->tcp_ooo, 0, sizeof(*ep->tcp_ooo));
	}

	free_slot = -1;
	for (i = 0; i < NET_ENDPOINT_TCP_OOO_SEGS; i++) {
		if (!ep->tcp_ooo->seg[i].used) {
			if (free_slot < 0) {
				free_slot = i;
			}
			continue;
		}
		/* Already holding this range: keep the first copy. */
		if (ep->tcp_ooo->seg[i].seq == seq &&
		    ep->tcp_ooo->seg[i].len >= len) {
			return (1);
		}
	}
	if (free_slot < 0) {
		return (0);
	}

	memcpy(ep->tcp_ooo->seg[free_slot].data, data, len);
	ep->tcp_ooo->seg[free_slot].seq = seq;
	ep->tcp_ooo->seg[free_slot].len = len;
	ep->tcp_ooo->seg[free_slot].used = 1;
	ep->tcp_ooo->count++;
	return (1);
}

/*
 * Feed held segments into the ring for as long as they are contiguous with
 * rcv_nxt.  Returns the number of bytes handed over so the caller knows
 * rcv_nxt moved.
 */
static u32
net_endpoint_tcp_ooo_drain(net_endpoint_t *ep)
{
	net_endpoint_tcp_ooo_seg_t	*seg;
	u32				advanced, skip, seq_off;
	u32				rem;
	int				i, progress;
	int				got;

	if (!ep || !ep->tcp_ooo || ep->tcp_ooo->count == 0) {
		return (0);
	}

	advanced = 0;
	do {
		progress = 0;
		for (i = 0; i < NET_ENDPOINT_TCP_OOO_SEGS; i++) {
			seg = &ep->tcp_ooo->seg[i];
			if (!seg->used) {
				continue;
			}
			/* Fully below the window: stale duplicate. */
			if (net_endpoint_seq_after_eq(ep->tcp_rcv_nxt,
			    seg->seq + seg->len)) {
				seg->used = 0;
				ep->tcp_ooo->count--;
				progress = 1;
				continue;
			}
			if (net_endpoint_seq_after(seg->seq,
			    ep->tcp_rcv_nxt)) {
				continue;
			}
			/*
			 * Overlaps rcv_nxt: drop the part already consumed
			 * and push only what is new.
			 */
			seq_off = ep->tcp_rcv_nxt - seg->seq;
			skip = seq_off;
			if (skip >= seg->len) {
				seg->used = 0;
				ep->tcp_ooo->count--;
				progress = 1;
				continue;
			}
			rem = seg->len - skip;
			got = net_endpoint_tcp_rx_push(ep,
			    seg->data + skip, (u16)rem);
			if (got <= 0) {
				continue;
			}
			ep->tcp_rcv_nxt += got;
			advanced += got;
			if ((u32)got < rem) {

				seg->seq += got;
				seg->len -= got;
				memmove(seg->data, seg->data + got, seg->len);
				continue;
			}
			seg->used = 0;
			ep->tcp_ooo->count--;
			progress = 1;
		}
	} while (progress && ep->tcp_ooo->count > 0);

	return (advanced);
}

static net_endpoint_t *
net_endpoint_tcp_find(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port)
{
	net_endpoint_t	*ep;

	ep = net_endpoint_find(dst_ip, src_ip, dst_port, src_port);
	if (!ep) {
		return (NULL);
	}
	if (ep->ifindex != NET_ENDPOINT_IF_AUTO &&
	    (!iface || ep->ifindex != iface->index)) {
		return (NULL);
	}
	return (ep);
}

static net_endpoint_t *
net_endpoint_tcp_find_listener(net_iface_t *iface, u32 dst_ip, u16 dst_port)
{
	net_endpoint_t	*ep;

	ep = net_endpoint_find_listener(dst_ip, dst_port);
	if (!ep) {
		return (NULL);
	}
	if (ep->ifindex != NET_ENDPOINT_IF_AUTO &&
	    (!iface || ep->ifindex != iface->index)) {
		return (NULL);
	}
	return (ep);
}

static int
net_endpoint_tcp_child(net_endpoint_t *listener, net_iface_t *iface,
    u32 src_ip, u32 dst_ip, u16 src_port, u16 dst_port, u32 seq,
    u16 window, const u8 *opts, u16 opt_len)
{
	net_endpoint_t	*child;
	u16		peer_mss;
	u8		peer_wscale;
	int		i;

	if (!listener || !iface ||
	    listener->tcp_accept_count >= listener->tcp_backlog) {
		return (-1);
	}

	child = NULL;
	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		if (!g_endpoints[i].used) {
			child = &g_endpoints[i];
			break;
		}
	}
	if (!child) {
		return (-1);
	}

	memset(child, 0, sizeof(*child));
	child->used = 1;
	child->proto = NET_ENDPOINT_PROTO_TCP;
	child->mode = NET_ENDPOINT_MODE_STREAM;
	child->flags = listener->flags;
	child->local_ip = dst_ip;
	child->peer_ip = src_ip;
	child->local_port = dst_port;
	child->peer_port = src_port;
	child->ifindex = iface->index;
	child->peer_ifindex = iface->index;
	child->tcp_parent = (int)(listener - g_endpoints);
	child->tcp_state = TCP_STATE_SYN_RECEIVED;
	child->hash_next_listener = NET_ENDPOINT_HASH_NONE;
	child->hash_next_connected = NET_ENDPOINT_HASH_NONE;
	child->tcp_irs = seq;
	child->tcp_rcv_nxt = seq + 1;
	child->tcp_iss = net_endpoint_tcp_new_isn();
	child->tcp_snd_una = child->tcp_iss;
	child->tcp_snd_nxt = child->tcp_iss + 1;
	child->tcp_peer_win = window;
	child->tcp_tx_seq = child->tcp_iss;
	child->tcp_tx_flags = TCP_FLAG_SYN | TCP_FLAG_ACK;
	child->tcp_cwnd = TCP_MSS_DEFAULT;
	child->tcp_ssthresh = NET_ENDPOINT_TCP_SNDBUF;
	child->tcp_rto_ms = NET_ENDPOINT_TCP_RTO_INIT_MS;

	peer_mss = TCP_MSS_DEFAULT;
	(void)tcp_opt_get_mss(opts, opt_len, &peer_mss);
	child->tcp_peer_mss = peer_mss;
	peer_wscale = 0;
	(void)tcp_opt_get_wscale(opts, opt_len, &peer_wscale);
	if (peer_wscale > NET_ENDPOINT_TCP_WSCALE_MAX) {
		peer_wscale = NET_ENDPOINT_TCP_WSCALE_MAX;
	}
	child->tcp_rcv_wscale = peer_wscale;

	child->tcp_cwnd = 10u * peer_mss;
	if (child->tcp_cwnd > NET_ENDPOINT_TCP_SNDBUF) {
		child->tcp_cwnd = NET_ENDPOINT_TCP_SNDBUF;
	}


	if (net_endpoint_tcp_alloc_buffers(child) != 0) {
		memset(child, 0, sizeof(*child));
		return (-1);
	}

	net_endpoint_hash_insert(child);

	if (net_endpoint_tcp_send_pending(child) != 0) {
		net_endpoint_tcp_release(child);
		memset(child, 0, sizeof(*child));
		return (-1);
	}
	return (0);
}

static int
net_endpoint_tcp_queue_accept(net_endpoint_t *ep)
{
	net_endpoint_t	*parent;
	int		slot;

	if (!ep || ep->tcp_parent < 0 ||
	    ep->tcp_parent >= NET_ENDPOINT_MAX) {
		return (-1);
	}
	parent = &g_endpoints[ep->tcp_parent];
	if (!parent->used || parent->tcp_state != TCP_STATE_LISTEN ||
	    parent->tcp_accept_count >= parent->tcp_backlog) {
		return (-1);
	}

	slot = parent->tcp_accept_tail;
	parent->tcp_accept_queue[slot] = (int)(ep - g_endpoints);
	parent->tcp_accept_tail = (u16)((parent->tcp_accept_tail + 1) %
	    NET_ENDPOINT_TCP_ACCEPT_QUEUE);
	parent->tcp_accept_count++;
	ep->tcp_parent = -1;

	proc_wakeup((void *)parent);
	event_notify_net_change(parent);
	return (0);
}

static void
net_endpoint_tcp_free(net_endpoint_t *ep)
{
	net_endpoint_free(ep);
}

static void
net_endpoint_tcp_drop(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return;
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP &&
	    ep->tcp_state != TCP_STATE_CLOSED &&
	    ep->peer_ip != 0 && ep->peer_port != 0) {
		ep->tcp_tx_seq = ep->tcp_snd_nxt;
		net_endpoint_tcp_send(ep, TCP_FLAG_RST | TCP_FLAG_ACK,
		    NULL, 0);
	}
	net_endpoint_tcp_free(ep);
}

void
net_endpoint_tcp_drop_children(net_endpoint_t *parent)
{
	net_endpoint_t	*ep;
	int		i;

	if (!parent) {
		return;
	}
	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_TCP) {
			continue;
		}
		if (ep->tcp_parent == (int)(parent - g_endpoints)) {
			net_endpoint_tcp_drop(ep);
		}
	}
	while (parent->tcp_accept_count > 0) {
		i = parent->tcp_accept_queue[parent->tcp_accept_head];
		parent->tcp_accept_head = (u16)((parent->tcp_accept_head + 1) %
		    NET_ENDPOINT_TCP_ACCEPT_QUEUE);
		parent->tcp_accept_count--;
		if (i >= 0 && i < NET_ENDPOINT_MAX) {
			net_endpoint_tcp_drop(&g_endpoints[i]);
		}
	}
}

void
net_endpoint_tcp_init(void)
{

	g_tcp_next_isn = (u32)(crypto_rng_u64() >> 16);
}


int
net_endpoint_tcp_active(void)
{
	int	i;

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		if (!g_endpoints[i].used ||
		    g_endpoints[i].proto != NET_ENDPOINT_PROTO_TCP) {
			continue;
		}
		if (g_endpoints[i].txseg_count != 0 ||
		    g_endpoints[i].tcp_tx_flags != 0) {
			return (1);
		}
	}
	return (0);
}

int
net_endpoint_tcp_connect(net_endpoint_t *ep,
    const net_endpoint_addr_t *addr)
{
	net_iface_t	*iface;
	int		ifindex, port;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->mode != NET_ENDPOINT_MODE_STREAM) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (ep->tcp_state != TCP_STATE_CLOSED ||
	    ep->peer_ip != 0 || ep->peer_port != 0) {
		return (-API_ERR_BUSY);
	}

	ifindex = addr->ifindex;
	if (ifindex == NET_ENDPOINT_IF_AUTO) {
		ifindex = ep->ifindex;
	}
	iface = net_endpoint_route(addr->ip, ep->local_ip, ifindex);
	if (!iface) {
		return (-API_ERR_NO_DEVICE);
	}
	if (ep->local_ip == 0) {
		ep->local_ip = iface->ip_addr;
	}
	if (ep->ifindex == NET_ENDPOINT_IF_AUTO) {
		ep->ifindex = iface->index;
	}
	if (ep->local_port == 0) {
		port = net_endpoint_alloc_port(ep, ep->local_ip);
		if (port < 0) {
			return (port);
		}
		ep->local_port = (u16)port;
	} else if (net_endpoint_bind_conflict(ep, ep->local_ip,
	    ep->local_port)) {
		return (-API_ERR_BUSY);
	}

	if (net_endpoint_tcp_alloc_buffers(ep) != 0) {
		return (-API_ERR_NO_MEMORY);
	}

	ep->peer_ip = addr->ip;
	ep->peer_port = addr->port;
	ep->peer_ifindex = iface->index;
	ep->tcp_state = TCP_STATE_SYN_SENT;
	ep->tcp_error = 0;
	ep->tcp_peer_mss = TCP_MSS_DEFAULT;
	ep->tcp_iss = net_endpoint_tcp_new_isn();
	ep->tcp_snd_una = ep->tcp_iss;
	ep->tcp_snd_nxt = ep->tcp_iss + 1;
	ep->tcp_tx_seq = ep->tcp_iss;
	ep->tcp_tx_flags = TCP_FLAG_SYN;
	ep->tcp_retries = 0;
	ep->tcp_cwnd = TCP_MSS_DEFAULT;
	ep->tcp_ssthresh = NET_ENDPOINT_TCP_SNDBUF;
	ep->tcp_rto_ms = NET_ENDPOINT_TCP_RTO_INIT_MS;
	net_endpoint_hash_insert(ep);

	if (net_endpoint_tcp_send_pending(ep) != 0) {
		ep->tcp_state = TCP_STATE_CLOSED;
		ep->peer_ip = 0;
		ep->peer_port = 0;
		ep->tcp_tx_flags = 0;
		net_endpoint_hash_remove(ep);
		return (-API_ERR_IO);
	}

	if (ep->tcp_state == TCP_STATE_ESTABLISHED) {
		return (0);
	}
	if (ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) {
		return (-API_ERR_RETRY);
	}
	while (ep->used && ep->tcp_state == TCP_STATE_SYN_SENT) {
		proc_sleep((void *)ep);
	}
	if (!ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->tcp_state == TCP_STATE_ESTABLISHED) {
		return (0);
	}
	return (ep->tcp_error ? -ep->tcp_error : -API_ERR_IO);
}

int
net_endpoint_tcp_listen(net_endpoint_t *ep, int backlog)
{
	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->mode != NET_ENDPOINT_MODE_STREAM) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (ep->local_port == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (ep->tcp_state != TCP_STATE_CLOSED) {
		return (-API_ERR_BUSY);
	}
	if (backlog <= 0) {
		backlog = 1;
	}
	if (backlog > NET_ENDPOINT_TCP_ACCEPT_QUEUE) {
		backlog = NET_ENDPOINT_TCP_ACCEPT_QUEUE;
	}

	ep->tcp_state = TCP_STATE_LISTEN;
	ep->tcp_backlog = (u16)backlog;
	ep->tcp_accept_head = 0;
	ep->tcp_accept_tail = 0;
	ep->tcp_accept_count = 0;
	net_endpoint_hash_insert(ep);
	return (0);
}

int
net_endpoint_tcp_accept(net_endpoint_t *ep, net_endpoint_t **out_ep,
    net_endpoint_addr_t *addr, u32 flags)
{
	net_endpoint_t	*child;
	int		idx;

	if (!out_ep) {
		return (-API_ERR_BAD_ADDR);
	}
	*out_ep = NULL;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (flags & ~NET_ENDPOINT_MSG_NONBLOCK) {
		return (-API_ERR_BAD_VALUE);
	}
	if (ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->mode != NET_ENDPOINT_MODE_STREAM ||
	    ep->tcp_state != TCP_STATE_LISTEN) {
		return (-API_ERR_BAD_VALUE);
	}

	while (ep->tcp_accept_count == 0) {
		if ((ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) ||
		    (flags & NET_ENDPOINT_MSG_NONBLOCK)) {
			return (-API_ERR_RETRY);
		}
		proc_sleep((void *)ep);
		if (!ep->used) {
			return (-API_ERR_BAD_HANDLE);
		}
	}

	idx = ep->tcp_accept_queue[ep->tcp_accept_head];
	ep->tcp_accept_head = (u16)((ep->tcp_accept_head + 1) %
	    NET_ENDPOINT_TCP_ACCEPT_QUEUE);
	ep->tcp_accept_count--;
	if (idx < 0 || idx >= NET_ENDPOINT_MAX ||
	    !g_endpoints[idx].used) {
		return (-API_ERR_IO);
	}

	child = &g_endpoints[idx];
	if (addr) {
		addr->family = NET_ENDPOINT_ADDR_IP4;
		addr->ip = child->peer_ip;
		addr->port = child->peer_port;
		addr->ifindex = child->ifindex;
	}
	*out_ep = child;
	event_notify_net_change(ep);
	return (0);
}

int
net_endpoint_tcp_send_user(net_endpoint_t *ep, const u8 *data,
    u32 len, u32 flags)
{
	u32	avail;
	u32	flight;
	u32	limit;
	u32	space;
	u32	to_send;
	u32	sent;
	int	ret;

	if (flags & ~NET_ENDPOINT_MSG_NONBLOCK) {
		return (-API_ERR_BAD_VALUE);
	}
	if (len == 0) {
		return (0);
	}
	if (!net_endpoint_tcp_can_send(ep)) {
		return (-API_ERR_PIPE_CLOSED);
	}


	sent = 0;
	while (sent < len) {
		avail = len - sent;
		to_send = avail > NET_ENDPOINT_TCP_TX_SIZE ?
		    NET_ENDPOINT_TCP_TX_SIZE : avail;
		if (ep->tcp_peer_mss != 0 && to_send > ep->tcp_peer_mss) {
			to_send = ep->tcp_peer_mss;
		}
		if (to_send == 0) {
			break;
		}

		flight = net_endpoint_tcp_flight(ep);
		limit = net_endpoint_tcp_send_limit(ep);
		space = limit > flight ? limit - flight : 0;
		if (space == 0) {

			if (flight == 0 &&
			    net_endpoint_tcp_peer_window(ep) == 0 &&
			    ep->txseg_count == 0) {
				space = 1;
			} else {
				break;
			}
		}
		if (to_send > space) {
			to_send = space;
		}

		ret = net_endpoint_tcp_queue(ep, data + sent,
		    (u16)to_send, TCP_FLAG_ACK | TCP_FLAG_PSH, 0);
		if (ret < 0) {
			break;
		}
		sent += to_send;
	}

	if (sent != 0) {
		(void)net_endpoint_tcp_flush(ep);
		return ((int)sent);
	}


	if ((ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) ||
	    (flags & NET_ENDPOINT_MSG_NONBLOCK)) {
		return (-API_ERR_RETRY);
	}
	while (ep->used && net_endpoint_tcp_can_send(ep) &&
	    net_endpoint_tcp_flight(ep) >= net_endpoint_tcp_send_limit(ep)) {
		proc_sleep((void *)ep);
	}
	if (!ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (!net_endpoint_tcp_can_send(ep)) {
		return (-API_ERR_PIPE_CLOSED);
	}
	return (-API_ERR_RETRY);
}

int
net_endpoint_tcp_recv_user(net_endpoint_t *ep, u8 *buf, u32 len,
    net_endpoint_addr_t *addr, u32 flags, u32 *out_flags)
{
	if (flags & ~(NET_ENDPOINT_MSG_NONBLOCK |
	    NET_ENDPOINT_MSG_TRUNC)) {
		return (-API_ERR_BAD_VALUE);
	}
	if (out_flags) {
		*out_flags = 0;
	}
	if (addr) {
		addr->family = NET_ENDPOINT_ADDR_IP4;
		addr->ip = ep->peer_ip;
		addr->port = ep->peer_port;
		addr->ifindex = ep->peer_ifindex;
	}
	if (len == 0) {
		return (0);
	}
	while (ep->tcp_rx_count == 0) {
		if (ep->tcp_state == TCP_STATE_CLOSE_WAIT ||
		    ep->tcp_state == TCP_STATE_CLOSED) {
			if (ep->tcp_error != 0) {
				return (-ep->tcp_error);
			}
			return (0);
		}
		if ((ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) ||
		    (flags & NET_ENDPOINT_MSG_NONBLOCK)) {
			return (-API_ERR_RETRY);
		}
		proc_sleep((void *)ep);
		if (!ep->used) {
			return (-API_ERR_BAD_HANDLE);
		}
	}
	return (net_endpoint_tcp_rx_pop(ep, buf, len));
}

int
net_endpoint_tcp_readable(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	if (ep->tcp_state == TCP_STATE_LISTEN) {
		return (ep->tcp_accept_count > 0);
	}
	return (ep->tcp_rx_count > 0 ||
	    ep->tcp_state == TCP_STATE_CLOSE_WAIT ||
	    ep->tcp_state == TCP_STATE_CLOSED);
}

int
net_endpoint_tcp_writable(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	return (net_endpoint_tcp_can_send(ep) &&
	    ep->txseg_count < NET_ENDPOINT_TCP_SNDSLOTS);
}

u32
net_endpoint_tcp_pending_bytes(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	if (ep->tcp_state == TCP_STATE_LISTEN) {
		return (ep->tcp_accept_count);
	}
	return (ep->tcp_rx_count);
}

u32
net_endpoint_tcp_write_space(net_endpoint_t *ep)
{
	u32	slots;

	if (!net_endpoint_tcp_writable(ep)) {
		return (0);
	}
	slots = NET_ENDPOINT_TCP_SNDSLOTS - ep->txseg_count;
	return (slots * NET_ENDPOINT_TCP_TX_SIZE);
}

int
net_endpoint_tcp_begin_close(net_endpoint_t *ep)
{
	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (0);
	}

	ep->tcp_orphan = 1;
	if (ep->tcp_state == TCP_STATE_CLOSED ||
	    ep->tcp_state == TCP_STATE_LISTEN ||
	    ep->peer_ip == 0 || ep->peer_port == 0) {
		net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
		return (0);
	}

	if (ep->tcp_state == TCP_STATE_SYN_SENT ||
	    ep->tcp_state == TCP_STATE_SYN_RECEIVED) {
		ep->tcp_tx_seq = ep->tcp_snd_nxt;
		net_endpoint_tcp_send(ep, TCP_FLAG_RST | TCP_FLAG_ACK,
		    NULL, 0);
		net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
		return (0);
	}

	if (ep->tcp_state == TCP_STATE_FIN_WAIT_1 ||
	    ep->tcp_state == TCP_STATE_FIN_WAIT_2 ||
	    ep->tcp_state == TCP_STATE_CLOSING ||
	    ep->tcp_state == TCP_STATE_LAST_ACK ||
	    ep->tcp_state == TCP_STATE_TIME_WAIT) {
		return (1);
	}

	if (ep->txseg_count != 0 || ep->tcp_tx_flags != 0) {
		ep->tcp_close_pending = 1;
		return (1);
	}

	ep->tcp_tx_seq = ep->tcp_snd_nxt;
	ep->tcp_tx_len = 0;
	ep->tcp_tx_flags = TCP_FLAG_FIN | TCP_FLAG_ACK;
	ep->tcp_snd_nxt++;
	ep->tcp_retries = 0;
	ep->tcp_last_tx = 0;
	ep->tcp_close_pending = 0;

	if (ep->tcp_state == TCP_STATE_CLOSE_WAIT) {
		net_endpoint_tcp_set_state(ep, TCP_STATE_LAST_ACK);
	} else {
		net_endpoint_tcp_set_state(ep, TCP_STATE_FIN_WAIT_1);
	}
	if (net_endpoint_tcp_send_pending(ep) != 0) {
		net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
		return (0);
	}
	return (1);
}


static void
net_endpoint_tcp_window_update(net_endpoint_t *ep)
{
	u32	win, threshold, mss;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return;
	}
	if (ep->tcp_state != TCP_STATE_ESTABLISHED &&
	    ep->tcp_state != TCP_STATE_FIN_WAIT_1 &&
	    ep->tcp_state != TCP_STATE_FIN_WAIT_2) {
		return;
	}

	win = net_endpoint_tcp_window(ep, 0);
	if (win == 0 || win <= ep->tcp_last_adv_win) {
		return;
	}

	mss = ep->tcp_peer_mss ? ep->tcp_peer_mss : TCP_MSS_DEFAULT;
	threshold = 2 * mss;
	if (threshold > ep->tcp_rx_size / 2) {
		threshold = ep->tcp_rx_size / 2;
	}
	if (threshold == 0) {
		threshold = 1;
	}

	if (ep->tcp_last_adv_win < mss || win - ep->tcp_last_adv_win >=
	    threshold) {
		net_endpoint_tcp_send_ack(ep);
	}
}

int
net_endpoint_tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, u32 seq, u32 ack, u16 flags,
    u16 window, const u8 *opts, u16 opt_len, const u8 *data, u16 len)
{
	net_endpoint_t	*ep;
	net_endpoint_t	*listener;
	u32		consume, seg_end;
	u16		peer_mss;
	u8		peer_wscale;
	int		got;

	ep = net_endpoint_tcp_find(iface, src_ip, dst_ip, src_port,
	    dst_port);
	if (!ep) {
		listener = net_endpoint_tcp_find_listener(iface, dst_ip,
		    dst_port);
		if (listener && (flags & TCP_FLAG_SYN) &&
		    !(flags & TCP_FLAG_RST)) {
			return (net_endpoint_tcp_child(listener, iface,
			    src_ip, dst_ip, src_port, dst_port, seq,
			    window, opts, opt_len) == 0);
		}
		if (!(flags & TCP_FLAG_RST)) {
			consume = len;
			if (flags & TCP_FLAG_SYN) {
				consume++;
			}
			if (flags & TCP_FLAG_FIN) {
				consume++;
			}
			if (flags & TCP_FLAG_ACK) {
				tcp_output(iface, src_ip, dst_port, src_port,
				    ack, 0, TCP_FLAG_RST, 0, NULL, 0);
			} else {
				tcp_output(iface, src_ip, dst_port, src_port,
				    0, seq + consume,
				    TCP_FLAG_RST | TCP_FLAG_ACK, 0, NULL, 0);
			}
		}
		return (0);
	}


	if (flags & TCP_FLAG_RST) {
		if (ep->tcp_state == TCP_STATE_SYN_SENT) {
			if (!(flags & TCP_FLAG_ACK) || ack != ep->tcp_snd_nxt) {
				return (1);
			}
		} else if (seq != ep->tcp_rcv_nxt) {
			return (1);
		}

		ep->tcp_error = API_ERR_IO;
		net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
		net_endpoint_tcp_ooo_reset(ep);
		if (ep->tcp_orphan) {
			net_endpoint_tcp_free(ep);
			return (1);
		}
		proc_wakeup((void *)ep);
		event_notify_net_change(ep);
		return (1);
	}

	ep->tcp_peer_win = window;

	if (ep->tcp_state == TCP_STATE_SYN_SENT) {

		if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) ==
		    (TCP_FLAG_SYN | TCP_FLAG_ACK) &&
		    ack == ep->tcp_snd_nxt) {
			peer_mss = TCP_MSS_DEFAULT;
			(void)tcp_opt_get_mss(opts, opt_len, &peer_mss);
			ep->tcp_peer_mss = peer_mss;
			peer_wscale = 0;
			(void)tcp_opt_get_wscale(opts, opt_len, &peer_wscale);
			if (peer_wscale > NET_ENDPOINT_TCP_WSCALE_MAX) {
				peer_wscale = NET_ENDPOINT_TCP_WSCALE_MAX;
			}
			ep->tcp_rcv_wscale = peer_wscale;
			ep->tcp_irs = seq;
			ep->tcp_rcv_nxt = seq + 1;
			ep->tcp_snd_una = ack;
			net_endpoint_tcp_set_state(ep, TCP_STATE_ESTABLISHED);
			ep->tcp_tx_flags = 0;
			ep->tcp_retries = 0;
			ep->tcp_cwnd = 10u * peer_mss;
			if (ep->tcp_cwnd > NET_ENDPOINT_TCP_SNDBUF) {
				ep->tcp_cwnd = NET_ENDPOINT_TCP_SNDBUF;
			}
			net_endpoint_tcp_send_ack(ep);
			proc_wakeup((void *)ep);
			event_notify_net_change(ep);
			return (1);
		}
		return (1);
	}

	if (ep->tcp_state == TCP_STATE_SYN_RECEIVED) {
		if ((flags & TCP_FLAG_ACK) &&
		    net_endpoint_seq_after_eq(ack, ep->tcp_snd_nxt)) {
			ep->tcp_snd_una = ack;
			net_endpoint_tcp_set_state(ep, TCP_STATE_ESTABLISHED);
			ep->tcp_tx_flags = 0;
			ep->tcp_retries = 0;
			if (net_endpoint_tcp_queue_accept(ep) != 0) {
				ep->tcp_tx_seq = ep->tcp_snd_nxt;
				net_endpoint_tcp_send(ep, TCP_FLAG_RST |
				    TCP_FLAG_ACK, NULL, 0);
				net_endpoint_tcp_drop(ep);
			} else {
				proc_wakeup((void *)ep);
				event_notify_net_change(ep);
			}
		} else if (flags & TCP_FLAG_SYN) {
			(void)net_endpoint_tcp_flush(ep);
		}
		return (1);
	}

	if (flags & TCP_FLAG_ACK) {

		if (len == 0 && !(flags & (TCP_FLAG_SYN | TCP_FLAG_FIN)) &&
		    ack == ep->tcp_snd_una && ep->txseg_count != 0 &&
		    net_endpoint_seq_after(ep->tcp_snd_nxt, ep->tcp_snd_una)) {
			ep->tcp_dupacks++;
			if (ep->tcp_dupacks ==
			    NET_ENDPOINT_TCP_DUPACK_THRESH) {
				net_endpoint_tcp_cwnd_loss(ep);
				ep->tcp_txseg[ep->txseg_head].in_flight = 0;
				ep->tcp_tx_seq =
				    ep->tcp_txseg[ep->txseg_head].seq;
				(void)net_endpoint_tcp_flush(ep);
			}
		} else if (ack != ep->tcp_snd_una) {
			ep->tcp_dupacks = 0;
		}

		net_endpoint_tcp_ack_tx(ep, ack);
		if (ep->tcp_orphan && ep->tcp_state == TCP_STATE_CLOSED) {
			net_endpoint_tcp_free(ep);
			return (1);
		}
	}

	if (ep->tcp_state != TCP_STATE_ESTABLISHED &&
	    ep->tcp_state != TCP_STATE_CLOSE_WAIT &&
	    ep->tcp_state != TCP_STATE_FIN_WAIT_1 &&
	    ep->tcp_state != TCP_STATE_FIN_WAIT_2 &&
	    ep->tcp_state != TCP_STATE_CLOSING &&
	    ep->tcp_state != TCP_STATE_TIME_WAIT) {
		return (1);
	}


	if (len != 0) {
		if (seq == ep->tcp_rcv_nxt) {
			if (ep->tcp_orphan) {
				/* No reader left; account for it and drop. */
				ep->tcp_rcv_nxt += len;
			} else {
				got = net_endpoint_tcp_rx_push(ep, data, len);
				if (got > 0) {
					ep->tcp_rcv_nxt += got;

					(void)net_endpoint_tcp_ooo_drain(ep);
				}
			}
			net_endpoint_tcp_send_ack(ep);
		} else if (net_endpoint_seq_after(seq, ep->tcp_rcv_nxt) &&
		    !ep->tcp_orphan) {
			(void)net_endpoint_tcp_ooo_store(ep, seq, data, len);

			net_endpoint_tcp_send_ack(ep);
		} else if (net_endpoint_seq_after(ep->tcp_rcv_nxt, seq)) {
			net_endpoint_tcp_send_ack(ep);
		}
	}


	seg_end = seq + len;
	if ((flags & TCP_FLAG_FIN) && seg_end == ep->tcp_rcv_nxt) {
		ep->tcp_rcv_nxt++;
		net_endpoint_tcp_ooo_reset(ep);
		if (ep->tcp_state == TCP_STATE_ESTABLISHED) {
			net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSE_WAIT);
		} else if (ep->tcp_state == TCP_STATE_FIN_WAIT_1) {
			net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSING);
		} else if (ep->tcp_state == TCP_STATE_FIN_WAIT_2) {
			net_endpoint_tcp_set_state(ep, TCP_STATE_TIME_WAIT);
		} else if (ep->tcp_state == TCP_STATE_TIME_WAIT) {
			net_endpoint_tcp_set_state(ep, TCP_STATE_TIME_WAIT);
		}
		net_endpoint_tcp_send_ack(ep);
		proc_wakeup((void *)ep);
		event_notify_net_change(ep);
	}

	return (1);
}

void
net_endpoint_tcp_tick(void)
{
	net_endpoint_t			*ep;
	net_endpoint_tcp_txseg_t	*seg;
	u64				now_ms;
	u64				ticks_now;
	u32				rto_ms;
	int				i;

	if (!timer_is_initialized()) {
		return;
	}
	now_ms = net_endpoint_tcp_now_ms();
	ticks_now = timer_get_ticks();

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_TCP) {
			continue;
		}

		if (ep->tcp_deadline != 0 && ticks_now >= ep->tcp_deadline) {
			net_endpoint_tcp_free(ep);
			continue;
		}


		if (ep->txseg_count == 0 && ep->tcp_tx_flags == 0) {
			continue;
		}

		rto_ms = ep->tcp_rto_ms;
		if (rto_ms == 0) {
			rto_ms = NET_ENDPOINT_TCP_RTO_INIT_MS;
		}

		if (ep->tcp_last_rto_arm != 0 &&
		    now_ms - ep->tcp_last_rto_arm < rto_ms) {
			continue;
		}

		if (ep->tcp_retries >= NET_ENDPOINT_TCP_MAX_RETRIES) {
			ep->tcp_error = API_ERR_IO;
			net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
			proc_wakeup((void *)ep);
			event_notify_net_change(ep);
			if (ep->tcp_parent >= 0 || ep->tcp_orphan) {
				net_endpoint_tcp_drop(ep);
			}
			continue;
		}

		

		net_endpoint_tcp_cwnd_loss(ep);
		ep->tcp_rto_ms = rto_ms * 2;
		if (ep->tcp_rto_ms > NET_ENDPOINT_TCP_RTO_MAX_MS) {
			ep->tcp_rto_ms = NET_ENDPOINT_TCP_RTO_MAX_MS;
		}
		ep->tcp_last_rto_arm = now_ms;
		ep->tcp_retries++;
		ep->tcp_rtseq_pending = 0;

		if (ep->tcp_tx_flags != 0) {
			(void)net_endpoint_tcp_send_pending(ep);
		} else if (ep->txseg_count != 0) {
			seg = &ep->tcp_txseg[ep->txseg_head];
			seg->in_flight = 0;
			ep->tcp_tx_seq = seg->seq;
			(void)net_endpoint_tcp_flush(ep);
		}
	}
}
