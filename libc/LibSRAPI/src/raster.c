/* !DEFINES!

$define %type srapi_vertex_out as transformed vertex data
$define %func srapi_raster_clear as function with args image, color
$define %func srapi_raster_clear_rect as function with args image, rect, color
$define %func srapi_raster_draw as function with args draw state

*/

/* !SPACE!

$space %internal clamp_i32, clamp_i64_to_i32, store_pixel
$space %internal fixed_to_u8, pack_color
$space %internal setup_vertex_input, fetch_vertex, edge_fn, choose_shift
$space %internal interp_slot, interp_delta_slot, shade_fragment
$space %internal clear_row_16, clear_row_24, clear_row_32
$space %internal raster_triangle
$space %export srapi_raster_clear, srapi_raster_clear_rect
$space %export srapi_raster_draw

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <srapi.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "srapi_private.h"

static int32_t
clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
	if (v < lo) {
		return (lo);
	}
	if (v > hi) {
		return (hi);
	}
	return (v);
}

static int32_t
clamp_i64_to_i32(int64_t v)
{
	if (v > 2147483647LL) {
		return (2147483647);
	}
	if (v < (-2147483647LL - 1LL)) {
		return ((int32_t)(-2147483647LL - 1LL));
	}
	return ((int32_t)v);
}

static void
store_pixel(srapi_image_t *image, uint32_t x, uint32_t y, uint32_t color)
{
	uint8_t		*base;
	uint64_t	offset;
	uint32_t	bytes_pp, r, g, b;
	uint16_t	rgb565;

	if (!image || !image->pixels || x >= image->width ||
	    y >= image->height) {
		return;
	}
	bytes_pp = image->bpp / 8;
	base = (uint8_t *)image->pixels;
	offset = (uint64_t)y * image->pitch + (uint64_t)x * bytes_pp;
	if (offset + bytes_pp > image->size) {
		return;
	}
	if (bytes_pp == 4) {
		*(uint32_t *)(base + offset) = color;
	} else if (bytes_pp == 3) {
		base[offset] = (uint8_t)(color & 0xff);
		base[offset + 1] = (uint8_t)((color >> 8) & 0xff);
		base[offset + 2] = (uint8_t)((color >> 16) & 0xff);
	} else if (bytes_pp == 2) {
		r = (color >> 16) & 0xff;
		g = (color >> 8) & 0xff;
		b = color & 0xff;
		rgb565 = (uint16_t)(((r >> 3) << 11) |
		    ((g >> 2) << 5) | (b >> 3));
		*(uint16_t *)(base + offset) = rgb565;
	}
}

static uint32_t
fixed_to_u8(int32_t v)
{
	v = clamp_i32(v, 0, SRAPI_FIXED_ONE);
	return ((uint32_t)(((int64_t)v * 255) / SRAPI_FIXED_ONE));
}

static uint32_t
pack_color(const int32_t *io)
{
	uint32_t	r, g, b, a;

	r = fixed_to_u8(io[SRAPI_IO_R]);
	g = fixed_to_u8(io[SRAPI_IO_G]);
	b = fixed_to_u8(io[SRAPI_IO_B]);
	a = fixed_to_u8(io[SRAPI_IO_A]);
	return ((a << 24) | (r << 16) | (g << 8) | b);
}

static int
setup_vertex_input(const srapi_pipeline_t *pipeline, const uint8_t *vertex,
    int32_t *input)
{
	const struct srapi_vertex_attr	*attr;
	const int32_t			*fixed;
	uint32_t			color, i, need;

	memset(input, 0, sizeof(int32_t) * SRAPI_VM_IO_SLOTS);
	input[SRAPI_IO_W] = SRAPI_FIXED_ONE;
	input[SRAPI_IO_A] = SRAPI_FIXED_ONE;
	for (i = 0; i < pipeline->vertex_layout.attr_count; i++) {
		attr = &pipeline->vertex_layout.attrs[i];
		if (attr->location == SRAPI_VERTEX_LOCATION_POSITION &&
		    attr->format == SRAPI_VERTEX_FORMAT_FIXED2) {
			need = sizeof(int32_t) * 2;
			if (attr->offset > pipeline->vertex_layout.stride ||
			    need > pipeline->vertex_layout.stride -
			    attr->offset) {
				return (SRAPI_ERR_RANGE);
			}
			fixed = (const int32_t *)(const void *)(vertex +
			    attr->offset);
			input[SRAPI_IO_X] = fixed[0];
			input[SRAPI_IO_Y] = fixed[1];
		} else if (attr->location == SRAPI_VERTEX_LOCATION_COLOR &&
		    attr->format == SRAPI_VERTEX_FORMAT_UNORM8_4) {
			need = sizeof(uint32_t);
			if (attr->offset > pipeline->vertex_layout.stride ||
			    need > pipeline->vertex_layout.stride -
			    attr->offset) {
				return (SRAPI_ERR_RANGE);
			}
			color = *(const uint32_t *)(const void *)(vertex +
			    attr->offset);
			input[SRAPI_IO_A] = SRAPI_FIXED_FROM_RATIO(
			    (color >> 24) & 0xff, 255);
			input[SRAPI_IO_R] = SRAPI_FIXED_FROM_RATIO(
			    (color >> 16) & 0xff, 255);
			input[SRAPI_IO_G] = SRAPI_FIXED_FROM_RATIO(
			    (color >> 8) & 0xff, 255);
			input[SRAPI_IO_B] = SRAPI_FIXED_FROM_RATIO(
			    color & 0xff, 255);
		}
	}
	return (SRAPI_OK);
}

static int
fetch_vertex(srapi_image_t *image, srapi_pipeline_t *pipeline,
    const struct srapi_viewport *viewport, srapi_buffer_t *buffer,
    uint32_t index, struct srapi_vertex_out *out)
{
	int32_t	input[SRAPI_VM_IO_SLOTS];
	int32_t	output[SRAPI_VM_IO_SLOTS];
	uint8_t	*vertex;
	uint64_t offset;
	uint32_t vx, vy, vw, vh;
	int	ret;

	offset = (uint64_t)index * pipeline->vertex_layout.stride;
	if (offset + pipeline->vertex_layout.stride > buffer->size) {
		return (SRAPI_ERR_RANGE);
	}
	vertex = (uint8_t *)buffer->data + offset;
	ret = setup_vertex_input(pipeline, vertex, input);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	memset(output, 0, sizeof(output));
	output[SRAPI_IO_A] = SRAPI_FIXED_ONE;
	ret = srapi_vm_run(pipeline->vertex_shader, input, pipeline->push,
	    output);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	memcpy(out->io, output, sizeof(out->io));
	vx = viewport && viewport->width != 0 ? viewport->x : 0;
	vy = viewport && viewport->height != 0 ? viewport->y : 0;
	vw = viewport && viewport->width != 0 ? viewport->width : image->width;
	vh = viewport && viewport->height != 0 ? viewport->height : image->height;
	if (vx >= image->width || vy >= image->height) {
		return (SRAPI_ERR_RANGE);
	}
	if (vw > image->width - vx) {
		vw = image->width - vx;
	}
	if (vh > image->height - vy) {
		vh = image->height - vy;
	}
	if (vw == 0 || vh == 0) {
		return (SRAPI_ERR_RANGE);
	}
	out->sx = (int32_t)(((int64_t)(output[SRAPI_IO_X] +
	    SRAPI_FIXED_ONE) * (int64_t)(vw - 1)) / 2) +
	    (int32_t)(vx << 16);
	out->sy = (int32_t)(((int64_t)(SRAPI_FIXED_ONE -
	    output[SRAPI_IO_Y]) * (int64_t)(vh - 1)) / 2) +
	    (int32_t)(vy << 16);
	return (SRAPI_OK);
}

static int64_t
edge_fn(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t px,
    int32_t py)
{
	return ((int64_t)(px - ax) * (int64_t)(by - ay) -
	    (int64_t)(py - ay) * (int64_t)(bx - ax));
}

static uint32_t
choose_shift(int64_t area)
{
	uint64_t	a;
	uint32_t	shift;

	a = area < 0 ? (uint64_t)-area : (uint64_t)area;
	shift = 16;
	while (shift > 0 && (a >> shift) == 0) {
		shift--;
	}
	while (shift < 30 && (a >> shift) > 0x3fffffffULL) {
		shift++;
	}
	return (shift);
}

static int32_t
interp_slot(const struct srapi_vertex_out *v0,
    const struct srapi_vertex_out *v1, const struct srapi_vertex_out *v2,
    uint32_t slot, int64_t w0, int64_t w1, int64_t w2, int64_t area,
    uint32_t shift)
{
	int64_t	a, r;

	a = area >> shift;
	if (a == 0) {
		return (v0->io[slot]);
	}
	w0 >>= shift;
	w1 >>= shift;
	w2 >>= shift;
	r = (int64_t)v0->io[slot] * w0 + (int64_t)v1->io[slot] * w1 +
	    (int64_t)v2->io[slot] * w2;
	return ((int32_t)(r / a));
}

static int32_t
interp_delta_slot(const struct srapi_vertex_out *v0,
    const struct srapi_vertex_out *v1, const struct srapi_vertex_out *v2,
    uint32_t slot, int64_t dw0, int64_t dw1, int64_t dw2, int64_t area)
{
	int64_t	r;

	if (area == 0) {
		return (0);
	}
	r = (int64_t)v0->io[slot] * dw0 + (int64_t)v1->io[slot] * dw1 +
	    (int64_t)v2->io[slot] * dw2;
	return (clamp_i64_to_i32(r / area));
}

static uint32_t
shade_fragment(srapi_pipeline_t *pipeline, const int32_t *input)
{
	int32_t	output[SRAPI_VM_IO_SLOTS];
	int	ret;

	memset(output, 0, sizeof(output));
	output[SRAPI_IO_A] = SRAPI_FIXED_ONE;
	ret = srapi_vm_run(pipeline->fragment_shader, input, pipeline->push,
	    output);
	if (ret != SRAPI_OK) {
		return (0xffff00ffU);
	}
	return (pack_color(output));
}

static void
raster_triangle(srapi_image_t *image, srapi_pipeline_t *pipeline,
    const struct srapi_vertex_out *v0, const struct srapi_vertex_out *v1,
    const struct srapi_vertex_out *v2)
{
	int32_t	input[SRAPI_VM_IO_SLOTS];
	int32_t	row_io[SRAPI_VM_IO_SLOTS];
	int32_t	dx_io[SRAPI_VM_IO_SLOTS];
	int32_t	dy_io[SRAPI_VM_IO_SLOTS];
	int64_t	area, w0, w1, w2, row_w0, row_w1, row_w2;
	int64_t	w0dx, w1dx, w2dx, w0dy, w1dy, w2dy;
	int32_t	minx, miny, maxx, maxy, px, py;
	int32_t	x, y;
	uint32_t shift, slot, color;

	area = edge_fn(v0->sx, v0->sy, v1->sx, v1->sy, v2->sx, v2->sy);
	if (area == 0) {
		return;
	}
	minx = v0->sx < v1->sx ? v0->sx : v1->sx;
	minx = minx < v2->sx ? minx : v2->sx;
	miny = v0->sy < v1->sy ? v0->sy : v1->sy;
	miny = miny < v2->sy ? miny : v2->sy;
	maxx = v0->sx > v1->sx ? v0->sx : v1->sx;
	maxx = maxx > v2->sx ? maxx : v2->sx;
	maxy = v0->sy > v1->sy ? v0->sy : v1->sy;
	maxy = maxy > v2->sy ? maxy : v2->sy;
	minx = clamp_i32(minx >> 16, 0, (int32_t)image->width - 1);
	miny = clamp_i32(miny >> 16, 0, (int32_t)image->height - 1);
	maxx = clamp_i32((maxx + SRAPI_FIXED_ONE - 1) >> 16, 0,
	    (int32_t)image->width - 1);
	maxy = clamp_i32((maxy + SRAPI_FIXED_ONE - 1) >> 16, 0,
	    (int32_t)image->height - 1);
	px = (minx << 16) + (SRAPI_FIXED_ONE / 2);
	py = (miny << 16) + (SRAPI_FIXED_ONE / 2);
	w0 = edge_fn(v1->sx, v1->sy, v2->sx, v2->sy, px, py);
	w1 = edge_fn(v2->sx, v2->sy, v0->sx, v0->sy, px, py);
	w2 = edge_fn(v0->sx, v0->sy, v1->sx, v1->sy, px, py);
	w0dx = (int64_t)(v2->sy - v1->sy) * SRAPI_FIXED_ONE;
	w1dx = (int64_t)(v0->sy - v2->sy) * SRAPI_FIXED_ONE;
	w2dx = (int64_t)(v1->sy - v0->sy) * SRAPI_FIXED_ONE;
	w0dy = -(int64_t)(v2->sx - v1->sx) * SRAPI_FIXED_ONE;
	w1dy = -(int64_t)(v0->sx - v2->sx) * SRAPI_FIXED_ONE;
	w2dy = -(int64_t)(v1->sx - v0->sx) * SRAPI_FIXED_ONE;
	if (area < 0) {
		area = -area;
		w0 = -w0;
		w1 = -w1;
		w2 = -w2;
		w0dx = -w0dx;
		w1dx = -w1dx;
		w2dx = -w2dx;
		w0dy = -w0dy;
		w1dy = -w1dy;
		w2dy = -w2dy;
	}
	shift = choose_shift(area);
	for (slot = 0; slot < SRAPI_VM_IO_SLOTS; slot++) {
		row_io[slot] = interp_slot(v0, v1, v2, slot, w0, w1, w2,
		    area, shift);
		dx_io[slot] = interp_delta_slot(v0, v1, v2, slot, w0dx,
		    w1dx, w2dx, area);
		dy_io[slot] = interp_delta_slot(v0, v1, v2, slot, w0dy,
		    w1dy, w2dy, area);
	}

	row_w0 = w0;
	row_w1 = w1;
	row_w2 = w2;
	for (y = miny; y <= maxy; y++) {
		memcpy(input, row_io, sizeof(input));
		w0 = row_w0;
		w1 = row_w1;
		w2 = row_w2;
		for (x = minx; x <= maxx; x++) {
			if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
				input[SRAPI_IO_PIXEL_X] = x << 16;
				input[SRAPI_IO_PIXEL_Y] = y << 16;
				color = shade_fragment(pipeline, input);
				store_pixel(image, (uint32_t)x, (uint32_t)y,
				    color);
			}
			w0 += w0dx;
			w1 += w1dx;
			w2 += w2dx;
			for (slot = 0; slot < SRAPI_VM_IO_SLOTS; slot++) {
				input[slot] += dx_io[slot];
			}
		}
		row_w0 += w0dy;
		row_w1 += w1dy;
		row_w2 += w2dy;
		for (slot = 0; slot < SRAPI_VM_IO_SLOTS; slot++) {
			row_io[slot] += dy_io[slot];
		}
	}
	srapi_image_mark_dirty(image, (uint32_t)minx, (uint32_t)miny,
	    (uint32_t)(maxx - minx + 1), (uint32_t)(maxy - miny + 1));
}

static void
clear_row_32(uint8_t *row, uint32_t width, uint32_t color)
{
	uint32_t	x;
	uint32_t	*dst;

	dst = (uint32_t *)(void *)row;
	for (x = 0; x < width; x++) {
		dst[x] = color;
	}
}

static void
clear_row_24(uint8_t *row, uint32_t width, uint32_t color)
{
	uint32_t	x;

	for (x = 0; x < width; x++) {
		row[x * 3] = (uint8_t)(color & 0xff);
		row[x * 3 + 1] = (uint8_t)((color >> 8) & 0xff);
		row[x * 3 + 2] = (uint8_t)((color >> 16) & 0xff);
	}
}

static void
clear_row_16(uint8_t *row, uint32_t width, uint32_t color)
{
	uint32_t	x, r, g, b;
	uint16_t	rgb565;
	uint16_t	*dst;

	r = (color >> 16) & 0xff;
	g = (color >> 8) & 0xff;
	b = color & 0xff;
	rgb565 = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
	dst = (uint16_t *)(void *)row;
	for (x = 0; x < width; x++) {
		dst[x] = rgb565;
	}
}

int
srapi_raster_clear_rect(srapi_image_t *image, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height, uint32_t color)
{
	uint8_t		*base, *first, *row;
	uint64_t	offset;
	uint32_t	bytes_pp, endx, endy, yy, row_bytes;

	if (!image || !image->pixels || width == 0 || height == 0) {
		return (SRAPI_ERR_INVALID);
	}
	if (x >= image->width || y >= image->height) {
		return (SRAPI_OK);
	}
	endx = x + width;
	endy = y + height;
	if (endx > image->width || endx < x) {
		endx = image->width;
	}
	if (endy > image->height || endy < y) {
		endy = image->height;
	}
	width = endx - x;
	height = endy - y;
	bytes_pp = image->bpp / 8;
	row_bytes = width * bytes_pp;
	offset = (uint64_t)y * image->pitch + (uint64_t)x * bytes_pp;
	if (offset + row_bytes > image->size) {
		return (SRAPI_ERR_RANGE);
	}
	base = (uint8_t *)image->pixels;
	first = base + offset;
	if (bytes_pp == 4) {
		clear_row_32(first, width, color);
	} else if (bytes_pp == 3) {
		clear_row_24(first, width, color);
	} else if (bytes_pp == 2) {
		clear_row_16(first, width, color);
	} else {
		return (SRAPI_ERR_UNSUPPORTED);
	}
	for (yy = 1; yy < height; yy++) {
		row = first + (uint64_t)yy * image->pitch;
		if ((uint64_t)(row - base) + row_bytes > image->size) {
			return (SRAPI_ERR_RANGE);
		}
		memcpy(row, first, row_bytes);
	}
	srapi_image_mark_dirty(image, x, y, width, height);
	return (SRAPI_OK);
}

int
srapi_raster_clear(srapi_image_t *image, uint32_t color)
{
	if (!image) {
		return (SRAPI_ERR_INVALID);
	}
	return (srapi_raster_clear_rect(image, 0, 0, image->width,
	    image->height, color));
}

int
srapi_raster_draw(srapi_device_t *device, srapi_image_t *image,
    srapi_pipeline_t *pipeline, const struct srapi_viewport *viewport,
    srapi_buffer_t *vertex_buffer, uint32_t first_vertex,
    uint32_t vertex_count)
{
	struct srapi_vertex_out	v[3];
	uint32_t		i;
	int			ret;

	(void)device;
	(void)viewport;
	if (!image || !pipeline || !vertex_buffer || !vertex_buffer->data) {
		return (SRAPI_ERR_INVALID);
	}
	if (vertex_count < 3) {
		return (SRAPI_OK);
	}
	for (i = 0; i + 2 < vertex_count; i += 3) {
		ret = fetch_vertex(image, pipeline, viewport, vertex_buffer,
		    first_vertex + i, &v[0]);
		if (ret != SRAPI_OK) {
			return (ret);
		}
		ret = fetch_vertex(image, pipeline, viewport, vertex_buffer,
		    first_vertex + i + 1, &v[1]);
		if (ret != SRAPI_OK) {
			return (ret);
		}
		ret = fetch_vertex(image, pipeline, viewport, vertex_buffer,
		    first_vertex + i + 2, &v[2]);
		if (ret != SRAPI_OK) {
			return (ret);
		}
		raster_triangle(image, pipeline, &v[0], &v[1], &v[2]);
	}
	return (SRAPI_OK);
}
