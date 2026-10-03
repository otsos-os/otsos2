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
$define %type uint32_t as 32 bit unsigned

$const AUDIO_VOLUME_* as Q15 linear volume anchors
$const AUDIO_PERCENT_* as percent-to-volume scale limits

$define %func audioSetVolume as function with args audio_t, uint32_t
$define %func audioGetVolume as function with args audio_t, uint32_t *
$define %func audioPercentToVolume as function with args uint32_t
$define %func audioVolumeToPercent as function with args uint32_t

*/

/* !SPACE!

$space %export audioSetVolume, audioGetVolume
$space %export audioPercentToVolume, audioVolumeToPercent

*/

#include <libaudio.h>
#include <native.h>
#include "private.h"

uint32_t
audioPercentToVolume(uint32_t percent)
{
	uint64_t	sq;

	if (percent == 0) {
		return (AUDIO_VOLUME_ZERO);
	}
	if (percent >= 100) {
		return (AUDIO_VOLUME_UNITY);
	}
	sq = (uint64_t)percent * (uint64_t)percent;
	return ((uint32_t)((sq * (uint64_t)AUDIO_VOLUME_UNITY) / 10000ULL));
}

uint32_t
audioVolumeToPercent(uint32_t volume)
{
	uint32_t	p;

	if (volume == 0) {
		return (0);
	}
	if (volume >= AUDIO_VOLUME_UNITY) {
		return (100);
	}
	p = 100;
	{
		uint32_t	lo, hi, mid;
		uint64_t	target;

		target = (uint64_t)volume * 10000ULL / AUDIO_VOLUME_UNITY;
		lo = 1;
		hi = 99;
		while (lo <= hi) {
			mid = (lo + hi) / 2;
			if ((uint64_t)mid * (uint64_t)mid <= target) {
				p = mid;
				lo = mid + 1;
			} else {
				hi = mid - 1;
			}
		}
	}
	if (p < 1) {
		p = 1;
	}
	return (p);
}

int
audioSetVolume(audio_t h, uint32_t volume)
{
	uint32_t	v;

	if (h < 0) {
		return (-1);
	}
	v = volume;
	if (v > AUDIO_VOLUME_MAX) {
		v = AUDIO_VOLUME_MAX;
	}
	return (entityIoctl(h, AUDIO_IOCTL_SET_VOLUME, &v));
}

int
audioGetVolume(audio_t h, uint32_t *volume)
{
	if (h < 0 || volume == NULL) {
		return (-1);
	}
	return (entityIoctl(h, AUDIO_IOCTL_GET_VOLUME, volume));
}
