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

#include <kernel/crypto/kdf/pbkdf2.h>
#include <kernel/crypto/hmac/hmac_sha256.h>
#include <kernel/crypto/util/crypto_util.h>

void pbkdf2_hmac_sha256(const u8 *password, u32 password_len,
                        const u8 *salt, u32 salt_len,
                        u32 iterations,
                        u8 *out, u32 out_len) {
  if (!password || !salt || !out || iterations == 0)
    return;

  u32 hlen = HMAC_SHA256_DIGEST_SIZE;
  u32 blocks = (out_len + hlen - 1) / hlen;
  u32 done = 0;

  u8 u[HMAC_SHA256_DIGEST_SIZE];
  u8 t[HMAC_SHA256_DIGEST_SIZE];

  for (u32 block_index = 1; block_index <= blocks; block_index++) {
    u8 salt_block[256];
    if (salt_len + 4 > sizeof(salt_block))
      return;
    memcpy(salt_block, salt, salt_len);
    salt_block[salt_len]     = (u8)((block_index >> 24) & 0xFF);
    salt_block[salt_len + 1] = (u8)((block_index >> 16) & 0xFF);
    salt_block[salt_len + 2] = (u8)((block_index >>  8) & 0xFF);
    salt_block[salt_len + 3] = (u8)(block_index & 0xFF);

    hmac_sha256(password, password_len,
                salt_block, salt_len + 4,
                u);
    memcpy(t, u, hlen);

    for (u32 j = 1; j < iterations; j++) {
      hmac_sha256(password, password_len, u, hlen, u);
      for (u32 k = 0; k < hlen; k++)
        t[k] ^= u[k];
    }

    u32 copy_len = hlen;
    if (done + copy_len > out_len)
      copy_len = out_len - done;
    memcpy(out + done, t, copy_len);
    done += copy_len;

    crypto_secure_wipe(salt_block, sizeof(salt_block));
  }

  crypto_secure_wipe(u, sizeof(u));
  crypto_secure_wipe(t, sizeof(t));
}

int pbkdf2_verify(const u8 *password, u32 password_len,
                  const u8 *salt, u32 salt_len,
                  u32 iterations,
                  const u8 *expected, u32 expected_len) {
  if (!password || !salt || !expected || iterations == 0 || expected_len == 0)
    return -1;

  u8 *derived = (u8 *)kcalloc(expected_len, 1);
  if (!derived) return -1;

  pbkdf2_hmac_sha256(password, password_len,
                     salt, salt_len,
                     iterations,
                     derived, expected_len);

  int result = crypto_constant_time_compare(derived, expected, expected_len);

  crypto_secure_wipe(derived, expected_len);
  kfree(derived);

  return result;
}
