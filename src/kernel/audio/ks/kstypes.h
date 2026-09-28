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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type s32 as 32 bit signed
$define %type ks_dataflow_t as direction of a streaming pin (in/out)
$define %type ks_state_t as run state of a streaming pin (stop/acquire/pause/run)
$define %type ks_format_t as canonical sample format descriptor
$define %type ks_position_t as play/capture cursor position pair
$define %type ks_packet_t as one data buffer exchanged with the port driver

$const KS_DATARANGE_* as sample container encodings a pin may offer
$const KS_PACKET_F_* as packet lifecycle flags
$const KS_MAX_CHANNELS as largest channel count the core supports
$const KS_MIN_RATE as lowest supported sample rate
$const KS_MAX_RATE as highest supported sample rate

*/

/* !SPACE!

$space %export ks_format_equal, ks_format_validate
$space %export ks_format_bytes_per_frame, ks_state_valid
$space %export ks_dataflow_is_render

*/

#ifndef KERNEL_AUDIO_KS_KSTYPES_H
#define KERNEL_AUDIO_KS_KSTYPES_H

#include <mlibc/mlibc.h>

typedef enum ks_dataflow {
	KS_DATAFLOW_IN,
	KS_DATAFLOW_OUT
} ks_dataflow_t;

typedef enum ks_state {
	KS_STATE_STOP,
	KS_STATE_ACQUIRE,
	KS_STATE_PAUSE,
	KS_STATE_RUN,
	KS_STATE_COUNT
} ks_state_t;

#define	KS_DATARANGE_PCM_S8		0x0001
#define	KS_DATARANGE_PCM_U8		0x0002
#define	KS_DATARANGE_PCM_S16LE	0x0003
#define	KS_DATARANGE_PCM_S16BE	0x0004
#define	KS_DATARANGE_PCM_S24LE	0x0005
#define	KS_DATARANGE_PCM_S32LE	0x0006
#define	KS_DATARANGE_PCM_S32BE	0x0007
#define	KS_DATARANGE_FLOAT32	0x0010
#define	KS_DATARANGE_FLOAT64	0x0011
#define	KS_DATARANGE_MAX		0x0100

typedef struct ks_format {
	u32	container;
	u32	valid_bits;
	u32	channels;
	u32	rate;
	u32	channel_mask;
} ks_format_t;
typedef struct ks_position {
	u64	play_offset;
	u64	write_offset;
} ks_position_t;

typedef struct ks_packet {
	u64	offset;
	u64	length;
	u32	flags;
	u32	pad;
} ks_packet_t;

#define	KS_PACKET_F_IN_FLIGHT	0x0001
#define	KS_PACKET_F_COMPLETE	0x0002
#define	KS_PACKET_F_LOOP	0x0004

static inline u32
ks_format_bytes_per_frame(const ks_format_t *fmt)
{
	u32	bytes;

	if (fmt == NULL || fmt->channels == 0) {
		return (0);
	}
	switch (fmt->container) {
	case KS_DATARANGE_PCM_S8:
	case KS_DATARANGE_PCM_U8:
		bytes = 1;
		break;
	case KS_DATARANGE_PCM_S16LE:
	case KS_DATARANGE_PCM_S16BE:
		bytes = 2;
		break;
	case KS_DATARANGE_PCM_S24LE:
		bytes = 3;
		break;
	case KS_DATARANGE_PCM_S32LE:
	case KS_DATARANGE_PCM_S32BE:
	case KS_DATARANGE_FLOAT32:
		bytes = 4;
		break;
	case KS_DATARANGE_FLOAT64:
		bytes = 8;
		break;
	default:
		return (0);
	}
	return (bytes * fmt->channels);
}
int
ks_format_equal(const ks_format_t *a, const ks_format_t *b);

#define	KS_MAX_CHANNELS		8
#define	KS_MIN_RATE		8000u
#define	KS_MAX_RATE		384000u

int
ks_format_validate(const ks_format_t *fmt);

static inline int
ks_state_valid(ks_state_t state)
{
	return ((u32)state < (u32)KS_STATE_COUNT);
}

static inline int
ks_dataflow_is_render(ks_dataflow_t flow)
{
	return (flow == KS_DATAFLOW_OUT);
}

#endif
