/* !DEFINES!

$define %type lc_pbkdf2_sha512_ctx as resumable PBKDF2-HMAC-SHA512 progress state
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type size_t as object size
$define %func lc_pbkdf2_sha512_init as function with args ctx, password, password length, salt, salt length, iterations
$define %func lc_pbkdf2_sha512_step as function with args ctx, iteration budget
$define %func lc_pbkdf2_sha512_final as function with args ctx, out, out length
$define %func lc_pbkdf2_sha512 as function with args password, password length, salt, salt length, iterations, out, out length

*/

/* !SPACE!

$space %export lc_pbkdf2_sha512_init, lc_pbkdf2_sha512_step
$space %export lc_pbkdf2_sha512_final, lc_pbkdf2_sha512

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
lc_pbkdf2_sha512_wipe(lc_pbkdf2_sha512_ctx *ctx)
{
	if (ctx != NULL) {
		lc_wipe(ctx, sizeof(*ctx));
	}
}

int
lc_pbkdf2_sha512_init(lc_pbkdf2_sha512_ctx *ctx, const void *password,
    size_t password_len, const void *salt, size_t salt_len,
    uint32_t iterations)
{
	lc_hmac_sha512_ctx	h;
	uint8_t			counter[4];

	if (ctx == NULL || (password == NULL && password_len != 0) ||
	    (salt == NULL && salt_len != 0)) {
		return (-1);
	}
	if (iterations == 0) {
		return (-1);
	}
	memset(ctx, 0, sizeof(*ctx));


	lc_hmac_sha512_init(&ctx->key, password, password_len);


	counter[0] = 0;
	counter[1] = 0;
	counter[2] = 0;
	counter[3] = 1;

	memcpy(&h, &ctx->key, sizeof(h));
	lc_hmac_sha512_update(&h, salt, salt_len);
	lc_hmac_sha512_update(&h, counter, sizeof(counter));
	lc_hmac_sha512_final(&h, ctx->u);

	memcpy(ctx->out, ctx->u, LC_SHA512_DIGEST_SIZE);
	ctx->iterations = iterations;
	ctx->done = 1;
	return (0);
}


int
lc_pbkdf2_sha512_step(lc_pbkdf2_sha512_ctx *ctx, uint32_t budget)
{
	lc_hmac_sha512_ctx	h;
	uint32_t		n;
	size_t			i;

	if (ctx == NULL || budget == 0 || ctx->iterations == 0) {
		return (-1);
	}
	if (ctx->done >= ctx->iterations) {
		return (1);
	}
	n = ctx->iterations - ctx->done;
	if (n > budget) {
		n = budget;
	}
	while (n-- != 0) {
		memcpy(&h, &ctx->key, sizeof(h));
		lc_hmac_sha512_update(&h, ctx->u, LC_SHA512_DIGEST_SIZE);
		lc_hmac_sha512_final(&h, ctx->u);
		for (i = 0; i < LC_SHA512_DIGEST_SIZE; i++) {
			ctx->out[i] ^= ctx->u[i];
		}
		ctx->done++;
	}
	return ((ctx->done >= ctx->iterations) ? 1 : 0);
}


int
lc_pbkdf2_sha512_final(lc_pbkdf2_sha512_ctx *ctx, uint8_t *out, size_t out_len)
{
	if (ctx == NULL || out == NULL) {
		return (-1);
	}
	if (out_len == 0 || out_len > LC_SHA512_DIGEST_SIZE) {
		return (-1);
	}
	if (ctx->iterations == 0 || ctx->done < ctx->iterations) {
		return (-1);
	}
	memcpy(out, ctx->out, out_len);
	return (0);
}

int
lc_pbkdf2_sha512(const void *password, size_t password_len, const void *salt,
    size_t salt_len, uint32_t iterations, uint8_t *out, size_t out_len)
{
	lc_pbkdf2_sha512_ctx	ctx;
	int			ret;

	if (lc_pbkdf2_sha512_init(&ctx, password, password_len, salt, salt_len,
	    iterations) != 0) {
		return (-1);
	}
	do {
		ret = lc_pbkdf2_sha512_step(&ctx, iterations);
	} while (ret == 0);
	if (ret < 0) {
		lc_pbkdf2_sha512_wipe(&ctx);
		return (-1);
	}
	ret = lc_pbkdf2_sha512_final(&ctx, out, out_len);
	lc_pbkdf2_sha512_wipe(&ctx);
	return (ret);
}
