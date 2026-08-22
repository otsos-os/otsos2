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

#include <native.h>
#include <sprot/client.h>

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sprot_surface {
	sprot_connection_t	*conn;
	uint32_t		id;
	uint32_t		client_handle;
	uint32_t		width;
	uint32_t		height;
	uint32_t		stride;
	size_t			size;
	int			shm_handle;
	void			*map;
	int			attached;
	struct sprot_surface	*next;
};

struct sprot_connection {
	int			endpoint;
	int			kq;
	uint32_t		serial;
	uint32_t		next_handle;
	uint32_t		display_width;
	uint32_t		display_height;
	uint32_t		version_major;
	uint32_t		version_minor;
	struct sprot_surface	*surfaces;
};

static char sprot_error[256] = "ok";

static void
set_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsnprintf(sprot_error, sizeof(sprot_error), fmt, ap);
	va_end(ap);
}

static int
surface_layout(uint32_t width, uint32_t height, uint32_t *stride,
    size_t *size)
{
	uint64_t total;
	uint32_t row;

	if (width == 0 || height == 0 || stride == NULL || size == NULL ||
	    width > UINT32_MAX / sizeof(uint32_t)) {
		errno = EINVAL;
		return (-1);
	}
	row = width * sizeof(uint32_t);
	total = (uint64_t)row * height;
	if (total == 0 || total > UINT32_MAX) {
		errno = EOVERFLOW;
		return (-1);
	}
	*stride = row;
	*size = (size_t)total;
	return (0);
}

static int
surface_buffer_create(size_t size, int *handle, void **map)
{
	int new_handle;
	void *new_map;

	new_handle = -1;
	if (shmGet(SHM_PRIVATE, size, SHM_CREAT | 0600, &new_handle) != 0) {
		return (-1);
	}
	new_map = shmMap(new_handle, NULL, size,
	    API_MAP_READ | API_MAP_WRITE, API_MAP_SHARED);
	if (new_map == NULL) {
		(void)shmCtl(new_handle, SHM_CTL_RMID, NULL);
		(void)shmClose(new_handle);
		return (-1);
	}
	memset(new_map, 0, size);
	*handle = new_handle;
	*map = new_map;
	return (0);
}

static void
surface_buffer_destroy(int handle, void *map, size_t size)
{
	if (map != NULL && size != 0) {
		(void)memUnmap(map, size);
	}
	if (handle >= 0) {
		(void)shmCtl(handle, SHM_CTL_RMID, NULL);
		(void)shmClose(handle);
	}
}

static int
connection_attach_events(sprot_connection_t *conn)
{
	struct kevent change;

	conn->kq = eventKqueue();
	if (conn->kq < 0) {
		return (-1);
	}
	memset(&change, 0, sizeof(change));
	change.ident = (uint64_t)conn->endpoint;
	change.filter = EVFILT_IPC;
	change.flags = EV_ADD | EV_CLEAR;
	change.fflags = NOTE_IPC_READ | NOTE_IPC_HUP;
	if (eventWait(conn->kq, &change, 1, NULL, 0, -1) < 0) {
		(void)eventClose(conn->kq);
		conn->kq = -1;
		return (-1);
	}
	return (0);
}

static void
connection_destroy(sprot_connection_t *conn)
{
	if (conn == NULL) {
		return;
	}
	if (conn->kq >= 0) {
		(void)eventClose(conn->kq);
	}
	if (conn->endpoint >= 0) {
		(void)entityClose(conn->endpoint);
	}
	free(conn);
}

static int
send_surface_message(sprot_surface_t *surface, uint16_t type,
    const void *body, size_t body_len, int handle)
{
	sprot_header_t hdr;

	if (surface == NULL || surface->conn == NULL || surface->id == 0) {
		errno = EINVAL;
		return (-1);
	}
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = type;
	hdr.object_id = surface->id;
	hdr.serial = surface->conn->serial++;
	return (sprot_send_message(surface->conn->endpoint, &hdr, body,
	    body_len, handle));
}

static void
apply_surface_created(sprot_connection_t *conn,
    const sprot_body_surface_created_t *body)
{
	sprot_surface_t *surface;

	for (surface = conn->surfaces; surface != NULL;
	    surface = surface->next) {
		if (surface->client_handle == body->client_handle) {
			surface->id = body->surface_id;
			return;
		}
	}
}

static int
event_body(const sprot_header_t *hdr, size_t need)
{
	return (hdr->length >= sizeof(*hdr) &&
	    hdr->length - sizeof(*hdr) >= need);
}

static int
decode_event(sprot_connection_t *conn, const sprot_header_t *hdr,
    const uint8_t *body, sprot_event_t *event)
{
	sprot_body_surface_created_t created;
	sprot_body_error_t error;
	size_t body_len, message_len;

	memset(event, 0, sizeof(*event));
	event->serial = hdr->serial;
	event->object_id = hdr->object_id;
	switch (hdr->type) {
	case SPROT_EVT_WELCOME:
		if (!event_body(hdr, sizeof(event->u.welcome))) {
			break;
		}
		memcpy(&event->u.welcome, body, sizeof(event->u.welcome));
		event->kind = SPROT_EVENT_WELCOME;
		return (0);
	case SPROT_EVT_SURFACE_CREATED:
		if (!event_body(hdr, sizeof(created))) {
			break;
		}
		memcpy(&created, body, sizeof(created));
		apply_surface_created(conn, &created);
		event->u.surface_created = created;
		event->kind = SPROT_EVENT_SURFACE_CREATED;
		return (0);
	case SPROT_EVT_SURFACE_CONFIGURE:
		if (!event_body(hdr, sizeof(event->u.configure))) {
			break;
		}
		memcpy(&event->u.configure, body, sizeof(event->u.configure));
		event->kind = SPROT_EVENT_SURFACE_CONFIGURE;
		return (0);
	case SPROT_EVT_SURFACE_CLOSE:
		event->kind = SPROT_EVENT_SURFACE_CLOSE;
		return (0);
	case SPROT_EVT_SURFACE_FRAME:
		if (!event_body(hdr, sizeof(event->u.frame))) {
			break;
		}
		memcpy(&event->u.frame, body, sizeof(event->u.frame));
		event->kind = SPROT_EVENT_SURFACE_FRAME;
		return (0);
	case SPROT_EVT_POINTER_MOTION:
	case SPROT_EVT_POINTER_ENTER:
		if (!event_body(hdr, sizeof(event->u.pointer_motion))) {
			break;
		}
		memcpy(&event->u.pointer_motion, body,
		    sizeof(event->u.pointer_motion));
		event->kind = hdr->type == SPROT_EVT_POINTER_ENTER ?
		    SPROT_EVENT_POINTER_ENTER : SPROT_EVENT_POINTER_MOTION;
		return (0);
	case SPROT_EVT_POINTER_BUTTON:
		if (!event_body(hdr, sizeof(event->u.pointer_button))) {
			break;
		}
		memcpy(&event->u.pointer_button, body,
		    sizeof(event->u.pointer_button));
		event->kind = SPROT_EVENT_POINTER_BUTTON;
		return (0);
	case SPROT_EVT_POINTER_LEAVE:
		event->kind = SPROT_EVENT_POINTER_LEAVE;
		return (0);
	case SPROT_EVT_POINTER_AXIS:
		if (!event_body(hdr, sizeof(event->u.pointer_axis))) {
			break;
		}
		memcpy(&event->u.pointer_axis, body,
		    sizeof(event->u.pointer_axis));
		event->kind = SPROT_EVENT_POINTER_AXIS;
		return (0);
	case SPROT_EVT_KEY:
		if (!event_body(hdr, sizeof(event->u.key))) {
			break;
		}
		memcpy(&event->u.key, body, sizeof(event->u.key));
		event->kind = SPROT_EVENT_KEY;
		return (0);
	case SPROT_EVT_SHELL_WINDOW:
		if (!event_body(hdr, sizeof(event->u.shell_window))) {
			break;
		}
		memcpy(&event->u.shell_window, body,
		    sizeof(event->u.shell_window));
		event->kind = SPROT_EVENT_SHELL_WINDOW;
		return (0);
	case SPROT_EVT_SHELL_REMOVE:
		event->kind = SPROT_EVENT_SHELL_REMOVE;
		return (0);
	case SPROT_EVT_SHELL_WORKAREA:
		if (!event_body(hdr, sizeof(event->u.shell_workarea))) {
			break;
		}
		memcpy(&event->u.shell_workarea, body,
		    sizeof(event->u.shell_workarea));
		event->kind = SPROT_EVENT_SHELL_WORKAREA;
		return (0);
	case SPROT_EVT_PONG:
		event->kind = SPROT_EVENT_PONG;
		return (0);
	case SPROT_EVT_ERROR:
		if (!event_body(hdr, sizeof(error))) {
			break;
		}
		memcpy(&error, body, sizeof(error));
		body_len = hdr->length - sizeof(*hdr);
		message_len = error.length;
		if (message_len > body_len - sizeof(error)) {
			break;
		}
		if (message_len >= sizeof(event->u.error.message)) {
			message_len = sizeof(event->u.error.message) - 1;
		}
		event->u.error.code = error.code;
		memcpy(event->u.error.message, body + sizeof(error),
		    message_len);
		event->u.error.message[message_len] = '\0';
		event->kind = SPROT_EVENT_ERROR;
		return (0);
	default:
		event->kind = SPROT_EVENT_NONE;
		return (0);
	}
	errno = EBADMSG;
	return (-1);
}

const char *
sprot_last_error(void)
{
	return (sprot_error);
}

sprot_connection_t *
sprot_connect(const char *service_name)
{
	sprot_body_welcome_t welcome;
	sprot_body_hello_t hello;
	sprot_connection_t *conn;
	sprot_header_t hdr;
	int handle, ret;

	if (service_name == NULL || service_name[0] == '\0') {
		service_name = SPROT_DEFAULT_SERVICE;
	}
	conn = calloc(1, sizeof(*conn));
	if (conn == NULL) {
		set_error("sprot: out of memory");
		return (NULL);
	}
	conn->endpoint = -1;
	conn->kq = -1;
	conn->endpoint = ipcConnect(service_name, 0);
	if (conn->endpoint < 0) {
		set_error("sprot: connect %s: %s", service_name,
		    strerror(errno));
		connection_destroy(conn);
		return (NULL);
	}
	if (connection_attach_events(conn) != 0) {
		set_error("sprot: kqueue: %s", strerror(errno));
		connection_destroy(conn);
		return (NULL);
	}
	conn->serial = 1;
	conn->next_handle = 1;
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_REQ_HELLO;
	hdr.serial = conn->serial++;
	hello.version_major = SPROT_VERSION_MAJOR;
	hello.version_minor = SPROT_VERSION_MINOR;
	if (sprot_send_message(conn->endpoint, &hdr, &hello, sizeof(hello),
	    -1) != 0) {
		set_error("sprot: send HELLO: %s", strerror(errno));
		connection_destroy(conn);
		return (NULL);
	}
	handle = -1;
	ret = sprot_recv_message(conn->endpoint, &hdr, &welcome,
	    sizeof(welcome), &handle);
	if (handle >= 0) {
		(void)entityClose(handle);
	}
	if (ret != (int)sizeof(welcome) || hdr.type != SPROT_EVT_WELCOME ||
	    welcome.version_major != SPROT_VERSION_MAJOR) {
		set_error("sprot: invalid WELCOME");
		connection_destroy(conn);
		errno = EPROTO;
		return (NULL);
	}
	conn->display_width = welcome.display_width;
	conn->display_height = welcome.display_height;
	conn->version_major = welcome.version_major;
	conn->version_minor = welcome.version_minor;
	return (conn);
}

void
sprot_disconnect(sprot_connection_t *conn)
{
	if (conn == NULL) {
		return;
	}
	while (conn->surfaces != NULL) {
		sprot_destroy_surface(conn->surfaces);
	}
	connection_destroy(conn);
}

int
sprot_connection_endpoint(const sprot_connection_t *conn)
{
	return (conn != NULL ? conn->endpoint : -1);
}

uint32_t
sprot_display_width(const sprot_connection_t *conn)
{
	return (conn != NULL ? conn->display_width : 0);
}

uint32_t
sprot_display_height(const sprot_connection_t *conn)
{
	return (conn != NULL ? conn->display_height : 0);
}

sprot_surface_t *
sprot_create_surface(sprot_connection_t *conn, uint32_t width,
    uint32_t height)
{
	sprot_body_surface_create_t body;
	sprot_surface_t *surface;
	sprot_header_t hdr;

	if (conn == NULL) {
		errno = EINVAL;
		return (NULL);
	}
	surface = calloc(1, sizeof(*surface));
	if (surface == NULL) {
		set_error("sprot: out of memory");
		return (NULL);
	}
	surface->shm_handle = -1;
	if (surface_layout(width, height, &surface->stride,
	    &surface->size) != 0 || surface_buffer_create(surface->size,
	    &surface->shm_handle, &surface->map) != 0) {
		set_error("sprot: create surface buffer: %s", strerror(errno));
		free(surface);
		return (NULL);
	}
	surface->conn = conn;
	surface->width = width;
	surface->height = height;
	surface->client_handle = conn->next_handle++;
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_REQ_SURFACE_CREATE;
	hdr.serial = conn->serial++;
	body.width = width;
	body.height = height;
	body.format = SPROT_PIXEL_FORMAT_BGRA8888;
	body.client_handle = surface->client_handle;
	if (sprot_send_message(conn->endpoint, &hdr, &body, sizeof(body), -1)
	    != 0) {
		set_error("sprot: send SURFACE_CREATE: %s", strerror(errno));
		surface_buffer_destroy(surface->shm_handle, surface->map,
		    surface->size);
		free(surface);
		return (NULL);
	}
	surface->next = conn->surfaces;
	conn->surfaces = surface;
	return (surface);
}

void
sprot_destroy_surface(sprot_surface_t *surface)
{
	sprot_connection_t *conn;
	sprot_surface_t **link;

	if (surface == NULL) {
		return;
	}
	conn = surface->conn;
	if (conn != NULL && surface->id != 0) {
		(void)send_surface_message(surface, SPROT_REQ_SURFACE_DESTROY,
		    NULL, 0, -1);
	}
	if (conn != NULL) {
		for (link = &conn->surfaces; *link != NULL;
		    link = &(*link)->next) {
			if (*link == surface) {
				*link = surface->next;
				break;
			}
		}
	}
	surface_buffer_destroy(surface->shm_handle, surface->map,
	    surface->size);
	free(surface);
}

uint32_t *
sprot_surface_pixels(sprot_surface_t *surface)
{
	return (surface != NULL ? surface->map : NULL);
}

uint32_t
sprot_surface_width(const sprot_surface_t *surface)
{
	return (surface != NULL ? surface->width : 0);
}

uint32_t
sprot_surface_height(const sprot_surface_t *surface)
{
	return (surface != NULL ? surface->height : 0);
}

uint32_t
sprot_surface_stride(const sprot_surface_t *surface)
{
	return (surface != NULL ? surface->stride : 0);
}

uint32_t
sprot_surface_id(const sprot_surface_t *surface)
{
	return (surface != NULL ? surface->id : 0);
}

int
sprot_attach_handle(sprot_surface_t *surface, int handle, uint32_t width,
    uint32_t height, uint32_t stride, uint32_t buffer_size, uint32_t format)
{
	sprot_body_surface_attach_t body;
	uint64_t minimum;

	minimum = (uint64_t)stride * height;
	if (surface == NULL || surface->conn == NULL || surface->id == 0 ||
	    handle < 0 || width == 0 || height == 0 ||
	    width > UINT32_MAX / sizeof(uint32_t) ||
	    stride < width * sizeof(uint32_t) || minimum > UINT32_MAX ||
	    buffer_size < minimum || format != SPROT_PIXEL_FORMAT_BGRA8888) {
		errno = EINVAL;
		return (-1);
	}
	body.width = width;
	body.height = height;
	body.stride = stride;
	body.buffer_size = buffer_size;
	body.format = format;
	if (send_surface_message(surface, SPROT_REQ_SURFACE_ATTACH, &body,
	    sizeof(body), handle) != 0) {
		set_error("sprot: send SURFACE_ATTACH: %s", strerror(errno));
		return (-1);
	}
	surface->attached = 1;
	return (0);
}

int
sprot_commit(sprot_surface_t *surface)
{
	if (surface == NULL || surface->conn == NULL || surface->id == 0) {
		errno = EINVAL;
		return (-1);
	}
	if (!surface->attached && sprot_attach_handle(surface,
	    surface->shm_handle, surface->width, surface->height,
	    surface->stride, (uint32_t)surface->size,
	    SPROT_PIXEL_FORMAT_BGRA8888) != 0) {
		return (-1);
	}
	return (send_surface_message(surface, SPROT_REQ_SURFACE_COMMIT,
	    NULL, 0, -1));
}

int
sprot_resize_surface(sprot_surface_t *surface, uint32_t width,
    uint32_t height)
{
	size_t new_size;
	void *new_map;
	uint32_t new_stride;
	int new_handle;

	if (surface == NULL || surface->conn == NULL) {
		errno = EINVAL;
		return (-1);
	}
	if (surface->width == width && surface->height == height) {
		return (0);
	}
	if (surface_layout(width, height, &new_stride, &new_size) != 0) {
		return (-1);
	}
	new_handle = -1;
	new_map = NULL;
	if (surface_buffer_create(new_size, &new_handle, &new_map) != 0) {
		set_error("sprot: resize surface buffer: %s", strerror(errno));
		return (-1);
	}
	surface_buffer_destroy(surface->shm_handle, surface->map,
	    surface->size);
	surface->shm_handle = new_handle;
	surface->map = new_map;
	surface->width = width;
	surface->height = height;
	surface->stride = new_stride;
	surface->size = new_size;
	surface->attached = 0;
	return (0);
}

int
sprot_damage(sprot_surface_t *surface, int32_t x, int32_t y, uint32_t width,
    uint32_t height)
{
	sprot_body_surface_damage_t body;

	body.x = x;
	body.y = y;
	body.width = width;
	body.height = height;
	return (send_surface_message(surface, SPROT_REQ_SURFACE_DAMAGE, &body,
	    sizeof(body), -1));
}

int
sprot_request_frame(sprot_surface_t *surface)
{
	return (send_surface_message(surface, SPROT_REQ_SURFACE_FRAME, NULL, 0,
	    -1));
}

int
sprot_set_title(sprot_surface_t *surface, const char *title)
{
	sprot_body_set_title_t body;
	uint8_t payload[sizeof(body) + SPROT_MAX_TITLE];
	size_t length;

	if (title == NULL) {
		title = "";
	}
	length = strlen(title);
	if (length > SPROT_MAX_TITLE) {
		length = SPROT_MAX_TITLE;
	}
	body.length = (uint32_t)length;
	memcpy(payload, &body, sizeof(body));
	memcpy(payload + sizeof(body), title, length);
	return (send_surface_message(surface, SPROT_REQ_SURFACE_SET_TITLE,
	    payload, sizeof(body) + length, -1));
}

int
sprot_set_role(sprot_surface_t *surface, uint32_t role, uint32_t parent_id,
    int32_t x, int32_t y)
{
	sprot_body_surface_set_role_t body;

	body.role = role;
	body.parent_id = parent_id;
	body.x = x;
	body.y = y;
	return (send_surface_message(surface, SPROT_REQ_SURFACE_SET_ROLE, &body,
	    sizeof(body), -1));
}

int
sprot_set_visible(sprot_surface_t *surface, int visible)
{
	sprot_body_surface_set_visible_t body;

	body.visible = visible != 0;
	return (send_surface_message(surface, SPROT_REQ_SURFACE_SET_VISIBLE,
	    &body, sizeof(body), -1));
}

int
sprot_set_cursor(sprot_surface_t *surface, uint32_t cursor_type)
{
	sprot_body_set_cursor_t body;

	body.cursor_type = cursor_type;
	return (send_surface_message(surface, SPROT_REQ_SET_CURSOR, &body,
	    sizeof(body), -1));
}

int
sprot_set_cursor_image(sprot_surface_t *surface, int handle, uint32_t width,
    uint32_t height, uint32_t stride, uint32_t buffer_size,
    int32_t hotspot_x, int32_t hotspot_y, uint32_t visible)
{
	sprot_body_set_cursor_image_t body;
	uint64_t minimum;

	minimum = (uint64_t)stride * height;
	if (visible != 0 && (handle < 0 || width == 0 || height == 0 ||
	    width > UINT32_MAX / sizeof(uint32_t) ||
	    stride < width * sizeof(uint32_t) || minimum > UINT32_MAX ||
	    buffer_size < minimum)) {
		errno = EINVAL;
		return (-1);
	}
	memset(&body, 0, sizeof(body));
	body.width = width;
	body.height = height;
	body.stride = stride;
	body.buffer_size = buffer_size;
	body.hotspot_x = hotspot_x;
	body.hotspot_y = hotspot_y;
	body.visible = visible != 0;
	body.format = SPROT_PIXEL_FORMAT_BGRA8888;
	return (send_surface_message(surface, SPROT_REQ_SET_CURSOR_IMAGE, &body,
	    sizeof(body), visible != 0 ? handle : -1));
}

int
sprot_shell_subscribe(sprot_connection_t *conn)
{
	sprot_body_shell_subscribe_t body;
	sprot_header_t hdr;

	if (conn == NULL) {
		errno = EINVAL;
		return (-1);
	}
	memset(&body, 0, sizeof(body));
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_REQ_SHELL_SUBSCRIBE;
	hdr.serial = conn->serial++;
	return (sprot_send_message(conn->endpoint, &hdr, &body, sizeof(body),
	    -1));
}

int
sprot_shell_action(sprot_connection_t *conn, uint32_t action,
	uint32_t target_id)
{
	sprot_body_shell_action_t body;
	sprot_header_t hdr;

	if (conn == NULL || action < SPROT_SHELL_ACTION_FOCUS ||
	    action > SPROT_SHELL_ACTION_QUIT) {
		errno = EINVAL;
		return (-1);
	}
	memset(&body, 0, sizeof(body));
	body.action = action;
	body.target_id = target_id;
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_REQ_SHELL_ACTION;
	hdr.serial = conn->serial++;
	return (sprot_send_message(conn->endpoint, &hdr, &body, sizeof(body),
	    -1));
}

int
sprot_ping(sprot_connection_t *conn, uint32_t serial)
{
	sprot_header_t hdr;

	if (conn == NULL) {
		errno = EINVAL;
		return (-1);
	}
	memset(&hdr, 0, sizeof(hdr));
	hdr.type = SPROT_REQ_PING;
	hdr.serial = serial;
	return (sprot_send_message(conn->endpoint, &hdr, NULL, 0, -1));
}

int
sprot_poll_event(sprot_connection_t *conn, sprot_event_t *event,
    int timeout_ms)
{
	struct kevent kev;
	sprot_header_t hdr;
	uint8_t body[SPROT_MAX_MESSAGE - sizeof(sprot_header_t)];
	int handle, ret;

	if (conn == NULL || event == NULL || timeout_ms < -1) {
		errno = EINVAL;
		return (-1);
	}
	handle = -1;
	ret = sprot_recv_message_from(conn->endpoint, NULL, &hdr, body,
	    sizeof(body), &handle, IPC_MSG_NONBLOCK);
	if (ret < 0 && errno != EAGAIN) {
		set_error("sprot: receive event: %s", strerror(errno));
		return (-1);
	}
	if (ret < 0) {
		memset(&kev, 0, sizeof(kev));
		ret = eventWait(conn->kq, NULL, 0, &kev, 1, timeout_ms);
		if (ret < 0) {
			set_error("sprot: wait event: %s", strerror(errno));
			return (-1);
		}
		if (ret == 0) {
			return (0);
		}
		if ((kev.fflags & NOTE_IPC_READ) == 0 &&
		    (kev.fflags & NOTE_IPC_HUP) != 0) {
			memset(event, 0, sizeof(*event));
			event->kind = SPROT_EVENT_DISCONNECT;
			return (1);
		}
		ret = sprot_recv_message_from(conn->endpoint, NULL, &hdr, body,
		    sizeof(body), &handle, IPC_MSG_NONBLOCK);
		if (ret < 0) {
			if (errno == EAGAIN) {
				if ((kev.fflags & NOTE_IPC_HUP) != 0) {
					memset(event, 0, sizeof(*event));
					event->kind = SPROT_EVENT_DISCONNECT;
					return (1);
				}
				return (0);
			}
			set_error("sprot: receive event: %s", strerror(errno));
			return (-1);
		}
	}
	if (handle >= 0) {
		(void)entityClose(handle);
		errno = EBADMSG;
		return (-1);
	}
	if (decode_event(conn, &hdr, body, event) != 0) {
		set_error("sprot: malformed %s", sprot_msg_type_name(hdr.type));
		return (-1);
	}
	return (1);
}
