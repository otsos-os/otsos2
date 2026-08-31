/* !DEFINES!

$define %type mtp_transport as TCP intermediate framing state
$define %type mtp_dc as datacenter id paired with its address
$define %func mtp_dc_address as function with args int
$define %func mtp_dc_id as function with args int
$define %func mtp_dc_index_of as function with args int
$define %func mtp_now_ms as function with args void
$define %func mtp_unix_time as function with args client
$define %func mtp_unix_time_ns as function with args client, out nanoseconds
$define %func mtp_wait as function with args client, fd, filter
$define %func mtp_unwatch as procedure with args client
$define %func mtp_transport_open as function with args client
$define %func mtp_transport_close as procedure with args client
$define %func mtp_transport_check_connect as function with args client
$define %func mtp_transport_queue as function with args client, data, length
$define %func mtp_transport_flush as function with args client
$define %func mtp_transport_recv as function with args client
$define %func mtp_transport_take_frame as function with args client, out, out length
$define %func mtp_transport_drop_frame as procedure with args client, length

*/

/* !SPACE!

$space %internal transport_grow, mtp_dc_table
$space %export mtp_dc, mtp_dc_address, mtp_dc_id, mtp_dc_index_of
$space %export mtp_now_ms, mtp_unix_time, mtp_unix_time_ns
$space %export mtp_wait, mtp_unwatch
$space %export mtp_transport_open, mtp_transport_close
$space %export mtp_transport_check_connect, mtp_transport_queue
$space %export mtp_transport_flush, mtp_transport_recv
$space %export mtp_transport_take_frame, mtp_transport_drop_frame

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS, USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



#include <errno.h>
#include <native.h>
#include <stdlib.h>
#include <string.h>

#include "mtp_internal.h"

#define TRANSPORT_IN_INIT	16384
#define TRANSPORT_RECV_ROUNDS	16
#define TRANSPORT_PORT		443

static const struct mtp_dc {
	int		id;
	uint32_t	address;
} mtp_dc_table[] = {
	{ 2, (149u << 24) | (154u << 16) | (167u << 8) | 51u },
	{ 1, (149u << 24) | (154u << 16) | (175u << 8) | 53u },
	{ 3, (149u << 24) | (154u << 16) | (175u << 8) | 100u },
	{ 4, (149u << 24) | (154u << 16) | (167u << 8) | 91u },
	{ 5, (91u << 24) | (108u << 16) | (56u << 8) | 130u }
};

#define MTP_DC_COUNT	(sizeof(mtp_dc_table) / sizeof(mtp_dc_table[0]))

uint32_t
mtp_dc_address(int index)
{
	if (index < 0 || (size_t)index >= MTP_DC_COUNT) {
		return (0);
	}
	return (mtp_dc_table[index].address);
}


int
mtp_dc_id(int index)
{
	if (index < 0 || (size_t)index >= MTP_DC_COUNT) {
		return (0);
	}
	return (mtp_dc_table[index].id);
}

int
mtp_dc_index_of(int id)
{
	size_t	i;

	for (i = 0; i < MTP_DC_COUNT; i++) {
		if (mtp_dc_table[i].id == id) {
			return ((int)i);
		}
	}
	return (-1);
}

uint64_t
mtp_now_ms(void)
{
	static uint64_t		last;
	struct api_timeinfo	ti;

	memset(&ti, 0, sizeof(ti));
	if (sysTimeInfo(&ti) != 0) {
		last++;
		return (last);
	}
	last = ti.uptime_sec * 1000u + ti.uptime_nsec / 1000000u;
	return (last);
}


int64_t
mtp_unix_time(const mtp_client_t *c)
{
	struct api_timeinfo	ti;

	memset(&ti, 0, sizeof(ti));
	if (sysTimeInfo(&ti) != 0) {
		return (0);
	}
	return ((int64_t)ti.wall_sec + (int64_t)c->time_offset);
}


int64_t
mtp_unix_time_ns(const mtp_client_t *c, uint32_t *out_nsec)
{
	struct api_timeinfo	ti;

	memset(&ti, 0, sizeof(ti));
	if (sysTimeInfo(&ti) != 0) {
		if (out_nsec != NULL) {
			*out_nsec = 0;
		}
		return (0);
	}
	if (out_nsec != NULL) {
		*out_nsec = ti.wall_nsec < 1000000000ull ?
		    (uint32_t)ti.wall_nsec : 999999999u;
	}
	return ((int64_t)ti.wall_sec + (int64_t)c->time_offset);
}


void
mtp_unwatch(mtp_client_t *c)
{
	struct kevent	change;

	if (c->wait_fd < 0 || c->kq < 0) {
		c->wait_fd = -1;
		c->wait_filter = 0;
		return;
	}

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)c->wait_fd;
	change.filter = c->wait_filter;
	change.flags = EV_DELETE;
	(void)eventWait(c->kq, &change, 1, NULL, 0, 0);

	c->wait_fd = -1;
	c->wait_filter = 0;
}


int
mtp_wait(mtp_client_t *c, int fd, int16_t filter)
{
	struct kevent	change;

	if (c->wait_fd == fd && c->wait_filter == filter) {
		return (0);
	}
	mtp_unwatch(c);
	if (fd < 0 || c->kq < 0) {
		return (0);
	}

	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)fd;
	change.filter = filter;
	change.flags = EV_ADD | EV_CLEAR;
	if (eventWait(c->kq, &change, 1, NULL, 0, 0) < 0) {
		mtp_logf(MTP_LOG_ERROR, "kqueue: cannot arm fd=%d filter=%d "
		    "(errno=%d)", fd, (int)filter, errno);
		return (-1);
	}
	mtp_logf(MTP_LOG_TRACE, "kqueue: armed fd=%d filter=%s", fd,
	    filter == EVFILT_READ ? "read" : "write");
	c->wait_fd = fd;
	c->wait_filter = filter;
	return (0);
}

void
mtp_transport_close(mtp_client_t *c)
{
	if (c->sock >= 0) {
		if (c->wait_fd == c->sock) {
			mtp_unwatch(c);
		}
		dataClose(c->sock);
		c->sock = -1;
	}
	c->out_len = 0;
	c->out_sent = 0;
	c->in_len = 0;
	c->magic_sent = 0;
}

int
mtp_transport_open(mtp_client_t *c)
{
	struct api_net_addr	addr;
	uint32_t		ip;
	int			ret;

	mtp_transport_close(c);

	ip = mtp_dc_address(c->dc_index);
	if (ip == 0) {
		return (mtp_fail(c, MTP_ERR_INVAL, "no address for datacenter "
		    "slot %d", c->dc_index));
	}

	if (c->in_buf == NULL) {
		c->in_buf = (uint8_t *)malloc(TRANSPORT_IN_INIT);
		if (c->in_buf == NULL) {
			return (mtp_fail(c, MTP_ERR_NOMEM,
			    "out of memory for receive buffer"));
		}
		c->in_cap = TRANSPORT_IN_INIT;
	}

	c->sock = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM,
	    API_NET_OPEN_NONBLOCK);
	if (c->sock < 0) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "cannot open TCP socket (errno=%d)", errno));
	}
	mtp_logf(MTP_LOG_INFO, "connect: DC%d %u.%u.%u.%u:%d fd=%d",
	    mtp_dc_id(c->dc_index), (unsigned int)(ip >> 24) & 0xffu,
	    (unsigned int)(ip >> 16) & 0xffu, (unsigned int)(ip >> 8) & 0xffu,
	    (unsigned int)ip & 0xffu, TRANSPORT_PORT, c->sock);

	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.port = TRANSPORT_PORT;
	addr.ip = ip;

	c->deadline = mtp_now_ms() + MTP_CONNECT_TIMEOUT_MS;
	ret = netConnect(c->sock, &addr);
	if (ret != 0 && errno != EAGAIN) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "connect to DC%d failed immediately (errno=%d)",
		    mtp_dc_id(c->dc_index), errno));
	}
	mtp_logf(MTP_LOG_DEBUG, "connect: %s", ret == 0 ? "completed inline" :
	    "in progress");


	mtp_set_state(c, MTP_STATE_CONNECT, "socket opened");
	if (mtp_wait(c, c->sock, EVFILT_WRITE) != 0) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "cannot watch socket for writability"));
	}
	return (MTP_OK);
}


int
mtp_transport_check_connect(mtp_client_t *c)
{
	struct api_net_state	st;

	memset(&st, 0, sizeof(st));
	if (netCtl(c->sock, API_NET_CTL_GET_STATE, &st) != 0) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "cannot read socket state (errno=%d)", errno));
	}
	if (st.error != 0) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "DC%d refused the connection (socket error %d)",
		    mtp_dc_id(c->dc_index), (int)st.error));
	}
	if (st.state == API_NET_STATE_CONNECTED) {
		mtp_logf(MTP_LOG_INFO, "connect: DC%d established",
		    mtp_dc_id(c->dc_index));
		return (1);
	}
	if (st.state == API_NET_STATE_PEER_CLOSED) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "DC%d closed the connection during setup",
		    mtp_dc_id(c->dc_index)));
	}
	if (st.state != API_NET_STATE_CONNECTING) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "connection to DC%d failed, socket state %d with no error "
		    "(likely RST)", mtp_dc_id(c->dc_index), (int)st.state));
	}
	if (mtp_now_ms() >= c->deadline) {
		return (mtp_fail(c, MTP_ERR_TIMEOUT,
		    "connection to DC%d timed out", mtp_dc_id(c->dc_index)));
	}
	return (0);
}

int
mtp_transport_queue(mtp_client_t *c, const void *data, size_t len)
{
	uint8_t	head[MTP_LEN_PREFIX];
	size_t	need;

	if (data == NULL || len == 0 || len > MTP_MAX_FRAME ||
	    (len % 4) != 0) {
		return (mtp_fail(c, MTP_ERR_INVAL,
		    "outgoing frame length %u is not a positive multiple of 4",
		    (unsigned int)len));
	}

	need = MTP_LEN_PREFIX + len;
	if (!c->magic_sent) {
		need += 4;
	}
	mtp_logf(MTP_LOG_TRACE, "tx: queue %u bytes (magic=%s buffered=%u)",
	    (unsigned int)len, c->magic_sent ? "sent" : "pending",
	    (unsigned int)c->out_len);
	if (need > sizeof(c->out_buf) - c->out_len) {

		return (mtp_fail(c, MTP_ERR_BUSY,
		    "send buffer full: need %u, %u of %u free",
		    (unsigned int)need,
		    (unsigned int)(sizeof(c->out_buf) - c->out_len),
		    (unsigned int)sizeof(c->out_buf)));
	}

	if (!c->magic_sent) {
		c->out_buf[c->out_len + 0] = 0xEE;
		c->out_buf[c->out_len + 1] = 0xEE;
		c->out_buf[c->out_len + 2] = 0xEE;
		c->out_buf[c->out_len + 3] = 0xEE;
		c->out_len += 4;
		c->magic_sent = 1;
	}

	head[0] = (uint8_t)(len & 0xFFu);
	head[1] = (uint8_t)((len >> 8) & 0xFFu);
	head[2] = (uint8_t)((len >> 16) & 0xFFu);
	head[3] = (uint8_t)((len >> 24) & 0xFFu);
	memcpy(c->out_buf + c->out_len, head, sizeof(head));
	c->out_len += sizeof(head);
	memcpy(c->out_buf + c->out_len, data, len);
	c->out_len += len;
	return (MTP_OK);
}


int
mtp_transport_flush(mtp_client_t *c)
{
	struct api_net_msg	msg;
	ssize_t			n;

	while (c->out_sent < c->out_len) {
		memset(&msg, 0, sizeof(msg));
		msg.data = c->out_buf + c->out_sent;
		msg.length = (uint32_t)(c->out_len - c->out_sent);
		n = netSend(c->sock, &msg);
		if (n < 0) {
			if (errno != EAGAIN) {
				return (mtp_fail(c, MTP_ERR_NET,
				    "send failed after %u of %u bytes "
				    "(errno=%d)", (unsigned int)c->out_sent,
				    (unsigned int)c->out_len, errno));
			}
			return (mtp_wait(c, c->sock, EVFILT_WRITE) == 0 ? 0 :
			    mtp_fail(c, MTP_ERR_NET, "cannot arm a write watch on "
			    "socket %d (errno=%d)", c->sock, errno));
		}
		if (n == 0) {
			return (mtp_wait(c, c->sock, EVFILT_WRITE) == 0 ? 0 :
			    mtp_fail(c, MTP_ERR_NET, "cannot arm a write watch on "
			    "socket %d after a zero-byte send (errno=%d)",
			    c->sock, errno));
		}
		c->out_sent += (size_t)n;
	}

	mtp_logf(MTP_LOG_TRACE, "tx: flushed %u bytes",
	    (unsigned int)c->out_len);
	c->out_len = 0;
	c->out_sent = 0;
	if (mtp_wait(c, c->sock, EVFILT_READ) != 0) {
		return (mtp_fail(c, MTP_ERR_NET,
		    "cannot watch socket for readability"));
	}
	return (1);
}

static int
transport_grow(mtp_client_t *c, size_t need)
{
	uint8_t	*buf;
	size_t	cap;

	if (c->in_len + need <= c->in_cap) {
		return (0);
	}
	if (c->in_len + need > MTP_MAX_FRAME + MTP_LEN_PREFIX) {
		return (-1);
	}

	cap = (c->in_cap != 0) ? c->in_cap : TRANSPORT_IN_INIT;
	while (cap < c->in_len + need) {
		cap *= 2;
		if (cap > MTP_MAX_FRAME + MTP_LEN_PREFIX) {
			cap = MTP_MAX_FRAME + MTP_LEN_PREFIX;
			break;
		}
	}
	if (cap < c->in_len + need) {
		return (-1);
	}

	buf = (uint8_t *)realloc(c->in_buf, cap);
	if (buf == NULL) {
		return (-1);
	}
	c->in_buf = buf;
	c->in_cap = cap;
	return (0);
}


int
mtp_transport_recv(mtp_client_t *c)
{
	struct api_net_msg	msg;
	ssize_t			n;
	int			rounds, got;

	got = 0;
	for (rounds = 0; rounds < TRANSPORT_RECV_ROUNDS; rounds++) {
		if (c->in_len == MTP_MAX_FRAME + MTP_LEN_PREFIX) {
			break;
		}
		if (transport_grow(c, 4096) != 0) {
			return (mtp_fail(c, MTP_ERR_PROTO,
			    "receive buffer limit reached"));
		}
		memset(&msg, 0, sizeof(msg));
		msg.data = c->in_buf + c->in_len;
		msg.length = (uint32_t)(c->in_cap - c->in_len);
		n = netRecv(c->sock, &msg);
		if (n < 0) {
			if (errno != EAGAIN) {
				return (mtp_fail(c, MTP_ERR_NET,
				    "receive failed (errno=%d)", errno));
			}
			break;
		}
		if (n == 0) {
			return (mtp_fail(c, MTP_ERR_NET,
			    "DC%d closed the connection in state %s after %u "
			    "buffered bytes", mtp_dc_id(c->dc_index),
			    mtpStateName(c->state), (unsigned int)c->in_len));
		}
		c->in_len += (size_t)n;
		mtp_logf(MTP_LOG_TRACE, "rx: %u bytes (buffered=%u)",
		    (unsigned int)n, (unsigned int)c->in_len);
		got = 1;
	}
	return (got);
}

int
mtp_transport_take_frame(mtp_client_t *c, const uint8_t **out, size_t *out_len)
{
	uint32_t	len;

	*out = NULL;
	*out_len = 0;
	if (c->in_len < MTP_LEN_PREFIX) {
		return (0);
	}

	len = (uint32_t)c->in_buf[0] | ((uint32_t)c->in_buf[1] << 8) |
	    ((uint32_t)c->in_buf[2] << 16) | ((uint32_t)c->in_buf[3] << 24);

	if (len == 4) {
		if (c->in_len < MTP_LEN_PREFIX + 4) {
			return (0);
		}
		{
			int32_t	code;

			code = (int32_t)((uint32_t)c->in_buf[4] |
			    ((uint32_t)c->in_buf[5] << 8) |
			    ((uint32_t)c->in_buf[6] << 16) |
			    ((uint32_t)c->in_buf[7] << 24));
			if (code < 0) {
				mtp_transport_drop_frame(c,
				    MTP_LEN_PREFIX + 4);
				if (code == -404 && c->auth_key_valid) {
					return (mtp_rekey(c) == MTP_OK ? 0 :
					    c->last_error);
				}
				return (mtp_fail(c, MTP_ERR_PROTO,
				    "DC%d transport error %d%s",
				    mtp_dc_id(c->dc_index), code,
				    code != -404 ? "" : c->auth_key_valid ?
				    " (auth_key_id unknown to this DC; delete "
				    "the auth file to re-key)" :
				    " (no key in play yet -- the server rejected "
				    "req_DH_params itself; check the dc field "
				    "and the RSA construction in "
				    "hs_rsa_encrypt)"));
			}
		}
	}

	if (len == 0 || len > MTP_MAX_FRAME || (len % 4) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO,
		    "incoming frame length %u is not a positive multiple of 4 "
		    "below %u (transport desynchronised)", (unsigned int)len,
		    (unsigned int)MTP_MAX_FRAME));
	}
	if (c->in_len < MTP_LEN_PREFIX + len) {
		if (transport_grow(c, MTP_LEN_PREFIX + len - c->in_len) != 0) {
			return (mtp_fail(c, MTP_ERR_PROTO,
			    "frame of %u bytes exceeds the receive limit %u",
			    (unsigned int)len, (unsigned int)MTP_MAX_FRAME));
		}
		mtp_logf(MTP_LOG_TRACE, "rx: partial frame, have %u of %u",
		    (unsigned int)(c->in_len - MTP_LEN_PREFIX),
		    (unsigned int)len);
		return (0);
	}

	mtp_logf(MTP_LOG_TRACE, "rx: complete frame of %u bytes",
	    (unsigned int)len);
	*out = c->in_buf + MTP_LEN_PREFIX;
	*out_len = len;
	return (1);
}

void
mtp_transport_drop_frame(mtp_client_t *c, size_t len)
{
	if (len > c->in_len) {
		len = c->in_len;
	}
	c->in_len -= len;
	if (c->in_len != 0) {
		memmove(c->in_buf, c->in_buf + len, c->in_len);
	}
}
