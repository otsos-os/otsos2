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

$define %type ks_format_t as canonical sample format descriptor
$define %type int as 32 bit signed
$define %type u32 as 32 bit unsigned

$define %func ks_format_equal as function with args const ks_format_t *, const ks_format_t *
$define %func ks_format_validate as function with args const ks_format_t *

*/

/* !SPACE!

$space %export ks_format_equal, ks_format_validate

*/

#include <kernel/audio/ks/kstypes.h>

int
ks_format_equal(const ks_format_t *a, const ks_format_t *b)
{
	if (a == NULL || b == NULL) {
		return (0);
	}
	return (a->container == b->container &&
	    a->channels == b->channels &&
	    a->rate == b->rate &&
	    a->channel_mask == b->channel_mask);
}

static int
ks_container_valid(u32 container)
{
	switch (container) {
	case KS_DATARANGE_PCM_S8:
	case KS_DATARANGE_PCM_U8:
	case KS_DATARANGE_PCM_S16LE:
	case KS_DATARANGE_PCM_S16BE:
	case KS_DATARANGE_PCM_S24LE:
	case KS_DATARANGE_PCM_S32LE:
	case KS_DATARANGE_PCM_S32BE:
	case KS_DATARANGE_FLOAT32:
	case KS_DATARANGE_FLOAT64:
		return (1);
	default:
		return (0);
	}
}

static u32
ks_container_width(u32 container)
{
	switch (container) {
	case KS_DATARANGE_PCM_S8:
	case KS_DATARANGE_PCM_U8:
		return (8);
	case KS_DATARANGE_PCM_S16LE:
	case KS_DATARANGE_PCM_S16BE:
		return (16);
	case KS_DATARANGE_PCM_S24LE:
		return (24);
	case KS_DATARANGE_PCM_S32LE:
	case KS_DATARANGE_PCM_S32BE:
	case KS_DATARANGE_FLOAT32:
		return (32);
	case KS_DATARANGE_FLOAT64:
		return (64);
	default:
		return (0);
	}
}

int
ks_format_validate(const ks_format_t *fmt)
{
	u32	width;

	if (fmt == NULL) {
		return (0);
	}
	if (!ks_container_valid(fmt->container)) {
		return (0);
	}
	width = ks_container_width(fmt->container);
	if (width == 0 || fmt->valid_bits == 0 ||
	    fmt->valid_bits > width) {
		return (0);
	}
	if (fmt->channels == 0 || fmt->channels > KS_MAX_CHANNELS) {
		return (0);
	}
	if (fmt->rate < KS_MIN_RATE || fmt->rate > KS_MAX_RATE) {
		return (0);
	}
	if (fmt->channels > 1 && fmt->channel_mask == 0) {
		return (0);
	}
	return (1);
}
