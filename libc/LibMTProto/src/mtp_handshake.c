/* !DEFINES!

$define %type mtp_handshake as MTProto authorization-key exchange state
$define %func mtp_handshake_begin as function with args client
$define %func mtp_handshake_step as function with args client, frame, frame length
$define %func mtp_factorize as function with args uint64_t, out p, out q
$define %func mtp_dh_generator_ok as function with args g, prime, prime length

*/

/* !SPACE!

$space %internal hs_get_raw, hs_nonce, hs_factor, hs_rsa_encrypt, hs_tmp_aes
$space %internal hs_pick_key, hs_validate_dh, hs_send_dh_params, hs_handle_res_pq
$space %internal hs_handle_dh_params, hs_handle_dh_gen
$space %export mtp_handshake_begin, mtp_handshake_step, mtp_factorize
$space %export mtp_dh_generator_ok

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mtp_internal.h"

#define HS_STEP_PQ		1
#define HS_STEP_DH_PARAMS	2
#define HS_STEP_DH_GEN		3
#define HS_INNER_MAX		(MTP_RSA_BLOB - LC_SHA1_DIGEST_SIZE)
#define HS_TMP_HEAD		20
#define HS_RSA_EXPONENT	65537u
#define HS_DH_MIN_BITS		2040
#define HS_DH_EXP_TRIES		16
#define HS_MAX_FINGERPRINTS	16
#define HS_OFFERED_MARK		4

static const uint8_t	hs_rsa_modulus[MTP_RSA_SIZE] = {
	0xc1, 0x50, 0x02, 0x3e, 0x2f, 0x70, 0xdb, 0x79, 0x85, 0xde, 0xd0, 0x64,
	0x75, 0x9c, 0xfe, 0xcf, 0x0a, 0xf3, 0x28, 0xe6, 0x9a, 0x41, 0xda, 0xf4,
	0xd6, 0xf0, 0x1b, 0x53, 0x81, 0x35, 0xa6, 0xf9, 0x1f, 0x8f, 0x8b, 0x2a,
	0x0e, 0xc9, 0xba, 0x97, 0x20, 0xce, 0x35, 0x2e, 0xfc, 0xf6, 0xc5, 0x68,
	0x0f, 0xfc, 0x42, 0x4b, 0xd6, 0x34, 0x86, 0x49, 0x02, 0xde, 0x0b, 0x4b,
	0xd6, 0xd4, 0x9f, 0x4e, 0x58, 0x02, 0x30, 0xe3, 0xae, 0x97, 0xd9, 0x5c,
	0x8b, 0x19, 0x44, 0x2b, 0x3c, 0x0a, 0x10, 0xd8, 0xf5, 0x63, 0x3f, 0xec,
	0xed, 0xd6, 0x92, 0x6a, 0x7f, 0x6d, 0xab, 0x0d, 0xdb, 0x7d, 0x45, 0x7f,
	0x9e, 0xa8, 0x1b, 0x84, 0x65, 0xfc, 0xd6, 0xff, 0xfe, 0xed, 0x11, 0x40,
	0x11, 0xdf, 0x91, 0xc0, 0x59, 0xca, 0xed, 0xaf, 0x97, 0x62, 0x5f, 0x6c,
	0x96, 0xec, 0xc7, 0x47, 0x25, 0x55, 0x69, 0x34, 0xef, 0x78, 0x1d, 0x86,
	0x6b, 0x34, 0xf0, 0x11, 0xfc, 0xe4, 0xd8, 0x35, 0xa0, 0x90, 0x19, 0x6e,
	0x9a, 0x5f, 0x0e, 0x44, 0x49, 0xaf, 0x7e, 0xb6, 0x97, 0xdd, 0xb9, 0x07,
	0x64, 0x94, 0xca, 0x5f, 0x81, 0x10, 0x4a, 0x30, 0x5b, 0x6d, 0xd2, 0x76,
	0x65, 0x72, 0x2c, 0x46, 0xb6, 0x0e, 0x5d, 0xf6, 0x80, 0xfb, 0x16, 0xb2,
	0x10, 0x60, 0x7e, 0xf2, 0x17, 0x65, 0x2e, 0x60, 0x23, 0x6c, 0x25, 0x5f,
	0x6a, 0x28, 0x31, 0x5f, 0x40, 0x83, 0xa9, 0x67, 0x91, 0xd7, 0x21, 0x4b,
	0xf6, 0x4c, 0x1d, 0xf4, 0xfd, 0x0d, 0xb1, 0x94, 0x4f, 0xb2, 0x6a, 0x2a,
	0x57, 0x03, 0x1b, 0x32, 0xee, 0xe6, 0x4a, 0xd1, 0x5a, 0x8b, 0xa6, 0x88,
	0x85, 0xcd, 0xe7, 0x4a, 0x5b, 0xfc, 0x92, 0x0f, 0x6a, 0xbf, 0x59, 0xba,
	0x5c, 0x75, 0x50, 0x63, 0x73, 0xe7, 0x13, 0x0f, 0x90, 0x42, 0xda, 0x92,
	0x21, 0x79, 0x25, 0x1f
};


static const struct hs_rsa_key {
	int64_t		fingerprint;
	const uint8_t	*modulus;
	size_t		modulus_len;
} hs_rsa_keys[] = {
	{ (int64_t)0xc3b42b026ce86b21ULL, hs_rsa_modulus, MTP_RSA_SIZE }
};

static int
hs_get_raw(const mtp_object_t *o, const char *field, size_t len,
    const uint8_t **out)
{
	mtp_reader_t	r;

	*out = NULL;
	if (mtp_object_at(o, field, &r) != 0) {
		return (-1);
	}
	*out = mtp_read_raw(&r, len);
	return (*out == NULL ? -1 : 0);
}

static uint64_t
hs_mul_mod(uint64_t a, uint64_t b, uint64_t m)
{
	uint64_t	r;

	r = 0;
	while (b != 0) {
		if (b & 1u) {
			r = (r >= m - a) ? r - (m - a) : r + a;
		}
		b >>= 1;
		if (b != 0) {
			a = (a >= m - a) ? a - (m - a) : a + a;
		}
	}
	return (r);
}

static uint64_t
hs_gcd(uint64_t a, uint64_t b)
{
	uint64_t	t;

	while (b != 0) {
		t = a % b;
		a = b;
		b = t;
	}
	return (a);
}

static uint64_t
hs_factor(uint64_t n)
{
	uint64_t	x, y, c, q, g, diff, k, i, r;
	int		try_no;

	if ((n & 1u) == 0) {
		return (2);
	}
	for (try_no = 1; try_no <= 24; try_no++) {
		y = (uint64_t)try_no + 1;
		c = (uint64_t)(try_no * 2 + 1);
		g = 1;
		r = 1;
		q = 1;
		while (g == 1 && r < (1ULL << 22)) {
			x = y;
			for (i = 0; i < r; i++) {
				y = (hs_mul_mod(y, y, n) + c) % n;
			}
			k = 0;
			while (k < r && g == 1) {
				for (i = 0; i < 64 && i < r - k; i++) {
					y = (hs_mul_mod(y, y, n) + c) % n;
					diff = x > y ? x - y : y - x;
					q = hs_mul_mod(q, diff, n);
				}
				g = hs_gcd(q, n);
				k += i;
			}
			r <<= 1;
		}
		if (g > 1 && g < n) {
			return (g);
		}
	}
	return (0);
}

int
mtp_factorize(uint64_t pq, uint64_t *out_p, uint64_t *out_q)
{
	uint64_t	p;

	if (out_p == NULL || out_q == NULL || pq < 4) {
		return (-1);
	}
	p = hs_factor(pq);
	if (p == 0 || p == pq || pq % p != 0) {
		return (-1);
	}
	*out_p = p;
	*out_q = pq / p;
	if (*out_p > *out_q) {
		p = *out_p;
		*out_p = *out_q;
		*out_q = p;
	}
	return (0);
}


static int
hs_rsa_encrypt(const struct hs_rsa_key *key, const uint8_t *data,
    size_t data_len, uint8_t out[MTP_RSA_SIZE])
{
	uint8_t		blob[MTP_RSA_SIZE];
	uint8_t		digest[LC_SHA1_DIGEST_SIZE];
	uint8_t		exp[3];
	size_t		pad_off;
	int		ret;

	if (key == NULL || data == NULL || data_len > HS_INNER_MAX) {
		mtp_logf(MTP_LOG_ERROR, "RSA: inner block of %u bytes exceeds "
		    "%u", (unsigned int)data_len, (unsigned int)HS_INNER_MAX);
		return (-1);
	}

	memset(blob, 0, sizeof(blob));
	lc_sha1(data, data_len, digest);
	memcpy(blob + 1, digest, sizeof(digest));
	memcpy(blob + 1 + sizeof(digest), data, data_len);
	pad_off = 1 + sizeof(digest) + data_len;
	if (lc_random(blob + pad_off, sizeof(blob) - pad_off) != 0) {
		mtp_logf(MTP_LOG_ERROR, "RSA: no randomness available for "
		    "padding");
		lc_wipe(blob, sizeof(blob));
		lc_wipe(digest, sizeof(digest));
		return (-1);
	}
	exp[0] = (uint8_t)(HS_RSA_EXPONENT >> 16);
	exp[1] = (uint8_t)(HS_RSA_EXPONENT >> 8);
	exp[2] = (uint8_t)HS_RSA_EXPONENT;
	ret = lc_rsa_public(key->modulus, key->modulus_len, exp, sizeof(exp),
	    blob, out, MTP_RSA_SIZE);
	if (ret != 0) {
		mtp_logf(MTP_LOG_ERROR, "RSA: modular exponentiation failed "
		    "for key %016llx",
		    (unsigned long long)key->fingerprint);
	} else {
		mtp_logf(MTP_LOG_DEBUG, "RSA: sealed %u-byte inner block into "
		    "%u bytes with key %016llx", (unsigned int)data_len,
		    (unsigned int)MTP_RSA_SIZE,
		    (unsigned long long)key->fingerprint);
	}
	lc_wipe(blob, sizeof(blob));
	lc_wipe(digest, sizeof(digest));
	return (ret);
}

static int
hs_nonce(const mtp_object_t *o, const char *field, const uint8_t *expected,
    size_t len)
{
	const uint8_t	*value;

	return (hs_get_raw(o, field, len, &value) == 0 &&
	    lc_memeq(value, expected, len) ? 0 : -1);
}

static uint64_t
hs_be64(const uint8_t *p)
{
	uint64_t	v;
	int		i;

	v = 0;
	for (i = 0; i < 8; i++) {
		v = (v << 8) | p[i];
	}
	return (v);
}

static uint64_t
hs_le64(const uint8_t *p)
{
	uint64_t	v;
	int		i;

	v = 0;
	for (i = 7; i >= 0; i--) {
		v = (v << 8) | p[i];
	}
	return (v);
}

static void
hs_tmp_aes(mtp_client_t *c)
{
	lc_sha1_ctx	ctx;
	uint8_t		a[LC_SHA1_DIGEST_SIZE];
	uint8_t		b[LC_SHA1_DIGEST_SIZE];
	uint8_t		c_hash[LC_SHA1_DIGEST_SIZE];

	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	lc_sha1_update(&ctx, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	lc_sha1_final(&ctx, a);
	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	lc_sha1_update(&ctx, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	lc_sha1_final(&ctx, b);
	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	lc_sha1_update(&ctx, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	lc_sha1_final(&ctx, c_hash);
	memcpy(c->hs_tmp_aes_key, a, sizeof(a));
	memcpy(c->hs_tmp_aes_key + sizeof(a), b, 12);
	memcpy(c->hs_tmp_aes_iv, b + 12, 8);
	memcpy(c->hs_tmp_aes_iv + 8, c_hash, sizeof(c_hash));
	memcpy(c->hs_tmp_aes_iv + 28, c->hs_new_nonce, 4);
	lc_wipe(&ctx, sizeof(ctx));
	lc_wipe(a, sizeof(a));
	lc_wipe(b, sizeof(b));
	lc_wipe(c_hash, sizeof(c_hash));
}


static const struct hs_rsa_key *
hs_pick_key(mtp_client_t *c, const mtp_object_t *o)
{
	mtp_reader_t	r;
	char		offered[MTP_MAX_ERROR];
	uint32_t	count, i;
	size_t		k, used, limit;
	int64_t		fingerprint;
	int		written, truncated;

	if (mtp_object_vector(o, "server_public_key_fingerprints", &r,
	    &count) != 0) {
		(void)mtp_fail(c, MTP_ERR_PROTO,
		    "resPQ carries no server_public_key_fingerprints vector");
		return (NULL);
	}
	if (count == 0 || count > HS_MAX_FINGERPRINTS) {
		(void)mtp_fail(c, MTP_ERR_PROTO,
		    "resPQ offers %u RSA fingerprints, expected 1..%u",
		    (unsigned int)count, (unsigned int)HS_MAX_FINGERPRINTS);
		return (NULL);
	}
	mtp_logf(MTP_LOG_INFO, "resPQ: server offers %u RSA key(s)",
	    (unsigned int)count);
	used = 0;
	truncated = 0;
	limit = sizeof(offered) - HS_OFFERED_MARK;
	offered[0] = '\0';
	for (i = 0; i < count; i++) {
		fingerprint = mtp_read_i64(&r);
		if (r.error) {
			(void)mtp_fail(c, MTP_ERR_PROTO,
			    "resPQ fingerprint vector truncated at entry %u of "
			    "%u", (unsigned int)i, (unsigned int)count);
			return (NULL);
		}
		if (!truncated) {
			written = snprintf(offered + used, limit - used,
			    "%s%016llx", used != 0 ? "," : "",
			    (unsigned long long)fingerprint);
			if (written > 0 && (size_t)written < limit - used) {
				used += (size_t)written;
			} else {
				truncated = 1;
				memcpy(offered + used, "...", 4);
			}
		}
		for (k = 0; k < sizeof(hs_rsa_keys) / sizeof(hs_rsa_keys[0]); k++) {
			if (hs_rsa_keys[k].fingerprint != fingerprint) {
				continue;
			}
			mtp_logf(MTP_LOG_INFO, "resPQ: key %u/%u fingerprint "
			    "%016llx is trusted, using it",
			    (unsigned int)i + 1, (unsigned int)count,
			    (unsigned long long)fingerprint);
			return (&hs_rsa_keys[k]);
		}
		mtp_logf(MTP_LOG_INFO, "resPQ: key %u/%u fingerprint %016llx "
		    "is not in the trusted table", (unsigned int)i + 1,
		    (unsigned int)count, (unsigned long long)fingerprint);
	}
	(void)mtp_fail(c, MTP_ERR_AUTH,
	    "DC%d offered no RSA key this client trusts (offered %s, trusted "
	    "%016llx)", mtp_dc_id(c->dc_index), offered,
	    (unsigned long long)hs_rsa_keys[0].fingerprint);
	return (NULL);
}

static int
hs_send_dh_params(mtp_client_t *c, const mtp_object_t *o)
{
	const struct hs_rsa_key	*key;
	mtp_reader_t		r;
	mtp_writer_t		w;
	const uint8_t		*pq;
	uint8_t			inner[HS_INNER_MAX];
	uint8_t			body[512];
	uint8_t			rsa[MTP_RSA_SIZE];
	uint8_t			f[4];
	uint64_t		pq_value, p, q;
	size_t			pq_len;

	if (hs_nonce(o, "nonce", c->hs_nonce, sizeof(c->hs_nonce)) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "resPQ echoed a different "
		    "nonce than req_pq_multi sent (reply is not ours, or the "
		    "frame was misparsed)"));
	}
	if (hs_get_raw(o, "server_nonce", sizeof(c->hs_server_nonce),
	    &pq) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO,
		    "resPQ has no %u-byte server_nonce",
		    (unsigned int)sizeof(c->hs_server_nonce)));
	}
	memcpy(c->hs_server_nonce, pq, sizeof(c->hs_server_nonce));
	mtp_log_hex(MTP_LOG_TRACE, "resPQ server_nonce", c->hs_server_nonce,
	    sizeof(c->hs_server_nonce));
	if (mtp_object_at(o, "pq", &r) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "resPQ has no pq field"));
	}
	if ((pq = mtp_read_bytes(&r, &pq_len)) == NULL || pq_len != 8) {
		return (mtp_fail(c, MTP_ERR_PROTO, "resPQ pq is %u bytes, "
		    "expected 8", (unsigned int)pq_len));
	}
	pq_value = hs_be64(pq);
	if (mtp_factorize(pq_value, &p, &q) != 0 || p > 0xffffffffu ||
	    q > 0xffffffffu) {
		return (mtp_fail(c, MTP_ERR_CRYPTO,
		    "cannot factor server pq=%016llx into two 32-bit primes",
		    (unsigned long long)pq_value));
	}
	mtp_logf(MTP_LOG_DEBUG, "resPQ: pq=%016llx = %llu * %llu",
	    (unsigned long long)pq_value, (unsigned long long)p,
	    (unsigned long long)q);
	if ((key = hs_pick_key(c, o)) == NULL) {
		return (c->last_error);
	}
	if (lc_random(c->hs_new_nonce, sizeof(c->hs_new_nonce)) != 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "no entropy for new_nonce"));
	}
	mtp_writer_init(&w, inner, sizeof(inner));
	mtp_write_u32(&w, MTP_ID_p_q_inner_data_dc);
	mtp_write_bytes(&w, pq, pq_len);
	f[0] = (uint8_t)(p >> 24); f[1] = (uint8_t)(p >> 16);
	f[2] = (uint8_t)(p >> 8); f[3] = (uint8_t)p;
	mtp_write_bytes(&w, f, sizeof(f));
	f[0] = (uint8_t)(q >> 24); f[1] = (uint8_t)(q >> 16);
	f[2] = (uint8_t)(q >> 8); f[3] = (uint8_t)q;
	mtp_write_bytes(&w, f, sizeof(f));
	mtp_write_raw(&w, c->hs_nonce, sizeof(c->hs_nonce));
	mtp_write_raw(&w, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	mtp_write_raw(&w, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	mtp_write_i32(&w, mtp_dc_id(c->dc_index));
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_PROTO, "p_q_inner_data_dc exceeds "
		    "the %u-byte buffer", (unsigned int)sizeof(inner)));
	}
	if (hs_rsa_encrypt(key, inner, w.len, rsa) != 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO,
		    "RSA encryption of p_q_inner_data_dc failed"));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_req_DH_params);
	mtp_write_raw(&w, c->hs_nonce, sizeof(c->hs_nonce));
	mtp_write_raw(&w, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	f[0] = (uint8_t)(p >> 24); f[1] = (uint8_t)(p >> 16);
	f[2] = (uint8_t)(p >> 8); f[3] = (uint8_t)p;
	mtp_write_bytes(&w, f, sizeof(f));
	f[0] = (uint8_t)(q >> 24); f[1] = (uint8_t)(q >> 16);
	f[2] = (uint8_t)(q >> 8); f[3] = (uint8_t)q;
	mtp_write_bytes(&w, f, sizeof(f));
	mtp_write_i64(&w, key->fingerprint);
	mtp_write_bytes(&w, rsa, sizeof(rsa));
	lc_wipe(rsa, sizeof(rsa));
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_PROTO, "req_DH_params exceeds the "
		    "%u-byte buffer", (unsigned int)sizeof(body)));
	}
	if (mtp_send_plain(c, body, w.len) != MTP_OK) {
		return (c->last_error);
	}
	mtp_logf(MTP_LOG_INFO, "req_DH_params: sent %u bytes to DC%d with key "
	    "%016llx", (unsigned int)w.len, mtp_dc_id(c->dc_index),
	    (unsigned long long)key->fingerprint);
	c->hs_step = HS_STEP_DH_PARAMS;
	c->deadline = mtp_now_ms() + MTP_HANDSHAKE_TIMEOUT_MS;
	return (MTP_OK);
}

static int
hs_dh_range(const lc_bn *value, const lc_bn *prime);

static int
hs_validate_dh(mtp_client_t *c, uint32_t g, const uint8_t *prime,
    size_t prime_len, const uint8_t *g_a, size_t g_a_len)
{
	lc_bn	p, ga;
	int	ret;

	ret = -1;
	if (g < 2 || g > 7) {
		mtp_logf(MTP_LOG_ERROR, "DH: generator g=%u outside 2..7",
		    (unsigned int)g);
		return (-1);
	}
	if (prime_len != MTP_DH_PRIME_SIZE || g_a_len != MTP_DH_PRIME_SIZE) {
		mtp_logf(MTP_LOG_ERROR, "DH: dh_prime is %u bytes and g_a is %u, "
		    "both must be %u", (unsigned int)prime_len,
		    (unsigned int)g_a_len, (unsigned int)MTP_DH_PRIME_SIZE);
		return (-1);
	}
	if (mtp_dh_generator_ok(g, prime, prime_len) != 0) {
		mtp_logf(MTP_LOG_ERROR, "DH: g=%u fails its congruence against "
		    "dh_prime (small-subgroup group offered)", (unsigned int)g);
		return (-1);
	}
	lc_bn_zero(&p); lc_bn_zero(&ga);
	if (lc_bn_from_bytes(&p, prime, prime_len) != 0 ||
	    lc_bn_from_bytes(&ga, g_a, g_a_len) != 0) {
		mtp_logf(MTP_LOG_ERROR, "DH: dh_prime or g_a does not fit a "
		    "bignum");
		goto done;
	}
	if (lc_bn_bits(&p) < HS_DH_MIN_BITS) {
		mtp_logf(MTP_LOG_ERROR, "DH: dh_prime is %u bits, minimum %u",
		    (unsigned int)lc_bn_bits(&p), (unsigned int)HS_DH_MIN_BITS);
		goto done;
	}
	mtp_logf(MTP_LOG_DEBUG, "DH: primality-testing the %u-bit dh_prime",
	    (unsigned int)lc_bn_bits(&p));
	if (!lc_bn_is_prime(&p)) {
		mtp_logf(MTP_LOG_ERROR, "DH: dh_prime is composite -- the DC is "
		    "not offering a Diffie-Hellman group");
		goto done;
	}
	if (hs_dh_range(&ga, &p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "DH: g_a outside [2, dh_prime-2] or too "
		    "close to either end");
		goto done;
	}
	memcpy(c->hs_dh_prime, prime, MTP_DH_PRIME_SIZE);
	memcpy(c->hs_g_a, g_a, MTP_DH_PRIME_SIZE);
	c->hs_g = g;
	mtp_logf(MTP_LOG_INFO, "DH: accepted g=%u with a %u-bit prime",
	    (unsigned int)g, (unsigned int)lc_bn_bits(&p));
	ret = 0;
done:
	lc_bn_wipe(&p); lc_bn_wipe(&ga);
	return (ret);
}

static uint32_t
hs_bytes_mod(const uint8_t *p, size_t len, uint32_t mod)
{
	uint64_t	r;
	size_t		i;

	r = 0;
	for (i = 0; i < len; i++) {
		r = ((r << 8) | p[i]) % mod;
	}
	return ((uint32_t)r);
}


int
mtp_dh_generator_ok(uint32_t g, const uint8_t *prime, size_t prime_len)
{
	uint32_t	rem;

	switch (g) {
	case 2:
		return (hs_bytes_mod(prime, prime_len, 8) == 7 ? 0 : -1);
	case 3:
		return (hs_bytes_mod(prime, prime_len, 3) == 2 ? 0 : -1);
	case 4:
		return (hs_bytes_mod(prime, prime_len, 4) == 3 ? 0 : -1);
	case 5:
		rem = hs_bytes_mod(prime, prime_len, 5);
		return (rem == 1 || rem == 4 ? 0 : -1);
	case 6:
		rem = hs_bytes_mod(prime, prime_len, 24);
		return (rem == 19 || rem == 23 ? 0 : -1);
	case 7:
		rem = hs_bytes_mod(prime, prime_len, 7);
		return (rem == 3 || rem == 5 || rem == 6 ? 0 : -1);
	default:
		return (-1);
	}
}

static int
hs_dh_range(const lc_bn *value, const lc_bn *prime)
{
	lc_bn	one, gap;
	int	ret;

	ret = -1;
	lc_bn_zero(&one);
	lc_bn_zero(&gap);
	if (lc_bn_from_bytes(&one, "\x01", 1) != 0 ||
	    lc_bn_cmp(value, &one) <= 0 || lc_bn_cmp(value, prime) >= 0 ||
	    lc_bn_sub(&gap, prime, value) != 0 || lc_bn_bits(value) < HS_DH_MIN_BITS ||
	    lc_bn_bits(&gap) < HS_DH_MIN_BITS) {
		goto done;
	}
	ret = 0;
done:
	lc_bn_wipe(&one);
	lc_bn_wipe(&gap);
	return (ret);
}

static int
hs_tmp_encrypt(mtp_client_t *c, const uint8_t *inner, size_t inner_len,
    uint8_t *out, size_t *out_len)
{
	lc_aes_ctx	aes;
	lc_sha1_ctx	ctx;
	uint8_t		*plain;
	uint8_t		digest[LC_SHA1_DIGEST_SIZE];
	uint8_t		iv[32];
	size_t		len, padding;
	int		ret;

	padding = (LC_AES_BLOCK_SIZE - ((HS_TMP_HEAD + inner_len) %
	    LC_AES_BLOCK_SIZE)) % LC_AES_BLOCK_SIZE;
	len = HS_TMP_HEAD + inner_len + padding;
	plain = (uint8_t *)malloc(len);
	if (plain == NULL) {
		return (-1);
	}
	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, inner, inner_len);
	lc_sha1_final(&ctx, digest);
	memcpy(plain, digest, sizeof(digest));
	memcpy(plain + HS_TMP_HEAD, inner, inner_len);
	if (padding != 0 && lc_random(plain + HS_TMP_HEAD + inner_len,
	    padding) != 0) {
		lc_wipe(plain, len);
		free(plain);
		return (-1);
	}
	memcpy(iv, c->hs_tmp_aes_iv, sizeof(iv));
	ret = lc_aes_init(&aes, c->hs_tmp_aes_key, sizeof(c->hs_tmp_aes_key));
	if (ret == 0) {
		ret = lc_aes_ige_encrypt(&aes, iv, plain, out, len);
	}
	if (ret == 0) {
		*out_len = len;
	}
	lc_aes_wipe(&aes);
	lc_wipe(&ctx, sizeof(ctx));
	lc_wipe(digest, sizeof(digest));
	lc_wipe(iv, sizeof(iv));
	lc_wipe(plain, len);
	free(plain);
	return (ret);
}

static int
hs_send_client_dh(mtp_client_t *c)
{
	lc_bn		prime, base, exponent, gb, auth;
	mtp_writer_t	w;
	const char	*why;
	uint8_t		inner[512];
	uint8_t		crypt[544];
	uint8_t		body[640];
	size_t		crypt_len;
	int		attempt, ret;

	ret = MTP_ERR_CRYPTO;
	why = "unknown failure";
	lc_bn_zero(&prime); lc_bn_zero(&base); lc_bn_zero(&exponent);
	lc_bn_zero(&gb); lc_bn_zero(&auth);
	if (lc_bn_from_bytes(&prime, c->hs_dh_prime, sizeof(c->hs_dh_prime)) != 0 ||
	    lc_bn_from_bytes(&base, "\x02", 1) != 0) {
		why = "dh_prime does not fit a bignum";
		goto done;
	}
	base.limb[0] = c->hs_g;
	for (attempt = 0; attempt < HS_DH_EXP_TRIES; attempt++) {
		if (lc_random(c->hs_b, sizeof(c->hs_b)) != 0 ||
		    lc_bn_from_bytes(&exponent, c->hs_b, sizeof(c->hs_b)) != 0 ||
		    lc_bn_cmp(&exponent, &prime) >= 0 || lc_bn_bits(&exponent) < HS_DH_MIN_BITS) {
			mtp_logf(MTP_LOG_DEBUG, "DH: secret exponent draw %d "
			    "unusable, redrawing", attempt + 1);
			continue;
		}
		break;
	}
	if (attempt == HS_DH_EXP_TRIES) {
		why = "no usable DH secret exponent in 16 draws (entropy source "
		    "is likely broken)";
		goto done;
	}
	if (lc_bn_mod_exp(&gb, &base, &exponent, &prime) != 0) {
		why = "g^b mod p failed";
		goto done;
	}
	if (hs_dh_range(&gb, &prime) != 0) {
		why = "computed g_b fell outside the safe range";
		goto done;
	}
	if (lc_bn_from_bytes(&base, c->hs_g_a, sizeof(c->hs_g_a)) != 0 ||
	    lc_bn_mod_exp(&auth, &base, &exponent, &prime) != 0) {
		why = "g_a^b mod p failed";
		goto done;
	}
	if (lc_bn_to_bytes(&gb, c->hs_g_b, sizeof(c->hs_g_b)) != 0 ||
	    lc_bn_to_bytes(&auth, c->auth_key, sizeof(c->auth_key)) != 0) {
		why = "g_b or auth_key does not fit its fixed-size buffer";
		goto done;
	}
	mtp_logf(MTP_LOG_INFO, "DH: computed a %u-byte auth_key",
	    (unsigned int)sizeof(c->auth_key));
	mtp_writer_init(&w, inner, sizeof(inner));
	mtp_write_u32(&w, MTP_ID_client_DH_inner_data);
	mtp_write_raw(&w, c->hs_nonce, sizeof(c->hs_nonce));
	mtp_write_raw(&w, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	mtp_write_i64(&w, 0);
	mtp_write_bytes(&w, c->hs_g_b, sizeof(c->hs_g_b));
	if (w.overflow) {
		why = "client_DH_inner_data exceeds its buffer";
		goto done;
	}
	if (hs_tmp_encrypt(c, inner, w.len, crypt, &crypt_len) != 0) {
		why = "AES-IGE of client_DH_inner_data failed";
		goto done;
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_set_client_DH_params);
	mtp_write_raw(&w, c->hs_nonce, sizeof(c->hs_nonce));
	mtp_write_raw(&w, c->hs_server_nonce, sizeof(c->hs_server_nonce));
	mtp_write_bytes(&w, crypt, crypt_len);
	if (w.overflow) {
		why = "set_client_DH_params exceeds its buffer";
		goto done;
	}
	if (mtp_send_plain(c, body, w.len) != MTP_OK) {
		ret = c->last_error;
		why = NULL;
		goto done;
	}
	mtp_logf(MTP_LOG_INFO, "set_client_DH_params: sent %u bytes (%u "
	    "encrypted)", (unsigned int)w.len, (unsigned int)crypt_len);
	c->auth_key_valid = 1;
	c->hs_step = HS_STEP_DH_GEN;
	c->deadline = mtp_now_ms() + MTP_HANDSHAKE_TIMEOUT_MS;
	ret = MTP_OK;
done:
	lc_bn_wipe(&prime); lc_bn_wipe(&base); lc_bn_wipe(&exponent);
	lc_bn_wipe(&gb); lc_bn_wipe(&auth);
	lc_wipe(inner, sizeof(inner)); lc_wipe(crypt, sizeof(crypt));
	lc_wipe(body, sizeof(body));
	if (ret != MTP_OK) {
		lc_wipe(c->auth_key, sizeof(c->auth_key));
		c->auth_key_valid = 0;
		if (why == NULL) {
			return (ret);
		}
		return (mtp_fail(c, ret, "cannot construct DH client key: %s",
		    why));
	}
	return (MTP_OK);
}


static int
hs_handle_dh_params(mtp_client_t *c, const mtp_object_t *o)
{
	lc_aes_ctx	aes;
	lc_sha1_ctx	ctx;
	mtp_reader_t	r;
	mtp_object_t	inner;
	const uint8_t	*encrypted, *prime, *g_a;
	const char	*why;
	uint8_t		*plain;
	uint8_t		digest[LC_SHA1_DIGEST_SIZE];
	uint8_t		iv[32];
	int32_t		g;
	int32_t		server_time;
	size_t		encrypted_len, prime_len, g_a_len;
	int		ret;

	ret = MTP_ERR_PROTO;
	why = "unknown failure";
	plain = NULL;
	encrypted_len = 0;
	if (hs_nonce(o, "nonce", c->hs_nonce, sizeof(c->hs_nonce)) != 0 ||
	    hs_nonce(o, "server_nonce", c->hs_server_nonce,
	    sizeof(c->hs_server_nonce)) != 0) {
		why = "nonce or server_nonce does not match req_DH_params";
		goto done;
	}
	if (mtp_object_at(o, "encrypted_answer", &r) != 0 ||
	    (encrypted = mtp_read_bytes(&r, &encrypted_len)) == NULL) {
		why = "server_DH_params_ok has no encrypted_answer";
		goto done;
	}
	if (encrypted_len < HS_TMP_HEAD ||
	    (encrypted_len % LC_AES_BLOCK_SIZE) != 0) {
		mtp_logf(MTP_LOG_ERROR, "server_DH_params_ok: encrypted_answer "
		    "is %u bytes, need >= %u and a multiple of %u",
		    (unsigned int)encrypted_len, (unsigned int)HS_TMP_HEAD,
		    (unsigned int)LC_AES_BLOCK_SIZE);
		why = "encrypted_answer length is not a whole number of AES "
		    "blocks";
		goto done;
	}
	mtp_logf(MTP_LOG_DEBUG, "server_DH_params_ok: %u encrypted bytes",
	    (unsigned int)encrypted_len);
	hs_tmp_aes(c);
	plain = (uint8_t *)malloc(encrypted_len);
	if (plain == NULL) {
		ret = MTP_ERR_NOMEM;
		why = "out of memory for the decrypted answer";
		goto done;
	}
	memcpy(iv, c->hs_tmp_aes_iv, sizeof(iv));
	if (lc_aes_init(&aes, c->hs_tmp_aes_key, sizeof(c->hs_tmp_aes_key)) != 0 ||
	    lc_aes_ige_decrypt(&aes, iv, encrypted, plain, encrypted_len) != 0) {
		ret = MTP_ERR_CRYPTO;
		why = "AES-IGE decryption of encrypted_answer failed";
		goto done;
	}
	mtp_reader_init(&r, plain + HS_TMP_HEAD, encrypted_len - HS_TMP_HEAD);
	if (mtp_object_parse(&r, &inner) != 0 || inner.end == 0 ||
	    inner.end > encrypted_len - HS_TMP_HEAD) {
		why = "decrypted answer does not parse (temporary AES key is "
		    "probably wrong)";
		goto done;
	}
	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, plain + HS_TMP_HEAD, inner.end);
	lc_sha1_final(&ctx, digest);
	if (!lc_memeq(digest, plain, sizeof(digest))) {
		ret = MTP_ERR_CRYPTO;
		why = "SHA1 of the decrypted answer does not match its prefix";
		goto done;
	}
	if (strcmp(inner.ctor->name, "server_DH_inner_data") != 0) {
		mtp_logf(MTP_LOG_ERROR, "encrypted_answer holds %s, expected "
		    "server_DH_inner_data", inner.ctor->name);
		why = "encrypted_answer holds the wrong constructor";
		goto done;
	}
	if (hs_nonce(&inner, "nonce", c->hs_nonce, sizeof(c->hs_nonce)) != 0 ||
	    hs_nonce(&inner, "server_nonce", c->hs_server_nonce,
	    sizeof(c->hs_server_nonce)) != 0) {
		why = "server_DH_inner_data nonces do not match the outer ones";
		goto done;
	}
	if (mtp_object_at(&inner, "dh_prime", &r) != 0 ||
	    (prime = mtp_read_bytes(&r, &prime_len)) == NULL) {
		why = "server_DH_inner_data has no dh_prime";
		goto done;
	}
	if (mtp_object_at(&inner, "g_a", &r) != 0 ||
	    (g_a = mtp_read_bytes(&r, &g_a_len)) == NULL) {
		why = "server_DH_inner_data has no g_a";
		goto done;
	}
	g = mtp_object_i32(&inner, "g", 0);
	server_time = mtp_object_i32(&inner, "server_time", 0);
	if (g == 0 || server_time == 0) {
		why = "server_DH_inner_data is missing g or server_time";
		goto done;
	}
	if (hs_validate_dh(c, (uint32_t)g, prime, prime_len, g_a,
	    g_a_len) != 0) {
		why = "server DH group rejected (see the DH: lines above)";
		goto done;
	}
	c->time_offset = server_time - (int32_t)mtp_unix_time(c);
	mtp_logf(MTP_LOG_INFO, "clock: server_time=%d, offset applied %+d s",
	    (int)server_time, (int)c->time_offset);
	ret = hs_send_client_dh(c);
	why = NULL;
done:
	lc_aes_wipe(&aes);
	lc_wipe(&ctx, sizeof(ctx));
	lc_wipe(digest, sizeof(digest));
	lc_wipe(iv, sizeof(iv));
	if (plain != NULL) {
		lc_wipe(plain, encrypted_len);
		free(plain);
	}
	if (ret != MTP_OK) {
		if (why == NULL) {
			return (ret);
		}
		return (mtp_fail(c, ret, "invalid server DH parameters: %s",
		    why));
	}
	return (MTP_OK);
}

static int
hs_handle_dh_gen(mtp_client_t *c, const mtp_object_t *o)
{
	lc_sha1_ctx	ctx;
	const uint8_t	*received;
	uint8_t		auth_hash[LC_SHA1_DIGEST_SIZE];
	uint8_t		expected[MTP_NONCE_SIZE];


	if (strcmp(o->ctor->name, "dh_gen_ok") != 0) {
		return (mtp_fail(c, MTP_ERR_AUTH, "DC%d answered %s instead of "
		    "dh_gen_ok", mtp_dc_id(c->dc_index), o->ctor->name));
	}
	if (hs_nonce(o, "nonce", c->hs_nonce, sizeof(c->hs_nonce)) != 0 ||
	    hs_nonce(o, "server_nonce", c->hs_server_nonce,
	    sizeof(c->hs_server_nonce)) != 0) {
		return (mtp_fail(c, MTP_ERR_AUTH,
		    "dh_gen_ok nonces do not match this exchange"));
	}
	if (hs_get_raw(o, "new_nonce_hash1", sizeof(expected), &received) != 0) {
		return (mtp_fail(c, MTP_ERR_AUTH,
		    "dh_gen_ok has no %u-byte new_nonce_hash1",
		    (unsigned int)sizeof(expected)));
	}
	lc_sha1(c->auth_key, sizeof(c->auth_key), auth_hash);
	lc_sha1_init(&ctx);
	lc_sha1_update(&ctx, c->hs_new_nonce, sizeof(c->hs_new_nonce));
	{
		const uint8_t one = 1;
		lc_sha1_update(&ctx, &one, 1);
	}
	lc_sha1_update(&ctx, auth_hash, 8);
	lc_sha1_final(&ctx, auth_hash);
	memcpy(expected, auth_hash + 4, sizeof(expected));
	if (!lc_memeq(expected, received, sizeof(expected))) {
		lc_wipe(auth_hash, sizeof(auth_hash));
		lc_wipe(expected, sizeof(expected));
		lc_wipe(c->auth_key, sizeof(c->auth_key));
		c->auth_key_valid = 0;
		return (mtp_fail(c, MTP_ERR_AUTH, "new_nonce_hash1 mismatch: "
		    "the DC derived a different auth_key than this client"));
	}
	mtp_derive_auth_key_id(c);
	mtp_session_reset(c);
	c->server_salt = (int64_t)(hs_le64(c->hs_new_nonce) ^
	    hs_le64(c->hs_server_nonce));
	mtp_logf(MTP_LOG_INFO, "handshake complete: auth_key_id=%016llx, "
	    "salt=%016llx", (unsigned long long)c->auth_key_id,
	    (unsigned long long)c->server_salt);
	if (mtp_store_save(c) != MTP_OK) {
		return (mtp_fail(c, MTP_ERR_STORE, "cannot save the negotiated "
		    "authorization key to %s", c->auth_path));
	}
	mtp_set_state(c, MTP_STATE_INIT, "handshake complete");
	lc_wipe(&ctx, sizeof(ctx));
	lc_wipe(auth_hash, sizeof(auth_hash));
	lc_wipe(expected, sizeof(expected));
	return (MTP_OK);
}


int
mtp_handshake_begin(mtp_client_t *c)
{
	mtp_writer_t	w;
	uint8_t		body[64];

	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	if (lc_random(c->hs_nonce, sizeof(c->hs_nonce)) != 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO,
		    "no entropy for the handshake nonce"));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_req_pq_multi);
	mtp_write_raw(&w, c->hs_nonce, sizeof(c->hs_nonce));
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_PROTO, "req_pq_multi exceeds the "
		    "%u-byte buffer", (unsigned int)sizeof(body)));
	}
	if (mtp_send_plain(c, body, w.len) != MTP_OK) {
		return (c->last_error);
	}
	mtp_log_hex(MTP_LOG_TRACE, "req_pq_multi nonce", c->hs_nonce,
	    sizeof(c->hs_nonce));
	mtp_logf(MTP_LOG_INFO, "req_pq_multi: sent to DC%d, awaiting resPQ",
	    mtp_dc_id(c->dc_index));
	c->hs_step = HS_STEP_PQ;
	mtp_set_state(c, MTP_STATE_HANDSHAKE, "req_pq_multi sent");
	return (MTP_OK);
}

int
mtp_handshake_step(mtp_client_t *c, const uint8_t *frame, size_t frame_len)
{
	mtp_reader_t	r;
	mtp_object_t	o;

	if (c == NULL || frame == NULL || c->state != MTP_STATE_HANDSHAKE) {
		return (MTP_ERR_INVAL);
	}
	if (mtp_recv_plain(c, frame, frame_len, &r) != MTP_OK) {
		return (c->last_error);
	}
	if (mtp_object_parse(&r, &o) != 0) {
		mtp_log_hex(MTP_LOG_ERROR, "unparsable handshake body", r.buf,
		    r.len < 16 ? r.len : 16);
		return (mtp_fail(c, MTP_ERR_PROTO, "handshake step %d: reply "
		    "does not parse as a known constructor", c->hs_step));
	}
	mtp_logf(MTP_LOG_DEBUG, "handshake step %d: received %s", c->hs_step,
	    o.ctor->name);
	if (c->hs_step == HS_STEP_PQ && strcmp(o.ctor->name, "resPQ") == 0) {
		return (hs_send_dh_params(c, &o));
	}
	if (c->hs_step == HS_STEP_DH_PARAMS && strcmp(o.ctor->name,
	    "server_DH_params_ok") == 0) {
		return (hs_handle_dh_params(c, &o));
	}
	if (c->hs_step == HS_STEP_DH_GEN) {
		return (hs_handle_dh_gen(c, &o));
	}
	return (mtp_fail(c, MTP_ERR_PROTO, "unexpected %s at handshake step %d",
	    o.ctor->name, c->hs_step));
}

