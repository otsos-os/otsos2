/* !DEFINES!

$define %type srapi_surface as CPU pixel surface
$define %func srapi_surface_bytes_per_pixel as function with args format
$define %func srapiCreateSurface as function with args device, desc, out surface
$define %func srapiDestroySurface as procedure with args surface
$define %func srapiMapSurface as function with args surface, out, out pitch
$define %func srapiSurfaceWrite as function with args surface, region, pixels, pitch
$define %func srapiSurfaceSetPalette as function with args surface, first, colors, count

*/

/* !SPACE!

$space %internal surface_init_palette
$space %export srapi_surface_bytes_per_pixel, srapiCreateSurface
$space %export srapiDestroySurface, srapiMapSurface, srapiUnmapSurface
$space %export srapiSurfaceWrite, srapiSurfaceSetPalette

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <srapi.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "srapi_private.h"

uint32_t
srapi_surface_bytes_per_pixel(uint32_t format)
{
	if (format == SRAPI_SURFACE_FORMAT_ARGB8888) {
		return (4);
	}
	if (format == SRAPI_SURFACE_FORMAT_INDEX8) {
		return (1);
	}
	return (0);
}

static void
surface_init_palette(srapi_surface_t *surface)
{
	uint32_t	i;

	for (i = 0; i < 256; i++) {
		surface->palette[i] = 0xff000000U | (i << 16) | (i << 8) | i;
	}
}

int
srapiCreateSurface(srapi_device_t *device,
    const struct srapi_surface_desc *desc, srapi_surface_t **out)
{
	srapi_surface_t	*surface;
	size_t		size;
	uint32_t	bytes_pp, pitch;

	if (!device || !desc || !out || desc->width == 0 ||
	    desc->height == 0) {
		return (SRAPI_ERR_INVALID);
	}
	bytes_pp = srapi_surface_bytes_per_pixel(desc->format);
	if (bytes_pp == 0) {
		return (SRAPI_ERR_UNSUPPORTED);
	}
	pitch = desc->pitch;
	if (pitch == 0) {
		pitch = desc->width * bytes_pp;
	}
	if (pitch / bytes_pp < desc->width) {
		return (SRAPI_ERR_RANGE);
	}
	size = (size_t)pitch * (size_t)desc->height;
	if (size == 0) {
		return (SRAPI_ERR_RANGE);
	}
	surface = calloc(1, sizeof(*surface));
	if (!surface) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	surface->pixels = calloc(1, size);
	if (!surface->pixels) {
		free(surface);
		return (SRAPI_ERR_NO_MEMORY);
	}
	surface->device = device;
	surface->size = size;
	surface->width = desc->width;
	surface->height = desc->height;
	surface->pitch = pitch;
	surface->format = desc->format;
	surface->flags = desc->flags;
	surface_init_palette(surface);
	*out = surface;
	return (SRAPI_OK);
}

void
srapiDestroySurface(srapi_surface_t *surface)
{
	if (!surface) {
		return;
	}
	free(surface->pixels);
	free(surface);
}

int
srapiMapSurface(srapi_surface_t *surface, void **out, uint32_t *out_pitch)
{
	if (!surface || !out) {
		return (SRAPI_ERR_INVALID);
	}
	*out = surface->pixels;
	if (out_pitch) {
		*out_pitch = surface->pitch;
	}
	return (SRAPI_OK);
}

void
srapiUnmapSurface(srapi_surface_t *surface)
{
	(void)surface;
}

int
srapiSurfaceWrite(srapi_surface_t *surface,
    const struct srapi_region *region, const void *pixels, uint32_t src_pitch)
{
	const uint8_t	*src;
	uint8_t		*dst;
	uint32_t	bytes_pp, x, y, width, height, row_bytes, row;

	if (!surface || !pixels || !region || region->width == 0 ||
	    region->height == 0) {
		return (SRAPI_ERR_INVALID);
	}
	if (region->x >= surface->width || region->y >= surface->height) {
		return (SRAPI_OK);
	}
	x = region->x;
	y = region->y;
	width = region->width;
	height = region->height;
	if (width > surface->width - x) {
		width = surface->width - x;
	}
	if (height > surface->height - y) {
		height = surface->height - y;
	}
	bytes_pp = srapi_surface_bytes_per_pixel(surface->format);
	row_bytes = width * bytes_pp;
	if (src_pitch == 0) {
		src_pitch = row_bytes;
	}
	if (src_pitch < row_bytes) {
		return (SRAPI_ERR_RANGE);
	}
	src = (const uint8_t *)pixels;
	dst = (uint8_t *)surface->pixels + (size_t)y * surface->pitch +
	    (size_t)x * bytes_pp;
	for (row = 0; row < height; row++) {
		memcpy(dst + (size_t)row * surface->pitch,
		    src + (size_t)row * src_pitch, row_bytes);
	}
	return (SRAPI_OK);
}

int
srapiSurfaceSetPalette(srapi_surface_t *surface, uint32_t first,
    const uint32_t *colors, uint32_t count)
{
	if (!surface || !colors || count == 0 || first >= 256 ||
	    count > 256 - first) {
		return (SRAPI_ERR_INVALID);
	}
	if (surface->format != SRAPI_SURFACE_FORMAT_INDEX8) {
		return (SRAPI_ERR_UNSUPPORTED);
	}
	memcpy(&surface->palette[first], colors, sizeof(uint32_t) * count);
	return (SRAPI_OK);
}
