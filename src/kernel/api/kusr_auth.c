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

#include <kernel/api/api.h>
#include <kernel/crypto/crypto.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mlibc/toml.h>

#define KUSR_AUTH_HASH_LEN     PBKDF2_DIGEST_SIZE
#define KUSR_AUTH_SALT_LEN     PBKDF2_SALT_DEFAULT
#define KUSR_AUTH_HASH_HEX_LEN (KUSR_AUTH_HASH_LEN * 2 + 1)
#define KUSR_AUTH_SALT_HEX_LEN (KUSR_AUTH_SALT_LEN * 2 + 1)
#define KUSR_AUTH_DEFAULT_ITERS PBKDF2_DEFAULT_ITERS

int api_kusr_auth(const char *password) {
  process_t *proc = process_current();
  if (!proc) return -API_ERR_BAD_VALUE;

  if (!is_user_address(password, 1))
    return -API_ERR_BAD_ADDR;

  char kpass[128];
  int pass_len = 0;
  while (pass_len < 127) {
    if (!is_user_address(password + pass_len, 1))
      return -API_ERR_BAD_ADDR;
    if (password[pass_len] == '\0') break;
    kpass[pass_len] = password[pass_len];
    pass_len++;
  }
  kpass[pass_len] = '\0';

  toml_doc_t *doc = toml_parse_file("/conf/kernel.toml");
  if (!doc) {
    crypto_secure_wipe(kpass, sizeof(kpass));
    return -API_ERR_NOT_FOUND;
  }

  const char *stored_hash = toml_get(doc, "kusr", "password_hash");
  const char *stored_salt = toml_get(doc, "kusr", "password_salt");
  const char *stored_iters = toml_get(doc, "kusr", "password_iterations");

  if (!stored_hash || !stored_salt) {
    toml_free(doc);
    crypto_secure_wipe(kpass, sizeof(kpass));
    return -API_ERR_NOT_FOUND;
  }

  u32 iterations = KUSR_AUTH_DEFAULT_ITERS;
  if (stored_iters) {
    int parsed = atoi(stored_iters);
    if (parsed > 0)
      iterations = (u32)parsed;
  }

  u8 salt[KUSR_AUTH_SALT_LEN];
  u8 expected_hash[KUSR_AUTH_HASH_LEN];
  u32 salt_decoded = 0;
  u32 hash_decoded = 0;

  if (crypto_hex_decode(stored_salt, salt, KUSR_AUTH_SALT_LEN, &salt_decoded) != 0
      || salt_decoded != KUSR_AUTH_SALT_LEN) {
    toml_free(doc);
    crypto_secure_wipe(kpass, sizeof(kpass));
    crypto_secure_wipe(salt, sizeof(salt));
    return -API_ERR_BAD_VALUE;
  }

  if (crypto_hex_decode(stored_hash, expected_hash, KUSR_AUTH_HASH_LEN,
                        &hash_decoded) != 0
      || hash_decoded != KUSR_AUTH_HASH_LEN) {
    toml_free(doc);
    crypto_secure_wipe(kpass, sizeof(kpass));
    crypto_secure_wipe(salt, sizeof(salt));
    crypto_secure_wipe(expected_hash, sizeof(expected_hash));
    return -API_ERR_BAD_VALUE;
  }

  toml_free(doc);

  int match = pbkdf2_verify((const u8 *)kpass, (u32)pass_len,
                            salt, KUSR_AUTH_SALT_LEN,
                            iterations,
                            expected_hash, KUSR_AUTH_HASH_LEN);

  crypto_secure_wipe(kpass, sizeof(kpass));
  crypto_secure_wipe(salt, sizeof(salt));
  crypto_secure_wipe(expected_hash, sizeof(expected_hash));

  if (match != 0)
    return -API_ERR_PERM;

  proc->kusr_auth = 1;
  return 0;
}
