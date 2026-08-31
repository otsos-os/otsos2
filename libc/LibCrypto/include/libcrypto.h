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
$define %type lc_hmac_sha512_ctx as HMAC-SHA512 streaming MAC state
$define %func lc_hmac_sha512_init as procedure with args lc_hmac_sha512_ctx *, const void *, size_t
$define %func lc_hmac_sha512_update as procedure with args lc_hmac_sha512_ctx *, const void *, size_t
$define %func lc_hmac_sha512_final as procedure with args lc_hmac_sha512_ctx *, uint8_t *
$define %func lc_hmac_sha512 as procedure with args key, key length, data, data length, out
$define %type lc_pbkdf2_sha512_ctx as resumable PBKDF2-HMAC-SHA512 progress state
$define %func lc_pbkdf2_sha512_init as function with args ctx, password, password length, salt, salt length, iterations
$define %func lc_pbkdf2_sha512_step as function with args ctx, iteration budget
$define %func lc_pbkdf2_sha512_final as function with args ctx, out, out length
$define %func lc_pbkdf2_sha512_wipe as procedure with args lc_pbkdf2_sha512_ctx *
$define %func lc_pbkdf2_sha512 as function with args password, password length, salt, salt length, iterations, out, out length
$define %func lc_chacha20_init as function with args lc_chacha20_ctx *, const uint8_t *, const uint8_t *, uint32_t
$define %func lc_chacha20_set_counter as procedure with args lc_chacha20_ctx *, uint32_t
$define %func lc_chacha20_xor as function with args lc_chacha20_ctx *, const void *, void *, size_t
$define %func lc_chacha20_block as function with args key, nonce, counter, out block
$define %func lc_chacha20_xor64 as function with args key, nonce, counter64, input, output, length
$define %func lc_chacha20_block64 as function with args key, nonce, counter64, out block
$define %func lc_chacha20_wipe as procedure with args lc_chacha20_ctx *
$define %func lc_poly1305_auth as procedure with args out, data, size, key
$define %func lc_chacha20_poly1305_seal as function with args key, nonce, aad, plaintext, ciphertext, tag
$define %func lc_chacha20_poly1305_open as function with args key, nonce, aad, ciphertext, tag, plaintext
$define %func lc_curve25519_public as function with args out public, private
$define %func lc_curve25519 as function with args out shared, private, peer public
$define %func lc_ed25519_verify as function with args public key, message, message length, signature
$define %type lc_sha1_ctx as SHA-1 streaming hash state
$define %func lc_sha1_init as procedure with args lc_sha1_ctx *
$define %func lc_sha1_update as procedure with args lc_sha1_ctx *, const void *, size_t
$define %func lc_sha1_final as procedure with args lc_sha1_ctx *, uint8_t *
$define %func lc_sha1 as procedure with args const void *, size_t, uint8_t *
$define %type lc_aes_ctx as AES expanded key schedule
$define %func lc_aes_init as function with args lc_aes_ctx *, const void *, uint32_t
$define %func lc_aes_wipe as procedure with args lc_aes_ctx *
$define %func lc_aes_ige_encrypt as function with args ctx, iv, in, out, len
$define %func lc_aes_ige_decrypt as function with args ctx, iv, in, out, len
$define %type lc_bn as fixed capacity big unsigned integer
$define %func lc_bn_zero as procedure with args lc_bn *
$define %func lc_bn_wipe as procedure with args lc_bn *
$define %func lc_bn_from_bytes as function with args lc_bn *, const void *, size_t
$define %func lc_bn_to_bytes as function with args const lc_bn *, void *, size_t
$define %func lc_bn_cmp as function with args const lc_bn *, const lc_bn *
$define %func lc_bn_cmp_u32 as function with args const lc_bn *, uint32_t
$define %func lc_bn_bits as function with args const lc_bn *
$define %func lc_bn_is_odd as function with args const lc_bn *
$define %func lc_bn_sub as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func lc_bn_sub_u32 as function with args lc_bn *, const lc_bn *, uint32_t
$define %func lc_bn_rshift1 as procedure with args lc_bn *, const lc_bn *
$define %func lc_bn_mod_exp as function with args result, base, exponent, modulus
$define %func lc_bn_is_prime as function with args const lc_bn *
$define %type lc_bn_prime_ctx as resumable Miller-Rabin primality state
$define %func lc_bn_is_prime_init as function with args ctx, n
$define %func lc_bn_is_prime_step as function with args lc_bn_prime_ctx *
$define %func lc_bn_is_prime_result as function with args const lc_bn_prime_ctx *
$define %func lc_bn_is_prime_wipe as procedure with args lc_bn_prime_ctx *
$define %func lc_bn_add as function with args result, a, b
$define %func lc_bn_mul as function with args result, a, b
$define %func lc_bn_mod_mul as function with args result, a, b, modulus
$define %func lc_bn_mod_sub as function with args result, a, b, modulus
$define %func lc_rsa_public as function with args modulus, exponent, in, out, len

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
$space %export lc_hmac_sha512_ctx
$space %export lc_hmac_sha512_init, lc_hmac_sha512_update
$space %export lc_hmac_sha512_final, lc_hmac_sha512
$space %export lc_pbkdf2_sha512_ctx
$space %export lc_pbkdf2_sha512_init, lc_pbkdf2_sha512_step
$space %export lc_pbkdf2_sha512_final, lc_pbkdf2_sha512_wipe
$space %export lc_pbkdf2_sha512
$space %export lc_chacha20_init, lc_chacha20_set_counter
$space %export lc_chacha20_xor, lc_chacha20_block, lc_chacha20_wipe
$space %export lc_chacha20_xor64, lc_chacha20_block64
$space %export lc_poly1305_auth
$space %export lc_chacha20_poly1305_seal
$space %export lc_chacha20_poly1305_open
$space %export lc_curve25519_public, lc_curve25519
$space %export lc_ed25519_verify
$space %export lc_sha1_ctx, lc_sha1_init, lc_sha1_update
$space %export lc_sha1_final, lc_sha1
$space %export lc_aes_ctx, lc_aes_init, lc_aes_wipe
$space %export lc_aes_ige_encrypt, lc_aes_ige_decrypt
$space %export lc_bn, lc_bn_zero, lc_bn_wipe
$space %export lc_bn_from_bytes, lc_bn_to_bytes
$space %export lc_bn_cmp, lc_bn_cmp_u32, lc_bn_bits, lc_bn_is_odd
$space %export lc_bn_sub, lc_bn_sub_u32, lc_bn_rshift1
$space %export lc_bn_mod_exp, lc_bn_is_prime
$space %export lc_bn_prime_ctx, lc_bn_is_prime_init, lc_bn_is_prime_step
$space %export lc_bn_is_prime_result, lc_bn_is_prime_wipe
$space %export lc_bn_add, lc_bn_mul, lc_bn_mod_mul, lc_bn_mod_sub
$space %export lc_rsa_public

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
#define LC_HMAC_SHA512_SIZE		64
#define LC_CHACHA20_KEY_SIZE		32
#define LC_CHACHA20_NONCE_SIZE		12
#define LC_CHACHA20_NONCE64_SIZE	8
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
#define LC_SHA1_BLOCK_SIZE		64
#define LC_SHA1_DIGEST_SIZE		20
#define LC_AES_BLOCK_SIZE		16
#define LC_AES_MAX_KEY_SIZE		32
#define LC_AES_IGE_IV_SIZE		32
#define LC_BN_LIMBS			68
#define LC_BN_MAX_BYTES			(LC_BN_LIMBS * 4)

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

typedef struct lc_hmac_sha512_ctx {
	lc_sha512_ctx	inner;
	lc_sha512_ctx	outer;
} lc_hmac_sha512_ctx;


typedef struct lc_pbkdf2_sha512_ctx {
	lc_hmac_sha512_ctx	key;
	uint8_t			u[LC_SHA512_DIGEST_SIZE];
	uint8_t			out[LC_SHA512_DIGEST_SIZE];
	uint32_t		iterations;
	uint32_t		done;
} lc_pbkdf2_sha512_ctx;

typedef struct lc_chacha20_ctx {
	uint32_t	state[16];
	uint8_t		keystream[LC_CHACHA20_BLOCK_SIZE];
	uint32_t	position;
} lc_chacha20_ctx;

typedef struct lc_sha1_ctx {
	uint32_t	state[5];
	uint64_t	bitlen;
	uint8_t		data[LC_SHA1_BLOCK_SIZE];
	uint32_t	datalen;
} lc_sha1_ctx;

typedef struct lc_aes_ctx {
	uint8_t		key[240];
	uint32_t	rounds;
} lc_aes_ctx;


typedef struct lc_bn {
	uint32_t	limb[LC_BN_LIMBS];
	uint32_t	used;
} lc_bn;


typedef struct lc_bn_prime_ctx {
	lc_bn		n;
	lc_bn		n1;
	lc_bn		d;
	lc_bn		r2;
	uint32_t	n0;
	int		s;
	uint32_t	next_base;
	int		verdict;
} lc_bn_prime_ctx;

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

void	lc_hmac_sha512_init(lc_hmac_sha512_ctx *ctx, const void *key,
	    size_t key_len);
void	lc_hmac_sha512_update(lc_hmac_sha512_ctx *ctx, const void *data,
	    size_t len);
void	lc_hmac_sha512_final(lc_hmac_sha512_ctx *ctx,
	    uint8_t mac[LC_HMAC_SHA512_SIZE]);
void	lc_hmac_sha512(const void *key, size_t key_len,
	    const void *data, size_t len, uint8_t mac[LC_HMAC_SHA512_SIZE]);

int	lc_pbkdf2_sha512_init(lc_pbkdf2_sha512_ctx *ctx, const void *password,
	    size_t password_len, const void *salt, size_t salt_len,
	    uint32_t iterations);
int	lc_pbkdf2_sha512_step(lc_pbkdf2_sha512_ctx *ctx, uint32_t budget);
int	lc_pbkdf2_sha512_final(lc_pbkdf2_sha512_ctx *ctx, uint8_t *out,
	    size_t out_len);
void	lc_pbkdf2_sha512_wipe(lc_pbkdf2_sha512_ctx *ctx);
int	lc_pbkdf2_sha512(const void *password, size_t password_len,
	    const void *salt, size_t salt_len, uint32_t iterations,
	    uint8_t *out, size_t out_len);

int	lc_chacha20_init(lc_chacha20_ctx *ctx,
	    const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter);
void	lc_chacha20_set_counter(lc_chacha20_ctx *ctx, uint32_t counter);
int	lc_chacha20_xor(lc_chacha20_ctx *ctx, const void *in,
	    void *out, size_t len);
int	lc_chacha20_block(const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE_SIZE], uint32_t counter,
	    uint8_t out[LC_CHACHA20_BLOCK_SIZE]);
int	lc_chacha20_xor64(const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE64_SIZE],
	    uint64_t counter, const void *in, void *out, size_t len);
int	lc_chacha20_block64(const uint8_t key[LC_CHACHA20_KEY_SIZE],
	    const uint8_t nonce[LC_CHACHA20_NONCE64_SIZE],
	    uint64_t counter, uint8_t out[LC_CHACHA20_BLOCK_SIZE]);
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

void	lc_sha1_init(lc_sha1_ctx *ctx);
void	lc_sha1_update(lc_sha1_ctx *ctx, const void *data, size_t len);
void	lc_sha1_final(lc_sha1_ctx *ctx, uint8_t digest[LC_SHA1_DIGEST_SIZE]);
void	lc_sha1(const void *data, size_t len,
	    uint8_t digest[LC_SHA1_DIGEST_SIZE]);
int	lc_aes_init(lc_aes_ctx *ctx, const void *key, uint32_t key_len);
void	lc_aes_wipe(lc_aes_ctx *ctx);
int	lc_aes_ige_encrypt(lc_aes_ctx *ctx, uint8_t iv[LC_AES_IGE_IV_SIZE],
	    const void *in, void *out, size_t len);
int	lc_aes_ige_decrypt(lc_aes_ctx *ctx, uint8_t iv[LC_AES_IGE_IV_SIZE],
	    const void *in, void *out, size_t len);

void	lc_bn_zero(lc_bn *a);
void	lc_bn_wipe(lc_bn *a);
int	lc_bn_from_bytes(lc_bn *a, const void *buf, size_t len);
int	lc_bn_to_bytes(const lc_bn *a, void *buf, size_t len);
int	lc_bn_cmp(const lc_bn *a, const lc_bn *b);
int	lc_bn_cmp_u32(const lc_bn *a, uint32_t v);
int	lc_bn_bits(const lc_bn *a);
int	lc_bn_is_odd(const lc_bn *a);
int	lc_bn_sub(lc_bn *r, const lc_bn *a, const lc_bn *b);
int	lc_bn_sub_u32(lc_bn *r, const lc_bn *a, uint32_t v);
void	lc_bn_rshift1(lc_bn *r, const lc_bn *a);
int	lc_bn_mod_exp(lc_bn *r, const lc_bn *base, const lc_bn *exp,
	    const lc_bn *mod);
int	lc_bn_is_prime(const lc_bn *n);

int	lc_bn_is_prime_init(lc_bn_prime_ctx *ctx, const lc_bn *n);
int	lc_bn_is_prime_step(lc_bn_prime_ctx *ctx);
int	lc_bn_is_prime_result(const lc_bn_prime_ctx *ctx);
void	lc_bn_is_prime_wipe(lc_bn_prime_ctx *ctx);
int	lc_bn_add(lc_bn *r, const lc_bn *a, const lc_bn *b);
int	lc_bn_mul(lc_bn *r, const lc_bn *a, const lc_bn *b);
int	lc_bn_mod_mul(lc_bn *r, const lc_bn *a, const lc_bn *b,
	    const lc_bn *mod);
int	lc_bn_mod_sub(lc_bn *r, const lc_bn *a, const lc_bn *b,
	    const lc_bn *mod);

int	lc_rsa_public(const void *modulus, size_t modulus_len,
	    const void *exponent, size_t exponent_len, const void *in,
	    void *out, size_t len);

#endif
