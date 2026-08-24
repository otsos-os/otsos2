/* !DEFINES!

$define %type li_png as PNG header state carried between chunks
$define %func li_png_load as function with args const void *, size_t, li_image **
$define %func li_probe as function with args const void *, size_t
$define %func li_load_memory as function with args const void *, size_t, li_image **
$define %func li_load_file as function with args const char *, li_image **
$define %func li_free as procedure with args li_image *
$define %func li_strerror as function with args int
$define %func png_inflate_err as function with args int

*/

/* !SPACE!

$space %internal li_png_t, png_inflate_err
$space %internal png_be32, png_sample, png_scale, png_paeth
$space %internal png_channels, png_depth_ok, png_raw_size
$space %internal png_unfilter, png_expand_row, png_decode_pixels
$space %internal png_read_ihdr, png_read_plte, png_read_trns, png_append_idat
$space %export li_png_load, li_probe, li_load_memory, li_load_file
$space %export li_free, li_strerror

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

#include <libarchive.h>
#include <libimage.h>
#include <native.h>
#include <stdlib.h>
#include <string.h>

#define PNG_SIG_LEN		8
#define PNG_CHUNK_HDR		8
#define PNG_CHUNK_CRC		4
#define PNG_IHDR_LEN		13
#define PNG_PALETTE_MAX		256

static int
png_inflate_err(int err)
{
	switch (err) {
	case LA_INF_OK:
		return (LI_OK);
	case LA_INF_INVAL:
		return (LI_ERR_INVAL);
	case LA_INF_UNSUPPORTED:
		return (LI_ERR_UNSUPPORTED);
	case LA_INF_TOO_LARGE:
		return (LI_ERR_TOO_LARGE);
	case LA_INF_IO:
		return (LI_ERR_IO);
	case LA_INF_CORRUPT:
	default:
		return (LI_ERR_CORRUPT);
	}
}

#define PNG_COLOR_GRAY		0
#define PNG_COLOR_RGB		2
#define PNG_COLOR_PALETTE	3
#define PNG_COLOR_GRAY_ALPHA	4
#define PNG_COLOR_RGBA		6

#define PNG_FILTER_NONE		0
#define PNG_FILTER_SUB		1
#define PNG_FILTER_UP		2
#define PNG_FILTER_AVG		3
#define PNG_FILTER_PAETH	4

#define PNG_TYPE(a, b, c, d)	(((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) \
				| ((uint32_t)(c) << 8) | (uint32_t)(d))
#define PNG_IHDR		PNG_TYPE('I', 'H', 'D', 'R')
#define PNG_PLTE		PNG_TYPE('P', 'L', 'T', 'E')
#define PNG_TRNS		PNG_TYPE('t', 'R', 'N', 'S')
#define PNG_IDAT		PNG_TYPE('I', 'D', 'A', 'T')
#define PNG_IEND		PNG_TYPE('I', 'E', 'N', 'D')

#define PNG_IDAT_INIT		16384u

static const uint8_t png_signature[PNG_SIG_LEN] = {
	0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A
};

static const uint32_t png_pass_x0[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const uint32_t png_pass_y0[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const uint32_t png_pass_dx[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const uint32_t png_pass_dy[7] = { 8, 8, 8, 4, 4, 2, 2 };

typedef struct li_png {
	uint32_t	palette[PNG_PALETTE_MAX];
	uint32_t	width;
	uint32_t	height;
	uint32_t	palette_count;
	uint32_t	trans_gray;
	uint32_t	trans_r;
	uint32_t	trans_g;
	uint32_t	trans_b;
	uint8_t		depth;
	uint8_t		color;
	uint8_t		interlace;
	int		has_trans_key;
} li_png_t;

static uint32_t
png_be32(const uint8_t *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static uint32_t
png_channels(uint8_t color)
{
	switch (color) {
	case PNG_COLOR_GRAY:
	case PNG_COLOR_PALETTE:
		return (1);
	case PNG_COLOR_GRAY_ALPHA:
		return (2);
	case PNG_COLOR_RGB:
		return (3);
	case PNG_COLOR_RGBA:
		return (4);
	default:
		return (0);
	}
}

static int
png_depth_ok(uint8_t color, uint8_t depth)
{
	switch (color) {
	case PNG_COLOR_GRAY:
		return (depth == 1 || depth == 2 || depth == 4 ||
		    depth == 8 || depth == 16);
	case PNG_COLOR_PALETTE:
		return (depth == 1 || depth == 2 || depth == 4 || depth == 8);
	case PNG_COLOR_RGB:
	case PNG_COLOR_GRAY_ALPHA:
	case PNG_COLOR_RGBA:
		return (depth == 8 || depth == 16);
	default:
		return (0);
	}
}

static uint32_t
png_sample(const uint8_t *row, uint32_t index, uint8_t depth)
{
	uint32_t	byte, shift, mask;

	switch (depth) {
	case 16:
		return (((uint32_t)row[index * 2] << 8) |
		    (uint32_t)row[index * 2 + 1]);
	case 8:
		return (row[index]);
	default:
		byte = index / (8u / depth);
		shift = 8u - depth - (index % (8u / depth)) * depth;
		mask = (1u << depth) - 1u;
		return ((uint32_t)(row[byte] >> shift) & mask);
	}
}

static uint32_t
png_scale(uint32_t value, uint8_t depth)
{
	switch (depth) {
	case 16:
		return (value >> 8);
	case 8:
		return (value);
	case 4:
		return (value * 17u);
	case 2:
		return (value * 85u);
	case 1:
		return (value != 0 ? 255u : 0u);
	default:
		return (0);
	}
}

static int
png_paeth(int a, int b, int c)
{
	int	p, pa, pb, pc;

	p = a + b - c;
	pa = (p > a) ? p - a : a - p;
	pb = (p > b) ? p - b : b - p;
	pc = (p > c) ? p - c : c - p;
	if (pa <= pb && pa <= pc) {
		return (a);
	}
	if (pb <= pc) {
		return (b);
	}
	return (c);
}
static size_t
png_raw_size(const li_png_t *png)
{
	size_t		total, row_bits, row_bytes;
	uint32_t	channels, pw, ph, pass;

	channels = png_channels(png->color);
	if (channels == 0) {
		return (0);
	}

	if (png->interlace == 0) {
		row_bits = (size_t)png->width * channels * png->depth;
		row_bytes = (row_bits + 7u) / 8u;
		return ((row_bytes + 1u) * png->height);
	}

	total = 0;
	for (pass = 0; pass < 7; pass++) {
		if (png->width <= png_pass_x0[pass] ||
		    png->height <= png_pass_y0[pass]) {
			continue;	/* pass has no pixels at this size */
		}
		pw = (png->width - png_pass_x0[pass] + png_pass_dx[pass] - 1) /
		    png_pass_dx[pass];
		ph = (png->height - png_pass_y0[pass] + png_pass_dy[pass] - 1) /
		    png_pass_dy[pass];
		if (pw == 0 || ph == 0) {
			continue;
		}
		row_bits = (size_t)pw * channels * png->depth;
		row_bytes = (row_bits + 7u) / 8u;
		total += (row_bytes + 1u) * ph;
	}
	return (total);
}

static int
png_unfilter(uint8_t *row, const uint8_t *prev, size_t row_bytes, uint32_t bpp,
    uint8_t filter)
{
	size_t	i;
	int	a, b, c;

	switch (filter) {
	case PNG_FILTER_NONE:
		return (LI_OK);
	case PNG_FILTER_SUB:
		for (i = bpp; i < row_bytes; i++) {
			row[i] = (uint8_t)(row[i] + row[i - bpp]);
		}
		return (LI_OK);
	case PNG_FILTER_UP:
		if (prev == NULL) {
			return (LI_OK);
		}
		for (i = 0; i < row_bytes; i++) {
			row[i] = (uint8_t)(row[i] + prev[i]);
		}
		return (LI_OK);
	case PNG_FILTER_AVG:
		for (i = 0; i < row_bytes; i++) {
			a = (i >= bpp) ? row[i - bpp] : 0;
			b = (prev != NULL) ? prev[i] : 0;
			row[i] = (uint8_t)(row[i] + (a + b) / 2);
		}
		return (LI_OK);
	case PNG_FILTER_PAETH:
		for (i = 0; i < row_bytes; i++) {
			a = (i >= bpp) ? row[i - bpp] : 0;
			b = (prev != NULL) ? prev[i] : 0;
			c = (prev != NULL && i >= bpp) ? prev[i - bpp] : 0;
			row[i] = (uint8_t)(row[i] + png_paeth(a, b, c));
		}
		return (LI_OK);
	default:
		return (LI_ERR_CORRUPT);
	}
}

static int
png_expand_row(const li_png_t *png, const uint8_t *row, uint32_t count,
    uint32_t *dst, uint32_t step)
{
	uint32_t	x, idx, a, r, g, b, s0, s1, s2, s3;

	for (x = 0; x < count; x++) {
		switch (png->color) {
		case PNG_COLOR_GRAY:
			s0 = png_sample(row, x, png->depth);
			a = 255;
			if (png->has_trans_key && s0 == png->trans_gray) {
				a = 0;
			}
			r = png_scale(s0, png->depth);
			g = r;
			b = r;
			break;
		case PNG_COLOR_GRAY_ALPHA:
			s0 = png_sample(row, x * 2, png->depth);
			s1 = png_sample(row, x * 2 + 1, png->depth);
			r = png_scale(s0, png->depth);
			g = r;
			b = r;
			a = png_scale(s1, png->depth);
			break;
		case PNG_COLOR_RGB:
			s0 = png_sample(row, x * 3, png->depth);
			s1 = png_sample(row, x * 3 + 1, png->depth);
			s2 = png_sample(row, x * 3 + 2, png->depth);
			a = 255;
			if (png->has_trans_key && s0 == png->trans_r &&
			    s1 == png->trans_g && s2 == png->trans_b) {
				a = 0;
			}
			r = png_scale(s0, png->depth);
			g = png_scale(s1, png->depth);
			b = png_scale(s2, png->depth);
			break;
		case PNG_COLOR_RGBA:
			s0 = png_sample(row, x * 4, png->depth);
			s1 = png_sample(row, x * 4 + 1, png->depth);
			s2 = png_sample(row, x * 4 + 2, png->depth);
			s3 = png_sample(row, x * 4 + 3, png->depth);
			r = png_scale(s0, png->depth);
			g = png_scale(s1, png->depth);
			b = png_scale(s2, png->depth);
			a = png_scale(s3, png->depth);
			break;
		case PNG_COLOR_PALETTE:
			idx = png_sample(row, x, png->depth);
			if (idx >= png->palette_count) {
				return (LI_ERR_CORRUPT);
			}
			dst[x * step] = png->palette[idx];
			continue;
		default:
			return (LI_ERR_UNSUPPORTED);
		}
		dst[x * step] = (a << 24) | (r << 16) | (g << 8) | b;
	}
	return (LI_OK);
}

static int
png_decode_pixels(const li_png_t *png, const uint8_t *raw, size_t raw_len,
    uint32_t *pixels)
{
	uint8_t		*cur, *prev, *swap;
	size_t		row_bytes, offset, max_row_bytes;
	uint32_t	channels, bpp, pass, pw, ph, y, first_pass, last_pass;
	int		ret;

	channels = png_channels(png->color);
	if (channels == 0) {
		return (LI_ERR_UNSUPPORTED);
	}
	bpp = (channels * png->depth + 7u) / 8u;
	if (bpp == 0) {
		bpp = 1;
	}

	max_row_bytes = ((size_t)png->width * channels * png->depth + 7u) / 8u;
	if (max_row_bytes == 0) {
		return (LI_ERR_CORRUPT);
	}
	cur = (uint8_t *)malloc(max_row_bytes);
	prev = (uint8_t *)malloc(max_row_bytes);
	if (cur == NULL || prev == NULL) {
		free(cur);
		free(prev);
		return (LI_ERR_NOMEM);
	}

	offset = 0;
	first_pass = (png->interlace != 0) ? 0 : 6;
	last_pass = 6;
	ret = LI_OK;

	for (pass = first_pass; pass <= last_pass && ret == LI_OK; pass++) {
		if (png->interlace != 0) {
			if (png->width <= png_pass_x0[pass] ||
			    png->height <= png_pass_y0[pass]) {
				continue;
			}
			pw = (png->width - png_pass_x0[pass] +
			    png_pass_dx[pass] - 1) / png_pass_dx[pass];
			ph = (png->height - png_pass_y0[pass] +
			    png_pass_dy[pass] - 1) / png_pass_dy[pass];
		} else {
			pw = png->width;
			ph = png->height;
		}
		if (pw == 0 || ph == 0) {
			continue;
		}
		row_bytes = ((size_t)pw * channels * png->depth + 7u) / 8u;

		memset(prev, 0, row_bytes);

		for (y = 0; y < ph; y++) {
			uint32_t	*dst;
			uint32_t	out_y, step;
			uint8_t		filter;

			if (offset + 1 + row_bytes > raw_len) {
				ret = LI_ERR_CORRUPT;
				break;
			}
			filter = raw[offset];
			memcpy(cur, raw + offset + 1, row_bytes);
			offset += 1 + row_bytes;

			ret = png_unfilter(cur, prev, row_bytes, bpp, filter);
			if (ret != LI_OK) {
				break;
			}

			if (png->interlace != 0) {
				out_y = png_pass_y0[pass] + y *
				    png_pass_dy[pass];
				step = png_pass_dx[pass];
				dst = pixels + (size_t)out_y * png->width +
				    png_pass_x0[pass];
			} else {
				out_y = y;
				step = 1;
				dst = pixels + (size_t)out_y * png->width;
			}
			ret = png_expand_row(png, cur, pw, dst, step);
			if (ret != LI_OK) {
				break;
			}

			swap = prev;
			prev = cur;
			cur = swap;
		}
	}

	free(cur);
	free(prev);
	return (ret);
}

static int
png_read_ihdr(li_png_t *png, const uint8_t *data, uint32_t len)
{
	if (len != PNG_IHDR_LEN) {
		return (LI_ERR_CORRUPT);
	}
	png->width = png_be32(data);
	png->height = png_be32(data + 4);
	png->depth = data[8];
	png->color = data[9];
	png->interlace = data[12];

	if (png->width == 0 || png->height == 0) {
		return (LI_ERR_CORRUPT);
	}
	if (png->width > LI_MAX_DIM || png->height > LI_MAX_DIM) {
		return (LI_ERR_TOO_LARGE);
	}
	if ((uint64_t)png->width * png->height > LI_MAX_PIXELS) {
		return (LI_ERR_TOO_LARGE);
	}
	if (!png_depth_ok(png->color, png->depth)) {
		return (LI_ERR_CORRUPT);
	}
	if (data[10] != 0 || data[11] != 0) {
		return (LI_ERR_UNSUPPORTED);
	}
	if (png->interlace > 1) {
		return (LI_ERR_UNSUPPORTED);
	}
	return (LI_OK);
}

static int
png_read_plte(li_png_t *png, const uint8_t *data, uint32_t len)
{
	uint32_t	i, n;

	if (len == 0 || len % 3u != 0u) {
		return (LI_ERR_CORRUPT);
	}
	n = len / 3u;
	if (n > PNG_PALETTE_MAX) {
		return (LI_ERR_CORRUPT);
	}
	for (i = 0; i < n; i++) {
		png->palette[i] = 0xFF000000u |
		    ((uint32_t)data[i * 3] << 16) |
		    ((uint32_t)data[i * 3 + 1] << 8) |
		    (uint32_t)data[i * 3 + 2];
	}
	png->palette_count = n;
	return (LI_OK);
}

static int
png_read_trns(li_png_t *png, const uint8_t *data, uint32_t len)
{
	uint32_t	i;

	switch (png->color) {
	case PNG_COLOR_PALETTE:
		if (png->palette_count == 0 || len > png->palette_count) {
			return (LI_ERR_CORRUPT);
		}
		for (i = 0; i < len; i++) {
			png->palette[i] = (png->palette[i] & 0x00FFFFFFu) |
			    ((uint32_t)data[i] << 24);
		}
		return (LI_OK);
	case PNG_COLOR_GRAY:
		if (len != 2) {
			return (LI_ERR_CORRUPT);
		}
		png->trans_gray = ((uint32_t)data[0] << 8) | data[1];
		png->has_trans_key = 1;
		return (LI_OK);
	case PNG_COLOR_RGB:
		if (len != 6) {
			return (LI_ERR_CORRUPT);
		}
		png->trans_r = ((uint32_t)data[0] << 8) | data[1];
		png->trans_g = ((uint32_t)data[2] << 8) | data[3];
		png->trans_b = ((uint32_t)data[4] << 8) | data[5];
		png->has_trans_key = 1;
		return (LI_OK);
	default:
		return (LI_ERR_CORRUPT);
	}
}

static int
png_append_idat(uint8_t **buf, size_t *len, size_t *cap, const uint8_t *data,
    uint32_t chunk_len)
{
	uint8_t	*grown;
	size_t	want;

	if (*len + chunk_len > LI_MAX_FILE_BYTES) {
		return (LI_ERR_TOO_LARGE);
	}
	if (*len + chunk_len > *cap) {
		want = (*cap != 0) ? *cap : PNG_IDAT_INIT;
		while (want < *len + chunk_len) {
			want *= 2u;
			if (want > LI_MAX_FILE_BYTES) {
				want = LI_MAX_FILE_BYTES;
				break;
			}
		}
		if (want < *len + chunk_len) {
			return (LI_ERR_TOO_LARGE);
		}
		grown = (uint8_t *)realloc(*buf, want);
		if (grown == NULL) {
			return (LI_ERR_NOMEM);
		}
		*buf = grown;
		*cap = want;
	}
	memcpy(*buf + *len, data, chunk_len);
	*len += chunk_len;
	return (LI_OK);
}

int
li_png_load(const void *data, size_t len, li_image_t **out)
{
	const uint8_t	*p;
	li_png_t	png;
	li_image_t	*img;
	uint8_t		*idat, *raw;
	uint32_t	*pixels;
	size_t		pos, idat_len, idat_cap, raw_cap, raw_len;
	uint32_t	chunk_len, type;
	int		ret, seen_ihdr, seen_idat;

	if (data == NULL || out == NULL) {
		return (LI_ERR_INVAL);
	}
	*out = NULL;
	if (len < PNG_SIG_LEN + PNG_CHUNK_HDR + PNG_CHUNK_CRC ||
	    memcmp(data, png_signature, PNG_SIG_LEN) != 0) {
		return (LI_ERR_FORMAT);
	}

	memset(&png, 0, sizeof(png));
	p = (const uint8_t *)data;
	pos = PNG_SIG_LEN;
	idat = NULL;
	idat_len = 0;
	idat_cap = 0;
	seen_ihdr = 0;
	seen_idat = 0;
	ret = LI_OK;

	while (pos + PNG_CHUNK_HDR + PNG_CHUNK_CRC <= len) {
		chunk_len = png_be32(p + pos);
		type = png_be32(p + pos + 4);
		if (chunk_len > LI_MAX_FILE_BYTES ||
		    pos + PNG_CHUNK_HDR + (size_t)chunk_len + PNG_CHUNK_CRC >
		    len) {
			ret = LI_ERR_CORRUPT;
			break;
		}

		if (la_crc32(p + pos + 4, chunk_len + 4u) !=
		    png_be32(p + pos + PNG_CHUNK_HDR + chunk_len)) {
			ret = LI_ERR_CORRUPT;
			break;
		}

		switch (type) {
		case PNG_IHDR:
			if (seen_ihdr) {
				ret = LI_ERR_CORRUPT;
				break;
			}
			ret = png_read_ihdr(&png, p + pos + PNG_CHUNK_HDR,
			    chunk_len);
			seen_ihdr = 1;
			break;
		case PNG_PLTE:
			if (!seen_ihdr || seen_idat) {
				ret = LI_ERR_CORRUPT;
				break;
			}
			ret = png_read_plte(&png, p + pos + PNG_CHUNK_HDR,
			    chunk_len);
			break;
		case PNG_TRNS:
			if (!seen_ihdr || seen_idat) {
				ret = LI_ERR_CORRUPT;
				break;
			}
			ret = png_read_trns(&png, p + pos + PNG_CHUNK_HDR,
			    chunk_len);
			break;
		case PNG_IDAT:
			if (!seen_ihdr) {
				ret = LI_ERR_CORRUPT;
				break;
			}
			seen_idat = 1;
			ret = png_append_idat(&idat, &idat_len, &idat_cap,
			    p + pos + PNG_CHUNK_HDR, chunk_len);
			break;
		case PNG_IEND:
			pos = len;
			break;
		default:
			if ((p[pos + 4] & 0x20u) == 0u) {
				ret = LI_ERR_UNSUPPORTED;
			}
			break;
		}
		if (ret != LI_OK) {
			break;
		}
		if (pos == len) {
			break;
		}
		pos += PNG_CHUNK_HDR + (size_t)chunk_len + PNG_CHUNK_CRC;
	}

	if (ret == LI_OK && (!seen_ihdr || !seen_idat || idat_len == 0)) {
		ret = LI_ERR_CORRUPT;
	}
	if (ret == LI_OK && png.color == PNG_COLOR_PALETTE &&
	    png.palette_count == 0) {
		ret = LI_ERR_CORRUPT;
	}
	if (ret != LI_OK) {
		free(idat);
		return (ret);
	}

	raw_cap = png_raw_size(&png);
	if (raw_cap == 0) {
		free(idat);
		return (LI_ERR_CORRUPT);
	}
	raw = (uint8_t *)malloc(raw_cap);
	if (raw == NULL) {
		free(idat);
		return (LI_ERR_NOMEM);
	}

	raw_len = 0;
	ret = png_inflate_err(la_zlib_inflate(idat, idat_len, raw, raw_cap,
	    &raw_len));
	free(idat);
	if (ret != LI_OK) {
		free(raw);
		return (ret);
	}

	if (raw_len != raw_cap) {
		free(raw);
		return (LI_ERR_CORRUPT);
	}

	pixels = (uint32_t *)malloc((size_t)png.width * png.height *
	    sizeof(uint32_t));
	if (pixels == NULL) {
		free(raw);
		return (LI_ERR_NOMEM);
	}

	ret = png_decode_pixels(&png, raw, raw_len, pixels);
	free(raw);
	if (ret != LI_OK) {
		free(pixels);
		return (ret);
	}

	img = (li_image_t *)malloc(sizeof(*img));
	if (img == NULL) {
		free(pixels);
		return (LI_ERR_NOMEM);
	}
	memset(img, 0, sizeof(*img));
	img->pixels = pixels;
	img->width = png.width;
	img->height = png.height;
	img->pitch = png.width * (uint32_t)sizeof(uint32_t);
	img->format = LI_FORMAT_PNG;
	img->has_alpha = (png.color == PNG_COLOR_RGBA ||
	    png.color == PNG_COLOR_GRAY_ALPHA || png.has_trans_key ||
	    png.color == PNG_COLOR_PALETTE);
	*out = img;
	return (LI_OK);
}

int
li_probe(const void *data, size_t len)
{
	if (data == NULL || len < PNG_SIG_LEN) {
		return (LI_FORMAT_UNKNOWN);
	}
	if (memcmp(data, png_signature, PNG_SIG_LEN) == 0) {
		return (LI_FORMAT_PNG);
	}
	return (LI_FORMAT_UNKNOWN);
}

int
li_load_memory(const void *data, size_t len, li_image_t **out)
{
	if (data == NULL || out == NULL) {
		return (LI_ERR_INVAL);
	}
	*out = NULL;

	switch (li_probe(data, len)) {
	case LI_FORMAT_PNG:
		return (li_png_load(data, len, out));
	default:
		return (LI_ERR_FORMAT);
	}
}

int
li_load_file(const char *path, li_image_t **out)
{
	uint8_t	*buf;
	long	size;
	size_t	got;
	int	fd, ret;
	ssize_t	n;

	if (path == NULL || out == NULL) {
		return (LI_ERR_INVAL);
	}
	*out = NULL;

	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		return (LI_ERR_IO);
	}
	size = dataSeek(fd, 0, API_SEEK_END);
	if (size <= 0) {
		dataClose(fd);
		return (LI_ERR_IO);
	}
	if ((size_t)size > LI_MAX_FILE_BYTES) {
		dataClose(fd);
		return (LI_ERR_TOO_LARGE);
	}
	if (dataSeek(fd, 0, API_SEEK_SET) != 0) {
		dataClose(fd);
		return (LI_ERR_IO);
	}

	buf = (uint8_t *)malloc((size_t)size);
	if (buf == NULL) {
		dataClose(fd);
		return (LI_ERR_NOMEM);
	}

	got = 0;
	while (got < (size_t)size) {
		n = dataRead(fd, buf + got, (size_t)size - got);
		if (n <= 0) {
			break;
		}
		got += (size_t)n;
	}
	dataClose(fd);

	if (got != (size_t)size) {
		free(buf);
		return (LI_ERR_IO);
	}

	ret = li_load_memory(buf, got, out);
	free(buf);
	return (ret);
}

void
li_free(li_image_t *img)
{
	if (img == NULL) {
		return;
	}
	free(img->pixels);
	free(img);
}

const char *
li_strerror(int err)
{
	switch (err) {
	case LI_OK:
		return ("ok");
	case LI_ERR_INVAL:
		return ("invalid argument");
	case LI_ERR_NOMEM:
		return ("out of memory");
	case LI_ERR_FORMAT:
		return ("unrecognized image format");
	case LI_ERR_UNSUPPORTED:
		return ("unsupported image feature");
	case LI_ERR_CORRUPT:
		return ("corrupt image data");
	case LI_ERR_TOO_LARGE:
		return ("image too large");
	case LI_ERR_IO:
		return ("image read failed");
	default:
		return ("unknown image error");
	}
}
