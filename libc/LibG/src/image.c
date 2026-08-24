/* !DEFINES!

$define %type libg_image as decoded raster wrapper around li_image_t
$define %type libg_context_t as LibG immediate UI context
$define %type li_image_t as LibImage ARGB8888 decode result
$define %type uint32_t as 32 bit unsigned
$define %type int32_t as 32 bit signed
$define %type size_t as unsigned size
$define %func libgImageProbe as function with args bytes, length
$define %func libgImageLoad as function with args bytes, length, out
$define %func libgImageLoadFile as function with args path, out
$define %func libgImageFree as procedure with args image
$define %func libgImageWidth as function with args image
$define %func libgImageHeight as function with args image
$define %func libgImageDraw as procedure with args context, image, x, y
$define %func libgImageDrawScaled as procedure with args context, image, rect
$define %func libgImageStrerror as function with args error code
$define %func libg_image_map_err as function with args LibImage error
$define %func libg_image_clip as function with args context, rect, out bounds
$define %func libg_image_blit_span as procedure with args dst, src, count

*/

/* !SPACE!

$space %internal libg_image_map_err, libg_image_clip, libg_image_blit_span
$space %export libgImageProbe, libgImageLoad, libgImageLoadFile
$space %export libgImageFree
$space %export libgImageWidth, libgImageHeight
$space %export libgImageDraw, libgImageDrawScaled, libgImageStrerror

*/

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

#include <libg.h>
#include <libimage.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "libg_int.h"
struct libg_image {
	li_image_t	*img;
};

typedef struct libg_image_bounds {
	int32_t	x0, y0, x1, y1;
} libg_image_bounds_t;

static int
libg_image_map_err(int err)
{
	switch (err) {
	case LI_OK:
		return (LIBG_OK);
	case LI_ERR_NOMEM:
		return (LIBG_ERR_NOMEM);
	case LI_ERR_INVAL:
		return (LIBG_ERR_INVAL);
	default:
		return (LIBG_ERR_DRIVER);
	}
}

static int
libg_image_clip(const libg_context_t *ctx, int32_t x, int32_t y,
    int32_t w, int32_t h, libg_image_bounds_t *out)
{
	out->x0 = x;
	out->y0 = y;
	out->x1 = x + w;
	out->y1 = y + h;

	if (ctx->clip_valid) {
		if (out->x0 < ctx->clip_x0) {
			out->x0 = ctx->clip_x0;
		}
		if (out->y0 < ctx->clip_y0) {
			out->y0 = ctx->clip_y0;
		}
		if (out->x1 > ctx->clip_x1) {
			out->x1 = ctx->clip_x1;
		}
		if (out->y1 > ctx->clip_y1) {
			out->y1 = ctx->clip_y1;
		}
	}
	if (out->x0 < 0) {
		out->x0 = 0;
	}
	if (out->y0 < 0) {
		out->y0 = 0;
	}
	if (out->x1 > (int32_t)ctx->width) {
		out->x1 = (int32_t)ctx->width;
	}
	if (out->y1 > (int32_t)ctx->height) {
		out->y1 = (int32_t)ctx->height;
	}
	if (out->x0 >= out->x1 || out->y0 >= out->y1) {
		return (0);
	}
	return (1);
}

static void
libg_image_blit_span(uint32_t *dst, const uint32_t *src, uint32_t count)
{
	uint32_t	pixel;
	uint32_t	i;

	for (i = 0; i < count; i++) {
		pixel = src[i];
		if ((pixel >> 24) == 0xffu) {
			dst[i] = pixel;
		} else if ((pixel >> 24) != 0u) {
			dst[i] = libg_blend(dst[i], pixel);
		}
	}
}


int
libgImageProbe(const void *data, size_t len)
{
	if (!data || len == 0) {
		return (0);
	}
	return (li_probe(data, len) != LI_FORMAT_UNKNOWN);
}

int
libgImageLoad(const void *data, size_t len, libg_image_t **out)
{
	libg_image_t	*wrap;
	li_image_t	*decoded;
	int		err;

	if (!out) {
		return (LIBG_ERR_INVAL);
	}
	*out = NULL;
	if (!data || len == 0) {
		return (LIBG_ERR_INVAL);
	}

	decoded = NULL;
	err = li_load_memory(data, len, &decoded);
	if (err != LI_OK) {
		return (libg_image_map_err(err));
	}

	wrap = (libg_image_t *)malloc(sizeof(*wrap));
	if (!wrap) {
		li_free(decoded);
		return (LIBG_ERR_NOMEM);
	}
	wrap->img = decoded;
	*out = wrap;
	return (LIBG_OK);
}

int
libgImageLoadFile(const char *path, libg_image_t **out)
{
	libg_image_t	*wrap;
	li_image_t	*decoded;
	int		err;

	if (!out) {
		return (LIBG_ERR_INVAL);
	}
	*out = NULL;
	if (!path || path[0] == '\0') {
		return (LIBG_ERR_INVAL);
	}

	decoded = NULL;
	err = li_load_file(path, &decoded);
	if (err != LI_OK) {
		return (libg_image_map_err(err));
	}

	wrap = (libg_image_t *)malloc(sizeof(*wrap));
	if (!wrap) {
		li_free(decoded);
		return (LIBG_ERR_NOMEM);
	}
	wrap->img = decoded;
	*out = wrap;
	return (LIBG_OK);
}

void
libgImageFree(libg_image_t *img)
{
	if (!img) {
		return;
	}
	li_free(img->img);
	img->img = NULL;
	free(img);
}

int32_t
libgImageWidth(const libg_image_t *img)
{
	if (!img || !img->img) {
		return (0);
	}
	return ((int32_t)img->img->width);
}

int32_t
libgImageHeight(const libg_image_t *img)
{
	if (!img || !img->img) {
		return (0);
	}
	return ((int32_t)img->img->height);
}

void
libgImageDraw(libg_context_t *ctx, const libg_image_t *img,
    int32_t x, int32_t y)
{
	libg_image_bounds_t	 b;
	const li_image_t	*src;
	const uint32_t		*srow;
	uint32_t		*drow;
	uint32_t		 spitch;
	int32_t			 row;

	if (!ctx || !ctx->pixels || !img || !img->img) {
		return;
	}
	src = img->img;
	if (src->width == 0 || src->height == 0 || !src->pixels) {
		return;
	}
	if ((src->pitch % sizeof(uint32_t)) != 0) {
		return;
	}
	spitch = src->pitch / (uint32_t)sizeof(uint32_t);
	if (!libg_image_clip(ctx, x, y, (int32_t)src->width,
	    (int32_t)src->height, &b)) {
		return;
	}

	libg_mark_dirty(ctx, b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0);

	for (row = b.y0; row < b.y1; row++) {
		srow = src->pixels + (uint32_t)(row - y) * spitch +
		    (uint32_t)(b.x0 - x);
		drow = (uint32_t *)(void *)(ctx->pixels + (uint32_t)row *
		    ctx->pitch + (uint32_t)b.x0 * sizeof(uint32_t));
		libg_image_blit_span(drow, srow, (uint32_t)(b.x1 - b.x0));
	}
}

void
libgImageDrawScaled(libg_context_t *ctx, const libg_image_t *img,
    libg_rect_t rect)
{
	libg_image_bounds_t	 b;
	const li_image_t	*src;
	const uint32_t		*srow;
	uint32_t		*drow;
	uint32_t		 step_x, step_y;
	uint32_t		 spitch;
	uint32_t		 sx, sy;
	uint32_t		 pixel;
	int32_t			 row, col;

	if (!ctx || !ctx->pixels || !img || !img->img) {
		return;
	}
	if (rect.width <= 0 || rect.height <= 0) {
		return;
	}
	src = img->img;
	if (src->width == 0 || src->height == 0 || !src->pixels) {
		return;
	}
	if ((src->pitch % sizeof(uint32_t)) != 0) {
		return;
	}
	spitch = src->pitch / (uint32_t)sizeof(uint32_t);
	if (!libg_image_clip(ctx, rect.x, rect.y, rect.width, rect.height,
	    &b)) {
		return;
	}

	step_x = (src->width << 16) / (uint32_t)rect.width;
	step_y = (src->height << 16) / (uint32_t)rect.height;

	libg_mark_dirty(ctx, b.x0, b.y0, b.x1 - b.x0, b.y1 - b.y0);

	for (row = b.y0; row < b.y1; row++) {
		sy = ((uint32_t)(row - rect.y) * step_y) >> 16;
		if (sy >= src->height) {
			sy = src->height - 1;
		}
		srow = src->pixels + sy * spitch;
		drow = (uint32_t *)(void *)(ctx->pixels + (uint32_t)row *
		    ctx->pitch + (uint32_t)b.x0 * sizeof(uint32_t));
		for (col = b.x0; col < b.x1; col++) {
			sx = ((uint32_t)(col - rect.x) * step_x) >> 16;
			if (sx >= src->width) {
				sx = src->width - 1;
			}
			pixel = srow[sx];
			if ((pixel >> 24) == 0xffu) {
				drow[col - b.x0] = pixel;
			} else if ((pixel >> 24) != 0u) {
				drow[col - b.x0] =
				    libg_blend(drow[col - b.x0], pixel);
			}
		}
	}
}

const char *
libgImageStrerror(int err)
{
	switch (err) {
	case LIBG_OK:
		return ("ok");
	case LIBG_ERR_INVAL:
		return ("invalid argument");
	case LIBG_ERR_NOMEM:
		return ("out of memory");
	case LIBG_ERR_DRIVER:
		return ("image not decodable");
	default:
		return ("unknown error");
	}
}
