/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

#include "buffer.h"

#include <native.h>

#include <stdlib.h>

struct swm_buffer {
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	size_t size;
	int handle;
	void *map;
};

swm_buffer_t *
swm_buffer_create(int handle, uint32_t width, uint32_t height,
    uint32_t stride, size_t size)
{
	swm_buffer_t *buffer;
	uint64_t minimum;

	minimum = (uint64_t)stride * height;
	if (handle < 0 || width == 0 || height == 0 ||
	    width > UINT32_MAX / sizeof(uint32_t) ||
	    stride < width * sizeof(uint32_t) || minimum > SIZE_MAX ||
	    size < (size_t)minimum) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		return (NULL);
	}
	buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		(void)shmClose(handle);
		return (NULL);
	}
	buffer->map = shmMap(handle, NULL, size, API_MAP_READ,
	    API_MAP_SHARED);
	if (buffer->map == NULL) {
		(void)shmClose(handle);
		free(buffer);
		return (NULL);
	}
	buffer->width = width;
	buffer->height = height;
	buffer->stride = stride;
	buffer->size = size;
	buffer->handle = handle;
	return (buffer);
}

void
swm_buffer_destroy(swm_buffer_t *buffer)
{
	if (buffer == NULL) {
		return;
	}
	if (buffer->map != NULL && buffer->size != 0) {
		(void)memUnmap(buffer->map, buffer->size);
	}
	if (buffer->handle >= 0) {
		(void)shmClose(buffer->handle);
	}
	free(buffer);
}

uint32_t
swm_buffer_width(const swm_buffer_t *buffer)
{
	return (buffer != NULL ? buffer->width : 0);
}

uint32_t
swm_buffer_height(const swm_buffer_t *buffer)
{
	return (buffer != NULL ? buffer->height : 0);
}

uint32_t
swm_buffer_stride(const swm_buffer_t *buffer)
{
	return (buffer != NULL ? buffer->stride : 0);
}

size_t
swm_buffer_size(const swm_buffer_t *buffer)
{
	return (buffer != NULL ? buffer->size : 0);
}

const void *
swm_buffer_pixels(const swm_buffer_t *buffer)
{
	return (buffer != NULL ? buffer->map : NULL);
}
