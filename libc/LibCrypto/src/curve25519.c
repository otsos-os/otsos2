/* !DEFINES!

$define %type lc_fe as Curve25519 radix-51 field element
$define %type uint8_t as 8 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type uint128_t as compiler 128 bit unsigned
$define %func fe_0 as procedure with args lc_fe *
$define %func fe_1 as procedure with args lc_fe *
$define %func fe_carry as procedure with args lc_fe *
$define %func fe_frombytes as procedure with args lc_fe *, const uint8_t *
$define %func fe_tobytes as procedure with args uint8_t *, const lc_fe *
$define %func fe_add as procedure with args lc_fe *, const lc_fe *, const lc_fe *
$define %func fe_sub as procedure with args lc_fe *, const lc_fe *, const lc_fe *
$define %func fe_mul as procedure with args lc_fe *, const lc_fe *, const lc_fe *
$define %func fe_sq as procedure with args lc_fe *, const lc_fe *
$define %func fe_mul_small as procedure with args lc_fe *, const lc_fe *, uint64_t
$define %func fe_cswap as procedure with args lc_fe *, lc_fe *, uint64_t
$define %func fe_invert as procedure with args lc_fe *, const lc_fe *
$define %func lc_curve25519_public as function with args out public, private
$define %func lc_curve25519 as function with args out shared, private, peer public

*/

/* !SPACE!

$space %internal lc_fe, fe_0, fe_1, fe_carry, fe_frombytes, fe_tobytes
$space %internal fe_add, fe_sub, fe_mul, fe_sq, fe_mul_small
$space %internal fe_cswap, fe_invert
$space %export lc_curve25519_public, lc_curve25519

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

#include <errno.h>
#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

#define FE_MASK		0x7ffffffffffffULL
#define FE_A24		121665ULL
#define FE_MUL(a, b)	((uint128_t)(a) * (uint128_t)(b))

typedef __uint128_t	uint128_t;

typedef struct lc_fe {
	uint64_t	v[5];
} lc_fe;

static const uint8_t	g_curve25519_base[32] = { 9 };
static const uint8_t	g_fe_inv_exp[32] = {
	0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

static void
fe_0(lc_fe *f)
{
	memset(f, 0, sizeof(*f));
}

static void
fe_1(lc_fe *f)
{
	fe_0(f);
	f->v[0] = 1;
}

static void
fe_carry(lc_fe *f)
{
	uint64_t	c;
	int		i, pass;

	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < 4; i++) {
			c = f->v[i] >> 51;
			f->v[i] &= FE_MASK;
			f->v[i + 1] += c;
		}
		c = f->v[4] >> 51;
		f->v[4] &= FE_MASK;
		f->v[0] += c * 19;
	}
}

static void
fe_frombytes(lc_fe *f, const uint8_t s[32])
{
	uint8_t	t[32];

	memcpy(t, s, sizeof(t));
	t[31] &= 0x7f;
	f->v[0] = lc_load64_le(t) & FE_MASK;
	f->v[1] = (lc_load64_le(t + 6) >> 3) & FE_MASK;
	f->v[2] = (lc_load64_le(t + 12) >> 6) & FE_MASK;
	f->v[3] = (lc_load64_le(t + 19) >> 1) & FE_MASK;
	f->v[4] = (lc_load64_le(t + 24) >> 12) & FE_MASK;
	lc_wipe(t, sizeof(t));
}

static void
fe_tobytes(uint8_t s[32], const lc_fe *f)
{
	lc_fe		h;
	uint64_t	q, c;

	h = *f;
	fe_carry(&h);
	q = (19 * h.v[4] + (((uint64_t)1) << 51)) >> 51;
	q = (h.v[0] + q) >> 51;
	q = (h.v[1] + q) >> 51;
	q = (h.v[2] + q) >> 51;
	q = (h.v[3] + q) >> 51;
	q = (h.v[4] + q) >> 51;
	h.v[0] += 19 * q;
	c = h.v[0] >> 51;
	h.v[0] &= FE_MASK;
	h.v[1] += c;
	c = h.v[1] >> 51;
	h.v[1] &= FE_MASK;
	h.v[2] += c;
	c = h.v[2] >> 51;
	h.v[2] &= FE_MASK;
	h.v[3] += c;
	c = h.v[3] >> 51;
	h.v[3] &= FE_MASK;
	h.v[4] += c;
	h.v[4] &= FE_MASK;

	lc_store64_le(s, h.v[0] | (h.v[1] << 51));
	lc_store64_le(s + 8, (h.v[1] >> 13) | (h.v[2] << 38));
	lc_store64_le(s + 16, (h.v[2] >> 26) | (h.v[3] << 25));
	lc_store64_le(s + 24, (h.v[3] >> 39) | (h.v[4] << 12));
	lc_wipe(&h, sizeof(h));
}

static void
fe_add(lc_fe *out, const lc_fe *a, const lc_fe *b)
{
	int	i;

	for (i = 0; i < 5; i++) {
		out->v[i] = a->v[i] + b->v[i];
	}
	fe_carry(out);
}

static void
fe_sub(lc_fe *out, const lc_fe *a, const lc_fe *b)
{
	out->v[0] = a->v[0] + 4 * FE_MASK - 72 - b->v[0];
	out->v[1] = a->v[1] + 4 * FE_MASK - b->v[1];
	out->v[2] = a->v[2] + 4 * FE_MASK - b->v[2];
	out->v[3] = a->v[3] + 4 * FE_MASK - b->v[3];
	out->v[4] = a->v[4] + 4 * FE_MASK - b->v[4];
	fe_carry(out);
}

static void
fe_mul(lc_fe *out, const lc_fe *a, const lc_fe *b)
{
	uint128_t	c0, c1, c2, c3, c4;
	uint64_t	carry;

	c0 = FE_MUL(a->v[0], b->v[0]) + (uint128_t)19 *
	    (FE_MUL(a->v[1], b->v[4]) + FE_MUL(a->v[2], b->v[3]) +
	    FE_MUL(a->v[3], b->v[2]) + FE_MUL(a->v[4], b->v[1]));
	c1 = FE_MUL(a->v[0], b->v[1]) + FE_MUL(a->v[1], b->v[0]) +
	    (uint128_t)19 * (FE_MUL(a->v[2], b->v[4]) +
	    FE_MUL(a->v[3], b->v[3]) + FE_MUL(a->v[4], b->v[2]));
	c2 = FE_MUL(a->v[0], b->v[2]) + FE_MUL(a->v[1], b->v[1]) +
	    FE_MUL(a->v[2], b->v[0]) + (uint128_t)19 *
	    (FE_MUL(a->v[3], b->v[4]) + FE_MUL(a->v[4], b->v[3]));
	c3 = FE_MUL(a->v[0], b->v[3]) + FE_MUL(a->v[1], b->v[2]) +
	    FE_MUL(a->v[2], b->v[1]) + FE_MUL(a->v[3], b->v[0]) +
	    (uint128_t)19 * FE_MUL(a->v[4], b->v[4]);
	c4 = FE_MUL(a->v[0], b->v[4]) + FE_MUL(a->v[1], b->v[3]) +
	    FE_MUL(a->v[2], b->v[2]) + FE_MUL(a->v[3], b->v[1]) +
	    FE_MUL(a->v[4], b->v[0]);

	out->v[0] = (uint64_t)c0 & FE_MASK;
	c1 += c0 >> 51;
	out->v[1] = (uint64_t)c1 & FE_MASK;
	c2 += c1 >> 51;
	out->v[2] = (uint64_t)c2 & FE_MASK;
	c3 += c2 >> 51;
	out->v[3] = (uint64_t)c3 & FE_MASK;
	c4 += c3 >> 51;
	out->v[4] = (uint64_t)c4 & FE_MASK;
	carry = (uint64_t)(c4 >> 51);
	out->v[0] += carry * 19;
	fe_carry(out);
}

static void
fe_sq(lc_fe *out, const lc_fe *a)
{
	fe_mul(out, a, a);
}

static void
fe_mul_small(lc_fe *out, const lc_fe *a, uint64_t n)
{
	uint128_t	c;
	uint64_t	carry;
	int		i;

	carry = 0;
	for (i = 0; i < 5; i++) {
		c = (uint128_t)a->v[i] * n + carry;
		out->v[i] = (uint64_t)c & FE_MASK;
		carry = (uint64_t)(c >> 51);
	}
	out->v[0] += carry * 19;
	fe_carry(out);
}

static void
fe_cswap(lc_fe *a, lc_fe *b, uint64_t swap)
{
	uint64_t	mask, t;
	int		i;

	mask = 0 - swap;
	for (i = 0; i < 5; i++) {
		t = mask & (a->v[i] ^ b->v[i]);
		a->v[i] ^= t;
		b->v[i] ^= t;
	}
}

static void
fe_invert(lc_fe *out, const lc_fe *z)
{
	lc_fe	result, base;
	int	byte, bit;

	fe_1(&result);
	base = *z;
	for (byte = 31; byte >= 0; byte--) {
		for (bit = 7; bit >= 0; bit--) {
			fe_sq(&result, &result);
			if ((g_fe_inv_exp[byte] >> bit) & 1) {
				fe_mul(&result, &result, &base);
			}
		}
	}
	*out = result;
	lc_wipe(&result, sizeof(result));
	lc_wipe(&base, sizeof(base));
}

int
lc_curve25519_public(uint8_t out[LC_CURVE25519_POINT_SIZE],
    const uint8_t scalar[LC_CURVE25519_SCALAR_SIZE])
{
	return (lc_curve25519(out, scalar, g_curve25519_base));
}

int
lc_curve25519(uint8_t out[LC_CURVE25519_POINT_SIZE],
    const uint8_t scalar[LC_CURVE25519_SCALAR_SIZE],
    const uint8_t point[LC_CURVE25519_POINT_SIZE])
{
	uint8_t	e[32];
	lc_fe	x1, x2, z2, x3, z3;
	lc_fe	a, aa, b, bb, efd, c, d, da, cb, t0, t1;
	int	pos;
	uint64_t	bit, swap;

	if (!out || !scalar || !point) {
		errno = EINVAL;
		return (-1);
	}
	memcpy(e, scalar, sizeof(e));
	e[0] &= 248;
	e[31] &= 127;
	e[31] |= 64;

	fe_frombytes(&x1, point);
	fe_1(&x2);
	fe_0(&z2);
	x3 = x1;
	fe_1(&z3);
	swap = 0;

	for (pos = 254; pos >= 0; pos--) {
		bit = (e[pos / 8] >> (pos & 7)) & 1;
		swap ^= bit;
		fe_cswap(&x2, &x3, swap);
		fe_cswap(&z2, &z3, swap);
		swap = bit;

		fe_add(&a, &x2, &z2);
		fe_sq(&aa, &a);
		fe_sub(&b, &x2, &z2);
		fe_sq(&bb, &b);
		fe_sub(&efd, &aa, &bb);
		fe_add(&c, &x3, &z3);
		fe_sub(&d, &x3, &z3);
		fe_mul(&da, &d, &a);
		fe_mul(&cb, &c, &b);
		fe_add(&t0, &da, &cb);
		fe_sq(&x3, &t0);
		fe_sub(&t0, &da, &cb);
		fe_sq(&t1, &t0);
		fe_mul(&z3, &x1, &t1);
		fe_mul(&x2, &aa, &bb);
		fe_mul_small(&t0, &efd, FE_A24);
		fe_add(&t1, &aa, &t0);
		fe_mul(&z2, &efd, &t1);
	}
	fe_cswap(&x2, &x3, swap);
	fe_cswap(&z2, &z3, swap);
	fe_invert(&z2, &z2);
	fe_mul(&x2, &x2, &z2);
	fe_tobytes(out, &x2);

	lc_wipe(e, sizeof(e));
	lc_wipe(&x1, sizeof(x1));
	lc_wipe(&x2, sizeof(x2));
	lc_wipe(&z2, sizeof(z2));
	lc_wipe(&x3, sizeof(x3));
	lc_wipe(&z3, sizeof(z3));
	lc_wipe(&a, sizeof(a));
	lc_wipe(&aa, sizeof(aa));
	lc_wipe(&b, sizeof(b));
	lc_wipe(&bb, sizeof(bb));
	lc_wipe(&efd, sizeof(efd));
	lc_wipe(&c, sizeof(c));
	lc_wipe(&d, sizeof(d));
	lc_wipe(&da, sizeof(da));
	lc_wipe(&cb, sizeof(cb));
	lc_wipe(&t0, sizeof(t0));
	lc_wipe(&t1, sizeof(t1));
	return (0);
}
