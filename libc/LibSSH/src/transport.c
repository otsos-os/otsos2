/* !DEFINES!

$define %type lssh_transport as native SSH transport session
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_slice as borrowed byte span
$define %type lssh_ident as parsed SSH identification line
$define %type lssh_kexinit as parsed SSH_MSG_KEXINIT payload
$define %type api_net_addr as native IPv4 endpoint address
$define %type api_net_msg as native network message descriptor
$define %func lssh_transport_ready as function with args lssh_transport *
$define %func lssh_transport_compact as procedure with args lssh_transport *, size_t
$define %func lssh_transport_read_more as function with args lssh_transport *
$define %func lssh_transport_write_all as function with args lssh_transport *, const void *, size_t
$define %func lssh_transport_store_ident as function with args char *, size_t *, const uint8_t *, size_t
$define %func lssh_transport_init as function with args lssh_transport *
$define %func lssh_transport_free as procedure with args lssh_transport *
$define %func lssh_transport_attach as function with args lssh_transport *, int, int
$define %func lssh_transport_close as procedure with args lssh_transport *
$define %func lssh_transport_connect_ipv4 as function with args lssh_transport *, uint32_t, uint32_t
$define %func lssh_transport_send_ident as function with args lssh_transport *, const char *, const char *
$define %func lssh_transport_recv_ident as function with args lssh_transport *, lssh_ident *
$define %func lssh_transport_exchange_ident as function with args lssh_transport *, const char *, const char *, lssh_ident *
$define %func lssh_transport_send_plain as function with args lssh_transport *, const void *, size_t
$define %func lssh_transport_recv_plain as function with args lssh_transport *, lssh_buf *
$define %func lssh_transport_send_packet as function with args lssh_transport *, const void *, size_t
$define %func lssh_transport_recv_packet as function with args lssh_transport *, lssh_buf *
$define %func lssh_transport_packet_pending as function with args const lssh_transport *
$define %func lssh_transport_set_chachapoly as function with args lssh_transport *, const uint8_t *, const uint8_t *
$define %func lssh_transport_set_chachapoly_read as function with args lssh_transport *, const uint8_t *
$define %func lssh_transport_set_chachapoly_write as function with args lssh_transport *, const uint8_t *
$define %func lssh_transport_clear_crypto as procedure with args lssh_transport *
$define %func lssh_transport_send_kexinit as function with args lssh_transport *, const lssh_kexinit_names *, const uint8_t *
$define %func lssh_transport_recv_kexinit as function with args lssh_transport *, lssh_buf *, lssh_kexinit *

*/

/* !SPACE!

$space %internal lssh_transport_ready, lssh_transport_compact
$space %internal lssh_transport_read_more, lssh_transport_write_all
$space %internal lssh_transport_store_ident
$space %export lssh_transport_init, lssh_transport_free
$space %export lssh_transport_attach, lssh_transport_close
$space %export lssh_transport_connect_ipv4
$space %export lssh_transport_send_ident, lssh_transport_recv_ident
$space %export lssh_transport_exchange_ident
$space %export lssh_transport_send_plain, lssh_transport_recv_plain
$space %export lssh_transport_send_packet, lssh_transport_recv_packet
$space %export lssh_transport_packet_pending
$space %export lssh_transport_set_chachapoly
$space %export lssh_transport_set_chachapoly_read
$space %export lssh_transport_set_chachapoly_write
$space %export lssh_transport_clear_crypto
$space %export lssh_transport_send_kexinit, lssh_transport_recv_kexinit

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
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <libssh.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

#define LSSH_TRANSPORT_RX_INITIAL	4096
#define LSSH_TRANSPORT_TX_INITIAL	4096
#define LSSH_TRANSPORT_READ_CHUNK	4096

static int
lssh_transport_ready(lssh_transport *transport)
{
	if (!transport || transport->handle < 0 || transport->closed) {
		return (0);
	}
	return (1);
}

static void
lssh_transport_compact(lssh_transport *transport, size_t consumed)
{
	size_t	left;

	if (!transport || consumed == 0) {
		return;
	}
	if (consumed >= transport->rx.len) {
		lssh_buf_reset(&transport->rx);
		return;
	}
	left = transport->rx.len - consumed;
	memmove(transport->rx.data, transport->rx.data + consumed, left);
	transport->rx.len = left;
}

static int
lssh_transport_read_more(lssh_transport *transport)
{
	struct api_net_msg	msg;
	size_t			space;
	ssize_t			n;
	int			ret;

	if (!lssh_transport_ready(transport)) {
		return (LSSH_ERR_STATE);
	}
	space = transport->rx.capacity - transport->rx.len;
	if (space < LSSH_TRANSPORT_READ_CHUNK) {
		ret = lssh_buf_reserve(&transport->rx,
		    LSSH_TRANSPORT_READ_CHUNK);
		if (ret != LSSH_OK) {
			return (ret);
		}
		space = transport->rx.capacity - transport->rx.len;
	}
	if (space > UINT32_MAX) {
		space = UINT32_MAX;
	}
	memset(&msg, 0, sizeof(msg));
	msg.data = transport->rx.data + transport->rx.len;
	msg.length = (uint32_t)space;
	n = netRecv(transport->handle, &msg);
	if (n <= 0 || (size_t)n > space) {
		lssh_logf(LSSH_LOG_ERROR,
		    "transport: netRecv failed handle=%d n=%d",
		    transport->handle, (int)n);
		transport->closed = 1;
		return (LSSH_ERR_IO);
	}
	transport->rx.len += (size_t)n;
	lssh_logf(LSSH_LOG_DEBUG,
	    "transport: read %lu bytes rx_len=%lu",
	    (unsigned long)n, (unsigned long)transport->rx.len);
	return (LSSH_OK);
}

static int
lssh_transport_write_all(lssh_transport *transport, const void *data,
    size_t len)
{
	struct api_net_msg	msg;
	const uint8_t		*p;
	size_t			left, chunk, off;
	ssize_t			n;

	if (!lssh_transport_ready(transport) || (!data && len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	p = (const uint8_t *)data;
	off = 0;
	while (off < len) {
		left = len - off;
		chunk = left;
		if (chunk > UINT32_MAX) {
			chunk = UINT32_MAX;
		}
		memset(&msg, 0, sizeof(msg));
		msg.data = (void *)(p + off);
		msg.length = (uint32_t)chunk;
		n = netSend(transport->handle, &msg);
		if (n <= 0 || (size_t)n > chunk) {
			lssh_logf(LSSH_LOG_ERROR,
			    "transport: netSend failed handle=%d n=%d",
			    transport->handle, (int)n);
			transport->closed = 1;
			return (LSSH_ERR_IO);
		}
		off += (size_t)n;
	}
	lssh_logf(LSSH_LOG_DEBUG, "transport: wrote %lu bytes",
	    (unsigned long)len);
	return (LSSH_OK);
}

static int
lssh_transport_store_ident(char *out, size_t *out_len,
    const uint8_t *line, size_t len)
{
	size_t	core_len;

	if (!out || !out_len || !line || len == 0) {
		return (LSSH_ERR_INVALID);
	}
	core_len = len;
	if (core_len != 0 && line[core_len - 1] == '\n') {
		core_len--;
	}
	if (core_len != 0 && line[core_len - 1] == '\r') {
		core_len--;
	}
	if (core_len > LSSH_IDENT_MAX) {
		return (LSSH_ERR_RANGE);
	}
	memcpy(out, line, core_len);
	out[core_len] = '\0';
	*out_len = core_len;
	return (LSSH_OK);
}

int
lssh_transport_init(lssh_transport *transport)
{
	int	ret;

	if (!transport) {
		return (LSSH_ERR_INVALID);
	}
	memset(transport, 0, sizeof(*transport));
	transport->handle = -1;
	ret = lssh_buf_init(&transport->rx, LSSH_TRANSPORT_RX_INITIAL);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&transport->tx, LSSH_TRANSPORT_TX_INITIAL);
	if (ret != LSSH_OK) {
		lssh_buf_free(&transport->rx);
		return (ret);
	}
	lssh_buf_set_secure(&transport->rx, 1);
	lssh_buf_set_secure(&transport->tx, 1);
	return (LSSH_OK);
}

void
lssh_transport_free(lssh_transport *transport)
{
	if (!transport) {
		return;
	}
	lssh_transport_close(transport);
	lssh_transport_clear_crypto(transport);
	lssh_buf_free(&transport->rx);
	lssh_buf_free(&transport->tx);
	memset(transport, 0, sizeof(*transport));
	transport->handle = -1;
}

int
lssh_transport_attach(lssh_transport *transport, int handle, int owns_handle)
{
	if (!transport || handle < 0) {
		return (LSSH_ERR_INVALID);
	}
	if (transport->handle >= 0 && !transport->closed) {
		return (LSSH_ERR_STATE);
	}
	transport->handle = handle;
	transport->owns_handle = owns_handle ? 1 : 0;
	transport->closed = 0;
	transport->seq_in = 0;
	transport->seq_out = 0;
	transport->userauth_service = 0;
	lssh_transport_clear_crypto(transport);
	lssh_buf_reset(&transport->rx);
	lssh_buf_reset(&transport->tx);
	return (LSSH_OK);
}

void
lssh_transport_close(lssh_transport *transport)
{
	if (!transport || transport->handle < 0) {
		return;
	}
	if (transport->owns_handle) {
		dataClose(transport->handle);
	}
	transport->handle = -1;
	transport->owns_handle = 0;
	transport->closed = 1;
}

int
lssh_transport_send_packet(lssh_transport *transport, const void *payload,
    size_t payload_len)
{
	const uint8_t	*p;
	int		ret;
	uint8_t		msg;

	if (!lssh_transport_ready(transport)) {
		return (LSSH_ERR_STATE);
	}
	if (!payload && payload_len != 0) {
		return (LSSH_ERR_INVALID);
	}
	p = (const uint8_t *)payload;
	msg = payload_len != 0 ? p[0] : 0;
	lssh_logf(LSSH_LOG_DEBUG,
	    "transport: send seq=%u encrypted=%d msg=%u(%s) payload_len=%lu",
	    (unsigned int)transport->seq_out, transport->write_encrypted,
	    (unsigned int)msg, lssh_log_packet_type_name(msg),
	    (unsigned long)payload_len);
	lssh_buf_reset(&transport->tx);
	if (transport->write_encrypted) {
		ret = lssh_packet_chachapoly_encode(&transport->tx,
		    &transport->write_cipher, transport->seq_out,
		    payload, payload_len);
	} else {
		ret = lssh_packet_plain_encode(&transport->tx, payload,
		    payload_len, 8);
	}
	if (ret != LSSH_OK) {
		lssh_buf_reset(&transport->tx);
		return (ret);
	}
	ret = lssh_transport_write_all(transport, transport->tx.data,
	    transport->tx.len);
	lssh_buf_reset(&transport->tx);
	if (ret != LSSH_OK) {
		return (ret);
	}
	transport->seq_out++;
	return (LSSH_OK);
}

int
lssh_transport_recv_packet(lssh_transport *transport, lssh_buf *payload)
{
	lssh_slice	slice;
	size_t		consumed;
	uint32_t	seq;
	int		ret;
	uint8_t		msg;

	if (!lssh_transport_ready(transport) || !payload) {
		return (LSSH_ERR_INVALID);
	}
	lssh_buf_reset(payload);
	for (;;) {
		seq = transport->seq_in;
		if (transport->read_encrypted) {
			ret = lssh_packet_chachapoly_decode(
			    transport->rx.data, transport->rx.len,
			    &transport->read_cipher, seq, payload,
			    &consumed);
		} else {
			ret = lssh_packet_plain_decode(transport->rx.data,
			    transport->rx.len, &slice, &consumed);
			if (ret == LSSH_OK) {
				ret = lssh_buf_append(payload, slice.data,
				    slice.len);
			}
		}
		if (ret == LSSH_OK) {
			lssh_transport_compact(transport, consumed);
			transport->seq_in++;
			msg = payload->len != 0 ? payload->data[0] : 0;
			lssh_logf(LSSH_LOG_DEBUG,
			    "transport: recv seq=%u encrypted=%d msg=%u(%s) "
			    "payload_len=%lu consumed=%lu",
			    (unsigned int)seq, transport->read_encrypted,
			    (unsigned int)msg, lssh_log_packet_type_name(msg),
			    (unsigned long)payload->len,
			    (unsigned long)consumed);
			return (LSSH_OK);
		}
		if (ret != LSSH_ERR_AGAIN) {
			lssh_logf(LSSH_LOG_ERROR,
			    "transport: recv failed seq=%u encrypted=%d "
			    "ret=%d rx_len=%lu",
			    (unsigned int)seq, transport->read_encrypted,
			    ret, (unsigned long)transport->rx.len);
			return (ret);
		}
		ret = lssh_transport_read_more(transport);
		if (ret != LSSH_OK) {
			return (ret);
		}
	}
}

int
lssh_transport_packet_pending(const lssh_transport *transport)
{
	lssh_slice	slice;
	size_t		consumed, packet_len;
	uint32_t	packet_len_u32;
	int		ret;

	if (!transport || transport->handle < 0 || transport->closed) {
		return (0);
	}
	if (transport->rx.len == 0) {
		return (0);
	}
	if (transport->read_encrypted) {
		ret = lssh_packet_chachapoly_peek_len(
		    &transport->read_cipher, transport->seq_in,
		    transport->rx.data, transport->rx.len,
		    &packet_len_u32);
		if (ret == LSSH_ERR_AGAIN) {
			return (0);
		}
		if (ret != LSSH_OK) {
			return (ret);
		}
		packet_len = (size_t)packet_len_u32;
		if (transport->rx.len < 4 + packet_len +
		    LSSH_CHACHAPOLY_TAG_SIZE) {
			return (0);
		}
		return (1);
	}
	ret = lssh_packet_plain_decode(transport->rx.data,
	    transport->rx.len, &slice, &consumed);
	if (ret == LSSH_ERR_AGAIN) {
		return (0);
	}
	if (ret != LSSH_OK) {
		return (ret);
	}
	(void)slice;
	(void)consumed;
	return (1);
}

int
lssh_transport_set_chachapoly(lssh_transport *transport,
    const uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE],
    const uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE])
{
	if (!transport || !read_key || !write_key) {
		return (LSSH_ERR_INVALID);
	}
	lssh_chachapoly_init(&transport->read_cipher, read_key);
	lssh_chachapoly_init(&transport->write_cipher, write_key);
	transport->read_encrypted = 1;
	transport->write_encrypted = 1;
	lssh_logf(LSSH_LOG_INFO,
	    "transport: enabled chachapoly read/write seq_in=%u seq_out=%u",
	    (unsigned int)transport->seq_in,
	    (unsigned int)transport->seq_out);
	return (LSSH_OK);
}

int
lssh_transport_set_chachapoly_read(lssh_transport *transport,
    const uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE])
{
	if (!transport || !read_key) {
		return (LSSH_ERR_INVALID);
	}
	lssh_chachapoly_init(&transport->read_cipher, read_key);
	transport->read_encrypted = 1;
	lssh_logf(LSSH_LOG_INFO,
	    "transport: enabled chachapoly read seq_in=%u",
	    (unsigned int)transport->seq_in);
	return (LSSH_OK);
}

int
lssh_transport_set_chachapoly_write(lssh_transport *transport,
    const uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE])
{
	if (!transport || !write_key) {
		return (LSSH_ERR_INVALID);
	}
	lssh_chachapoly_init(&transport->write_cipher, write_key);
	transport->write_encrypted = 1;
	lssh_logf(LSSH_LOG_INFO,
	    "transport: enabled chachapoly write seq_out=%u",
	    (unsigned int)transport->seq_out);
	return (LSSH_OK);
}

void
lssh_transport_clear_crypto(lssh_transport *transport)
{
	if (!transport) {
		return;
	}
	lssh_chachapoly_free(&transport->read_cipher);
	lssh_chachapoly_free(&transport->write_cipher);
	transport->read_encrypted = 0;
	transport->write_encrypted = 0;
}

int
lssh_transport_connect_ipv4(lssh_transport *transport, uint32_t ip,
    uint32_t port)
{
	struct api_net_addr	addr;
	int			handle, ret;

	if (!transport || port == 0 || port > 65535) {
		return (LSSH_ERR_INVALID);
	}
	handle = netOpen(API_NET_PROTO_TCP, API_NET_MODE_STREAM, 0);
	if (handle < 0) {
		return (LSSH_ERR_IO);
	}
	memset(&addr, 0, sizeof(addr));
	addr.family = API_NET_ADDR_IP4;
	addr.ip = ip;
	addr.port = port;
	addr.ifindex = 0;
	if (netConnect(handle, &addr) != 0) {
		dataClose(handle);
		return (LSSH_ERR_IO);
	}
	ret = lssh_transport_attach(transport, handle, 1);
	if (ret != LSSH_OK) {
		dataClose(handle);
		return (ret);
	}
	return (LSSH_OK);
}

int
lssh_transport_send_ident(lssh_transport *transport, const char *software,
    const char *comment)
{
	lssh_buf	line;
	int		ret;

	if (!lssh_transport_ready(transport)) {
		return (LSSH_ERR_STATE);
	}
	ret = lssh_buf_init(&line, 0);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_ident_make(&line, software, comment);
	if (ret == LSSH_OK) {
		ret = lssh_transport_store_ident(transport->local_ident,
		    &transport->local_ident_len, line.data, line.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_transport_write_all(transport, line.data,
		    line.len);
	}
	lssh_buf_free(&line);
	return (ret);
}

int
lssh_transport_recv_ident(lssh_transport *transport, lssh_ident *out)
{
	size_t	consumed, line_start, i, line_len;
	int	ret;

	if (!lssh_transport_ready(transport) || !out) {
		return (LSSH_ERR_INVALID);
	}
	for (;;) {
		ret = lssh_ident_scan(transport->rx.data, transport->rx.len,
		    &consumed, out);
		if (ret == LSSH_OK) {
			line_start = 0;
			for (i = 0; i + 1 < consumed; i++) {
				if (transport->rx.data[i] == '\n') {
					line_start = i + 1;
				}
			}
			line_len = consumed - line_start;
			ret = lssh_transport_store_ident(
			    transport->peer_ident_line,
			    &transport->peer_ident_len,
			    transport->rx.data + line_start, line_len);
			if (ret != LSSH_OK) {
				return (ret);
			}
			transport->peer_ident = *out;
			lssh_transport_compact(transport, consumed);
			return (LSSH_OK);
		}
		if (ret != LSSH_ERR_AGAIN) {
			return (ret);
		}
		if (consumed != 0) {
			lssh_transport_compact(transport, consumed);
		}
		ret = lssh_transport_read_more(transport);
		if (ret != LSSH_OK) {
			return (ret);
		}
	}
}

int
lssh_transport_exchange_ident(lssh_transport *transport,
    const char *software, const char *comment, lssh_ident *out)
{
	int	ret;

	ret = lssh_transport_send_ident(transport, software, comment);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_transport_recv_ident(transport, out));
}

int
lssh_transport_send_plain(lssh_transport *transport, const void *payload,
    size_t payload_len)
{
	int	ret;

	if (!lssh_transport_ready(transport)) {
		return (LSSH_ERR_STATE);
	}
	lssh_buf_reset(&transport->tx);
	ret = lssh_packet_plain_encode(&transport->tx, payload,
	    payload_len, 8);
	if (ret != LSSH_OK) {
		lssh_buf_reset(&transport->tx);
		return (ret);
	}
	ret = lssh_transport_write_all(transport, transport->tx.data,
	    transport->tx.len);
	lssh_buf_reset(&transport->tx);
	if (ret != LSSH_OK) {
		return (ret);
	}
	transport->seq_out++;
	return (LSSH_OK);
}

int
lssh_transport_recv_plain(lssh_transport *transport, lssh_buf *payload)
{
	lssh_slice	slice;
	size_t		consumed;
	int		ret;

	if (!lssh_transport_ready(transport) || !payload) {
		return (LSSH_ERR_INVALID);
	}
	lssh_buf_reset(payload);
	for (;;) {
		ret = lssh_packet_plain_decode(transport->rx.data,
		    transport->rx.len, &slice, &consumed);
		if (ret == LSSH_OK) {
			ret = lssh_buf_append(payload, slice.data, slice.len);
			if (ret != LSSH_OK) {
				return (ret);
			}
			lssh_transport_compact(transport, consumed);
			transport->seq_in++;
			return (LSSH_OK);
		}
		if (ret != LSSH_ERR_AGAIN) {
			return (ret);
		}
		ret = lssh_transport_read_more(transport);
		if (ret != LSSH_OK) {
			return (ret);
		}
	}
}

int
lssh_transport_send_kexinit(lssh_transport *transport,
    const lssh_kexinit_names *names, const uint8_t cookie[16])
{
	lssh_buf	payload;
	int		ret;

	if (!lssh_transport_ready(transport)) {
		return (LSSH_ERR_STATE);
	}
	ret = lssh_buf_init(&payload, 512);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_encode(&payload, names, cookie);
	if (ret == LSSH_OK) {
		ret = lssh_transport_send_packet(transport, payload.data,
		    payload.len);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_transport_recv_kexinit(lssh_transport *transport, lssh_buf *payload,
    lssh_kexinit *out)
{
	int	ret;

	if (!out) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_transport_recv_packet(transport, payload);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_kexinit_parse(payload->data, payload->len, out));
}
