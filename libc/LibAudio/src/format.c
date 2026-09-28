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

$define %type audio_pcm_t as decoded PCM header for one WAV stream
$define %type uint8_t as 8 bit unsigned
$define %type uint16_t as 16 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type int16_t as 16 bit signed
$define %type int32_t as 32 bit signed

$define %func audioFormatToS16 as function with args void *, const void *, uint32_t, uint32_t, uint32_t
$define %func audioFormatFromS16 as function with args void *, const void *, uint32_t, uint32_t, uint32_t
$define %func audioWavParse as function with args const uint8_t *, uint32_t, audio_pcm_t *
$define %func load_u16le as function with args const uint8_t *
$define %func load_u32le as function with args const uint8_t *
$define %func container_bytes as function with args uint32_t
$define %func read_sample_s16 as function with args const uint8_t *, uint32_t
$define %func write_sample_s16 as procedure with args uint8_t *, int16_t
$define %func write_sample as procedure with args uint8_t *, int16_t, uint32_t

*/

/* !SPACE!

$space %export audioFormatToS16, audioFormatFromS16, audioWavParse
$space %internal load_u16le, load_u32le, container_bytes
$space %internal read_sample_s16, write_sample_s16, write_sample

*/

#include <libaudio.h>
#include <stdint.h>

static uint16_t
load_u16le(const uint8_t *src)
{
	return ((uint16_t)src[0] | ((uint16_t)src[1] << 8));
}

static uint32_t
load_u32le(const uint8_t *src)
{
	return ((uint32_t)src[0] | ((uint32_t)src[1] << 8) |
	    ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24));
}

static uint32_t
container_bytes(uint32_t container)
{
	switch (container) {
	case AUDIO_FMT_S8:
	case AUDIO_FMT_U8:
		return (1);
	case AUDIO_FMT_S16LE:
	case AUDIO_FMT_S16BE:
		return (2);
	case AUDIO_FMT_S24LE:
		return (3);
	case AUDIO_FMT_S32LE:
	case AUDIO_FMT_S32BE:
	case AUDIO_FMT_FLOAT32:
		return (4);
	case AUDIO_FMT_FLOAT64:
		return (8);
	default:
		return (0);
	}
}


static int16_t
read_sample_s16(const uint8_t *src, uint32_t container)
{
	uint32_t	u32;
	int32_t		i32;

	switch (container) {
	case AUDIO_FMT_S8:
		return ((int16_t)((int8_t)src[0] << 8));
	case AUDIO_FMT_U8:
		return ((int16_t)((int16_t)((int8_t)(src[0] ^ 0x80)) << 8));
	case AUDIO_FMT_S16LE:
		return ((int16_t)load_u16le(src));
	case AUDIO_FMT_S16BE:
		return ((int16_t)((load_u16le(src) << 8) |
		    (load_u16le(src) >> 8)));
	case AUDIO_FMT_S24LE:
		i32 = (int32_t)(((uint32_t)src[0]) |
		    ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16));
		i32 = (i32 << 8) >> 8;
		return ((int16_t)(i32 >> 8));
	case AUDIO_FMT_S32LE:
		u32 = load_u32le(src);
		return ((int16_t)(u32 >> 16));
	case AUDIO_FMT_S32BE:
		u32 = ((uint32_t)src[0] << 24) | ((uint32_t)src[1] << 16) |
		    ((uint32_t)src[2] << 8) | (uint32_t)src[3];
		return ((int16_t)(u32 >> 16));
	case AUDIO_FMT_FLOAT32:
		/* Reinterpret IEEE754 float as a crude scale; unsupported. */
		return (0);
	default:
		return (0);
	}
}

static void
write_sample_s16(uint8_t *dst, int16_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)((uint16_t)value >> 8);
}

static void
write_sample(uint8_t *dst, int16_t value, uint32_t container)
{
	int32_t	i32;

	switch (container) {
	case AUDIO_FMT_S8:
		dst[0] = (uint8_t)(value >> 8);
		break;
	case AUDIO_FMT_U8:
		dst[0] = (uint8_t)((value >> 8) ^ 0x80);
		break;
	case AUDIO_FMT_S16LE:
		write_sample_s16(dst, value);
		break;
	case AUDIO_FMT_S16BE:
		dst[0] = (uint8_t)((uint16_t)value >> 8);
		dst[1] = (uint8_t)value;
		break;
	case AUDIO_FMT_S24LE:
		i32 = ((int32_t)value) << 8;
		dst[0] = (uint8_t)i32;
		dst[1] = (uint8_t)((uint32_t)i32 >> 8);
		dst[2] = (uint8_t)((uint32_t)i32 >> 16);
		break;
	case AUDIO_FMT_S32LE:
		i32 = ((int32_t)value) << 16;
		dst[0] = (uint8_t)i32;
		dst[1] = (uint8_t)((uint32_t)i32 >> 8);
		dst[2] = (uint8_t)((uint32_t)i32 >> 16);
		dst[3] = (uint8_t)((uint32_t)i32 >> 24);
		break;
	case AUDIO_FMT_FLOAT32:
		dst[0] = 0;
		dst[1] = 0;
		dst[2] = 0;
		dst[3] = 0;
		break;
	default:
		dst[0] = 0;
		dst[1] = 0;
		break;
	}
}

uint32_t
audioFormatToS16(void *dst, const void *src, uint32_t frames,
    uint32_t channels, uint32_t container)
{
	const uint8_t	*in;
	uint8_t		*out;
	uint32_t	bytes;
	uint32_t	i;
	uint32_t	c;

	if (dst == NULL || src == NULL || frames == 0 || channels == 0) {
		return (0);
	}
	bytes = container_bytes(container);
	if (bytes == 0) {
		return (0);
	}
	in = (const uint8_t *)src;
	out = (uint8_t *)dst;
	for (i = 0; i < frames; i++) {
		for (c = 0; c < channels; c++) {
			write_sample_s16(out, read_sample_s16(in, container));
			in += bytes;
			out += 2;
		}
	}
	return (frames);
}

uint32_t
audioFormatFromS16(void *dst, const void *src, uint32_t frames,
    uint32_t channels, uint32_t container)
{
	const uint8_t	*in;
	uint8_t		*out;
	uint32_t	bytes;
	uint32_t	i;
	uint32_t	c;

	if (dst == NULL || src == NULL || frames == 0 || channels == 0) {
		return (0);
	}
	bytes = container_bytes(container);
	if (bytes == 0) {
		return (0);
	}
	in = (const uint8_t *)src;
	out = (uint8_t *)dst;
	for (i = 0; i < frames; i++) {
		for (c = 0; c < channels; c++) {
			write_sample(out, (int16_t)load_u16le(in), container);
			in += 2;
			out += bytes;
		}
	}
	return (frames);
}


int
audioWavParse(const uint8_t *data, uint32_t len, audio_pcm_t *pcm)
{
	const uint8_t	*p;
	const uint8_t	*end;
	uint32_t	chunk_id;
	uint32_t	chunk_size;
	uint32_t	audio_format;
	uint32_t	channels;
	uint32_t	rate;
	uint32_t	bits;

	if (data == NULL || pcm == NULL || len < 44) {
		return (-1);
	}
	if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' ||
	    data[3] != 'F' || data[8] != 'W' || data[9] != 'A' ||
	    data[10] != 'V' || data[11] != 'E') {
		return (-1);
	}
	p = data + 12;
	end = data + len;
	audio_format = 0;
	channels = 0;
	rate = 0;
	bits = 0;
	pcm->data_offset = 0;
	pcm->data_length = 0;
	while (p + 8 <= end) {
		chunk_id = load_u32le(p);
		chunk_size = load_u32le(p + 4);
		p += 8;
		if (p + chunk_size > end) {
			break;
		}
		if (chunk_id == 0x20746d66U) {
			audio_format = load_u16le(p);
			channels = load_u16le(p + 2);
			rate = load_u32le(p + 4);
			bits = load_u16le(p + 14);
		} else if (chunk_id == 0x61746164U) {
			pcm->data_offset = (uint32_t)(p - data);
			pcm->data_length = chunk_size;
		}
		p += chunk_size + (chunk_size & 1);
	}
	if (audio_format != 1 || channels == 0 || rate == 0 ||
	    bits == 0 || pcm->data_offset == 0) {
		return (-1);
	}
	pcm->channels = channels;
	pcm->rate = rate;
	pcm->valid_bits = bits;
	switch (bits) {
	case 8:
		pcm->container = AUDIO_FMT_U8;
		break;
	case 16:
		pcm->container = AUDIO_FMT_S16LE;
		break;
	case 24:
		pcm->container = AUDIO_FMT_S24LE;
		break;
	case 32:
		pcm->container = AUDIO_FMT_S32LE;
		break;
	default:
		return (-1);
	}
	return (0);
}
