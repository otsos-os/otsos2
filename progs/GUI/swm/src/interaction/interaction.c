/* !DEFINES!

$define %type swm_hit_region as compositor hit-test result
$define %func hit_test as function with args state, coordinates, surface
$define %func clamp_pointer as function with args coordinate, limit
$define %func window_ancestor as function with args state, surface
$define %func swm_interaction_forward_input as procedure with args state, event

*/

/* !SPACE!

$space %internal hit_test, clamp_pointer, window_ancestor
$space %internal send_close_to, send_configure_to
$space %internal toggle_maximize, send_pointer_button
$space %export swm_interaction_forward_input

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include "interaction.h"

#include "../protocol/protocol.h"
#include "../surface/surface.h"

#include <string.h>

enum swm_hit_region {
	SWM_HIT_NONE = 0,
	SWM_HIT_CONTENT,
	SWM_HIT_TITLEBAR,
	SWM_HIT_BTN_MIN,
	SWM_HIT_BTN_MAX,
	SWM_HIT_BTN_CLOSE
};

static int32_t
clamp_pointer(int64_t value, uint32_t limit)
{
	if (value < 0 || limit == 0) {
		return (0);
	}
	if ((uint64_t)value >= limit) {
		return ((int32_t)limit - 1);
	}
	return ((int32_t)value);
}

static enum swm_hit_region
hit_test(swm_state_t *swm, int32_t x, int32_t y, swm_surface_t **out)
{
	swm_surface_t *list[SWM_MAX_SURFACES];
	swm_surface_t *surface;
	int32_t ox, oy, ow, oh;
	int32_t ex, ey, ew, eh;
	int32_t min_x, max_x, close_x, button_y;
	int i, count;

	*out = NULL;
	count = swm_surface_collect_z_asc(swm, list, SWM_MAX_SURFACES);
	for (i = count - 1; i >= 0; i--) {
		surface = list[i];
		swm_surface_outer_rect(swm, surface, &ox, &oy, &ow, &oh);
		if (x < ox || x >= ox + ow || y < oy || y >= oy + oh) {
			continue;
		}
		swm_surface_effective_rect(swm, surface, &ex, &ey, &ew, &eh);
		*out = surface;
		if (x >= ex && x < ex + ew && y >= ey && y < ey + eh) {
			return (SWM_HIT_CONTENT);
		}
		if (swm_surface_role_is_child(surface->role) ||
		    swm_surface_role_is_panel(surface->role)) {
			return (SWM_HIT_CONTENT);
		}
		swm_surface_titlebar_button_rects(swm, surface, &min_x, &max_x,
		    &close_x, &button_y);
		if (y >= button_y && y < button_y + SWM_BTN_SIZE) {
			if (x >= close_x && x < close_x + SWM_BTN_SIZE) {
				return (SWM_HIT_BTN_CLOSE);
			}
			if (x >= max_x && x < max_x + SWM_BTN_SIZE) {
				return (SWM_HIT_BTN_MAX);
			}
			if (x >= min_x && x < min_x + SWM_BTN_SIZE) {
				return (SWM_HIT_BTN_MIN);
			}
		}
		return (SWM_HIT_TITLEBAR);
	}
	return (SWM_HIT_NONE);
}

static swm_surface_t *
window_ancestor(swm_state_t *swm, swm_surface_t *surface)
{
	int depth;

	for (depth = 0; surface != NULL && depth < SWM_MAX_SURFACES; depth++) {
		if (!swm_surface_role_is_child(surface->role)) {
			return (surface);
		}
		surface = swm_surface_find(swm, surface->parent_id);
	}
	return (NULL);
}

static void
send_close_to(swm_state_t *swm, swm_surface_t *surface)
{
	if (surface != NULL && surface->owner != NULL) {
		(void)swm_protocol_send_event_nb(swm, surface->owner->peer,
		    SPROT_EVT_SURFACE_CLOSE, surface->id, 0, NULL, 0);
	}
}

static void
send_configure_to(swm_state_t *swm, swm_surface_t *surface,
	uint32_t state)
{
	sprot_body_configure_t body;
	int32_t width, height;

	if (surface == NULL || surface->owner == NULL) {
		return;
	}
	if (surface->maximized) {
		swm_surface_maximize_target(swm, &width, &height);
	} else if (surface->saved_width > 0 && surface->saved_height > 0) {
		width = (int32_t)surface->saved_width;
		height = (int32_t)surface->saved_height;
	} else {
		width = (int32_t)surface->width;
		height = (int32_t)surface->height;
	}
	if (width <= 0) {
		width = (int32_t)surface->width;
	}
	if (height <= 0) {
		height = (int32_t)surface->height;
	}
	body.width = (uint32_t)width;
	body.height = (uint32_t)height;
	body.state = state;
	body.serial = ++swm->next_z;
	(void)swm_protocol_send_event_nb(swm, surface->owner->peer,
	    SPROT_EVT_SURFACE_CONFIGURE, surface->id, body.serial, &body,
	    sizeof(body));
}

static void
toggle_maximize(swm_state_t *swm, swm_surface_t *surface)
{
	if (surface == NULL) {
		return;
	}
	if (!surface->maximized) {
		surface->saved_pos_x = surface->pos_x;
		surface->saved_pos_y = surface->pos_y;
		surface->saved_width = surface->width;
		surface->saved_height = surface->height;
		surface->maximized = 1;
	} else {
		surface->maximized = 0;
		surface->pos_x = surface->saved_pos_x;
		surface->pos_y = surface->saved_pos_y;
	}
	send_configure_to(swm, surface, surface->maximized ?
	    SPROT_SURFACE_STATE_MAXIMIZED | SPROT_SURFACE_STATE_FOCUSED :
	    SPROT_SURFACE_STATE_FOCUSED);
	swm_protocol_shell_changed(swm);
}

static void
send_pointer_button(swm_state_t *swm, swm_surface_t *surface,
	uint32_t button, uint32_t state)
{
	sprot_body_pointer_button_t body;

	if (surface == NULL || surface->owner == NULL) {
		return;
	}
	body.button = button;
	body.state = state;
	(void)swm_protocol_send_event(swm, surface->owner->peer,
	    SPROT_EVT_POINTER_BUTTON, surface->id, 0, &body, sizeof(body));
}

void
swm_interaction_forward_input(swm_state_t *swm,
	const struct srapi_input_event *event)
{
	swm_surface_t *surface, *popup, *focused;
	enum swm_hit_region region;
	sprot_body_pointer_motion_t motion;
	sprot_body_pointer_axis_t axis;
	sprot_body_key_t key;
	int64_t dx, dy;
	uint32_t old_buttons, changed, button, state;
	int32_t local_x, local_y;

	if (swm == NULL || event == NULL) {
		return;
	}
	if (event->type == SRAPI_INPUT_MOUSE &&
	    (event->flags & SRAPI_MOUSE_MOVE) != 0) {
		if (swm->input_have_raw) {
			dx = (int64_t)event->x - swm->input_raw_x;
			dy = (int64_t)event->y - swm->input_raw_y;
		} else {
			dx = event->dx;
			dy = event->dy;
			swm->input_have_raw = 1;
		}
		swm->input_raw_x = event->x;
		swm->input_raw_y = event->y;
		swm->mouse_x = clamp_pointer(swm->mouse_x + dx, swm->display_w);
		swm->mouse_y = clamp_pointer(swm->mouse_y + dy, swm->display_h);
		if (swm->interaction == SWM_INTERACT_MOVE &&
		    swm->grab_surface != NULL) {
			swm->grab_surface->pos_x = swm->mouse_x - swm->grab_offset_x;
			swm->grab_surface->pos_y = swm->mouse_y - swm->grab_offset_y;
			swm_protocol_shell_changed(swm);
			return;
		}
		region = hit_test(swm, swm->mouse_x, swm->mouse_y, &surface);
		if (region == SWM_HIT_CONTENT && surface != NULL &&
		    surface->owner != NULL) {
			swm_surface_local_coords(swm, surface, swm->mouse_x,
			    swm->mouse_y, &local_x, &local_y);
			motion.x = local_x;
			motion.y = local_y;
			(void)swm_protocol_send_event_nb(swm, surface->owner->peer,
			    SPROT_EVT_POINTER_MOTION, surface->id, 0, &motion,
			    sizeof(motion));
		}
		if (surface != swm->hovered_surface) {
			if (swm->hovered_surface != NULL &&
			    swm->hovered_surface->owner != NULL) {
				(void)swm_protocol_send_event_nb(swm,
				    swm->hovered_surface->owner->peer,
				    SPROT_EVT_POINTER_LEAVE,
				    swm->hovered_surface->id, 0, NULL, 0);
			}
			swm->hovered_surface = region == SWM_HIT_CONTENT ? surface : NULL;
			if (swm->hovered_surface != NULL &&
			    swm->hovered_surface->owner != NULL) {
				(void)swm_protocol_send_event_nb(swm,
				    swm->hovered_surface->owner->peer,
				    SPROT_EVT_POINTER_ENTER,
				    swm->hovered_surface->id, 0, &motion,
				    sizeof(motion));
			}
		}
		return;
	}
	if (event->type == SRAPI_INPUT_MOUSE &&
	    (event->flags & SRAPI_MOUSE_BUTTON) != 0) {
		old_buttons = swm->mouse_buttons;
		changed = old_buttons ^ event->buttons;
		swm->mouse_buttons = event->buttons;
		swm->mouse_left_down =
		    (event->buttons & SRAPI_MOUSE_LEFT) != 0;
		popup = swm_surface_topmost_popup(swm);
		region = hit_test(swm, swm->mouse_x, swm->mouse_y, &surface);
		if (popup != NULL && surface != popup) {
			send_close_to(swm, popup);
			return;
		}
		if (surface != NULL) {
			focused = window_ancestor(swm, surface);
			if (focused != NULL) {
				swm_surface_raise_tree(swm, focused);
			}
		}
		if (region == SWM_HIT_BTN_CLOSE && (changed & SRAPI_MOUSE_LEFT) != 0 &&
		    (event->buttons & SRAPI_MOUSE_LEFT) == 0) {
			send_close_to(swm, surface);
			return;
		}
		if (region == SWM_HIT_BTN_MIN && (changed & SRAPI_MOUSE_LEFT) != 0 &&
		    (event->buttons & SRAPI_MOUSE_LEFT) == 0) {
			surface->minimized = 1;
			swm_protocol_shell_changed(swm);
			return;
		}
		if (region == SWM_HIT_BTN_MAX && (changed & SRAPI_MOUSE_LEFT) != 0 &&
		    (event->buttons & SRAPI_MOUSE_LEFT) == 0) {
			toggle_maximize(swm, surface);
			return;
		}
		if (region == SWM_HIT_TITLEBAR &&
		    (event->buttons & SRAPI_MOUSE_LEFT) != 0 && surface != NULL &&
		    !swm_surface_role_is_child(surface->role) &&
		    !swm_surface_role_is_panel(surface->role)) {
			swm->interaction = SWM_INTERACT_MOVE;
			swm->grab_surface = surface;
			swm->grab_offset_x = swm->mouse_x - surface->pos_x;
			swm->grab_offset_y = swm->mouse_y - surface->pos_y;
			return;
		}
		if (region == SWM_HIT_CONTENT && surface != NULL &&
		    (event->mods & SRAPI_MOD_ALT) != 0 &&
		    (event->buttons & SRAPI_MOUSE_LEFT) != 0) {
			focused = window_ancestor(swm, surface);
			if (focused != NULL &&
			    swm_surface_role_is_window(focused->role)) {
				swm->interaction = SWM_INTERACT_MOVE;
				swm->grab_surface = focused;
				swm->grab_offset_x = swm->mouse_x - focused->pos_x;
				swm->grab_offset_y = swm->mouse_y - focused->pos_y;
				return;
			}
		}
		for (button = SRAPI_MOUSE_LEFT; button <= SRAPI_MOUSE_X2;
		    button <<= 1) {
			if ((changed & button) == 0 || surface == NULL) {
				continue;
			}
			state = (event->buttons & button) != 0 ?
			    SPROT_BUTTON_STATE_PRESSED : SPROT_BUTTON_STATE_RELEASED;
			send_pointer_button(swm, surface, button, state);
		}
		if ((event->buttons & SRAPI_MOUSE_LEFT) == 0 &&
		    swm->interaction == SWM_INTERACT_MOVE) {
			swm->interaction = SWM_INTERACT_NONE;
			swm->grab_surface = NULL;
		}
		return;
	}
	if (event->type == SRAPI_INPUT_MOUSE &&
	    (event->flags & SRAPI_MOUSE_WHEEL) != 0) {
		if (swm->hovered_surface != NULL &&
		    swm->hovered_surface->owner != NULL) {
			axis.dx = event->dx;
			axis.dy = event->dz != 0 ? event->dz : event->dy;
			(void)swm_protocol_send_event_nb(swm,
			    swm->hovered_surface->owner->peer,
			    SPROT_EVT_POINTER_AXIS, swm->hovered_surface->id, 0,
			    &axis, sizeof(axis));
		}
		return;
	}
	if (event->type != SRAPI_INPUT_KEYBOARD) {
		return;
	}
	swm->modifiers = event->mods;
	if ((event->flags & SRAPI_KEY_PRESS) != 0 &&
	    event->key == SRAPI_KEY_ESCAPE &&
	    (event->mods & SRAPI_MOD_CTRL) != 0 &&
	    (event->mods & SRAPI_MOD_ALT) != 0) {
		swm->should_quit = 1;
		return;
	}
	if (event->key == SRAPI_KEY_LSUPER || event->key == SRAPI_KEY_RSUPER) {
		if (swm->shell_client != NULL) {
			key.scancode = event->key;
			key.state = (event->flags & SRAPI_KEY_PRESS) != 0 ?
			    SPROT_KEY_STATE_PRESSED : SPROT_KEY_STATE_RELEASED;
			key.modifiers = event->mods;
			(void)swm_protocol_send_event_nb(swm,
			    swm->shell_client->peer, SPROT_EVT_KEY,
			    swm->shell_surface != NULL ? swm->shell_surface->id : 0,
			    0, &key, sizeof(key));
			return;
		}
	}
	focused = swm_surface_topmost_window(swm);
	if (focused != NULL && focused->owner != NULL) {
		key.scancode = event->key;
		key.state = (event->flags & SRAPI_KEY_PRESS) != 0 ?
		    SPROT_KEY_STATE_PRESSED : SPROT_KEY_STATE_RELEASED;
		key.modifiers = event->mods;
		(void)swm_protocol_send_event(swm, focused->owner->peer,
		    SPROT_EVT_KEY, focused->id, 0, &key, sizeof(key));
	}
}
