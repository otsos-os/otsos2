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

$define %type mix_gain_t as fixed-point linear gain (Q15, 1.0 = MIX_GAIN_UNITY)
$define %type mix_decibel_t as fixed-point decibel value (Q8, -inf = MIX_DB_MUTE)
$define %type mix_account_t as per-sample signed saturation accumulator
$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s16 as 16 bit signed
$define %type s32 as 32 bit signed
$define %type s64 as 64 bit signed

$const MIX_GAIN_* as gain scale anchors in Q15 linear space
$const MIX_DB_* as decibel anchors and range limits in Q8
$const MIX_SAMPLE_* as per-sample saturation bounds and limits

$define %func mix_gain_from_db as function with args mix_decibel_t
$define %func mix_gain_db as function with args mix_gain_t
$define %func mix_apply_gain_c as function with args u32, void *, const void *, u64, mix_gain_t
$define %func mix_accumulate_c as function with args u32, mix_account_t *, const void *, u64, mix_gain_t
$define %func mix_account_saturate_c as function with args u32, void *, const mix_account_t *, u64
$define %func mix_merge_c as function with args u32, void *, const void *, u64, mix_gain_t
$define %func mix_zero_c as procedure with args u32, void *, u64
$define %func mix_mixdown as function with args u32, mix_gain_t, u32, const mix_source_t *, void *, u64
$define %func mix_apply_crossfade as function with args u32, const void *, const void *, void *, u64, mix_gain_t, mix_gain_t
$define %func mix_gain_scale as function with args mix_gain_t, mix_gain_t
$define %func mix_sample_bytes as function with args u32

*/

/* !SPACE!

$space %export mix_gain_from_db, mix_gain_db
$space %export mix_apply_gain_c, mix_accumulate_c, mix_account_saturate_c
$space %export mix_merge_c, mix_zero_c
$space %export mix_mixdown, mix_apply_crossfade, mix_gain_scale
$space %export mix_sample_bytes

*/

#ifndef KERNEL_AUDIO_MIX_MIX_H
#define KERNEL_AUDIO_MIX_MIX_H

#include <mlibc/mlibc.h>

#define	MIX_CONTAINER_S8	0x0001
#define	MIX_CONTAINER_U8	0x0002
#define	MIX_CONTAINER_S16LE	0x0003
#define	MIX_CONTAINER_S16BE	0x0004
#define	MIX_CONTAINER_S24LE	0x0005
#define	MIX_CONTAINER_S32LE	0x0006
#define	MIX_CONTAINER_S32BE	0x0007
#define	MIX_CONTAINER_FLOAT32	0x0010
#define	MIX_CONTAINER_FLOAT64	0x0011

typedef u32	mix_gain_t;
#define	MIX_GAIN_ZERO		0x00000000U
#define	MIX_GAIN_UNITY		0x00008000U
#define	MIX_GAIN_MAX		0x00028000U

typedef s32	mix_decibel_t;
#define	MIX_DB_UNITY		0
#define	MIX_DB_MUTE		((s32)0x80000000)
#define	MIX_DB_MIN		(-9600)
#define	MIX_DB_MAX		3200

typedef s64	mix_account_t;

#define	MIX_SAMPLE_MIN_S16	(-32768)
#define	MIX_SAMPLE_MAX_S16	32767
#define	MIX_SAMPLE_MIN_S32	((s32)0x80000000)
#define	MIX_SAMPLE_MAX_S32	((s32)0x7FFFFFFF)
#define	MIX_MAX_MIXED_STREAMS	16

typedef struct mix_source {
	const void	*data;
	mix_gain_t	gain;
} mix_source_t;

mix_gain_t	mix_gain_from_db(mix_decibel_t db);
mix_decibel_t	mix_gain_db(mix_gain_t gain);

void		mix_apply_gain_c(u32 container, void *dst, const void *src,
		    u64 count, mix_gain_t gain);
void		mix_accumulate_c(u32 container, mix_account_t *acc,
		    const void *src, u64 count, mix_gain_t gain);
void		mix_account_saturate_c(u32 container, void *dst,
		    const mix_account_t *acc, u64 count);
void		mix_merge_c(u32 container, void *dst, const void *src,
		    u64 count, mix_gain_t gain);
void		mix_zero_c(u32 container, void *dst, u64 count);

u64		mix_mixdown(u32 container, mix_gain_t master, u32 nsrc,
		    const mix_source_t *sources, void *dst, u64 count);
void		mix_apply_crossfade(u32 container, const void *a, const void *b,
		    void *dst, u64 count, mix_gain_t ga, mix_gain_t gb);

mix_gain_t	mix_gain_scale(mix_gain_t a, mix_gain_t b);

u32		mix_sample_bytes(u32 container);

#endif
