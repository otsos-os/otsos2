/* !DEFINES!

$define %type libg_context as LibG immediate UI context, shared across LibG TUs
$define %func libg_mark_dirty as procedure with args context, x, y, w, h
$define %func libg_blend as function with args dst color, src color
$define %func libg_clamp_i32 as function with args value, min, max
$define %func libg_rect_contains as function with args rect, x, y

*/

/* !SPACE!

$space %internal libg_context, libg_mark_dirty, libg_blend
$space %internal libg_clamp_i32, libg_rect_contains

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


#ifndef LIBG_INT_H
#define LIBG_INT_H

#include <libg.h>
#include <stddef.h>
#include <stdint.h>

#define LIBG_TEXT_INPUT_MAX		32
#define LIBG_MOUSE_LEFT			SRAPI_MOUSE_LEFT

struct libg_context {
	srapi_device_t		*device;
	libg_present_fn		present;
	void			*present_userdata;
	srapi_surface_t		*surface;
	srapi_cmd_buffer_t	*cmd;
	libg_style_t		style;
	uint8_t			*pixels;
	uint32_t		width;
	uint32_t		height;
	uint32_t		pitch;
	int32_t			mouse_x;
	int32_t			mouse_y;
	int32_t			raw_mouse_x;
	int32_t			raw_mouse_y;
	uint32_t		mouse_buttons;
	int			mouse_pressed;
	int			mouse_released;
	int			have_raw_mouse;
	uint32_t		hot_id;
	uint32_t		active_id;
	uint32_t		focus_id;
	char			text_input[LIBG_TEXT_INPUT_MAX];
	uint32_t		text_count;
	uint32_t		backspace_count;
	int			submit_pressed;

	int32_t			drag_grab;
	int32_t			dirty_x1, dirty_y1, dirty_x2, dirty_y2;
	int			dirty_valid;
	int32_t			clip_x0, clip_y0, clip_x1, clip_y1;
	int			clip_valid;
};

#define LIBG_DRAG_NONE			(-1)

void		libg_mark_dirty(libg_context_t *ctx, int32_t x, int32_t y,
		    int32_t w, int32_t h);
uint32_t	libg_blend(uint32_t dst, uint32_t src);
int32_t		libg_clamp_i32(int32_t value, int32_t min, int32_t max);
int		libg_rect_contains(libg_rect_t rect, int32_t x, int32_t y);

#endif
