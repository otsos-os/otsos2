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

#include <sprot/srapi_bridge.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

static int
wait_surface_ready(sprot_connection_t *conn)
{
	sprot_event_t event;
	int i, ret;

	for (i = 0; i < 80; i++) {
		ret = sprot_poll_event(conn, &event, 50);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_SURFACE_CREATED) {
			return (0);
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			break;
		}
	}
	errno = ETIMEDOUT;
	return (-1);
}

static int
copy_rows(uint8_t *dst, uint32_t dst_pitch, const uint8_t *src,
    uint32_t src_pitch, uint32_t width, uint32_t height)
{
	uint32_t row_size, y;

	if (dst == NULL || src == NULL || width == 0 || height == 0 ||
	    width > UINT32_MAX / sizeof(uint32_t)) {
		return (-1);
	}
	row_size = width * sizeof(uint32_t);
	if (dst_pitch < row_size || src_pitch < row_size) {
		return (-1);
	}
	for (y = 0; y < height; y++) {
		memcpy(dst + (size_t)y * dst_pitch,
		    src + (size_t)y * src_pitch, row_size);
	}
	return (0);
}

sprot_surface_t *
sprot_create_surface_for_image(sprot_connection_t *conn,
    srapi_image_t *image, const char *title)
{
	sprot_surface_t *surface;

	if (conn == NULL || image == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	surface = sprot_create_surface(conn, srapiImageWidth(image),
	    srapiImageHeight(image));
	if (surface == NULL) {
		return (NULL);
	}
	if (wait_surface_ready(conn) != 0) {
		sprot_destroy_surface(surface);
		return (NULL);
	}
	if (title != NULL && title[0] != '\0' &&
	    sprot_set_title(surface, title) != 0) {
		sprot_destroy_surface(surface);
		return (NULL);
	}
	return (surface);
}

int
sprot_present_image(sprot_surface_t *surface, srapi_image_t *image)
{
	uint32_t width, height, pitch;
	void *pixels;

	if (surface == NULL || image == NULL) {
		errno = EINVAL;
		return (-1);
	}
	width = srapiImageWidth(image);
	height = srapiImageHeight(image);
	if (width != sprot_surface_width(surface) ||
	    height != sprot_surface_height(surface)) {
		errno = EINVAL;
		return (-1);
	}
	pitch = srapiImagePitch(image);
	pixels = srapiImagePixels(image);
	if (copy_rows((uint8_t *)sprot_surface_pixels(surface),
	    sprot_surface_stride(surface), pixels, pitch, width, height) != 0) {
		errno = EINVAL;
		return (-1);
	}
	if (sprot_damage(surface, 0, 0, width, height) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}
