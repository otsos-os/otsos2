/* !DEFINES!

$define %type lc_sha512_ctx as SHA-512 streaming hash state
$define %type uint8_t as 8 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %func rotr64 as function with args uint64_t, uint64_t
$define %func lc_sha512_add_bits as procedure with args lc_sha512_ctx *, uint64_t
$define %func lc_sha512_transform as procedure with args lc_sha512_ctx *, const uint8_t *
$define %func lc_sha512_init as procedure with args lc_sha512_ctx *
$define %func lc_sha512_update as procedure with args lc_sha512_ctx *, const void *, size_t
$define %func lc_sha512_final as procedure with args lc_sha512_ctx *, uint8_t *
$define %func lc_sha512 as procedure with args const void *, size_t, uint8_t *

*/

/* !SPACE!

$space %internal rotr64, lc_sha512_add_bits, lc_sha512_transform
$space %export lc_sha512_init, lc_sha512_update, lc_sha512_final
$space %export lc_sha512

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

static const uint64_t	g_sha512_k[80] = {
	0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
	0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
	0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
	0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
	0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
	0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
	0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
	0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
	0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
	0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
	0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
	0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
	0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
	0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
	0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
	0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
	0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
	0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
	0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
	0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
	0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
	0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
	0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
	0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
	0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
	0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
	0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
	0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
	0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
	0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
	0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
	0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
	0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
	0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
	0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
	0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
	0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
	0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
	0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
	0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static uint64_t
rotr64(uint64_t x, uint64_t n)
{
	return ((x >> n) | (x << (64 - n)));
}

static void
lc_sha512_add_bits(lc_sha512_ctx *ctx, uint64_t bits)
{
	uint64_t	old;

	old = ctx->bitlen_lo;
	ctx->bitlen_lo += bits;
	if (ctx->bitlen_lo < old) {
		ctx->bitlen_hi++;
	}
}

static void
lc_sha512_transform(lc_sha512_ctx *ctx, const uint8_t *block)
{
	uint64_t	w[80];
	uint64_t	a, b, c, d, e, f, g, h;
	uint64_t	t1, t2;
	int		i;

	for (i = 0; i < 16; i++) {
		w[i] = ((uint64_t)block[i * 8] << 56) |
		    ((uint64_t)block[i * 8 + 1] << 48) |
		    ((uint64_t)block[i * 8 + 2] << 40) |
		    ((uint64_t)block[i * 8 + 3] << 32) |
		    ((uint64_t)block[i * 8 + 4] << 24) |
		    ((uint64_t)block[i * 8 + 5] << 16) |
		    ((uint64_t)block[i * 8 + 6] << 8) |
		    (uint64_t)block[i * 8 + 7];
	}
	for (i = 16; i < 80; i++) {
		w[i] = (rotr64(w[i - 2], 19) ^ rotr64(w[i - 2], 61) ^
		    (w[i - 2] >> 6)) + w[i - 7] +
		    (rotr64(w[i - 15], 1) ^ rotr64(w[i - 15], 8) ^
		    (w[i - 15] >> 7)) + w[i - 16];
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 80; i++) {
		t1 = h + (rotr64(e, 14) ^ rotr64(e, 18) ^
		    rotr64(e, 41)) + ((e & f) ^ (~e & g)) +
		    g_sha512_k[i] + w[i];
		t2 = (rotr64(a, 28) ^ rotr64(a, 34) ^
		    rotr64(a, 39)) + ((a & b) ^ (a & c) ^ (b & c));
		h = g;
		g = f;
		f = e;
		e = d + t1;
		d = c;
		c = b;
		b = a;
		a = t1 + t2;
	}

	ctx->state[0] += a;
	ctx->state[1] += b;
	ctx->state[2] += c;
	ctx->state[3] += d;
	ctx->state[4] += e;
	ctx->state[5] += f;
	ctx->state[6] += g;
	ctx->state[7] += h;
	lc_wipe(w, sizeof(w));
}

void
lc_sha512_init(lc_sha512_ctx *ctx)
{
	if (!ctx) {
		return;
	}
	ctx->state[0] = 0x6a09e667f3bcc908ULL;
	ctx->state[1] = 0xbb67ae8584caa73bULL;
	ctx->state[2] = 0x3c6ef372fe94f82bULL;
	ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
	ctx->state[4] = 0x510e527fade682d1ULL;
	ctx->state[5] = 0x9b05688c2b3e6c1fULL;
	ctx->state[6] = 0x1f83d9abfb41bd6bULL;
	ctx->state[7] = 0x5be0cd19137e2179ULL;
	ctx->bitlen_hi = 0;
	ctx->bitlen_lo = 0;
	ctx->datalen = 0;
	memset(ctx->data, 0, sizeof(ctx->data));
}

void
lc_sha512_update(lc_sha512_ctx *ctx, const void *data, size_t len)
{
	const uint8_t	*p;
	size_t		i;

	if (!ctx || (!data && len != 0)) {
		return;
	}
	p = (const uint8_t *)data;
	for (i = 0; i < len; i++) {
		ctx->data[ctx->datalen] = p[i];
		ctx->datalen++;
		if (ctx->datalen == LC_SHA512_BLOCK_SIZE) {
			lc_sha512_transform(ctx, ctx->data);
			lc_sha512_add_bits(ctx, 1024);
			ctx->datalen = 0;
		}
	}
}

void
lc_sha512_final(lc_sha512_ctx *ctx, uint8_t digest[LC_SHA512_DIGEST_SIZE])
{
	uint64_t	hi, lo, old;
	uint32_t	i;

	if (!ctx || !digest) {
		return;
	}
	hi = ctx->bitlen_hi;
	lo = ctx->bitlen_lo;
	old = lo;
	lo += (uint64_t)ctx->datalen * 8;
	if (lo < old) {
		hi++;
	}

	ctx->data[ctx->datalen] = 0x80;
	ctx->datalen++;
	if (ctx->datalen > 112) {
		while (ctx->datalen < LC_SHA512_BLOCK_SIZE) {
			ctx->data[ctx->datalen] = 0;
			ctx->datalen++;
		}
		lc_sha512_transform(ctx, ctx->data);
		ctx->datalen = 0;
	}
	while (ctx->datalen < 112) {
		ctx->data[ctx->datalen] = 0;
		ctx->datalen++;
	}
	for (i = 0; i < 8; i++) {
		ctx->data[119 - i] = (uint8_t)(hi >> (i * 8));
		ctx->data[127 - i] = (uint8_t)(lo >> (i * 8));
	}
	lc_sha512_transform(ctx, ctx->data);

	for (i = 0; i < 8; i++) {
		digest[i * 8] = (uint8_t)(ctx->state[i] >> 56);
		digest[i * 8 + 1] = (uint8_t)(ctx->state[i] >> 48);
		digest[i * 8 + 2] = (uint8_t)(ctx->state[i] >> 40);
		digest[i * 8 + 3] = (uint8_t)(ctx->state[i] >> 32);
		digest[i * 8 + 4] = (uint8_t)(ctx->state[i] >> 24);
		digest[i * 8 + 5] = (uint8_t)(ctx->state[i] >> 16);
		digest[i * 8 + 6] = (uint8_t)(ctx->state[i] >> 8);
		digest[i * 8 + 7] = (uint8_t)ctx->state[i];
	}
	lc_wipe(ctx, sizeof(*ctx));
}

void
lc_sha512(const void *data, size_t len,
    uint8_t digest[LC_SHA512_DIGEST_SIZE])
{
	lc_sha512_ctx	ctx;

	lc_sha512_init(&ctx);
	lc_sha512_update(&ctx, data, len);
	lc_sha512_final(&ctx, digest);
}
