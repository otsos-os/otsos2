/* !DEFINES!

$define %type lc_sha256_ctx as SHA-256 streaming hash state
$define %type lc_sha512_ctx as SHA-512 streaming hash state
$define %type lc_hmac_sha256_ctx as HMAC-SHA256 streaming MAC state
$define %type lc_chacha20_ctx as ChaCha20 stream cipher state
$define %func lc_wipe as procedure with args void *, size_t
$define %func lc_memeq as function with args const void *, const void *, size_t
$define %func lc_random as function with args void *, size_t
$define %func lc_sha256_init as procedure with args lc_sha256_ctx *
$define %func lc_sha256_update as procedure with args lc_sha256_ctx *, const void *, size_t
$define %func lc_sha256_final as procedure with args lc_sha256_ctx *, uint8_t *
$define %func lc_sha256 as procedure with args const void *, size_t, uint8_t *
$define %func lc_sha512_init as procedure with args lc_sha512_ctx *
$define %func lc_sha512_update as procedure with args lc_sha512_ctx *, const void *, size_t
$define %func lc_sha512_final as procedure with args lc_sha512_ctx *, uint8_t *
$define %func lc_sha512 as procedure with args const void *, size_t, uint8_t *
$define %func lc_hmac_sha256_init as procedure with args lc_hmac_sha256_ctx *, const void *, size_t
$define %func lc_hmac_sha256_update as procedure with args lc_hmac_sha256_ctx *, const void *, size_t
$define %func lc_hmac_sha256_final as procedure with args lc_hmac_sha256_ctx *, uint8_t *
$define %func lc_hmac_sha256 as procedure with args key, key length, data, data length, out
$define %func lc_chacha20_init as function with args lc_chacha20_ctx *, const uint8_t *, const uint8_t *, uint32_t
$define %func lc_chacha20_set_counter as procedure with args lc_chacha20_ctx *, uint32_t
$define %func lc_chacha20_xor as function with args lc_chacha20_ctx *, const void *, void *, size_t
$define %func lc_chacha20_block as function with args key, nonce, counter, out block
$define %func lc_chacha20_wipe as procedure with args lc_chacha20_ctx *
$define %func lc_poly1305_auth as procedure with args out, data, size, key
$define %func lc_chacha20_poly1305_seal as function with args key, nonce, aad, plaintext, ciphertext, tag
$define %func lc_chacha20_poly1305_open as function with args key, nonce, aad, ciphertext, tag, plaintext
$define %func lc_curve25519_public as function with args out public, private
$define %func lc_curve25519 as function with args out shared, private, peer public
$define %func lc_ed25519_verify as function with args public key, message, message length, signature

*/

/* !SPACE!

$space %export lc_sha256_ctx, lc_sha512_ctx, lc_hmac_sha256_ctx
$space %export lc_chacha20_ctx
$space %export lc_wipe, lc_memeq, lc_random
$space %export lc_sha256_init, lc_sha256_update, lc_sha256_final
$space %export lc_sha256
$space %export lc_sha512_init, lc_sha512_update, lc_sha512_final
$space %export lc_sha512
$space %export lc_hmac_sha256_init, lc_hmac_sha256_update
$space %export lc_hmac_sha256_final, lc_hmac_sha256
$space %export lc_chacha20_init, lc_chacha20_set_counter
$space %export lc_chacha20_xor, lc_chacha20_block, lc_chacha20_wipe
$space %export lc_poly1305_auth
$space %export lc_chacha20_poly1305_seal
$space %export lc_chacha20_poly1305_open
$space %export lc_curve25519_public, lc_curve25519
$space %export lc_ed25519_verify

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

#ifndef LIBCRYPTO_H
#define LIBCRYPTO_H

#include <stddef.h>
#include <stdint.h>

#define LC_SHA256_BLOCK_SIZE		64
#define LC_SHA256_DIGEST_SIZE		32
#define LC_SHA512_BLOCK_SIZE		128
#define LC_SHA512_DIGEST_SIZE		64
#define LC_HMAC_SHA256_SIZE		32
#define LC_CHACHA20_KEY_SIZE		32
#define LC_CHACHA20_NONCE_SIZE		12
#define LC_CHACHA20_BLOCK_SIZE		64
#define LC_POLY1305_KEY_SIZE		32
#define LC_POLY1305_TAG_SIZE		16
#define LC_CHACHA20_POLY1305_KEY_SIZE	32
#define LC_CHACHA20_POLY1305_NONCE_SIZE	12
#define LC_CHACHA20_POLY1305_TAG_SIZE	16
#define LC_CURVE25519_SCALAR_SIZE	32
#define LC_CURVE25519_POINT_SIZE	32
#define LC_ED25519_PUBLIC_KEY_SIZE	32
#define LC_ED25519_SIGNATURE_SIZE	64

typedef struct lc_sha256_ctx {
	uint32_t	state[8];
	uint64_t	bitlen;
	uint8_t		data[LC_SHA256_BLOCK_SIZE];
	uint32_t	datalen;
} lc_sha256_ctx;

typedef struct lc_sha512_ctx {
	uint64_t	state[8];
	uint64_t	bitlen_hi;
	uint64_t	bitlen_lo;
	uint8_t		data[LC_SHA512_BLOCK_SIZE];
	uint32_t	datalen;
} lc_sha512_ctx;

typedef struct lc_hmac_sha256_ctx {
	lc_sha256_ctx	inner;
	lc_sha256_ctx	outer;
} lc_hmac_sha256_ctx;

typedef struct lc_chacha20_ctx {
	uint32_t	state[16];
	uint8_t		keystream[LC_CHACHA20_BLOCK_SIZE];
	uint32_t	position;
} lc_chacha20_ctx;

void	lc_wipe(void *ptr, size_t len);
int	lc_memeq(const void *a, const void *b, size_t len);
int	lc_random(void *buf, size_t len);

void	lc_sha256_init(lc_sha256_ctx *ctx);
void	lc_sha256_update(lc_sha256_ctx *ctx, const void *data,
	    size_t len);
void	lc_sha256_final(lc_sha256_ctx *ctx,
	    uint8_t digest[LC_SHA256_DIGEST_SIZE]);
void	lc_sha256(const void *data, size_t len,
	    uint8_t digest[LC_SHA256_DIGEST_SIZE]);

void	lc_sha512_init(lc_sha512_ctx *ctx);
void	lc_sha512_update(lc_sha512_ctx *ctx, const void *data,
	    size_t len);
void	lc_sha512_final(lc_sha512_ctx *ctx,
	    uint8_t digest[LC_SHA512_DIGEST_SIZE]);
void	lc_sha512(const void *data, size_t len,
	    uint8_t digest[LC_SHA512_DIGEST_SIZE]);

void	lc_hmac_sha256_init(lc_hmac_sha256_ctx *ctx, const void *key,
	    size_t key_len);
void	lc_hmac_sha256_update(lc_hmac_sha256_ctx *ctx,
	    const void *data, size_t len);
void	lc_hmac_sha256_final(lc_hmac_sha256_ctx *ctx,
	    uint8_t mac[LC_HMAC_SHA256_SIZE]);
void	lc_hmac_sha256(const void *key, size_t key_len,
	    const void *data, size_t len, uint8_t mac[LC_HMAC_SHA256_SIZE]);

int	lc_chacha20_init(lc_chacha20_ctx *ctx,
	    const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter);
void	lc_chacha20_set_counter(lc_chacha20_ctx *ctx, uint32_t counter);
int	lc_chacha20_xor(lc_chacha20_ctx *ctx, const void *in,
	    void *out, size_t len);
int	lc_chacha20_block(const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter,
	    uint8_t out[LC_CHACHA20_BLOCK_SIZE]);
void	lc_chacha20_wipe(lc_chacha20_ctx *ctx);

void	lc_poly1305_auth(uint8_t tag[LC_POLY1305_TAG_SIZE],
	    const void *data, size_t len,
	    const uint8_t key[LC_POLY1305_KEY_SIZE]);

int	lc_chacha20_poly1305_seal(
	    const uint8_t key[LC_CHACHA20_POLY1305_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_POLY1305_NONCE_SIZE],
	    const void *aad, size_t aad_len, const void *plaintext,
	    size_t plaintext_len, void *ciphertext,
	    uint8_t tag[LC_CHACHA20_POLY1305_TAG_SIZE]);
int	lc_chacha20_poly1305_open(
	    const uint8_t key[LC_CHACHA20_POLY1305_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_POLY1305_NONCE_SIZE],
	    const void *aad, size_t aad_len, const void *ciphertext,
	    size_t ciphertext_len,
	    const uint8_t tag[LC_CHACHA20_POLY1305_TAG_SIZE],
	    void *plaintext);

int	lc_curve25519_public(uint8_t out[LC_CURVE25519_POINT_SIZE],
	    const uint8_t scalar[LC_CURVE25519_SCALAR_SIZE]);
int	lc_curve25519(uint8_t out[LC_CURVE25519_POINT_SIZE],
	    const uint8_t scalar[LC_CURVE25519_SCALAR_SIZE],
	    const uint8_t point[LC_CURVE25519_POINT_SIZE]);
int	lc_ed25519_verify(const uint8_t public_key[LC_ED25519_PUBLIC_KEY_SIZE],
	    const void *message, size_t message_len,
	    const uint8_t signature[LC_ED25519_SIGNATURE_SIZE]);

#endif
