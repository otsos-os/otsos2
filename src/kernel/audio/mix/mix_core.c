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

$define %type mix_gain_t as fixed-point linear gain (Q15)
$define %type mix_account_t as per-sample signed saturation accumulator
$define %type mix_source_t as one weight+data input slice
$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s32 as 32 bit signed
$define %type s64 as 64 bit signed

$const MIX_SAMPLE_* as per-sample saturation bounds (Q15 domain)
$const MIX_CONTAINER_* as sample container encodings

$define %func mix_apply_gain_c as procedure with args u32, void *, const void *, u64, mix_gain_t
$define %func mix_accumulate_c as procedure with args u32, mix_account_t *, const void *, u64, mix_gain_t
$define %func mix_account_saturate_c as procedure with args u32, void *, const mix_account_t *, u64
$define %func mix_merge_c as procedure with args u32, void *, const void *, u64, mix_gain_t
$define %func mix_zero_c as procedure with args u32, void *, u64
$define %func mix_mixdown as function with args u32, mix_gain_t, u32, const mix_source_t *, void *, u64
$define %func mix_apply_crossfade as procedure with args u32, const void *, const void *, void *, u64, mix_gain_t, mix_gain_t
$define %func mix_scale_q15 as function with args s64, u64
$define %func mix_read_sample as function with args u32, const u8 *
$define %func mix_write_sample as procedure with args u32, u8 *, s64
$define %func mix_sample_width as function with args u32
$define %func mix_ieee32_to_s64 as function with args u32
$define %func mix_s64_to_ieee32 as function with args s64
$define %func mix_ieee64_to_s64 as function with args u64
$define %func mix_s64_to_ieee64 as function with args s64

*/

/* !SPACE!

$space %export mix_apply_gain_c, mix_accumulate_c, mix_account_saturate_c
$space %export mix_merge_c, mix_zero_c
$space %export mix_mixdown, mix_apply_crossfade
$space %internal mix_read_sample, mix_write_sample, mix_sample_width
$space %internal mix_scale_q15
$space %internal mix_ieee32_to_s64, mix_s64_to_ieee32
$space %internal mix_ieee64_to_s64, mix_s64_to_ieee64

*/

#include <kernel/audio/mix/mix.h>

static u32
mix_sample_width(u32 container)
{
	switch (container) {
	case MIX_CONTAINER_S8:
	case MIX_CONTAINER_U8:
		return (1);
	case MIX_CONTAINER_S16LE:
	case MIX_CONTAINER_S16BE:
		return (2);
	case MIX_CONTAINER_S24LE:
		return (3);
	case MIX_CONTAINER_S32LE:
	case MIX_CONTAINER_S32BE:
	case MIX_CONTAINER_FLOAT32:
		return (4);
	case MIX_CONTAINER_FLOAT64:
		return (8);
	default:
		return (0);
	}
}

u32
mix_sample_bytes(u32 container)
{
	return (mix_sample_width(container));
}

static s64
mix_scale_q15(s64 v, u64 g)
{
	s64	r;

	r = (v * (s64)g) >> 15;
	if (r > MIX_SAMPLE_MAX_S16) {
		return (MIX_SAMPLE_MAX_S16);
	}
	if (r < MIX_SAMPLE_MIN_S16) {
		return (MIX_SAMPLE_MIN_S16);
	}
	return (r);
}

static s64
mix_ieee32_to_s64(u32 b)
{
	s32	sign;
	s32	e;
	s32	shift;
	u32	mant;
	s64	v;

	sign = (s32)((b >> 31) & 1);
	e = (s32)((b >> 23) & 0xFF);
	mant = b & 0x7FFFFF;

	v = 0;
	if (e == 0) {
		v = 0;
	} else if (e == 0xFF) {
		v = (mant == 0) ? (sign ? MIX_SAMPLE_MIN_S16 :
		    MIX_SAMPLE_MAX_S16) : 0;
		return (v);
	} else {
		u32	m;

		m = (1u << 23) | mant;
		shift = e - 127 - 23 + 15;
		if (shift >= 15) {
			v = (sign ? MIX_SAMPLE_MIN_S16 : MIX_SAMPLE_MAX_S16);
			return (v);
		}
		if (shift <= -32) {
			v = 0;
		} else if (shift >= 0) {
			v = (s64)(m << shift);
		} else {
			v = (s64)(m >> (-shift));
		}
	}
	return (sign ? -v : v);
}

static u32
mix_s64_to_ieee32(s64 v)
{
	u32	sign;
	u32	mag;
	s32	e;
	u32	mant;

	if (v == 0) {
		return (0);
	}
	sign = (v < 0) ? 0x80000000U : 0;
	mag = (u32)(v < 0 ? -v : v);
	e = 127;
	while (mag >= (1u << 15)) {
		mag >>= 1;
		e++;
	}
	while ((mag & (1u << 14)) == 0 && e > 0) {
		mag <<= 1;
		e--;
	}
	mant = (mag & 0x7FFF) << 8;
	return (sign | ((u32)e << 23) | mant);
}

static s64
mix_ieee64_to_s64(u64 d)
{
	s32	sign;
	s32	e;
	s64	v;
	u64	mant;
	s32	shift;

	sign = (s32)((d >> 63) & 1);
	e = (s32)((d >> 52) & 0x7FF);
	mant = d & 0xFFFFFFFFFFFFFULL;

	if (e == 0) {
		v = 0;
	} else if (e == 0x7FF) {
		v = (mant == 0) ? (sign ? MIX_SAMPLE_MIN_S16 :
		    MIX_SAMPLE_MAX_S16) : 0;
		return (v);
	} else {
		u64	m;

		m = (1ULL << 52) | mant;
		shift = e - 1023 - 52 + 15;
		if (shift >= 15) {
			return (sign ? MIX_SAMPLE_MIN_S16 :
			    MIX_SAMPLE_MAX_S16);
		}
		if (shift <= -64) {
			v = 0;
		} else if (shift >= 0) {
			v = (s64)(m << shift);
		} else {
			v = (s64)(m >> (-shift));
		}
	}
	return (sign ? -v : v);
}

static u64
mix_s64_to_ieee64(s64 v)
{
	u64	sign;
	u64	mag;
	s32	e;

	if (v == 0) {
		return (0);
	}
	sign = (v < 0) ? 0x8000000000000000ULL : 0;
	mag = (u64)(v < 0 ? -v : v);
	e = 1023;
	while (mag >= (1ULL << 15)) {
		mag >>= 1;
		e++;
	}
	while ((mag & (1ULL << 14)) == 0 && e > 0) {
		mag <<= 1;
		e--;
	}
	return (sign | ((u64)e << 52) | ((mag & 0x7FFFULL) << 37));
}

static s64
mix_read_sample(u32 container, const u8 *p)
{
	switch (container) {
	case MIX_CONTAINER_S8:
		return ((s64)((s8)p[0]) << 8);
	case MIX_CONTAINER_U8:
		return ((s64)((s32)p[0] - 128) << 8);
	case MIX_CONTAINER_S16LE:
		return ((s64)(s16)((u16)p[0] | ((u16)p[1] << 8)));
	case MIX_CONTAINER_S16BE:
		return ((s64)(s16)(((u16)p[0] << 8) | (u16)p[1]));
	case MIX_CONTAINER_S24LE: {
		s32	v;

		v = (s32)(((u32)p[0]) | ((u32)p[1] << 8) |
		    ((u32)p[2] << 16));
		return ((s64)((v << 8) >> 16));	/* sign-extend, then Q15 */
	}
	case MIX_CONTAINER_S32LE:
		return ((s64)(s32)(((u32)p[0]) | ((u32)p[1] << 8) |
		    ((u32)p[2] << 16) | ((u32)p[3] << 24))) >> 16;
	case MIX_CONTAINER_S32BE:
		return ((s64)(s32)(((u32)p[0] << 24) | ((u32)p[1] << 16) |
		    ((u32)p[2] << 8) | (u32)p[3])) >> 16;
	case MIX_CONTAINER_FLOAT32:
		return (mix_ieee32_to_s64(((u32)p[0]) | ((u32)p[1] << 8) |
		    ((u32)p[2] << 16) | ((u32)p[3] << 24)));
	case MIX_CONTAINER_FLOAT64:
		return (mix_ieee64_to_s64((u64)p[0] | ((u64)p[1] << 8) |
		    ((u64)p[2] << 16) | ((u64)p[3] << 24) |
		    ((u64)p[4] << 32) | ((u64)p[5] << 40) |
		    ((u64)p[6] << 48) | ((u64)p[7] << 56)));
	default:
		return (0);
	}
}

static void
mix_write_sample(u32 container, u8 *p, s64 v)
{
	u32	f;

	switch (container) {
	case MIX_CONTAINER_S8:
		p[0] = (u8)(v >> 8);
		break;
	case MIX_CONTAINER_U8:
		p[0] = (u8)((v >> 8) + 128);
		break;
	case MIX_CONTAINER_S16LE:
		p[0] = (u8)((u16)v);
		p[1] = (u8)((u16)v >> 8);
		break;
	case MIX_CONTAINER_S16BE:
		p[0] = (u8)((u16)v >> 8);
		p[1] = (u8)((u16)v);
		break;
	case MIX_CONTAINER_S24LE: {
		s32	s;

		s = (s32)v << 8;
		p[0] = (u8)s;
		p[1] = (u8)((u32)s >> 8);
		p[2] = (u8)((u32)s >> 16);
		break;
	}
	case MIX_CONTAINER_S32LE: {
		s32	s;

		s = (s32)v << 16;
		p[0] = (u8)s;
		p[1] = (u8)((u32)s >> 8);
		p[2] = (u8)((u32)s >> 16);
		p[3] = (u8)((u32)s >> 24);
		break;
	}
	case MIX_CONTAINER_S32BE: {
		s32	s;

		s = (s32)v << 16;
		p[0] = (u8)((u32)s >> 24);
		p[1] = (u8)((u32)s >> 16);
		p[2] = (u8)((u32)s >> 8);
		p[3] = (u8)s;
		break;
	}
	case MIX_CONTAINER_FLOAT32:
		f = mix_s64_to_ieee32(v);
		p[0] = (u8)f;
		p[1] = (u8)(f >> 8);
		p[2] = (u8)(f >> 16);
		p[3] = (u8)(f >> 24);
		break;
	case MIX_CONTAINER_FLOAT64: {
		u64	d;

		d = mix_s64_to_ieee64(v);
		p[0] = (u8)d;
		p[1] = (u8)(d >> 8);
		p[2] = (u8)(d >> 16);
		p[3] = (u8)(d >> 24);
		p[4] = (u8)(d >> 32);
		p[5] = (u8)(d >> 40);
		p[6] = (u8)(d >> 48);
		p[7] = (u8)(d >> 56);
		break;
	}
	default:
		break;
	}
}

void
mix_apply_gain_c(u32 container, void *dst, const void *src, u64 count,
    mix_gain_t gain)
{
	u8		*out;
	const u8	*in;
	u64		i;
	u32		w;

	if (dst == NULL || src == NULL || count == 0) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	if (gain == MIX_GAIN_UNITY) {
		memcpy(dst, src, count * w);
		return;
	}
	out = (u8 *)dst;
	in = (const u8 *)src;
	for (i = 0; i < count; i++) {
		mix_write_sample(container, out,
		    mix_scale_q15(mix_read_sample(container, in), gain));
		out += w;
		in += w;
	}
}

void
mix_accumulate_c(u32 container, mix_account_t *acc, const void *src,
    u64 count, mix_gain_t gain)
{
	const u8	*in;
	u64		i;
	u32		w;

	if (acc == NULL || src == NULL || count == 0) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	in = (const u8 *)src;
	if (gain == MIX_GAIN_UNITY) {
		for (i = 0; i < count; i++) {
			acc[i] += mix_read_sample(container, in);
			in += w;
		}
	} else {
		for (i = 0; i < count; i++) {
			acc[i] += mix_scale_q15(mix_read_sample(container, in),
			    gain);
			in += w;
		}
	}
}

void
mix_account_saturate_c(u32 container, void *dst, const mix_account_t *acc,
    u64 count)
{
	u8	*out;
	u64	i;
	u32	w;
	s64	v;

	if (dst == NULL || acc == NULL || count == 0) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	out = (u8 *)dst;
	for (i = 0; i < count; i++) {
		v = acc[i];
		if (v > MIX_SAMPLE_MAX_S16) {
			v = MIX_SAMPLE_MAX_S16;
		} else if (v < MIX_SAMPLE_MIN_S16) {
			v = MIX_SAMPLE_MIN_S16;
		}
		mix_write_sample(container, out, v);
		out += w;
	}
}

void
mix_merge_c(u32 container, void *dst, const void *src, u64 count,
    mix_gain_t gain)
{
	u8		*out;
	const u8	*in;
	u64		i;
	u32		w;
	s64		v;

	if (dst == NULL || src == NULL || count == 0) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	out = (u8 *)dst;
	in = (const u8 *)src;
	for (i = 0; i < count; i++) {
		v = mix_read_sample(container, out) +
		    mix_scale_q15(mix_read_sample(container, in), gain);
		if (v > MIX_SAMPLE_MAX_S16) {
			v = MIX_SAMPLE_MAX_S16;
		} else if (v < MIX_SAMPLE_MIN_S16) {
			v = MIX_SAMPLE_MIN_S16;
		}
		mix_write_sample(container, out, v);
		out += w;
		in += w;
	}
}

void
mix_zero_c(u32 container, void *dst, u64 count)
{
	u32	w;

	if (dst == NULL || count == 0) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	if (container == MIX_CONTAINER_U8) {
		memset(dst, 0x80, count * w);
	} else {
		memset(dst, 0, count * w);
	}
}

u64
mix_mixdown(u32 container, mix_gain_t master, u32 nsrc,
    const mix_source_t *sources, void *dst, u64 count)
{
	mix_account_t	stack_acc[256];
	mix_account_t	*acc;
	u32		i;

	if (dst == NULL || count == 0 ||
	    (nsrc != 0 && sources == NULL)) {
		return (0);
	}
	if (nsrc == 0 || master == MIX_GAIN_ZERO) {
		mix_zero_c(container, dst, count);
		return (count);
	}
	if (nsrc == 1 && master == MIX_GAIN_UNITY) {
		mix_apply_gain_c(container, dst, sources[0].data, count,
		    sources[0].gain);
		return (count);
	}

	if (count <= (sizeof(stack_acc) / sizeof(stack_acc[0]))) {
		acc = stack_acc;
	} else {
		acc = (mix_account_t *)kmem_calloc(count, sizeof(*acc));
	}
	if (acc == NULL) {
		mix_zero_c(container, dst, count);
		return (0);
	}
	memset(acc, 0, count * sizeof(*acc));
	for (i = 0; i < nsrc; i++) {
		mix_accumulate_c(container, acc, sources[i].data, count,
		    mix_gain_scale(master, sources[i].gain));
	}
	mix_account_saturate_c(container, dst, acc, count);
	if (acc != stack_acc) {
		kmem_free(acc);
	}
	return (count);
}

void
mix_apply_crossfade(u32 container, const void *a, const void *b, void *dst,
    u64 count, mix_gain_t ga, mix_gain_t gb)
{
	u8		*out;
	const u8	*pa;
	const u8	*pb;
	u64		i;
	u32		w;
	s64		v;

	if (dst == NULL || count == 0 || (a == NULL && b == NULL)) {
		return;
	}
	w = mix_sample_width(container);
	if (w == 0) {
		return;
	}
	out = (u8 *)dst;
	pa = (const u8 *)a;
	pb = (const u8 *)b;
	for (i = 0; i < count; i++) {
		v = 0;
		if (pa != NULL) {
			v += mix_scale_q15(mix_read_sample(container, pa), ga);
		}
		if (pb != NULL) {
			v += mix_scale_q15(mix_read_sample(container, pb), gb);
		}
		if (v > MIX_SAMPLE_MAX_S16) {
			v = MIX_SAMPLE_MAX_S16;
		} else if (v < MIX_SAMPLE_MIN_S16) {
			v = MIX_SAMPLE_MIN_S16;
		}
		mix_write_sample(container, out, v);
		if (pa != NULL) {
			pa += w;
		}
		if (pb != NULL) {
			pb += w;
		}
		out += w;
	}
}
