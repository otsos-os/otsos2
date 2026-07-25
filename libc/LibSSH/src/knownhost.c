/* !DEFINES!

$define %type lssh_slice as borrowed byte span
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_knownhost_result as known-hosts verification result
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type size_t as object size
$define %func lssh_kh_ws as function with args int
$define %func lssh_kh_base64_value as function with args uint8_t
$define %func lssh_kh_base64_encode as function with args lssh_buf *, const uint8_t *, size_t
$define %func lssh_kh_base64_decode as function with args lssh_buf *, lssh_slice
$define %func lssh_kh_next_token as function with args const char *, size_t, size_t *, lssh_slice *
$define %func lssh_kh_slice_eq as function with args lssh_slice, lssh_slice
$define %func lssh_kh_pattern_match as function with args lssh_slice, lssh_slice
$define %func lssh_kh_hostlist_match as function with args lssh_slice, lssh_slice
$define %func lssh_kh_key_type as function with args lssh_slice, lssh_slice *
$define %func lssh_kh_check_line as function with args line, len, host, key, decoded, changed
$define %func lssh_knownhost_format_host as function with args char *, size_t, const char *, uint32_t
$define %func lssh_knownhost_fingerprint as function with args lssh_slice, uint8_t *
$define %func lssh_knownhost_fingerprint_hex as function with args lssh_slice, char *, size_t
$define %func lssh_knownhosts_check as function with args path, host, port, host key, result
$define %func lssh_knownhosts_add as function with args path, host, port, host key

*/

/* !SPACE!

$space %internal lssh_kh_ws, lssh_kh_base64_value
$space %internal lssh_kh_base64_encode, lssh_kh_base64_decode
$space %internal lssh_kh_next_token, lssh_kh_slice_eq
$space %internal lssh_kh_pattern_match, lssh_kh_hostlist_match
$space %internal lssh_kh_key_type, lssh_kh_check_line
$space %export lssh_knownhost_format_host, lssh_knownhost_fingerprint
$space %export lssh_knownhost_fingerprint_hex
$space %export lssh_knownhosts_check, lssh_knownhosts_add

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

#include <errno.h>
#include <libcrypto.h>
#include <libssh.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LSSH_KH_READ_SIZE	512

static const char	g_lssh_kh_b64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int
lssh_kh_ws(int ch)
{
	return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

static int
lssh_kh_base64_value(uint8_t ch)
{
	if (ch >= 'A' && ch <= 'Z') {
		return ((int)(ch - 'A'));
	}
	if (ch >= 'a' && ch <= 'z') {
		return ((int)(ch - 'a') + 26);
	}
	if (ch >= '0' && ch <= '9') {
		return ((int)(ch - '0') + 52);
	}
	if (ch == '+') {
		return (62);
	}
	if (ch == '/') {
		return (63);
	}
	return (-1);
}

static int
lssh_kh_base64_encode(lssh_buf *out, const uint8_t *data, size_t len)
{
	char	chunk[4];
	size_t	i;
	uint32_t	v;
	int	ret;

	if (!out || (!data && len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	for (i = 0; i < len; i += 3) {
		v = (uint32_t)data[i] << 16;
		if (i + 1 < len) {
			v |= (uint32_t)data[i + 1] << 8;
		}
		if (i + 2 < len) {
			v |= (uint32_t)data[i + 2];
		}
		chunk[0] = g_lssh_kh_b64[(v >> 18) & 0x3f];
		chunk[1] = g_lssh_kh_b64[(v >> 12) & 0x3f];
		chunk[2] = i + 1 < len ?
		    g_lssh_kh_b64[(v >> 6) & 0x3f] : '=';
		chunk[3] = i + 2 < len ? g_lssh_kh_b64[v & 0x3f] : '=';
		ret = lssh_buf_append(out, chunk, sizeof(chunk));
		if (ret != LSSH_OK) {
			return (ret);
		}
	}
	return (LSSH_OK);
}

static int
lssh_kh_base64_decode(lssh_buf *out, lssh_slice token)
{
	uint8_t	byte;
	uint32_t	acc, lowmask;
	size_t	i;
	int	bits, value, ended, ret;

	if (!out || (!token.data && token.len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	lssh_buf_reset(out);
	acc = 0;
	bits = 0;
	ended = 0;
	for (i = 0; i < token.len; i++) {
		if (token.data[i] == '=') {
			ended = 1;
			continue;
		}
		value = lssh_kh_base64_value(token.data[i]);
		if (value < 0 || ended) {
			return (LSSH_ERR_FORMAT);
		}
		acc = (acc << 6) | (uint32_t)value;
		bits += 6;
		while (bits >= 8) {
			bits -= 8;
			byte = (uint8_t)((acc >> bits) & 0xff);
			ret = lssh_buf_append(out, &byte, 1);
			if (ret != LSSH_OK) {
				return (ret);
			}
		}
	}
	if (bits != 0) {
		lowmask = ((uint32_t)1 << bits) - 1;
		if ((acc & lowmask) != 0) {
			lssh_buf_reset(out);
			return (LSSH_ERR_FORMAT);
		}
	}
	return (LSSH_OK);
}

static int
lssh_kh_next_token(const char *line, size_t len, size_t *off,
    lssh_slice *token)
{
	size_t	start, end;

	if (!line || !off || !token || *off > len) {
		return (LSSH_ERR_INVALID);
	}
	start = *off;
	while (start < len && lssh_kh_ws((uint8_t)line[start])) {
		start++;
	}
	if (start >= len || line[start] == '#') {
		token->data = NULL;
		token->len = 0;
		*off = start;
		return (0);
	}
	end = start;
	while (end < len && !lssh_kh_ws((uint8_t)line[end])) {
		end++;
	}
	token->data = (const uint8_t *)line + start;
	token->len = end - start;
	*off = end;
	return (1);
}

static int
lssh_kh_slice_eq(lssh_slice a, lssh_slice b)
{
	if (a.len != b.len || (!a.data && a.len != 0) ||
	    (!b.data && b.len != 0)) {
		return (0);
	}
	if (a.len == 0) {
		return (1);
	}
	return (memcmp(a.data, b.data, a.len) == 0);
}

static int
lssh_kh_pattern_match(lssh_slice pattern, lssh_slice text)
{
	size_t	p, t, star_p, star_t;

	p = 0;
	t = 0;
	star_p = (size_t)-1;
	star_t = 0;
	while (t < text.len) {
		if (p < pattern.len &&
		    (pattern.data[p] == '?' || pattern.data[p] == text.data[t])) {
			p++;
			t++;
			continue;
		}
		if (p < pattern.len && pattern.data[p] == '*') {
			star_p = p;
			star_t = t;
			p++;
			continue;
		}
		if (star_p != (size_t)-1) {
			p = star_p + 1;
			star_t++;
			t = star_t;
			continue;
		}
		return (0);
	}
	while (p < pattern.len && pattern.data[p] == '*') {
		p++;
	}
	return (p == pattern.len);
}

static int
lssh_kh_hostlist_match(lssh_slice hostlist, lssh_slice host)
{
	lssh_slice	pattern;
	size_t		start, end;
	int		matched, negated;

	matched = 0;
	start = 0;
	while (start < hostlist.len) {
		end = start;
		while (end < hostlist.len && hostlist.data[end] != ',') {
			end++;
		}
		pattern.data = hostlist.data + start;
		pattern.len = end - start;
		negated = 0;
		if (pattern.len != 0 && pattern.data[0] == '!') {
			negated = 1;
			pattern.data++;
			pattern.len--;
		}
		if (pattern.len != 0 &&
		    lssh_kh_pattern_match(pattern, host)) {
			if (negated) {
				return (0);
			}
			matched = 1;
		}
		start = end + 1;
	}
	return (matched);
}

static int
lssh_kh_key_type(lssh_slice host_key_blob, lssh_slice *key_type)
{
	lssh_reader	reader;
	int		ret;

	if (!host_key_blob.data || !key_type) {
		return (LSSH_ERR_INVALID);
	}
	lssh_reader_init(&reader, host_key_blob.data, host_key_blob.len);
	ret = lssh_reader_string(&reader, key_type);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (key_type->len == 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

static int
lssh_kh_check_line(const char *line, size_t len, lssh_slice host,
    lssh_slice host_key_blob, lssh_buf *decoded, int *changed)
{
	lssh_slice	hostlist, key_type, encoded, expected_type;
	size_t		off;
	int		ret;

	if (!line || !decoded || !changed) {
		return (LSSH_ERR_INVALID);
	}
	*changed = 0;
	off = 0;
	ret = lssh_kh_next_token(line, len, &off, &hostlist);
	if (ret <= 0) {
		return (ret);
	}
	if (hostlist.len != 0 && hostlist.data[0] == '@') {
		ret = lssh_kh_next_token(line, len, &off, &hostlist);
		if (ret <= 0) {
			return (LSSH_OK);
		}
	}
	ret = lssh_kh_next_token(line, len, &off, &key_type);
	if (ret <= 0) {
		return (LSSH_OK);
	}
	ret = lssh_kh_next_token(line, len, &off, &encoded);
	if (ret <= 0) {
		return (LSSH_OK);
	}
	if (!lssh_kh_hostlist_match(hostlist, host)) {
		return (LSSH_OK);
	}
	ret = lssh_kh_key_type(host_key_blob, &expected_type);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (!lssh_kh_slice_eq(key_type, expected_type)) {
		return (LSSH_OK);
	}
	ret = lssh_kh_base64_decode(decoded, encoded);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (decoded->len == host_key_blob.len &&
	    memcmp(decoded->data, host_key_blob.data, decoded->len) == 0) {
		return (1);
	}
	*changed = 1;
	return (LSSH_OK);
}

int
lssh_knownhost_format_host(char *out, size_t out_size,
    const char *host, uint32_t port)
{
	int	len;

	if (!out || out_size == 0 || !host || host[0] == '\0') {
		return (LSSH_ERR_INVALID);
	}
	if (port == 0 || port == 22) {
		len = snprintf(out, out_size, "%s", host);
	} else {
		len = snprintf(out, out_size, "[%s]:%u", host,
		    (unsigned int)port);
	}
	if (len < 0 || (size_t)len >= out_size) {
		return (LSSH_ERR_RANGE);
	}
	return (LSSH_OK);
}

int
lssh_knownhost_fingerprint(lssh_slice host_key_blob,
    uint8_t fingerprint[LSSH_SHA256_SIZE])
{
	if (!host_key_blob.data || !fingerprint) {
		return (LSSH_ERR_INVALID);
	}
	lc_sha256(host_key_blob.data, host_key_blob.len, fingerprint);
	return (LSSH_OK);
}

int
lssh_knownhost_fingerprint_hex(lssh_slice host_key_blob,
    char *out, size_t out_size)
{
	static const char	hex[] = "0123456789abcdef";
	uint8_t			fp[LSSH_SHA256_SIZE];
	size_t			i;
	int			ret;

	if (!out || out_size < LSSH_FINGERPRINT_HEX_MAX) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_knownhost_fingerprint(host_key_blob, fp);
	if (ret != LSSH_OK) {
		return (ret);
	}
	for (i = 0; i < sizeof(fp); i++) {
		out[i * 2] = hex[fp[i] >> 4];
		out[i * 2 + 1] = hex[fp[i] & 0x0f];
	}
	out[sizeof(fp) * 2] = '\0';
	lc_wipe(fp, sizeof(fp));
	return (LSSH_OK);
}

int
lssh_knownhosts_check(const char *path, const char *host, uint32_t port,
    lssh_slice host_key_blob, lssh_knownhost_result *result)
{
	lssh_buf	decoded;
	lssh_slice	target;
	char		read_buf[LSSH_KH_READ_SIZE];
	char		line[LSSH_KNOWNHOST_LINE_MAX];
	ssize_t		n;
	size_t		i, line_len;
	uint32_t	line_no, changed_line;
	int		fd, ret, changed, saw_changed;

	if (!path || !host || !host_key_blob.data || !result) {
		return (LSSH_ERR_INVALID);
	}
	memset(result, 0, sizeof(*result));
	ret = lssh_knownhost_format_host(result->host, sizeof(result->host),
	    host, port);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_knownhost_fingerprint(host_key_blob, result->fingerprint);
	if (ret != LSSH_OK) {
		return (ret);
	}
	result->status = LSSH_KNOWNHOST_NEW;
	target.data = (const uint8_t *)result->host;
	target.len = strlen(result->host);
	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		return (errno == ENOENT ? LSSH_OK : LSSH_ERR_IO);
	}
	ret = lssh_buf_init(&decoded, 256);
	if (ret != LSSH_OK) {
		dataClose(fd);
		return (ret);
	}
	line_len = 0;
	line_no = 1;
	changed_line = 0;
	saw_changed = 0;
	for (;;) {
		n = dataRead(fd, read_buf, sizeof(read_buf));
		if (n < 0) {
			ret = LSSH_ERR_IO;
			break;
		}
		if (n == 0) {
			break;
		}
		for (i = 0; i < (size_t)n; i++) {
			if (line_len + 1 >= sizeof(line)) {
				ret = LSSH_ERR_RANGE;
				break;
			}
			line[line_len] = read_buf[i];
			line_len++;
			if (read_buf[i] != '\n') {
				continue;
			}
			changed = 0;
			ret = lssh_kh_check_line(line, line_len, target,
			    host_key_blob, &decoded, &changed);
			if (ret == 1) {
				result->status = LSSH_KNOWNHOST_TRUSTED;
				result->line = line_no;
				ret = LSSH_OK;
				goto out;
			}
			if (ret != LSSH_OK) {
				break;
			}
			if (changed && !saw_changed) {
				saw_changed = 1;
				changed_line = line_no;
			}
			line_len = 0;
			line_no++;
		}
		if (ret != LSSH_OK) {
			break;
		}
	}
	if (ret == LSSH_OK && line_len != 0) {
		changed = 0;
		ret = lssh_kh_check_line(line, line_len, target,
		    host_key_blob, &decoded, &changed);
		if (ret == 1) {
			result->status = LSSH_KNOWNHOST_TRUSTED;
			result->line = line_no;
			ret = LSSH_OK;
		} else if (ret == LSSH_OK && changed && !saw_changed) {
			saw_changed = 1;
			changed_line = line_no;
		}
	}
	if (ret == LSSH_OK && result->status != LSSH_KNOWNHOST_TRUSTED &&
	    saw_changed) {
		result->status = LSSH_KNOWNHOST_CHANGED;
		result->line = changed_line;
	}

out:
	lssh_buf_free(&decoded);
	dataClose(fd);
	return (ret);
}

int
lssh_knownhosts_add(const char *path, const char *host, uint32_t port,
    lssh_slice host_key_blob)
{
	lssh_buf	line, encoded;
	lssh_slice	key_type;
	char		host_text[LSSH_KNOWNHOST_HOST_MAX];
	int		fd, ret;

	if (!path || !host || !host_key_blob.data) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_knownhost_format_host(host_text, sizeof(host_text),
	    host, port);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kh_key_type(host_key_blob, &key_type);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&line, 256);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&encoded, 256);
	if (ret != LSSH_OK) {
		lssh_buf_free(&line);
		return (ret);
	}
	ret = lssh_kh_base64_encode(&encoded, host_key_blob.data,
	    host_key_blob.len);
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, host_text, strlen(host_text));
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, " ", 1);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, key_type.data, key_type.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, " ", 1);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, encoded.data, encoded.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_append(&line, "\n", 1);
	}
	if (ret != LSSH_OK) {
		lssh_buf_free(&encoded);
		lssh_buf_free(&line);
		return (ret);
	}
	fd = dataOpen(path, API_OPEN_WRITE | API_OPEN_CREATE |
	    API_OPEN_APPEND);
	if (fd < 0) {
		lssh_buf_free(&encoded);
		lssh_buf_free(&line);
		return (LSSH_ERR_IO);
	}
	ret = dataWriteFull(fd, line.data, line.len) == 0 ?
	    LSSH_OK : LSSH_ERR_IO;
	dataClose(fd);
	lssh_buf_free(&encoded);
	lssh_buf_free(&line);
	return (ret);
}
