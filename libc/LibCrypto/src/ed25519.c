/* !DEFINES!

$define %type ed_fe as Ed25519 radix-51 field element
$define %type ed_point as extended Edwards point
$define %type uint8_t as 8 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type uint128_t as compiler 128 bit unsigned
$define %func fe_0 as procedure with args ed_fe *
$define %func fe_1 as procedure with args ed_fe *
$define %func fe_carry as procedure with args ed_fe *
$define %func fe_frombytes as procedure with args ed_fe *, const uint8_t *
$define %func fe_tobytes as procedure with args uint8_t *, const ed_fe *
$define %func fe_add as procedure with args ed_fe *, const ed_fe *, const ed_fe *
$define %func fe_sub as procedure with args ed_fe *, const ed_fe *, const ed_fe *
$define %func fe_neg as procedure with args ed_fe *, const ed_fe *
$define %func fe_mul as procedure with args ed_fe *, const ed_fe *, const ed_fe *
$define %func fe_sq as procedure with args ed_fe *, const ed_fe *
$define %func fe_pow as procedure with args ed_fe *, const ed_fe *, const uint8_t *
$define %func fe_invert as procedure with args ed_fe *, const ed_fe *
$define %func fe_isnegative as function with args const ed_fe *
$define %func fe_iszero as function with args const ed_fe *
$define %func fe_equal as function with args const ed_fe *, const ed_fe *
$define %func fe_is_canonical as function with args const uint8_t *
$define %func point_identity as procedure with args ed_point *
$define %func point_is_identity as function with args const ed_point *
$define %func point_add as procedure with args ed_point *, const ed_point *, const ed_point *
$define %func point_double as procedure with args ed_point *, const ed_point *
$define %func point_decode as function with args ed_point *, const uint8_t *
$define %func point_encode as procedure with args uint8_t *, const ed_point *
$define %func scalar_ge_l as function with args const uint8_t *
$define %func scalar_sub_l as procedure with args uint8_t *
$define %func scalar_reduce as procedure with args uint8_t *, const uint8_t *
$define %func scalar_is_canonical as function with args const uint8_t *
$define %func scalar_bit as function with args const uint8_t *, int
$define %func point_scalar_mul as procedure with args ed_point *, const ed_point *, const uint8_t *
$define %func point_base_mul as function with args ed_point *, const uint8_t *
$define %func point_is_prime_order as function with args const ed_point *
$define %func lc_ed25519_verify as function with args public key, message, message length, signature

*/

/* !SPACE!

$space %internal ed_fe, ed_point, fe_0, fe_1, fe_carry, fe_frombytes
$space %internal fe_tobytes, fe_add, fe_sub, fe_neg, fe_mul, fe_sq
$space %internal fe_pow, fe_invert, fe_isnegative, fe_iszero, fe_equal
$space %internal fe_is_canonical, point_identity, point_is_identity
$space %internal point_add, point_double, point_decode, point_encode
$space %internal scalar_ge_l, scalar_sub_l, scalar_reduce
$space %internal scalar_is_canonical, scalar_bit, point_scalar_mul
$space %internal point_base_mul, point_is_prime_order
$space %export lc_ed25519_verify

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

#define ED_MASK		0x7ffffffffffffULL
#define ED_MUL(a, b)	((uint128_t)(a) * (uint128_t)(b))

typedef __uint128_t	uint128_t;

typedef struct ed_fe {
	uint64_t	v[5];
} ed_fe;

typedef struct ed_point {
	ed_fe	x;
	ed_fe	y;
	ed_fe	z;
	ed_fe	t;
} ed_point;

static const uint8_t	g_ed_p[32] = {
	0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

static const uint8_t	g_ed_l[32] = {
	0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58,
	0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

static const uint8_t	g_ed_d[32] = {
	0xa3, 0x78, 0x59, 0x13, 0xca, 0x4d, 0xeb, 0x75,
	0xab, 0xd8, 0x41, 0x41, 0x4d, 0x0a, 0x70, 0x00,
	0x98, 0xe8, 0x79, 0x77, 0x79, 0x40, 0xc7, 0x8c,
	0x73, 0xfe, 0x6f, 0x2b, 0xee, 0x6c, 0x03, 0x52
};

static const uint8_t	g_ed_sqrtm1[32] = {
	0xb0, 0xa0, 0x0e, 0x4a, 0x27, 0x1b, 0xee, 0xc4,
	0x78, 0xe4, 0x2f, 0xad, 0x06, 0x18, 0x43, 0x2f,
	0xa7, 0xd7, 0xfb, 0x3d, 0x99, 0x00, 0x4d, 0x2b,
	0x0b, 0xdf, 0xc1, 0x4f, 0x80, 0x24, 0x83, 0x2b
};

static const uint8_t	g_ed_base[32] = {
	0x58, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
	0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66
};

static const uint8_t	g_fe_inv_exp[32] = {
	0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

static const uint8_t	g_fe_sqrt_exp[32] = {
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f
};

static void
fe_0(ed_fe *f)
{
	memset(f, 0, sizeof(*f));
}

static void
fe_1(ed_fe *f)
{
	fe_0(f);
	f->v[0] = 1;
}

static void
fe_carry(ed_fe *f)
{
	uint64_t	c;
	int		i, pass;

	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < 4; i++) {
			c = f->v[i] >> 51;
			f->v[i] &= ED_MASK;
			f->v[i + 1] += c;
		}
		c = f->v[4] >> 51;
		f->v[4] &= ED_MASK;
		f->v[0] += c * 19;
	}
}

static void
fe_frombytes(ed_fe *f, const uint8_t s[32])
{
	uint8_t	t[32];

	memcpy(t, s, sizeof(t));
	t[31] &= 0x7f;
	f->v[0] = lc_load64_le(t) & ED_MASK;
	f->v[1] = (lc_load64_le(t + 6) >> 3) & ED_MASK;
	f->v[2] = (lc_load64_le(t + 12) >> 6) & ED_MASK;
	f->v[3] = (lc_load64_le(t + 19) >> 1) & ED_MASK;
	f->v[4] = (lc_load64_le(t + 24) >> 12) & ED_MASK;
	lc_wipe(t, sizeof(t));
}

static void
fe_tobytes(uint8_t s[32], const ed_fe *f)
{
	ed_fe		h;
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
	h.v[0] &= ED_MASK;
	h.v[1] += c;
	c = h.v[1] >> 51;
	h.v[1] &= ED_MASK;
	h.v[2] += c;
	c = h.v[2] >> 51;
	h.v[2] &= ED_MASK;
	h.v[3] += c;
	c = h.v[3] >> 51;
	h.v[3] &= ED_MASK;
	h.v[4] += c;
	h.v[4] &= ED_MASK;

	lc_store64_le(s, h.v[0] | (h.v[1] << 51));
	lc_store64_le(s + 8, (h.v[1] >> 13) | (h.v[2] << 38));
	lc_store64_le(s + 16, (h.v[2] >> 26) | (h.v[3] << 25));
	lc_store64_le(s + 24, (h.v[3] >> 39) | (h.v[4] << 12));
	lc_wipe(&h, sizeof(h));
}

static void
fe_add(ed_fe *out, const ed_fe *a, const ed_fe *b)
{
	int	i;

	for (i = 0; i < 5; i++) {
		out->v[i] = a->v[i] + b->v[i];
	}
	fe_carry(out);
}

static void
fe_sub(ed_fe *out, const ed_fe *a, const ed_fe *b)
{
	out->v[0] = a->v[0] + 4 * ED_MASK - 72 - b->v[0];
	out->v[1] = a->v[1] + 4 * ED_MASK - b->v[1];
	out->v[2] = a->v[2] + 4 * ED_MASK - b->v[2];
	out->v[3] = a->v[3] + 4 * ED_MASK - b->v[3];
	out->v[4] = a->v[4] + 4 * ED_MASK - b->v[4];
	fe_carry(out);
}

static void
fe_neg(ed_fe *out, const ed_fe *a)
{
	ed_fe	zero;

	fe_0(&zero);
	fe_sub(out, &zero, a);
}

static void
fe_mul(ed_fe *out, const ed_fe *a, const ed_fe *b)
{
	uint128_t	c0, c1, c2, c3, c4;
	uint64_t	carry;

	c0 = ED_MUL(a->v[0], b->v[0]) + (uint128_t)19 *
	    (ED_MUL(a->v[1], b->v[4]) + ED_MUL(a->v[2], b->v[3]) +
	    ED_MUL(a->v[3], b->v[2]) + ED_MUL(a->v[4], b->v[1]));
	c1 = ED_MUL(a->v[0], b->v[1]) + ED_MUL(a->v[1], b->v[0]) +
	    (uint128_t)19 * (ED_MUL(a->v[2], b->v[4]) +
	    ED_MUL(a->v[3], b->v[3]) + ED_MUL(a->v[4], b->v[2]));
	c2 = ED_MUL(a->v[0], b->v[2]) + ED_MUL(a->v[1], b->v[1]) +
	    ED_MUL(a->v[2], b->v[0]) + (uint128_t)19 *
	    (ED_MUL(a->v[3], b->v[4]) + ED_MUL(a->v[4], b->v[3]));
	c3 = ED_MUL(a->v[0], b->v[3]) + ED_MUL(a->v[1], b->v[2]) +
	    ED_MUL(a->v[2], b->v[1]) + ED_MUL(a->v[3], b->v[0]) +
	    (uint128_t)19 * ED_MUL(a->v[4], b->v[4]);
	c4 = ED_MUL(a->v[0], b->v[4]) + ED_MUL(a->v[1], b->v[3]) +
	    ED_MUL(a->v[2], b->v[2]) + ED_MUL(a->v[3], b->v[1]) +
	    ED_MUL(a->v[4], b->v[0]);

	out->v[0] = (uint64_t)c0 & ED_MASK;
	c1 += c0 >> 51;
	out->v[1] = (uint64_t)c1 & ED_MASK;
	c2 += c1 >> 51;
	out->v[2] = (uint64_t)c2 & ED_MASK;
	c3 += c2 >> 51;
	out->v[3] = (uint64_t)c3 & ED_MASK;
	c4 += c3 >> 51;
	out->v[4] = (uint64_t)c4 & ED_MASK;
	carry = (uint64_t)(c4 >> 51);
	out->v[0] += carry * 19;
	fe_carry(out);
}

static void
fe_sq(ed_fe *out, const ed_fe *a)
{
	fe_mul(out, a, a);
}

static void
fe_pow(ed_fe *out, const ed_fe *z, const uint8_t exp[32])
{
	ed_fe	result, base;
	int	byte, bit;

	fe_1(&result);
	base = *z;
	for (byte = 31; byte >= 0; byte--) {
		for (bit = 7; bit >= 0; bit--) {
			fe_sq(&result, &result);
			if ((exp[byte] >> bit) & 1) {
				fe_mul(&result, &result, &base);
			}
		}
	}
	*out = result;
	lc_wipe(&result, sizeof(result));
	lc_wipe(&base, sizeof(base));
}

static void
fe_invert(ed_fe *out, const ed_fe *z)
{
	fe_pow(out, z, g_fe_inv_exp);
}

static int
fe_isnegative(const ed_fe *f)
{
	uint8_t	s[32];
	int	ret;

	fe_tobytes(s, f);
	ret = s[0] & 1;
	lc_wipe(s, sizeof(s));
	return (ret);
}

static int
fe_iszero(const ed_fe *f)
{
	uint8_t	s[32];
	uint8_t	acc;
	int	i;

	fe_tobytes(s, f);
	acc = 0;
	for (i = 0; i < 32; i++) {
		acc |= s[i];
	}
	lc_wipe(s, sizeof(s));
	return (acc == 0);
}

static int
fe_equal(const ed_fe *a, const ed_fe *b)
{
	uint8_t	aa[32];
	uint8_t	bb[32];
	int	ret;

	fe_tobytes(aa, a);
	fe_tobytes(bb, b);
	ret = lc_memeq(aa, bb, sizeof(aa));
	lc_wipe(aa, sizeof(aa));
	lc_wipe(bb, sizeof(bb));
	return (ret);
}

static int
fe_is_canonical(const uint8_t in[32])
{
	uint8_t	t[32];
	int	i;

	memcpy(t, in, sizeof(t));
	t[31] &= 0x7f;
	for (i = 31; i >= 0; i--) {
		if (t[i] < g_ed_p[i]) {
			lc_wipe(t, sizeof(t));
			return (1);
		}
		if (t[i] > g_ed_p[i]) {
			lc_wipe(t, sizeof(t));
			return (0);
		}
	}
	lc_wipe(t, sizeof(t));
	return (0);
}

static void
point_identity(ed_point *p)
{
	fe_0(&p->x);
	fe_1(&p->y);
	fe_1(&p->z);
	fe_0(&p->t);
}

static int
point_is_identity(const ed_point *p)
{
	return (fe_iszero(&p->x) && fe_equal(&p->y, &p->z));
}

static void
point_add(ed_point *out, const ed_point *p, const ed_point *q)
{
	ed_fe	d, d2, a, b, c, dtmp, e, f, g, h;
	ed_fe	y1mx1, y1px1, y2mx2, y2px2;
	ed_fe	tmp1;

	fe_frombytes(&d, g_ed_d);
	fe_add(&d2, &d, &d);
	fe_sub(&y1mx1, &p->y, &p->x);
	fe_add(&y1px1, &p->y, &p->x);
	fe_sub(&y2mx2, &q->y, &q->x);
	fe_add(&y2px2, &q->y, &q->x);
	fe_mul(&a, &y1mx1, &y2mx2);
	fe_mul(&b, &y1px1, &y2px2);
	fe_mul(&tmp1, &p->t, &q->t);
	fe_mul(&c, &tmp1, &d2);
	fe_mul(&tmp1, &p->z, &q->z);
	fe_add(&dtmp, &tmp1, &tmp1);
	fe_sub(&e, &b, &a);
	fe_sub(&f, &dtmp, &c);
	fe_add(&g, &dtmp, &c);
	fe_add(&h, &b, &a);
	fe_mul(&out->x, &e, &f);
	fe_mul(&out->y, &g, &h);
	fe_mul(&out->t, &e, &h);
	fe_mul(&out->z, &f, &g);
	lc_wipe(&d, sizeof(d));
	lc_wipe(&d2, sizeof(d2));
	lc_wipe(&tmp1, sizeof(tmp1));
}

static void
point_double(ed_point *out, const ed_point *p)
{
	ed_fe	a, b, c, d, e, f, g, h;
	ed_fe	tmp1, tmp2;

	fe_sq(&a, &p->x);
	fe_sq(&b, &p->y);
	fe_sq(&tmp1, &p->z);
	fe_add(&c, &tmp1, &tmp1);
	fe_neg(&d, &a);
	fe_add(&tmp1, &p->x, &p->y);
	fe_sq(&tmp2, &tmp1);
	fe_sub(&tmp1, &tmp2, &a);
	fe_sub(&e, &tmp1, &b);
	fe_add(&g, &d, &b);
	fe_sub(&f, &g, &c);
	fe_sub(&h, &d, &b);
	fe_mul(&out->x, &e, &f);
	fe_mul(&out->y, &g, &h);
	fe_mul(&out->t, &e, &h);
	fe_mul(&out->z, &f, &g);
	lc_wipe(&tmp1, sizeof(tmp1));
	lc_wipe(&tmp2, sizeof(tmp2));
}

static int
point_decode(ed_point *p, const uint8_t in[32])
{
	uint8_t	ybytes[32];
	ed_fe	y, y2, u, v, inv_v, x2, x, check, sqrtm1, d;
	int	sign;

	if (!fe_is_canonical(in)) {
		return (0);
	}
	sign = in[31] >> 7;
	memcpy(ybytes, in, sizeof(ybytes));
	ybytes[31] &= 0x7f;
	fe_frombytes(&y, ybytes);
	fe_sq(&y2, &y);
	fe_1(&u);
	fe_sub(&u, &y2, &u);
	fe_frombytes(&d, g_ed_d);
	fe_mul(&v, &d, &y2);
	fe_1(&check);
	fe_add(&v, &v, &check);
	fe_invert(&inv_v, &v);
	fe_mul(&x2, &u, &inv_v);
	fe_pow(&x, &x2, g_fe_sqrt_exp);
	fe_sq(&check, &x);
	if (!fe_equal(&check, &x2)) {
		fe_frombytes(&sqrtm1, g_ed_sqrtm1);
		fe_mul(&x, &x, &sqrtm1);
		fe_sq(&check, &x);
		if (!fe_equal(&check, &x2)) {
			lc_wipe(ybytes, sizeof(ybytes));
			return (0);
		}
	}
	if (fe_iszero(&x) && sign) {
		lc_wipe(ybytes, sizeof(ybytes));
		return (0);
	}
	if (fe_isnegative(&x) != sign) {
		fe_neg(&x, &x);
	}
	p->x = x;
	p->y = y;
	fe_1(&p->z);
	fe_mul(&p->t, &x, &y);
	lc_wipe(ybytes, sizeof(ybytes));
	return (1);
}

static void
point_encode(uint8_t out[32], const ed_point *p)
{
	ed_fe	zinv, x, y;

	fe_invert(&zinv, &p->z);
	fe_mul(&x, &p->x, &zinv);
	fe_mul(&y, &p->y, &zinv);
	fe_tobytes(out, &y);
	out[31] |= (uint8_t)(fe_isnegative(&x) << 7);
	lc_wipe(&zinv, sizeof(zinv));
	lc_wipe(&x, sizeof(x));
	lc_wipe(&y, sizeof(y));
}

static int
scalar_ge_l(const uint8_t s[32])
{
	int	i;

	for (i = 31; i >= 0; i--) {
		if (s[i] > g_ed_l[i]) {
			return (1);
		}
		if (s[i] < g_ed_l[i]) {
			return (0);
		}
	}
	return (1);
}

static void
scalar_sub_l(uint8_t s[32])
{
	int	borrow, i, v;

	borrow = 0;
	for (i = 0; i < 32; i++) {
		v = (int)s[i] - (int)g_ed_l[i] - borrow;
		if (v < 0) {
			v += 256;
			borrow = 1;
		} else {
			borrow = 0;
		}
		s[i] = (uint8_t)v;
	}
}

static void
scalar_reduce(uint8_t out[32], const uint8_t in[64])
{
	uint32_t	v, carry;
	int	bit, i;

	memset(out, 0, 32);
	for (bit = 511; bit >= 0; bit--) {
		carry = (uint32_t)((in[bit / 8] >> (bit & 7)) & 1);
		for (i = 0; i < 32; i++) {
			v = ((uint32_t)out[i] << 1) | carry;
			out[i] = (uint8_t)v;
			carry = v >> 8;
		}
		if (scalar_ge_l(out)) {
			scalar_sub_l(out);
		}
	}
}

static int
scalar_is_canonical(const uint8_t s[32])
{
	return (!scalar_ge_l(s));
}

static int
scalar_bit(const uint8_t s[32], int bit)
{
	return ((s[bit / 8] >> (bit & 7)) & 1);
}

static void
point_scalar_mul(ed_point *out, const ed_point *p, const uint8_t scalar[32])
{
	ed_point	q;
	int		i;

	point_identity(&q);
	for (i = 255; i >= 0; i--) {
		point_double(&q, &q);
		if (scalar_bit(scalar, i)) {
			point_add(&q, &q, p);
		}
	}
	*out = q;
	lc_wipe(&q, sizeof(q));
}

static int
point_base_mul(ed_point *out, const uint8_t scalar[32])
{
	ed_point	base;

	if (!point_decode(&base, g_ed_base)) {
		return (0);
	}
	point_scalar_mul(out, &base, scalar);
	lc_wipe(&base, sizeof(base));
	return (1);
}

static int
point_is_prime_order(const ed_point *p)
{
	ed_point	check;
	int	ret;

	point_scalar_mul(&check, p, g_ed_l);
	ret = point_is_identity(&check);
	lc_wipe(&check, sizeof(check));
	return (ret);
}

int
lc_ed25519_verify(const uint8_t public_key[LC_ED25519_PUBLIC_KEY_SIZE],
    const void *message, size_t message_len,
    const uint8_t signature[LC_ED25519_SIGNATURE_SIZE])
{
	lc_sha512_ctx	ctx;
	ed_point	a, r, sb, ha, rhs;
	uint8_t	h_digest[LC_SHA512_DIGEST_SIZE];
	uint8_t	h[32], lhs_enc[32], rhs_enc[32];
	int	ret;

	if (!public_key || (!message && message_len != 0) || !signature) {
		errno = EINVAL;
		return (-1);
	}
	if (!scalar_is_canonical(signature + 32)) {
		errno = EINVAL;
		return (-1);
	}
	if (!point_decode(&a, public_key) || point_is_identity(&a) ||
	    !point_is_prime_order(&a)) {
		errno = EINVAL;
		return (-1);
	}
	if (!point_decode(&r, signature) || point_is_identity(&r) ||
	    !point_is_prime_order(&r)) {
		errno = EINVAL;
		return (-1);
	}

	lc_sha512_init(&ctx);
	lc_sha512_update(&ctx, signature, 32);
	lc_sha512_update(&ctx, public_key, LC_ED25519_PUBLIC_KEY_SIZE);
	lc_sha512_update(&ctx, message, message_len);
	lc_sha512_final(&ctx, h_digest);
	scalar_reduce(h, h_digest);

	if (!point_base_mul(&sb, signature + 32)) {
		errno = EINVAL;
		return (-1);
	}
	point_scalar_mul(&ha, &a, h);
	point_add(&rhs, &r, &ha);
	point_encode(lhs_enc, &sb);
	point_encode(rhs_enc, &rhs);
	ret = lc_memeq(lhs_enc, rhs_enc, sizeof(lhs_enc)) ? 0 : -1;
	if (ret != 0) {
		errno = EINVAL;
	}

	lc_wipe(&a, sizeof(a));
	lc_wipe(&r, sizeof(r));
	lc_wipe(&sb, sizeof(sb));
	lc_wipe(&ha, sizeof(ha));
	lc_wipe(&rhs, sizeof(rhs));
	lc_wipe(h_digest, sizeof(h_digest));
	lc_wipe(h, sizeof(h));
	lc_wipe(lhs_enc, sizeof(lhs_enc));
	lc_wipe(rhs_enc, sizeof(rhs_enc));
	return (ret);
}
