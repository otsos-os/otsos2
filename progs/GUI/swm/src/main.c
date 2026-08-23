/* !DEFINES!

$define %type swm_frame_delivery as frame callback counters
$define %type swm_terminal_guard as saved terminal state
$define %func swm_drain_protocol as function with args swm_state *
$define %func swm_poll_input as function with args swm_state *
$define %func swm_now_ms as function with args swm_state *
$define %func swm_deliver_frames as function with args swm_state *
$define %func swm_terminal_acquire as function with args guard
$define %func swm_terminal_release as procedure with args guard
$define %func swm_start_de as function with args void
$define %func swm_run as function with args swm_state *
$define %func main as start with args void

*/

/* !SPACE!

$space %internal swm_drain_protocol, swm_poll_input, swm_now_ms
$space %internal swm_deliver_frames, swm_run
$space %internal swm_terminal_acquire, swm_terminal_release, swm_start_de
$space %export main

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <swm/swm.h>

#include "backend/backend.h"
#include "interaction/interaction.h"
#include "protocol/protocol.h"
#include "render/render.h"

#include <native.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#define SWM_WAIT_MS		8
#define SWM_INPUT_BATCH		64
#define SWM_BACKGROUND		0xff101418U
#define SWM_DE_PATH		"/bin/de"

struct swm_frame_delivery {
	uint32_t sent;
	uint32_t dropped;
};

struct swm_terminal_guard {
	int tty;
	int state;
	int suspended;
};

static int
swm_terminal_acquire(struct swm_terminal_guard *guard)
{
	struct api_term_info info;
	struct api_term_power power;

	if (guard == NULL) {
		errno = EINVAL;
		return (-1);
	}
	memset(guard, 0, sizeof(*guard));
	guard->tty = -1;
	memset(&info, 0, sizeof(info));
	if (termInfo(&info) != 0) {
		return (-1);
	}
	guard->tty = info.tty;
	guard->state = info.state;
	if (info.state != TERM_STATE_ACTIVE) {
		return (0);
	}
	memset(&power, 0, sizeof(power));
	power.op = API_TERM_POWER_CHANGE;
	power.tty = info.tty;
	power.state = TERM_STATE_SUSPENDED;
	if (termPower(&power) != 0) {
		return (-1);
	}
	guard->suspended = 1;
	return (0);
}

static void
swm_terminal_release(struct swm_terminal_guard *guard)
{
	struct api_term_power power;

	if (guard == NULL || !guard->suspended) {
		return;
	}
	memset(&power, 0, sizeof(power));
	power.op = API_TERM_POWER_CHANGE;
	power.tty = guard->tty;
	power.state = guard->state;
	(void)termPower(&power);
	guard->suspended = 0;
}

static int
swm_start_de(void)
{
	char *argv[2];

	argv[0] = (char *)SWM_DE_PATH;
	argv[1] = NULL;
	return (procSpawnNative(SWM_DE_PATH, argv, NULL));
}

static int
swm_drain_protocol(swm_state_t *swm)
{
	int ret;

	for (;;) {
		ret = swm_protocol_dispatch(swm);
		if (ret <= 0) {
			return (0);
		}
	}
}

static int
swm_poll_input(swm_state_t *swm)
{
	struct srapi_input_event events[SWM_INPUT_BATCH];
	srapi_device_t *device;
	int count, total;
	uint32_t i;

	device = swm_output_device(swm->output);
	if (device == NULL) {
		return (0);
	}
	total = 0;
	for (;;) {
		count = srapiPollInput(device, events, SWM_INPUT_BATCH);
		if (count <= 0) {
			return (total);
		}
		for (i = 0; i < (uint32_t)count; i++) {
			swm_interaction_forward_input(swm, &events[i]);
		}
		total += count;
		if (count < SWM_INPUT_BATCH || swm->should_quit) {
			return (total);
		}
	}
}

static uint32_t
swm_now_ms(const swm_state_t *swm)
{
	struct api_timeinfo timeinfo;
	uint64_t now;

	memset(&timeinfo, 0, sizeof(timeinfo));
	if (sysTimeInfo(&timeinfo) != 0) {
		return ((uint32_t)(swm->frame_count * 16U));
	}
	now = timeinfo.uptime_sec * 1000ULL;
	now += timeinfo.uptime_nsec / 1000000ULL;
	return ((uint32_t)now);
}

static struct swm_frame_delivery
swm_deliver_frames(swm_state_t *swm)
{
	struct swm_frame_delivery result;
	sprot_body_frame_t body;
	swm_surface_t *surface;
	uint32_t now;
	uint32_t i;

	memset(&result, 0, sizeof(result));
	now = swm_now_ms(swm);
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		surface = &swm->surfaces[i];
		if (!surface->in_use || !surface->wants_frame ||
		    surface->owner == NULL) {
			continue;
		}
		body.time_ms = now;
		body.serial = (uint32_t)swm->frame_count;
		if (swm_protocol_send_event_nb(swm, surface->owner->peer,
		    SPROT_EVT_SURFACE_FRAME, surface->id, body.serial, &body,
		    sizeof(body)) != 0) {
			result.dropped++;
			continue;
		}
		surface->wants_frame = 0;
		result.sent++;
	}
	return (result);
}

static int
swm_run(swm_state_t *swm)
{
	struct kevent event;
	struct swm_frame_delivery delivery;
	struct srapi_region region;
	srapi_image_t *backbuffer;
	swm_rect_t damage;
	int changed, painted, ret;

	swm_damage_all(swm);
	changed = 1;
	while (!swm->should_quit) {
		memset(&event, 0, sizeof(event));
		ret = eventWait(swm->kq, NULL, 0, &event, 1, SWM_WAIT_MS);
		if (ret < 0) {
			return (-1);
		}
		if (ret > 0 && (event.fflags & NOTE_IPC_READ) != 0) {
			(void)swm_drain_protocol(swm);
			changed = 1;
		}
		if (swm_poll_input(swm) > 0) {
			changed = 1;
		}
		swm_protocol_shell_flush(swm);
		if (!changed) {
			continue;
		}
		backbuffer = swm_output_backbuffer(swm->output);
		if (backbuffer == NULL) {
			return (-1);
		}
		painted = 0;
		swm_render_composite(swm, backbuffer, SWM_BACKGROUND, &damage,
		    &painted);
		if (painted) {
			region.x = (uint32_t)damage.x;
			region.y = (uint32_t)damage.y;
			region.width = (uint32_t)damage.w;
			region.height = (uint32_t)damage.h;
			if (swm_output_present(swm->output, &region) !=
			    SRAPI_OK) {
				return (-1);
			}
			swm->frame_count++;
		}
		delivery = swm_deliver_frames(swm);
		(void)delivery;
		changed = 0;
	}
	return (0);
}

int
main(void)
{
	struct swm_terminal_guard terminal;
	swm_state_t swm;
	int de_pid, i, ret, run_ret;

	memset(&terminal, 0, sizeof(terminal));
	memset(&swm, 0, sizeof(swm));
	swm.ipc = -1;
	swm.kq = -1;
	swm.next_cascade_x = 64;
	swm.next_cascade_y = 64;
	ret = swm_output_create_default(&swm.output);
	if (ret != SRAPI_OK) {
		fprintf(stderr, "swm: cannot initialize SRAPI (%d)\n", ret);
		return (1);
	}
	swm.display_w = swm_output_width(swm.output);
	swm.display_h = swm_output_height(swm.output);
	swm.mouse_x = (int32_t)swm.display_w / 2;
	swm.mouse_y = (int32_t)swm.display_h / 2;
	ret = swm_protocol_setup_service(&swm);
	if (ret != 0) {
		fprintf(stderr, "swm: cannot create Sprot service\n");
		swm_output_destroy(swm.output);
		return (1);
	}
	fprintf(stderr, "swm: ready on %ux%u\n", swm.display_w,
	    swm.display_h);
	if (swm_terminal_acquire(&terminal) != 0) {
		fprintf(stderr, "swm: cannot suspend terminal: %s\n",
		    strerror(errno));
		(void)eventClose(swm.kq);
		(void)entityClose(swm.ipc);
		swm_output_destroy(swm.output);
		return (1);
	}
	de_pid = swm_start_de();
	if (de_pid < 0) {
		swm_terminal_release(&terminal);
		fprintf(stderr, "swm: cannot start %s: %s\n", SWM_DE_PATH,
		    strerror(errno));
		(void)eventClose(swm.kq);
		(void)entityClose(swm.ipc);
		swm_output_destroy(swm.output);
		return (1);
	}
	run_ret = swm_run(&swm);
	if (de_pid > 0) {
		(void)procKill((uint32_t)de_pid, 9);
	}
	for (i = 0; i < SWM_MAX_CLIENTS; i++) {
		if (swm.clients[i].in_use) {
			swm_protocol_drop_client(&swm, &swm.clients[i],
			    "shutdown");
		}
	}
	if (swm.kq >= 0) {
		(void)eventClose(swm.kq);
	}
	if (swm.ipc >= 0) {
		(void)entityClose(swm.ipc);
	}
	swm_output_destroy(swm.output);
	swm_terminal_release(&terminal);
	return (run_ret == 0 ? 0 : 1);
}
