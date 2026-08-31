/* !DEFINES!

$define %type mtp_session as MTProto 2.0 encrypted message session
$define %func mtp_next_msg_id as function with args client
$define %func mtp_next_seqno as function with args client, content related
$define %func mtp_send_plain as function with args client, body, length
$define %func mtp_recv_plain as function with args client, frame, frame length, out
$define %func mtp_send_encrypted as function with args client, body, length, content related, out message id
$define %func mtp_decrypt_frame as function with args client, frame, frame length, out, out capacity, out length, out message id, out sequence number
$define %func mtp_derive_auth_key_id as procedure with args client
$define %func mtp_session_reset as procedure with args client

*/

/* !SPACE!

$space %internal session_put_i32, session_put_i64, session_get_i32, session_get_i64
$space %internal session_msg_key, session_aes, session_random_padding
$space %export mtp_next_msg_id, mtp_next_seqno, mtp_send_plain, mtp_recv_plain
$space %export mtp_send_encrypted, mtp_decrypt_frame, mtp_derive_auth_key_id
$space %export mtp_session_reset

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
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



#include <stdlib.h>
#include <string.h>

#include "mtp_internal.h"

#define SESSION_PLAIN_HEAD	20

#define SESSION_CRYPT_HEAD	24
#define SESSION_DATA_HEAD	32
#define SESSION_MIN_PADDING	12
#define SESSION_MAX_PADDING	1024

static void
session_put_i32(uint8_t *p, int32_t v)
{
	uint32_t	u;

	u = (uint32_t)v;
	p[0] = (uint8_t)(u & 0xFFu);
	p[1] = (uint8_t)((u >> 8) & 0xFFu);
	p[2] = (uint8_t)((u >> 16) & 0xFFu);
	p[3] = (uint8_t)((u >> 24) & 0xFFu);
}

static void
session_put_i64(uint8_t *p, int64_t v)
{
	uint64_t	u;

	u = (uint64_t)v;
	session_put_i32(p, (int32_t)(u & 0xFFFFFFFFu));
	session_put_i32(p + 4, (int32_t)(u >> 32));
}

static int32_t
session_get_i32(const uint8_t *p)
{
	return ((int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)));
}

static int64_t
session_get_i64(const uint8_t *p)
{
	uint64_t	lo, hi;

	lo = (uint32_t)session_get_i32(p);
	hi = (uint32_t)session_get_i32(p + 4);
	return ((int64_t)(lo | (hi << 32)));
}

static void
session_msg_key(const mtp_client_t *c, int x, const uint8_t *plain,
    size_t plain_len, uint8_t out[MTP_MSG_KEY_SIZE])
{
	lc_sha256_ctx	ctx;
	uint8_t		digest[LC_SHA256_DIGEST_SIZE];

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, c->auth_key + 88 + x, 32);
	lc_sha256_update(&ctx, plain, plain_len);
	lc_sha256_final(&ctx, digest);
	memcpy(out, digest + 8, MTP_MSG_KEY_SIZE);
	lc_wipe(digest, sizeof(digest));
	lc_wipe(&ctx, sizeof(ctx));
}

static void
session_aes(const mtp_client_t *c, int x,
    const uint8_t msg_key[MTP_MSG_KEY_SIZE], uint8_t key[32], uint8_t iv[32])
{
	lc_sha256_ctx	ctx;
	uint8_t		a[LC_SHA256_DIGEST_SIZE];
	uint8_t		b[LC_SHA256_DIGEST_SIZE];

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, msg_key, MTP_MSG_KEY_SIZE);
	lc_sha256_update(&ctx, c->auth_key + x, 36);
	lc_sha256_final(&ctx, a);

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, c->auth_key + 40 + x, 36);
	lc_sha256_update(&ctx, msg_key, MTP_MSG_KEY_SIZE);
	lc_sha256_final(&ctx, b);
	memcpy(key, a, 8);
	memcpy(key + 8, b + 8, 16);
	memcpy(key + 24, a + 24, 8);

	memcpy(iv, b, 8);
	memcpy(iv + 8, a + 8, 16);
	memcpy(iv + 24, b + 24, 8);

	lc_wipe(a, sizeof(a));
	lc_wipe(b, sizeof(b));
	lc_wipe(&ctx, sizeof(ctx));
}

static int
session_random_padding(uint8_t *out, size_t len)
{
	if (len == 0) {
		return (0);
	}
	return (lc_random(out, len) == 0 ? 0 : -1);
}

int64_t
mtp_next_msg_id(mtp_client_t *c)
{
	int64_t		id;
	int64_t		sec;
	uint32_t	frac;
	uint32_t	nsec;

	nsec = 0;
	sec = mtp_unix_time_ns(c, &nsec);
	frac = (uint32_t)(((uint64_t)nsec << 30) / 1000000000ull);
	id = (sec << 32) | ((int64_t)(frac & 0x3FFFFFFFu) << 2);
	if (id <= c->last_msg_id) {
		id = c->last_msg_id + 4;
	}
	c->last_msg_id = id;
	return (id);
}

int32_t
mtp_next_seqno(mtp_client_t *c, int content_related)
{
	int32_t	seqno;

	seqno = c->seq_no * 2;
	if (content_related) {
		c->seq_no++;
		seqno++;
	}
	return (seqno);
}

int
mtp_send_plain(mtp_client_t *c, const void *body, size_t len)
{
	uint8_t	packet[MTP_MAX_REQUEST];
	int64_t	msg_id;

	if (body == NULL || len == 0 || len > sizeof(packet) - SESSION_PLAIN_HEAD ||
	    (len % 4) != 0) {
		return (mtp_fail(c, MTP_ERR_INVAL, "unencrypted body is %u bytes, "
		    "must be a multiple of 4 up to %u", (unsigned int)len,
		    (unsigned int)(sizeof(packet) - SESSION_PLAIN_HEAD)));
	}
	msg_id = mtp_next_msg_id(c);
	memset(packet, 0, 8);
	session_put_i64(packet + 8, msg_id);
	session_put_i32(packet + 16, (int32_t)len);
	memcpy(packet + SESSION_PLAIN_HEAD, body, len);
	return (mtp_transport_queue(c, packet, SESSION_PLAIN_HEAD + len));
}

int
mtp_recv_plain(mtp_client_t *c, const uint8_t *frame, size_t frame_len,
    mtp_reader_t *out)
{
	int32_t	len;

	if (frame == NULL || out == NULL || frame_len < SESSION_PLAIN_HEAD) {
		return (mtp_fail(c, MTP_ERR_PROTO, "unencrypted frame is %u "
		    "bytes, minimum %u", (unsigned int)frame_len,
		    (unsigned int)SESSION_PLAIN_HEAD));
	}
	if (session_get_i64(frame) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "handshake reply carries "
		    "auth_key_id %016llx, expected 0",
		    (unsigned long long)session_get_i64(frame)));
	}
	len = session_get_i32(frame + 16);
	if (len <= 0 || (len % 4) != 0 || (size_t)len != frame_len -
	    SESSION_PLAIN_HEAD) {
		return (mtp_fail(c, MTP_ERR_PROTO, "handshake reply declares %d "
		    "body bytes, frame carries %u", (int)len,
		    (unsigned int)(frame_len - SESSION_PLAIN_HEAD)));
	}
	mtp_reader_init(out, frame + SESSION_PLAIN_HEAD, (size_t)len);
	return (MTP_OK);
}

int
mtp_send_encrypted(mtp_client_t *c, const void *body, size_t len,
    int content_related, int64_t *out_msg_id)
{
	lc_aes_ctx	aes;
	uint8_t		*plain;
	uint8_t		*packet;
	uint8_t		key[32], iv[32], msg_key[MTP_MSG_KEY_SIZE];
	int64_t		msg_id;
	int32_t		seqno;
	size_t		plain_len, padding, packet_len;
	int		ret;

	if (!c->auth_key_valid) {
		return (mtp_fail(c, MTP_ERR_AUTH,
		    "cannot send an encrypted message without an auth_key"));
	}
	if (body == NULL || len == 0 || (len % 4) != 0) {
		return (mtp_fail(c, MTP_ERR_INVAL, "encrypted body is %u bytes, "
		    "must be a positive multiple of 4", (unsigned int)len));
	}
	if (len > MTP_MAX_REQUEST - SESSION_CRYPT_HEAD - SESSION_DATA_HEAD) {
		return (mtp_fail(c, MTP_ERR_INVAL, "encrypted body is %u bytes, "
		    "maximum %u", (unsigned int)len,
		    (unsigned int)(MTP_MAX_REQUEST - SESSION_CRYPT_HEAD -
		    SESSION_DATA_HEAD)));
	}

	padding = SESSION_MIN_PADDING;
	while ((SESSION_DATA_HEAD + len + padding) % LC_AES_BLOCK_SIZE != 0) {
		padding++;
	}
	if (padding > SESSION_MAX_PADDING) {
		return (mtp_fail(c, MTP_ERR_PROTO, "cannot align a %u-byte body "
		    "within %u..%u padding bytes", (unsigned int)len,
		    (unsigned int)SESSION_MIN_PADDING,
		    (unsigned int)SESSION_MAX_PADDING));
	}
	plain_len = SESSION_DATA_HEAD + len + padding;
	packet_len = SESSION_CRYPT_HEAD + plain_len;
	plain = (uint8_t *)malloc(plain_len);
	packet = (uint8_t *)malloc(packet_len);
	if (plain == NULL || packet == NULL) {
		free(plain);
		free(packet);
		return (mtp_fail(c, MTP_ERR_NOMEM, "out of memory for a %u-byte "
		    "encrypted packet", (unsigned int)packet_len));
	}

	msg_id = mtp_next_msg_id(c);
	seqno = mtp_next_seqno(c, content_related);
	session_put_i64(plain, c->server_salt);
	session_put_i64(plain + 8, c->session_id);
	session_put_i64(plain + 16, msg_id);
	session_put_i32(plain + 24, seqno);
	session_put_i32(plain + 28, (int32_t)len);
	memcpy(plain + SESSION_DATA_HEAD, body, len);
	if (session_random_padding(plain + SESSION_DATA_HEAD + len, padding) != 0) {
		lc_wipe(plain, plain_len);
		free(plain);
		free(packet);
		return (mtp_fail(c, MTP_ERR_CRYPTO, "no entropy for %u padding "
		    "bytes", (unsigned int)padding));
	}

	session_msg_key(c, 0, plain, plain_len, msg_key);
	session_aes(c, 0, msg_key, key, iv);
	if (lc_aes_init(&aes, key, sizeof(key)) != 0 ||
	    lc_aes_ige_encrypt(&aes, iv, plain, packet + SESSION_CRYPT_HEAD,
	    plain_len) != 0) {
		lc_aes_wipe(&aes);
		lc_wipe(plain, plain_len);
		lc_wipe(packet, packet_len);
		free(plain);
		free(packet);
		return (mtp_fail(c, MTP_ERR_CRYPTO, "AES-IGE encryption failed"));
	}
	session_put_i64(packet, c->auth_key_id);
	memcpy(packet + 8, msg_key, sizeof(msg_key));
	mtp_logf(MTP_LOG_DEBUG, "tx: %s msg_id=%016llx seqno=%d body=%u pad=%u",
	    mtp_log_ctor_name((uint32_t)session_get_i32(
	    (const uint8_t *)body)), (unsigned long long)msg_id, (int)seqno,
	    (unsigned int)len, (unsigned int)padding);
	ret = mtp_transport_queue(c, packet, packet_len);
	lc_aes_wipe(&aes);
	lc_wipe(key, sizeof(key));
	lc_wipe(iv, sizeof(iv));
	lc_wipe(msg_key, sizeof(msg_key));
	lc_wipe(plain, plain_len);
	lc_wipe(packet, packet_len);
	free(plain);
	free(packet);
	if (ret != MTP_OK) {
		return (ret);
	}
	if (out_msg_id != NULL) {
		*out_msg_id = msg_id;
	}
	return (MTP_OK);
}

int
mtp_decrypt_frame(mtp_client_t *c, const uint8_t *frame, size_t frame_len,
    uint8_t *out, size_t out_cap, size_t *out_len, int64_t *out_msg_id,
    int32_t *out_seqno)
{
	lc_aes_ctx	aes;
	uint8_t		*plain;
	uint8_t		key[32], iv[32], calculated[MTP_MSG_KEY_SIZE];
	int32_t		body_len;
	size_t		crypt_len, padding;
	int		ret;

	if (out_len != NULL) {
		*out_len = 0;
	}
	if (frame == NULL || out == NULL || out_len == NULL || out_msg_id == NULL ||
	    out_seqno == NULL) {
		return (mtp_fail(c, MTP_ERR_INVAL,
		    "mtp_decrypt_frame called with a null argument"));
	}
	if (!c->auth_key_valid) {
		return (mtp_fail(c, MTP_ERR_AUTH,
		    "encrypted frame arrived before an auth_key exists"));
	}
	if (frame_len < SESSION_CRYPT_HEAD + SESSION_DATA_HEAD +
	    SESSION_MIN_PADDING) {
		return (mtp_fail(c, MTP_ERR_PROTO, "encrypted frame is %u bytes, "
		    "minimum %u", (unsigned int)frame_len,
		    (unsigned int)(SESSION_CRYPT_HEAD + SESSION_DATA_HEAD +
		    SESSION_MIN_PADDING)));
	}
	if ((frame_len - SESSION_CRYPT_HEAD) % LC_AES_BLOCK_SIZE != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "encrypted payload is %u "
		    "bytes, not a multiple of %u",
		    (unsigned int)(frame_len - SESSION_CRYPT_HEAD),
		    (unsigned int)LC_AES_BLOCK_SIZE));
	}
	if (session_get_i64(frame) != c->auth_key_id) {
		return (mtp_fail(c, MTP_ERR_AUTH, "frame carries auth_key_id "
		    "%016llx, this client holds %016llx (delete %s to re-key)",
		    (unsigned long long)session_get_i64(frame),
		    (unsigned long long)c->auth_key_id, c->auth_path));
	}

	crypt_len = frame_len - SESSION_CRYPT_HEAD;
	plain = (uint8_t *)malloc(crypt_len);
	if (plain == NULL) {
		return (mtp_fail(c, MTP_ERR_NOMEM,
		    "out of memory for a %u-byte decrypted packet",
		    (unsigned int)crypt_len));
	}
	session_aes(c, 8, frame + 8, key, iv);
	if (lc_aes_init(&aes, key, sizeof(key)) != 0 ||
	    lc_aes_ige_decrypt(&aes, iv, frame + SESSION_CRYPT_HEAD, plain,
	    crypt_len) != 0) {
		ret = mtp_fail(c, MTP_ERR_CRYPTO,
		    "AES-IGE decryption of a %u-byte payload failed",
		    (unsigned int)crypt_len);
		goto done;
	}
	session_msg_key(c, 8, plain, crypt_len, calculated);
	if (!lc_memeq(calculated, frame + 8, sizeof(calculated))) {
		ret = mtp_fail(c, MTP_ERR_CRYPTO, "msg_key mismatch: auth_key_id "
		    "matches but the key bytes differ (stored key is corrupt)");
		goto done;
	}
	if (session_get_i64(plain + 8) != c->session_id) {
		ret = mtp_fail(c, MTP_ERR_PROTO, "packet is for session %016llx, "
		    "this one is %016llx",
		    (unsigned long long)session_get_i64(plain + 8),
		    (unsigned long long)c->session_id);
		goto done;
	}
	body_len = session_get_i32(plain + 28);
	padding = crypt_len - SESSION_DATA_HEAD;
	if (body_len <= 0 || (body_len % 4) != 0 || (size_t)body_len > padding ||
	    padding - (size_t)body_len < SESSION_MIN_PADDING ||
	    padding - (size_t)body_len > SESSION_MAX_PADDING ||
	    (size_t)body_len > out_cap) {
		ret = mtp_fail(c, MTP_ERR_PROTO, "declared body %d bytes with %u "
		    "available, padding %u..%u, capacity %u", (int)body_len,
		    (unsigned int)padding, (unsigned int)SESSION_MIN_PADDING,
		    (unsigned int)SESSION_MAX_PADDING, (unsigned int)out_cap);
		goto done;
	}
	mtp_logf(MTP_LOG_TRACE, "rx: decrypted %d-byte body, msg_id=%016llx "
	    "seqno=%d", (int)body_len,
	    (unsigned long long)session_get_i64(plain + 16),
	    (int)session_get_i32(plain + 24));
	memcpy(out, plain + SESSION_DATA_HEAD, (size_t)body_len);
	*out_len = (size_t)body_len;
	*out_msg_id = session_get_i64(plain + 16);
	*out_seqno = session_get_i32(plain + 24);
	ret = MTP_OK;

done:
	lc_aes_wipe(&aes);
	lc_wipe(key, sizeof(key));
	lc_wipe(iv, sizeof(iv));
	lc_wipe(calculated, sizeof(calculated));
	lc_wipe(plain, crypt_len);
	free(plain);
	return (ret);
}

void
mtp_derive_auth_key_id(mtp_client_t *c)
{
	uint8_t	digest[LC_SHA1_DIGEST_SIZE];

	lc_sha1(c->auth_key, sizeof(c->auth_key), digest);
	c->auth_key_id = session_get_i64(digest + 12);
	lc_wipe(digest, sizeof(digest));
}

void
mtp_session_reset(mtp_client_t *c)
{
	c->server_salt = 0;
	c->seq_no = 0;
	c->last_msg_id = 0;
	c->init_done = 0;
	c->ack_count = 0;
	c->next_ping = 0;
	c->ping_id = 0;
	memset(c->pending, 0, sizeof(c->pending));
	c->last_req_len = 0;
	c->last_req_msg_id = 0;
	c->last_req_resent = 0;
	if (lc_random(&c->session_id, sizeof(c->session_id)) != 0) {
		c->session_id = 0;
		mtp_logf(MTP_LOG_ERROR, "session: no entropy for a session id; "
		    "every request will be rejected until this is fixed");
		return;
	}
	mtp_logf(MTP_LOG_INFO, "session: reset, id=%016llx",
	    (unsigned long long)c->session_id);
}
