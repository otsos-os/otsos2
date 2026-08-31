/* !DEFINES!

$define %type lc_hmac_sha512_ctx as HMAC-SHA512 streaming MAC state
$define %type uint8_t as 8 bit unsigned
$define %type size_t as object size
$define %func lc_hmac_sha512_init as procedure with args lc_hmac_sha512_ctx *, const void *, size_t
$define %func lc_hmac_sha512_update as procedure with args lc_hmac_sha512_ctx *, const void *, size_t
$define %func lc_hmac_sha512_final as procedure with args lc_hmac_sha512_ctx *, uint8_t *
$define %func lc_hmac_sha512 as procedure with args key, key length, data, data length, out

*/

/* !SPACE!

$space %export lc_hmac_sha512_init, lc_hmac_sha512_update
$space %export lc_hmac_sha512_final, lc_hmac_sha512

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

void
lc_hmac_sha512_init(lc_hmac_sha512_ctx *ctx, const void *key, size_t key_len)
{
	uint8_t	block[LC_SHA512_BLOCK_SIZE];
	uint8_t	digest[LC_SHA512_DIGEST_SIZE];
	uint8_t	ipad[LC_SHA512_BLOCK_SIZE];
	uint8_t	opad[LC_SHA512_BLOCK_SIZE];
	size_t	i;

	if (!ctx || (!key && key_len != 0)) {
		return;
	}
	memset(block, 0, sizeof(block));
	if (key_len > LC_SHA512_BLOCK_SIZE) {
		lc_sha512(key, key_len, digest);
		memcpy(block, digest, LC_SHA512_DIGEST_SIZE);
	} else if (key_len != 0) {
		memcpy(block, key, key_len);
	}
	for (i = 0; i < LC_SHA512_BLOCK_SIZE; i++) {
		ipad[i] = block[i] ^ 0x36;
		opad[i] = block[i] ^ 0x5c;
	}

	lc_sha512_init(&ctx->inner);
	lc_sha512_update(&ctx->inner, ipad, sizeof(ipad));
	lc_sha512_init(&ctx->outer);
	lc_sha512_update(&ctx->outer, opad, sizeof(opad));

	lc_wipe(block, sizeof(block));
	lc_wipe(digest, sizeof(digest));
	lc_wipe(ipad, sizeof(ipad));
	lc_wipe(opad, sizeof(opad));
}

void
lc_hmac_sha512_update(lc_hmac_sha512_ctx *ctx, const void *data, size_t len)
{
	if (!ctx) {
		return;
	}
	lc_sha512_update(&ctx->inner, data, len);
}

void
lc_hmac_sha512_final(lc_hmac_sha512_ctx *ctx,
    uint8_t mac[LC_HMAC_SHA512_SIZE])
{
	uint8_t	inner[LC_SHA512_DIGEST_SIZE];

	if (!ctx || !mac) {
		return;
	}
	lc_sha512_final(&ctx->inner, inner);
	lc_sha512_update(&ctx->outer, inner, sizeof(inner));
	lc_sha512_final(&ctx->outer, mac);
	lc_wipe(inner, sizeof(inner));
	lc_wipe(ctx, sizeof(*ctx));
}

void
lc_hmac_sha512(const void *key, size_t key_len, const void *data,
    size_t len, uint8_t mac[LC_HMAC_SHA512_SIZE])
{
	lc_hmac_sha512_ctx	ctx;

	lc_hmac_sha512_init(&ctx, key, key_len);
	lc_hmac_sha512_update(&ctx, data, len);
	lc_hmac_sha512_final(&ctx, mac);
}
