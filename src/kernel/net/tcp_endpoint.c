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
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_seq_after as function with args u32, u32
$define %func net_endpoint_seq_after_eq as function with args u32, u32
$define %func net_endpoint_tcp_ticks as function with args void
$define %func net_endpoint_tcp_set_state as procedure with args net_endpoint_t *, int
$define %func net_endpoint_tcp_can_send as function with args net_endpoint_t *
$define %func net_endpoint_tcp_window as function with args net_endpoint_t *
$define %func net_endpoint_tcp_send as function with args net_endpoint_t *, u16, const u8 *, u16
$define %func net_endpoint_tcp_send_ack as function with args net_endpoint_t *
$define %func net_endpoint_tcp_send_pending as function with args net_endpoint_t *
$define %func net_endpoint_tcp_ack_tx as procedure with args net_endpoint_t *, u32
$define %func net_endpoint_tcp_rx_push as function with args net_endpoint_t *, const u8 *, u16
$define %func net_endpoint_tcp_rx_pop as function with args net_endpoint_t *, u8 *, u32
$define %func net_endpoint_tcp_find as function with args net_iface_t *, u32, u32, u16, u16
$define %func net_endpoint_tcp_find_listener as function with args net_iface_t *, u32, u16
$define %func net_endpoint_tcp_child as function with args net_endpoint_t *, net_iface_t *, u32, u32, u16, u16, u32, u16
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
$define %func net_endpoint_tcp_input as function with args net_iface_t *, u32, u32, u16, u16, u32, u32, u16, u16, const u8 *, u16
$define %func net_endpoint_tcp_tick as procedure with args void

*/

/* !SPACE!

$space %internal net_endpoint_seq_after, net_endpoint_seq_after_eq
$space %internal net_endpoint_tcp_ticks, net_endpoint_tcp_set_state
$space %internal net_endpoint_tcp_can_send, net_endpoint_tcp_window
$space %internal net_endpoint_tcp_send, net_endpoint_tcp_send_ack
$space %internal net_endpoint_tcp_send_pending, net_endpoint_tcp_ack_tx
$space %internal net_endpoint_tcp_rx_push, net_endpoint_tcp_rx_pop
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

*/

#include <kernel/api/errno.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <kernel/net/endpoint_internal.h>
#include <kernel/net/tcp.h>
#include <kernel/net/tcp_endpoint.h>
#include <mlibc/mlibc.h>

static u32	g_tcp_next_isn = 0x10203040;

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

static u16
net_endpoint_tcp_window(net_endpoint_t *ep)
{
	u32	space;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return (0);
	}
	space = NET_ENDPOINT_TCP_RX_SIZE - ep->tcp_rx_count;
	if (space > 65535) {
		space = 65535;
	}
	return ((u16)space);
}

static int
net_endpoint_tcp_send(net_endpoint_t *ep, u16 flags,
    const u8 *data, u16 len)
{
	net_iface_t	*iface;
	int		ifindex;

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

	return (tcp_output(iface, ep->peer_ip, ep->local_port,
	    ep->peer_port, ep->tcp_tx_seq, ep->tcp_rcv_nxt, flags,
	    net_endpoint_tcp_window(ep), data, len));
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
		return (0);
	}
	return (-1);
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

	if (ep->tcp_tx_len != 0 || ep->tcp_tx_flags != 0) {
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
net_endpoint_tcp_ack_tx(net_endpoint_t *ep, u32 ack)
{
	u32	end;
	u16	flags;

	if (!ep || ep->proto != NET_ENDPOINT_PROTO_TCP) {
		return;
	}
	if (net_endpoint_seq_after(ack, ep->tcp_snd_nxt)) {
		return;
	}
	if (net_endpoint_seq_after(ack, ep->tcp_snd_una)) {
		ep->tcp_snd_una = ack;
	}

	if (ep->tcp_tx_len != 0) {
		end = ep->tcp_tx_seq + ep->tcp_tx_len;
		if (net_endpoint_seq_after_eq(ack, end)) {
			ep->tcp_tx_len = 0;
			ep->tcp_tx_flags = 0;
			ep->tcp_retries = 0;
			if (ep->tcp_close_pending) {
				net_endpoint_tcp_begin_close(ep);
				return;
			}
			proc_wakeup((void *)ep);
			event_notify_net_change(ep);
		}
	} else if (ep->tcp_tx_flags != 0 &&
	    net_endpoint_seq_after_eq(ack, ep->tcp_snd_nxt)) {
		flags = ep->tcp_tx_flags;
		ep->tcp_tx_flags = 0;
		ep->tcp_retries = 0;
		if (flags & TCP_FLAG_FIN) {
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
		if (ep->tcp_close_pending) {
			net_endpoint_tcp_begin_close(ep);
			return;
		}
		proc_wakeup((void *)ep);
		event_notify_net_change(ep);
	}
}

static int
net_endpoint_tcp_rx_push(net_endpoint_t *ep, const u8 *data, u16 len)
{
	u32	space, first;

	if (!ep || !data || len == 0) {
		return (0);
	}
	space = NET_ENDPOINT_TCP_RX_SIZE - ep->tcp_rx_count;
	if (len > space) {
		return (0);
	}

	first = NET_ENDPOINT_TCP_RX_SIZE - ep->tcp_rx_tail;
	if (first > len) {
		first = len;
	}
	memcpy(ep->tcp_rx + ep->tcp_rx_tail, data, first);
	if (len > first) {
		memcpy(ep->tcp_rx, data + first, len - first);
	}
	ep->tcp_rx_tail = (ep->tcp_rx_tail + len) %
	    NET_ENDPOINT_TCP_RX_SIZE;
	ep->tcp_rx_count += len;
	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
	return (1);
}

static int
net_endpoint_tcp_rx_pop(net_endpoint_t *ep, u8 *buf, u32 len)
{
	u32	to_copy, first;

	if (!ep || !buf || len == 0 || ep->tcp_rx_count == 0) {
		return (0);
	}
	to_copy = ep->tcp_rx_count;
	if (to_copy > len) {
		to_copy = len;
	}

	first = NET_ENDPOINT_TCP_RX_SIZE - ep->tcp_rx_head;
	if (first > to_copy) {
		first = to_copy;
	}
	memcpy(buf, ep->tcp_rx + ep->tcp_rx_head, first);
	if (to_copy > first) {
		memcpy(buf + first, ep->tcp_rx, to_copy - first);
	}
	ep->tcp_rx_head = (ep->tcp_rx_head + to_copy) %
	    NET_ENDPOINT_TCP_RX_SIZE;
	ep->tcp_rx_count -= to_copy;
	event_notify_net_change(ep);
	return ((int)to_copy);
}

static net_endpoint_t *
net_endpoint_tcp_find(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port)
{
	net_endpoint_t	*ep;
	int		i;

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_TCP ||
		    ep->mode != NET_ENDPOINT_MODE_STREAM ||
		    ep->tcp_state == TCP_STATE_LISTEN) {
			continue;
		}
		if (ep->local_port != dst_port ||
		    ep->peer_port != src_port ||
		    ep->peer_ip != src_ip) {
			continue;
		}
		if (ep->local_ip != 0 && ep->local_ip != dst_ip) {
			continue;
		}
		if (ep->ifindex != NET_ENDPOINT_IF_AUTO &&
		    (!iface || ep->ifindex != iface->index)) {
			continue;
		}
		return (ep);
	}
	return (NULL);
}

static net_endpoint_t *
net_endpoint_tcp_find_listener(net_iface_t *iface, u32 dst_ip, u16 dst_port)
{
	net_endpoint_t	*ep;
	int		i;

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_TCP ||
		    ep->mode != NET_ENDPOINT_MODE_STREAM ||
		    ep->tcp_state != TCP_STATE_LISTEN) {
			continue;
		}
		if (ep->local_port != dst_port) {
			continue;
		}
		if (ep->local_ip != 0 && ep->local_ip != dst_ip) {
			continue;
		}
		if (ep->ifindex != NET_ENDPOINT_IF_AUTO &&
		    (!iface || ep->ifindex != iface->index)) {
			continue;
		}
		return (ep);
	}
	return (NULL);
}

static int
net_endpoint_tcp_child(net_endpoint_t *listener, net_iface_t *iface,
    u32 src_ip, u32 dst_ip, u16 src_port, u16 dst_port, u32 seq,
    u16 window)
{
	net_endpoint_t	*child;
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
	child->tcp_irs = seq;
	child->tcp_rcv_nxt = seq + 1;
	child->tcp_iss = g_tcp_next_isn;
	g_tcp_next_isn += 0x10101;
	child->tcp_snd_una = child->tcp_iss;
	child->tcp_snd_nxt = child->tcp_iss + 1;
	child->tcp_peer_win = window;
	child->tcp_tx_seq = child->tcp_iss;
	child->tcp_tx_flags = TCP_FLAG_SYN | TCP_FLAG_ACK;

	if (net_endpoint_tcp_send_pending(child) != 0) {
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
	g_tcp_next_isn = 0x10203040;
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

	ep->peer_ip = addr->ip;
	ep->peer_port = addr->port;
	ep->peer_ifindex = iface->index;
	ep->tcp_state = TCP_STATE_SYN_SENT;
	ep->tcp_error = 0;
	ep->tcp_iss = g_tcp_next_isn;
	g_tcp_next_isn += 0x10101;
	ep->tcp_snd_una = ep->tcp_iss;
	ep->tcp_snd_nxt = ep->tcp_iss + 1;
	ep->tcp_tx_seq = ep->tcp_iss;
	ep->tcp_tx_flags = TCP_FLAG_SYN;
	ep->tcp_retries = 0;

	if (net_endpoint_tcp_send_pending(ep) != 0) {
		ep->tcp_state = TCP_STATE_CLOSED;
		ep->peer_ip = 0;
		ep->peer_port = 0;
		ep->tcp_tx_flags = 0;
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
	u32	to_send;
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
	while (ep->tcp_tx_len != 0 || ep->tcp_tx_flags != 0) {
		if ((ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) ||
		    (flags & NET_ENDPOINT_MSG_NONBLOCK)) {
			return (-API_ERR_RETRY);
		}
		proc_sleep((void *)ep);
		if (!ep->used) {
			return (-API_ERR_BAD_HANDLE);
		}
		if (!net_endpoint_tcp_can_send(ep)) {
			return (-API_ERR_PIPE_CLOSED);
		}
	}

	to_send = len;
	if (to_send > NET_ENDPOINT_TCP_TX_SIZE) {
		to_send = NET_ENDPOINT_TCP_TX_SIZE;
	}
	memcpy(ep->tcp_tx, data, to_send);
	ep->tcp_tx_seq = ep->tcp_snd_nxt;
	ep->tcp_tx_len = to_send;
	ep->tcp_tx_flags = TCP_FLAG_ACK | TCP_FLAG_PSH;
	ep->tcp_snd_nxt += to_send;
	ret = net_endpoint_tcp_send_pending(ep);
	if (ret != 0) {
		ep->tcp_snd_nxt -= to_send;
		ep->tcp_tx_len = 0;
		ep->tcp_tx_flags = 0;
		return (-API_ERR_IO);
	}
	return ((int)to_send);
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
	    ep->tcp_tx_len == 0 && ep->tcp_tx_flags == 0);
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
	if (net_endpoint_tcp_writable(ep)) {
		return (NET_ENDPOINT_TCP_TX_SIZE);
	}
	return (0);
}

int
net_endpoint_tcp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, u32 seq, u32 ack, u16 flags,
    u16 window, const u8 *data, u16 len)
{
	net_endpoint_t	*ep;
	net_endpoint_t	*listener;
	u32		consume, end_seq;

	ep = net_endpoint_tcp_find(iface, src_ip, dst_ip, src_port,
	    dst_port);
	if (!ep) {
		listener = net_endpoint_tcp_find_listener(iface, dst_ip,
		    dst_port);
		if (listener && (flags & TCP_FLAG_SYN) &&
		    !(flags & TCP_FLAG_RST)) {
			return (net_endpoint_tcp_child(listener, iface,
			    src_ip, dst_ip, src_port, dst_port, seq,
			    window) == 0);
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
		ep->tcp_error = API_ERR_IO;
		net_endpoint_tcp_set_state(ep, TCP_STATE_CLOSED);
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
			ep->tcp_irs = seq;
			ep->tcp_rcv_nxt = seq + 1;
			ep->tcp_snd_una = ack;
			net_endpoint_tcp_set_state(ep, TCP_STATE_ESTABLISHED);
			ep->tcp_tx_flags = 0;
			ep->tcp_retries = 0;
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
				net_endpoint_tcp_send(ep, TCP_FLAG_RST |
				    TCP_FLAG_ACK, NULL, 0);
				net_endpoint_tcp_drop(ep);
			} else {
				proc_wakeup((void *)ep);
				event_notify_net_change(ep);
			}
		} else if (flags & TCP_FLAG_SYN) {
			net_endpoint_tcp_send_pending(ep);
		}
		return (1);
	}

	if (flags & TCP_FLAG_ACK) {
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

	end_seq = seq;
	if (len != 0 && seq == ep->tcp_rcv_nxt) {
		if (ep->tcp_orphan) {
			ep->tcp_rcv_nxt += len;
			end_seq = ep->tcp_rcv_nxt;
		} else if (net_endpoint_tcp_rx_push(ep, data, len)) {
			ep->tcp_rcv_nxt += len;
			end_seq = ep->tcp_rcv_nxt;
		}
	}
	if (len != 0) {
		net_endpoint_tcp_send_ack(ep);
	}

	if ((flags & TCP_FLAG_FIN) && end_seq == ep->tcp_rcv_nxt) {
		ep->tcp_rcv_nxt++;
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
	net_endpoint_t	*ep;
	u64		now, rto;
	u32		freq;
	int		i;

	if (!timer_is_initialized()) {
		return;
	}
	freq = timer_get_frequency();
	rto = freq == 0 ? 100 : freq;
	if (rto == 0) {
		rto = 1;
	}
	now = timer_get_ticks();

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_TCP) {
			continue;
		}
		if (ep->tcp_deadline != 0 && now >= ep->tcp_deadline) {
			net_endpoint_tcp_free(ep);
			continue;
		}
		if (ep->tcp_tx_flags == 0 && ep->tcp_tx_len == 0) {
			continue;
		}
		if (ep->tcp_last_tx != 0 && now - ep->tcp_last_tx < rto) {
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
		if (net_endpoint_tcp_send_pending(ep) == 0) {
			ep->tcp_retries++;
		}
	}
}
