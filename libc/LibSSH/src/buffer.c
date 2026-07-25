/* !DEFINES!

$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_slice as borrowed byte span
$define %type size_t as object size
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %func lssh_buf_init as function with args lssh_buf *, size_t
$define %func lssh_buf_free as procedure with args lssh_buf *
$define %func lssh_buf_reset as procedure with args lssh_buf *
$define %func lssh_buf_set_secure as procedure with args lssh_buf *, int
$define %func lssh_buf_reserve as function with args lssh_buf *, size_t
$define %func lssh_buf_append as function with args lssh_buf *, const void *, size_t
$define %func lssh_buf_put_u8 as function with args lssh_buf *, uint8_t
$define %func lssh_buf_put_u32 as function with args lssh_buf *, uint32_t
$define %func lssh_buf_put_string as function with args lssh_buf *, const void *, size_t
$define %func lssh_buf_put_cstring as function with args lssh_buf *, const char *
$define %func lssh_reader_init as procedure with args lssh_reader *, const void *, size_t
$define %func lssh_reader_remaining as function with args const lssh_reader *
$define %func lssh_reader_u8 as function with args lssh_reader *, uint8_t *
$define %func lssh_reader_u32 as function with args lssh_reader *, uint32_t *
$define %func lssh_reader_string as function with args lssh_reader *, lssh_slice *

*/

/* !SPACE!

$space %export lssh_buf_init, lssh_buf_free, lssh_buf_reset
$space %export lssh_buf_set_secure, lssh_buf_reserve, lssh_buf_append
$space %export lssh_buf_put_u8, lssh_buf_put_u32
$space %export lssh_buf_put_string, lssh_buf_put_cstring
$space %export lssh_reader_init, lssh_reader_remaining
$space %export lssh_reader_u8, lssh_reader_u32, lssh_reader_string

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
#include <stdlib.h>
#include <string.h>
#include "private.h"

int
lssh_buf_init(lssh_buf *buf, size_t initial_capacity)
{
	if (!buf) {
		return (LSSH_ERR_INVALID);
	}
	memset(buf, 0, sizeof(*buf));
	if (initial_capacity == 0) {
		return (LSSH_OK);
	}
	buf->data = malloc(initial_capacity);
	if (!buf->data) {
		return (LSSH_ERR_NO_MEMORY);
	}
	buf->capacity = initial_capacity;
	return (LSSH_OK);
}

void
lssh_buf_free(lssh_buf *buf)
{
	if (!buf) {
		return;
	}
	if (buf->data) {
		if (buf->secure) {
			lc_wipe(buf->data, buf->capacity);
		}
		free(buf->data);
	}
	memset(buf, 0, sizeof(*buf));
}

void
lssh_buf_reset(lssh_buf *buf)
{
	if (!buf) {
		return;
	}
	if (buf->secure && buf->data) {
		lc_wipe(buf->data, buf->len);
	}
	buf->len = 0;
}

void
lssh_buf_set_secure(lssh_buf *buf, int secure)
{
	if (!buf) {
		return;
	}
	buf->secure = secure ? 1 : 0;
}

int
lssh_buf_reserve(lssh_buf *buf, size_t extra)
{
	uint8_t	*next;
	size_t	need, cap;

	if (!buf) {
		return (LSSH_ERR_INVALID);
	}
	if (extra > ((size_t)-1) - buf->len) {
		return (LSSH_ERR_RANGE);
	}
	need = buf->len + extra;
	if (need <= buf->capacity) {
		return (LSSH_OK);
	}
	cap = buf->capacity == 0 ? 256 : buf->capacity;
	while (cap < need) {
		if (cap > ((size_t)-1) / 2) {
			cap = need;
			break;
		}
		cap *= 2;
	}
	next = malloc(cap);
	if (!next) {
		return (LSSH_ERR_NO_MEMORY);
	}
	if (buf->len != 0) {
		memcpy(next, buf->data, buf->len);
	}
	if (buf->data) {
		if (buf->secure) {
			lc_wipe(buf->data, buf->capacity);
		}
		free(buf->data);
	}
	buf->data = next;
	buf->capacity = cap;
	return (LSSH_OK);
}

int
lssh_buf_append(lssh_buf *buf, const void *data, size_t len)
{
	int	ret;

	if (!buf || (!data && len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_reserve(buf, len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (len != 0) {
		memcpy(buf->data + buf->len, data, len);
		buf->len += len;
	}
	return (LSSH_OK);
}

int
lssh_buf_put_u8(lssh_buf *buf, uint8_t value)
{
	return (lssh_buf_append(buf, &value, sizeof(value)));
}

int
lssh_buf_put_u32(lssh_buf *buf, uint32_t value)
{
	uint8_t	tmp[4];

	lssh_store_u32(tmp, value);
	return (lssh_buf_append(buf, tmp, sizeof(tmp)));
}

int
lssh_buf_put_string(lssh_buf *buf, const void *data, size_t len)
{
	int	ret;

	if (len > UINT32_MAX) {
		return (LSSH_ERR_RANGE);
	}
	ret = lssh_buf_put_u32(buf, (uint32_t)len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_append(buf, data, len));
}

int
lssh_buf_put_cstring(lssh_buf *buf, const char *str)
{
	if (!str) {
		return (LSSH_ERR_INVALID);
	}
	return (lssh_buf_put_string(buf, str, strlen(str)));
}

void
lssh_reader_init(lssh_reader *reader, const void *data, size_t len)
{
	if (!reader) {
		return;
	}
	reader->data = (const uint8_t *)data;
	reader->len = len;
	reader->off = 0;
}

size_t
lssh_reader_remaining(const lssh_reader *reader)
{
	if (!reader || reader->off > reader->len) {
		return (0);
	}
	return (reader->len - reader->off);
}

int
lssh_reader_u8(lssh_reader *reader, uint8_t *out)
{
	if (!reader || !out || lssh_reader_remaining(reader) < 1) {
		return (LSSH_ERR_FORMAT);
	}
	*out = reader->data[reader->off];
	reader->off++;
	return (LSSH_OK);
}

int
lssh_reader_u32(lssh_reader *reader, uint32_t *out)
{
	if (!reader || !out || lssh_reader_remaining(reader) < 4) {
		return (LSSH_ERR_FORMAT);
	}
	*out = lssh_load_u32(reader->data + reader->off);
	reader->off += 4;
	return (LSSH_OK);
}

int
lssh_reader_string(lssh_reader *reader, lssh_slice *out)
{
	uint32_t	len;
	int		ret;

	if (!reader || !out) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_reader_u32(reader, &len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(reader) < len) {
		return (LSSH_ERR_FORMAT);
	}
	out->data = reader->data + reader->off;
	out->len = len;
	reader->off += len;
	return (LSSH_OK);
}
