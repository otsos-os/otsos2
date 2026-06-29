/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type chacha20_ctx_t as struct with state, counter, keystream, position

$define %func load32_le as function with args const u8 *
$define %func store32_le as procedure with args u8 *, u32
$define %func chacha20_block as procedure with args const u32 *, u8 *
$define %func chacha20_refill as procedure with args chacha20_ctx_t *
$define %func chacha20_init as procedure with args chacha20_ctx_t *, const u8 *, const u8 *
$define %func chacha20_set_counter as procedure with args chacha20_ctx_t *, u64
$define %func chacha20_xor as procedure with args chacha20_ctx_t *, const u8 *, u8 *, u32
$define %func chacha20_encrypt as function with args const u8 *, const u8 *, const u8 *, u32, u8 *
$define %func chacha20_decrypt as function with args const u8 *, const u8 *, const u8 *, u32, u8 *
$define %func chacha20_wipe as procedure with args chacha20_ctx_t *

*/

/* !SPACE!

$space %internal load32_le, store32_le, chacha20_block, chacha20_refill
$space %export chacha20_init, chacha20_set_counter, chacha20_xor
$space %export chacha20_encrypt, chacha20_decrypt, chacha20_wipe

*/

/*
 * ChaCha20 stream cipher (RFC 8439).
 *
 * 256-bit key, 96-bit nonce, 64-bit counter.
 * The quarter-round is the standard ARX construction:
 *
 *   a += b; d ^= a; d = rotl32(d, 16);
 *   c += d; b ^= c; b = rotl32(b, 12);
 *   a += b; d ^= a; d = rotl32(d,  8);
 *   c += d; b ^= c; b = rotl32(b,  7);
 *
 * 20 rounds = 10 double-rounds.  The state is 16 x u32 words:
 *
 *   state[ 0..3]  = "expa" "nd 3" "2-by" "te k"   (constants)
 *   state[ 4..11] = key (8 words)
 *   state[12..13] = counter (little-endian, 64-bit)
 *   state[14..15] = nonce (2 words, little-endian)
 */

#include <kernel/crypto/cipher/chacha20.h>
#include <kernel/crypto/util/crypto_util.h>

#define ROTL32(x, n) \
	(((x) << (n)) | ((x) >> (32 - (n))))

static inline u32
load32_le(const u8 *src)
{
	return ((u32)src[0] | ((u32)src[1] << 8) |
	    ((u32)src[2] << 16) | ((u32)src[3] << 24));
}

static inline void
store32_le(u8 *dst, u32 val)
{
	dst[0] = (u8)(val);
	dst[1] = (u8)(val >> 8);
	dst[2] = (u8)(val >> 16);
	dst[3] = (u8)(val >> 24);
}

static const u32 CHACHA_CONST[4] = {
	0x61707865, /* "expa" */
	0x3320646e, /* "nd 3" */
	0x79622d32, /* "2-by" */
	0x6b206574  /* "te k" */
};

static void
chacha20_block(const u32 input[16], u8 out[CHACHA20_BLOCK_SIZE])
{
	u32	x[16];
	int	i;

	for (i = 0; i < 16; i++) {
		x[i] = input[i];
	}

#define QR(a, b, c, d)				\
	do {					\
		a += b;  d ^= a;  d = ROTL32(d, 16);	\
		c += d;  b ^= c;  b = ROTL32(b, 12);	\
		a += b;  d ^= a;  d = ROTL32(d,  8);	\
		c += d;  b ^= c;  b = ROTL32(b,  7);	\
	} while (0)

	for (i = 0; i < 10; i++) {
		QR(x[0], x[4], x[ 8], x[12]);
		QR(x[1], x[5], x[ 9], x[13]);
		QR(x[2], x[6], x[10], x[14]);
		QR(x[3], x[7], x[11], x[15]);
		QR(x[0], x[5], x[10], x[15]);
		QR(x[1], x[6], x[11], x[12]);
		QR(x[2], x[7], x[ 8], x[13]);
		QR(x[3], x[4], x[ 9], x[14]);
	}

#undef QR

	for (i = 0; i < 16; i++) {
		x[i] += input[i];
		store32_le(out + i * 4, x[i]);
	}

	crypto_secure_wipe(x, sizeof(x));
}

static void
chacha20_refill(chacha20_ctx_t *ctx)
{
	chacha20_block(ctx->state, ctx->keystream);

	ctx->counter++;
	ctx->state[12] = (u32)(ctx->counter & 0xFFFFFFFF);
	ctx->state[13] = (u32)(ctx->counter >> 32);

	ctx->position = 0;
}

void
chacha20_init(chacha20_ctx_t *ctx, const u8 *key, const u8 *nonce)
{
	int	i;

	if (!ctx || !key || !nonce) {
		return;
	}

	ctx->state[0] = CHACHA_CONST[0];
	ctx->state[1] = CHACHA_CONST[1];
	ctx->state[2] = CHACHA_CONST[2];
	ctx->state[3] = CHACHA_CONST[3];

	for (i = 0; i < 8; i++) {
		ctx->state[4 + i] = load32_le(key + i * 4);
	}

	ctx->counter = 0;
	ctx->state[12] = 0;
	ctx->state[13] = 0;

	ctx->state[14] = load32_le(nonce + 0);
	ctx->state[15] = load32_le(nonce + 8);

	ctx->position = CHACHA20_BLOCK_SIZE;
	memset(ctx->keystream, 0, CHACHA20_BLOCK_SIZE);
}

void
chacha20_set_counter(chacha20_ctx_t *ctx, u64 counter)
{
	if (!ctx) {
		return;
	}

	ctx->counter = counter;
	ctx->state[12] = (u32)(counter & 0xFFFFFFFF);
	ctx->state[13] = (u32)(counter >> 32);
	ctx->position = CHACHA20_BLOCK_SIZE;
}

void
chacha20_xor(chacha20_ctx_t *ctx, const u8 *in, u8 *out, u32 len)
{
	u32	i;

	if (!ctx || !in || !out || len == 0) {
		return;
	}

	for (i = 0; i < len; i++) {
		if (ctx->position >= CHACHA20_BLOCK_SIZE) {
			chacha20_refill(ctx);
		}
		out[i] = in[i] ^ ctx->keystream[ctx->position];
		ctx->position++;
	}
}

int
chacha20_encrypt(const u8 *key, const u8 *nonce,
    const u8 *plaintext, u32 len, u8 *ciphertext)
{
	chacha20_ctx_t	ctx;

	if (!key || !nonce || !plaintext || !ciphertext) {
		return (-1);
	}

	chacha20_init(&ctx, key, nonce);
	chacha20_xor(&ctx, plaintext, ciphertext, len);
	chacha20_wipe(&ctx);
	return (0);
}

int
chacha20_decrypt(const u8 *key, const u8 *nonce,
    const u8 *ciphertext, u32 len, u8 *plaintext)
{
	/*
	 * ChaCha20 is a stream cipher: encryption and decryption are
	 * the same XOR operation with the keystream.
	 */
	return (chacha20_encrypt(key, nonce, ciphertext, len,
	    plaintext));
}

void
chacha20_wipe(chacha20_ctx_t *ctx)
{
	if (!ctx) {
		return;
	}

	crypto_secure_wipe(ctx->state, sizeof(ctx->state));
	crypto_secure_wipe(ctx->keystream, sizeof(ctx->keystream));
	ctx->counter = 0;
	ctx->position = 0;
}
