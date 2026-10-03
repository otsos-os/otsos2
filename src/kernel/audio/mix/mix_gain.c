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
$define %type mix_decibel_t as fixed-point decibel value (Q8)
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type s64 as 64 bit signed

$const MIX_GAIN_* as linear gain anchors
$const MIX_DB_* as decibel anchors and range limits

$define %func mix_gain_from_db as function with args mix_decibel_t
$define %func mix_gain_db as function with args mix_gain_t
$define %func mix_gain_scale as function with args mix_gain_t, mix_gain_t
$define %func mix_db_table_gain as function with args s32
$define %func mix_linear_to_db8 as function with args u32

*/

/* !SPACE!

$space %export mix_gain_from_db, mix_gain_db, mix_gain_scale
$space %internal mix_db_table_gain, mix_linear_to_db8

*/

#include <kernel/audio/mix/mix.h>

static const u32	mix_db_gain_table[18] = {
	0x00028000,	/* +32 dB */
	0x00008000,	/*   0 dB */
	0x00005A86,	/*  -6 dB */
	0x00004000,	/* -12 dB */
	0x00002D43,	/* -18 dB */
	0x00002000,	/* -24 dB */
	0x000016A2,	/* -30 dB */
	0x00001000,	/* -36 dB */
	0x00000B51,	/* -42 dB */
	0x00000800,	/* -48 dB */
	0x000005A9,	/* -54 dB */
	0x00000400,	/* -60 dB */
	0x000002D5,	/* -66 dB */
	0x00000200,	/* -72 dB */
	0x0000016B,	/* -78 dB */
	0x00000100,	/* -84 dB */
	0x000000B6,	/* -90 dB */
	0x00000080,	/* -96 dB */
};

static u32
mix_db_table_gain(s32 db)
{
	s32	t;
	s32	lo;
	s32	hi;
	u32	glo;
	u32	ghi;
	s32	rem;

	if (db <= MIX_DB_MIN) {
		return (MIX_GAIN_ZERO);
	}
	if (db >= MIX_DB_MAX) {
		return (MIX_GAIN_MAX);
	}
	if (db >= 0) {
		return ((u32)((u64)MIX_GAIN_UNITY +
		    (((u64)(MIX_GAIN_MAX - MIX_GAIN_UNITY) * (u64)db) /
		     (u64)MIX_DB_MAX)));
	}
	t = (-db) / 6;
	if (t >= 17) {
		return (MIX_GAIN_ZERO);
	}
	lo = t + 1;
	hi = lo + 1;
	if (hi > 17) {
		hi = 17;
	}
	glo = mix_db_gain_table[lo];
	ghi = mix_db_gain_table[hi];
	rem = (-db) - t * 6;
	return (glo - (u32)(((u64)(glo - ghi) * (u64)rem) / 6ULL));
}

static s32
mix_linear_to_db8(u32 gain)
{
	s32	lo;
	s32	hi;

	if (gain == 0) {
		return (MIX_DB_MUTE);
	}
	if (gain >= MIX_GAIN_MAX) {
		return (MIX_DB_MAX);
	}
	for (lo = 1; lo < 17; lo++) {
		if (gain >= mix_db_gain_table[lo]) {
			break;
		}
	}
	hi = lo - 1;
	if (hi < 0) {
		hi = 0;
	}
	{
		u32	glo;
		u32	ghi;
		s32	base;
		u32	num;

		glo = mix_db_gain_table[lo];
		ghi = mix_db_gain_table[hi];
		base = -(lo - 1) * 6;
		num = (glo - gain);
		base -= (s32)(((u64)num * 6ULL) /
		    (u64)((glo > ghi) ? (glo - ghi) : 1));
		return (base);
	}
}

mix_gain_t
mix_gain_from_db(mix_decibel_t db)
{
	if (db == MIX_DB_MUTE) {
		return (MIX_GAIN_ZERO);
	}
	return (mix_db_table_gain(db));
}

mix_decibel_t
mix_gain_db(mix_gain_t gain)
{
	return (mix_linear_to_db8(gain));
}

mix_gain_t
mix_gain_scale(mix_gain_t a, mix_gain_t b)
{
	u32	r;

	r = (u32)(((u64)a * (u64)b) >> 15);
	if (r > MIX_GAIN_MAX) {
		r = MIX_GAIN_MAX;
	}
	return (r);
}
