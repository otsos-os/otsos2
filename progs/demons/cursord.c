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

$define %func daemonize as function with args void
$define %func cursor_shape as function with args int, int
$define %func cursor_outline as function with args int, int
$define %func put_cursor_pixel as procedure with args uint32_t, uint32_t, uint32_t, uint32_t
$define %func draw_cursor as procedure with args uint32_t
$define %func commit_cursor as function with args uint32_t, uint32_t, int, int
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal daemonize, cursor_shape, cursor_outline
$space %internal put_cursor_pixel, draw_cursor, commit_cursor
$space %export main

*/

#include <native.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define	CURSOR_W		24
#define	CURSOR_H		32
#define	CURSOR_PITCH		(CURSOR_W * 4)
#define	CURSOR_EVENT_BATCH	32

static void
log_msg(const char *msg)
{
	if (msg) {
		termPrint(msg);
	}
}

static int
daemonize(void)
{
	int	pid;

	pid = procCopy();
	if (pid < 0) {
		log_msg("cursord: first fork failed\n");
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	if (procSetsid() < 0) {
		log_msg("cursord: setsid failed\n");
		return (-1);
	}

	pid = procCopy();
	if (pid < 0) {
		log_msg("cursord: second fork failed\n");
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	return (0);
}

static int
cursor_shape(int x, int y)
{
	if (x < 0 || y < 0) {
		return (0);
	}
	if (y < 19 && x <= y / 2) {
		return (1);
	}
	if (y >= 13 && y < 29 && x >= 6 && x <= 10) {
		return (1);
	}
	if (y >= 18 && y < 23 && x >= 9 && x <= 17) {
		return (1);
	}
	return (0);
}

static int
cursor_outline(int x, int y)
{
	if (!cursor_shape(x, y)) {
		return (0);
	}
	if (!cursor_shape(x - 1, y) || !cursor_shape(x + 1, y) ||
	    !cursor_shape(x, y - 1) || !cursor_shape(x, y + 1)) {
		return (1);
	}
	return (0);
}

static void
put_cursor_pixel(uint32_t gem, uint32_t x, uint32_t y, uint32_t color)
{
	(void)drmRapiPutPixel(gem, CURSOR_PITCH, 32, x, y, color);
}

static void
draw_cursor(uint32_t gem)
{
	uint32_t	x, y;

	(void)drmRapiClear(gem, CURSOR_PITCH, 32, 0x00000000);

	for (y = 0; y < CURSOR_H; y++) {
		for (x = 0; x < CURSOR_W; x++) {
			if (!cursor_shape((int)x, (int)y)) {
				continue;
			}
			if (cursor_outline((int)x, (int)y)) {
				put_cursor_pixel(gem, x, y, 0xFF000000);
			} else {
				put_cursor_pixel(gem, x, y, 0xFFFFFFFF);
			}
		}
	}
}

static int
commit_cursor(uint32_t plane, uint32_t fb, int x, int y)
{
	struct api_drm_atomic_req	reqs[9];

	memset(reqs, 0, sizeof(reqs));
	reqs[0].obj_id = plane;
	reqs[0].prop_id = DRM_PROP_PLANE_FB_ID;
	reqs[0].value = fb;
	reqs[1].obj_id = plane;
	reqs[1].prop_id = DRM_PROP_PLANE_CRTC_X;
	reqs[1].value = (uint64_t)x;
	reqs[2].obj_id = plane;
	reqs[2].prop_id = DRM_PROP_PLANE_CRTC_Y;
	reqs[2].value = (uint64_t)y;
	reqs[3].obj_id = plane;
	reqs[3].prop_id = DRM_PROP_PLANE_CRTC_W;
	reqs[3].value = CURSOR_W;
	reqs[4].obj_id = plane;
	reqs[4].prop_id = DRM_PROP_PLANE_CRTC_H;
	reqs[4].value = CURSOR_H;
	reqs[5].obj_id = plane;
	reqs[5].prop_id = DRM_PROP_PLANE_SRC_X;
	reqs[5].value = 0;
	reqs[6].obj_id = plane;
	reqs[6].prop_id = DRM_PROP_PLANE_SRC_Y;
	reqs[6].value = 0;
	reqs[7].obj_id = plane;
	reqs[7].prop_id = DRM_PROP_PLANE_SRC_W;
	reqs[7].value = CURSOR_W;
	reqs[8].obj_id = plane;
	reqs[8].prop_id = DRM_PROP_PLANE_SRC_H;
	reqs[8].value = CURSOR_H;

	return (drmAtomicCommit(reqs, 9, 0));
}

int
main(int argc, char **argv, char **envp)
{
	struct api_drm_objects	objects;
	struct api_drm_info	info;
	struct kevent		change;
	struct kevent		events[CURSOR_EVENT_BATCH];
	uint32_t		gem, fb;
	int			i, kq, n, x, y, dx, dy, max_x, max_y;

	(void)argc;
	(void)argv;
	(void)envp;

	(void)personality(API_PERSONALITY_NATIVE);
	if (daemonize() != 0) {
		return (1);
	}

	memset(&info, 0, sizeof(info));
	if (drmInfo(&info) != 0 || !info.available) {
		log_msg("cursord: drm not available\n");
		return (1);
	}

	memset(&objects, 0, sizeof(objects));
	if (drmGetObjects(&objects) != 0 || objects.cursor_plane_id == 0) {
		log_msg("cursord: cursor plane not available\n");
		return (1);
	}

	gem = 0;
	if (drmGemCreate(CURSOR_PITCH * CURSOR_H, &gem) != 0 || gem == 0) {
		log_msg("cursord: gem create failed\n");
		return (1);
	}

	fb = 0;
	if (drmFbCreate(gem, CURSOR_W, CURSOR_H, CURSOR_PITCH, 32, &fb) != 0 ||
	    fb == 0) {
		log_msg("cursord: fb create failed\n");
		return (1);
	}

	draw_cursor(gem);

	x = (int)(info.width / 2);
	y = (int)(info.height / 2);
	max_x = info.width > 0 ? (int)info.width - 1 : 0;
	max_y = info.height > 0 ? (int)info.height - 1 : 0;
	(void)commit_cursor(objects.cursor_plane_id, fb, x, y);

	kq = eventKqueue();
	if (kq < 0) {
		log_msg("cursord: kqueue failed\n");
		return (1);
	}

	memset(&change, 0, sizeof(change));
	change.ident = 0;
	change.filter = EVFILT_MOUSE;
	change.flags = EV_ADD | EV_CLEAR;
	if (eventWait(kq, &change, 1, NULL, 0, -1) < 0) {
		log_msg("cursord: mouse event attach failed\n");
		return (1);
	}

	log_msg("cursord: started\n");

	for (;;) {
		n = eventWait(kq, NULL, 0, events, CURSOR_EVENT_BATCH, -1);
		if (n <= 0) {
			continue;
		}
		dx = 0;
		dy = 0;
		for (i = 0; i < n; i++) {
			if (events[i].filter != EVFILT_MOUSE) {
				continue;
			}
			dx += (int)MOUSE_DATA_DX(events[i].data);
			dy += (int)MOUSE_DATA_DY(events[i].data);
		}
		if (dx == 0 && dy == 0) {
			continue;
		}
		x += dx;
		y += dy;
		if (x < 0) {
			x = 0;
		}
		if (y < 0) {
			y = 0;
		}
		if (x > max_x) {
			x = max_x;
		}
		if (y > max_y) {
			y = max_y;
		}
		(void)commit_cursor(objects.cursor_plane_id, fb, x, y);
	}

	return (0);
}
