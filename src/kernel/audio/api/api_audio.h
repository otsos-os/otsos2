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

$define %type api_audio_info_t as public endpoint descriptor
$define %type api_audio_format_t as public format descriptor
$define %type api_audio_position_t as public cursor snapshot

$const AUDIO_IOCTL_* as audio entity ioctl command codes
$const AUDIO_NAME_MAX as longest endpoint name in /Entity/Interface/Audio
$const AUDIO_MAX_ENDPOINTS as endpoints the api layer publishes

$define %func api_audio_init as procedure with args void
$define %func api_audio_publish as function with args const char *, uint, uint, void *
$define %func api_audio_endpoint_count as function with args void

*/

/* !SPACE!

$space %export api_audio_init, api_audio_publish
$space %export api_audio_unpublish_port, api_audio_endpoint_count

*/

#ifndef KERNEL_AUDIO_API_API_AUDIO_H
#define KERNEL_AUDIO_API_API_AUDIO_H

#include <mlibc/mlibc.h>

struct pc_port;

#define	AUDIO_NAME_MAX		64
#define	AUDIO_MAX_ENDPOINTS	64
#define	AUDIO_IOCTL_GET_INFO	0x4100
#define	AUDIO_IOCTL_SET_FORMAT	0x4102
#define	AUDIO_IOCTL_GET_STATE	0x4103
#define	AUDIO_IOCTL_SET_STATE	0x4104
#define	AUDIO_IOCTL_GET_POSITION 0x4105
#define	AUDIO_IOCTL_GET_VOLUME	0x4106
#define	AUDIO_IOCTL_SET_VOLUME	0x4107
#define	AUDIO_IOCTL_OPEN_STREAM	0x4108
#define	AUDIO_VOLUME_ZERO	0x00000000U
#define	AUDIO_VOLUME_UNITY	0x00008000U
#define	AUDIO_VOLUME_MAX	0x00028000U

typedef struct api_audio_info {
	u32		id;
	u32		flow;
	u32		state;
	u32		formats_count;
	u32		channels;
	u32		rate;
	u32		container;
	u32		valid_bits;
	char		name[AUDIO_NAME_MAX];
} api_audio_info_t;

typedef struct api_audio_format {
	u32		container;
	u32		valid_bits;
	u32		channels;
	u32		rate;
	u32		channel_mask;
} api_audio_format_t;

typedef struct api_audio_position {
	u64		play_offset;
	u64		write_offset;
} api_audio_position_t;

void		api_audio_init(void);
int		api_audio_publish(const char *name, u32 flow, u32 pin_id,
		    void *port);
void		api_audio_unpublish_port(void *port);
u32		api_audio_endpoint_count(void);

#endif
