/* !DEFINES!

$define %type mtp_writer as bounded TL serialisation cursor
$define %func mtp_writer_init as procedure with args writer, buffer, capacity
$define %func mtp_write_u32 as procedure with args writer, uint32_t
$define %func mtp_write_i32 as procedure with args writer, int32_t
$define %func mtp_write_i64 as procedure with args writer, int64_t
$define %func mtp_write_double as procedure with args writer, double
$define %func mtp_write_raw as procedure with args writer, data, length
$define %func mtp_write_bytes as procedure with args writer, data, length
$define %func mtp_write_string as procedure with args writer, const char *
$define %func mtp_write_bool as procedure with args writer, int
$define %func mtp_write_peer as procedure with args writer, peer
$define %func mtp_write_pad_to as procedure with args writer, multiple

*/

/* !SPACE!

$space %internal writer_room
$space %export mtp_writer_init, mtp_write_u32, mtp_write_i32, mtp_write_i64
$space %export mtp_write_double, mtp_write_raw, mtp_write_bytes
$space %export mtp_write_string, mtp_write_bool, mtp_write_peer
$space %export mtp_write_pad_to

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



#include <string.h>

#include "mtp_internal.h"

static int
writer_room(mtp_writer_t *w, size_t need)
{
	if (w->overflow) {
		return (0);
	}
	if (need > w->cap - w->len) {
		w->overflow = 1;
		return (0);
	}
	return (1);
}

void
mtp_writer_init(mtp_writer_t *w, void *buf, size_t cap)
{
	w->buf = (uint8_t *)buf;
	w->cap = cap;
	w->len = 0;
	w->overflow = 0;
}

void
mtp_write_u32(mtp_writer_t *w, uint32_t v)
{
	if (!writer_room(w, 4)) {
		return;
	}
	w->buf[w->len + 0] = (uint8_t)(v & 0xFFu);
	w->buf[w->len + 1] = (uint8_t)((v >> 8) & 0xFFu);
	w->buf[w->len + 2] = (uint8_t)((v >> 16) & 0xFFu);
	w->buf[w->len + 3] = (uint8_t)((v >> 24) & 0xFFu);
	w->len += 4;
}

void
mtp_write_i32(mtp_writer_t *w, int32_t v)
{
	mtp_write_u32(w, (uint32_t)v);
}

void
mtp_write_i64(mtp_writer_t *w, int64_t v)
{
	uint64_t	u;

	u = (uint64_t)v;
	mtp_write_u32(w, (uint32_t)(u & 0xFFFFFFFFu));
	mtp_write_u32(w, (uint32_t)(u >> 32));
}

void
mtp_write_double(mtp_writer_t *w, double v)
{
	uint64_t	bits;

	memcpy(&bits, &v, sizeof(bits));
	mtp_write_u32(w, (uint32_t)(bits & 0xFFFFFFFFu));
	mtp_write_u32(w, (uint32_t)(bits >> 32));
}

void
mtp_write_raw(mtp_writer_t *w, const void *data, size_t len)
{
	if (len == 0) {
		return;
	}
	if (!writer_room(w, len)) {
		return;
	}
	memcpy(w->buf + w->len, data, len);
	w->len += len;
}


void
mtp_write_bytes(mtp_writer_t *w, const void *data, size_t len)
{
	size_t	total, pad;
	uint8_t	head[4];

	if (len > 0xFFFFFFu) {
		w->overflow = 1;
		return;
	}

	if (len < 254) {
		head[0] = (uint8_t)len;
		total = 1 + len;
		if (!writer_room(w, total)) {
			return;
		}
		mtp_write_raw(w, head, 1);
	} else {
		head[0] = 0xFE;
		head[1] = (uint8_t)(len & 0xFFu);
		head[2] = (uint8_t)((len >> 8) & 0xFFu);
		head[3] = (uint8_t)((len >> 16) & 0xFFu);
		total = 4 + len;
		if (!writer_room(w, total)) {
			return;
		}
		mtp_write_raw(w, head, 4);
	}
	mtp_write_raw(w, data, len);

	pad = (4 - (total % 4)) % 4;
	if (pad != 0) {
		static const uint8_t zeros[4] = { 0, 0, 0, 0 };

		mtp_write_raw(w, zeros, pad);
	}
}

void
mtp_write_string(mtp_writer_t *w, const char *s)
{
	if (s == NULL) {
		mtp_write_bytes(w, "", 0);
		return;
	}
	mtp_write_bytes(w, s, strlen(s));
}

void
mtp_write_bool(mtp_writer_t *w, int v)
{
	mtp_write_u32(w, v ? MTP_ID_boolTrue : MTP_ID_boolFalse);
}


void
mtp_write_peer(mtp_writer_t *w, const mtp_peer_t *peer)
{
	if (peer == NULL) {
		mtp_write_u32(w, MTP_ID_inputPeerEmpty);
		return;
	}
	switch (peer->kind) {
	case MTP_PEER_USER:
		mtp_write_u32(w, MTP_ID_inputPeerUser);
		mtp_write_i64(w, peer->id);
		mtp_write_i64(w, peer->access_hash);
		break;
	case MTP_PEER_CHAT:
		mtp_write_u32(w, MTP_ID_inputPeerChat);
		mtp_write_i64(w, peer->id);
		break;
	case MTP_PEER_CHANNEL:
		mtp_write_u32(w, MTP_ID_inputPeerChannel);
		mtp_write_i64(w, peer->id);
		mtp_write_i64(w, peer->access_hash);
		break;
	default:
		mtp_write_u32(w, MTP_ID_inputPeerEmpty);
		break;
	}
}

void
mtp_write_pad_to(mtp_writer_t *w, size_t multiple)
{
	static const uint8_t	zeros[16] = { 0 };
	size_t			pad;

	if (multiple == 0 || multiple > sizeof(zeros)) {
		w->overflow = 1;
		return;
	}
	pad = (multiple - (w->len % multiple)) % multiple;
	if (pad != 0) {
		mtp_write_raw(w, zeros, pad);
	}
}
