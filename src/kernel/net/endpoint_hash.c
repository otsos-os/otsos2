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

$define %func net_endpoint_hash_init as procedure with args void
$define %func net_endpoint_hash_insert as procedure with args net_endpoint_t *
$define %func net_endpoint_hash_remove as procedure with args net_endpoint_t *
$define %func net_endpoint_find as function with args u32, u32, u16, u16
$define %func net_endpoint_find_listener as function with args u32, u16

*/

/* !SPACE!

$space %export net_endpoint_hash_init, net_endpoint_hash_insert
$space %export net_endpoint_hash_remove
$space %export net_endpoint_find, net_endpoint_find_listener

*/

#include <kernel/net/endpoint_hash.h>
#include <kernel/net/endpoint_internal.h>
#include <mlibc/mlibc.h>

static int	g_conn_head[NET_ENDPOINT_HASH_SIZE];
static int	g_list_head[NET_ENDPOINT_HASH_SIZE];
static int	g_initialized;


static u32
net_endpoint_hash_mix(const u8 *data, u32 len, u32 seed)
{
	u32	hash;
	u32	i;

	hash = seed;
	for (i = 0; i < len; i++) {
		hash ^= data[i];
		hash *= 16777619u;
	}
	return (hash);
}

static int
net_endpoint_conn_bucket(u32 local_ip, u32 peer_ip, u16 local_port,
    u16 peer_port)
{
	u8	key[12];
	u32	hash;

	key[0] = (u8)(local_port >> 8);
	key[1] = (u8)local_port;
	key[2] = (u8)(peer_port >> 8);
	key[3] = (u8)peer_port;
	key[4] = (u8)(peer_ip >> 24);
	key[5] = (u8)(peer_ip >> 16);
	key[6] = (u8)(peer_ip >> 8);
	key[7] = (u8)peer_ip;
	key[8] = (u8)(local_ip >> 24);
	key[9] = (u8)(local_ip >> 16);
	key[10] = (u8)(local_ip >> 8);
	key[11] = (u8)local_ip;
	hash = net_endpoint_hash_mix(key, sizeof(key), 2166136261u);
	return ((int)(hash % NET_ENDPOINT_HASH_SIZE));
}

static int
net_endpoint_list_bucket(u32 local_ip, u16 local_port)
{
	u8	key[6];
	u32	hash;

	key[0] = (u8)(local_port >> 8);
	key[1] = (u8)local_port;
	key[2] = (u8)(local_ip >> 24);
	key[3] = (u8)(local_ip >> 16);
	key[4] = (u8)(local_ip >> 8);
	key[5] = (u8)local_ip;
	hash = net_endpoint_hash_mix(key, sizeof(key), 2166136261u);
	return ((int)(hash % NET_ENDPOINT_HASH_SIZE));
}

void
net_endpoint_hash_init(void)
{
	int	i;

	for (i = 0; i < NET_ENDPOINT_HASH_SIZE; i++) {
		g_conn_head[i] = NET_ENDPOINT_HASH_NONE;
		g_list_head[i] = NET_ENDPOINT_HASH_NONE;
	}
	g_initialized = 1;
}


void
net_endpoint_hash_insert(net_endpoint_t *ep)
{
	int	bucket;

	if (!g_initialized || !ep || !ep->used) {
		return;
	}
	net_endpoint_hash_remove(ep);

	if (ep->proto == NET_ENDPOINT_PROTO_TCP &&
	    ep->mode == NET_ENDPOINT_MODE_STREAM &&
	    ep->tcp_state == TCP_STATE_LISTEN) {
		bucket = net_endpoint_list_bucket(ep->local_ip,
		    ep->local_port);
		ep->hash_next_listener = g_list_head[bucket];
		g_list_head[bucket] = (int)(ep - g_endpoints);
		ep->hash_in_listener = 1;
	} else if (ep->proto == NET_ENDPOINT_PROTO_TCP &&
	    ep->mode == NET_ENDPOINT_MODE_STREAM && ep->local_port != 0 &&
	    ep->peer_port != 0) {
		bucket = net_endpoint_conn_bucket(ep->local_ip,
		    ep->peer_ip, ep->local_port, ep->peer_port);
		ep->hash_next_connected = g_conn_head[bucket];
		g_conn_head[bucket] = (int)(ep - g_endpoints);
		ep->hash_in_connected = 1;
	}
}

void
net_endpoint_hash_remove(net_endpoint_t *ep)
{
	net_endpoint_t	*cursor;
	int		bucket;
	int		*prev_slot;

	if (!g_initialized || !ep) {
		return;
	}

	if (ep->hash_in_listener) {
		bucket = net_endpoint_list_bucket(ep->local_ip,
		    ep->local_port);
		prev_slot = &g_list_head[bucket];
		while (*prev_slot != NET_ENDPOINT_HASH_NONE) {
			cursor = &g_endpoints[*prev_slot];
			if (cursor == ep) {
				*prev_slot = ep->hash_next_listener;
				break;
			}
			prev_slot = &cursor->hash_next_listener;
		}
		ep->hash_next_listener = NET_ENDPOINT_HASH_NONE;
		ep->hash_in_listener = 0;
	}

	if (ep->hash_in_connected) {
		bucket = net_endpoint_conn_bucket(ep->local_ip,
		    ep->peer_ip, ep->local_port, ep->peer_port);
		prev_slot = &g_conn_head[bucket];
		while (*prev_slot != NET_ENDPOINT_HASH_NONE) {
			cursor = &g_endpoints[*prev_slot];
			if (cursor == ep) {
				*prev_slot = ep->hash_next_connected;
				break;
			}
			prev_slot = &cursor->hash_next_connected;
		}
		ep->hash_next_connected = NET_ENDPOINT_HASH_NONE;
		ep->hash_in_connected = 0;
	}
}


net_endpoint_t *
net_endpoint_find(u32 local_ip, u32 peer_ip, u16 local_port, u16 peer_port)
{
	net_endpoint_t	*ep;
	int		slot;
	int		bucket;

	if (!g_initialized) {
		return (NULL);
	}
	bucket = net_endpoint_conn_bucket(local_ip, peer_ip, local_port,
	    peer_port);
	slot = g_conn_head[bucket];
	while (slot != NET_ENDPOINT_HASH_NONE) {
		ep = &g_endpoints[slot];
		if (ep->used && ep->proto == NET_ENDPOINT_PROTO_TCP &&
		    ep->mode == NET_ENDPOINT_MODE_STREAM &&
		    ep->tcp_state != TCP_STATE_LISTEN &&
		    ep->local_port == local_port &&
		    ep->peer_port == peer_port &&
		    ep->peer_ip == peer_ip &&
		    (ep->local_ip == 0 || ep->local_ip == local_ip)) {
			return (ep);
		}
		slot = ep->hash_next_connected;
	}
	return (NULL);
}

net_endpoint_t *
net_endpoint_find_listener(u32 local_ip, u16 local_port)
{
	net_endpoint_t	*ep;
	int		slot;
	int		bucket;

	if (!g_initialized) {
		return (NULL);
	}
	bucket = net_endpoint_list_bucket(local_ip, local_port);
	slot = g_list_head[bucket];
	while (slot != NET_ENDPOINT_HASH_NONE) {
		ep = &g_endpoints[slot];
		if (ep->used && ep->proto == NET_ENDPOINT_PROTO_TCP &&
		    ep->mode == NET_ENDPOINT_MODE_STREAM &&
		    ep->tcp_state == TCP_STATE_LISTEN &&
		    ep->local_port == local_port &&
		    (ep->local_ip == 0 || ep->local_ip == local_ip)) {
			return (ep);
		}
		slot = ep->hash_next_listener;
	}
	return (NULL);
}
