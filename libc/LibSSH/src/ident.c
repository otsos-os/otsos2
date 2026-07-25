/* !DEFINES!

$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_ident as parsed SSH identification line
$define %type size_t as object size
$define %type uint8_t as 8 bit unsigned
$define %func lssh_ident_software_valid as function with args const char *, size_t
$define %func lssh_ident_comment_valid as function with args const char *, size_t
$define %func lssh_ident_parse_core as function with args const uint8_t *, size_t, lssh_ident *
$define %func lssh_ident_make as function with args lssh_buf *, const char *, const char *
$define %func lssh_ident_parse_line as function with args const void *, size_t, lssh_ident *
$define %func lssh_ident_scan as function with args const void *, size_t, size_t *, lssh_ident *

*/

/* !SPACE!

$space %internal lssh_ident_software_valid, lssh_ident_comment_valid
$space %internal lssh_ident_parse_core
$space %export lssh_ident_make, lssh_ident_parse_line, lssh_ident_scan

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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LSSH_IDENT_SCAN_MAX	8192

static int
lssh_ident_software_valid(const char *data, size_t len)
{
	size_t	i;
	uint8_t	ch;

	if (!data || len == 0 || len >= LSSH_SOFTWARE_MAX) {
		return (0);
	}
	for (i = 0; i < len; i++) {
		ch = (uint8_t)data[i];
		if (ch <= 32 || ch >= 127) {
			return (0);
		}
	}
	return (1);
}

static int
lssh_ident_comment_valid(const char *data, size_t len)
{
	size_t	i;
	uint8_t	ch;

	if (!data && len != 0) {
		return (0);
	}
	if (len >= LSSH_COMMENT_MAX) {
		return (0);
	}
	for (i = 0; i < len; i++) {
		ch = (uint8_t)data[i];
		if (ch < 32 || ch == 127) {
			return (0);
		}
	}
	return (1);
}

static int
lssh_ident_parse_core(const uint8_t *p, size_t len, lssh_ident *out)
{
	size_t	dash, space, proto_len, software_len, comment_len;

	if (!p || !out || len < 9 || len > LSSH_IDENT_MAX) {
		return (LSSH_ERR_FORMAT);
	}
	if (memcmp(p, "SSH-", 4) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	dash = 4;
	while (dash < len && p[dash] != '-') {
		dash++;
	}
	if (dash == len || dash == 4) {
		return (LSSH_ERR_FORMAT);
	}
	proto_len = dash;
	if (proto_len >= sizeof(out->protocol)) {
		return (LSSH_ERR_RANGE);
	}
	if (!((dash == 7 && memcmp(p + 4, "2.0", 3) == 0) ||
	    (dash == 8 && memcmp(p + 4, "1.99", 4) == 0))) {
		return (LSSH_ERR_UNSUPPORTED);
	}
	space = dash + 1;
	while (space < len && p[space] != ' ') {
		space++;
	}
	software_len = space - (dash + 1);
	if (!lssh_ident_software_valid((const char *)p + dash + 1,
	    software_len)) {
		return (LSSH_ERR_FORMAT);
	}
	comment_len = 0;
	if (space < len) {
		comment_len = len - space - 1;
		if (comment_len != 0 &&
		    !lssh_ident_comment_valid((const char *)p + space + 1,
		    comment_len)) {
			return (LSSH_ERR_FORMAT);
		}
	}
	memset(out, 0, sizeof(*out));
	memcpy(out->protocol, p, proto_len);
	out->protocol[proto_len] = '\0';
	memcpy(out->software, p + dash + 1, software_len);
	out->software[software_len] = '\0';
	if (comment_len != 0) {
		memcpy(out->comment, p + space + 1, comment_len);
		out->comment[comment_len] = '\0';
		out->has_comment = 1;
	}
	return (LSSH_OK);
}

int
lssh_ident_make(lssh_buf *out, const char *software, const char *comment)
{
	size_t	software_len, comment_len, total_len;
	int	ret;

	if (!out) {
		return (LSSH_ERR_INVALID);
	}
	if (!software) {
		software = LSSH_DEFAULT_SOFTWARE;
	}
	software_len = strlen(software);
	if (!lssh_ident_software_valid(software, software_len)) {
		return (LSSH_ERR_FORMAT);
	}
	comment_len = 0;
	if (comment) {
		comment_len = strlen(comment);
		if (comment_len != 0 &&
		    !lssh_ident_comment_valid(comment, comment_len)) {
			return (LSSH_ERR_FORMAT);
		}
	}
	total_len = 8 + software_len + 2;
	if (comment_len != 0) {
		total_len += 1 + comment_len;
	}
	if (total_len > LSSH_IDENT_MAX) {
		return (LSSH_ERR_RANGE);
	}
	ret = lssh_buf_append(out, "SSH-2.0-", 8);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_append(out, software, software_len);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (comment_len != 0) {
		ret = lssh_buf_append(out, " ", 1);
		if (ret != LSSH_OK) {
			return (ret);
		}
		ret = lssh_buf_append(out, comment, comment_len);
		if (ret != LSSH_OK) {
			return (ret);
		}
	}
	return (lssh_buf_append(out, "\r\n", 2));
}

int
lssh_ident_parse_line(const void *line, size_t len, lssh_ident *out)
{
	const uint8_t	*p;
	size_t		core_len;

	if (!line || !out || len == 0 || len > LSSH_IDENT_MAX) {
		return (LSSH_ERR_INVALID);
	}
	p = (const uint8_t *)line;
	core_len = len;
	if (core_len != 0 && p[core_len - 1] == '\n') {
		core_len--;
	}
	if (core_len != 0 && p[core_len - 1] == '\r') {
		core_len--;
	}
	return (lssh_ident_parse_core(p, core_len, out));
}

int
lssh_ident_scan(const void *data, size_t len, size_t *consumed,
    lssh_ident *out)
{
	const uint8_t	*p;
	size_t		line_start, i, line_len;
	int		ret;

	if (!data || !consumed || !out) {
		return (LSSH_ERR_INVALID);
	}
	*consumed = 0;
	p = (const uint8_t *)data;
	line_start = 0;
	for (i = 0; i < len; i++) {
		if (i > LSSH_IDENT_SCAN_MAX) {
			return (LSSH_ERR_RANGE);
		}
		if (i - line_start + 1 > LSSH_IDENT_MAX) {
			return (LSSH_ERR_RANGE);
		}
		if (p[i] != '\n') {
			continue;
		}
		line_len = i - line_start + 1;
		if (line_len >= 4 &&
		    memcmp(p + line_start, "SSH-", 4) == 0) {
			ret = lssh_ident_parse_line(p + line_start,
			    line_len, out);
			if (ret != LSSH_OK) {
				return (ret);
			}
			*consumed = i + 1;
			return (LSSH_OK);
		}
		line_start = i + 1;
		*consumed = line_start;
	}
	if (len - line_start >= LSSH_IDENT_MAX) {
		return (LSSH_ERR_RANGE);
	}
	if (line_start > LSSH_IDENT_SCAN_MAX) {
		return (LSSH_ERR_RANGE);
	}
	return (LSSH_ERR_AGAIN);
}
