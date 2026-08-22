/* !DEFINES!

$define %func swm_surface_role_is_child as function with args role
$define %func swm_surface_role_is_window as function with args role
$define %func swm_surface_role_is_panel as function with args role
$define %func swm_surface_alloc as function with args state
$define %func swm_surface_free as procedure with args state, surface
$define %func swm_surface_find as function with args state, id
$define %func surface_layer as function with args surface
$define %func surface_z_gt as function with args a, b
$define %func sort_surfaces_by_z as procedure with args list, count
$define %func surface_tree_is_visible as function with args state, surface, depth

*/

/* !SPACE!

$space %export swm_surface_role_is_child, swm_surface_role_is_window
$space %export swm_surface_role_is_panel, swm_surface_alloc
$space %export swm_surface_free, swm_surface_find
$space %internal surface_layer, surface_z_gt, sort_surfaces_by_z
$space %internal surface_tree_is_visible
$space %export swm_surface_collect_z_asc, swm_surface_raise
$space %export swm_surface_raise_tree, swm_surface_effective_rect
$space %export swm_surface_outer_rect, swm_surface_titlebar_button_rects
$space %export swm_surface_maximize_target, swm_surface_local_coords
$space %export swm_surface_topmost_window, swm_surface_topmost_popup

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include "surface.h"

#include "../buffer/buffer.h"
#include "../protocol/protocol.h"
#include "../render/render.h"

#include <limits.h>
#include <string.h>

int
swm_surface_role_is_child(uint32_t role)
{
	return (role == SPROT_SURFACE_ROLE_POPUP ||
	    role == SPROT_SURFACE_ROLE_SUBSURFACE);
}

int
swm_surface_role_is_window(uint32_t role)
{
	return (role == SPROT_SURFACE_ROLE_TOPLEVEL);
}

int
swm_surface_role_is_panel(uint32_t role)
{
	return (role == SPROT_SURFACE_ROLE_PANEL);
}

swm_surface_t *
swm_surface_alloc(swm_state_t *swm)
{
	swm_surface_t *surface;
	int i;

	if (swm == NULL) {
		return (NULL);
	}
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		if (!swm->surfaces[i].in_use) {
			surface = &swm->surfaces[i];
			memset(surface, 0, sizeof(*surface));
			surface->in_use = 1;
			return (surface);
		}
	}
	return (NULL);
}

void
swm_surface_free(swm_state_t *swm, swm_surface_t *surface)
{
	swm_surface_t *child;
	uint32_t surface_id;
	int i, was_window;

	if (swm == NULL || surface == NULL || !surface->in_use) {
		return;
	}
	surface_id = surface->id;
	was_window = swm_surface_role_is_window(surface->role);
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		child = &swm->surfaces[i];
		if (child->in_use && swm_surface_role_is_child(child->role) &&
		    child->parent_id == surface_id) {
			swm_surface_free(swm, child);
		}
	}
	if (swm->grab_surface == surface) {
		swm->grab_surface = NULL;
		swm->interaction = SWM_INTERACT_NONE;
	}
	if (swm->hovered_surface == surface) {
		swm->hovered_surface = NULL;
	}
	if (swm->shell_surface == surface) {
		swm->shell_surface = NULL;
	}
	if (was_window) {
		swm_protocol_shell_removed(swm, surface_id);
	}
	swm_buffer_destroy(surface->buffer);
	if (surface->owner != NULL) {
		for (i = 0; i < surface->owner->surface_count; i++) {
			if (surface->owner->surfaces[i] == surface) {
				surface->owner->surfaces[i] =
				    surface->owner->surfaces[
				    surface->owner->surface_count - 1];
				surface->owner->surface_count--;
				break;
			}
		}
	}
	memset(surface, 0, sizeof(*surface));
	swm_damage_all(swm);
	swm_protocol_shell_changed(swm);
}

swm_surface_t *
swm_surface_find(swm_state_t *swm, uint32_t id)
{
	int i;

	if (swm == NULL) {
		return (NULL);
	}
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		if (swm->surfaces[i].in_use && swm->surfaces[i].id == id) {
			return (&swm->surfaces[i]);
		}
	}
	return (NULL);
}

static int
surface_layer(const swm_surface_t *s)
{
	if (s->role == SPROT_SURFACE_ROLE_POPUP ||
	    s->role == SPROT_SURFACE_ROLE_SUBSURFACE) {
		return (4);
	}
	if (s->fullscreen) {
		return (3);
	}
	if (s->role == SPROT_SURFACE_ROLE_PANEL) {
		return (2);
	}
	return (1);
}

static int
surface_z_gt(const swm_surface_t *a, const swm_surface_t *b)
{
	int la, lb;

	la = surface_layer(a);
	lb = surface_layer(b);
	if (la != lb) {
		return (la > lb);
	}
	return (a->z > b->z);
}

static void
sort_surfaces_by_z(swm_surface_t **list, int count)
{
	swm_surface_t *surface;
	int i, j;

	for (i = 1; i < count; i++) {
		surface = list[i];
		j = i - 1;
		while (j >= 0 && surface_z_gt(list[j], surface)) {
			list[j + 1] = list[j];
			j--;
		}
		list[j + 1] = surface;
	}
}

static int
surface_tree_is_visible(swm_state_t *swm, swm_surface_t *surface, int depth)
{
	if (surface == NULL || !surface->in_use || !surface->committed ||
	    !surface->visible || surface->minimized) {
		return (0);
	}
	if (!swm_surface_role_is_child(surface->role)) {
		return (1);
	}
	if (depth >= SWM_MAX_SURFACES) {
		return (0);
	}
	return (surface_tree_is_visible(swm,
	    swm_surface_find(swm, surface->parent_id), depth + 1));
}

int
swm_surface_collect_z_asc(swm_state_t *swm, swm_surface_t **out, int max)
{
	int count, i;

	if (swm == NULL || out == NULL || max <= 0) {
		return (0);
	}
	count = 0;
	for (i = 0; i < SWM_MAX_SURFACES && count < max; i++) {
		if (surface_tree_is_visible(swm, &swm->surfaces[i], 0)) {
			out[count++] = &swm->surfaces[i];
		}
	}
	sort_surfaces_by_z(out, count);
	return (count);
}

void
swm_surface_raise(swm_state_t *swm, swm_surface_t *surface)
{
	if (swm == NULL || surface == NULL ||
	    swm_surface_role_is_panel(surface->role)) {
		return;
	}
	if (surface == swm_surface_topmost_window(swm)) {
		return;
	}
	surface->z = ++swm->next_z;
	swm_damage_all(swm);
	swm_protocol_shell_changed(swm);
}

void
swm_surface_raise_tree(swm_state_t *swm, swm_surface_t *surface)
{
	swm_surface_t *child;
	int i;

	if (swm == NULL || surface == NULL) {
		return;
	}
	swm_surface_raise(swm, surface);
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		child = &swm->surfaces[i];
		if (child->in_use && swm_surface_role_is_child(child->role) &&
		    child->parent_id == surface->id) {
			swm_surface_raise_tree(swm, child);
		}
	}
}

void
swm_surface_effective_rect(swm_state_t *swm, const swm_surface_t *surface,
    int32_t *x, int32_t *y, int32_t *width, int32_t *height)
{
	swm_surface_t *parent;
	int32_t parent_x, parent_y, parent_width, parent_height;

	if (surface->fullscreen) {
		*x = 0;
		*y = 0;
		*width = (int32_t)swm->display_w;
		*height = (int32_t)swm->display_h;
		return;
	}
	if (swm_surface_role_is_child(surface->role) ||
	    swm_surface_role_is_panel(surface->role)) {
		parent = swm_surface_find(swm, surface->parent_id);
		if (parent != NULL) {
			swm_surface_effective_rect(swm, parent, &parent_x, &parent_y,
			    &parent_width, &parent_height);
			*x = parent_x + surface->rel_x;
			*y = parent_y + surface->rel_y;
		} else {
			*x = surface->pos_x;
			*y = surface->pos_y;
		}
		*width = (int32_t)surface->width;
		*height = (int32_t)surface->height;
		return;
	}
	*x = surface->maximized ? SWM_BORDER : surface->pos_x;
	*y = surface->maximized ? SWM_TITLEBAR_H + SWM_BORDER : surface->pos_y;
	*width = (int32_t)surface->width;
	*height = (int32_t)surface->height;
}

void
swm_surface_maximize_target(swm_state_t *swm, int32_t *width, int32_t *height)
{
	int32_t workspace_height;

	workspace_height = (int32_t)swm->display_h;
	if (swm->shell_surface != NULL && swm->shell_surface->height <
	    swm->display_h) {
		workspace_height -= (int32_t)swm->shell_surface->height;
	}
	*width = (int32_t)swm->display_w - 2 * SWM_BORDER;
	*height = workspace_height - SWM_TITLEBAR_H - 2 * SWM_BORDER;
	if (*width < 1) {
		*width = 1;
	}
	if (*height < 1) {
		*height = 1;
	}
}

void
swm_surface_outer_rect(swm_state_t *swm, const swm_surface_t *surface,
    int32_t *x, int32_t *y, int32_t *width, int32_t *height)
{
	int32_t ex, ey, ew, eh;

	swm_surface_effective_rect(swm, surface, &ex, &ey, &ew, &eh);
	if (surface->fullscreen || swm_surface_role_is_child(surface->role) ||
	    swm_surface_role_is_panel(surface->role)) {
		*x = ex;
		*y = ey;
		*width = ew;
		*height = eh;
		return;
	}
	*x = ex - SWM_BORDER;
	*y = ey - SWM_TITLEBAR_H - SWM_BORDER;
	*width = ew + 2 * SWM_BORDER;
	*height = eh + SWM_TITLEBAR_H + 2 * SWM_BORDER;
}

void
swm_surface_titlebar_button_rects(swm_state_t *swm,
    const swm_surface_t *surface, int32_t *min_x, int32_t *max_x,
    int32_t *close_x, int32_t *button_y)
{
	int32_t outer_x, outer_y, outer_width, outer_height, right;

	swm_surface_outer_rect(swm, surface, &outer_x, &outer_y, &outer_width,
	    &outer_height);
	right = outer_x + outer_width - SWM_BORDER;
	*close_x = right - 4 - SWM_BTN_SIZE;
	*max_x = *close_x - 4 - SWM_BTN_SIZE;
	*min_x = *max_x - 4 - SWM_BTN_SIZE;
	*button_y = outer_y + SWM_BORDER +
	    (SWM_TITLEBAR_H - SWM_BTN_SIZE) / 2;
}

void
swm_surface_local_coords(swm_state_t *swm, swm_surface_t *surface,
    int32_t mouse_x, int32_t mouse_y, int32_t *local_x, int32_t *local_y)
{
	int32_t ex, ey, ew, eh;

	swm_surface_effective_rect(swm, surface, &ex, &ey, &ew, &eh);
	*local_x = (int32_t)(((int64_t)(mouse_x - ex) * surface->width) /
	    (ew > 0 ? ew : 1));
	*local_y = (int32_t)(((int64_t)(mouse_y - ey) * surface->height) /
	    (eh > 0 ? eh : 1));
}

swm_surface_t *
swm_surface_topmost_window(swm_state_t *swm)
{
	swm_surface_t *surface, *best;
	int best_z, i;

	best = NULL;
	best_z = INT_MIN;
	if (swm == NULL) {
		return (NULL);
	}
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		surface = &swm->surfaces[i];
		if (!surface_tree_is_visible(swm, surface, 0) ||
		    !swm_surface_role_is_window(surface->role)) {
			continue;
		}
		if (best == NULL || surface->z > best_z) {
			best = surface;
			best_z = surface->z;
		}
	}
	return (best);
}

swm_surface_t *
swm_surface_topmost_popup(swm_state_t *swm)
{
	swm_surface_t *surface, *best;
	int best_z, i;

	best = NULL;
	best_z = INT_MIN;
	if (swm == NULL) {
		return (NULL);
	}
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		surface = &swm->surfaces[i];
		if (!surface_tree_is_visible(swm, surface, 0) ||
		    surface->role != SPROT_SURFACE_ROLE_POPUP) {
			continue;
		}
		if (best == NULL || surface->z > best_z) {
			best = surface;
			best_z = surface->z;
		}
	}
	return (best);
}
