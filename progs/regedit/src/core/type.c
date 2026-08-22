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

/* !DEFINES!

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type int32_t as 32 bit signed
$define %type size_t as native object size
$define %type re_type_map as registry type name to identifier row

$define %func re_skip_space as function with args const char **
$define %func re_parse_u64_bounded as function with args text, max, out
$define %func re_parse_i32_bounded as function with args text, out
$define %func re_hex_digit as function with args int
$define %func re_type_name as function with args uint32_t
$define %func re_type_id as function with args const char *
$define %func re_type_hint as function with args uint32_t
$define %func re_format_string as function with args data, size, out, out_size
$define %func re_format_multi as function with args data, size, out, out_size
$define %func re_format_bytes as function with args data, size, out, out_size
$define %func re_format as function with args type, data, size, out, out_size
$define %func re_parse_bool as function with args const char *, uint8_t *
$define %func re_parse_ipv4 as function with args const char *, uint8_t *
$define %func re_parse_bytes as function with args text, out, out_size, bytes
$define %func re_parse_multi as function with args text, out, out_size, bytes
$define %func re_parse as function with args type, text, out, out_size, bytes

*/

/* !SPACE!

$space %internal re_skip_space, re_parse_u64_bounded, re_parse_i32_bounded
$space %internal re_hex_digit, re_format_string, re_format_multi
$space %internal re_format_bytes, re_parse_bool, re_parse_ipv4
$space %internal re_parse_bytes, re_parse_multi
$space %export re_type_name, re_type_id, re_type_hint, re_format, re_parse

*/

#include <ctype.h>
#include <errno.h>
#include <native.h>
#include <regedit/regedit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct re_type_map {
	const char	*name;
	const char	*hint;
	uint32_t	type;
} re_type_map_t;

static const re_type_map_t	re_types[] = {
	{ "string", "text", API_REG_TYPE_STRING },
	{ "bool", "true or false", API_REG_TYPE_BOOL },
	{ "i32", "-2147483648..2147483647", API_REG_TYPE_I32 },
	{ "u32", "0..4294967295", API_REG_TYPE_U32 },
	{ "u64", "0..18446744073709551615", API_REG_TYPE_U64 },
	{ "ipv4", "a.b.c.d", API_REG_TYPE_IPV4 },
	{ "bytes", "hex pairs, e.g. 0a ff 10", API_REG_TYPE_BYTES },
	{ "multi_string", "items separated by |",
	    API_REG_TYPE_MULTI_STRING }
};

static void
re_skip_space(const char **text)
{
	const char	*p;

	p = *text;
	while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	}
	*text = p;
}

static int
re_parse_u64_bounded(const char *text, uint64_t max, uint64_t *out)
{
	const char	*p;
	uint64_t	value;
	uint32_t	digits;

	p = text;
	re_skip_space(&p);
	value = 0;
	digits = 0;
	while (*p >= '0' && *p <= '9') {
		if (value > max / 10) {
			errno = EOVERFLOW;
			return (-1);
		}
		value *= 10;
		if (value > max - (uint64_t)(*p - '0')) {
			errno = EOVERFLOW;
			return (-1);
		}
		value += (uint64_t)(*p - '0');
		digits++;
		p++;
	}
	re_skip_space(&p);
	if (digits == 0 || *p != '\0') {
		errno = EINVAL;
		return (-1);
	}
	*out = value;
	return (0);
}

static int
re_parse_i32_bounded(const char *text, int32_t *out)
{
	const char	*p;
	uint64_t	magnitude;
	int		negative;

	p = text;
	re_skip_space(&p);
	negative = 0;
	if (*p == '+' || *p == '-') {
		negative = (*p == '-');
		p++;
	}
	if (re_parse_u64_bounded(p, negative ? 2147483648ULL :
	    2147483647ULL, &magnitude) != 0) {
		return (-1);
	}
	if (negative) {
		*out = (int32_t)(-(int64_t)magnitude);
	} else {
		*out = (int32_t)magnitude;
	}
	return (0);
}

static int
re_hex_digit(int c)
{
	if (c >= '0' && c <= '9') {
		return (c - '0');
	}
	if (c >= 'a' && c <= 'f') {
		return (c - 'a' + 10);
	}
	if (c >= 'A' && c <= 'F') {
		return (c - 'A' + 10);
	}
	return (-1);
}

const char *
re_type_name(uint32_t type)
{
	size_t	i;

	for (i = 0; i < sizeof(re_types) / sizeof(re_types[0]); i++) {
		if (re_types[i].type == type) {
			return (re_types[i].name);
		}
	}
	return ("unknown");
}

uint32_t
re_type_id(const char *name)
{
	size_t	i;

	if (!name) {
		return (0);
	}
	for (i = 0; i < sizeof(re_types) / sizeof(re_types[0]); i++) {
		if (strcmp(re_types[i].name, name) == 0) {
			return (re_types[i].type);
		}
	}
	return (0);
}

const char *
re_type_hint(uint32_t type)
{
	size_t	i;

	for (i = 0; i < sizeof(re_types) / sizeof(re_types[0]); i++) {
		if (re_types[i].type == type) {
			return (re_types[i].hint);
		}
	}
	return ("");
}

static int
re_format_string(const void *data, uint32_t size, char *out, size_t out_size)
{
	const char	*src;
	size_t		i, pos;

	src = (const char *)data;
	pos = 0;
	for (i = 0; i < size; i++) {
		if (pos + 1 >= out_size) {
			out[pos] = '\0';
			errno = E2BIG;
			return (-1);
		}
		if (src[i] == '\0') {
			break;
		}
		out[pos++] = src[i];
	}
	out[pos] = '\0';
	return (0);
}

static int
re_format_multi(const void *data, uint32_t size, char *out, size_t out_size)
{
	const char	*src;
	size_t		pos;
	uint32_t	i;
	int		started;

	src = (const char *)data;
	pos = 0;
	started = 0;
	for (i = 0; i < size; i++) {
		if (src[i] == '\0') {
			if (i + 1 >= size || src[i + 1] == '\0') {
				break;
			}
			if (pos + 2 >= out_size) {
				out[pos] = '\0';
				errno = E2BIG;
				return (-1);
			}
			out[pos++] = ' ';
			out[pos++] = '|';
			started = 0;
			continue;
		}
		if (!started && pos != 0) {
			if (pos + 1 >= out_size) {
				out[pos] = '\0';
				errno = E2BIG;
				return (-1);
			}
			out[pos++] = ' ';
		}
		started = 1;
		if (pos + 1 >= out_size) {
			out[pos] = '\0';
			errno = E2BIG;
			return (-1);
		}
		out[pos++] = src[i];
	}
	out[pos] = '\0';
	return (0);
}

static int
re_format_bytes(const void *data, uint32_t size, char *out, size_t out_size)
{
	static const char	hex[] = "0123456789abcdef";
	const uint8_t		*src;
	size_t			pos;
	uint32_t		i;

	src = (const uint8_t *)data;
	pos = 0;
	for (i = 0; i < size; i++) {
		if (pos + (i != 0 ? 4 : 3) > out_size) {
			out[pos] = '\0';
			errno = E2BIG;
			return (-1);
		}
		if (i != 0) {
			out[pos++] = ' ';
		}
		out[pos++] = hex[(src[i] >> 4) & 0x0F];
		out[pos++] = hex[src[i] & 0x0F];
	}
	out[pos] = '\0';
	return (0);
}

int
re_format(uint32_t type, const void *data, uint32_t size, char *out,
    size_t out_size)
{
	const uint8_t	*src;
	uint64_t	u64;
	uint32_t	u32;
	int32_t		i32;

	if (!out || out_size == 0) {
		errno = EINVAL;
		return (-1);
	}
	out[0] = '\0';
	if (!data && size != 0) {
		errno = EINVAL;
		return (-1);
	}
	src = (const uint8_t *)data;
	switch (type) {
	case API_REG_TYPE_STRING:
		return (re_format_string(data, size, out, out_size));
	case API_REG_TYPE_BOOL:
		if (size != 1) {
			errno = EINVAL;
			return (-1);
		}
		return (snprintf(out, out_size, "%s",
		    src[0] != 0 ? "true" : "false") < 0 ? -1 : 0);
	case API_REG_TYPE_I32:
		if (size != 4) {
			errno = EINVAL;
			return (-1);
		}
		u32 = (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
		    ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
		i32 = (int32_t)u32;
		return (snprintf(out, out_size, "%d", i32) < 0 ? -1 : 0);
	case API_REG_TYPE_U32:
		if (size != 4) {
			errno = EINVAL;
			return (-1);
		}
		u32 = (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
		    ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
		return (snprintf(out, out_size, "%u", u32) < 0 ? -1 : 0);
	case API_REG_TYPE_U64:
		if (size != 8) {
			errno = EINVAL;
			return (-1);
		}
		u64 = 0;
		for (u32 = 0; u32 < 8; u32++) {
			u64 |= (uint64_t)src[u32] << (u32 * 8);
		}
		return (snprintf(out, out_size, "%llu",
		    (unsigned long long)u64) < 0 ? -1 : 0);
	case API_REG_TYPE_IPV4:
		if (size != 4) {
			errno = EINVAL;
			return (-1);
		}
		return (snprintf(out, out_size, "%u.%u.%u.%u",
		    (unsigned int)src[0], (unsigned int)src[1],
		    (unsigned int)src[2], (unsigned int)src[3]) < 0 ?
		    -1 : 0);
	case API_REG_TYPE_BYTES:
		return (re_format_bytes(data, size, out, out_size));
	case API_REG_TYPE_MULTI_STRING:
		return (re_format_multi(data, size, out, out_size));
	default:
		errno = EINVAL;
		return (-1);
	}
}

static int
re_parse_bool(const char *text, uint8_t *out)
{
	const char	*p;
	char		word[8];
	size_t		i;

	p = text;
	re_skip_space(&p);
	for (i = 0; i + 1 < sizeof(word) && p[i] != '\0' &&
	    !isspace((unsigned char)p[i]); i++) {
		word[i] = (char)tolower((unsigned char)p[i]);
	}
	word[i] = '\0';
	p += i;
	re_skip_space(&p);
	if (*p != '\0') {
		errno = EINVAL;
		return (-1);
	}
	if (strcmp(word, "true") == 0 || strcmp(word, "1") == 0 ||
	    strcmp(word, "yes") == 0 || strcmp(word, "on") == 0) {
		*out = 1;
		return (0);
	}
	if (strcmp(word, "false") == 0 || strcmp(word, "0") == 0 ||
	    strcmp(word, "no") == 0 || strcmp(word, "off") == 0) {
		*out = 0;
		return (0);
	}
	errno = EINVAL;
	return (-1);
}

static int
re_parse_ipv4(const char *text, uint8_t *out)
{
	const char	*p;
	uint64_t	octet;
	char		part[4];
	size_t		i;
	int		field;

	p = text;
	re_skip_space(&p);
	for (field = 0; field < 4; field++) {
		for (i = 0; i + 1 < sizeof(part) && p[i] >= '0' &&
		    p[i] <= '9'; i++) {
			part[i] = p[i];
		}
		part[i] = '\0';
		if (i == 0) {
			errno = EINVAL;
			return (-1);
		}
		p += i;
		if (re_parse_u64_bounded(part, 255, &octet) != 0) {
			return (-1);
		}
		out[field] = (uint8_t)octet;
		if (field == 3) {
			break;
		}
		if (*p != '.') {
			errno = EINVAL;
			return (-1);
		}
		p++;
	}
	re_skip_space(&p);
	if (*p != '\0') {
		errno = EINVAL;
		return (-1);
	}
	return (0);
}

static int
re_parse_bytes(const char *text, uint8_t *out, size_t out_size,
    uint32_t *bytes)
{
	const char	*p;
	size_t		pos;
	int		hi, lo;

	p = text;
	pos = 0;
	for (;;) {
		while (*p != '\0' && (isspace((unsigned char)*p) ||
		    *p == ',' || *p == ':')) {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		hi = re_hex_digit((unsigned char)p[0]);
		lo = re_hex_digit((unsigned char)p[1]);
		if (hi < 0 || lo < 0) {
			errno = EINVAL;
			return (-1);
		}
		if (pos >= out_size) {
			errno = E2BIG;
			return (-1);
		}
		out[pos++] = (uint8_t)((hi << 4) | lo);
		p += 2;
	}
	*bytes = (uint32_t)pos;
	return (0);
}

static int
re_parse_multi(const char *text, char *out, size_t out_size, uint32_t *bytes)
{
	const char	*p;
	size_t		pos, trim;

	p = text;
	pos = 0;
	re_skip_space(&p);
	if (*p == '\0') {
		if (out_size < 1) {
			errno = E2BIG;
			return (-1);
		}
		out[0] = '\0';
		*bytes = 1;
		return (0);
	}
	for (;;) {
		re_skip_space(&p);
		trim = pos;
		while (*p != '\0' && *p != '|') {
			if (pos + 2 > out_size) {
				errno = E2BIG;
				return (-1);
			}
			out[pos++] = *p;
			p++;
		}
		while (pos > trim && isspace((unsigned char)out[pos - 1])) {
			pos--;
		}
		if (pos + 1 > out_size) {
			errno = E2BIG;
			return (-1);
		}
		out[pos++] = '\0';
		if (*p != '|') {
			break;
		}
		p++;
	}
	*bytes = (uint32_t)pos;
	return (0);
}

int
re_parse(uint32_t type, const char *text, void *out, size_t out_size,
    uint32_t *bytes)
{
	uint8_t		*dst;
	uint64_t	value;
	int32_t		signed_value;
	size_t		len;
	uint32_t	i;

	if (!text || !out || out_size == 0 || !bytes) {
		errno = EINVAL;
		return (-1);
	}
	dst = (uint8_t *)out;
	*bytes = 0;
	switch (type) {
	case API_REG_TYPE_STRING:
		len = strlen(text);
		if (len > out_size) {
			errno = E2BIG;
			return (-1);
		}
		memcpy(dst, text, len);
		*bytes = (uint32_t)len;
		return (0);
	case API_REG_TYPE_BOOL:
		if (re_parse_bool(text, dst) != 0) {
			return (-1);
		}
		*bytes = 1;
		return (0);
	case API_REG_TYPE_I32:
		if (out_size < 4) {
			errno = E2BIG;
			return (-1);
		}
		if (re_parse_i32_bounded(text, &signed_value) != 0) {
			return (-1);
		}
		value = (uint32_t)signed_value;
		for (i = 0; i < 4; i++) {
			dst[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
		}
		*bytes = 4;
		return (0);
	case API_REG_TYPE_U32:
		if (out_size < 4) {
			errno = E2BIG;
			return (-1);
		}
		if (re_parse_u64_bounded(text, 0xFFFFFFFFULL, &value) != 0) {
			return (-1);
		}
		for (i = 0; i < 4; i++) {
			dst[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
		}
		*bytes = 4;
		return (0);
	case API_REG_TYPE_U64:
		if (out_size < 8) {
			errno = E2BIG;
			return (-1);
		}
		if (re_parse_u64_bounded(text, 0xFFFFFFFFFFFFFFFFULL,
		    &value) != 0) {
			return (-1);
		}
		for (i = 0; i < 8; i++) {
			dst[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
		}
		*bytes = 8;
		return (0);
	case API_REG_TYPE_IPV4:
		if (out_size < 4) {
			errno = E2BIG;
			return (-1);
		}
		if (re_parse_ipv4(text, dst) != 0) {
			return (-1);
		}
		*bytes = 4;
		return (0);
	case API_REG_TYPE_BYTES:
		return (re_parse_bytes(text, dst, out_size, bytes));
	case API_REG_TYPE_MULTI_STRING:
		return (re_parse_multi(text, (char *)out, out_size, bytes));
	default:
		errno = EINVAL;
		return (-1);
	}
}
