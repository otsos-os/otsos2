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

$define %type audio_t as userspace handle to one opened endpoint
$define %type audio_info_t as endpoint descriptor (mirrors api_audio_info_t)
$define %type audio_format_t as canonical format (mirrors api_audio_format_t)
$define %type audio_position_t as cursor snapshot (mirrors api_audio_position_t)
$define %type audio_endpoint_t as one enumerable endpoint
$define %type audio_pcm_t as decoded PCM header for one WAV stream

$const AUDIO_FLOW_* as data-flow directions
$const AUDIO_STATE_* as stream states
$const AUDIO_FMT_* as sample container encodings
$const AUDIO_NAME_MAX as longest endpoint name
$const AUDIO_VOLUME_* as Q15 linear volume anchors

$define %func audioEnumerate as function with args audio_endpoint_t *, uint32_t
$define %func audioOpen as function with args const char *
$define %func audioClose as procedure with args audio_t
$define %func audioGetInfo as function with args audio_t, audio_info_t *
$define %func audioSetFormat as function with args audio_t, const audio_format_t *
$define %func audioGetPosition as function with args audio_t, audio_position_t *
$define %func audioSetState as function with args audio_t, uint32_t
$define %func audioWrite as function with args audio_t, const void *, size_t
$define %func audioRead as function with args audio_t, void *, size_t
$define %func audioStart as function with args audio_t
$define %func audioStop as function with args audio_t
$define %func audioPause as function with args audio_t
$define %func audioSetVolume as function with args audio_t, uint32_t
$define %func audioGetVolume as function with args audio_t, uint32_t *
$define %func audioBytesFree as function with args audio_t
$define %func audioBytesAvailable as function with args audio_t
$define %func audioFormatToS16 as function with args void *, const void *, uint, uint
$define %func audioFormatFromS16 as function with args void *, const void *, uint, uint

*/

/* !SPACE!

$space %export audioEnumerate, audioOpen, audioClose
$space %export audioGetInfo, audioSetFormat, audioGetPosition
$space %export audioSetState, audioWrite, audioRead
$space %export audioStart, audioStop, audioPause
$space %export audioSetVolume, audioGetVolume
$space %export audioBytesFree, audioBytesAvailable
$space %export audioFormatToS16, audioFormatFromS16

*/

#ifndef LIBAUDIO_H
#define LIBAUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef int	audio_t;

#define	AUDIO_FLOW_IN		0
#define	AUDIO_FLOW_OUT		1
#define	AUDIO_STATE_STOP		0
#define	AUDIO_STATE_ACQUIRE	1
#define	AUDIO_STATE_PAUSE		2
#define	AUDIO_STATE_RUN		3
#define	AUDIO_FMT_S8		0x0001
#define	AUDIO_FMT_U8		0x0002
#define	AUDIO_FMT_S16LE	0x0003
#define	AUDIO_FMT_S16BE	0x0004
#define	AUDIO_FMT_S24LE	0x0005
#define	AUDIO_FMT_S32LE	0x0006
#define	AUDIO_FMT_S32BE	0x0007
#define	AUDIO_FMT_FLOAT32	0x0010
#define	AUDIO_FMT_FLOAT64	0x0011
#define	AUDIO_NAME_MAX		64
#define	AUDIO_VOLUME_ZERO	0x00000000U
#define	AUDIO_VOLUME_UNITY	0x00008000U
#define	AUDIO_VOLUME_MAX	0x00028000U

typedef struct audio_info {
	uint32_t	id;
	uint32_t	flow;
	uint32_t	state;
	uint32_t	formats_count;
	uint32_t	channels;
	uint32_t	rate;
	uint32_t	container;
	uint32_t	valid_bits;
	char		name[AUDIO_NAME_MAX];
} audio_info_t;

typedef struct audio_format {
	uint32_t	container;
	uint32_t	valid_bits;
	uint32_t	channels;
	uint32_t	rate;
	uint32_t	channel_mask;
} audio_format_t;

typedef struct audio_position {
	uint64_t	play_offset;
	uint64_t	write_offset;
} audio_position_t;

typedef struct audio_endpoint {
	uint32_t	id;
	uint32_t	flow;
	char		name[AUDIO_NAME_MAX];
} audio_endpoint_t;

typedef struct audio_pcm {
	uint32_t	container;
	uint32_t	channels;
	uint32_t	rate;
	uint32_t	valid_bits;
	uint32_t	data_offset;
	uint32_t	data_length;
} audio_pcm_t;

int	audioEnumerate(audio_endpoint_t *endpoints, uint32_t max_endpoints);
audio_t	audioOpen(const char *name);
void	audioClose(audio_t h);
int	audioGetInfo(audio_t h, audio_info_t *info);
int	audioSetFormat(audio_t h, const audio_format_t *format);
int	audioGetPosition(audio_t h, audio_position_t *position);
int	audioSetState(audio_t h, uint32_t state);
int	audioWrite(audio_t h, const void *buf, size_t count);
int	audioRead(audio_t h, void *buf, size_t count);
int	audioStart(audio_t h);
int	audioStop(audio_t h);
int	audioPause(audio_t h);
int	audioSetVolume(audio_t h, uint32_t volume);
int	audioGetVolume(audio_t h, uint32_t *volume);
uint32_t	audioPercentToVolume(uint32_t percent);
uint32_t	audioVolumeToPercent(uint32_t volume);
int	audioBytesFree(audio_t h);
int	audioBytesAvailable(audio_t h);
uint32_t	audioFormatToS16(void *dst, const void *src, uint32_t frames,
		    uint32_t channels, uint32_t container);
uint32_t	audioFormatFromS16(void *dst, const void *src, uint32_t frames,
		    uint32_t channels, uint32_t container);
int	audioWavParse(const uint8_t *data, uint32_t len, audio_pcm_t *pcm);

#endif
