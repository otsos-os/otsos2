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
$define %type net_endpoint_t as native network endpoint state
$define %type net_endpoint_rx_t as queued UDP datagram
$define %type net_endpoint_addr_t as endpoint IPv4 address tuple
$define %type net_iface_t as struct with logical network interface state

$define %func net_endpoint_iface_up as function with args net_iface_t *
$define %func net_endpoint_ip_local as function with args u32
$define %func net_endpoint_find_iface_by_ip as function with args u32
$define %func net_endpoint_find_loopback as function with args void
$define %func net_endpoint_find_broadcast_iface as function with args void
$define %func net_endpoint_route as function with args u32, u32, int
$define %func net_endpoint_bind_conflict as function with args net_endpoint_t *, u32, u16
$define %func net_endpoint_alloc_port as function with args net_endpoint_t *, u32
$define %func net_endpoint_valid_addr as function with args const net_endpoint_addr_t *, int
$define %func net_endpoint_match as function with args net_endpoint_t *, net_iface_t *, u32, u32, u16, u16
$define %func net_endpoint_enqueue as function with args net_endpoint_t *, net_iface_t *, u32, u32, u16, u16, const u8 *, u16
$define %func net_endpoint_free as procedure with args net_endpoint_t *
$define %func net_endpoint_init as procedure with args void
$define %func net_endpoint_open as function with args int, int, u32
$define %func net_endpoint_close as procedure with args net_endpoint_t *
$define %func net_endpoint_set_nonblock as procedure with args endpoint, int
$define %func net_endpoint_bind as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_connect as function with args net_endpoint_t *, const net_endpoint_addr_t *
$define %func net_endpoint_listen as function with args net_endpoint_t *, int
$define %func net_endpoint_accept as function with args net_endpoint_t *, net_endpoint_t **, net_endpoint_addr_t *, u32
$define %func net_endpoint_send as function with args net_endpoint_t *, const u8 *, u32, const net_endpoint_addr_t *, u32
$define %func net_endpoint_recv as function with args net_endpoint_t *, u8 *, u32, net_endpoint_addr_t *, u32, u32 *
$define %func net_endpoint_get_local as procedure with args net_endpoint_t *, net_endpoint_addr_t *
$define %func net_endpoint_get_peer as function with args net_endpoint_t *, net_endpoint_addr_t *
$define %func net_endpoint_get_state as function with args net_endpoint_t *, u32 *, u32 *
$define %func net_endpoint_readable as function with args net_endpoint_t *
$define %func net_endpoint_writable as function with args net_endpoint_t *
$define %func net_endpoint_pending_bytes as function with args net_endpoint_t *
$define %func net_endpoint_write_space as function with args net_endpoint_t *
$define %func net_endpoint_udp_input as function with args net_iface_t *, u32, u32, u16, u16, const u8 *, u16
$define %func net_endpoint_tick as procedure with args void

*/

/* !SPACE!

$space %internal net_endpoint_iface_up, net_endpoint_ip_local
$space %internal net_endpoint_find_iface_by_ip
$space %internal net_endpoint_find_loopback
$space %internal net_endpoint_find_broadcast_iface, net_endpoint_route
$space %internal net_endpoint_bind_conflict, net_endpoint_alloc_port
$space %internal net_endpoint_valid_addr, net_endpoint_match
$space %internal net_endpoint_enqueue
$space %export net_endpoint_free
$space %export net_endpoint_init, net_endpoint_open, net_endpoint_close
$space %export net_endpoint_set_nonblock
$space %export net_endpoint_bind, net_endpoint_connect
$space %export net_endpoint_listen, net_endpoint_accept
$space %export net_endpoint_send, net_endpoint_recv
$space %export net_endpoint_get_local, net_endpoint_get_peer
$space %export net_endpoint_get_state
$space %export net_endpoint_readable, net_endpoint_writable
$space %export net_endpoint_pending_bytes, net_endpoint_write_space
$space %export net_endpoint_udp_input
$space %export net_endpoint_tick

*/

#include <kernel/api/errno.h>
#include <kernel/event/event.h>
#include <kernel/net/endpoint.h>
#include <kernel/net/endpoint_internal.h>
#include <kernel/net/ethernet.h>
#include <kernel/net/ipv4.h>
#include <kernel/net/tcp_endpoint.h>
#include <kernel/net/udp.h>
#include <mlibc/mlibc.h>

net_endpoint_t		g_endpoints[NET_ENDPOINT_MAX];
static u16		g_next_ephemeral;

static int
net_endpoint_iface_up(net_iface_t *iface)
{
	if (!iface || !iface->ndev) {
		return (0);
	}
	if (!(iface->flags & NET_IFF_UP)) {
		return (0);
	}
	if (!(iface->ndev->flags & NETDEV_F_UP)) {
		return (0);
	}
	return (1);
}

static int
net_endpoint_ip_local(u32 ip)
{
	net_iface_t	*iface;
	int		i, count;

	if (ip == 0) {
		return (1);
	}

	count = net_iface_count();
	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    iface->ip_addr == ip) {
			return (1);
		}
	}
	return (0);
}

static net_iface_t *
net_endpoint_find_iface_by_ip(u32 ip)
{
	net_iface_t	*iface;
	int		i, count;

	count = net_iface_count();
	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    iface->ip_addr == ip) {
			return (iface);
		}
	}
	return (NULL);
}

static net_iface_t *
net_endpoint_find_loopback(void)
{
	net_iface_t	*iface;
	int		i, count;

	count = net_iface_count();
	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    (iface->flags & NET_IFF_LOOPBACK)) {
			return (iface);
		}
	}
	return (NULL);
}

static net_iface_t *
net_endpoint_find_broadcast_iface(void)
{
	net_iface_t	*iface;
	int		i, count;

	count = net_iface_count();
	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    !(iface->flags & NET_IFF_LOOPBACK)) {
			return (iface);
		}
	}
	return (NULL);
}

net_iface_t *
net_endpoint_route(u32 dst_ip, u32 local_ip, int ifindex)
{
	net_iface_t	*iface;
	int		i, count;

	if (ifindex != NET_ENDPOINT_IF_AUTO) {
		iface = net_iface_get(ifindex);
		if (!net_endpoint_iface_up(iface)) {
			return (NULL);
		}
		if (local_ip != 0 && iface->ip_addr != local_ip) {
			return (NULL);
		}
		return (iface);
	}

	if (local_ip != 0) {
		return (net_endpoint_find_iface_by_ip(local_ip));
	}

	if ((dst_ip & 0xFF000000u) == 0x7F000000u) {
		return (net_endpoint_find_loopback());
	}
	if (dst_ip == 0xFFFFFFFF) {
		return (net_endpoint_find_broadcast_iface());
	}

	count = net_iface_count();
	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (!net_endpoint_iface_up(iface) ||
		    (iface->flags & NET_IFF_LOOPBACK)) {
			continue;
		}
		if (iface->ip_addr != 0 && iface->netmask != 0 &&
		    (dst_ip & iface->netmask) ==
		    (iface->ip_addr & iface->netmask)) {
			return (iface);
		}
	}

	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    !(iface->flags & NET_IFF_LOOPBACK) &&
		    iface->ip_addr != 0 && iface->gw_addr != 0) {
			return (iface);
		}
	}

	for (i = 0; i < count; i++) {
		iface = net_iface_get(i);
		if (net_endpoint_iface_up(iface) &&
		    !(iface->flags & NET_IFF_LOOPBACK) &&
		    iface->ip_addr != 0) {
			return (iface);
		}
	}

	return (NULL);
}

int
net_endpoint_bind_conflict(net_endpoint_t *self, u32 ip, u16 port)
{
	net_endpoint_t	*ep;
	int		i;

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!ep->used || ep == self) {
			continue;
		}
		if (ep->proto != self->proto) {
			continue;
		}
		if (ep->local_port != port) {
			continue;
		}
		if (ep->local_ip == 0 || ip == 0 ||
		    ep->local_ip == ip) {
			return (1);
		}
	}
	return (0);
}

int
net_endpoint_alloc_port(net_endpoint_t *self, u32 ip)
{
	u16	port;
	int	tries;

	for (tries = 0; tries <= NET_ENDPOINT_EPHEMERAL_LAST -
	    NET_ENDPOINT_EPHEMERAL_FIRST; tries++) {
		port = g_next_ephemeral;
		g_next_ephemeral++;
		if (g_next_ephemeral > NET_ENDPOINT_EPHEMERAL_LAST) {
			g_next_ephemeral = NET_ENDPOINT_EPHEMERAL_FIRST;
		}
		if (!net_endpoint_bind_conflict(self, ip, port)) {
			return ((int)port);
		}
	}
	return (-API_ERR_BUSY);
}

static int
net_endpoint_valid_addr(const net_endpoint_addr_t *addr, int peer)
{
	if (!addr) {
		return (-API_ERR_BAD_ADDR);
	}
	if (addr->family != NET_ENDPOINT_ADDR_IP4) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (addr->ifindex != NET_ENDPOINT_IF_AUTO &&
	    !net_iface_get(addr->ifindex)) {
		return (-API_ERR_NO_DEVICE);
	}
	if (peer && (addr->ip == 0 || addr->port == 0)) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!peer && !net_endpoint_ip_local(addr->ip)) {
		return (-API_ERR_NO_DEVICE_ADDR);
	}
	return (0);
}

static int
net_endpoint_match(net_endpoint_t *ep, net_iface_t *iface, u32 src_ip,
    u32 dst_ip, u16 src_port, u16 dst_port)
{
	if (!ep->used || ep->proto != NET_ENDPOINT_PROTO_UDP ||
	    ep->mode != NET_ENDPOINT_MODE_DGRAM) {
		return (0);
	}
	if (ep->local_port == 0 || ep->local_port != dst_port) {
		return (0);
	}
	if (ep->local_ip != 0 && ep->local_ip != dst_ip) {
		return (0);
	}
	if (ep->ifindex != NET_ENDPOINT_IF_AUTO &&
	    (!iface || ep->ifindex != iface->index)) {
		return (0);
	}
	if (ep->peer_ip != 0 && ep->peer_ip != src_ip) {
		return (0);
	}
	if (ep->peer_port != 0 && ep->peer_port != src_port) {
		return (0);
	}
	return (1);
}

static int
net_endpoint_enqueue(net_endpoint_t *ep, net_iface_t *iface, u32 src_ip,
    u32 dst_ip, u16 src_port, u16 dst_port, const u8 *data, u16 len)
{
	net_endpoint_rx_t	*rx;

	if (ep->rx_count >= NET_ENDPOINT_RX_QUEUE) {
		ep->rx_drops++;
		return (0);
	}
	if (len > NET_ENDPOINT_DGRAM_MAX) {
		ep->rx_drops++;
		return (0);
	}

	rx = &ep->rx[ep->rx_tail];
	memset(rx, 0, sizeof(*rx));
	if (len != 0) {
		memcpy(rx->data, data, len);
	}
	rx->src_ip = src_ip;
	rx->dst_ip = dst_ip;
	rx->src_port = src_port;
	rx->dst_port = dst_port;
	rx->len = len;
	rx->ifindex = iface ? iface->index : NET_ENDPOINT_IF_AUTO;

	ep->rx_tail = (ep->rx_tail + 1) % NET_ENDPOINT_RX_QUEUE;
	ep->rx_count++;
	ep->rx_bytes += len;

	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
	return (0);
}

void
net_endpoint_free(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return;
	}
	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
	/*
	 * Heap-backed TCP state has to go back before the slot is wiped,
	 * otherwise the pointers are gone and the heap leaks per connection.
	 */
	net_endpoint_tcp_release(ep);
	memset(ep, 0, sizeof(*ep));
}

void
net_endpoint_init(void)
{
	memset(g_endpoints, 0, sizeof(g_endpoints));
	g_next_ephemeral = NET_ENDPOINT_EPHEMERAL_FIRST;
	net_endpoint_tcp_init();
}

net_endpoint_t *
net_endpoint_open(int proto, int mode, u32 flags)
{
	net_endpoint_t	*ep;
	int		i;

	if (!((proto == NET_ENDPOINT_PROTO_UDP &&
	    mode == NET_ENDPOINT_MODE_DGRAM) ||
	    (proto == NET_ENDPOINT_PROTO_TCP &&
	    mode == NET_ENDPOINT_MODE_STREAM))) {
		return (NULL);
	}
	if (flags & ~NET_ENDPOINT_FLAG_NONBLOCK) {
		return (NULL);
	}

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (ep->used) {
			continue;
		}
		memset(ep, 0, sizeof(*ep));
		ep->used = 1;
		ep->proto = proto;
		ep->mode = mode;
		ep->flags = flags;
		ep->ifindex = NET_ENDPOINT_IF_AUTO;
		ep->peer_ifindex = NET_ENDPOINT_IF_AUTO;
		ep->tcp_parent = -1;
		return (ep);
	}
	return (NULL);
}

void
net_endpoint_close(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return;
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		if (ep->tcp_state == TCP_STATE_LISTEN) {
			net_endpoint_tcp_drop_children(ep);
			net_endpoint_free(ep);
			return;
		}
		if (net_endpoint_tcp_begin_close(ep)) {
			proc_wakeup((void *)ep);
			event_notify_net_change(ep);
			return;
		}
	}
	net_endpoint_free(ep);
}

void
net_endpoint_set_nonblock(net_endpoint_t *ep, int nonblock)
{
	if (!ep || !ep->used) {
		return;
	}
	if (nonblock) {
		ep->flags |= NET_ENDPOINT_FLAG_NONBLOCK;
	} else {
		ep->flags &= ~NET_ENDPOINT_FLAG_NONBLOCK;
	}
	proc_wakeup((void *)ep);
	event_notify_net_change(ep);
}

int
net_endpoint_bind(net_endpoint_t *ep, const net_endpoint_addr_t *addr)
{
	int	ret, port;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP &&
	    ep->tcp_state != TCP_STATE_CLOSED) {
		return (-API_ERR_BUSY);
	}
	ret = net_endpoint_valid_addr(addr, 0);
	if (ret != 0) {
		return (ret);
	}
	if (ep->local_port != 0) {
		return (-API_ERR_BUSY);
	}

	port = (int)addr->port;
	if (port == 0) {
		port = net_endpoint_alloc_port(ep, addr->ip);
		if (port < 0) {
			return (port);
		}
	}
	if (net_endpoint_bind_conflict(ep, addr->ip, (u16)port)) {
		return (-API_ERR_BUSY);
	}

	ep->local_ip = addr->ip;
	ep->local_port = (u16)port;
	ep->ifindex = addr->ifindex;
	return (0);
}

int
net_endpoint_connect(net_endpoint_t *ep, const net_endpoint_addr_t *addr)
{
	int	ret, port;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	ret = net_endpoint_valid_addr(addr, 1);
	if (ret != 0) {
		return (ret);
	}

	if (ep->proto == NET_ENDPOINT_PROTO_UDP) {
		if (ep->local_port == 0) {
			port = net_endpoint_alloc_port(ep, ep->local_ip);
			if (port < 0) {
				return (port);
			}
			ep->local_port = (u16)port;
		}

		ep->peer_ip = addr->ip;
		ep->peer_port = addr->port;
		ep->peer_ifindex = addr->ifindex;
		return (0);
	}

	if (ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->mode != NET_ENDPOINT_MODE_STREAM) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (net_endpoint_tcp_connect(ep, addr));
}

int
net_endpoint_listen(net_endpoint_t *ep, int backlog)
{
	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->proto != NET_ENDPOINT_PROTO_TCP ||
	    ep->mode != NET_ENDPOINT_MODE_STREAM) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	return (net_endpoint_tcp_listen(ep, backlog));
}

int
net_endpoint_accept(net_endpoint_t *ep, net_endpoint_t **out_ep,
    net_endpoint_addr_t *addr, u32 flags)
{
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
	return (net_endpoint_tcp_accept(ep, out_ep, addr, flags));
}

int
net_endpoint_send(net_endpoint_t *ep, const u8 *data, u32 len,
    const net_endpoint_addr_t *addr, u32 flags)
{
	net_endpoint_addr_t	dst;
	net_iface_t		*iface;
	const u8		*payload;
	u8			empty;
	u32			src_ip;
	int			ifindex;
	int			ret, port;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (flags & ~NET_ENDPOINT_MSG_NONBLOCK) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!data && len != 0) {
		return (-API_ERR_BAD_ADDR);
	}

	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		if (addr) {
			return (-API_ERR_BAD_VALUE);
		}
		return (net_endpoint_tcp_send_user(ep, data, len, flags));
	}

	if (len > NET_ENDPOINT_DGRAM_MAX) {
		return (-API_ERR_TOO_BIG);
	}

	if (addr) {
		ret = net_endpoint_valid_addr(addr, 1);
		if (ret != 0) {
			return (ret);
		}
		dst = *addr;
	} else {
		if (ep->peer_ip == 0 || ep->peer_port == 0) {
			return (-API_ERR_BAD_VALUE);
		}
		dst.ip = ep->peer_ip;
		dst.port = ep->peer_port;
		dst.family = NET_ENDPOINT_ADDR_IP4;
		dst.ifindex = ep->peer_ifindex;
	}

	if (ep->local_port == 0) {
		port = net_endpoint_alloc_port(ep, ep->local_ip);
		if (port < 0) {
			return (port);
		}
		ep->local_port = (u16)port;
	}

	ifindex = dst.ifindex;
	if (ifindex == NET_ENDPOINT_IF_AUTO) {
		ifindex = ep->ifindex;
	}
	iface = net_endpoint_route(dst.ip, ep->local_ip, ifindex);
	if (!iface) {
		return (-API_ERR_NO_DEVICE);
	}

	empty = 0;
	payload = len == 0 ? &empty : data;
	src_ip = ep->local_ip != 0 ? ep->local_ip : iface->ip_addr;
	if (dst.ip == 0xFFFFFFFF && ep->local_ip == 0 &&
	    ep->local_port == 68) {
		src_ip = 0;
	}
	ret = udp_output_src(iface, src_ip, dst.ip, ep->local_port,
	    dst.port, payload, (u16)len);
	if (ret == 0 || ret == NET_TX_PENDING) {
		return ((int)len);
	}
	return (-API_ERR_IO);
}

int
net_endpoint_recv(net_endpoint_t *ep, u8 *buf, u32 len,
    net_endpoint_addr_t *addr, u32 flags, u32 *out_flags)
{
	net_endpoint_rx_t	*rx;
	u32			to_copy;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (flags & ~(NET_ENDPOINT_MSG_NONBLOCK |
	    NET_ENDPOINT_MSG_TRUNC)) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!buf && len != 0) {
		return (-API_ERR_BAD_ADDR);
	}
	if (out_flags) {
		*out_flags = 0;
	}

	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		return (net_endpoint_tcp_recv_user(ep, buf, len, addr,
		    flags, out_flags));
	}

	while (ep->rx_count == 0) {
		if ((ep->flags & NET_ENDPOINT_FLAG_NONBLOCK) ||
		    (flags & NET_ENDPOINT_MSG_NONBLOCK)) {
			return (-API_ERR_RETRY);
		}
		proc_sleep((void *)ep);
		if (!ep->used) {
			return (-API_ERR_BAD_HANDLE);
		}
	}

	rx = &ep->rx[ep->rx_head];
	to_copy = rx->len;
	if (to_copy > len) {
		to_copy = len;
		if (out_flags) {
			*out_flags |= NET_ENDPOINT_MSG_TRUNC;
		}
	}
	if (to_copy != 0) {
		memcpy(buf, rx->data, to_copy);
	}
	if (addr) {
		addr->ip = rx->src_ip;
		addr->port = rx->src_port;
		addr->family = NET_ENDPOINT_ADDR_IP4;
		addr->ifindex = rx->ifindex;
	}

	ep->rx_head = (ep->rx_head + 1) % NET_ENDPOINT_RX_QUEUE;
	ep->rx_count--;
	ep->rx_bytes -= rx->len;
	memset(rx, 0, sizeof(*rx));
	event_notify_net_change(ep);
	return ((int)to_copy);
}

void
net_endpoint_get_local(net_endpoint_t *ep, net_endpoint_addr_t *addr)
{
	if (!addr) {
		return;
	}
	memset(addr, 0, sizeof(*addr));
	addr->family = NET_ENDPOINT_ADDR_IP4;
	addr->ifindex = NET_ENDPOINT_IF_AUTO;
	if (!ep || !ep->used) {
		return;
	}
	addr->ip = ep->local_ip;
	addr->port = ep->local_port;
	addr->ifindex = ep->ifindex;
}

int
net_endpoint_get_peer(net_endpoint_t *ep, net_endpoint_addr_t *addr)
{
	if (!ep || !ep->used || !addr) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (ep->peer_ip == 0 || ep->peer_port == 0) {
		return (-API_ERR_NOT_FOUND);
	}
	addr->ip = ep->peer_ip;
	addr->port = ep->peer_port;
	addr->family = NET_ENDPOINT_ADDR_IP4;
	addr->ifindex = ep->peer_ifindex;
	return (0);
}

/*
 * Reports connection progress and drains the latched error, which is what a
 * nonblocking connect() needs to finish: send() alone cannot distinguish a
 * handshake still in flight from one the peer refused - both leave it failing.
 * Reading clears tcp_error so the condition is reported exactly once, the same
 * contract as SO_ERROR.
 */
int
net_endpoint_get_state(net_endpoint_t *ep, u32 *out_state, u32 *out_error)
{
	u32	state;

	if (!ep || !ep->used) {
		return (-API_ERR_BAD_HANDLE);
	}

	if (ep->proto != NET_ENDPOINT_PROTO_TCP) {
		/*
		 * Datagram sockets have no handshake; "connected" here only
		 * means a default peer has been set by connect().
		 */
		state = (ep->peer_ip != 0 && ep->peer_port != 0) ?
		    NET_ENDPOINT_STATE_CONNECTED : NET_ENDPOINT_STATE_CLOSED;
		if (out_state) {
			*out_state = state;
		}
		if (out_error) {
			*out_error = 0;
		}
		return (0);
	}

	switch (ep->tcp_state) {
	case TCP_STATE_LISTEN:
		state = NET_ENDPOINT_STATE_LISTEN;
		break;
	case TCP_STATE_SYN_SENT:
	case TCP_STATE_SYN_RECEIVED:
		state = NET_ENDPOINT_STATE_CONNECTING;
		break;
	case TCP_STATE_ESTABLISHED:
		state = NET_ENDPOINT_STATE_CONNECTED;
		break;
	case TCP_STATE_CLOSE_WAIT:
		state = NET_ENDPOINT_STATE_PEER_CLOSED;
		break;
	case TCP_STATE_FIN_WAIT_1:
	case TCP_STATE_FIN_WAIT_2:
	case TCP_STATE_CLOSING:
	case TCP_STATE_LAST_ACK:
	case TCP_STATE_TIME_WAIT:
		state = NET_ENDPOINT_STATE_CLOSING;
		break;
	case TCP_STATE_CLOSED:
	default:
		state = NET_ENDPOINT_STATE_CLOSED;
		break;
	}

	if (out_state) {
		*out_state = state;
	}
	if (out_error) {
		*out_error = (u32)ep->tcp_error;
	}
	ep->tcp_error = 0;
	return (0);
}

int
net_endpoint_readable(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		return (net_endpoint_tcp_readable(ep));
	}
	return (ep->rx_count > 0);
}

int
net_endpoint_writable(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		return (net_endpoint_tcp_writable(ep));
	}
	return (1);
}

u32
net_endpoint_pending_bytes(net_endpoint_t *ep)
{
	if (!ep || !ep->used) {
		return (0);
	}
	if (ep->proto == NET_ENDPOINT_PROTO_TCP) {
		return (net_endpoint_tcp_pending_bytes(ep));
	}
	if (ep->rx_count == 0) {
		return (0);
	}
	if (ep->rx_bytes == 0) {
		return (ep->rx_count);
	}
	return (ep->rx_bytes);
}

u32
net_endpoint_write_space(net_endpoint_t *ep)
{
	if (ep && ep->used && ep->proto == NET_ENDPOINT_PROTO_TCP) {
		return (net_endpoint_tcp_write_space(ep));
	}
	return (NET_ENDPOINT_DGRAM_MAX);
}

int
net_endpoint_udp_input(net_iface_t *iface, u32 src_ip, u32 dst_ip,
    u16 src_port, u16 dst_port, const u8 *data, u16 len)
{
	net_endpoint_t	*ep;
	int		i;

	for (i = 0; i < NET_ENDPOINT_MAX; i++) {
		ep = &g_endpoints[i];
		if (!net_endpoint_match(ep, iface, src_ip, dst_ip,
		    src_port, dst_port)) {
			continue;
		}
		net_endpoint_enqueue(ep, iface, src_ip, dst_ip,
		    src_port, dst_port, data, len);
		return (1);
	}
	return (0);
}

void
net_endpoint_tick(void)
{
	net_endpoint_tcp_tick();
}
