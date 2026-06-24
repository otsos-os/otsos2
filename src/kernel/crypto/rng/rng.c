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

#include <kernel/crypto/rng/rng.h>
#include <kernel/crypto/hash/sha256.h>
#include <kernel/crypto/util/crypto_util.h>
#include <kernel/drivers/timer.h>
#include <mlibc/memory.h>

#define RNG_POOL_SIZE 128

static int g_rng_initialized = 0;
static u8 g_pool[RNG_POOL_SIZE];
static u32 g_pool_pos = 0;
static u64 g_reseed_counter = 0;
static u8 g_output_key[SHA256_DIGEST_SIZE];

static int rdrand_available(void) {
  u32 eax, ebx, ecx, edx;
  eax = 1;
  __asm__ volatile("cpuid"
                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(eax));
  return (ecx & (1u << 30)) ? 1 : 0;
}

static int rdseed_available(void) {
  u32 eax, ebx, ecx, edx;
  eax = 7;
  ecx = 0;
  __asm__ volatile("cpuid"
                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                   : "a"(eax), "c"(ecx));
  return (ebx & (1u << 18)) ? 1 : 0;
}

static int rdrand64(u64 *val) {
  unsigned char ok;
  for (int i = 0; i < 8; i++) {
    __asm__ volatile("rdrand %0; setc %1"
                     : "=r"(*val), "=qm"(ok)
                     : : "cc");
    if (ok) return 1;
  }
  return 0;
}

static int rdseed64(u64 *val) {
  unsigned char ok;
  for (int i = 0; i < 8; i++) {
    __asm__ volatile("rdseed %0; setc %1"
                     : "=r"(*val), "=qm"(ok)
                     : : "cc");
    if (ok) return 1;
  }
  return 0;
}

static u64 collect_raw_entropy(void) {
  u64 val = 0;
  u64 mix = 0;

  if (rdrand_available()) {
    u64 rnd;
    if (rdrand64(&rnd)) mix ^= rnd;
  }

  if (rdseed_available()) {
    u64 seed;
    if (rdseed64(&seed)) mix ^= seed;
  }

  if (timer_is_initialized()) {
    val ^= timer_get_ticks();
  }

  u64 tsc;
  __asm__ volatile("rdtsc" : "=A"(tsc));
  val ^= tsc;
  val ^= (u64)(unsigned long)&val;
  val ^= mix;

  return val;
}

static void pool_reseed(void) {
  sha256_ctx_t ctx;
  sha256_init(&ctx);

  sha256_update(&ctx, g_pool, RNG_POOL_SIZE);
  sha256_update(&ctx, g_output_key, SHA256_DIGEST_SIZE);

  u64 counter_val = ++g_reseed_counter;
  sha256_update(&ctx, (u8 *)&counter_val, sizeof(counter_val));

  u64 raw = collect_raw_entropy();
  sha256_update(&ctx, (u8 *)&raw, sizeof(raw));

  sha256_final(&ctx, g_output_key);

  sha256_init(&ctx);
  sha256_update(&ctx, g_pool, RNG_POOL_SIZE);
  sha256_update(&ctx, g_output_key, SHA256_DIGEST_SIZE);
  u8 new_pool[RNG_POOL_SIZE];
  u8 digest[SHA256_DIGEST_SIZE];
  sha256_final(&ctx, digest);
  memcpy(new_pool, digest, SHA256_DIGEST_SIZE);
  sha256_hash(new_pool, SHA256_DIGEST_SIZE, new_pool + SHA256_DIGEST_SIZE);
  memcpy(g_pool, new_pool, RNG_POOL_SIZE);
  g_pool_pos = 0;

  crypto_secure_wipe(digest, sizeof(digest));
  crypto_secure_wipe(new_pool, sizeof(new_pool));
}

void crypto_rng_init(void) {
  if (g_rng_initialized) return;

  crypto_secure_wipe(g_pool, RNG_POOL_SIZE);
  crypto_secure_wipe(g_output_key, sizeof(g_output_key));
  g_pool_pos = 0;
  g_reseed_counter = 0;

  for (int i = 0; i < 16; i++) {
    u64 raw = collect_raw_entropy();
    g_pool[g_pool_pos % RNG_POOL_SIZE] ^= (u8)(raw & 0xFF);
    g_pool[(g_pool_pos + 1) % RNG_POOL_SIZE] ^= (u8)((raw >> 8) & 0xFF);
    g_pool[(g_pool_pos + 2) % RNG_POOL_SIZE] ^= (u8)((raw >> 16) & 0xFF);
    g_pool[(g_pool_pos + 3) % RNG_POOL_SIZE] ^= (u8)((raw >> 24) & 0xFF);
    g_pool[(g_pool_pos + 4) % RNG_POOL_SIZE] ^= (u8)((raw >> 32) & 0xFF);
    g_pool[(g_pool_pos + 5) % RNG_POOL_SIZE] ^= (u8)((raw >> 40) & 0xFF);
    g_pool[(g_pool_pos + 6) % RNG_POOL_SIZE] ^= (u8)((raw >> 48) & 0xFF);
    g_pool[(g_pool_pos + 7) % RNG_POOL_SIZE] ^= (u8)((raw >> 56) & 0xFF);
    g_pool_pos += 8;
    if (g_pool_pos >= RNG_POOL_SIZE) g_pool_pos = 0;
  }

  pool_reseed();
  g_rng_initialized = 1;
}

void crypto_rng_add_entropy(const u8 *data, u32 len) {
  if (!data || len == 0) return;
  if (!g_rng_initialized) crypto_rng_init();

  for (u32 i = 0; i < len; i++) {
    g_pool[g_pool_pos] ^= data[i];
    g_pool_pos = (g_pool_pos + 1) % RNG_POOL_SIZE;
  }

  if (g_pool_pos == 0)
    pool_reseed();
}

static void rng_extract(u8 *out, u32 len) {
  if (!g_rng_initialized) crypto_rng_init();

  u32 done = 0;
  while (done < len) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, g_output_key, SHA256_DIGEST_SIZE);

    u64 counter = ++g_reseed_counter;
    sha256_update(&ctx, (u8 *)&counter, sizeof(counter));

    u8 block[SHA256_DIGEST_SIZE];
    sha256_final(&ctx, block);

    u32 copy_len = len - done;
    if (copy_len > SHA256_DIGEST_SIZE)
      copy_len = SHA256_DIGEST_SIZE;

    memcpy(out + done, block, copy_len);
    done += copy_len;

    crypto_secure_wipe(block, sizeof(block));
  }

  pool_reseed();
}

int crypto_rng_bytes(u8 *out, u32 len) {
  if (!out || len == 0) return -1;
  rng_extract(out, len);
  return 0;
}

u64 crypto_rng_u64(void) {
  u8 buf[8];
  rng_extract(buf, 8);
  u64 val = ((u64)buf[0])       | ((u64)buf[1] <<  8) |
            ((u64)buf[2] << 16) | ((u64)buf[3] << 24) |
            ((u64)buf[4] << 32) | ((u64)buf[5] << 40) |
            ((u64)buf[6] << 48) | ((u64)buf[7] << 56);
  crypto_secure_wipe(buf, sizeof(buf));
  return val;
}
