/* !DEFINES!

$define %func srapi_input_shutdown as procedure with args device
$define %func srapiPollInput as function with args device, events, max events
$define %func srapiFlushInput as function with args device
$define %func srapiGetInputState as function with args device, out state

*/

/* !SPACE!

$space %internal input_ensure, input_convert_event
$space %export srapi_input_shutdown, srapiPollInput, srapiFlushInput
$space %export srapiGetInputState

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <native.h>
#include <srapi.h>
#include <stdint.h>
#include <string.h>

#include "../srapi_private.h"

#define SRAPI_INPUT_BATCH	16

static int
input_ensure(srapi_device_t *device)
{
	struct kevent	change;
	int		ret;

	if (!device) {
		return (SRAPI_ERR_INVALID);
	}
	if (device->input_ready) {
		return (SRAPI_OK);
	}
	device->input_kq = eventKqueue();
	if (device->input_kq < 0) {
		return (SRAPI_ERR_DRIVER);
	}
	memset(&change, 0, sizeof(change));
	change.ident = 0;
	change.filter = EVFILT_INPUT;
	change.flags = EV_ADD | EV_CLEAR;
	ret = eventWait(device->input_kq, &change, 1, NULL, 0, 0);
	if (ret < 0) {
		eventClose(device->input_kq);
		device->input_kq = -1;
		return (SRAPI_ERR_DRIVER);
	}
	device->input_ready = 1;
	return (SRAPI_OK);
}

static void
input_convert_event(struct srapi_input_event *dst,
    const struct api_input_event *src)
{
	memset(dst, 0, sizeof(*dst));
	dst->timestamp = src->timestamp;
	dst->seq = src->seq;
	dst->type = src->type;
	dst->device = src->device;
	dst->flags = src->flags;
	dst->lost = src->lost;
	dst->x = src->x;
	dst->y = src->y;
	dst->dx = src->dx;
	dst->dy = src->dy;
	dst->dz = src->dz;
	dst->buttons = src->buttons;
	dst->key = src->key;
	dst->raw = src->raw;
	dst->mods = src->mods;
	dst->ch = src->ch;
}

void
srapi_input_shutdown(srapi_device_t *device)
{
	if (!device) {
		return;
	}
	if (device->input_ready && device->input_kq >= 0) {
		eventClose(device->input_kq);
	}
	device->input_kq = -1;
	device->input_ready = 0;
}

int
srapiPollInput(srapi_device_t *device, struct srapi_input_event *events,
    uint32_t max_events)
{
	struct kevent		native_events[SRAPI_INPUT_BATCH];
	struct srapi_input_event converted;
	uint32_t		i, copied, limit;
	int			ret;

	if (!device || (!events && max_events != 0)) {
		return (SRAPI_ERR_INVALID);
	}
	ret = input_ensure(device);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	srapi_input_state_begin_poll(&device->input_state);
	if (max_events == 0) {
		return (0);
	}
	limit = max_events;
	if (limit > SRAPI_INPUT_BATCH) {
		limit = SRAPI_INPUT_BATCH;
	}
	memset(native_events, 0, sizeof(native_events));
	ret = eventWait(device->input_kq, NULL, 0, native_events,
	    (int)limit, 0);
	if (ret < 0) {
		return (SRAPI_ERR_DRIVER);
	}
	copied = 0;
	for (i = 0; i < (uint32_t)ret; i++) {
		if (native_events[i].filter != EVFILT_INPUT) {
			continue;
		}
		input_convert_event(&converted, &native_events[i].input);
		srapi_input_state_apply(&device->input_state, &converted);
		if (events && copied < max_events) {
			events[copied] = converted;
			copied++;
		}
	}
	return ((int)copied);
}

int
srapiFlushInput(srapi_device_t *device)
{
	if (!device) {
		return (SRAPI_ERR_INVALID);
	}
	srapi_input_shutdown(device);
	srapi_input_state_reset(&device->input_state);
	return (input_ensure(device));
}

int
srapiGetInputState(srapi_device_t *device, struct srapi_input_state *out)
{
	if (!device || !out) {
		return (SRAPI_ERR_INVALID);
	}
	memcpy(out, &device->input_state, sizeof(*out));
	return (SRAPI_OK);
}
