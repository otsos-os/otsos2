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
$define %type audio_format_t as canonical format (mirrors api_audio_format_t)
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %type ssize_t as signed object size

$const AUDIO_IOCTL_GET_INFO as endpoint info ioctl command
$const AUDIO_IOCTL_SET_FORMAT as endpoint format ioctl command
$const AUDIO_IOCTL_GET_POSITION as endpoint position ioctl command
$const AUDIO_IOCTL_SET_STATE as endpoint state ioctl command
$const AUDIO_STAGING_BYTES as internal staging buffer size

$define %func audioPacedWait as function with args uint32_t

*/

/* !SPACE!

$space %internal audioPacedWait

*/

#ifndef LIBAUDIO_PRIVATE_H
#define LIBAUDIO_PRIVATE_H

#include <libaudio.h>
#include <stdint.h>

#define	AUDIO_IOCTL_GET_INFO		0x4100
#define	AUDIO_IOCTL_SET_FORMAT		0x4102
#define	AUDIO_IOCTL_GET_STATE		0x4103
#define	AUDIO_IOCTL_SET_STATE		0x4104
#define	AUDIO_IOCTL_GET_POSITION		0x4105
#define	AUDIO_IOCTL_GET_VOLUME		0x4106
#define	AUDIO_IOCTL_SET_VOLUME		0x4107
#define	AUDIO_IOCTL_OPEN_STREAM		0x4108
#define	AUDIO_STAGING_BYTES		16384

int	audioPacedWait(uint32_t ms);

#endif
