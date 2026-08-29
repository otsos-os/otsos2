/*
 * Copyright (c) 2026, otsos team
 *
 * SWM / Sprot platform backend for DoomGeneric (Native otsos ABI)
 */

#include "doomgeneric/doomgeneric/doomkeys.h"
#include "doomgeneric/doomgeneric/doomgeneric.h"

#include <native.h>
#include <sprot/client.h>
#include <sprot/sprot.h>

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_QUEUE_SIZE 64
#define DOOM_WINDOW_W 640
#define DOOM_WINDOW_H 480
#define DOOM_ASPECT_NUM 4
#define DOOM_ASPECT_DEN 3
#define DOOM_FIXED_SHIFT 16

static sprot_connection_t	*s_conn;
static sprot_surface_t		*s_surface;
static uint32_t			 s_surface_w;
static uint32_t			 s_surface_h;
static uint32_t			 s_img_x;
static uint32_t			 s_img_y;
static uint32_t			 s_img_w;
static uint32_t			 s_img_h;
static int			 s_geometry_dirty;

static uint16_t			 s_key_queue[KEY_QUEUE_SIZE];
static uint32_t			 s_key_queue_write;
static uint32_t			 s_key_queue_read;

static unsigned char
convert_scancode_to_doom(uint32_t scancode)
{
	if (scancode >= 0x04 && scancode <= 0x1d) {
		return (unsigned char)('a' + (scancode - 0x04));
	}
	if (scancode >= 0x1e && scancode <= 0x26) {
		return (unsigned char)('1' + (scancode - 0x1e));
	}
	if (scancode == 0x27) {
		return ('0');
	}

	switch (scancode) {
	case 0x28: /* Enter */
	case 0x58: /* KP Enter */
		return (KEY_ENTER);
	case 0x29: /* Escape */
		return (KEY_ESCAPE);
	case 0x2a: /* Backspace */
		return (KEY_BACKSPACE);
	case 0x2b: /* Tab */
		return (KEY_TAB);
	case 0x2c: /* Space */
		return (KEY_USE);
	case 0x2d: /* Minus */
	case 0x56: /* KP Minus */
		return (KEY_MINUS);
	case 0x2e: /* Equals */
	case 0x57: /* KP Plus */
		return (KEY_EQUALS);
	case 0x3a: /* F1 */
		return (KEY_F1);
	case 0x3b: /* F2 */
		return (KEY_F2);
	case 0x3c: /* F3 */
		return (KEY_F3);
	case 0x3d: /* F4 */
		return (KEY_F4);
	case 0x3e: /* F5 */
		return (KEY_F5);
	case 0x3f: /* F6 */
		return (KEY_F6);
	case 0x40: /* F7 */
		return (KEY_F7);
	case 0x41: /* F8 */
		return (KEY_F8);
	case 0x42: /* F9 */
		return (KEY_F9);
	case 0x43: /* F10 */
		return (KEY_F10);
	case 0x44: /* F11 */
		return (KEY_F11);
	case 0x45: /* F12 */
		return (KEY_F12);
	case 0x4f: /* Right */
	case 0x5e: /* KP 6 */
		return (KEY_RIGHTARROW);
	case 0x50: /* Left */
	case 0x5c: /* KP 4 */
		return (KEY_LEFTARROW);
	case 0x51: /* Down */
	case 0x5a: /* KP 2 */
		return (KEY_DOWNARROW);
	case 0x52: /* Up */
	case 0x60: /* KP 8 */
		return (KEY_UPARROW);
	case 0xe0: /* LCtrl */
	case 0xe4: /* RCtrl */
		return (KEY_FIRE);
	case 0xe1: /* LShift */
	case 0xe5: /* RShift */
		return (KEY_RSHIFT);
	case 0xe2: /* LAlt */
	case 0xe6: /* RAlt */
		return (KEY_LALT);
	default:
		return (0);
	}
}

static void
queue_key_event(int pressed, unsigned char doom_key)
{
	uint32_t next;

	if (doom_key == 0) {
		return;
	}
	next = (s_key_queue_write + 1) % KEY_QUEUE_SIZE;
	if (next == s_key_queue_read) {
		return;
	}
	s_key_queue[s_key_queue_write] = ((uint16_t)(pressed ? 1 : 0) << 8) | doom_key;
	s_key_queue_write = next;
}

static void
compute_fit(uint32_t surface_w, uint32_t surface_h)
{
	uint32_t	width, height;

	if (surface_w == 0 || surface_h == 0) {
		s_img_x = 0;
		s_img_y = 0;
		s_img_w = 0;
		s_img_h = 0;
		return;
	}

	width = surface_w;
	height = (uint32_t)(((uint64_t)width * DOOM_ASPECT_DEN) /
	    DOOM_ASPECT_NUM);
	if (height > surface_h) {
		height = surface_h;
		width = (uint32_t)(((uint64_t)height * DOOM_ASPECT_NUM) /
		    DOOM_ASPECT_DEN);
	}

	if (width == 0) {
		width = 1;
	}
	if (height == 0) {
		height = 1;
	}
	if (width > surface_w) {
		width = surface_w;
	}
	if (height > surface_h) {
		height = surface_h;
	}

	s_img_w = width;
	s_img_h = height;
	s_img_x = (surface_w - width) / 2;
	s_img_y = (surface_h - height) / 2;
}

static void
clear_surface(void)
{
	uint32_t	*pixels;
	size_t		 bytes;

	if (s_surface == NULL) {
		return;
	}
	pixels = sprot_surface_pixels(s_surface);
	if (pixels == NULL) {
		return;
	}
	bytes = (size_t)sprot_surface_stride(s_surface) * (size_t)s_surface_h;
	memset(pixels, 0, bytes);
}

static void
scale_row(uint32_t *dst, const uint32_t *src, uint32_t x_step)
{
	uint32_t	x_acc, sx;
	uint32_t	x;

	if (s_img_w == (uint32_t)DOOMGENERIC_RESX) {
		memcpy(dst, src,
		    (size_t)DOOMGENERIC_RESX * sizeof(uint32_t));
		return;
	}

	x_acc = 0;
	for (x = 0; x < s_img_w; x++) {
		sx = x_acc >> DOOM_FIXED_SHIFT;
		x_acc += x_step;
		if (sx >= (uint32_t)DOOMGENERIC_RESX) {
			sx = (uint32_t)DOOMGENERIC_RESX - 1;
		}
		dst[x] = src[sx];
	}
}

static void
blit_scaled(uint32_t *dst, uint32_t dst_stride_px)
{
	const uint32_t	*src;
	uint32_t	*row_dst, *prev_dst;
	uint32_t	 x_step, y_step, y_acc, sy, prev_sy;
	uint32_t	 y;

	src = (const uint32_t *)DG_ScreenBuffer;
	x_step = ((uint32_t)DOOMGENERIC_RESX << DOOM_FIXED_SHIFT) / s_img_w;
	y_step = ((uint32_t)DOOMGENERIC_RESY << DOOM_FIXED_SHIFT) / s_img_h;

	prev_sy = (uint32_t)-1;
	prev_dst = NULL;
	y_acc = 0;

	for (y = 0; y < s_img_h; y++) {
		sy = y_acc >> DOOM_FIXED_SHIFT;
		y_acc += y_step;
		if (sy >= (uint32_t)DOOMGENERIC_RESY) {
			sy = (uint32_t)DOOMGENERIC_RESY - 1;
		}

		row_dst = dst + (size_t)(s_img_y + y) * (size_t)dst_stride_px +
		    (size_t)s_img_x;

		if (sy == prev_sy && prev_dst != NULL) {
			memcpy(row_dst, prev_dst,
			    (size_t)s_img_w * sizeof(uint32_t));
			continue;
		}

		scale_row(row_dst,
		    src + (size_t)sy * (size_t)DOOMGENERIC_RESX, x_step);
		prev_sy = sy;
		prev_dst = row_dst;
	}
}

static int
resize_surface(uint32_t width, uint32_t height)
{
	if (s_surface == NULL || width == 0 || height == 0) {
		return (-1);
	}
	if (width == s_surface_w && height == s_surface_h) {
		return (0);
	}
	if (sprot_resize_surface(s_surface, width, height) != 0) {
		return (-1);
	}

	s_surface_w = sprot_surface_width(s_surface);
	s_surface_h = sprot_surface_height(s_surface);
	compute_fit(s_surface_w, s_surface_h);
	clear_surface();
	s_geometry_dirty = 1;
	return (0);
}

static void
poll_sprot_events(int timeout_ms)
{
	sprot_event_t event;
	unsigned char doom_key;

	if (s_conn == NULL) {
		return;
	}

	while (sprot_poll_event(s_conn, &event, timeout_ms) > 0) {
		timeout_ms = 0;
		if (event.kind == SPROT_EVENT_KEY) {
			doom_key = convert_scancode_to_doom(event.u.key.scancode);
			if (doom_key != 0) {
				queue_key_event(event.u.key.state != 0, doom_key);
			}
		} else if (event.kind == SPROT_EVENT_SURFACE_CONFIGURE) {
			(void)resize_surface(event.u.configure.width,
			    event.u.configure.height);
		} else if (event.kind == SPROT_EVENT_SURFACE_CLOSE) {
			exit(0);
		} else if (event.kind == SPROT_EVENT_DISCONNECT) {
			exit(0);
		}
	}
}

static void
cleanup_sprot(void)
{
	if (s_surface != NULL) {
		sprot_destroy_surface(s_surface);
		s_surface = NULL;
	}
	if (s_conn != NULL) {
		sprot_disconnect(s_conn);
		s_conn = NULL;
	}
}

static int
wait_surface_ready(sprot_connection_t *conn, sprot_surface_t *surface)
{
	sprot_event_t event;
	int i, ret;

	for (i = 0; i < 100; i++) {
		if (sprot_surface_id(surface) != 0) {
			return (0);
		}
		ret = sprot_poll_event(conn, &event, 20);
		if (ret < 0) {
			return (-1);
		}
		if (ret == 0) {
			continue;
		}
		if (event.kind == SPROT_EVENT_SURFACE_CREATED &&
		    sprot_surface_id(surface) != 0) {
			return (0);
		}
		if (event.kind == SPROT_EVENT_DISCONNECT) {
			return (-1);
		}
	}
	return (sprot_surface_id(surface) != 0 ? 0 : -1);
}

void
DG_Init(void)
{
	s_key_queue_write = 0;
	s_key_queue_read = 0;

	s_conn = sprot_connect(SPROT_DEFAULT_SERVICE);
	if (s_conn == NULL) {
		termPrint("doom: cannot connect to SWM / Sprot service\n");
		procExit(1);
	}

	atexit(cleanup_sprot);

	s_surface = sprot_create_surface(s_conn, DOOM_WINDOW_W, DOOM_WINDOW_H);
	if (s_surface == NULL || wait_surface_ready(s_conn, s_surface) != 0) {
		termPrint("doom: cannot create window surface\n");
		procExit(1);
	}

	s_surface_w = sprot_surface_width(s_surface);
	s_surface_h = sprot_surface_height(s_surface);
	compute_fit(s_surface_w, s_surface_h);
	clear_surface();
	s_geometry_dirty = 1;

	if (sprot_set_role(s_surface, SPROT_SURFACE_ROLE_TOPLEVEL, 0, 80, 50) != 0 ||
	    sprot_set_title(s_surface, "DOOM") != 0 ||
	    sprot_set_visible(s_surface, 1) != 0) {
		termPrint("doom: failed to configure window surface\n");
		procExit(1);
	}
}

void
DG_DrawFrame(void)
{
	uint32_t *dst;
	uint32_t stride_px;

	if (s_surface == NULL || DG_ScreenBuffer == NULL) {
		return;
	}
	if (s_img_w == 0 || s_img_h == 0) {
		return;
	}

	dst = sprot_surface_pixels(s_surface);
	if (dst == NULL) {
		return;
	}

	stride_px = sprot_surface_stride(s_surface) / sizeof(uint32_t);
	if (stride_px == 0 || s_img_x + s_img_w > stride_px ||
	    s_img_y + s_img_h > s_surface_h) {
		return;
	}

	blit_scaled(dst, stride_px);

	if (s_geometry_dirty) {
		(void)sprot_damage(s_surface, 0, 0, s_surface_w, s_surface_h);
		s_geometry_dirty = 0;
	} else {
		(void)sprot_damage(s_surface, (int32_t)s_img_x,
		    (int32_t)s_img_y, s_img_w, s_img_h);
	}
	(void)sprot_commit(s_surface);

	poll_sprot_events(0);
}

void
DG_SleepMs(uint32_t ms)
{
	poll_sprot_events((int)ms);
}

uint32_t
DG_GetTicksMs(void)
{
	struct api_timeinfo ti;

	if (sysTimeInfo(&ti) != 0) {
		return (0);
	}
	return ((uint32_t)(ti.uptime_sec * 1000 + ti.uptime_nsec / 1000000));
}

int
DG_GetKey(int *pressed, unsigned char *key)
{
	uint16_t entry;

	poll_sprot_events(0);

	if (s_key_queue_read == s_key_queue_write) {
		return (0);
	}

	entry = s_key_queue[s_key_queue_read];
	s_key_queue_read = (s_key_queue_read + 1) % KEY_QUEUE_SIZE;

	if (pressed != NULL) {
		*pressed = (entry >> 8) & 1;
	}
	if (key != NULL) {
		*key = (unsigned char)(entry & 0xff);
	}
	return (1);
}

void
DG_SetWindowTitle(const char *title)
{
	if (s_surface != NULL && title != NULL) {
		(void)sprot_set_title(s_surface, title);
	}
}

int
main(int argc, char **argv)
{
	doomgeneric_Create(argc, argv);

	while (1) {
		doomgeneric_Tick();
	}

	return (0);
}
