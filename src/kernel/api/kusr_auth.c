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

#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/api/api.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/memory.h>
#include <mlibc/mlibc.h>
#include <mlibc/toml.h>

static u64 fnv1a_64(const char *data, int len) {
  u64 hash = 0xcbf29ce484222325ULL;
  for (int i = 0; i < len; i++) {
    hash ^= (u8)data[i];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

static u64 hex_to_u64(const char *hex) {
  u64 val = 0;
  for (int i = 0; i < 16 && hex[i]; i++) {
    val <<= 4;
    char c = hex[i];
    if (c >= '0' && c <= '9')      val |= (c - '0');
    else if (c >= 'a' && c <= 'f') val |= (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') val |= (c - 'A' + 10);
    else return 0;
  }
  return val;
}

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
    for (int i = 0; i < pass_len; i++) kpass[i] = 0;
    return -API_ERR_NOT_FOUND;
  }

  const char *stored = toml_get(doc, "kusr", "password_hash");
  if (!stored) {
    toml_free(doc);
    for (int i = 0; i < pass_len; i++) kpass[i] = 0;
    return -API_ERR_NOT_FOUND;
  }

  u64 expected = hex_to_u64(stored);
  u64 actual = fnv1a_64(kpass, pass_len);

  for (int i = 0; i < pass_len; i++) kpass[i] = 0;
  toml_free(doc);

  if (actual != expected)
    return -API_ERR_PERM;

  proc->kusr_auth = 1;
  return 0;
}
