/* !DEFINES!

$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_slice as borrowed byte span
$define %type lssh_chachapoly as OpenSSH ChaCha20-Poly1305 packet keys
$define %type size_t as object size
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %func lssh_packet_seq_nonce as procedure with args uint8_t *, uint32_t
$define %func lssh_packet_chachapoly_poly_key as function with args cipher, seq, key
$define %func lssh_packet_wipe_buf as procedure with args lssh_buf *
$define %func lssh_packet_plain_encode as function with args lssh_buf *, const void *, size_t, size_t
$define %func lssh_packet_plain_decode as function with args const void *, size_t, lssh_slice *, size_t *
$define %func lssh_chachapoly_init as procedure with args lssh_chachapoly *, const uint8_t *
$define %func lssh_chachapoly_free as procedure with args lssh_chachapoly *
$define %func lssh_packet_chachapoly_padding as function with args size_t, size_t *
$define %func lssh_packet_chachapoly_encode as function with args lssh_buf *, const lssh_chachapoly *, uint32_t, const void *, size_t
$define %func lssh_packet_chachapoly_peek_len as function with args const lssh_chachapoly *, uint32_t, const void *, size_t, uint32_t *
$define %func lssh_packet_chachapoly_decode as function with args const void *, size_t, const lssh_chachapoly *, uint32_t, lssh_buf *, size_t *

*/

/* !SPACE!

$space %internal lssh_packet_seq_nonce, lssh_packet_chachapoly_poly_key
$space %internal lssh_packet_wipe_buf
$space %export lssh_packet_plain_encode, lssh_packet_plain_decode
$space %export lssh_chachapoly_init, lssh_chachapoly_free
$space %internal lssh_packet_chachapoly_padding
$space %export lssh_packet_chachapoly_encode
$space %export lssh_packet_chachapoly_peek_len
$space %export lssh_packet_chachapoly_decode

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

#include <libcrypto.h>
#include <libssh.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

static void
lssh_packet_seq_nonce(uint8_t nonce[LC_CHACHA20_NONCE64_SIZE],
    uint32_t seq)
{
	nonce[0] = 0;
	nonce[1] = 0;
	nonce[2] = 0;
	nonce[3] = 0;
	nonce[4] = (uint8_t)(seq >> 24);
	nonce[5] = (uint8_t)(seq >> 16);
	nonce[6] = (uint8_t)(seq >> 8);
	nonce[7] = (uint8_t)seq;
}

static int
lssh_packet_chachapoly_poly_key(const lssh_chachapoly *cipher,
    uint32_t seq, uint8_t poly_key[LC_POLY1305_KEY_SIZE])
{
	uint8_t	block[LC_CHACHA20_BLOCK_SIZE];
	uint8_t	nonce[LC_CHACHA20_NONCE64_SIZE];
	int	ret;

	if (!cipher || !cipher->valid || !poly_key) {
		return (LSSH_ERR_INVALID);
	}
	lssh_packet_seq_nonce(nonce, seq);
	ret = lc_chacha20_block64(cipher->main_key, nonce, 0, block);
	if (ret != 0) {
		lc_wipe(block, sizeof(block));
		lc_wipe(nonce, sizeof(nonce));
		return (LSSH_ERR_CRYPTO);
	}
	memcpy(poly_key, block, LC_POLY1305_KEY_SIZE);
	lc_wipe(block, sizeof(block));
	lc_wipe(nonce, sizeof(nonce));
	return (LSSH_OK);
}

static void
lssh_packet_wipe_buf(lssh_buf *buf)
{
	if (!buf || !buf->data || buf->len == 0) {
		return;
	}
	lc_wipe(buf->data, buf->len);
	lssh_buf_reset(buf);
}

int
lssh_packet_plain_encode(lssh_buf *out, const void *payload,
    size_t payload_len, size_t block_size)
{
	uint8_t	header[5];
	size_t	block, padding_len, packet_len, total_no_pad;
	int	ret;

	if (!out || (!payload && payload_len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	block = block_size < 8 ? 8 : block_size;
	if (block == 0 || block > 255) {
		return (LSSH_ERR_RANGE);
	}
	total_no_pad = payload_len + 5;
	padding_len = block - (total_no_pad % block);
	if (padding_len < LSSH_PACKET_MIN_PAD) {
		padding_len += block;
	}
	packet_len = payload_len + padding_len + 1;
	if (packet_len > LSSH_PACKET_MAX || packet_len > UINT32_MAX ||
	    padding_len > UINT8_MAX) {
		return (LSSH_ERR_RANGE);
	}
	lssh_store_u32(header, (uint32_t)packet_len);
	header[4] = (uint8_t)padding_len;
	ret = lssh_buf_append(out, header, sizeof(header));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_append(out, payload, payload_len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_reserve(out, padding_len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lc_random(out->data + out->len, padding_len) != 0) {
		return (LSSH_ERR_CRYPTO);
	}
	out->len += padding_len;
	return (LSSH_OK);
}

void
lssh_chachapoly_init(lssh_chachapoly *cipher,
    const uint8_t key[LSSH_CHACHAPOLY_KEY_SIZE])
{
	if (!cipher) {
		return;
	}
	memset(cipher, 0, sizeof(*cipher));
	if (!key) {
		return;
	}
	memcpy(cipher->main_key, key, LSSH_CHACHAPOLY_HALF_KEY_SIZE);
	memcpy(cipher->header_key, key + LSSH_CHACHAPOLY_HALF_KEY_SIZE,
	    LSSH_CHACHAPOLY_HALF_KEY_SIZE);
	cipher->valid = 1;
}

void
lssh_chachapoly_free(lssh_chachapoly *cipher)
{
	if (!cipher) {
		return;
	}
	lc_wipe(cipher, sizeof(*cipher));
}

static int
lssh_packet_chachapoly_padding(size_t payload_len, size_t *padding_len)
{
	size_t	block, total_no_pad, pad;

	if (!padding_len) {
		return (LSSH_ERR_INVALID);
	}
	block = 8;
	total_no_pad = payload_len + 1;
	pad = block - (total_no_pad % block);
	if (pad < LSSH_PACKET_MIN_PAD) {
		pad += block;
	}
	if (pad > UINT8_MAX) {
		return (LSSH_ERR_RANGE);
	}
	*padding_len = pad;
	return (LSSH_OK);
}

int
lssh_packet_chachapoly_encode(lssh_buf *out,
    const lssh_chachapoly *cipher, uint32_t seq, const void *payload,
    size_t payload_len)
{
	uint8_t		poly_key[LC_POLY1305_KEY_SIZE];
	uint8_t		nonce[LC_CHACHA20_NONCE64_SIZE];
	uint8_t		*packet, *body, *tag;
	size_t		start_len, padding_len, packet_len;
	int		ret;

	if (!out || !cipher || !cipher->valid ||
	    (!payload && payload_len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_packet_chachapoly_padding(payload_len, &padding_len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	packet_len = payload_len + padding_len + 1;
	if (packet_len > LSSH_PACKET_MAX || packet_len > UINT32_MAX ||
	    padding_len > UINT8_MAX) {
		return (LSSH_ERR_RANGE);
	}
	start_len = out->len;
	ret = lssh_buf_reserve(out, 4 + packet_len +
	    LSSH_CHACHAPOLY_TAG_SIZE);
	if (ret != LSSH_OK) {
		return (ret);
	}
	packet = out->data + out->len;
	lssh_store_u32(packet, (uint32_t)packet_len);
	packet[4] = (uint8_t)padding_len;
	if (payload_len != 0) {
		memcpy(packet + 5, payload, payload_len);
	}
	if (lc_random(packet + 5 + payload_len, padding_len) != 0) {
		lc_wipe(packet, 4 + packet_len);
		return (LSSH_ERR_CRYPTO);
	}
	out->len += 4 + packet_len + LSSH_CHACHAPOLY_TAG_SIZE;
	body = packet + 4;
	tag = packet + 4 + packet_len;
	ret = lssh_packet_chachapoly_poly_key(cipher, seq, poly_key);
	if (ret != LSSH_OK) {
		out->len = start_len;
		return (ret);
	}
	lssh_packet_seq_nonce(nonce, seq);
	if (lc_chacha20_xor64(cipher->header_key, nonce, 0,
	    packet, packet, 4) != 0) {
		ret = LSSH_ERR_CRYPTO;
	} else if (lc_chacha20_xor64(cipher->main_key, nonce, 1,
	    body, body, packet_len) != 0) {
		ret = LSSH_ERR_CRYPTO;
	} else {
		lc_poly1305_auth(tag, packet, 4 + packet_len, poly_key);
		ret = LSSH_OK;
	}
	lc_wipe(poly_key, sizeof(poly_key));
	lc_wipe(nonce, sizeof(nonce));
	if (ret != LSSH_OK) {
		lc_wipe(packet, 4 + packet_len + LSSH_CHACHAPOLY_TAG_SIZE);
		out->len = start_len;
		return (ret);
	}
	return (LSSH_OK);
}

int
lssh_packet_chachapoly_peek_len(const lssh_chachapoly *cipher,
    uint32_t seq, const void *packet, size_t len, uint32_t *packet_len)
{
	uint8_t	nonce[LC_CHACHA20_NONCE64_SIZE];
	uint8_t	plain_len[4];
	int	ret;

	if (!cipher || !cipher->valid || !packet || !packet_len) {
		return (LSSH_ERR_INVALID);
	}
	*packet_len = 0;
	if (len < 4) {
		return (LSSH_ERR_AGAIN);
	}
	lssh_packet_seq_nonce(nonce, seq);
	ret = lc_chacha20_xor64(cipher->header_key, nonce, 0,
	    packet, plain_len, sizeof(plain_len));
	lc_wipe(nonce, sizeof(nonce));
	if (ret != 0) {
		return (LSSH_ERR_CRYPTO);
	}
	*packet_len = lssh_load_u32(plain_len);
	lc_wipe(plain_len, sizeof(plain_len));
	if (*packet_len < 5 || *packet_len > LSSH_PACKET_MAX) {
		lssh_logf(LSSH_LOG_ERROR,
		    "packet: invalid encrypted packet length seq=%u "
		    "packet_len=%u rx_len=%lu",
		    (unsigned int)seq, (unsigned int)*packet_len,
		    (unsigned long)len);
		return (LSSH_ERR_FORMAT);
	}
	if ((*packet_len % 8) != 0) {
		lssh_logf(LSSH_LOG_ERROR,
		    "packet: invalid chachapoly block alignment seq=%u "
		    "packet_len=%u",
		    (unsigned int)seq, (unsigned int)*packet_len);
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_packet_chachapoly_decode(const void *packet, size_t len,
    const lssh_chachapoly *cipher, uint32_t seq, lssh_buf *payload,
    size_t *consumed)
{
	const uint8_t	*p;
	uint8_t		poly_key[LC_POLY1305_KEY_SIZE];
	uint8_t		nonce[LC_CHACHA20_NONCE64_SIZE];
	uint8_t		calc_tag[LSSH_CHACHAPOLY_TAG_SIZE];
	uint32_t	packet_len_u32;
	uint8_t		padding_len;
	size_t		packet_len, payload_len, total_len;
	int		ret;

	if (!packet || !cipher || !cipher->valid || !payload || !consumed) {
		return (LSSH_ERR_INVALID);
	}
	*consumed = 0;
	lssh_buf_reset(payload);
	p = (const uint8_t *)packet;
	ret = lssh_packet_chachapoly_peek_len(cipher, seq, packet, len,
	    &packet_len_u32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	packet_len = (size_t)packet_len_u32;
	total_len = 4 + packet_len + LSSH_CHACHAPOLY_TAG_SIZE;
	if (len < total_len) {
		return (LSSH_ERR_AGAIN);
	}
	ret = lssh_packet_chachapoly_poly_key(cipher, seq, poly_key);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lc_poly1305_auth(calc_tag, p, 4 + packet_len, poly_key);
	lc_wipe(poly_key, sizeof(poly_key));
	if (!lc_memeq(calc_tag, p + 4 + packet_len, sizeof(calc_tag))) {
		lssh_logf(LSSH_LOG_ERROR,
		    "packet: chachapoly tag mismatch seq=%u "
		    "packet_len=%lu rx_len=%lu",
		    (unsigned int)seq, (unsigned long)packet_len,
		    (unsigned long)len);
		lc_wipe(calc_tag, sizeof(calc_tag));
		return (LSSH_ERR_VERIFY);
	}
	lc_wipe(calc_tag, sizeof(calc_tag));
	ret = lssh_buf_append(payload, p + 4, packet_len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lssh_packet_seq_nonce(nonce, seq);
	if (lc_chacha20_xor64(cipher->main_key, nonce, 1,
	    payload->data, payload->data, packet_len) != 0) {
		lc_wipe(nonce, sizeof(nonce));
		lssh_packet_wipe_buf(payload);
		return (LSSH_ERR_CRYPTO);
	}
	lc_wipe(nonce, sizeof(nonce));
	padding_len = payload->data[0];
	if (padding_len < LSSH_PACKET_MIN_PAD ||
	    padding_len + 1 > packet_len) {
		lssh_packet_wipe_buf(payload);
		return (LSSH_ERR_FORMAT);
	}
	payload_len = packet_len - padding_len - 1;
	memmove(payload->data, payload->data + 1, payload_len);
	if (packet_len > payload_len) {
		lc_wipe(payload->data + payload_len,
		    packet_len - payload_len);
	}
	payload->len = payload_len;
	*consumed = total_len;
	if (payload_len != 0) {
		lssh_logf(LSSH_LOG_DEBUG,
		    "packet: decoded chachapoly seq=%u msg=%u(%s) "
		    "packet_len=%lu payload_len=%lu",
		    (unsigned int)seq, (unsigned int)payload->data[0],
		    lssh_log_packet_type_name(payload->data[0]),
		    (unsigned long)packet_len, (unsigned long)payload_len);
	} else {
		lssh_logf(LSSH_LOG_DEBUG,
		    "packet: decoded chachapoly seq=%u empty packet_len=%lu",
		    (unsigned int)seq, (unsigned long)packet_len);
	}
	return (LSSH_OK);
}

int
lssh_packet_plain_decode(const void *packet, size_t len,
    lssh_slice *payload, size_t *consumed)
{
	const uint8_t	*p;
	uint32_t	packet_len;
	uint8_t		padding_len;
	size_t		total_len, payload_len;

	if (!packet || !payload || !consumed) {
		return (LSSH_ERR_INVALID);
	}
	*consumed = 0;
	payload->data = NULL;
	payload->len = 0;
	if (len < 5) {
		return (LSSH_ERR_AGAIN);
	}
	p = (const uint8_t *)packet;
	packet_len = lssh_load_u32(p);
	if (packet_len < 5 || packet_len > LSSH_PACKET_MAX) {
		return (LSSH_ERR_FORMAT);
	}
	total_len = (size_t)packet_len + 4;
	if (len < total_len) {
		return (LSSH_ERR_AGAIN);
	}
	padding_len = p[4];
	if (padding_len < LSSH_PACKET_MIN_PAD ||
	    padding_len + 1 > packet_len) {
		return (LSSH_ERR_FORMAT);
	}
	payload_len = (size_t)packet_len - padding_len - 1;
	payload->data = p + 5;
	payload->len = payload_len;
	*consumed = total_len;
	return (LSSH_OK);
}
