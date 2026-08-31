/* !DEFINES!

$define %type mtp_srp as Telegram SRP-6a cloud-password state
$define %func mtp_srp_reset as procedure with args client
$define %func mtp_srp_take_params as function with args client, account.password object
$define %func mtp_srp_begin as function with args client, password
$define %func mtp_srp_step as function with args client
$define %func mtp_srp_ready as function with args client
$define %func mtp_srp_busy as function with args client
$define %func mtp_srp_write_check as function with args client, writer
$define %func mtp_srp_hint as function with args client
$define %func mtp_srp_have_params as function with args client

*/

/* !SPACE!

$space %internal srp_pad, srp_sh, srp_generator_ok, srp_take_algo
$space %internal srp_finish, srp_state_name
$space %export mtp_srp_reset, mtp_srp_take_params, mtp_srp_begin
$space %export mtp_srp_step, mtp_srp_ready, mtp_srp_busy
$space %export mtp_srp_write_check, mtp_srp_hint, mtp_srp_have_params

*/

/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
 * SUBSTITUTE GOODS OR SERVICES; LOSS, USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "mtp_internal.h"
#include "mtp_schema.h"

#include <libcrypto.h>
#include <string.h>
#define SRP_PBKDF2_ITERS	100000u
#define SRP_PBKDF2_BUDGET	2000u
#define SRP_P_BITS		2048
#define SRP_NUM_SIZE		256
#define SRP_IDLE		0
#define SRP_CHECK_P		1
#define SRP_CHECK_Q		2
#define SRP_PBKDF2		3
#define SRP_FINISH		4
#define SRP_READY		5


static void
srp_sh(const uint8_t *data, size_t data_len, const uint8_t *salt,
    size_t salt_len, uint8_t out[LC_SHA256_DIGEST_SIZE])
{
	lc_sha256_ctx	ctx;

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, salt, salt_len);
	lc_sha256_update(&ctx, data, data_len);
	lc_sha256_update(&ctx, salt, salt_len);
	lc_sha256_final(&ctx, out);
	lc_wipe(&ctx, sizeof(ctx));
}


static int
srp_pad(const lc_bn *v, uint8_t out[SRP_NUM_SIZE])
{
	return (lc_bn_to_bytes(v, out, SRP_NUM_SIZE));
}

static int
srp_generator_ok(uint32_t g, const uint8_t *p, size_t p_len)
{
	return (mtp_dh_generator_ok(g, p, p_len));
}

static const char *
srp_state_name(int state)
{
	switch (state) {
	case SRP_IDLE: return ("idle");
	case SRP_CHECK_P: return ("checking p");
	case SRP_CHECK_Q: return ("checking (p-1)/2");
	case SRP_PBKDF2: return ("pbkdf2");
	case SRP_FINISH: return ("finishing");
	case SRP_READY: return ("ready");
	default: return ("unknown");
	}
}

void
mtp_srp_reset(mtp_client_t *c)
{
	if (c == NULL) {
		return;
	}
	lc_wipe(&c->srp, sizeof(c->srp));
	c->srp.state = SRP_IDLE;
}

static int
srp_take_algo(mtp_client_t *c, const mtp_object_t *algo)
{
	mtp_reader_t	r;
	const uint8_t	*b;
	size_t		len;

	if (algo->ctor->id !=
	    MTP_ID_passwordKdfAlgoSHA256SHA256PBKDF2HMACSHA512iter100000SHA256ModPow) {
		return (mtp_soft_fail(c, MTP_ERR_UNSUPPORTED, "account.password "
		    "offers KDF %s, this client implements only "
		    "SHA256..PBKDF2HMACSHA512iter100000..ModPow",
		    algo->ctor->name));
	}

	if (mtp_object_at(algo, "salt1", &r) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: current_algo has no "
		    "salt1"));
	}
	b = mtp_read_bytes(&r, &len);
	if (b == NULL || len == 0 || len > sizeof(c->srp.salt1)) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: salt1 is %u bytes, "
		    "expected 1..%u", (unsigned int)len,
		    (unsigned int)sizeof(c->srp.salt1)));
	}
	memcpy(c->srp.salt1, b, len);
	c->srp.salt1_len = len;

	if (mtp_object_at(algo, "salt2", &r) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: current_algo has no "
		    "salt2"));
	}
	b = mtp_read_bytes(&r, &len);
	if (b == NULL || len == 0 || len > sizeof(c->srp.salt2)) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: salt2 is %u bytes, "
		    "expected 1..%u", (unsigned int)len,
		    (unsigned int)sizeof(c->srp.salt2)));
	}
	memcpy(c->srp.salt2, b, len);
	c->srp.salt2_len = len;

	c->srp.g = (uint32_t)mtp_object_i32(algo, "g", 0);

	if (mtp_object_at(algo, "p", &r) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: current_algo has no p"));
	}
	b = mtp_read_bytes(&r, &len);
	if (b == NULL || len != SRP_NUM_SIZE) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: p is %u bytes, expected "
		    "%u", (unsigned int)len, (unsigned int)SRP_NUM_SIZE));
	}
	memcpy(c->srp.p, b, len);
	return (MTP_OK);
}


int
mtp_srp_take_params(mtp_client_t *c, const mtp_object_t *o)
{
	mtp_object_t	algo;
	mtp_reader_t	r;
	const uint8_t	*b;
	size_t		len;
	int		ret;

	if (c == NULL || o == NULL) {
		return (MTP_ERR_INVAL);
	}
	mtp_srp_reset(c);

	if (o->ctor->id != MTP_ID_account_password) {
		return (mtp_fail(c, MTP_ERR_PROTO, "account.getPassword returned "
		    "%s, expected account.password", o->ctor->name));
	}
	if (mtp_object_has(o, "has_password") == 0) {
		return (mtp_soft_fail(c, MTP_ERR_AUTH, "account.password reports "
		    "no cloud password set, yet sign-in asked for one"));
	}

	if (mtp_object_at(o, "current_algo", &r) != 0 ||
	    mtp_object_parse(&r, &algo) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: current_algo missing or "
		    "not a known constructor"));
	}
	ret = srp_take_algo(c, &algo);
	if (ret != MTP_OK) {
		return (ret);
	}

	if (mtp_object_at(o, "srp_B", &r) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: account.password has no "
		    "srp_B"));
	}
	b = mtp_read_bytes(&r, &len);
	if (b == NULL || len == 0 || len > SRP_NUM_SIZE) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: srp_B is %u bytes, "
		    "expected 1..%u", (unsigned int)len,
		    (unsigned int)SRP_NUM_SIZE));
	}
	memset(c->srp.g_b, 0, SRP_NUM_SIZE);
	memcpy(c->srp.g_b + (SRP_NUM_SIZE - len), b, len);

	c->srp.srp_id = mtp_object_i64(o, "srp_id", 0);
	if (c->srp.srp_id == 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: account.password carried "
		    "no srp_id, so the check cannot be addressed to a session"));
	}
	(void)mtp_object_str(o, "hint", c->srp.hint, sizeof(c->srp.hint));

	c->srp.have_params = 1;
	mtp_logf(MTP_LOG_INFO, "SRP: parameters received, g=%u salt1=%u salt2=%u "
	    "hint=%s", (unsigned int)c->srp.g, (unsigned int)c->srp.salt1_len,
	    (unsigned int)c->srp.salt2_len,
	    c->srp.hint[0] != '\0' ? "present" : "none");
	return (MTP_OK);
}

const char *
mtp_srp_hint(const mtp_client_t *c)
{
	if (c == NULL || c->srp.have_params == 0) {
		return ("");
	}
	return (c->srp.hint);
}

int
mtp_srp_have_params(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return (c->srp.have_params != 0);
}


int
mtp_srp_begin(mtp_client_t *c, const char *password)
{
	size_t	len;
	int	bits, r;

	if (c == NULL || password == NULL) {
		return (MTP_ERR_INVAL);
	}
	if (c->srp.have_params == 0) {
		return (mtp_soft_fail(c, MTP_ERR_NOTREADY, "SRP: no parameters "
		    "yet, account.getPassword has to complete first"));
	}
	if (c->srp.state != SRP_IDLE) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "SRP: already %s",
		    srp_state_name(c->srp.state)));
	}
	len = strlen(password);
	if (len == 0 || len >= sizeof(c->srp.password)) {
		return (mtp_soft_fail(c, MTP_ERR_INVAL, "SRP: password length %u "
		    "outside 1..%u", (unsigned int)len,
		    (unsigned int)sizeof(c->srp.password) - 1));
	}

	if (c->srp.g < 2 || c->srp.g > 7) {
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: g=%u outside 2..7",
		    (unsigned int)c->srp.g));
	}
	if (srp_generator_ok(c->srp.g, c->srp.p, SRP_NUM_SIZE) != 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: g=%u fails its "
		    "congruence against p, which makes it a small-subgroup "
		    "generator", (unsigned int)c->srp.g));
	}
	if (lc_bn_from_bytes(&c->srp.bn_p, c->srp.p, SRP_NUM_SIZE) != 0 ||
	    lc_bn_from_bytes(&c->srp.bn_b, c->srp.g_b, SRP_NUM_SIZE) != 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: p or srp_B does not fit "
		    "a bignum"));
	}
	bits = lc_bn_bits(&c->srp.bn_p);
	if (bits != SRP_P_BITS) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: p is %d bits, the spec "
		    "requires exactly %d", bits, SRP_P_BITS));
	}
	if (lc_bn_is_odd(&c->srp.bn_p) == 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: p is even"));
	}
	if (lc_bn_cmp_u32(&c->srp.bn_b, 1) <= 0 ||
	    lc_bn_cmp(&c->srp.bn_b, &c->srp.bn_p) >= 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: srp_B outside "
		    "[2, p-1]"));
	}

	memcpy(c->srp.password, password, len);
	c->srp.password[len] = '\0';
	c->srp.password_len = len;

	r = lc_bn_is_prime_init(&c->srp.prime, &c->srp.bn_p);
	if (r < 0) {
		return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: cannot start the "
		    "primality test on p"));
	}
	c->srp.state = SRP_CHECK_P;
	mtp_logf(MTP_LOG_INFO, "SRP: validating the %d-bit group, then deriving "
	    "the key (%u PBKDF2 rounds)", bits, (unsigned int)SRP_PBKDF2_ITERS);
	return (MTP_OK);
}


static int
srp_finish(mtp_client_t *c)
{
	lc_sha256_ctx	ctx;
	lc_bn		bn_g, bn_x, bn_v, bn_k, bn_u, bn_kv, bn_t, bn_e, bn_s;
	lc_bn		bn_a, bn_ga, bn_ux;
	uint8_t		pad_p[SRP_NUM_SIZE], pad_g[SRP_NUM_SIZE];
	uint8_t		pad_ga[SRP_NUM_SIZE], pad_gb[SRP_NUM_SIZE];
	uint8_t		pad_v[SRP_NUM_SIZE], pad_s[SRP_NUM_SIZE];
	uint8_t		h_p[LC_SHA256_DIGEST_SIZE], h_g[LC_SHA256_DIGEST_SIZE];
	uint8_t		h_s1[LC_SHA256_DIGEST_SIZE], h_s2[LC_SHA256_DIGEST_SIZE];
	uint8_t		h_sa[LC_SHA256_DIGEST_SIZE], x[LC_SHA256_DIGEST_SIZE];
	uint8_t		a_raw[SRP_NUM_SIZE], mix[LC_SHA256_DIGEST_SIZE];
	size_t		i;
	int		ret;

	ret = MTP_ERR_CRYPTO;
	lc_bn_zero(&bn_g); lc_bn_zero(&bn_x); lc_bn_zero(&bn_v);
	lc_bn_zero(&bn_k); lc_bn_zero(&bn_u); lc_bn_zero(&bn_kv);
	lc_bn_zero(&bn_t); lc_bn_zero(&bn_e); lc_bn_zero(&bn_s);
	lc_bn_zero(&bn_a); lc_bn_zero(&bn_ga); lc_bn_zero(&bn_ux);

	srp_sh(c->srp.pbkdf2_out, sizeof(c->srp.pbkdf2_out), c->srp.salt2,
	    c->srp.salt2_len, x);
	if (lc_bn_from_bytes(&bn_x, x, sizeof(x)) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: x does not fit a bignum");
		goto done;
	}

	{
		uint8_t	gb[4];

		gb[0] = (uint8_t)(c->srp.g >> 24);
		gb[1] = (uint8_t)(c->srp.g >> 16);
		gb[2] = (uint8_t)(c->srp.g >> 8);
		gb[3] = (uint8_t)c->srp.g;
		if (lc_bn_from_bytes(&bn_g, gb, sizeof(gb)) != 0) {
			goto done;
		}
	}
	if (srp_pad(&c->srp.bn_p, pad_p) != 0 || srp_pad(&bn_g, pad_g) != 0 ||
	    srp_pad(&c->srp.bn_b, pad_gb) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: p, g or srp_B will not pad to %u "
		    "bytes", (unsigned int)SRP_NUM_SIZE);
		goto done;
	}

	if (lc_bn_mod_exp(&bn_v, &bn_g, &bn_x, &c->srp.bn_p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: v = g^x mod p failed");
		goto done;
	}
	if (srp_pad(&bn_v, pad_v) != 0) {
		goto done;
	}

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, pad_p, SRP_NUM_SIZE);
	lc_sha256_update(&ctx, pad_g, SRP_NUM_SIZE);
	lc_sha256_final(&ctx, mix);
	if (lc_bn_from_bytes(&bn_k, mix, LC_SHA256_DIGEST_SIZE) != 0) {
		goto done;
	}


	if (lc_random(a_raw, sizeof(a_raw)) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: no entropy for the client secret");
		ret = MTP_ERR_CRYPTO;
		goto done;
	}
	if (lc_bn_from_bytes(&bn_a, a_raw, sizeof(a_raw)) != 0) {
		goto done;
	}

	if (lc_bn_cmp(&bn_a, &c->srp.bn_p) >= 0) {
		a_raw[0] = 0;
		if (lc_bn_from_bytes(&bn_a, a_raw, sizeof(a_raw)) != 0) {
			goto done;
		}
	}
	if (lc_bn_cmp_u32(&bn_a, 1) <= 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: entropy produced a degenerate "
		    "client secret");
		goto done;
	}

	if (lc_bn_mod_exp(&bn_ga, &bn_g, &bn_a, &c->srp.bn_p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: g_a = g^a mod p failed");
		goto done;
	}
	if (srp_pad(&bn_ga, pad_ga) != 0) {
		goto done;
	}

	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, pad_ga, SRP_NUM_SIZE);
	lc_sha256_update(&ctx, pad_gb, SRP_NUM_SIZE);
	lc_sha256_final(&ctx, mix);
	if (lc_bn_from_bytes(&bn_u, mix, LC_SHA256_DIGEST_SIZE) != 0) {
		goto done;
	}

	if (bn_u.used == 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: u hashed to zero");
		goto done;
	}

	if (lc_bn_mod_mul(&bn_kv, &bn_k, &bn_v, &c->srp.bn_p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: k*v mod p failed");
		goto done;
	}
	if (lc_bn_mod_sub(&bn_t, &c->srp.bn_b, &bn_kv, &c->srp.bn_p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: (srp_B - k*v) mod p failed");
		goto done;
	}
	if (bn_t.used == 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: srp_B equals k*v mod p, so the "
		    "session key would be zero for any password");
		goto done;
	}


	if (lc_bn_mul(&bn_ux, &bn_u, &bn_x) != 0 ||
	    lc_bn_add(&bn_e, &bn_a, &bn_ux) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: a + u*x overflowed");
		goto done;
	}

	if (lc_bn_mod_exp(&bn_s, &bn_t, &bn_e, &c->srp.bn_p) != 0) {
		mtp_logf(MTP_LOG_ERROR, "SRP: s_a = t^(a+u*x) mod p failed");
		goto done;
	}
	if (srp_pad(&bn_s, pad_s) != 0) {
		goto done;
	}


	lc_sha256(pad_p, SRP_NUM_SIZE, h_p);
	lc_sha256(pad_g, SRP_NUM_SIZE, h_g);
	lc_sha256(c->srp.salt1, c->srp.salt1_len, h_s1);
	lc_sha256(c->srp.salt2, c->srp.salt2_len, h_s2);
	lc_sha256(pad_s, SRP_NUM_SIZE, h_sa);
	for (i = 0; i < LC_SHA256_DIGEST_SIZE; i++) {
		mix[i] = (uint8_t)(h_p[i] ^ h_g[i]);
	}
	lc_sha256_init(&ctx);
	lc_sha256_update(&ctx, mix, LC_SHA256_DIGEST_SIZE);
	lc_sha256_update(&ctx, h_s1, LC_SHA256_DIGEST_SIZE);
	lc_sha256_update(&ctx, h_s2, LC_SHA256_DIGEST_SIZE);
	lc_sha256_update(&ctx, pad_ga, SRP_NUM_SIZE);
	lc_sha256_update(&ctx, pad_gb, SRP_NUM_SIZE);
	lc_sha256_update(&ctx, h_sa, LC_SHA256_DIGEST_SIZE);
	lc_sha256_final(&ctx, c->srp.m1);

	memcpy(c->srp.a_pub, pad_ga, SRP_NUM_SIZE);
	c->srp.state = SRP_READY;
	mtp_logf(MTP_LOG_INFO, "SRP: proof built for srp_id set");
	ret = MTP_OK;
done:
	lc_wipe(&ctx, sizeof(ctx));
	lc_bn_wipe(&bn_g); lc_bn_wipe(&bn_x); lc_bn_wipe(&bn_v);
	lc_bn_wipe(&bn_k); lc_bn_wipe(&bn_u); lc_bn_wipe(&bn_kv);
	lc_bn_wipe(&bn_t); lc_bn_wipe(&bn_e); lc_bn_wipe(&bn_s);
	lc_bn_wipe(&bn_a); lc_bn_wipe(&bn_ga); lc_bn_wipe(&bn_ux);
	lc_wipe(pad_v, sizeof(pad_v));
	lc_wipe(pad_s, sizeof(pad_s));
	lc_wipe(h_sa, sizeof(h_sa));
	lc_wipe(x, sizeof(x));
	lc_wipe(a_raw, sizeof(a_raw));
	lc_wipe(mix, sizeof(mix));
	lc_wipe(c->srp.pbkdf2_out, sizeof(c->srp.pbkdf2_out));
	lc_wipe(c->srp.password, sizeof(c->srp.password));
	c->srp.password_len = 0;
	return (ret);
}


int
mtp_srp_step(mtp_client_t *c)
{
	uint8_t	ph1[LC_SHA256_DIGEST_SIZE], inner[LC_SHA256_DIGEST_SIZE];
	int	r, ret;

	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	switch (c->srp.state) {
	case SRP_IDLE:
	case SRP_READY:
		return (MTP_OK);

	case SRP_CHECK_P:
		r = lc_bn_is_prime_step(&c->srp.prime);
		if (r < 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: primality test "
			    "on p failed to run"));
		}
		if (r == 0) {
			return (MTP_OK);
		}
		if (lc_bn_is_prime_result(&c->srp.prime) != 1) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: p is composite "
			    "-- the DC is not offering an SRP group"));
		}
		lc_bn_is_prime_wipe(&c->srp.prime);
		if (lc_bn_sub_u32(&c->srp.bn_q, &c->srp.bn_p, 1) != 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: cannot form "
			    "p-1"));
		}
		lc_bn_rshift1(&c->srp.bn_q, &c->srp.bn_q);
		if (lc_bn_is_prime_init(&c->srp.prime, &c->srp.bn_q) < 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: cannot start "
			    "the primality test on (p-1)/2"));
		}
		c->srp.state = SRP_CHECK_Q;
		return (MTP_OK);

	case SRP_CHECK_Q:
		r = lc_bn_is_prime_step(&c->srp.prime);
		if (r < 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: primality test "
			    "on (p-1)/2 failed to run"));
		}
		if (r == 0) {
			return (MTP_OK);
		}
		if (lc_bn_is_prime_result(&c->srp.prime) != 1) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: p is prime but "
			    "(p-1)/2 is not, so p is not a safe prime"));
		}
		lc_bn_is_prime_wipe(&c->srp.prime);
		lc_bn_wipe(&c->srp.bn_q);
		mtp_logf(MTP_LOG_INFO, "SRP: group accepted (safe prime, g=%u "
		    "is a quadratic residue)", (unsigned int)c->srp.g);
		srp_sh((const uint8_t *)c->srp.password, c->srp.password_len,
		    c->srp.salt1, c->srp.salt1_len, inner);
		srp_sh(inner, sizeof(inner), c->srp.salt2, c->srp.salt2_len,
		    ph1);
		r = lc_pbkdf2_sha512_init(&c->srp.kdf, ph1, sizeof(ph1),
		    c->srp.salt1, c->srp.salt1_len, SRP_PBKDF2_ITERS);
		lc_wipe(inner, sizeof(inner));
		lc_wipe(ph1, sizeof(ph1));
		if (r != 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: cannot start "
			    "PBKDF2"));
		}
		c->srp.state = SRP_PBKDF2;
		return (MTP_OK);

	case SRP_PBKDF2:
		r = lc_pbkdf2_sha512_step(&c->srp.kdf, SRP_PBKDF2_BUDGET);
		if (r < 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: PBKDF2 step "
			    "failed"));
		}
		if (r == 0) {
			return (MTP_OK);
		}
		r = lc_pbkdf2_sha512_final(&c->srp.kdf, c->srp.pbkdf2_out,
		    sizeof(c->srp.pbkdf2_out));
		lc_pbkdf2_sha512_wipe(&c->srp.kdf);
		if (r != 0) {
			return (mtp_fail(c, MTP_ERR_CRYPTO, "SRP: PBKDF2 refused "
			    "to produce its output"));
		}
		c->srp.state = SRP_FINISH;
		return (MTP_OK);

	case SRP_FINISH:
		ret = srp_finish(c);
		if (ret != MTP_OK) {
			c->srp.state = SRP_IDLE;
			c->srp.have_params = 0;
			return (mtp_soft_fail(c, MTP_ERR_CRYPTO, "SRP: could not "
			    "build the password proof"));
		}
		return (MTP_OK);

	default:
		return (mtp_fail(c, MTP_ERR_PROTO, "SRP: state %d is not a state",
		    c->srp.state));
	}
}

int
mtp_srp_ready(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return (c->srp.state == SRP_READY);
}


int
mtp_srp_busy(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return (c->srp.state > SRP_IDLE && c->srp.state < SRP_READY);
}


int
mtp_srp_write_check(mtp_client_t *c, mtp_writer_t *w)
{
	if (c == NULL || w == NULL) {
		return (MTP_ERR_INVAL);
	}
	if (c->srp.state != SRP_READY) {
		return (mtp_soft_fail(c, MTP_ERR_NOTREADY, "SRP: proof is not "
		    "built yet (%s)", srp_state_name(c->srp.state)));
	}
	mtp_write_u32(w, MTP_ID_inputCheckPasswordSRP);
	mtp_write_i64(w, c->srp.srp_id);
	mtp_write_bytes(w, c->srp.a_pub, sizeof(c->srp.a_pub));
	mtp_write_bytes(w, c->srp.m1, sizeof(c->srp.m1));
	return (MTP_OK);
}
