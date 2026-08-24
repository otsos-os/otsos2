/* !DEFINES!

$define %type libg_scroll_metrics as resolved scrollbar geometry along one axis
$define %func libg_scroll_metrics_calc as function with args out, rect, axis, visible, total, value
$define %func libg_scroll_value_at as function with args metrics, position
$define %func libg_scroll_arrow as function with args context, id, rect, axis, up
$define %func libgScrollbar as function with args context, id, rect, axis, visible, total, step, value
$define %func libgScrollbarThickness as function with args void

*/

/* !SPACE!

$space %internal libg_scroll_metrics_t, libg_scroll_metrics_calc
$space %internal libg_scroll_value_at, libg_scroll_arrow
$space %export libgScrollbar, libgScrollbarThickness

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
#include <stddef.h>
#include <stdint.h>

#include "libg_int.h"

#define LIBG_SCROLL_THICK		16
#define LIBG_SCROLL_ARROW		16
#define LIBG_SCROLL_THUMB_MIN		18
#define LIBG_SCROLL_ARROWS_MIN		64
#define LIBG_SCROLL_ID_UP(id)		((id) ^ 0x5C000001U)
#define LIBG_SCROLL_ID_DOWN(id)		((id) ^ 0x5C000002U)

typedef struct libg_scroll_metrics {
	int32_t	track_pos;
	int32_t	track_len;
	int32_t	thumb_pos;
	int32_t	thumb_len;
	int32_t	span;
	int32_t	arrow_len;
} libg_scroll_metrics_t;


static int
libg_scroll_metrics_calc(libg_scroll_metrics_t *out, libg_rect_t rect, int axis,
    int32_t visible, int32_t total, int32_t value)
{
	int64_t	travel;
	int32_t	bar_len;

	out->track_pos = 0;
	out->track_len = 0;
	out->thumb_pos = 0;
	out->thumb_len = 0;
	out->span = 0;
	out->arrow_len = 0;

	bar_len = (axis == LIBG_SCROLL_HORIZONTAL) ? rect.width : rect.height;
	if (bar_len <= 0) {
		return (0);
	}

	out->arrow_len = (bar_len >= LIBG_SCROLL_ARROWS_MIN) ?
	    LIBG_SCROLL_ARROW : 0;
	out->track_pos = ((axis == LIBG_SCROLL_HORIZONTAL) ? rect.x : rect.y) +
	    out->arrow_len;
	out->track_len = bar_len - 2 * out->arrow_len;
	if (out->track_len < 0) {
		out->track_len = 0;
	}

	if (visible <= 0 || total <= 0 || total <= visible) {
		return (0);
	}
	out->span = total - visible;

	out->thumb_len = (int32_t)(((int64_t)out->track_len * visible) / total);
	if (out->thumb_len < LIBG_SCROLL_THUMB_MIN) {
		out->thumb_len = LIBG_SCROLL_THUMB_MIN;
	}
	if (out->thumb_len > out->track_len) {
		out->thumb_len = out->track_len;
	}

	travel = (int64_t)out->track_len - out->thumb_len;
	if (travel <= 0) {
		out->thumb_pos = out->track_pos;
		return (0);
	}

	value = libg_clamp_i32(value, 0, out->span);
	out->thumb_pos = out->track_pos +
	    (int32_t)(((int64_t)value * travel) / out->span);
	return (1);
}


static int32_t
libg_scroll_value_at(const libg_scroll_metrics_t *m, int32_t pos)
{
	int64_t	travel, value;

	travel = (int64_t)m->track_len - m->thumb_len;
	if (travel <= 0) {
		return (0);
	}
	value = (((int64_t)(pos - m->track_pos) * m->span) + travel / 2) /
	    travel;
	if (value < 0) {
		value = 0;
	}
	if (value > m->span) {
		value = m->span;
	}
	return ((int32_t)value);
}


static uint32_t
libg_scroll_arrow(libg_context_t *ctx, uint32_t id, libg_rect_t rect, int axis,
    int up)
{
	libg_rect_t	run;
	uint32_t	fill, state;
	int32_t		cx, cy, half, i;
	int		inside;

	state = LIBG_WIDGET_NONE;
	inside = libg_rect_contains(rect, ctx->mouse_x, ctx->mouse_y);
	if (inside) {
		ctx->hot_id = id;
		state |= LIBG_WIDGET_HOT;
	}
	if (inside && ctx->mouse_pressed) {
		ctx->active_id = id;
		state |= LIBG_WIDGET_CLICKED;
	}

	fill = ctx->style.control;
	if (ctx->active_id == id) {
		fill = ctx->style.control_active;
	} else if (inside) {
		fill = ctx->style.control_hot;
	}
	libgFillRect(ctx, rect, fill);
	libgStrokeRect(ctx, rect, ctx->style.panel_border);

	half = 4;
	cx = rect.x + rect.width / 2;
	cy = rect.y + rect.height / 2;
	for (i = 0; i < half; i++) {
		if (axis == LIBG_SCROLL_HORIZONTAL) {
			run.x = up ? cx - half / 2 + i : cx + half / 2 - i;
			run.y = cy - i;
			run.width = 1;
			run.height = 2 * i + 1;
		} else {
			run.x = cx - i;
			run.y = up ? cy - half / 2 + i : cy + half / 2 - i;
			run.width = 2 * i + 1;
			run.height = 1;
		}
		libgFillRect(ctx, run, ctx->style.text);
	}
	return (state);
}

int32_t
libgScrollbarThickness(void)
{
	return (LIBG_SCROLL_THICK);
}

uint32_t
libgScrollbar(libg_context_t *ctx, uint32_t id, libg_rect_t rect, int axis,
    int32_t visible, int32_t total, int32_t step, int32_t *value)
{
	libg_scroll_metrics_t	m;
	libg_rect_t		part;
	uint32_t		state, fill;
	int32_t			mouse, old, want;
	int			scrollable, held, took;

	if (ctx == NULL || value == NULL || rect.width <= 0 ||
	    rect.height <= 0) {
		return (LIBG_WIDGET_NONE);
	}
	if (axis != LIBG_SCROLL_HORIZONTAL) {
		axis = LIBG_SCROLL_VERTICAL;
	}

	state = LIBG_WIDGET_NONE;
	old = *value;
	took = 0;
	scrollable = libg_scroll_metrics_calc(&m, rect, axis, visible, total,
	    *value);
	*value = libg_clamp_i32(*value, 0, m.span);
	mouse = (axis == LIBG_SCROLL_HORIZONTAL) ? ctx->mouse_x : ctx->mouse_y;
	held = (ctx->mouse_buttons & LIBG_MOUSE_LEFT) != 0;

	libgFillRect(ctx, rect, ctx->style.panel);
	if (axis == LIBG_SCROLL_HORIZONTAL) {
		libgLine(ctx, rect.x, rect.y, rect.x + rect.width, rect.y,
		    ctx->style.panel_border);
	} else {
		libgLine(ctx, rect.x, rect.y, rect.x, rect.y + rect.height,
		    ctx->style.panel_border);
	}

	if (m.arrow_len > 0) {
		part = rect;
		if (axis == LIBG_SCROLL_HORIZONTAL) {
			part.width = m.arrow_len;
		} else {
			part.height = m.arrow_len;
		}
		if (libg_scroll_arrow(ctx, LIBG_SCROLL_ID_UP(id), part, axis,
		    1) & LIBG_WIDGET_CLICKED) {
			*value = libg_clamp_i32(*value - step, 0, m.span);
			ctx->drag_grab = LIBG_DRAG_NONE;
		}

		part = rect;
		if (axis == LIBG_SCROLL_HORIZONTAL) {
			part.x = rect.x + rect.width - m.arrow_len;
			part.width = m.arrow_len;
		} else {
			part.y = rect.y + rect.height - m.arrow_len;
			part.height = m.arrow_len;
		}
		if (libg_scroll_arrow(ctx, LIBG_SCROLL_ID_DOWN(id), part, axis,
		    0) & LIBG_WIDGET_CLICKED) {
			*value = libg_clamp_i32(*value + step, 0, m.span);
			ctx->drag_grab = LIBG_DRAG_NONE;
		}
	}

	if (scrollable) {
		part = rect;
		if (axis == LIBG_SCROLL_HORIZONTAL) {
			part.x = m.thumb_pos;
			part.width = m.thumb_len;
			part.y = rect.y + 1;
			part.height = rect.height - 2;
		} else {
			part.y = m.thumb_pos;
			part.height = m.thumb_len;
			part.x = rect.x + 1;
			part.width = rect.width - 2;
		}

		if (libg_rect_contains(part, ctx->mouse_x, ctx->mouse_y)) {
			ctx->hot_id = id;
			state |= LIBG_WIDGET_HOT;
			if (ctx->mouse_pressed) {
				ctx->active_id = id;
				ctx->drag_grab = mouse - m.thumb_pos;
				took = 1;
			}
		} else if (ctx->mouse_pressed && mouse >= m.track_pos &&
		    mouse < m.track_pos + m.track_len &&
		    libg_rect_contains(rect, ctx->mouse_x, ctx->mouse_y)) {
			ctx->active_id = id;
			ctx->drag_grab = m.thumb_len / 2;
			*value = libg_scroll_value_at(&m,
			    mouse - ctx->drag_grab);
			took = 1;
		}

		if (took == 0 && ctx->active_id == id && held &&
		    ctx->drag_grab != LIBG_DRAG_NONE) {
			want = libg_scroll_value_at(&m, mouse - ctx->drag_grab);
			*value = libg_clamp_i32(want, 0, m.span);
		}

		libg_scroll_metrics_calc(&m, rect, axis, visible, total,
		    *value);
		if (axis == LIBG_SCROLL_HORIZONTAL) {
			part.x = m.thumb_pos;
			part.width = m.thumb_len;
		} else {
			part.y = m.thumb_pos;
			part.height = m.thumb_len;
		}

		fill = ctx->style.control;
		if (ctx->active_id == id) {
			fill = ctx->style.control_active;
		} else if (ctx->hot_id == id) {
			fill = ctx->style.control_hot;
		}
		libgFillRect(ctx, part, fill);
		libgStrokeRect(ctx, part, ctx->style.panel_border);
	}

	if (ctx->active_id == id ||
	    ctx->active_id == LIBG_SCROLL_ID_UP(id) ||
	    ctx->active_id == LIBG_SCROLL_ID_DOWN(id)) {
		if (ctx->mouse_released || !held) {
			ctx->active_id = 0;
			ctx->drag_grab = LIBG_DRAG_NONE;
		} else {
			state |= LIBG_WIDGET_ACTIVE;
		}
	}
	if (*value != old) {
		state |= LIBG_WIDGET_CHANGED;
	}
	return (state);
}
