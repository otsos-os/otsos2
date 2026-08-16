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
 * 3. Neither the name of the otsos team nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
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

#include "protocol.h"

#include "../buffer/buffer.h"
#include "../surface/surface.h"

#include <native.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void shell_send_snapshot(swm_state_t *swm);

static int
client_is_privileged(const swm_client_t *client)
{
	return (client != NULL && client->has_cred && client->uid == 0);
}

static int
send_error(swm_state_t *swm, uint64_t peer, uint32_t code, const char *text)
{
	sprot_body_error_t body;
	uint8_t payload[sizeof(body) + 128];
	size_t length;
	sprot_header_t hdr;

	length = strlen(text);
	if (length > sizeof(payload) - sizeof(body)) {
		length = sizeof(payload) - sizeof(body);
	}
	body.code = code;
	body.length = (uint32_t)length;
	memcpy(payload, &body, sizeof(body));
	memcpy(payload + sizeof(body), text, length);
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_EVT_ERROR;
	return (swm_protocol_send_event(swm, peer, hdr.type, 0, 0, payload,
	    sizeof(body) + length));
}

int
swm_protocol_send_event(swm_state_t *swm, uint64_t peer, uint16_t type,
    uint32_t object_id, uint32_t serial, const void *body, size_t body_len)
{
	sprot_header_t hdr;

	if (swm == NULL || swm->ipc < 0 || peer == 0) {
		errno = EINVAL;
		return (-1);
	}
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = type;
	hdr.object_id = object_id;
	hdr.serial = serial;
	return (sprot_send_message_to(swm->ipc, peer, &hdr, body, body_len,
	    -1));
}

int
swm_protocol_send_event_nb(swm_state_t *swm, uint64_t peer, uint16_t type,
    uint32_t object_id, uint32_t serial, const void *body, size_t body_len)
{
	return (swm_protocol_send_event(swm, peer, type, object_id, serial, body,
	    body_len));
}

void
swm_protocol_clear_client_cursor(swm_state_t *swm)
{
	if (swm == NULL) {
		return;
	}
	if (swm->cursor_buffer != NULL) {
		swm_buffer_destroy(swm->cursor_buffer);
		swm->cursor_buffer = NULL;
	}
	swm->cursor_visible = 0;
	swm->current_cursor = SPROT_CURSOR_ARROW;
}

void
swm_protocol_drop_client(swm_state_t *swm, swm_client_t *client,
    const char *reason)
{
	if (swm == NULL || client == NULL || !client->in_use) {
		return;
	}
	fprintf(stderr, "[swm] client peer=%llu dropped: %s\n",
	    (unsigned long long)client->peer, reason);
	while (client->surface_count > 0) {
		swm_surface_free(swm,
		    client->surfaces[client->surface_count - 1]);
	}
	if (swm->hovered_surface != NULL &&
	    swm->hovered_surface->owner == client) {
		swm->hovered_surface = NULL;
	}
	if (swm->shell_client == client) {
		swm->shell_client = NULL;
		swm->shell_surface = NULL;
	}
	memset(client, 0, sizeof(*client));
}

swm_client_t *
swm_protocol_alloc_client(swm_state_t *swm, uint64_t peer)
{
	int i;

	if (swm == NULL || peer == 0) {
		return (NULL);
	}
	for (i = 0; i < SWM_MAX_CLIENTS; i++) {
		if (!swm->clients[i].in_use) {
			memset(&swm->clients[i], 0, sizeof(swm->clients[i]));
			swm->clients[i].in_use = 1;
			swm->clients[i].peer = peer;
			return (&swm->clients[i]);
		}
	}
	return (NULL);
}

static swm_client_t *
find_client(swm_state_t *swm, uint64_t peer)
{
	int i;

	for (i = 0; i < SWM_MAX_CLIENTS; i++) {
		if (swm->clients[i].in_use && swm->clients[i].peer == peer) {
			return (&swm->clients[i]);
		}
	}
	return (NULL);
}

int
swm_protocol_setup_service(swm_state_t *swm)
{
	struct kevent change;

	if (swm == NULL) {
		errno = EINVAL;
		return (-1);
	}
	swm->ipc = ipcCreate(SPROT_DEFAULT_SERVICE,
	    IPC_OPEN_EXCLUSIVE | IPC_OPEN_NONBLOCK, 0666);
	if (swm->ipc < 0) {
		return (-1);
	}
	swm->kq = eventKqueue();
	if (swm->kq < 0) {
		(void)entityClose(swm->ipc);
		swm->ipc = -1;
		return (-1);
	}
	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)swm->ipc;
	change.filter = EVFILT_IPC;
	change.flags = EV_ADD | EV_CLEAR;
	change.fflags = NOTE_IPC_READ | NOTE_IPC_HUP;
	if (eventWait(swm->kq, &change, 1, NULL, 0, -1) < 0) {
		(void)eventClose(swm->kq);
		(void)entityClose(swm->ipc);
		swm->kq = -1;
		swm->ipc = -1;
		return (-1);
	}
	return (0);
}

static int
validate_buffer(swm_state_t *swm, uint32_t width, uint32_t height,
    uint32_t stride, uint32_t size)
{
	uint64_t minimum;

	minimum = (uint64_t)stride * height;
	if (width == 0 || height == 0 || width > UINT32_MAX / 4u ||
	    stride < width * 4u || minimum > UINT32_MAX || size < minimum ||
	    width > swm->display_w * 2u || height > swm->display_h * 2u) {
		return (-1);
	}
	return (0);
}

static int
handle_hello(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length)
{
	sprot_body_hello_t hello;
	sprot_body_welcome_t welcome;

	if (length < sizeof(hello)) {
		(void)send_error(swm, client->peer, SPROT_ERROR_PROTOCOL,
		    "short HELLO");
		return (-1);
	}
	memcpy(&hello, data, sizeof(hello));
	if (hello.version_major != SPROT_VERSION_MAJOR) {
		(void)send_error(swm, client->peer, SPROT_ERROR_PROTOCOL,
		    "unsupported protocol version");
		return (-1);
	}
	welcome.display_width = swm->display_w;
	welcome.display_height = swm->display_h;
	welcome.version_major = SPROT_VERSION_MAJOR;
	welcome.version_minor = SPROT_VERSION_MINOR;
	if (swm_protocol_send_event(swm, client->peer, SPROT_EVT_WELCOME, 0,
	    hdr->serial, &welcome, sizeof(welcome)) != 0) {
		return (-1);
	}
	client->has_hello = 1;
	return (0);
}

static int
handle_surface_create(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length)
{
	sprot_body_surface_create_t body;
	sprot_body_surface_created_t response;
	swm_surface_t *surface;

	if (length < sizeof(body)) {
		(void)send_error(swm, client->peer, SPROT_ERROR_PROTOCOL,
		    "short SURFACE_CREATE");
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	if (body.format != SPROT_PIXEL_FORMAT_BGRA8888 ||
	    validate_buffer(swm, body.width, body.height, body.width * 4u,
	    body.width * body.height * 4u) != 0) {
		(void)send_error(swm, client->peer, SPROT_ERROR_INVALID_ARG,
		    "bad surface dimensions");
		return (-1);
	}
	if (client->surface_count >=
	    (int)(sizeof(client->surfaces) / sizeof(client->surfaces[0]))) {
		(void)send_error(swm, client->peer, SPROT_ERROR_LIMIT,
		    "surface limit per client");
		return (-1);
	}
	surface = swm_surface_alloc(swm);
	if (surface == NULL) {
		(void)send_error(swm, client->peer, SPROT_ERROR_LIMIT,
		    "server surface table full");
		return (-1);
	}
	surface->id = ++swm->next_surface_id;
	surface->client_handle = body.client_handle;
	surface->owner = client;
	surface->width = body.width;
	surface->height = body.height;
	surface->stride = body.width * 4u;
	surface->buffer_size = (size_t)surface->stride * body.height;
	surface->visible = 1;
	surface->pos_x = swm->next_cascade_x;
	surface->pos_y = swm->next_cascade_y;
	swm->next_cascade_x += 32;
	swm->next_cascade_y += 32;
	if (swm->next_cascade_x + 100 > (int32_t)swm->display_w) {
		swm->next_cascade_x = 32;
	}
	if (swm->next_cascade_y + 100 > (int32_t)swm->display_h) {
		swm->next_cascade_y = 32;
	}
	surface->z = ++swm->next_z;
	snprintf(surface->title, sizeof(surface->title), "client %llu",
	    (unsigned long long)client->peer);
	client->surfaces[client->surface_count++] = surface;
	response.surface_id = surface->id;
	response.client_handle = surface->client_handle;
	swm_protocol_shell_changed(swm);
	return (swm_protocol_send_event(swm, client->peer,
	    SPROT_EVT_SURFACE_CREATED, surface->id, hdr->serial, &response,
	    sizeof(response)));
}

static int
handle_surface_attach(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length, int handle)
{
	sprot_body_surface_attach_t body;
	swm_surface_t *surface;
	swm_buffer_t *buffer;

	if (length < sizeof(body) || handle < 0) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		(void)send_error(swm, client->peer, SPROT_ERROR_PROTOCOL,
		    "invalid SURFACE_ATTACH");
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	surface = swm_surface_find(swm, hdr->object_id);
	if (surface == NULL || surface->owner != client ||
	    body.format != SPROT_PIXEL_FORMAT_BGRA8888 ||
	    validate_buffer(swm, body.width, body.height, body.stride,
	    body.buffer_size) != 0) {
		(void)shmClose(handle);
		(void)send_error(swm, client->peer, SPROT_ERROR_INVALID_ARG,
		    "bad SURFACE_ATTACH");
		return (-1);
	}
	buffer = swm_buffer_create(handle, body.width, body.height,
	    body.stride, body.buffer_size);
	if (buffer == NULL) {
		(void)send_error(swm, client->peer, SPROT_ERROR_OUT_OF_MEMORY,
		    "cannot map SHM buffer");
		return (-1);
	}
	swm_buffer_destroy(surface->buffer);
	surface->buffer = buffer;
	surface->width = body.width;
	surface->height = body.height;
	surface->stride = body.stride;
	surface->buffer_size = body.buffer_size;
	surface->buffer_has_alpha = 1;
	surface->has_pending = 1;
	return (0);
}

static int
surface_owned(swm_surface_t *surface, swm_client_t *client)
{
	return (surface != NULL && surface->owner == client);
}

static int
handle_surface_set_title(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length)
{
	sprot_body_set_title_t body;
	swm_surface_t *surface;

	if (length < sizeof(body)) {
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	if (body.length > SPROT_MAX_TITLE || body.length > length - sizeof(body)) {
		return (-1);
	}
	surface = swm_surface_find(swm, hdr->object_id);
	if (!surface_owned(surface, client)) {
		return (0);
	}
	memcpy(surface->title, (const uint8_t *)data + sizeof(body),
	    body.length);
	surface->title[body.length] = '\0';
	swm_protocol_shell_changed(swm);
	return (0);
}

static int
handle_surface_set_role(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length)
{
	sprot_body_surface_set_role_t body;
	swm_surface_t *surface, *parent;

	if (length < sizeof(body)) {
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	surface = swm_surface_find(swm, hdr->object_id);
	if (!surface_owned(surface, client)) {
		return (0);
	}
	if (body.role != SPROT_SURFACE_ROLE_TOPLEVEL &&
	    body.role != SPROT_SURFACE_ROLE_PANEL &&
	    !swm_surface_role_is_child(body.role)) {
		return (-1);
	}
	if (body.role == SPROT_SURFACE_ROLE_PANEL &&
	    (!client_is_privileged(client) ||
	    (swm->shell_client != NULL && swm->shell_client != client) ||
	    (swm->shell_surface != NULL && swm->shell_surface != surface))) {
		return (-1);
	}
	if (swm_surface_role_is_child(body.role)) {
		parent = swm_surface_find(swm, body.parent_id);
		if (!surface_owned(parent, client) || parent == surface) {
			return (-1);
		}
		surface->parent_id = body.parent_id;
		surface->rel_x = body.x;
		surface->rel_y = body.y;
	} else {
		surface->parent_id = 0;
		surface->rel_x = 0;
		surface->rel_y = 0;
	}
	if (swm->shell_surface == surface &&
	    body.role != SPROT_SURFACE_ROLE_PANEL) {
		swm->shell_surface = NULL;
	}
	surface->role = body.role;
	if (body.role == SPROT_SURFACE_ROLE_PANEL) {
		client->is_shell = 1;
		swm->shell_client = client;
		swm->shell_surface = surface;
		surface->pos_x = 0;
		surface->pos_y = body.y;
		surface->z = INT_MAX;
		swm_protocol_shell_changed(swm);
	} else {
		surface->z = ++swm->next_z;
	}
	return (0);
}

static int
handle_cursor_image(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length, int handle)
{
	sprot_body_set_cursor_image_t body;
	swm_surface_t *surface;
	swm_buffer_t *buffer;

	if (length < sizeof(body)) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	surface = swm_surface_find(swm, hdr->object_id);
	if (!surface_owned(surface, client) || swm->hovered_surface != surface) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		return (0);
	}
	if (body.visible == 0) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		swm_protocol_clear_client_cursor(swm);
		return (0);
	}
	if (handle < 0 || body.format != SPROT_PIXEL_FORMAT_BGRA8888 ||
	    body.width == 0 || body.height == 0 || body.width > 256 ||
	    body.height > 256 || validate_buffer(swm, body.width, body.height,
	    body.stride, body.buffer_size) != 0) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		return (-1);
	}
	buffer = swm_buffer_create(handle, body.width, body.height,
	    body.stride, body.buffer_size);
	if (buffer == NULL) {
		return (-1);
	}
	swm_protocol_clear_client_cursor(swm);
	swm->cursor_buffer = buffer;
	swm->cursor_visible = 1;
	swm->cursor_hotspot_x = body.hotspot_x;
	swm->cursor_hotspot_y = body.hotspot_y;
	return (0);
}

static int
shell_send_state(swm_state_t *swm, swm_surface_t *surface)
{
	sprot_body_shell_window_t body;

	if (swm == NULL || swm->shell_client == NULL ||
	    swm->shell_client->peer == 0 || surface == NULL ||
	    !surface->in_use || !swm_surface_role_is_window(surface->role)) {
		return (0);
	}
	memset(&body, 0, sizeof(body));
	body.id = surface->id;
	body.state = surface->minimized ? SPROT_SURFACE_STATE_MINIMIZED :
	    SPROT_SURFACE_STATE_NORMAL;
	if (surface->maximized) {
		body.state |= SPROT_SURFACE_STATE_MAXIMIZED;
	}
	if (surface == swm_surface_topmost_window(swm)) {
		body.state |= SPROT_SURFACE_STATE_FOCUSED;
	}
	body.x = surface->pos_x;
	body.y = surface->pos_y;
	body.width = surface->width;
	body.height = surface->height;
	memcpy(body.title, surface->title, sizeof(body.title) - 1);
	body.title[sizeof(body.title) - 1] = '\0';
	return (swm_protocol_send_event_nb(swm, swm->shell_client->peer,
	    SPROT_EVT_SHELL_WINDOW, surface->id, 0, &body, sizeof(body)));
}

static void
shell_send_snapshot(swm_state_t *swm)
{
	sprot_body_shell_workarea_t workarea;
	int i;

	if (swm == NULL || swm->shell_client == NULL) {
		return;
	}
	memset(&workarea, 0, sizeof(workarea));
	workarea.x = 0;
	workarea.y = 0;
	workarea.width = swm->display_w;
	workarea.height = swm->display_h;
	if (swm->shell_surface != NULL && swm->shell_surface->height <
	    swm->display_h) {
		workarea.height -= swm->shell_surface->height;
	}
	(void)swm_protocol_send_event_nb(swm, swm->shell_client->peer,
	    SPROT_EVT_SHELL_WORKAREA, 0, 0, &workarea, sizeof(workarea));
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		(void)shell_send_state(swm, &swm->surfaces[i]);
	}
}

void
swm_protocol_shell_changed(swm_state_t *swm)
{
	if (swm != NULL) {
		swm->shell_dirty = 1;
	}
}

void
swm_protocol_shell_removed(swm_state_t *swm, uint32_t surface_id)
{
	if (swm == NULL || swm->shell_client == NULL || surface_id == 0) {
		return;
	}
	(void)swm_protocol_send_event_nb(swm, swm->shell_client->peer,
	    SPROT_EVT_SHELL_REMOVE, surface_id, 0, NULL, 0);
}

void
swm_protocol_shell_flush(swm_state_t *swm)
{
	if (swm == NULL || !swm->shell_dirty) {
		return;
	}
	shell_send_snapshot(swm);
	swm->shell_dirty = 0;
}

static void
shell_tile(swm_state_t *swm)
{
	swm_surface_t *list[SWM_MAX_SURFACES];
	int32_t width, height, cell_width, cell_height;
	int count, cols, rows, i;

	count = 0;
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		if (swm_surface_role_is_window(swm->surfaces[i].role) &&
		    swm->surfaces[i].in_use && !swm->surfaces[i].minimized) {
			list[count++] = &swm->surfaces[i];
		}
	}
	if (count == 0) {
		return;
	}
	cols = 1;
	rows = 1;
	while (cols * rows < count) {
		if (cols <= rows) {
			cols++;
		} else {
			rows++;
		}
	}
	width = (int32_t)swm->display_w / cols;
	height = (int32_t)swm->display_h / rows;
	if (swm->shell_surface != NULL && height >
	    (int32_t)swm->shell_surface->height) {
		height = ((int32_t)swm->display_h -
		    (int32_t)swm->shell_surface->height) / rows;
	}
	cell_width = width;
	cell_height = height;
	for (i = 0; i < count; i++) {
		list[i]->maximized = 0;
		list[i]->pos_x = (i % cols) * cell_width + SWM_BORDER;
		list[i]->pos_y = (i / cols) * cell_height +
		    SWM_TITLEBAR_H + SWM_BORDER;
	}
}

static void
shell_cascade(swm_state_t *swm)
{
	int i, index;

	index = 0;
	for (i = 0; i < SWM_MAX_SURFACES; i++) {
		if (!swm->surfaces[i].in_use ||
		    !swm_surface_role_is_window(swm->surfaces[i].role)) {
			continue;
		}
		swm->surfaces[i].maximized = 0;
		swm->surfaces[i].minimized = 0;
		swm->surfaces[i].pos_x = 48 + index * 28;
		swm->surfaces[i].pos_y = 56 + index * 28;
		index++;
	}
}

static int
handle_shell_action(swm_state_t *swm, swm_client_t *client,
	const void *data, size_t length)
{
	sprot_body_shell_action_t body;
	swm_surface_t *surface;
	int i;

	if (client != swm->shell_client || length < sizeof(body)) {
		return (-1);
	}
	memcpy(&body, data, sizeof(body));
	surface = swm_surface_find(swm, body.target_id);
	switch (body.action) {
	case SPROT_SHELL_ACTION_FOCUS:
		if (surface != NULL && swm_surface_role_is_window(surface->role)) {
			surface->minimized = 0;
			swm_surface_raise(swm, surface);
		}
		break;
	case SPROT_SHELL_ACTION_MINIMIZE:
		if (surface != NULL && swm_surface_role_is_window(surface->role)) {
			surface->minimized = 1;
		}
		break;
	case SPROT_SHELL_ACTION_TILE:
		shell_tile(swm);
		break;
	case SPROT_SHELL_ACTION_CASCADE:
		shell_cascade(swm);
		break;
	case SPROT_SHELL_ACTION_MINIMIZE_ALL:
		for (i = 0; i < SWM_MAX_SURFACES; i++) {
			if (swm_surface_role_is_window(swm->surfaces[i].role)) {
				swm->surfaces[i].minimized = 1;
			}
		}
		break;
	case SPROT_SHELL_ACTION_QUIT:
		swm->should_quit = 1;
		break;
	default:
		return (-1);
	}
	swm_protocol_shell_changed(swm);
	return (0);
}

static int
dispatch_message(swm_state_t *swm, swm_client_t *client,
    const sprot_header_t *hdr, const void *data, size_t length, int handle)
{
	if (hdr->type != SPROT_REQ_HELLO && !client->has_hello) {
		return (-1);
	}
	switch (hdr->type) {
	case SPROT_REQ_HELLO:
		return (handle_hello(swm, client, hdr, data, length));
	case SPROT_REQ_SURFACE_CREATE:
		return (handle_surface_create(swm, client, hdr, data, length));
	case SPROT_REQ_SURFACE_ATTACH:
		return (handle_surface_attach(swm, client, hdr, data, length,
		    handle));
	case SPROT_REQ_SURFACE_COMMIT: {
		swm_surface_t *surface = swm_surface_find(swm, hdr->object_id);
		if (!surface_owned(surface, client) || surface->buffer == NULL) {
			return (-1);
		}
		surface->committed = 1;
		surface->has_pending = 0;
		swm_protocol_shell_changed(swm);
		return (0);
	}
	case SPROT_REQ_SURFACE_DAMAGE:
		return (0);
	case SPROT_REQ_SURFACE_DESTROY: {
		swm_surface_t *surface = swm_surface_find(swm, hdr->object_id);
		if (surface_owned(surface, client)) {
			swm_surface_free(swm, surface);
		}
		return (0);
	}
	case SPROT_REQ_SURFACE_FRAME: {
		swm_surface_t *surface = swm_surface_find(swm, hdr->object_id);
		if (surface_owned(surface, client)) {
			surface->wants_frame = 1;
		}
		return (0);
	}
	case SPROT_REQ_SURFACE_SET_TITLE:
		return (handle_surface_set_title(swm, client, hdr, data, length));
	case SPROT_REQ_SURFACE_SET_ROLE:
		return (handle_surface_set_role(swm, client, hdr, data, length));
	case SPROT_REQ_SURFACE_SET_VISIBLE: {
		sprot_body_surface_set_visible_t body;
		swm_surface_t *surface;

		if (length < sizeof(body)) {
			return (-1);
		}
		memcpy(&body, data, sizeof(body));
		surface = swm_surface_find(swm, hdr->object_id);
		if (surface_owned(surface, client)) {
			surface->visible = body.visible != 0;
			swm_protocol_shell_changed(swm);
		}
		return (0);
	}
	case SPROT_REQ_SET_CURSOR: {
		swm_surface_t *surface = swm_surface_find(swm, hdr->object_id);
		if (surface_owned(surface, client) && length >=
		    sizeof(sprot_body_set_cursor_t)) {
			sprot_body_set_cursor_t body;
			memcpy(&body, data, sizeof(body));
			swm_protocol_clear_client_cursor(swm);
			swm->current_cursor = body.cursor_type;
		}
		return (0);
	}
	case SPROT_REQ_SET_CURSOR_IMAGE:
		return (handle_cursor_image(swm, client, hdr, data, length,
		    handle));
	case SPROT_REQ_SHELL_SUBSCRIBE:
		if (!client_is_privileged(client)) {
			(void)send_error(swm, client->peer, SPROT_ERROR_ACCESS,
			    "shell access denied");
			return (-1);
		}
		if (swm->shell_client != NULL && swm->shell_client != client) {
			return (-1);
		}
		client->is_shell = 1;
		swm->shell_client = client;
		swm_protocol_shell_changed(swm);
		return (0);
	case SPROT_REQ_SHELL_ACTION:
		if (!client_is_privileged(client)) {
			return (-1);
		}
		return (handle_shell_action(swm, client, data, length));
	case SPROT_REQ_PING:
		return (swm_protocol_send_event(swm, client->peer,
		    SPROT_EVT_PONG, 0, hdr->serial, NULL, 0));
	default:
		return (-1);
	}
}

int
swm_protocol_dispatch(swm_state_t *swm)
{
	struct api_ipc_cred cred;
	sprot_header_t hdr;
	swm_client_t *client;
	uint8_t body[SPROT_MAX_MESSAGE - sizeof(sprot_header_t)];
	uint64_t peer;
	int handle, ret;

	if (swm == NULL) {
		errno = EINVAL;
		return (-1);
	}
	handle = -1;
	memset(&cred, 0, sizeof(cred));
	ret = sprot_recv_message_from_cred(swm->ipc, &peer, &cred, &hdr, body,
	    sizeof(body), &handle, IPC_MSG_NONBLOCK);
	if (ret < 0) {
		return (errno == EAGAIN ? 0 : -1);
	}
	client = find_client(swm, peer);
	if (client == NULL) {
		client = swm_protocol_alloc_client(swm, peer);
	}
	if (client == NULL) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		(void)send_error(swm, peer, SPROT_ERROR_LIMIT, "client limit");
		return (1);
	}
	if (!client->has_cred) {
		client->pid = cred.pid;
		client->uid = cred.uid;
		client->gid = cred.gid;
		client->has_cred = 1;
	} else if (client->pid != cred.pid || client->uid != cred.uid ||
	    client->gid != cred.gid) {
		if (handle >= 0) {
			(void)shmClose(handle);
		}
		(void)send_error(swm, peer, SPROT_ERROR_ACCESS,
		    "IPC credentials changed");
		swm_protocol_drop_client(swm, client,
		    "IPC credentials changed");
		return (1);
	}
	ret = dispatch_message(swm, client, &hdr, body, (size_t)ret, handle);
	if (handle >= 0 && hdr.type != SPROT_REQ_SURFACE_ATTACH &&
	    hdr.type != SPROT_REQ_SET_CURSOR_IMAGE) {
		(void)shmClose(handle);
	}
	if (ret != 0) {
		(void)send_error(swm, peer, SPROT_ERROR_PROTOCOL,
		    "invalid protocol message");
		swm_protocol_drop_client(swm, client, "protocol error");
		return (1);
	}
	return (1);
}
