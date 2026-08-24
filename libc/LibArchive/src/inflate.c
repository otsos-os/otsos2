/* !DEFINES!

$define %type la_huff as canonical Huffman decode table view
$define %func bits_refill as procedure with args state, count
$define %func bits_get as function with args state, count
$define %func bits_align as procedure with args state
$define %func bits_byte as function with args state, out
$define %func sink_flush as function with args state
$define %func sink_byte as function with args state, byte
$define %func sink_match as function with args state, distance, length
$define %func huff_build as function with args counts, symbols, lengths, n, cap
$define %func huff_decode as function with args state, counts, symbols
$define %func huff_fixed as function with args state
$define %func inflate_stored as function with args state
$define %func inflate_dynamic as function with args state
$define %func inflate_block as function with args state
$define %func inflate_run as function with args state
$define %func inflate_reset as procedure with args state
$define %func la_inflate_stream as function with args state, window, cap, read, read arg, write, write arg, out
$define %func la_inflate as function with args in, in_len, out, out_cap, out_len
$define %func la_zlib_inflate as function with args in, in_len, out, out_cap, out_len
$define %func la_inflate_strerror as function with args err
$define %func la_crc32 as function with args data, len
$define %func la_crc32_update as function with args crc, data, len
$define %func la_adler32 as function with args data, len

*/

/* !SPACE!

$space %internal bits_refill, bits_get, bits_align, bits_byte
$space %internal sink_flush, sink_byte, sink_match
$space %internal huff_build, huff_decode, huff_fixed
$space %internal inflate_stored, inflate_dynamic, inflate_block
$space %internal inflate_run, inflate_reset
$space %internal la_clen_order, la_len_base, la_len_extra
$space %internal la_dist_base, la_dist_extra, la_crc_table, la_crc_ready
$space %internal la_crc_build
$space %export la_inflate, la_zlib_inflate, la_inflate_stream
$space %export la_inflate_strerror
$space %export la_crc32, la_crc32_update, la_adler32

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
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define LA_MAX_BITS		15
#define LA_LIT_SYMS		288
#define LA_LIT_MAX		286
#define LA_DIST_SYMS		30
#define LA_CLEN_SYMS		19


static void
bits_refill(la_inflate_t *st, uint32_t n)
{
	long	got;

	while (st->nbits < n) {
		if (st->in_pos >= st->in_len) {
			if (st->read == NULL) {
				st->eof = 1;
				st->nbits = n;
				return;
			}
			got = st->read(st->read_arg, st->in_buf,
			    sizeof(st->in_buf));
			if (got <= 0) {
				if (got < 0) {
					st->io_err = 1;
				}
				st->eof = 1;
				st->nbits = n;
				return;
			}
			st->in_data = st->in_buf;
			st->in_len = (size_t)got;
			st->in_pos = 0;
		}
		st->acc |= (uint32_t)st->in_data[st->in_pos] << st->nbits;
		st->in_pos++;
		st->nbits += 8;
	}
}

static uint32_t
bits_get(la_inflate_t *st, uint32_t n)
{
	uint32_t	v;

	if (n == 0) {
		return (0);
	}
	bits_refill(st, n);
	v = st->acc & ((n >= 32) ? 0xFFFFFFFFu : ((1u << n) - 1u));
	st->acc >>= n;
	st->nbits -= n;
	return (v);
}

static void
bits_align(la_inflate_t *st)
{
	uint32_t	drop;

	drop = st->nbits & 7u;
	st->acc >>= drop;
	st->nbits -= drop;
}

static int
bits_byte(la_inflate_t *st, uint8_t *out)
{
	uint32_t	v;

	v = bits_get(st, 8);
	if (st->eof) {
		return (LA_INF_CORRUPT);
	}
	*out = (uint8_t)v;
	return (LA_INF_OK);
}

static int
sink_flush(la_inflate_t *st)
{
	size_t	len;

	if (st->linear || st->write == NULL) {
		return (LA_INF_OK);
	}
	if (st->win_pos <= st->win_flushed) {
		return (LA_INF_OK);
	}
	len = st->win_pos - st->win_flushed;
	if (st->write(st->write_arg, st->win + st->win_flushed, len) != 0) {
		return (LA_INF_IO);
	}
	st->win_flushed = st->win_pos;
	return (LA_INF_OK);
}

static int
sink_byte(la_inflate_t *st, uint8_t b)
{
	int	ret;

	if (st->win_pos >= st->win_cap) {
		if (st->linear) {
			return (LA_INF_TOO_LARGE);
		}
		ret = sink_flush(st);
		if (ret != LA_INF_OK) {
			return (ret);
		}
		st->win_pos = 0;
		st->win_flushed = 0;
	}
	st->win[st->win_pos] = b;
	st->win_pos++;
	st->produced++;
	return (LA_INF_OK);
}

static int
sink_match(la_inflate_t *st, uint32_t dist, uint32_t len)
{
	size_t	from;
	int	ret;
	if (dist == 0 || (uint64_t)dist > st->produced ||
	    dist > LA_INF_WINDOW) {
		return (LA_INF_CORRUPT);
	}
	while (len-- > 0) {
		from = (st->win_pos + st->win_cap - dist) % st->win_cap;
		ret = sink_byte(st, st->win[from]);
		if (ret != LA_INF_OK) {
			return (ret);
		}
	}
	return (LA_INF_OK);
}

static int
huff_build(uint16_t *count, uint16_t *symbol, const uint8_t *lengths,
    uint32_t n, uint32_t sym_cap)
{
	uint16_t	offs[LA_MAX_BITS + 2];
	uint32_t	i;
	int		left;

	memset(count, 0, (LA_MAX_BITS + 1) * sizeof(*count));
	for (i = 0; i < n; i++) {
		if (lengths[i] > LA_MAX_BITS) {
			return (LA_INF_CORRUPT);
		}
		count[lengths[i]]++;
	}
	if (count[0] == n) {
		return (LA_INF_CORRUPT);
	}

	left = 1;
	for (i = 1; i <= LA_MAX_BITS; i++) {
		left <<= 1;
		left -= (int)count[i];
		if (left < 0) {
			return (LA_INF_CORRUPT);
		}
	}

	offs[1] = 0;
	for (i = 1; i <= LA_MAX_BITS; i++) {
		offs[i + 1] = (uint16_t)(offs[i] + count[i]);
	}
	if (offs[LA_MAX_BITS + 1] > sym_cap) {
		return (LA_INF_CORRUPT);
	}
	for (i = 0; i < n; i++) {
		if (lengths[i] != 0) {
			symbol[offs[lengths[i]]] = (uint16_t)i;
			offs[lengths[i]]++;
		}
	}
	return (LA_INF_OK);
}

static int
huff_decode(la_inflate_t *st, const uint16_t *count, const uint16_t *symbol)
{
	int	code, first, index, len, bit;

	code = 0;
	first = 0;
	index = 0;
	for (len = 1; len <= LA_MAX_BITS; len++) {
		bit = (int)bits_get(st, 1);
		if (st->eof) {
			return (LA_INF_CORRUPT);
		}
		code |= bit;
		if (code - first < (int)count[len]) {
			return (symbol[index + (code - first)]);
		}
		index += (int)count[len];
		first += (int)count[len];
		first <<= 1;
		code <<= 1;
	}
	return (LA_INF_CORRUPT);
}

static int
huff_fixed(la_inflate_t *st)
{
	uint8_t	lengths[LA_LIT_SYMS];
	int	i, ret;

	for (i = 0; i < 144; i++) {
		lengths[i] = 8;
	}
	for (; i < 256; i++) {
		lengths[i] = 9;
	}
	for (; i < 280; i++) {
		lengths[i] = 7;
	}
	for (; i < LA_LIT_SYMS; i++) {
		lengths[i] = 8;
	}
	ret = huff_build(st->lit_count, st->lit_symbol, lengths, LA_LIT_SYMS,
	    LA_LIT_SYMS);
	if (ret != LA_INF_OK) {
		return (ret);
	}

	for (i = 0; i < LA_DIST_SYMS; i++) {
		lengths[i] = 5;
	}
	return (huff_build(st->dist_count, st->dist_symbol, lengths,
	    LA_DIST_SYMS, LA_DIST_SYMS));
}

static int
inflate_stored(la_inflate_t *st)
{
	uint8_t		b;
	uint32_t	len, nlen;
	int		ret;

	bits_align(st);
	len = bits_get(st, 16);
	nlen = bits_get(st, 16);
	if (st->eof) {
		return (LA_INF_CORRUPT);
	}
	if ((len ^ 0xFFFFu) != nlen) {
		return (LA_INF_CORRUPT);
	}
	while (len-- > 0) {
		ret = bits_byte(st, &b);
		if (ret != LA_INF_OK) {
			return (ret);
		}
		ret = sink_byte(st, b);
		if (ret != LA_INF_OK) {
			return (ret);
		}
	}
	return (LA_INF_OK);
}

static const uint8_t la_clen_order[LA_CLEN_SYMS] = {
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

static int
inflate_dynamic(la_inflate_t *st)
{
	uint8_t		lengths[LA_LIT_SYMS + LA_DIST_SYMS];
	uint16_t	clen_count[LA_MAX_BITS + 1];
	uint16_t	clen_symbol[LA_CLEN_SYMS];
	uint32_t	nlit, ndist, nclen, i, n, prev, rep;
	int		sym, ret;

	nlit = bits_get(st, 5) + 257u;
	ndist = bits_get(st, 5) + 1u;
	nclen = bits_get(st, 4) + 4u;
	if (st->eof) {
		return (LA_INF_CORRUPT);
	}
	if (nlit > LA_LIT_MAX || ndist > LA_DIST_SYMS) {
		return (LA_INF_CORRUPT);
	}

	memset(lengths, 0, LA_CLEN_SYMS);
	for (i = 0; i < nclen; i++) {
		lengths[la_clen_order[i]] = (uint8_t)bits_get(st, 3);
	}
	if (st->eof) {
		return (LA_INF_CORRUPT);
	}
	ret = huff_build(clen_count, clen_symbol, lengths, LA_CLEN_SYMS,
	    LA_CLEN_SYMS);
	if (ret != LA_INF_OK) {
		return (ret);
	}

	memset(lengths, 0, sizeof(lengths));
	n = 0;
	prev = 0;
	while (n < nlit + ndist) {
		sym = huff_decode(st, clen_count, clen_symbol);
		if (sym < 0) {
			return (LA_INF_CORRUPT);
		}
		if (sym < 16) {
			prev = (uint32_t)sym;
			lengths[n++] = (uint8_t)sym;
			continue;
		}
		if (sym == 16) {
			if (n == 0) {
				return (LA_INF_CORRUPT);
			}
			rep = bits_get(st, 2) + 3u;
		} else if (sym == 17) {
			prev = 0;
			rep = bits_get(st, 3) + 3u;
		} else {
			prev = 0;
			rep = bits_get(st, 7) + 11u;
		}
		if (st->eof || n + rep > nlit + ndist) {
			return (LA_INF_CORRUPT);
		}
		while (rep-- > 0) {
			lengths[n++] = (uint8_t)prev;
		}
	}

	if (lengths[256] == 0) {
		return (LA_INF_CORRUPT);
	}

	ret = huff_build(st->lit_count, st->lit_symbol, lengths, nlit,
	    LA_LIT_SYMS);
	if (ret != LA_INF_OK) {
		return (ret);
	}
	return (huff_build(st->dist_count, st->dist_symbol, lengths + nlit,
	    ndist, LA_DIST_SYMS));
}

static const uint16_t la_len_base[29] = {
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
	59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};
static const uint8_t la_len_extra[29] = {
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
	4, 5, 5, 5, 5, 0
};
static const uint16_t la_dist_base[LA_DIST_SYMS] = {
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
	513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};
static const uint8_t la_dist_extra[LA_DIST_SYMS] = {
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
	10, 11, 11, 12, 12, 13, 13
};

static int
inflate_block(la_inflate_t *st)
{
	uint32_t	len, dist;
	int		sym, ret;

	for (;;) {
		sym = huff_decode(st, st->lit_count, st->lit_symbol);
		if (sym < 0) {
			return (LA_INF_CORRUPT);
		}
		if (sym < 256) {
			ret = sink_byte(st, (uint8_t)sym);
			if (ret != LA_INF_OK) {
				return (ret);
			}
			continue;
		}
		if (sym == 256) {
			return (LA_INF_OK);
		}
		sym -= 257;
		if (sym >= 29) {
			return (LA_INF_CORRUPT);
		}
		len = (uint32_t)la_len_base[sym] +
		    bits_get(st, la_len_extra[sym]);

		sym = huff_decode(st, st->dist_count, st->dist_symbol);
		if (sym < 0 || sym >= LA_DIST_SYMS) {
			return (LA_INF_CORRUPT);
		}
		dist = (uint32_t)la_dist_base[sym] +
		    bits_get(st, la_dist_extra[sym]);
		if (st->eof) {
			return (LA_INF_CORRUPT);
		}
		ret = sink_match(st, dist, len);
		if (ret != LA_INF_OK) {
			return (ret);
		}
	}
}

static int
inflate_run(la_inflate_t *st)
{
	uint32_t	final, type;
	int		ret;

	do {
		final = bits_get(st, 1);
		type = bits_get(st, 2);
		if (st->eof) {
			return (LA_INF_CORRUPT);
		}
		switch (type) {
		case 0:
			ret = inflate_stored(st);
			break;
		case 1:
			ret = huff_fixed(st);
			if (ret == LA_INF_OK) {
				ret = inflate_block(st);
			}
			break;
		case 2:
			ret = inflate_dynamic(st);
			if (ret == LA_INF_OK) {
				ret = inflate_block(st);
			}
			break;
		default:
			ret = LA_INF_CORRUPT;
			break;
		}
		if (ret != LA_INF_OK) {
			return (ret);
		}
	} while (final == 0);

	return (sink_flush(st));
}

static void
inflate_reset(la_inflate_t *st)
{
	st->in_data = NULL;
	st->in_len = 0;
	st->in_pos = 0;
	st->read = NULL;
	st->read_arg = NULL;
	st->win = NULL;
	st->win_cap = 0;
	st->win_pos = 0;
	st->win_flushed = 0;
	st->produced = 0;
	st->write = NULL;
	st->write_arg = NULL;
	st->acc = 0;
	st->nbits = 0;
	st->eof = 0;
	st->io_err = 0;
	st->linear = 0;
}

int
la_inflate_stream(la_inflate_t *st, void *window, size_t win_cap,
    la_read_fn read, void *read_arg, la_write_fn write, void *write_arg,
    uint64_t *out_len)
{
	int	ret;

	if (st == NULL || window == NULL || read == NULL || write == NULL) {
		return (LA_INF_INVAL);
	}
	if (win_cap < LA_INF_WINDOW) {
		return (LA_INF_INVAL);
	}

	inflate_reset(st);
	st->win = (uint8_t *)window;
	st->win_cap = win_cap;
	st->read = read;
	st->read_arg = read_arg;
	st->write = write;
	st->write_arg = write_arg;
	ret = inflate_run(st);
	if (ret != LA_INF_OK && st->io_err) {
		ret = LA_INF_IO;
	}
	if (out_len != NULL) {
		*out_len = (ret == LA_INF_OK) ? st->produced : 0;
	}
	return (ret);
}

int
la_inflate(const void *in, size_t in_len, void *out, size_t out_cap,
    size_t *out_len)
{
	la_inflate_t	st;
	int		ret;

	if (in == NULL || out == NULL || out_len == NULL) {
		return (LA_INF_INVAL);
	}
	*out_len = 0;
	if (in_len == 0) {
		return (LA_INF_CORRUPT);
	}

	inflate_reset(&st);
	st.in_data = (const uint8_t *)in;
	st.in_len = in_len;
	st.win = (uint8_t *)out;
	st.win_cap = out_cap;
	st.linear = 1;

	ret = inflate_run(&st);
	if (ret != LA_INF_OK) {
		return (ret);
	}
	*out_len = st.win_pos;
	return (LA_INF_OK);
}

int
la_zlib_inflate(const void *in, size_t in_len, void *out, size_t out_cap,
    size_t *out_len)
{
	const uint8_t	*p;
	uint32_t	cmf, flg, stored, actual;
	int		ret;

	if (in == NULL || out == NULL || out_len == NULL) {
		return (LA_INF_INVAL);
	}
	*out_len = 0;
	if (in_len < 7) {
		return (LA_INF_CORRUPT);
	}

	p = (const uint8_t *)in;
	cmf = p[0];
	flg = p[1];
	if ((cmf & 0x0Fu) != 8u) {
		return (LA_INF_UNSUPPORTED);
	}
	if (((cmf << 8) | flg) % 31u != 0u) {
		return (LA_INF_CORRUPT);
	}
	if ((flg & 0x20u) != 0u) {
		return (LA_INF_UNSUPPORTED);
	}

	ret = la_inflate(p + 2, in_len - 2 - 4, out, out_cap, out_len);
	if (ret != LA_INF_OK) {
		return (ret);
	}

	stored = ((uint32_t)p[in_len - 4] << 24) |
	    ((uint32_t)p[in_len - 3] << 16) |
	    ((uint32_t)p[in_len - 2] << 8) | (uint32_t)p[in_len - 1];
	actual = la_adler32(out, *out_len);
	if (stored != actual) {
		*out_len = 0;
		return (LA_INF_CORRUPT);
	}
	return (LA_INF_OK);
}

const char *
la_inflate_strerror(int err)
{
	switch (err) {
	case LA_INF_OK:
		return ("ok");
	case LA_INF_INVAL:
		return ("invalid argument");
	case LA_INF_UNSUPPORTED:
		return ("unsupported compression");
	case LA_INF_CORRUPT:
		return ("corrupt compressed data");
	case LA_INF_TOO_LARGE:
		return ("output too large");
	case LA_INF_IO:
		return ("io error");
	default:
		return ("unknown error");
	}
}

static uint32_t	la_crc_table[256];
static int	la_crc_ready;

static void
la_crc_build(void)
{
	uint32_t	c, i, j;

	if (la_crc_ready) {
		return;
	}
	for (i = 0; i < 256; i++) {
		c = i;
		for (j = 0; j < 8; j++) {
			if (c & 1u) {
				c = 0xEDB88320u ^ (c >> 1);
			} else {
				c >>= 1;
			}
		}
		la_crc_table[i] = c;
	}
	la_crc_ready = 1;
}

uint32_t
la_crc32_update(uint32_t crc, const void *data, size_t len)
{
	const uint8_t	*p;
	size_t		i;

	if (data == NULL) {
		return (crc);
	}
	la_crc_build();
	p = (const uint8_t *)data;
	for (i = 0; i < len; i++) {
		crc = la_crc_table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
	}
	return (crc);
}

uint32_t
la_crc32(const void *data, size_t len)
{
	if (data == NULL) {
		return (0);
	}
	return (la_crc32_update(0xFFFFFFFFu, data, len) ^ 0xFFFFFFFFu);
}

uint32_t
la_adler32(const void *data, size_t len)
{
	const uint8_t	*p;
	uint32_t	a, b;
	size_t		i, block;

	if (data == NULL) {
		return (1u);
	}
	p = (const uint8_t *)data;
	a = 1u;
	b = 0u;
	while (len > 0) {
		block = (len > 5552u) ? 5552u : len;
		for (i = 0; i < block; i++) {
			a += p[i];
			b += a;
		}
		a %= 65521u;
		b %= 65521u;
		p += block;
		len -= block;
	}
	return ((b << 16) | a);
}
