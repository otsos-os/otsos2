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

#include <kernel/crypto/util/crypto_util.h>

static const char s_hex_chars[] = "0123456789abcdef";

void crypto_hex_encode(const u8 *data, u32 len, char *out) {
  if (!data || !out || len == 0) {
    if (out) out[0] = '\0';
    return;
  }
  for (u32 i = 0; i < len; i++) {
    out[i * 2]     = s_hex_chars[(data[i] >> 4) & 0xF];
    out[i * 2 + 1] = s_hex_chars[data[i] & 0xF];
  }
  out[len * 2] = '\0';
}

static s8 hex_val(char c) {
  if (c >= '0' && c <= '9') return (s8)(c - '0');
  if (c >= 'a' && c <= 'f') return (s8)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (s8)(c - 'A' + 10);
  return -1;
}

int crypto_hex_decode(const char *hex, u8 *out, u32 out_max, u32 *out_len) {
  if (!hex || !out || out_max == 0)
    return -1;

  u32 i = 0;
  u32 opos = 0;

  while (hex[i]) {
    s8 hi = hex_val(hex[i]);
    if (hi < 0) return -1;
    i++;
    if (!hex[i]) return -1;
    s8 lo = hex_val(hex[i]);
    if (lo < 0) return -1;
    i++;

    if (opos >= out_max) return -1;
    out[opos++] = (u8)((hi << 4) | lo);
  }

  if (out_len) *out_len = opos;
  return (opos > 0) ? 0 : -1;
}

int crypto_constant_time_compare(const u8 *a, const u8 *b, u32 len) {
  if (!a || !b) {
    if (a == b) return 0;
    return -1;
  }
  u8 diff = 0;
  for (u32 i = 0; i < len; i++)
    diff |= a[i] ^ b[i];
  return (diff == 0) ? 0 : 1;
}

void crypto_secure_wipe(void *ptr, u32 len) {
  if (!ptr || len == 0) return;
  volatile u8 *p = (volatile u8 *)ptr;
  for (u32 i = 0; i < len; i++)
    p[i] = 0;
}

u32 crypto_strlen_safe(const char *s, u32 max) {
  if (!s) return 0;
  u32 i = 0;
  while (i < max && s[i] != '\0')
    i++;
  return i;
}
