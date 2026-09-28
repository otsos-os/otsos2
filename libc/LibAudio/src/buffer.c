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
$define %type audio_position_t as cursor snapshot
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %type ssize_t as signed object size

$define %func audioWrite as function with args audio_t, const void *, size_t
$define %func audioRead as function with args audio_t, void *, size_t
$define %func audioBytesFree as function with args audio_t
$define %func audioBytesAvailable as function with args audio_t
$define %func audioPacedWait as function with args uint32_t

*/

/* !SPACE!

$space %export audioWrite, audioRead
$space %export audioBytesFree, audioBytesAvailable
$space %internal audioPacedWait

*/

#include <libaudio.h>
#include <native.h>
#include <string.h>
#include "private.h"

int
audioPacedWait(uint32_t ms)
{
	struct kevent	ch;
	struct kevent	ev;
	int		kq;
	int		ret;

	kq = eventKqueue();
	if (kq < 0) {
		return (-1);
	}
	memset(&ch, 0, sizeof(ch));
	ch.ident = 1;
	ch.filter = EVFILT_TIMER;
	ch.flags = EV_ADD | EV_ENABLE;
	if (ms == 0) {
		ms = 1;
	}
	ch.data = (int64_t)ms * 1000000LL;
	ret = eventWait(kq, &ch, 1, &ev, 1, (int64_t)ms + 1);
	eventClose(kq);
	return (ret < 0 ? -1 : 0);
}

int
audioWrite(audio_t h, const void *buf, size_t count)
{
	const uint8_t	*src;
	size_t		done;
	ssize_t		n;

	if (h < 0 || (buf == NULL && count != 0)) {
		return (-1);
	}
	src = (const uint8_t *)buf;
	done = 0;
	while (done < count) {
		n = entityWrite(h, src + done, count - done);
		if (n > 0) {
			done += (size_t)n;
			continue;
		}
		if (n == 0) {
			audioPacedWait(0);
			continue;
		}
		return (-1);
	}
	return ((int)done);
}

int
audioRead(audio_t h, void *buf, size_t count)
{
	uint8_t	*dst;
	size_t	done;
	ssize_t	n;

	if (h < 0 || (buf == NULL && count != 0)) {
		return (-1);
	}
	dst = (uint8_t *)buf;
	done = 0;
	while (done < count) {
		n = entityRead(h, dst + done, count - done);
		if (n > 0) {
			done += (size_t)n;
			continue;
		}
		if (n == 0) {
			audioPacedWait(0);
			continue;
		}
		return (-1);
	}
	return ((int)done);
}


int
audioBytesFree(audio_t h)
{
	audio_position_t	pos;

	if (audioGetPosition(h, &pos) != 0) {
		return (0);
	}
	if (pos.write_offset >= pos.play_offset) {
		return ((int)(pos.write_offset - pos.play_offset));
	}
	return (0);
}

int
audioBytesAvailable(audio_t h)
{
	audio_position_t	pos;

	if (audioGetPosition(h, &pos) != 0) {
		return (0);
	}
	if (pos.play_offset >= pos.write_offset) {
		return ((int)(pos.play_offset - pos.write_offset));
	}
	return (0);
}
