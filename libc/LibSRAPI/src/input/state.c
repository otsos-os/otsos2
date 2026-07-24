/* !DEFINES!

$define %func srapi_input_state_reset as procedure with args state
$define %func srapi_input_state_begin_poll as procedure with args state
$define %func srapi_input_state_apply as procedure with args state, event
$define %func srapiInputKeyDown as function with args state, key

*/

/* !SPACE!

$space %internal key_set, key_clear
$space %export srapi_input_state_reset, srapi_input_state_begin_poll
$space %export srapi_input_state_apply, srapiInputKeyDown

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <srapi.h>
#include <stdint.h>
#include <string.h>

#include "../srapi_private.h"

static void
key_set(struct srapi_input_state *state, uint32_t key)
{
	if (key >= SRAPI_INPUT_KEY_BITS) {
		return;
	}
	state->keys[key / 32] |= (uint32_t)1 << (key % 32);
}

static void
key_clear(struct srapi_input_state *state, uint32_t key)
{
	if (key >= SRAPI_INPUT_KEY_BITS) {
		return;
	}
	state->keys[key / 32] &= ~((uint32_t)1 << (key % 32));
}

void
srapi_input_state_reset(struct srapi_input_state *state)
{
	if (!state) {
		return;
	}
	memset(state, 0, sizeof(*state));
}

void
srapi_input_state_begin_poll(struct srapi_input_state *state)
{
	if (!state) {
		return;
	}
	state->mouse_dx = 0;
	state->mouse_dy = 0;
	state->mouse_dz = 0;
	state->events = 0;
	state->dropped = 0;
}

void
srapi_input_state_apply(struct srapi_input_state *state,
    const struct srapi_input_event *event)
{
	if (!state || !event) {
		return;
	}
	state->last_timestamp = event->timestamp;
	state->events++;
	if (event->flags & SRAPI_INPUT_DROPPED) {
		state->dropped += event->lost;
	}
	if (event->type == SRAPI_INPUT_KEYBOARD) {
		state->mods = event->mods;
		if (event->flags & (SRAPI_KEY_PRESS | SRAPI_KEY_REPEAT)) {
			key_set(state, event->key);
		}
		if (event->flags & SRAPI_KEY_RELEASE) {
			key_clear(state, event->key);
		}
	} else if (event->type == SRAPI_INPUT_MOUSE) {
		state->mouse_x = event->x;
		state->mouse_y = event->y;
		state->mouse_dx += event->dx;
		state->mouse_dy += event->dy;
		state->mouse_dz += event->dz;
		state->mouse_buttons = event->buttons;
	}
}

int
srapiInputKeyDown(const struct srapi_input_state *state, uint32_t key)
{
	if (!state || key >= SRAPI_INPUT_KEY_BITS) {
		return (0);
	}
	return ((state->keys[key / 32] & ((uint32_t)1 << (key % 32))) != 0);
}
