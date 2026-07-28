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

$define %type cursor_output as struct with cursor backend state
$define %func daemonize as function with args void
$define %func clamp_cursor as function with args int, int
$define %func attach_input as function with args int
$define %func arm_frame_timer as function with args int
$define %func cursor_shape as function with args int, int
$define %func cursor_outline as function with args int, int
$define %func put_cursor_pixel as procedure with args pixels, position, color
$define %func draw_cursor as procedure with args uint32_t *
$define %func commit_cursor as function with args uint32_t, uint32_t, int, int
$define %func send_terminal_mouse as function with args int, int, int, int, int
$define %func sync_cursor_output as function with args output state, drm, pos, buttons
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal daemonize, clamp_cursor, attach_input, arm_frame_timer
$space %internal cursor_shape, cursor_outline
$space %internal put_cursor_pixel, draw_cursor, commit_cursor
$space %internal send_terminal_mouse, sync_cursor_output
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
#define	CURSOR_INPUT_IDENT	0
#define	CURSOR_TIMER_IDENT	1
#define	CURSOR_FRAME_MS		16

struct cursor_output {
	int	drm_visible;
	int	drm_x;
	int	drm_y;
	int	term_mouse_visible;
	int	term_mouse_tty;
	int	term_mouse_x;
	int	term_mouse_y;
	int	term_mouse_buttons;
};

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
clamp_cursor(int value, int max)
{
	if (value < 0) {
		return (0);
	}
	if (value > max) {
		return (max);
	}
	return (value);
}

static int
attach_input(int kq)
{
	struct kevent	change;

	memset(&change, 0, sizeof(change));
	change.ident = CURSOR_INPUT_IDENT;
	change.filter = EVFILT_INPUT;
	change.flags = EV_ADD | EV_CLEAR;
	return (eventWait(kq, &change, 1, NULL, 0, -1));
}

static int
arm_frame_timer(int kq)
{
	struct kevent	change;

	memset(&change, 0, sizeof(change));
	change.ident = CURSOR_TIMER_IDENT;
	change.filter = EVFILT_TIMER;
	change.flags = EV_ADD | EV_ONESHOT;
	change.data = CURSOR_FRAME_MS;
	return (eventWait(kq, &change, 1, NULL, 0, -1));
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
put_cursor_pixel(uint32_t *pixels, uint32_t x, uint32_t y, uint32_t color)
{
	pixels[y * CURSOR_W + x] = color;
}

static void
draw_cursor(uint32_t *pixels)
{
	uint32_t	x, y;

	memset(pixels, 0, CURSOR_PITCH * CURSOR_H);

	for (y = 0; y < CURSOR_H; y++) {
		for (x = 0; x < CURSOR_W; x++) {
			if (!cursor_shape((int)x, (int)y)) {
				continue;
			}
			if (cursor_outline((int)x, (int)y)) {
				put_cursor_pixel(pixels, x, y, 0xFF000000);
			} else {
				put_cursor_pixel(pixels, x, y, 0xFFFFFFFF);
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

static int
send_terminal_mouse(int tty, int visible, int x, int y, int buttons)
{
	struct api_term_mouse	mouse;

	memset(&mouse, 0, sizeof(mouse));
	mouse.op = API_TERM_MOUSE_UPDATE;
	mouse.tty = tty;
	mouse.flags = visible ? API_TERM_MOUSE_VISIBLE : 0;
	mouse.x = x;
	mouse.y = y;
	mouse.buttons = buttons;
	return (termMouse(&mouse));
}

static int
sync_cursor_output(struct cursor_output *output, uint32_t plane, uint32_t fb,
    int x, int y, int buttons)
{
	struct api_term_info	term;
	int			ret, use_term;

	if (!output) {
		return (-1);
	}

	memset(&term, 0, sizeof(term));
	use_term = 0;
	if (termInfo(&term) == 0 && term.state == TERM_STATE_ACTIVE) {
		use_term = 1;
	}

	if (use_term) {
		if (output->term_mouse_visible &&
		    output->term_mouse_tty != term.tty) {
			(void)send_terminal_mouse(output->term_mouse_tty, 0,
			    output->term_mouse_x, output->term_mouse_y,
			    output->term_mouse_buttons);
			output->term_mouse_visible = 0;
		}

		if (!output->term_mouse_visible ||
		    output->term_mouse_x != x || output->term_mouse_y != y ||
		    output->term_mouse_buttons != buttons) {
			ret = send_terminal_mouse(term.tty, 1, x, y, buttons);
			if (ret != 0) {
				use_term = 0;
			} else {
				output->term_mouse_visible = 1;
				output->term_mouse_tty = term.tty;
				output->term_mouse_x = x;
				output->term_mouse_y = y;
				output->term_mouse_buttons = buttons;
			}
		}
	}

	if (use_term) {
		if (output->drm_visible != 0) {
			(void)commit_cursor(plane, 0, x, y);
			output->drm_visible = 0;
		}
		return (0);
	}

	if (output->term_mouse_visible) {
		(void)send_terminal_mouse(output->term_mouse_tty, 0,
		    output->term_mouse_x, output->term_mouse_y,
		    output->term_mouse_buttons);
		output->term_mouse_visible = 0;
	}

	if (output->drm_visible != 1 || output->drm_x != x ||
	    output->drm_y != y) {
		(void)commit_cursor(plane, fb, x, y);
		output->drm_visible = 1;
		output->drm_x = x;
		output->drm_y = y;
	}

	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	struct api_drm_objects	objects;
	struct api_drm_info	info;
	struct kevent		events[CURSOR_EVENT_BATCH];
	struct cursor_output	output;
	struct api_input_event	*input;
	void			*cursor_pixels;
	uint32_t		gem, fb;
	int			i, kq, n, x, y, target_x, target_y, dx, dy;
	int			max_x, max_y, raw_x, raw_y, have_raw;
	int			buttons, button_changed;
	int			pending, timer_armed, timer_ready;

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

	cursor_pixels = drmGemMmap(gem, CURSOR_PITCH * CURSOR_H,
	    API_MAP_READ | API_MAP_WRITE);
	if (!cursor_pixels) {
		log_msg("cursord: cursor map failed\n");
		return (1);
	}
	draw_cursor((uint32_t *)cursor_pixels);

	x = (int)(info.width / 2);
	y = (int)(info.height / 2);
	target_x = x;
	target_y = y;
	max_x = info.width > 0 ? (int)info.width - 1 : 0;
	max_y = info.height > 0 ? (int)info.height - 1 : 0;
	raw_x = 0;
	raw_y = 0;
	have_raw = 0;
	buttons = 0;
	pending = 0;
	timer_armed = 0;
	memset(&output, 0, sizeof(output));
	output.drm_visible = -1;
	output.drm_x = -1;
	output.drm_y = -1;
	output.term_mouse_tty = API_TERM_ACTIVE;

	kq = eventKqueue();
	if (kq < 0) {
		log_msg("cursord: kqueue failed\n");
		return (1);
	}

	if (attach_input(kq) < 0) {
		log_msg("cursord: input event attach failed\n");
		return (1);
	}

	(void)sync_cursor_output(&output, objects.cursor_plane_id, fb, x, y,
	    buttons);
	if (arm_frame_timer(kq) == 0) {
		timer_armed = 1;
	}

	log_msg("cursord: started\n");

	for (;;) {
		n = eventWait(kq, NULL, 0, events, CURSOR_EVENT_BATCH, -1);
		if (n <= 0) {
			continue;
		}
		timer_ready = 0;
		for (i = 0; i < n; i++) {
			if (events[i].filter == EVFILT_TIMER &&
			    events[i].ident == CURSOR_TIMER_IDENT) {
				timer_ready = 1;
				timer_armed = 0;
				continue;
			}
			if (events[i].filter != EVFILT_INPUT) {
				continue;
			}
			input = &events[i].input;
			if (input->type != API_INPUT_TYPE_MOUSE) {
				continue;
			}
			dx = 0;
			dy = 0;
			if (have_raw) {
				dx += input->x - raw_x;
				dy += input->y - raw_y;
			} else {
				dx += input->dx;
				dy += input->dy;
				have_raw = 1;
			}
			raw_x = input->x;
			raw_y = input->y;
			button_changed = buttons != (int)input->buttons;
			buttons = (int)input->buttons;
			if (dx == 0 && dy == 0 && !button_changed) {
				continue;
			}
			if (dx != 0 || dy != 0) {
				target_x = clamp_cursor(target_x + dx, max_x);
				target_y = clamp_cursor(target_y + dy, max_y);
			}
			pending = 1;
		}
		if (timer_ready) {
			if (pending && (target_x != x || target_y != y)) {
				x = target_x;
				y = target_y;
			}
			(void)sync_cursor_output(&output,
			    objects.cursor_plane_id, fb, x, y, buttons);
			pending = 0;
		}
		if (!timer_armed && arm_frame_timer(kq) == 0) {
			timer_armed = 1;
		}
	}

	return (0);
}
