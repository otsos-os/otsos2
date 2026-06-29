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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s64 as 64 bit signed
$define %type int as 32 bit signed
$define %type sha256_ctx_t as struct with SHA-256 hashing state
$define %type chacha20_ctx_t as struct with ChaCha20 stream cipher state

$define %func rdrand_available as function with args void
$define %func rdseed_available as function with args void
$define %func rdrand64 as function with args u64 *
$define %func rdseed64 as function with args u64 *
$define %func collect_raw_entropy as function with args void
$define %func entropy_pool_init as procedure with args void
$define %func entropy_pool_add as procedure with args const u8 *, u32
$define %func entropy_pool_extract as procedure with args u8 *, u32
$define %func drbg_reseed as procedure with args void
$define %func drbg_generate as procedure with args u8 *, u32
$define %func crypto_rng_init as procedure with args void
$define %func crypto_rng_add_entropy as procedure with args const u8 *, u32
$define %func crypto_rng_bytes as function with args u8 *, u32
$define %func crypto_rng_u64 as function with args void
$define %func crypto_rng_tick as procedure with args void

*/

/* !SPACE!

$space %internal rdrand_available, rdseed_available
$space %internal rdrand64, rdseed64
$space %internal collect_raw_entropy
$space %internal entropy_pool_init, entropy_pool_add
$space %internal entropy_pool_extract
$space %internal drbg_reseed, drbg_generate
$space %export crypto_rng_init, crypto_rng_add_entropy
$space %export crypto_rng_bytes, crypto_rng_u64
$space %export crypto_rng_tick
*/

#include <kernel/crypto/rng/rng.h>
#include <kernel/crypto/hash/sha256.h>
#include <kernel/crypto/cipher/chacha20.h>
#include <kernel/crypto/util/crypto_util.h>
#include <kernel/drivers/timer.h>
#include <kernel/event/event.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

#define ENTROPY_POOL_SIZE	256
#define DRBG_RESEED_THRESHOLD	(1 << 20)
#define MAX_ENTROPY_SAMPLES	32
static u8		g_pool[ENTROPY_POOL_SIZE];
static u32		g_pool_pos;
static u64		g_pool_mix_count;
static int		g_pool_initialized;
static chacha20_ctx_t	g_drbg_ctx;
static u8		g_drbg_key[CHACHA20_KEY_SIZE];
static u8		g_drbg_nonce[CHACHA20_NONCE_SIZE];
static u64		g_drbg_bytes_since_reseed;
static int		g_drbg_seeded;
static int		g_rng_ready;

static int
rdrand_available(void)
{
	u32	eax, ebx, ecx, edx;

	eax = 1;
	__asm__ volatile("cpuid"
	    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	    : "a"(eax));
	return ((ecx & (1u << 30)) ? 1 : 0);
}

static int
rdseed_available(void)
{
	u32	eax, ebx, ecx, edx;

	eax = 7;
	ecx = 0;
	__asm__ volatile("cpuid"
	    : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	    : "a"(eax), "c"(ecx));
	return ((ebx & (1u << 18)) ? 1 : 0);
}

static int
rdrand64(u64 *val)
{
	unsigned char	ok;
	int		i;

	for (i = 0; i < 8; i++) {
		__asm__ volatile("rdrand %0; setc %1"
		    : "=r"(*val), "=qm"(ok)
		    : : "cc");
		if (ok) {
			return (1);
		}
	}
	return (0);
}

static int
rdseed64(u64 *val)
{
	unsigned char	ok;
	int		i;

	for (i = 0; i < 8; i++) {
		__asm__ volatile("rdseed %0; setc %1"
		    : "=r"(*val), "=qm"(ok)
		    : : "cc");
		if (ok) {
			return (1);
		}
	}
	return (0);
}

static u64
collect_raw_entropy(void)
{
	u64	val;
	u64	tsc;
	u64	mix;

	val = 0;
	mix = 0;

	if (rdrand_available()) {
		u64	rnd;

		if (rdrand64(&rnd)) {
			mix ^= rnd;
		}
	}

	if (rdseed_available()) {
		u64	seed;

		if (rdseed64(&seed)) {
			mix ^= seed;
		}
	}

	if (timer_is_initialized()) {
		val ^= timer_get_ticks();
	}

	__asm__ volatile("rdtsc" : "=A"(tsc));
	val ^= tsc;
	val ^= (u64)(unsigned long)&val;
	val ^= mix;
	val ^= g_pool_mix_count;

	return (val);
}

static void
entropy_pool_init(void)
{
	int	i;

	crypto_secure_wipe(g_pool, ENTROPY_POOL_SIZE);
	g_pool_pos = 0;
	g_pool_mix_count = 0;

	for (i = 0; i < MAX_ENTROPY_SAMPLES; i++) {
		u64	raw;

		raw = collect_raw_entropy();
		g_pool[g_pool_pos] ^= (u8)(raw & 0xFF);
		g_pool[(g_pool_pos + 1) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 8) & 0xFF);
		g_pool[(g_pool_pos + 2) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 16) & 0xFF);
		g_pool[(g_pool_pos + 3) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 24) & 0xFF);
		g_pool[(g_pool_pos + 4) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 32) & 0xFF);
		g_pool[(g_pool_pos + 5) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 40) & 0xFF);
		g_pool[(g_pool_pos + 6) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 48) & 0xFF);
		g_pool[(g_pool_pos + 7) % ENTROPY_POOL_SIZE] ^=
		    (u8)((raw >> 56) & 0xFF);
		g_pool_pos += 8;
		if (g_pool_pos >= ENTROPY_POOL_SIZE) {
			g_pool_pos = 0;
		}
		g_pool_mix_count++;
	}

	g_pool_initialized = 1;
}

static void
entropy_pool_add(const u8 *data, u32 len)
{
	u32	i;

	if (!data || len == 0) {
		return;
	}

	if (!g_pool_initialized) {
		entropy_pool_init();
	}

	for (i = 0; i < len; i++) {
		g_pool[g_pool_pos] ^= data[i];
		g_pool_pos = (g_pool_pos + 1) % ENTROPY_POOL_SIZE;
		g_pool_mix_count++;
	}
}

static void
entropy_pool_extract(u8 *out, u32 len)
{
	sha256_ctx_t	ctx;
	u8		digest[SHA256_DIGEST_SIZE];
	u32		done;
	u64		counter;

	if (!out || len == 0) {
		return;
	}

	counter = g_pool_mix_count;
	done = 0;

	while (done < len) {
		sha256_init(&ctx);
		sha256_update(&ctx, g_pool, ENTROPY_POOL_SIZE);
		sha256_update(&ctx, (u8 *)&counter, sizeof(counter));
		sha256_final(&ctx, digest);

		u32	copy_len;

		copy_len = len - done;
		if (copy_len > SHA256_DIGEST_SIZE) {
			copy_len = SHA256_DIGEST_SIZE;
		}

		memcpy(out + done, digest, copy_len);
		done += copy_len;
		counter++;

		crypto_secure_wipe(digest, sizeof(digest));
	}

	sha256_init(&ctx);
	sha256_update(&ctx, g_pool, ENTROPY_POOL_SIZE);
	sha256_update(&ctx, out, len);
	sha256_update(&ctx, (u8 *)&counter, sizeof(counter));
	sha256_final(&ctx, digest);
	memcpy(g_pool, digest, SHA256_DIGEST_SIZE);

	sha256_hash(g_pool, SHA256_DIGEST_SIZE,
	    g_pool + SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 2 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 2 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 3 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 3 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 4 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 4 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 5 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 5 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 6 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 6 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 7 * SHA256_DIGEST_SIZE);
	sha256_hash(g_pool + 7 * SHA256_DIGEST_SIZE,
	    SHA256_DIGEST_SIZE,
	    g_pool + 8 * SHA256_DIGEST_SIZE);

	g_pool_pos = 0;

	crypto_secure_wipe(digest, sizeof(digest));
}

static void
drbg_reseed(void)
{
	u8	seed_material[CHACHA20_KEY_SIZE + CHACHA20_NONCE_SIZE];
	u64	raw;

	/* Gather fresh entropy from all sources */
	entropy_pool_extract(seed_material,
	    CHACHA20_KEY_SIZE + CHACHA20_NONCE_SIZE);

	raw = collect_raw_entropy();
	for (int i = 0; i < 8; i++) {
		seed_material[i] ^= (u8)((raw >> (i * 8)) & 0xFF);
	}

	memcpy(g_drbg_key, seed_material, CHACHA20_KEY_SIZE);
	memcpy(g_drbg_nonce, seed_material + CHACHA20_KEY_SIZE,
	    CHACHA20_NONCE_SIZE);

	chacha20_init(&g_drbg_ctx, g_drbg_key, g_drbg_nonce);

	g_drbg_bytes_since_reseed = 0;
	g_drbg_seeded = 1;

	crypto_secure_wipe(seed_material,
	    sizeof(seed_material));
}

static void
drbg_generate(u8 *out, u32 len)
{
	u32	done;
	u8	block[CHACHA20_BLOCK_SIZE];

	if (!g_drbg_seeded) {
		drbg_reseed();
	}

	done = 0;
	while (done < len) {
		u32	copy_len;

		copy_len = len - done;
		if (copy_len > CHACHA20_BLOCK_SIZE) {
			copy_len = CHACHA20_BLOCK_SIZE;
		}
		memset(block, 0, CHACHA20_BLOCK_SIZE);
		chacha20_xor(&g_drbg_ctx, block, block,
		    CHACHA20_BLOCK_SIZE);
		memcpy(out + done, block, copy_len);
		done += copy_len;

		crypto_secure_wipe(block, sizeof(block));
	}

	g_drbg_bytes_since_reseed += len;

	if (g_drbg_bytes_since_reseed >= DRBG_RESEED_THRESHOLD) {
		drbg_reseed();
	}
}


void
crypto_rng_init(void)
{
	if (g_rng_ready) {
		return;
	}

	com1_printf("[RNG] initializing ChaCha20-DRBG...\n");

	entropy_pool_init();

	for (int i = 0; i < 8; i++) {
		u64	raw;

		raw = collect_raw_entropy();
		entropy_pool_add((u8 *)&raw, sizeof(raw));
	}

	drbg_reseed();
	g_rng_ready = 1;

	com1_printf("[RNG] CSPRNG ready (pool=%d bytes, "
	    "reseed threshold=%d bytes)\n",
	    ENTROPY_POOL_SIZE, DRBG_RESEED_THRESHOLD);
}

void
crypto_rng_add_entropy(const u8 *data, u32 len)
{
	if (!data || len == 0) {
		return;
	}

	if (!g_rng_ready) {
		crypto_rng_init();
		return;
	}

	entropy_pool_add(data, len);

	if ((g_pool_mix_count & 0xFF) == 0) {
		drbg_reseed();
	}
}

int
crypto_rng_bytes(u8 *out, u32 len)
{
	if (!out || len == 0) {
		return (-1);
	}

	if (!g_rng_ready) {
		crypto_rng_init();
	}

	drbg_generate(out, len);
	return (0);
}

u64
crypto_rng_u64(void)
{
	u8	buf[8];

	if (!g_rng_ready) {
		crypto_rng_init();
	}

	drbg_generate(buf, 8);
	u64	val;

	val = ((u64)buf[0])       | ((u64)buf[1] <<  8) |
	    ((u64)buf[2] << 16) | ((u64)buf[3] << 24) |
	    ((u64)buf[4] << 32) | ((u64)buf[5] << 40) |
	    ((u64)buf[6] << 48) | ((u64)buf[7] << 56);
	crypto_secure_wipe(buf, sizeof(buf));
	return (val);
}


void
crypto_rng_tick(void)
{
	u64	raw;

	if (!g_rng_ready) {
		return;
	}

	raw = collect_raw_entropy();

	g_pool[g_pool_pos] ^= (u8)(raw & 0xFF);
	g_pool[(g_pool_pos + 1) % ENTROPY_POOL_SIZE] ^=
	    (u8)((raw >> 8) & 0xFF);
	g_pool[(g_pool_pos + 2) % ENTROPY_POOL_SIZE] ^=
	    (u8)((raw >> 16) & 0xFF);
	g_pool[(g_pool_pos + 3) % ENTROPY_POOL_SIZE] ^=
	    (u8)((raw >> 24) & 0xFF);
	g_pool_pos = (g_pool_pos + 4) % ENTROPY_POOL_SIZE;
	g_pool_mix_count++;
	if ((g_pool_mix_count & 0x3FF) == 0) {
		drbg_reseed();
	}
}
