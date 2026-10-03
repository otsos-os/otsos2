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
$define %type audio_info_t as endpoint descriptor
$define %type audio_format_t as canonical format
$define %type audio_position_t as cursor snapshot
$define %type uint32_t as 32 bit unsigned
$define %type size_t as object size

$define %func audioOpen as function with args const char *
$define %func audioClose as procedure with args audio_t
$define %func audioGetInfo as function with args audio_t, audio_info_t *
$define %func audioSetFormat as function with args audio_t, const audio_format_t *
$define %func audioGetPosition as function with args audio_t, audio_position_t *
$define %func audioSetState as function with args audio_t, uint32_t
$define %func audioStart as function with args audio_t
$define %func audioStop as function with args audio_t
$define %func audioPause as function with args audio_t

*/

/* !SPACE!

$space %export audioOpen, audioClose, audioGetInfo, audioSetFormat
$space %export audioGetPosition, audioSetState
$space %export audioStart, audioStop, audioPause

*/

#include <libaudio.h>
#include <native.h>
#include <string.h>
#include "private.h"

int
audioOpen(const char *name)
{
	char	path[AUDIO_NAME_MAX + 32];
	size_t	len;
	int	ep;
	int	stream;

	if (name == NULL || name[0] == '\0') {
		return (-1);
	}
	if (name[0] == '/') {
		ep = entityOpen(name, ENTITY_ACCESS_READ |
		    ENTITY_ACCESS_WRITE);
	} else {
		len = strlen(name);
		if (len >= sizeof(path) - 24) {
			return (-1);
		}
		memcpy(path, "/Entity/Interface/Audio/", 24);
		memcpy(path + 24, name, len + 1);
		ep = entityOpen(path, ENTITY_ACCESS_READ |
		    ENTITY_ACCESS_WRITE);
	}
	if (ep < 0) {
		return (ep);
	}

	stream = entityIoctl(ep, AUDIO_IOCTL_OPEN_STREAM, NULL);
	entityClose(ep);
	return (stream);
}

void
audioClose(audio_t h)
{
	entityClose(h);
}

int
audioGetInfo(audio_t h, audio_info_t *info)
{
	if (info == NULL) {
		return (-1);
	}
	return (entityIoctl(h, AUDIO_IOCTL_GET_INFO, info));
}

int
audioSetFormat(audio_t h, const audio_format_t *format)
{
	audio_format_t	fmt;

	if (format == NULL) {
		return (-1);
	}
	fmt = *format;
	return (entityIoctl(h, AUDIO_IOCTL_SET_FORMAT, &fmt));
}

int
audioGetPosition(audio_t h, audio_position_t *position)
{
	if (position == NULL) {
		return (-1);
	}
	return (entityIoctl(h, AUDIO_IOCTL_GET_POSITION, position));
}

int
audioSetState(audio_t h, uint32_t state)
{
	return (entityIoctl(h, AUDIO_IOCTL_SET_STATE, &state));
}

int
audioStart(audio_t h)
{
	return (audioSetState(h, AUDIO_STATE_RUN));
}

int
audioStop(audio_t h)
{
	return (audioSetState(h, AUDIO_STATE_STOP));
}

int
audioPause(audio_t h)
{
	return (audioSetState(h, AUDIO_STATE_PAUSE));
}
