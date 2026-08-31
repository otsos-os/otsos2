/* !DEFINES!

$define %type lc_bn as fixed capacity big unsigned integer
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %func bn_trim as procedure with args lc_bn *
$define %func bn_set_u32 as procedure with args lc_bn *, uint32_t
$define %func bn_add_into as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func bn_sub_into as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func bn_mont_n0 as function with args uint32_t
$define %func bn_mont_mul as procedure with args lc_bn *, const lc_bn *, const lc_bn *, const lc_bn *, uint32_t
$define %func bn_mont_r2 as procedure with args lc_bn *, const lc_bn *
$define %func lc_bn_zero as procedure with args lc_bn *
$define %func lc_bn_from_bytes as function with args lc_bn *, const void *, size_t
$define %func lc_bn_to_bytes as function with args const lc_bn *, void *, size_t
$define %func lc_bn_cmp as function with args const lc_bn *, const lc_bn *
$define %func lc_bn_cmp_u32 as function with args const lc_bn *, uint32_t
$define %func lc_bn_bits as function with args const lc_bn *
$define %func lc_bn_is_odd as function with args const lc_bn *
$define %func lc_bn_sub as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func lc_bn_sub_u32 as function with args lc_bn *, const lc_bn *, uint32_t
$define %func lc_bn_rshift1 as procedure with args lc_bn *, const lc_bn *
$define %func lc_bn_mod_exp as function with args lc_bn *, base, exp, mod
$define %func lc_bn_is_prime as function with args const lc_bn *
$define %func lc_bn_wipe as procedure with args lc_bn *
$define %func lc_bn_add as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func lc_bn_mul as function with args lc_bn *, const lc_bn *, const lc_bn *
$define %func lc_bn_mod_mul as function with args lc_bn *, a, b, mod
$define %func lc_bn_mod_sub as function with args lc_bn *, a, b, mod

*/

/* !SPACE!

$space %internal bn_trim, bn_set_u32, bn_add_into, bn_sub_into
$space %internal bn_mont_n0, bn_mont_mul, bn_mont_r2
$space %export lc_bn_zero, lc_bn_from_bytes, lc_bn_to_bytes
$space %export lc_bn_cmp, lc_bn_cmp_u32, lc_bn_bits, lc_bn_is_odd
$space %export lc_bn_sub, lc_bn_sub_u32, lc_bn_rshift1
$space %export lc_bn_mod_exp, lc_bn_is_prime, lc_bn_wipe
$space %export lc_bn_add, lc_bn_mul, lc_bn_mod_mul, lc_bn_mod_sub

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
 *    and/or other materials propagated with the distribution.
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



#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void
bn_trim(lc_bn *a)
{
	while (a->used > 0 && a->limb[a->used - 1] == 0) {
		a->used--;
	}
}

static void
bn_set_u32(lc_bn *a, uint32_t v)
{
	memset(a->limb, 0, sizeof(a->limb));
	a->limb[0] = v;
	a->used = (v != 0) ? 1 : 0;
}

void
lc_bn_zero(lc_bn *a)
{
	if (a != NULL) {
		memset(a, 0, sizeof(*a));
	}
}

void
lc_bn_wipe(lc_bn *a)
{
	if (a != NULL) {
		lc_wipe(a, sizeof(*a));
	}
}

int
lc_bn_is_odd(const lc_bn *a)
{
	if (a == NULL || a->used == 0) {
		return (0);
	}
	return ((int)(a->limb[0] & 1u));
}

int
lc_bn_bits(const lc_bn *a)
{
	uint32_t	top;
	int		bits;

	if (a == NULL || a->used == 0) {
		return (0);
	}
	top = a->limb[a->used - 1];
	bits = (int)(a->used - 1) * 32;
	while (top != 0) {
		bits++;
		top >>= 1;
	}
	return (bits);
}

int
lc_bn_from_bytes(lc_bn *a, const void *buf, size_t len)
{
	const uint8_t	*p;
	size_t		i;

	if (a == NULL || (buf == NULL && len != 0)) {
		return (-1);
	}
	if (len > (size_t)LC_BN_LIMBS * 4) {
		return (-1);
	}

	lc_bn_zero(a);
	p = (const uint8_t *)buf;
	for (i = 0; i < len; i++) {
		size_t	pos;

		pos = len - 1 - i;
		a->limb[i / 4] |= (uint32_t)p[pos] << ((i % 4) * 8);
	}
	a->used = (uint32_t)((len + 3) / 4);
	bn_trim(a);
	return (0);
}

int
lc_bn_to_bytes(const lc_bn *a, void *buf, size_t len)
{
	uint8_t		*p;
	size_t		i, nbytes;

	if (a == NULL || buf == NULL) {
		return (-1);
	}
	nbytes = (size_t)(lc_bn_bits(a) + 7) / 8;
	if (nbytes > len) {
		return (-1);
	}

	p = (uint8_t *)buf;
	memset(p, 0, len);
	for (i = 0; i < nbytes; i++) {
		p[len - 1 - i] = (uint8_t)(a->limb[i / 4] >> ((i % 4) * 8));
	}
	return (0);
}

int
lc_bn_cmp(const lc_bn *a, const lc_bn *b)
{
	uint32_t	i;

	if (a->used != b->used) {
		return ((a->used > b->used) ? 1 : -1);
	}
	for (i = a->used; i > 0; i--) {
		if (a->limb[i - 1] != b->limb[i - 1]) {
			return ((a->limb[i - 1] > b->limb[i - 1]) ? 1 : -1);
		}
	}
	return (0);
}

int
lc_bn_cmp_u32(const lc_bn *a, uint32_t v)
{
	if (a->used > 1) {
		return (1);
	}
	if (a->used == 0) {
		return ((v == 0) ? 0 : -1);
	}
	if (a->limb[0] == v) {
		return (0);
	}
	return ((a->limb[0] > v) ? 1 : -1);
}

static int
bn_sub_into(lc_bn *r, const lc_bn *a, const lc_bn *b)
{
	uint64_t	borrow, cur;
	uint32_t	i, n;

	n = (a->used > b->used) ? a->used : b->used;
	borrow = 0;
	for (i = 0; i < n; i++) {
		cur = (uint64_t)((i < a->used) ? a->limb[i] : 0);
		cur -= (uint64_t)((i < b->used) ? b->limb[i] : 0) + borrow;
		r->limb[i] = (uint32_t)cur;
		borrow = ((cur >> 32) != 0) ? 1 : 0;
	}
	for (i = n; i < LC_BN_LIMBS; i++) {
		r->limb[i] = 0;
	}
	r->used = n;
	bn_trim(r);
	return ((borrow != 0) ? -1 : 0);
}

int
lc_bn_sub(lc_bn *r, const lc_bn *a, const lc_bn *b)
{
	if (r == NULL || a == NULL || b == NULL) {
		return (-1);
	}
	if (lc_bn_cmp(a, b) < 0) {
		return (-1);
	}
	return (bn_sub_into(r, a, b));
}

int
lc_bn_sub_u32(lc_bn *r, const lc_bn *a, uint32_t v)
{
	lc_bn	tmp;
	int	ret;

	if (r == NULL || a == NULL) {
		return (-1);
	}
	lc_bn_zero(&tmp);
	bn_set_u32(&tmp, v);
	ret = lc_bn_sub(r, a, &tmp);
	lc_bn_wipe(&tmp);
	return (ret);
}

void
lc_bn_rshift1(lc_bn *r, const lc_bn *a)
{
	uint32_t	i, carry, next;

	if (r == NULL || a == NULL) {
		return;
	}
	carry = 0;
	for (i = a->used; i > 0; i--) {
		next = a->limb[i - 1] & 1u;
		r->limb[i - 1] = (a->limb[i - 1] >> 1) | (carry << 31);
		carry = next;
	}
	for (i = a->used; i < LC_BN_LIMBS; i++) {
		r->limb[i] = 0;
	}
	r->used = a->used;
	bn_trim(r);
}


static uint32_t
bn_mont_n0(uint32_t n)
{
	uint32_t	x;
	int		i;

	x = 1;
	for (i = 0; i < 5; i++) {
		x *= 2u - n * x;
	}
	return (0u - x);
}


static void
bn_mont_mul(lc_bn *r, const lc_bn *a, const lc_bn *b, const lc_bn *n,
    uint32_t n0)
{
	uint32_t	t[LC_BN_LIMBS + 2];
	uint64_t	acc;
	uint32_t	carry, m, i, j, len;

	len = n->used;
	memset(t, 0, sizeof(t[0]) * (len + 2));

	for (i = 0; i < len; i++) {
		carry = 0;
		for (j = 0; j < len; j++) {
			acc = (uint64_t)a->limb[j] *
			    (uint64_t)((i < b->used) ? b->limb[i] : 0) +
			    (uint64_t)t[j] + (uint64_t)carry;
			t[j] = (uint32_t)acc;
			carry = (uint32_t)(acc >> 32);
		}
		acc = (uint64_t)t[len] + (uint64_t)carry;
		t[len] = (uint32_t)acc;
		t[len + 1] = (uint32_t)(acc >> 32);

		m = (uint32_t)((uint64_t)t[0] * (uint64_t)n0);
		carry = 0;
		for (j = 0; j < len; j++) {
			acc = (uint64_t)m * (uint64_t)n->limb[j] +
			    (uint64_t)t[j] + (uint64_t)carry;
			t[j] = (uint32_t)acc;
			carry = (uint32_t)(acc >> 32);
		}
		acc = (uint64_t)t[len] + (uint64_t)carry;
		t[len] = (uint32_t)acc;
		t[len + 1] += (uint32_t)(acc >> 32);

		for (j = 0; j <= len; j++) {
			t[j] = t[j + 1];
		}
		t[len + 1] = 0;
	}

	for (j = 0; j <= len; j++) {
		r->limb[j] = t[j];
	}
	for (j = len + 1; j < LC_BN_LIMBS; j++) {
		r->limb[j] = 0;
	}
	r->used = len + 1;
	bn_trim(r);
	if (lc_bn_cmp(r, n) >= 0) {
		(void)bn_sub_into(r, r, n);
	}
	lc_wipe(t, sizeof(t));
}

static void
bn_dbl_mod(lc_bn *a, const lc_bn *n)
{
	uint32_t	i, carry, next;

	carry = 0;
	for (i = 0; i < n->used; i++) {
		next = a->limb[i] >> 31;
		a->limb[i] = (a->limb[i] << 1) | carry;
		carry = next;
	}
	a->limb[n->used] = carry;
	a->used = n->used + 1;
	bn_trim(a);
	if (lc_bn_cmp(a, n) >= 0) {
		(void)bn_sub_into(a, a, n);
	}
}

static void
bn_mont_r2(lc_bn *r2, const lc_bn *n)
{
	uint32_t	i, count;

	bn_set_u32(r2, 1);
	if (lc_bn_cmp(r2, n) >= 0) {
		(void)bn_sub_into(r2, r2, n);
	}
	count = n->used * 64u;
	for (i = 0; i < count; i++) {
		bn_dbl_mod(r2, n);
	}
}

static int
bn_bit(const lc_bn *a, uint32_t index)
{
	if (index / 32u >= a->used) {
		return (0);
	}
	return ((int)((a->limb[index / 32u] >> (index % 32u)) & 1u));
}


int
lc_bn_mod_exp(lc_bn *r, const lc_bn *base, const lc_bn *exp, const lc_bn *mod)
{
	lc_bn		r2, acc, b_mont;
	uint32_t	n0;
	int		bits, i;

	if (r == NULL || base == NULL || exp == NULL || mod == NULL) {
		return (-1);
	}
	if (mod->used == 0 || (mod->limb[0] & 1u) == 0) {
		return (-1);
	}
	if (lc_bn_cmp(base, mod) >= 0) {
		return (-1);
	}

	lc_bn_zero(&r2);
	lc_bn_zero(&acc);
	lc_bn_zero(&b_mont);

	n0 = bn_mont_n0(mod->limb[0]);
	bn_mont_r2(&r2, mod);

	bn_set_u32(&acc, 1);
	bn_mont_mul(&acc, &acc, &r2, mod, n0);
	bn_mont_mul(&b_mont, base, &r2, mod, n0);

	bits = lc_bn_bits(exp);
	for (i = bits - 1; i >= 0; i--) {
		bn_mont_mul(&acc, &acc, &acc, mod, n0);
		if (bn_bit(exp, (uint32_t)i) != 0) {
			bn_mont_mul(&acc, &acc, &b_mont, mod, n0);
		}
	}

	bn_set_u32(&r2, 1);
	bn_mont_mul(r, &acc, &r2, mod, n0);

	lc_bn_wipe(&acc);
	lc_bn_wipe(&b_mont);
	lc_bn_wipe(&r2);
	return (0);
}

static uint32_t
bn_mod_u32(const lc_bn *a, uint32_t small)
{
	uint64_t	rem;
	uint32_t	i;

	rem = 0;
	for (i = a->used; i > 0; i--) {
		rem = ((rem << 32) | (uint64_t)a->limb[i - 1]) % small;
	}
	return ((uint32_t)rem);
}

static const uint32_t	g_small_primes[] = {
	3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67,
	71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139,
	149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211
};

#define BN_SMALL_PRIMES \
    (sizeof(g_small_primes) / sizeof(g_small_primes[0]))


static const uint32_t	g_mr_bases[] = {
	2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47
};

#define BN_MR_BASES \
    (sizeof(g_mr_bases) / sizeof(g_mr_bases[0]))

int
lc_bn_is_prime(const lc_bn *n)
{
	lc_bn_prime_ctx	ctx;
	int		r;

	r = lc_bn_is_prime_init(&ctx, n);
	while (r == 0) {
		r = lc_bn_is_prime_step(&ctx);
	}
	if (r < 0) {
		lc_bn_is_prime_wipe(&ctx);
		return (-1);
	}
	r = lc_bn_is_prime_result(&ctx);
	lc_bn_is_prime_wipe(&ctx);
	return (r);
}


int
lc_bn_is_prime_init(lc_bn_prime_ctx *ctx, const lc_bn *n)
{
	uint32_t	i;

	if (ctx == NULL || n == NULL) {
		return (-1);
	}
	memset(ctx, 0, sizeof(*ctx));
	ctx->verdict = -1;

	if (lc_bn_cmp_u32(n, 2) < 0) {
		ctx->verdict = 0;
		return (1);
	}
	if (lc_bn_cmp_u32(n, 2) == 0) {
		ctx->verdict = 1;
		return (1);
	}
	if (lc_bn_is_odd(n) == 0) {
		ctx->verdict = 0;
		return (1);
	}
	for (i = 0; i < BN_SMALL_PRIMES; i++) {
		if (lc_bn_cmp_u32(n, g_small_primes[i]) == 0) {
			ctx->verdict = 1;
			return (1);
		}
		if (bn_mod_u32(n, g_small_primes[i]) == 0) {
			ctx->verdict = 0;
			return (1);
		}
	}

	ctx->n = *n;
	if (lc_bn_sub_u32(&ctx->n1, n, 1) != 0) {
		return (-1);
	}
	ctx->d = ctx->n1;
	ctx->s = 0;
	while (lc_bn_is_odd(&ctx->d) == 0 && ctx->d.used != 0) {
		lc_bn_rshift1(&ctx->d, &ctx->d);
		ctx->s++;
	}
	ctx->n0 = bn_mont_n0(n->limb[0]);
	bn_mont_r2(&ctx->r2, n);
	ctx->next_base = 0;
	return (0);
}

int
lc_bn_is_prime_step(lc_bn_prime_ctx *ctx)
{
	lc_bn	a, x, a_mont;
	int	j, ret;

	if (ctx == NULL) {
		return (-1);
	}
	if (ctx->verdict >= 0) {
		return (1);
	}
	if (ctx->next_base >= BN_MR_BASES) {
		ctx->verdict = 1;
		return (1);
	}

	lc_bn_zero(&a);
	lc_bn_zero(&x);
	lc_bn_zero(&a_mont);
	ret = 0;

	bn_set_u32(&a, g_mr_bases[ctx->next_base]);
	ctx->next_base++;
	if (lc_bn_cmp(&a, &ctx->n1) >= 0) {
		goto done;
	}
	if (lc_bn_mod_exp(&x, &a, &ctx->d, &ctx->n) != 0) {
		ret = -1;
		goto done;
	}
	if (lc_bn_cmp_u32(&x, 1) == 0 || lc_bn_cmp(&x, &ctx->n1) == 0) {
		goto done;
	}
	bn_mont_mul(&a_mont, &x, &ctx->r2, &ctx->n, ctx->n0);
	for (j = 1; j < ctx->s; j++) {
		bn_mont_mul(&a_mont, &a_mont, &a_mont, &ctx->n, ctx->n0);
		bn_set_u32(&x, 1);
		bn_mont_mul(&x, &a_mont, &x, &ctx->n, ctx->n0);
		if (lc_bn_cmp(&x, &ctx->n1) == 0) {
			break;
		}
		if (lc_bn_cmp_u32(&x, 1) == 0) {
			j = ctx->s;
			break;
		}
	}
	if (j >= ctx->s) {
		ctx->verdict = 0;
		ret = 1;
	}
done:
	lc_bn_wipe(&a);
	lc_bn_wipe(&x);
	lc_bn_wipe(&a_mont);
	if (ret == 0 && ctx->next_base >= BN_MR_BASES) {
		ctx->verdict = 1;
		ret = 1;
	}
	return (ret);
}

int
lc_bn_is_prime_result(const lc_bn_prime_ctx *ctx)
{
	if (ctx == NULL) {
		return (-1);
	}
	return (ctx->verdict);
}

void
lc_bn_is_prime_wipe(lc_bn_prime_ctx *ctx)
{
	if (ctx == NULL) {
		return;
	}
	lc_wipe(ctx, sizeof(*ctx));
}


int
lc_bn_add(lc_bn *r, const lc_bn *a, const lc_bn *b)
{
	uint64_t	acc;
	uint32_t	i, n, carry;

	if (r == NULL || a == NULL || b == NULL) {
		return (-1);
	}
	n = (a->used > b->used) ? a->used : b->used;
	carry = 0;
	for (i = 0; i < n; i++) {
		acc = (uint64_t)((i < a->used) ? a->limb[i] : 0);
		acc += (uint64_t)((i < b->used) ? b->limb[i] : 0);
		acc += (uint64_t)carry;
		r->limb[i] = (uint32_t)acc;
		carry = (uint32_t)(acc >> 32);
	}
	if (carry != 0) {
		if (n >= LC_BN_LIMBS) {
			return (-1);
		}
		r->limb[n] = carry;
		n++;
	}
	for (i = n; i < LC_BN_LIMBS; i++) {
		r->limb[i] = 0;
	}
	r->used = n;
	bn_trim(r);
	return (0);
}


int
lc_bn_mul(lc_bn *r, const lc_bn *a, const lc_bn *b)
{
	uint32_t	t[LC_BN_LIMBS];
	uint64_t	acc;
	uint32_t	i, j, carry, need;

	if (r == NULL || a == NULL || b == NULL) {
		return (-1);
	}
	if (a->used == 0 || b->used == 0) {
		lc_bn_zero(r);
		return (0);
	}

	need = a->used + b->used;
	if (need > LC_BN_LIMBS) {
		return (-1);
	}
	memset(t, 0, sizeof(t));
	for (i = 0; i < a->used; i++) {
		carry = 0;
		for (j = 0; j < b->used; j++) {
			acc = (uint64_t)a->limb[i] * (uint64_t)b->limb[j];
			acc += (uint64_t)t[i + j] + (uint64_t)carry;
			t[i + j] = (uint32_t)acc;
			carry = (uint32_t)(acc >> 32);
		}
		if (i + b->used < LC_BN_LIMBS) {
			t[i + b->used] += carry;
		} else if (carry != 0) {
			lc_wipe(t, sizeof(t));
			return (-1);
		}
	}
	memcpy(r->limb, t, sizeof(t));
	r->used = need;
	bn_trim(r);
	lc_wipe(t, sizeof(t));
	return (0);
}


int
lc_bn_mod_mul(lc_bn *r, const lc_bn *a, const lc_bn *b, const lc_bn *n)
{
	lc_bn		r2, t;
	uint32_t	n0;

	if (r == NULL || a == NULL || b == NULL || n == NULL) {
		return (-1);
	}
	if (n->used == 0 || (n->limb[0] & 1u) == 0) {
		return (-1);
	}
	if (lc_bn_cmp(a, n) >= 0 || lc_bn_cmp(b, n) >= 0) {
		return (-1);
	}
	lc_bn_zero(&r2);
	lc_bn_zero(&t);
	n0 = bn_mont_n0(n->limb[0]);
	bn_mont_r2(&r2, n);
	bn_mont_mul(&t, a, b, n, n0);
	bn_mont_mul(r, &t, &r2, n, n0);
	lc_bn_wipe(&r2);
	lc_bn_wipe(&t);
	return (0);
}


int
lc_bn_mod_sub(lc_bn *r, const lc_bn *a, const lc_bn *b, const lc_bn *n)
{
	lc_bn	t;
	int	ret;

	if (r == NULL || a == NULL || b == NULL || n == NULL) {
		return (-1);
	}
	if (lc_bn_cmp(a, n) >= 0 || lc_bn_cmp(b, n) >= 0) {
		return (-1);
	}
	if (lc_bn_cmp(a, b) >= 0) {
		return (bn_sub_into(r, a, b));
	}
	lc_bn_zero(&t);
	ret = -1;
	if (lc_bn_add(&t, a, n) == 0) {
		ret = bn_sub_into(r, &t, b);
	}
	lc_bn_wipe(&t);
	return (ret);
}
