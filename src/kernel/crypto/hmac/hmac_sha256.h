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

#ifndef CRYPTO_HMAC_SHA256_H
#define CRYPTO_HMAC_SHA256_H

#include <mlibc/mlibc.h>
#include <kernel/crypto/hash/sha256.h>

#define HMAC_SHA256_BLOCK_SIZE  SHA256_BLOCK_SIZE
#define HMAC_SHA256_DIGEST_SIZE SHA256_DIGEST_SIZE

typedef struct {
  sha256_ctx_t inner;
  sha256_ctx_t outer;
  u8 opad[SHA256_BLOCK_SIZE];
} hmac_sha256_ctx_t;

void hmac_sha256_init(hmac_sha256_ctx_t *ctx,
                      const u8 *key, u32 key_len);
void hmac_sha256_update(hmac_sha256_ctx_t *ctx,
                        const u8 *data, u32 len);
void hmac_sha256_final(hmac_sha256_ctx_t *ctx,
                       u8 digest[HMAC_SHA256_DIGEST_SIZE]);

void hmac_sha256(const u8 *key, u32 key_len,
                 const u8 *data, u32 data_len,
                 u8 digest[HMAC_SHA256_DIGEST_SIZE]);

#endif
