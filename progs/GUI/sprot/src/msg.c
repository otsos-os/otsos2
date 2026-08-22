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
#include <sprot/sprot.h>

#include <errno.h>
#include <stdint.h>
#include <string.h>

static int
message_type_flags(uint16_t type)
{
	return (type >= 0x4000 ? (IPC_MSG_EVENT | IPC_MSG_NONBLOCK) :
	    IPC_MSG_REQUEST);
}

int
sprot_send_message_to(int endpoint, uint64_t peer,
    const sprot_header_t *header, const void *body, size_t body_len,
    int attach_handle)
{
	struct api_ipc_message message;
	uint8_t payload[SPROT_MAX_MESSAGE];
	sprot_header_t hdr;
	size_t total;

	if (endpoint < 0 || header == NULL || body_len > SPROT_MAX_MESSAGE -
	    sizeof(hdr) || (body_len != 0 && body == NULL)) {
		errno = EINVAL;
		return (-1);
	}
	total = sizeof(hdr) + body_len;
	hdr = *header;
	hdr.length = (uint32_t)total;
	if (attach_handle >= 0) {
		hdr.flags |= SPROT_FLAG_HAS_HANDLE;
	} else {
		hdr.flags &= (uint16_t)~SPROT_FLAG_HAS_HANDLE;
	}
	memcpy(payload, &hdr, sizeof(hdr));
	if (body_len != 0) {
		memcpy(payload + sizeof(hdr), body, body_len);
	}
	memset(&message, 0, sizeof(message));
	message.peer = peer;
	message.opcode = hdr.type;
	message.flags = (uint32_t)message_type_flags(hdr.type);
	message.length = (uint32_t)total;
	message.data = payload;
	if (attach_handle >= 0) {
		message.handle_count = 1;
		message.handles[0] = attach_handle;
	}
	if (ipcSend(endpoint, &message) < 0) {
		return (-1);
	}
	return (0);
}

int
sprot_send_message(int endpoint, const sprot_header_t *header,
    const void *body, size_t body_len, int attach_handle)
{
	return (sprot_send_message_to(endpoint, 0, header, body, body_len,
	    attach_handle));
}

int
sprot_recv_message_from_cred(int endpoint, uint64_t *peer,
    struct api_ipc_cred *cred, sprot_header_t *header_out, void *body_out,
    size_t body_cap, int *received_handle, uint32_t flags)
{
	struct api_ipc_message message;
	uint8_t payload[SPROT_MAX_MESSAGE];
	sprot_header_t hdr;
	uint32_t body_len;
	int i, handle;
	ssize_t ret;

	if (endpoint < 0 || header_out == NULL || body_cap > SPROT_MAX_MESSAGE ||
	    (body_cap != 0 && body_out == NULL)) {
		errno = EINVAL;
		return (-1);
	}
	if (received_handle != NULL) {
		*received_handle = -1;
	}
	memset(&message, 0, sizeof(message));
	message.capacity = sizeof(payload);
	message.data = payload;
	message.handle_capacity = IPC_MAX_HANDLES;
	ret = ipcRecv(endpoint, &message, flags);
	if (ret < 0) {
		return (-1);
	}
	if ((size_t)ret < sizeof(hdr) || (size_t)ret != message.length) {
		errno = EBADMSG;
		goto fail_handles;
	}
	memcpy(&hdr, payload, sizeof(hdr));
	if (hdr.length != message.length || hdr.length < sizeof(hdr) ||
	    hdr.length > SPROT_MAX_MESSAGE) {
		errno = EBADMSG;
		goto fail_handles;
	}
	body_len = hdr.length - sizeof(hdr);
	if (body_len > body_cap) {
		errno = EOVERFLOW;
		goto fail_handles;
	}
	if (body_len != 0) {
		memcpy(body_out, payload + sizeof(hdr), body_len);
	}
	if (((hdr.flags & SPROT_FLAG_HAS_HANDLE) != 0) !=
	    (message.handle_count == 1)) {
		errno = EBADMSG;
		goto fail_handles;
	}
	if (peer != NULL) {
		*peer = message.peer;
	}
	if (cred != NULL) {
		*cred = message.cred;
	}
	*header_out = hdr;
	if (message.handle_count == 1) {
		handle = message.handles[0];
		if (received_handle != NULL) {
			*received_handle = handle;
		} else {
			(void)entityClose(handle);
		}
	}
	return ((int)body_len);

fail_handles:
	for (i = 0; i < (int)message.handle_count; i++) {
		(void)entityClose(message.handles[i]);
	}
	return (-1);
}

int
sprot_recv_message_from(int endpoint, uint64_t *peer,
    sprot_header_t *header_out, void *body_out, size_t body_cap,
    int *received_handle, uint32_t flags)
{
	return (sprot_recv_message_from_cred(endpoint, peer, NULL, header_out,
	    body_out, body_cap, received_handle, flags));
}

int
sprot_recv_message(int endpoint, sprot_header_t *header_out, void *body_out,
    size_t body_cap, int *received_handle)
{
	return (sprot_recv_message_from(endpoint, NULL, header_out, body_out,
	    body_cap, received_handle, 0));
}

const char *
sprot_msg_type_name(uint16_t type)
{
	switch (type) {
	case SPROT_REQ_HELLO:
		return ("REQ_HELLO");
	case SPROT_REQ_SURFACE_CREATE:
		return ("REQ_SURFACE_CREATE");
	case SPROT_REQ_SURFACE_ATTACH:
		return ("REQ_SURFACE_ATTACH");
	case SPROT_REQ_SURFACE_DAMAGE:
		return ("REQ_SURFACE_DAMAGE");
	case SPROT_REQ_SURFACE_COMMIT:
		return ("REQ_SURFACE_COMMIT");
	case SPROT_REQ_SURFACE_DESTROY:
		return ("REQ_SURFACE_DESTROY");
	case SPROT_REQ_SURFACE_FRAME:
		return ("REQ_SURFACE_FRAME");
	case SPROT_REQ_SURFACE_SET_TITLE:
		return ("REQ_SURFACE_SET_TITLE");
	case SPROT_REQ_SURFACE_SET_ROLE:
		return ("REQ_SURFACE_SET_ROLE");
	case SPROT_REQ_SET_CURSOR:
		return ("REQ_SET_CURSOR");
	case SPROT_REQ_SET_CURSOR_IMAGE:
		return ("REQ_SET_CURSOR_IMAGE");
	case SPROT_REQ_SURFACE_SET_VISIBLE:
		return ("REQ_SURFACE_SET_VISIBLE");
	case SPROT_REQ_SURFACE_SET_FULLSCREEN:
		return ("REQ_SURFACE_SET_FULLSCREEN");
	case SPROT_REQ_SHELL_SUBSCRIBE:
		return ("REQ_SHELL_SUBSCRIBE");
	case SPROT_REQ_SHELL_ACTION:
		return ("REQ_SHELL_ACTION");
	case SPROT_REQ_PING:
		return ("REQ_PING");
	case SPROT_EVT_WELCOME:
		return ("EVT_WELCOME");
	case SPROT_EVT_SURFACE_CREATED:
		return ("EVT_SURFACE_CREATED");
	case SPROT_EVT_SURFACE_CONFIGURE:
		return ("EVT_SURFACE_CONFIGURE");
	case SPROT_EVT_SURFACE_CLOSE:
		return ("EVT_SURFACE_CLOSE");
	case SPROT_EVT_SURFACE_FRAME:
		return ("EVT_SURFACE_FRAME");
	case SPROT_EVT_POINTER_MOTION:
		return ("EVT_POINTER_MOTION");
	case SPROT_EVT_POINTER_BUTTON:
		return ("EVT_POINTER_BUTTON");
	case SPROT_EVT_POINTER_ENTER:
		return ("EVT_POINTER_ENTER");
	case SPROT_EVT_POINTER_LEAVE:
		return ("EVT_POINTER_LEAVE");
	case SPROT_EVT_POINTER_AXIS:
		return ("EVT_POINTER_AXIS");
	case SPROT_EVT_SHELL_WINDOW:
		return ("EVT_SHELL_WINDOW");
	case SPROT_EVT_SHELL_REMOVE:
		return ("EVT_SHELL_REMOVE");
	case SPROT_EVT_SHELL_WORKAREA:
		return ("EVT_SHELL_WORKAREA");
	case SPROT_EVT_KEY:
		return ("EVT_KEY");
	case SPROT_EVT_PONG:
		return ("EVT_PONG");
	case SPROT_EVT_ERROR:
		return ("EVT_ERROR");
	default:
		return ("UNKNOWN");
	}
}
