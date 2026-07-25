/* !DEFINES!

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %func lc_poly1305_blocks as procedure with args state arrays, data, len
$define %func lc_poly1305_auth as procedure with args out, data, size, key

*/

/* !SPACE!

$space %internal lc_poly1305_blocks
$space %export lc_poly1305_auth

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
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

#define LC_POLY1305_MASK	0x3ffffffUL

static void
lc_poly1305_blocks(uint32_t h[5], const uint32_t r[5],
    const uint8_t *data, size_t blocks, uint32_t hibit)
{
	uint64_t	d0, d1, d2, d3, d4;
	uint32_t	s1, s2, s3, s4;
	uint32_t	t0, t1, t2, t3;
	uint32_t	c;
	size_t		i;

	s1 = r[1] * 5;
	s2 = r[2] * 5;
	s3 = r[3] * 5;
	s4 = r[4] * 5;
	for (i = 0; i < blocks; i++) {
		t0 = lc_load32_le(data);
		t1 = lc_load32_le(data + 4);
		t2 = lc_load32_le(data + 8);
		t3 = lc_load32_le(data + 12);
		data += 16;

		h[0] += t0 & LC_POLY1305_MASK;
		h[1] += ((t0 >> 26) | (t1 << 6)) & LC_POLY1305_MASK;
		h[2] += ((t1 >> 20) | (t2 << 12)) & LC_POLY1305_MASK;
		h[3] += ((t2 >> 14) | (t3 << 18)) & LC_POLY1305_MASK;
		h[4] += (t3 >> 8) | hibit;

		d0 = ((uint64_t)h[0] * r[0]) + ((uint64_t)h[1] * s4) +
		    ((uint64_t)h[2] * s3) + ((uint64_t)h[3] * s2) +
		    ((uint64_t)h[4] * s1);
		d1 = ((uint64_t)h[0] * r[1]) + ((uint64_t)h[1] * r[0]) +
		    ((uint64_t)h[2] * s4) + ((uint64_t)h[3] * s3) +
		    ((uint64_t)h[4] * s2);
		d2 = ((uint64_t)h[0] * r[2]) + ((uint64_t)h[1] * r[1]) +
		    ((uint64_t)h[2] * r[0]) + ((uint64_t)h[3] * s4) +
		    ((uint64_t)h[4] * s3);
		d3 = ((uint64_t)h[0] * r[3]) + ((uint64_t)h[1] * r[2]) +
		    ((uint64_t)h[2] * r[1]) + ((uint64_t)h[3] * r[0]) +
		    ((uint64_t)h[4] * s4);
		d4 = ((uint64_t)h[0] * r[4]) + ((uint64_t)h[1] * r[3]) +
		    ((uint64_t)h[2] * r[2]) + ((uint64_t)h[3] * r[1]) +
		    ((uint64_t)h[4] * r[0]);

		c = (uint32_t)(d0 >> 26);
		h[0] = (uint32_t)d0 & LC_POLY1305_MASK;
		d1 += c;
		c = (uint32_t)(d1 >> 26);
		h[1] = (uint32_t)d1 & LC_POLY1305_MASK;
		d2 += c;
		c = (uint32_t)(d2 >> 26);
		h[2] = (uint32_t)d2 & LC_POLY1305_MASK;
		d3 += c;
		c = (uint32_t)(d3 >> 26);
		h[3] = (uint32_t)d3 & LC_POLY1305_MASK;
		d4 += c;
		c = (uint32_t)(d4 >> 26);
		h[4] = (uint32_t)d4 & LC_POLY1305_MASK;
		h[0] += c * 5;
		c = h[0] >> 26;
		h[0] &= LC_POLY1305_MASK;
		h[1] += c;
	}
}

void
lc_poly1305_auth(uint8_t tag[LC_POLY1305_TAG_SIZE], const void *data,
    size_t len, const uint8_t key[LC_POLY1305_KEY_SIZE])
{
	uint8_t		block[16];
	const uint8_t	*m;
	uint32_t	r[5], h[5], g[5];
	uint64_t	f0, f1, f2, f3;
	uint32_t	w0, w1, w2, w3;
	uint32_t	mask, nmask, c;
	size_t		blocks, rem;

	if (!tag || (!data && len != 0) || !key) {
		return;
	}

	r[0] = lc_load32_le(key) & 0x3ffffff;
	r[1] = (lc_load32_le(key + 3) >> 2) & 0x3ffff03;
	r[2] = (lc_load32_le(key + 6) >> 4) & 0x3ffc0ff;
	r[3] = (lc_load32_le(key + 9) >> 6) & 0x3f03fff;
	r[4] = (lc_load32_le(key + 12) >> 8) & 0x00fffff;
	memset(h, 0, sizeof(h));

	m = (const uint8_t *)data;
	blocks = len / 16;
	if (blocks != 0) {
		lc_poly1305_blocks(h, r, m, blocks, 1U << 24);
		m += blocks * 16;
	}

	rem = len & 15;
	if (rem != 0) {
		memset(block, 0, sizeof(block));
		memcpy(block, m, rem);
		block[rem] = 1;
		lc_poly1305_blocks(h, r, block, 1, 0);
	}

	c = h[1] >> 26;
	h[1] &= LC_POLY1305_MASK;
	h[2] += c;
	c = h[2] >> 26;
	h[2] &= LC_POLY1305_MASK;
	h[3] += c;
	c = h[3] >> 26;
	h[3] &= LC_POLY1305_MASK;
	h[4] += c;
	c = h[4] >> 26;
	h[4] &= LC_POLY1305_MASK;
	h[0] += c * 5;
	c = h[0] >> 26;
	h[0] &= LC_POLY1305_MASK;
	h[1] += c;

	g[0] = h[0] + 5;
	c = g[0] >> 26;
	g[0] &= LC_POLY1305_MASK;
	g[1] = h[1] + c;
	c = g[1] >> 26;
	g[1] &= LC_POLY1305_MASK;
	g[2] = h[2] + c;
	c = g[2] >> 26;
	g[2] &= LC_POLY1305_MASK;
	g[3] = h[3] + c;
	c = g[3] >> 26;
	g[3] &= LC_POLY1305_MASK;
	g[4] = h[4] + c - (1U << 26);

	mask = (g[4] >> 31) - 1;
	nmask = ~mask;
	h[0] = (h[0] & nmask) | (g[0] & mask);
	h[1] = (h[1] & nmask) | (g[1] & mask);
	h[2] = (h[2] & nmask) | (g[2] & mask);
	h[3] = (h[3] & nmask) | (g[3] & mask);
	h[4] = (h[4] & nmask) | (g[4] & mask);

	w0 = (uint32_t)(((uint64_t)h[0] |
	    ((uint64_t)h[1] << 26)) & 0xffffffffULL);
	w1 = (uint32_t)(((uint64_t)(h[1] >> 6) |
	    ((uint64_t)h[2] << 20)) & 0xffffffffULL);
	w2 = (uint32_t)(((uint64_t)(h[2] >> 12) |
	    ((uint64_t)h[3] << 14)) & 0xffffffffULL);
	w3 = (uint32_t)(((uint64_t)(h[3] >> 18) |
	    ((uint64_t)h[4] << 8)) & 0xffffffffULL);

	f0 = (uint64_t)w0 + lc_load32_le(key + 16);
	f1 = (uint64_t)w1 + lc_load32_le(key + 20) + (f0 >> 32);
	f2 = (uint64_t)w2 + lc_load32_le(key + 24) + (f1 >> 32);
	f3 = (uint64_t)w3 + lc_load32_le(key + 28) + (f2 >> 32);

	lc_store32_le(tag, (uint32_t)f0);
	lc_store32_le(tag + 4, (uint32_t)f1);
	lc_store32_le(tag + 8, (uint32_t)f2);
	lc_store32_le(tag + 12, (uint32_t)f3);
	lc_wipe(block, sizeof(block));
	lc_wipe(r, sizeof(r));
	lc_wipe(h, sizeof(h));
	lc_wipe(g, sizeof(g));
}
