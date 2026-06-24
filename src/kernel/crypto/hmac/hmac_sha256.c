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
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/crypto/hmac/hmac_sha256.h>
#include <kernel/crypto/util/crypto_util.h>

#define HMAC_IPAD 0x36
#define HMAC_OPAD 0x5c

void hmac_sha256_init(hmac_sha256_ctx_t *ctx,
                      const u8 *key, u32 key_len) {
  if (!ctx) return;

  u8 k_block[SHA256_BLOCK_SIZE];
  crypto_secure_wipe(k_block, sizeof(k_block));

  if (key && key_len > 0) {
    if (key_len > SHA256_BLOCK_SIZE) {
      sha256_hash(key, key_len, k_block);
    } else {
      memcpy(k_block, key, key_len);
    }
  }

  u8 ipad[SHA256_BLOCK_SIZE];
  for (int i = 0; i < SHA256_BLOCK_SIZE; i++)
    ipad[i] = k_block[i] ^ HMAC_IPAD;

  for (int i = 0; i < SHA256_BLOCK_SIZE; i++)
    ctx->opad[i] = k_block[i] ^ HMAC_OPAD;

  crypto_secure_wipe(k_block, sizeof(k_block));

  sha256_init(&ctx->inner);
  sha256_update(&ctx->inner, ipad, SHA256_BLOCK_SIZE);
  crypto_secure_wipe(ipad, sizeof(ipad));

  sha256_init(&ctx->outer);
  sha256_update(&ctx->outer, ctx->opad, SHA256_BLOCK_SIZE);
}

void hmac_sha256_update(hmac_sha256_ctx_t *ctx,
                        const u8 *data, u32 len) {
  if (!ctx || !data) return;
  sha256_update(&ctx->inner, data, len);
}

void hmac_sha256_final(hmac_sha256_ctx_t *ctx,
                       u8 digest[HMAC_SHA256_DIGEST_SIZE]) {
  if (!ctx || !digest) return;

  u8 inner_digest[SHA256_DIGEST_SIZE];
  sha256_final(&ctx->inner, inner_digest);

  sha256_update(&ctx->outer, inner_digest, SHA256_DIGEST_SIZE);
  sha256_final(&ctx->outer, digest);

  crypto_secure_wipe(inner_digest, sizeof(inner_digest));
  crypto_secure_wipe(ctx->opad, sizeof(ctx->opad));
}

void hmac_sha256(const u8 *key, u32 key_len,
                 const u8 *data, u32 data_len,
                 u8 digest[HMAC_SHA256_DIGEST_SIZE]) {
  hmac_sha256_ctx_t ctx;
  hmac_sha256_init(&ctx, key, key_len);
  hmac_sha256_update(&ctx, data, data_len);
  hmac_sha256_final(&ctx, digest);
}
