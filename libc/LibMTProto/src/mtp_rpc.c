/* !DEFINES!

$define %type mtp_rpc as encrypted MTProto result and service-message dispatcher
$define %func mtp_pending_add as function with args client, message id, request kind, auxiliary
$define %func mtp_pending_find as function with args client, message id
$define %func mtp_pending_kind as function with args client, request kind
$define %func mtp_pending_active as function with args client, request kind
$define %func mtp_pending_clear as procedure with args client, message id
$define %func mtp_ack_add as procedure with args client, message id
$define %func mtp_flush_acks as function with args client
$define %func mtp_handle_frame as function with args client, frame, frame length
$define %func mtp_fail as function with args client, error, format
$define %func mtp_soft_fail as function with args client, error, format
$define %func mtp_flood_left as function with args client

*/

/* !SPACE!

$space %internal rpc_handle_body, rpc_handle_result, rpc_handle_container
$space %internal rpc_inflate, rpc_handle_result_object
$space %export mtp_pending_add, mtp_pending_find, mtp_pending_kind
$space %export mtp_pending_active, mtp_pending_clear, mtp_ack_add
$space %export mtp_flush_acks, mtp_handle_frame, mtp_fail, mtp_soft_fail
$space %export mtp_flood_left

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


#include <libarchive.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtp_internal.h"

static int
rpc_handle_body(mtp_client_t *c, int64_t msg_id, int32_t seqno,
    const uint8_t *body, size_t body_len);
static uint8_t	*rpc_inflate(mtp_client_t *c, mtp_reader_t *r, size_t *out_len);

int
mtp_fail(mtp_client_t *c, int err, const char *fmt, ...)
{
	va_list	ap;

	if (c == NULL) {
		return (err);
	}
	c->last_error = err;
	if (fmt == NULL) {
		c->error[0] = '\0';
	} else {
		va_start(ap, fmt);
		vsnprintf(c->error, sizeof(c->error), fmt, ap);
		va_end(ap);
	}
	mtp_logf(MTP_LOG_ERROR, "fail: %s (err=%d %s, state=%s, DC%d)",
	    c->error, err, mtpStrerror(err), mtpStateName(c->state),
	    mtp_dc_id(c->dc_index));
	mtp_set_state(c, MTP_STATE_FAILED, "failure");
	mtp_transport_close(c);
	return (err);
}


int
mtp_soft_fail(mtp_client_t *c, int err, const char *fmt, ...)
{
	va_list	ap;

	if (c == NULL) {
		return (err);
	}
	c->last_error = err;
	if (fmt == NULL) {
		c->error[0] = '\0';
	} else {
		va_start(ap, fmt);
		vsnprintf(c->error, sizeof(c->error), fmt, ap);
		va_end(ap);
	}
	c->soft_error = err;
	mtp_logf(MTP_LOG_ERROR, "reject: %s (err=%d %s, state=%s, DC%d, session "
	    "kept)", c->error, err, mtpStrerror(err), mtpStateName(c->state),
	    mtp_dc_id(c->dc_index));
	return (err);
}

uint64_t
mtp_flood_left(const mtp_client_t *c)
{
	uint64_t	now;

	if (c == NULL || c->flood_until == 0) {
		return (0);
	}
	now = mtp_now_ms();
	return (now >= c->flood_until ? 0 : c->flood_until - now);
}

int
mtp_pending_add(mtp_client_t *c, int64_t msg_id, uint32_t kind, int64_t aux)
{
	int	i;

	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (!c->pending[i].in_use) {
			c->pending[i].msg_id = msg_id;
			c->pending[i].kind = kind;
			c->pending[i].aux = aux;
			c->pending[i].deadline = mtp_now_ms() + MTP_RPC_TIMEOUT_MS;
			c->pending[i].in_use = 1;
			mtp_logf(MTP_LOG_DEBUG, "pending[%d]: %s msg_id=%lld "
			    "aux=%lld", i, mtp_log_req_name(kind),
			    (long long)msg_id, (long long)aux);
			return (MTP_OK);
		}
	}
	return (mtp_fail(c, MTP_ERR_BUSY, "all %d RPC slots are outstanding",
	    MTP_MAX_PENDING));
}

mtp_pending_t *
mtp_pending_kind(mtp_client_t *c, uint32_t kind)
{
	int	i;

	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (c->pending[i].in_use && c->pending[i].kind == kind) {
			return (&c->pending[i]);
		}
	}
	return (NULL);
}


int
mtp_pending_active(const mtp_client_t *c, uint32_t kind)
{
	int	i;

	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (c->pending[i].in_use && c->pending[i].kind == kind) {
			return (1);
		}
	}
	return (0);
}

mtp_pending_t *
mtp_pending_find(mtp_client_t *c, int64_t msg_id)
{
	int	i;

	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (c->pending[i].in_use && c->pending[i].msg_id == msg_id) {
			return (&c->pending[i]);
		}
	}
	return (NULL);
}

void
mtp_pending_clear(mtp_client_t *c, int64_t msg_id)
{
	mtp_pending_t	*p;

	p = mtp_pending_find(c, msg_id);
	if (p != NULL) {
		memset(p, 0, sizeof(*p));
	}
}

void
mtp_ack_add(mtp_client_t *c, int64_t msg_id)
{
	int	i;

	for (i = 0; i < c->ack_count; i++) {
		if (c->ack[i] == msg_id) {
			return;
		}
	}
	if (c->ack_count == MTP_MAX_ACK) {
		return;
	}
	c->ack[c->ack_count++] = msg_id;
}

int
mtp_flush_acks(mtp_client_t *c)
{
	mtp_writer_t	w;
	uint8_t		body[4 + 4 + MTP_MAX_ACK * 8];
	int64_t		msg_id;
	int			i;

	if (c->ack_count == 0 || !c->auth_key_valid) {
		return (MTP_OK);
	}
	mtp_logf(MTP_LOG_DEBUG, "ack: flushing %d message id(s)", c->ack_count);
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_ID_msgs_ack);
	mtp_write_u32(&w, MTP_ID_vector);
	mtp_write_i32(&w, c->ack_count);
	for (i = 0; i < c->ack_count; i++) {
		mtp_write_i64(&w, c->ack[i]);
	}
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_PROTO, "msgs_ack for %d ids exceeds "
		    "its buffer", c->ack_count));
	}
	if (mtp_send_encrypted(c, body, w.len, 0, &msg_id) != MTP_OK) {
		return (c->last_error);
	}
	c->ack_count = 0;
	return (MTP_OK);
}


static uint8_t *
rpc_inflate(mtp_client_t *c, mtp_reader_t *r, size_t *out_len)
{
	uint8_t		*unpacked;
	const uint8_t	*packed;
	size_t		packed_len;
	int		ret;

	packed = mtp_read_bytes(r, &packed_len);
	if (packed == NULL || r->error) {
		(void)mtp_fail(c, MTP_ERR_PROTO, "gzip_packed carries no readable "
		    "byte string");
		return (NULL);
	}
	unpacked = (uint8_t *)malloc(MTP_MAX_UNPACK);
	if (unpacked == NULL) {
		(void)mtp_fail(c, MTP_ERR_NOMEM, "out of memory for gzip result");
		return (NULL);
	}
	ret = la_gzip_inflate(packed, packed_len, unpacked, MTP_MAX_UNPACK,
	    out_len);

	if (ret != 0 || *out_len == 0 || (*out_len % 4) != 0) {
		(void)mtp_fail(c, MTP_ERR_PROTO, "gzip_packed inflate failed "
		    "(ret=%d in=%u out=%u)", ret, (unsigned int)packed_len,
		    (unsigned int)*out_len);
		free(unpacked);
		return (NULL);
	}
	mtp_logf(MTP_LOG_DEBUG, "gzip_packed: %u -> %u bytes",
	    (unsigned int)packed_len, (unsigned int)*out_len);
	return (unpacked);
}


static int
rpc_handle_result_object(mtp_client_t *c, mtp_pending_t *p, int64_t request_id,
    mtp_reader_t *r)
{
	mtp_object_t	o;
	int32_t		code;
	size_t		result_pos, avail;
	char		message[MTP_MAX_ERROR];

	result_pos = r->pos;
	avail = r->len > result_pos ? r->len - result_pos : 0;
	if (mtp_object_parse(r, &o) != 0) {
		char	why[MTP_MAX_ERROR];
		uint32_t	head;

		(void)mtp_reader_explain(r, why, sizeof(why));

		head = 0;
		if (avail >= 4) {
			head = (uint32_t)r->buf[result_pos] |
			    ((uint32_t)r->buf[result_pos + 1] << 8) |
			    ((uint32_t)r->buf[result_pos + 2] << 16) |
			    ((uint32_t)r->buf[result_pos + 3] << 24);
		}
		mtp_logf(MTP_LOG_ERROR, "unparsed result: id=0x%08x, %u bytes from "
		    "offset %u", (unsigned int)head, (unsigned int)avail,
		    (unsigned int)result_pos);
		mtp_log_hex(MTP_LOG_ERROR, "unparsed result", r->buf + result_pos,
		    avail < 64 ? avail : 64);
		return (mtp_fail(c, MTP_ERR_PROTO, "%s reply does not parse: %s",
		    mtp_log_req_name(p->kind), why));
	}
	mtp_logf(MTP_LOG_DEBUG, "rpc_result: %s -> %s", mtp_log_req_name(p->kind),
	    o.ctor->name);
	if (o.ctor->id == MTP_ID_rpc_error) {
		code = mtp_object_i32(&o, "error_code", 0);
		(void)mtp_object_str(&o, "error_message", message, sizeof(message));
		code = mtp_dispatch_error(c, p, code, message);
		mtp_pending_clear(c, request_id);
		return (c->state == MTP_STATE_FAILED ? code : MTP_OK);
	}
	r->pos = result_pos;
	if (mtp_dispatch_result(c, p, r) != MTP_OK) {
		return (MTP_ERR_RPC);
	}
	mtp_pending_clear(c, request_id);
	return (MTP_OK);
}

static int
rpc_handle_result(mtp_client_t *c, mtp_reader_t *r)
{
	mtp_reader_t	inner;
	mtp_pending_t	*p;
	uint8_t		*unpacked;
	int64_t		request_id;
	size_t		unpacked_len;

	request_id = mtp_read_i64(r);
	if (r->error) {
		return (mtp_fail(c, MTP_ERR_PROTO, "truncated rpc_result"));
	}
	p = mtp_pending_find(c, request_id);
	if (p == NULL) {
		mtp_logf(MTP_LOG_DEBUG, "rpc_result: no pending slot for "
		    "msg_id=%lld, ignored as a duplicate",
		    (long long)request_id);
		return (MTP_OK);
	}
	if (mtp_reader_peek_u32(r) == MTP_ID_gzip_packed) {
		int	ret;

		(void)mtp_read_u32(r);
		unpacked = rpc_inflate(c, r, &unpacked_len);
		if (unpacked == NULL) {
			return (c->last_error);
		}
		mtp_reader_init(&inner, unpacked, unpacked_len);
		ret = rpc_handle_result_object(c, p, request_id, &inner);
		free(unpacked);
		return (ret);
	}
	return (rpc_handle_result_object(c, p, request_id, r));
}

static int
rpc_handle_container(mtp_client_t *c, mtp_reader_t *r)
{
	const uint8_t	*body;
	uint32_t	count, i;
	int64_t		msg_id;
	int32_t		seqno, bytes;

	count = mtp_read_u32(r);
	if (r->error || count > 1024) {
		return (mtp_fail(c, MTP_ERR_PROTO,
		    "message container claims %u members", (unsigned int)count));
	}
	mtp_logf(MTP_LOG_DEBUG, "container: %u member(s)", (unsigned int)count);
	for (i = 0; i < count; i++) {
		msg_id = mtp_read_i64(r);
		seqno = mtp_read_i32(r);
		bytes = mtp_read_i32(r);
		if (r->error || bytes <= 0 || (bytes % 4) != 0 ||
		    (size_t)bytes > r->len - r->pos || (size_t)bytes > MTP_MAX_FRAME) {
			return (mtp_fail(c, MTP_ERR_PROTO, "container member %u of "
			    "%u declares %d bytes with %u left in the frame",
			    (unsigned int)i + 1, (unsigned int)count, (int)bytes,
			    (unsigned int)(r->len - r->pos)));
		}
		body = mtp_read_raw(r, (size_t)bytes);
		if (rpc_handle_body(c, msg_id, seqno, body, (size_t)bytes) != MTP_OK) {
			return (c->last_error);
		}
	}
	return (MTP_OK);
}

static int
rpc_handle_body(mtp_client_t *c, int64_t msg_id, int32_t seqno,
    const uint8_t *body, size_t body_len)
{
	mtp_reader_t	r;
	mtp_object_t	o;
	uint8_t		*unpacked;
	mtp_pending_t	*p;
	uint32_t	id;
	size_t		unpacked_len;
	int		ret;

	if (body_len < 4 || (body_len % 4) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "message body is %u bytes, "
		    "need at least 4 and a multiple of 4",
		    (unsigned int)body_len));
	}
	mtp_ack_add(c, msg_id);
	mtp_reader_init(&r, body, body_len);
	id = mtp_read_u32(&r);
	if (r.error) {
		return (mtp_fail(c, MTP_ERR_PROTO, "message body of %u bytes has "
		    "no constructor id", (unsigned int)body_len));
	}
	mtp_logf(MTP_LOG_DEBUG, "body: %s (id=0x%08x) msg_id=%lld seqno=%d "
	    "len=%u", mtp_log_ctor_name(id), (unsigned int)id,
	    (long long)msg_id, seqno, (unsigned int)body_len);
	if (id == MTP_ID_msg_container) {
		return (rpc_handle_container(c, &r));
	}
	if (id == MTP_ID_rpc_result) {
		return (rpc_handle_result(c, &r));
	}
	if (id == MTP_ID_gzip_packed) {
		unpacked = rpc_inflate(c, &r, &unpacked_len);
		if (unpacked == NULL) {
			return (c->last_error);
		}
		ret = rpc_handle_body(c, msg_id, seqno, unpacked, unpacked_len);
		free(unpacked);
		return (ret);
	}

	r.pos = 0;
	if (mtp_object_parse(&r, &o) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO,
		    "service message 0x%08x is not in the compiled schema",
		    (unsigned int)id));
	}
	if (mtp_updates_is_container(o.ctor->id) ||
	    o.ctor->id == MTP_ID_updateNewMessage ||
	    o.ctor->id == MTP_ID_updateNewChannelMessage ||
	    o.ctor->id == MTP_ID_updateUserStatus) {
		return (mtp_dispatch_update(c, &o));
	}
	if (o.ctor->id == MTP_ID_bad_server_salt) {
		int64_t	bad_msg_id;

		bad_msg_id = mtp_object_i64(&o, "bad_msg_id", 0);
		c->server_salt = mtp_object_i64(&o, "new_server_salt", 0);
		mtp_logf(MTP_LOG_INFO, "bad_server_salt: adopted %016llx, resending "
		    "the rejected msg_id=%lld",
		    (unsigned long long)c->server_salt, (long long)bad_msg_id);
		return (mtp_resend_last(c, bad_msg_id));
	}

	if (o.ctor->id == MTP_ID_pong) {
		int64_t	ping_msg_id, ping_id;

		ping_msg_id = mtp_object_i64(&o, "msg_id", 0);
		ping_id = mtp_object_i64(&o, "ping_id", 0);
		p = ping_msg_id != 0 ? mtp_pending_find(c, ping_msg_id) : NULL;

		if (p == NULL) {
			mtp_logf(MTP_LOG_DEBUG, "pong: ping_id=%lld answers "
			    "msg_id=%lld, which is not pending",
			    (long long)ping_id, (long long)ping_msg_id);
			return (MTP_OK);
		}
		mtp_logf(MTP_LOG_DEBUG, "pong: keepalive acknowledged, "
		    "ping_id=%lld%s", (long long)ping_id,
		    ping_id == c->ping_id ? "" : " (an earlier ping)");
		mtp_pending_clear(c, ping_msg_id);
		return (MTP_OK);
	}
	if (o.ctor->id == MTP_ID_new_session_created) {
		c->server_salt = mtp_object_i64(&o, "server_salt", c->server_salt);
		mtp_logf(MTP_LOG_INFO, "new_session_created: salt adopted");
		return (MTP_OK);
	}
	if (o.ctor->id == MTP_ID_bad_msg_notification) {

		return (mtp_fail(c, MTP_ERR_PROTO,
		    "bad_msg_notification %d for msg_id=%lld seqno=%d "
		    "(16/17=clock skew, 32-35=seqno)",
		    mtp_object_i32(&o, "error_code", 0),
		    (long long)mtp_object_i64(&o, "bad_msg_id", 0),
		    mtp_object_i32(&o, "bad_msg_seqno", 0)));
	}
	return (MTP_OK);
}

int
mtp_handle_frame(mtp_client_t *c, const uint8_t *frame, size_t frame_len)
{
	uint8_t		*body;
	int64_t		msg_id;
	int32_t		seqno;
	size_t		body_len;
	int		ret;

	body = (uint8_t *)malloc(MTP_MAX_FRAME);
	if (body == NULL) {
		return (mtp_fail(c, MTP_ERR_NOMEM, "out of memory for message frame"));
	}
	ret = mtp_decrypt_frame(c, frame, frame_len, body, MTP_MAX_FRAME,
	    &body_len, &msg_id, &seqno);
	if (ret == MTP_OK) {
		ret = rpc_handle_body(c, msg_id, seqno, body, body_len);
	}
	free(body);
	return (ret);
}
