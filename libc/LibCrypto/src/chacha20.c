/* !DEFINES!

$define %type lc_chacha20_ctx as ChaCha20 stream cipher state
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type size_t as object size
$define %func rotl32 as function with args uint32_t, uint32_t
$define %func lc_chacha20_rounds as procedure with args const uint32_t *, uint8_t *
$define %func lc_chacha20_refill as procedure with args lc_chacha20_ctx *
$define %func lc_chacha20_init as function with args lc_chacha20_ctx *, const uint8_t *, const uint8_t *, uint32_t
$define %func lc_chacha20_set_counter as procedure with args lc_chacha20_ctx *, uint32_t
$define %func lc_chacha20_xor as function with args lc_chacha20_ctx *, const void *, void *, size_t
$define %func lc_chacha20_block as function with args key, nonce, counter, out block
$define %func lc_chacha20_wipe as procedure with args lc_chacha20_ctx *

*/

/* !SPACE!

$space %internal rotl32, lc_chacha20_rounds, lc_chacha20_refill
$space %export lc_chacha20_init, lc_chacha20_set_counter
$space %export lc_chacha20_xor, lc_chacha20_block, lc_chacha20_wipe

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

#include <errno.h>
#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

static uint32_t
rotl32(uint32_t x, uint32_t n)
{
	return ((x << n) | (x >> (32 - n)));
}

static void
lc_chacha20_rounds(const uint32_t input[16],
    uint8_t out[LC_CHACHA20_BLOCK_SIZE])
{
	uint32_t	x[16];
	int	i;

	for (i = 0; i < 16; i++) {
		x[i] = input[i];
	}

#define LC_QR(a, b, c, d)				\
	do {						\
		a += b; d ^= a; d = rotl32(d, 16);	\
		c += d; b ^= c; b = rotl32(b, 12);	\
		a += b; d ^= a; d = rotl32(d, 8);	\
		c += d; b ^= c; b = rotl32(b, 7);	\
	} while (0)

	for (i = 0; i < 10; i++) {
		LC_QR(x[0], x[4], x[8], x[12]);
		LC_QR(x[1], x[5], x[9], x[13]);
		LC_QR(x[2], x[6], x[10], x[14]);
		LC_QR(x[3], x[7], x[11], x[15]);
		LC_QR(x[0], x[5], x[10], x[15]);
		LC_QR(x[1], x[6], x[11], x[12]);
		LC_QR(x[2], x[7], x[8], x[13]);
		LC_QR(x[3], x[4], x[9], x[14]);
	}

#undef LC_QR

	for (i = 0; i < 16; i++) {
		x[i] += input[i];
		lc_store32_le(out + i * 4, x[i]);
	}
	lc_wipe(x, sizeof(x));
}

static void
lc_chacha20_refill(lc_chacha20_ctx *ctx)
{
	lc_chacha20_rounds(ctx->state, ctx->keystream);
	ctx->state[12]++;
	ctx->position = 0;
}

int
lc_chacha20_init(lc_chacha20_ctx *ctx,
    const uint8_t key[LC_CHACHA20_KEY_SIZE],
    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter)
{
	int	i;

	if (!ctx || !key || !nonce) {
		errno = EINVAL;
		return (-1);
	}
	ctx->state[0] = 0x61707865;
	ctx->state[1] = 0x3320646e;
	ctx->state[2] = 0x79622d32;
	ctx->state[3] = 0x6b206574;
	for (i = 0; i < 8; i++) {
		ctx->state[4 + i] = lc_load32_le(key + i * 4);
	}
	ctx->state[12] = counter;
	ctx->state[13] = lc_load32_le(nonce);
	ctx->state[14] = lc_load32_le(nonce + 4);
	ctx->state[15] = lc_load32_le(nonce + 8);
	memset(ctx->keystream, 0, sizeof(ctx->keystream));
	ctx->position = LC_CHACHA20_BLOCK_SIZE;
	return (0);
}

void
lc_chacha20_set_counter(lc_chacha20_ctx *ctx, uint32_t counter)
{
	if (!ctx) {
		return;
	}
	ctx->state[12] = counter;
	ctx->position = LC_CHACHA20_BLOCK_SIZE;
}

int
lc_chacha20_xor(lc_chacha20_ctx *ctx, const void *in, void *out,
    size_t len)
{
	const uint8_t	*src;
	uint8_t		*dst;
	size_t		i;

	if (!ctx || (!in && len != 0) || (!out && len != 0)) {
		errno = EINVAL;
		return (-1);
	}
	src = (const uint8_t *)in;
	dst = (uint8_t *)out;
	for (i = 0; i < len; i++) {
		if (ctx->position >= LC_CHACHA20_BLOCK_SIZE) {
			lc_chacha20_refill(ctx);
		}
		dst[i] = src[i] ^ ctx->keystream[ctx->position];
		ctx->position++;
	}
	return (0);
}

int
lc_chacha20_block(const uint8_t key[LC_CHACHA20_KEY_SIZE],
    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter,
    uint8_t out[LC_CHACHA20_BLOCK_SIZE])
{
	lc_chacha20_ctx	ctx;

	if (!out) {
		errno = EINVAL;
		return (-1);
	}
	if (lc_chacha20_init(&ctx, key, nonce, counter) != 0) {
		return (-1);
	}
	lc_chacha20_refill(&ctx);
	memcpy(out, ctx.keystream, LC_CHACHA20_BLOCK_SIZE);
	lc_chacha20_wipe(&ctx);
	return (0);
}

void
lc_chacha20_wipe(lc_chacha20_ctx *ctx)
{
	if (!ctx) {
		return;
	}
	lc_wipe(ctx, sizeof(*ctx));
}
