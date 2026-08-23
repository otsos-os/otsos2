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

/* !DEFINES!

$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type api_timeinfo as native clock snapshot
$define %type srapi_region as rectangular pixel region
$define %type sprot_event as one compositor event
$define %type rg_state as regedit gui global state

$define %func rg_present_cb as target present callback
$define %func rg_wait_surface as function with args rg_state *
$define %func rg_bind_libg as function with args rg_state *
$define %func rg_now_ms as function with args void
$define %func rg_window_open as function with args rg_state *, const char *
$define %func rg_window_resize as function with args rg_state *, uint32_t, uint32_t
$define %func rg_window_present as function with args rg_state *
$define %func rg_window_close as procedure with args rg_state *

*/

/* !SPACE!

$space %internal rg_present_cb, rg_wait_surface, rg_bind_libg
$space %export rg_now_ms, rg_window_open, rg_window_resize
$space %export rg_window_present, rg_window_close

*/

#include <native.h>
#include <regedit/frontend.h>
#include <stdint.h>
#include <string.h>
#include "gui.h"

#define RG_SURFACE_TRIES	100
#define RG_SURFACE_WAIT_MS	20

static int
rg_present_cb(void *userdata, const struct srapi_region *region)
{
	sprot_surface_t	*surface;

	surface = (sprot_surface_t *)userdata;
	if (surface == NULL) {
		return (-1);
	}
	if (region != NULL) {
		if (sprot_damage(surface, (int32_t)region->x,
		    (int32_t)region->y, region->width, region->height) != 0) {
			return (-1);
		}
	} else if (sprot_damage(surface, 0, 0, sprot_surface_width(surface),
	    sprot_surface_height(surface)) != 0) {
		return (-1);
	}
	return (sprot_commit(surface));
}

static int
rg_wait_surface(rg_state_t *st)
{
	sprot_event_t	event;
	int		i, ret;

	for (i = 0; i < RG_SURFACE_TRIES; i++) {
		if (sprot_surface_id(st->surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(st->conn, &event, RG_SURFACE_WAIT_MS);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (sprot_surface_id(st->surface) != 0 ? 0 : -1);
}

static int
rg_bind_libg(rg_state_t *st)
{
	int	ret;

	if (st->ui != NULL) {
		libgDestroy(st->ui);
		st->ui = NULL;
	}
	ret = libgCreateForTarget(sprot_surface_pixels(st->surface),
	    sprot_surface_width(st->surface),
	    sprot_surface_height(st->surface),
	    sprot_surface_stride(st->surface), rg_present_cb, st->surface,
	    &st->style, &st->ui);
	if (ret != LIBG_OK || st->ui == NULL) {
		st->ui = NULL;
		return (-1);
	}
	st->width = sprot_surface_width(st->surface);
	st->height = sprot_surface_height(st->surface);
	return (0);
}

uint64_t
rg_now_ms(void)
{
	struct api_timeinfo	ti;

	memset(&ti, 0, sizeof(ti));
	if (sysTimeInfo(&ti) != 0) {
		return (0);
	}
	return (ti.uptime_sec * 1000 + ti.uptime_nsec / 1000000);
}

int
rg_window_open(rg_state_t *st, const char *title)
{
	uint32_t	w, h, dw, dh;

	libgDefaultStyle(&st->style);
	st->conn = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (st->conn == NULL) {
		return (RG_UNAVAILABLE);
	}

	w = RG_DEFAULT_W;
	h = RG_DEFAULT_H;
	dw = sprot_display_width(st->conn);
	dh = sprot_display_height(st->conn);
	if (dw != 0 && w > dw - dw / 8) {
		w = dw - dw / 8;
	}
	if (dh != 0 && h > dh - dh / 6) {
		h = dh - dh / 6;
	}
	if (w < RG_MIN_W) {
		w = RG_MIN_W;
	}
	if (h < RG_MIN_H) {
		h = RG_MIN_H;
	}

	st->surface = sprot_create_surface(st->conn, w, h);
	if (st->surface == NULL || rg_wait_surface(st) != 0) {
		return (RG_FAILED);
	}
	if (sprot_set_role(st->surface, SPROT_SURFACE_ROLE_TOPLEVEL, 0,
	    48, 48) != 0 ||
	    sprot_set_title(st->surface, title != NULL ? title :
	    RG_TITLE) != 0 ||
	    sprot_set_visible(st->surface, 1) != 0) {
		return (RG_FAILED);
	}
	if (rg_bind_libg(st) != 0) {
		return (RG_FAILED);
	}
	st->focused = 1;
	st->dirty = 1;
	return (RG_OK);
}

int
rg_window_resize(rg_state_t *st, uint32_t width, uint32_t height)
{
	if (width < RG_MIN_W) {
		width = RG_MIN_W;
	}
	if (height < RG_MIN_H) {
		height = RG_MIN_H;
	}
	if (width == st->width && height == st->height) {
		return (0);
	}
	if (sprot_resize_surface(st->surface, width, height) != 0) {
		return (-1);
	}
	if (rg_bind_libg(st) != 0) {
		st->running = 0;
		return (-1);
	}
	st->dirty = 1;
	return (0);
}

int
rg_window_present(rg_state_t *st)
{
	if (st->ui == NULL) {
		return (-1);
	}
	return (libgPresent(st->ui) == LIBG_OK ? 0 : -1);
}

void
rg_window_close(rg_state_t *st)
{
	if (st->ui != NULL) {
		libgDestroy(st->ui);
		st->ui = NULL;
	}
	if (st->surface != NULL) {
		sprot_destroy_surface(st->surface);
		st->surface = NULL;
	}
	if (st->conn != NULL) {
		sprot_disconnect(st->conn);
		st->conn = NULL;
	}
}
