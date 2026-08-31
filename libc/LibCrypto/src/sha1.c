/* !DEFINES!

$define %type lc_sha1_ctx as SHA-1 streaming hash state
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %func rotl32 as function with args uint32_t, uint32_t
$define %func lc_sha1_transform as procedure with args lc_sha1_ctx *, const uint8_t *
$define %func lc_sha1_init as procedure with args lc_sha1_ctx *
$define %func lc_sha1_update as procedure with args lc_sha1_ctx *, const void *, size_t
$define %func lc_sha1_final as procedure with args lc_sha1_ctx *, uint8_t *
$define %func lc_sha1 as procedure with args const void *, size_t, uint8_t *

*/

/* !SPACE!

$space %internal rotl32, lc_sha1_transform
$space %export lc_sha1_init, lc_sha1_update, lc_sha1_final
$space %export lc_sha1

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

static uint32_t
rotl32(uint32_t x, uint32_t n)
{
	return ((x << n) | (x >> (32 - n)));
}

static void
lc_sha1_transform(lc_sha1_ctx *ctx, const uint8_t *data)
{
	uint32_t	w[80];
	uint32_t	a, b, c, d, e, f, k, tmp;
	int		i;

	for (i = 0; i < 16; i++) {
		w[i] = ((uint32_t)data[i * 4] << 24) |
		    ((uint32_t)data[i * 4 + 1] << 16) |
		    ((uint32_t)data[i * 4 + 2] << 8) |
		    (uint32_t)data[i * 4 + 3];
	}
	for (i = 16; i < 80; i++) {
		w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];

	for (i = 0; i < 80; i++) {
		if (i < 20) {
			f = (b & c) | ((~b) & d);
			k = 0x5a827999;
		} else if (i < 40) {
			f = b ^ c ^ d;
			k = 0x6ed9eba1;
		} else if (i < 60) {
			f = (b & c) | (b & d) | (c & d);
			k = 0x8f1bbcdc;
		} else {
			f = b ^ c ^ d;
			k = 0xca62c1d6;
		}
		tmp = rotl32(a, 5) + f + e + k + w[i];
		e = d;
		d = c;
		c = rotl32(b, 30);
		b = a;
		a = tmp;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
}

void
lc_sha1_init(lc_sha1_ctx *ctx)
{
	if (ctx == NULL) {
		return;
	}
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xefcdab89;
	ctx->state[2] = 0x98badcfe;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xc3d2e1f0;
	ctx->bitlen = 0;
	ctx->datalen = 0;
}

void
lc_sha1_update(lc_sha1_ctx *ctx, const void *data, size_t len)
{
	const uint8_t	*p;
	size_t		i;

	if (ctx == NULL || (data == NULL && len != 0)) {
		return;
	}

	p = (const uint8_t *)data;
	for (i = 0; i < len; i++) {
		ctx->data[ctx->datalen++] = p[i];
		if (ctx->datalen == LC_SHA1_BLOCK_SIZE) {
			lc_sha1_transform(ctx, ctx->data);
			ctx->bitlen += LC_SHA1_BLOCK_SIZE * 8;
			ctx->datalen = 0;
		}
	}
}

void
lc_sha1_final(lc_sha1_ctx *ctx, uint8_t digest[LC_SHA1_DIGEST_SIZE])
{
	uint64_t	bitlen;
	uint32_t	i;

	if (ctx == NULL || digest == NULL) {
		return;
	}

	bitlen = ctx->bitlen + (uint64_t)ctx->datalen * 8;
	i = ctx->datalen;
	ctx->data[i++] = 0x80;

	if (i > LC_SHA1_BLOCK_SIZE - 8) {
		while (i < LC_SHA1_BLOCK_SIZE) {
			ctx->data[i++] = 0x00;
		}
		lc_sha1_transform(ctx, ctx->data);
		i = 0;
	}
	while (i < LC_SHA1_BLOCK_SIZE - 8) {
		ctx->data[i++] = 0x00;
	}
	for (i = 0; i < 8; i++) {
		ctx->data[LC_SHA1_BLOCK_SIZE - 1 - i] =
		    (uint8_t)(bitlen >> (i * 8));
	}
	lc_sha1_transform(ctx, ctx->data);

	for (i = 0; i < 5; i++) {
		digest[i * 4] = (uint8_t)(ctx->state[i] >> 24);
		digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
		digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
		digest[i * 4 + 3] = (uint8_t)ctx->state[i];
	}
	lc_wipe(ctx, sizeof(*ctx));
}

void
lc_sha1(const void *data, size_t len, uint8_t digest[LC_SHA1_DIGEST_SIZE])
{
	lc_sha1_ctx	ctx;

	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, data, len);
	lc_sha1_final(&ctx, digest);
}
