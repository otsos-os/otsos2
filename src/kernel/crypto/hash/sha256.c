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

#include <kernel/crypto/hash/sha256.h>
#include <kernel/crypto/util/crypto_util.h>

static const u32 s_sha256_k[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
  0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
  0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
  0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
  0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
  0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
  0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
  0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
  0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const u32 s_sha256_init[8] = {
  0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
  0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHR(x,n)  ((x) >> (n))
#define CH(x,y,z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define BSIG0(x) (ROTR((x),2)  ^ ROTR((x),13) ^ ROTR((x),22))
#define BSIG1(x) (ROTR((x),6)  ^ ROTR((x),11) ^ ROTR((x),25))
#define SSIG0(x) (ROTR((x),7)  ^ ROTR((x),18) ^ SHR((x),3))
#define SSIG1(x) (ROTR((x),17) ^ ROTR((x),19) ^ SHR((x),10))

static void sha256_transform(sha256_ctx_t *ctx, const u8 *block) {
  u32 w[64];
  u32 a, b, c, d, e, f, g, h;
  u32 t1, t2;

  for (int i = 0; i < 16; i++) {
    w[i] = ((u32)block[i * 4]     << 24) |
           ((u32)block[i * 4 + 1] << 16) |
           ((u32)block[i * 4 + 2] <<  8) |
           ((u32)block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; i++) {
    w[i] = SSIG1(w[i - 2]) + w[i - 7] + SSIG0(w[i - 15]) + w[i - 16];
  }

  a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
  e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

  for (int i = 0; i < 64; i++) {
    t1 = h + BSIG1(e) + CH(e, f, g) + s_sha256_k[i] + w[i];
    t2 = BSIG0(a) + MAJ(a, b, c);
    h = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }

  ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
  ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx) {
  if (!ctx) return;
  for (int i = 0; i < 8; i++)
    ctx->state[i] = s_sha256_init[i];
  ctx->bitlen  = 0;
  ctx->datalen = 0;
}

void sha256_update(sha256_ctx_t *ctx, const u8 *data, u32 len) {
  if (!ctx || !data) return;

  for (u32 i = 0; i < len; i++) {
    ctx->data[ctx->datalen++] = data[i];
    if (ctx->datalen == SHA256_BLOCK_SIZE) {
      sha256_transform(ctx, ctx->data);
      ctx->bitlen += (u64)SHA256_BLOCK_SIZE * 8;
      ctx->datalen = 0;
    }
  }
}

void sha256_final(sha256_ctx_t *ctx, u8 digest[SHA256_DIGEST_SIZE]) {
  if (!ctx || !digest) return;

  u64 bitlen = ctx->bitlen + (u64)ctx->datalen * 8;

  ctx->data[ctx->datalen++] = 0x80;

  if (ctx->datalen > 56) {
    while (ctx->datalen < SHA256_BLOCK_SIZE)
      ctx->data[ctx->datalen++] = 0;
    sha256_transform(ctx, ctx->data);
    ctx->datalen = 0;
  }

  while (ctx->datalen < 56)
    ctx->data[ctx->datalen++] = 0;

  for (int i = 7; i >= 0; i--)
    ctx->data[ctx->datalen++] = (u8)((bitlen >> (i * 8)) & 0xFF);

  sha256_transform(ctx, ctx->data);

  for (int i = 0; i < 8; i++) {
    digest[i * 4]     = (u8)((ctx->state[i] >> 24) & 0xFF);
    digest[i * 4 + 1] = (u8)((ctx->state[i] >> 16) & 0xFF);
    digest[i * 4 + 2] = (u8)((ctx->state[i] >>  8) & 0xFF);
    digest[i * 4 + 3] = (u8)(ctx->state[i] & 0xFF);
  }

  crypto_secure_wipe(ctx->data, SHA256_BLOCK_SIZE);
  crypto_secure_wipe(ctx->state, sizeof(ctx->state));
  ctx->datalen = 0;
  ctx->bitlen  = 0;
}

void sha256_hash(const u8 *data, u32 len, u8 digest[SHA256_DIGEST_SIZE]) {
  sha256_ctx_t ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, data, len);
  sha256_final(&ctx, digest);
}
