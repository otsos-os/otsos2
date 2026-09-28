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

$define %type audio_endpoint_t as one enumerable endpoint
$define %type audio_info_t as endpoint descriptor
$define %type uint32_t as 32 bit unsigned
$define %type size_t as object size

$define %func audioEnumerate as function with args audio_endpoint_t *, uint32_t

*/

/* !SPACE!

$space %export audioEnumerate

*/

#include <libaudio.h>
#include <native.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"

int
audioEnumerate(audio_endpoint_t *endpoints, uint32_t max_endpoints)
{
	struct api_entity_entry	*list;
	uint32_t		i;
	int			count;
	int			handle;
	audio_info_t		info;

	if (endpoints == NULL || max_endpoints == 0) {
		return (0);
	}
	list = (struct api_entity_entry *)malloc(sizeof(*list) *
	    max_endpoints);
	if (list == NULL) {
		return (-1);
	}
	count = entityQuery(API_ENTITY_ARCH_AUDIO, 0, list, max_endpoints);
	if (count < 0) {
		free(list);
		return (-1);
	}
	for (i = 0; i < (uint32_t)count; i++) {
		const char	*full;
		const char	*base;
		size_t		len;

		full = list[i].name;
		base = full;
		if (memcmp(full, "/Entity/Interface/Audio/", 24) == 0) {
			base = full + 24;
		}
		len = strlen(base);
		if (len >= AUDIO_NAME_MAX) {
			len = AUDIO_NAME_MAX - 1;
		}
		memset(endpoints[i].name, 0, AUDIO_NAME_MAX);
		memcpy(endpoints[i].name, base, len);
		endpoints[i].id = i;
		endpoints[i].flow = AUDIO_FLOW_OUT;
		handle = audioOpen(full);
		if (handle >= 0) {
			if (audioGetInfo(handle, &info) == 0) {
				endpoints[i].flow = info.flow;
			}
			audioClose(handle);
		}
	}
	free(list);
	return (count);
}
