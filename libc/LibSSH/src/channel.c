/* !DEFINES!

$define %type lssh_transport as native SSH transport session
$define %type lssh_channel as SSH connection channel state
$define %type lssh_channel_open_confirmation as parsed channel open confirmation
$define %type lssh_channel_open_failure as parsed channel open failure
$define %type lssh_channel_event as parsed channel message event
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_slice as borrowed byte span
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %func lssh_channel_packet_type as function with args const lssh_buf *, uint8_t *
$define %func lssh_channel_skip_packet as function with args uint8_t
$define %func lssh_channel_send_encoded as function with args lssh_transport *, lssh_buf *
$define %func lssh_channel_handle_global_request as function with args lssh_transport *, lssh_buf *
$define %func lssh_channel_recv_typed as function with args lssh_transport *, lssh_buf *, uint8_t *
$define %func lssh_channel_apply_event as function with args lssh_channel *, const lssh_channel_event *
$define %func lssh_channel_init as procedure with args lssh_channel *, uint32_t, uint32_t, uint32_t
$define %func lssh_channel_open_session_encode as function with args lssh_buf *, const lssh_channel *
$define %func lssh_channel_open_confirmation_parse as function with args const void *, size_t, lssh_channel_open_confirmation *
$define %func lssh_channel_open_failure_parse as function with args const void *, size_t, lssh_channel_open_failure *
$define %func lssh_channel_apply_open_confirmation as function with args lssh_channel *, const lssh_channel_open_confirmation *
$define %func lssh_channel_request_encode as function with args lssh_buf *, const lssh_channel *, const char *, int, const void *, size_t
$define %func lssh_channel_request_pty_encode as function with args lssh_buf *, const lssh_channel *, const char *, uint32_t, uint32_t, uint32_t, uint32_t, int
$define %func lssh_channel_request_window_change_encode as function with args lssh_buf *, const lssh_channel *, uint32_t, uint32_t, uint32_t, uint32_t
$define %func lssh_channel_request_shell_encode as function with args lssh_buf *, const lssh_channel *, int
$define %func lssh_channel_request_exec_encode as function with args lssh_buf *, const lssh_channel *, const char *, int
$define %func lssh_channel_data_encode as function with args lssh_buf *, const lssh_channel *, const void *, size_t
$define %func lssh_channel_eof_encode as function with args lssh_buf *, const lssh_channel *
$define %func lssh_channel_close_encode as function with args lssh_buf *, const lssh_channel *
$define %func lssh_channel_window_adjust_encode as function with args lssh_buf *, const lssh_channel *, uint32_t
$define %func lssh_channel_event_parse as function with args const void *, size_t, lssh_channel_event *
$define %func lssh_client_open_session as function with args lssh_transport *, lssh_channel *, uint32_t
$define %func lssh_client_channel_request as function with args lssh_transport *, lssh_channel *, const char *, int, const void *, size_t
$define %func lssh_client_channel_request_pty as function with args transport, channel, term, cols, rows, pixels
$define %func lssh_client_channel_request_window_change as function with args transport, channel, cols, rows, pixels
$define %func lssh_client_channel_request_shell as function with args lssh_transport *, lssh_channel *
$define %func lssh_client_channel_request_exec as function with args lssh_transport *, lssh_channel *, const char *
$define %func lssh_client_channel_send_data as function with args lssh_transport *, lssh_channel *, const void *, size_t
$define %func lssh_client_channel_send_eof as function with args lssh_transport *, lssh_channel *
$define %func lssh_client_channel_close as function with args lssh_transport *, lssh_channel *
$define %func lssh_client_channel_adjust_window as function with args lssh_transport *, lssh_channel *, uint32_t
$define %func lssh_client_channel_recv_event as function with args lssh_transport *, lssh_channel *, lssh_buf *, lssh_channel_event *

*/

/* !SPACE!

$space %internal lssh_channel_packet_type, lssh_channel_skip_packet
$space %internal lssh_channel_send_encoded
$space %internal lssh_channel_handle_global_request
$space %internal lssh_channel_recv_typed
$space %internal lssh_channel_apply_event
$space %export lssh_channel_init, lssh_channel_open_session_encode
$space %export lssh_channel_open_confirmation_parse
$space %export lssh_channel_open_failure_parse
$space %export lssh_channel_apply_open_confirmation
$space %export lssh_channel_request_encode
$space %export lssh_channel_request_pty_encode
$space %export lssh_channel_request_window_change_encode
$space %export lssh_channel_request_shell_encode
$space %export lssh_channel_request_exec_encode
$space %export lssh_channel_data_encode, lssh_channel_eof_encode
$space %export lssh_channel_close_encode
$space %export lssh_channel_window_adjust_encode
$space %export lssh_channel_event_parse
$space %export lssh_client_open_session, lssh_client_channel_request
$space %export lssh_client_channel_request_pty
$space %export lssh_client_channel_request_window_change
$space %export lssh_client_channel_request_shell
$space %export lssh_client_channel_request_exec
$space %export lssh_client_channel_send_data
$space %export lssh_client_channel_send_eof
$space %export lssh_client_channel_close
$space %export lssh_client_channel_adjust_window
$space %export lssh_client_channel_recv_event

*/

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

#include <libssh.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

static int
lssh_channel_packet_type(const lssh_buf *payload, uint8_t *type)
{
	if (!payload || !type || payload->len == 0) {
		return (LSSH_ERR_FORMAT);
	}
	*type = payload->data[0];
	return (LSSH_OK);
}

static int
lssh_channel_skip_packet(uint8_t type)
{
	if (type == LSSH_MSG_IGNORE || type == LSSH_MSG_DEBUG ||
	    type == LSSH_MSG_UNIMPLEMENTED || type == LSSH_MSG_EXT_INFO) {
		return (1);
	}
	return (0);
}

static int
lssh_channel_send_encoded(lssh_transport *transport, lssh_buf *payload)
{
	int	ret;

	if (!transport || !payload) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_transport_send_packet(transport, payload->data,
	    payload->len);
	lssh_buf_reset(payload);
	return (ret);
}

static int
lssh_channel_handle_global_request(lssh_transport *transport,
    lssh_buf *payload)
{
	lssh_reader	reader;
	lssh_slice	request;
	lssh_buf	reply;
	uint8_t		type, want_reply, failure;
	int		ret;

	if (!transport || !payload) {
		return (LSSH_ERR_INVALID);
	}
	lssh_reader_init(&reader, payload->data, payload->len);
	ret = lssh_reader_u8(&reader, &type);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (type != LSSH_MSG_GLOBAL_REQUEST) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_string(&reader, &request);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u8(&reader, &want_reply);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (want_reply > 1) {
		return (LSSH_ERR_FORMAT);
	}
	lssh_logf(LSSH_LOG_INFO,
	    "channel: global request len=%lu want_reply=%u",
	    (unsigned long)request.len, (unsigned int)want_reply);
	if (!want_reply) {
		return (LSSH_OK);
	}
	ret = lssh_buf_init(&reply, 16);
	if (ret != LSSH_OK) {
		return (ret);
	}
	failure = LSSH_MSG_REQUEST_FAILURE;
	ret = lssh_buf_put_u8(&reply, failure);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &reply);
	}
	lssh_buf_free(&reply);
	return (ret);
}

static int
lssh_channel_recv_typed(lssh_transport *transport, lssh_buf *payload,
    uint8_t *type)
{
	int	ret;

	for (;;) {
		ret = lssh_transport_recv_packet(transport, payload);
		if (ret != LSSH_OK) {
			return (ret);
		}
		ret = lssh_channel_packet_type(payload, type);
		if (ret != LSSH_OK) {
			return (ret);
		}
		lssh_logf(LSSH_LOG_DEBUG,
		    "channel: received msg=%u(%s) len=%lu",
		    (unsigned int)*type, lssh_log_packet_type_name(*type),
		    (unsigned long)payload->len);
		if (!lssh_channel_skip_packet(*type)) {
			if (*type == LSSH_MSG_GLOBAL_REQUEST) {
				ret = lssh_channel_handle_global_request(
				    transport, payload);
				if (ret != LSSH_OK) {
					return (ret);
				}
				continue;
			}
			return (LSSH_OK);
		}
		lssh_logf(LSSH_LOG_DEBUG,
		    "channel: skipped msg=%u(%s)",
		    (unsigned int)*type, lssh_log_packet_type_name(*type));
	}
}

static int
lssh_channel_apply_event(lssh_channel *channel,
    const lssh_channel_event *event)
{
	if (!channel || !event) {
		return (LSSH_ERR_INVALID);
	}
	if (event->recipient != channel->local_id) {
		return (LSSH_ERR_STATE);
	}
	if (event->type == LSSH_MSG_CHANNEL_WINDOW_ADJUST) {
		if (UINT32_MAX - channel->remote_window < event->bytes) {
			return (LSSH_ERR_RANGE);
		}
		channel->remote_window += event->bytes;
	} else if (event->type == LSSH_MSG_CHANNEL_DATA ||
	    event->type == LSSH_MSG_CHANNEL_EXTENDED_DATA) {
		if (channel->local_window < event->data.len) {
			return (LSSH_ERR_RANGE);
		}
		channel->local_window -= (uint32_t)event->data.len;
	} else if (event->type == LSSH_MSG_CHANNEL_EOF) {
		channel->eof_received = 1;
	} else if (event->type == LSSH_MSG_CHANNEL_CLOSE) {
		channel->close_received = 1;
		channel->open = 0;
	}
	return (LSSH_OK);
}

void
lssh_channel_init(lssh_channel *channel, uint32_t local_id,
    uint32_t window, uint32_t max_packet)
{
	if (!channel) {
		return;
	}
	memset(channel, 0, sizeof(*channel));
	channel->local_id = local_id;
	channel->local_window = window != 0 ?
	    window : LSSH_CHANNEL_DEFAULT_WINDOW;
	channel->local_max_packet = max_packet != 0 ?
	    max_packet : LSSH_CHANNEL_DEFAULT_MAX_PACKET;
}

int
lssh_channel_open_session_encode(lssh_buf *out,
    const lssh_channel *channel)
{
	int	ret;

	if (!out || !channel) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_OPEN);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(out, "session");
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(out, channel->local_id);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(out, channel->local_window);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_u32(out, channel->local_max_packet));
}

int
lssh_channel_open_confirmation_parse(const void *payload, size_t len,
    lssh_channel_open_confirmation *out)
{
	lssh_reader	reader;
	uint8_t		msg;
	int		ret;

	if (!payload || !out) {
		return (LSSH_ERR_INVALID);
	}
	memset(out, 0, sizeof(*out));
	lssh_reader_init(&reader, payload, len);
	ret = lssh_reader_u8(&reader, &msg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (msg != LSSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_u32(&reader, &out->recipient);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u32(&reader, &out->sender);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u32(&reader, &out->initial_window);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u32(&reader, &out->max_packet);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_channel_open_failure_parse(const void *payload, size_t len,
    lssh_channel_open_failure *out)
{
	lssh_reader	reader;
	uint8_t		msg;
	int		ret;

	if (!payload || !out) {
		return (LSSH_ERR_INVALID);
	}
	memset(out, 0, sizeof(*out));
	lssh_reader_init(&reader, payload, len);
	ret = lssh_reader_u8(&reader, &msg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (msg != LSSH_MSG_CHANNEL_OPEN_FAILURE) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_u32(&reader, &out->recipient);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u32(&reader, &out->reason);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_string(&reader, &out->description);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_string(&reader, &out->language);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_channel_apply_open_confirmation(lssh_channel *channel,
    const lssh_channel_open_confirmation *confirmation)
{
	if (!channel || !confirmation) {
		return (LSSH_ERR_INVALID);
	}
	if (confirmation->recipient != channel->local_id ||
	    confirmation->max_packet == 0) {
		return (LSSH_ERR_STATE);
	}
	channel->remote_id = confirmation->sender;
	channel->remote_window = confirmation->initial_window;
	channel->remote_max_packet = confirmation->max_packet;
	channel->open = 1;
	return (LSSH_OK);
}

int
lssh_channel_request_encode(lssh_buf *out, const lssh_channel *channel,
    const char *request, int want_reply, const void *extra,
    size_t extra_len)
{
	int	ret;

	if (!out || !channel || !channel->open || !request ||
	    (!extra && extra_len != 0)) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_REQUEST);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(out, channel->remote_id);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(out, request);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u8(out, want_reply ? 1 : 0);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_append(out, extra, extra_len));
}

int
lssh_channel_request_pty_encode(lssh_buf *out,
    const lssh_channel *channel, const char *term, uint32_t cols,
    uint32_t rows, uint32_t width_px, uint32_t height_px, int want_reply)
{
	lssh_buf	extra;
	int		ret;

	if (!term) {
		term = "xterm";
	}
	ret = lssh_buf_init(&extra, 96);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(&extra, term);
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, cols);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, rows);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, width_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, height_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&extra, NULL, 0);
	}
	if (ret == LSSH_OK) {
		ret = lssh_channel_request_encode(out, channel, "pty-req",
		    want_reply, extra.data, extra.len);
	}
	lssh_buf_free(&extra);
	return (ret);
}

int
lssh_channel_request_shell_encode(lssh_buf *out,
    const lssh_channel *channel, int want_reply)
{
	return (lssh_channel_request_encode(out, channel, "shell",
	    want_reply, NULL, 0));
}

int
lssh_channel_request_window_change_encode(lssh_buf *out,
    const lssh_channel *channel, uint32_t cols, uint32_t rows,
    uint32_t width_px, uint32_t height_px)
{
	lssh_buf	extra;
	int		ret;

	ret = lssh_buf_init(&extra, 32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(&extra, cols);
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, rows);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, width_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, height_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_channel_request_encode(out, channel,
		    "window-change", 0, extra.data, extra.len);
	}
	lssh_buf_free(&extra);
	return (ret);
}

int
lssh_channel_request_exec_encode(lssh_buf *out,
    const lssh_channel *channel, const char *command, int want_reply)
{
	lssh_buf	extra;
	int		ret;

	if (!command) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&extra, 128);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(&extra, command);
	if (ret == LSSH_OK) {
		ret = lssh_channel_request_encode(out, channel, "exec",
		    want_reply, extra.data, extra.len);
	}
	lssh_buf_free(&extra);
	return (ret);
}

int
lssh_channel_data_encode(lssh_buf *out, const lssh_channel *channel,
    const void *data, size_t len)
{
	int	ret;

	if (!out || !channel || !channel->open ||
	    (!data && len != 0) || len > UINT32_MAX) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_DATA);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(out, channel->remote_id);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_string(out, data, len));
}

int
lssh_channel_eof_encode(lssh_buf *out, const lssh_channel *channel)
{
	int	ret;

	if (!out || !channel || !channel->open) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_EOF);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_u32(out, channel->remote_id));
}

int
lssh_channel_close_encode(lssh_buf *out, const lssh_channel *channel)
{
	int	ret;

	if (!out || !channel) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_CLOSE);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_u32(out, channel->remote_id));
}

int
lssh_channel_window_adjust_encode(lssh_buf *out,
    const lssh_channel *channel, uint32_t bytes)
{
	int	ret;

	if (!out || !channel || !channel->open || bytes == 0) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_CHANNEL_WINDOW_ADJUST);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u32(out, channel->remote_id);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_u32(out, bytes));
}

int
lssh_channel_event_parse(const void *payload, size_t len,
    lssh_channel_event *event)
{
	lssh_reader	reader, body;
	lssh_slice	extra;
	uint8_t		msg;
	int		ret;

	if (!payload || !event) {
		return (LSSH_ERR_INVALID);
	}
	memset(event, 0, sizeof(*event));
	lssh_reader_init(&reader, payload, len);
	ret = lssh_reader_u8(&reader, &msg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	event->type = msg;
	ret = lssh_reader_u32(&reader, &event->recipient);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (msg == LSSH_MSG_CHANNEL_WINDOW_ADJUST) {
		ret = lssh_reader_u32(&reader, &event->bytes);
	} else if (msg == LSSH_MSG_CHANNEL_DATA) {
		ret = lssh_reader_string(&reader, &event->data);
		event->bytes = (uint32_t)event->data.len;
	} else if (msg == LSSH_MSG_CHANNEL_EXTENDED_DATA) {
		ret = lssh_reader_u32(&reader, &event->extended_type);
		if (ret == LSSH_OK) {
			ret = lssh_reader_string(&reader, &event->data);
			event->bytes = (uint32_t)event->data.len;
		}
	} else if (msg == LSSH_MSG_CHANNEL_EOF ||
	    msg == LSSH_MSG_CHANNEL_CLOSE ||
	    msg == LSSH_MSG_CHANNEL_SUCCESS ||
	    msg == LSSH_MSG_CHANNEL_FAILURE) {
		ret = LSSH_OK;
	} else if (msg == LSSH_MSG_CHANNEL_REQUEST) {
		ret = lssh_reader_string(&reader, &event->request_type);
		if (ret == LSSH_OK) {
			ret = lssh_reader_u8(&reader, &event->want_reply);
		}
		if (ret == LSSH_OK) {
			extra.data = reader.data + reader.off;
			extra.len = lssh_reader_remaining(&reader);
			event->request_data = extra;
			reader.off = reader.len;
			if (event->request_type.len == 11 &&
			    memcmp(event->request_type.data,
			    "exit-status", 11) == 0) {
				lssh_reader_init(&body, extra.data,
				    extra.len);
				ret = lssh_reader_u32(&body,
				    &event->exit_status);
				if (ret == LSSH_OK) {
					if (lssh_reader_remaining(&body) != 0) {
						ret = LSSH_ERR_FORMAT;
					} else {
						event->has_exit_status = 1;
					}
				}
			}
		}
	} else {
		return (LSSH_ERR_UNSUPPORTED);
	}
	if (ret != LSSH_OK) {
		return (ret);
	}
	if ((msg == LSSH_MSG_CHANNEL_DATA ||
	    msg == LSSH_MSG_CHANNEL_EXTENDED_DATA) &&
	    event->bytes > LSSH_PACKET_MAX) {
		return (LSSH_ERR_RANGE);
	}
	if (lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_client_open_session(lssh_transport *transport, lssh_channel *channel,
    uint32_t local_id)
{
	lssh_channel_open_confirmation	confirmation;
	lssh_channel_open_failure	failure;
	lssh_buf			payload;
	uint8_t				type;
	int				ret;

	if (!transport || !channel) {
		return (LSSH_ERR_INVALID);
	}
	lssh_channel_init(channel, local_id, 0, 0);
	lssh_logf(LSSH_LOG_INFO, "channel: opening session local_id=%u",
	    (unsigned int)channel->local_id);
	ret = lssh_buf_init(&payload, 256);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_open_session_encode(&payload, channel);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &payload);
	}
	while (ret == LSSH_OK) {
		ret = lssh_channel_recv_typed(transport, &payload, &type);
		if (ret != LSSH_OK) {
			break;
		}
		if (type == LSSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
			ret = lssh_channel_open_confirmation_parse(
			    payload.data, payload.len, &confirmation);
			if (ret == LSSH_OK) {
				ret = lssh_channel_apply_open_confirmation(
				    channel, &confirmation);
			}
			if (ret == LSSH_OK) {
				lssh_logf(LSSH_LOG_INFO,
				    "channel: open confirmed local=%u "
				    "remote=%u window=%u max_packet=%u",
				    (unsigned int)channel->local_id,
				    (unsigned int)channel->remote_id,
				    (unsigned int)channel->remote_window,
				    (unsigned int)channel->remote_max_packet);
			}
			break;
		}
		if (type == LSSH_MSG_CHANNEL_OPEN_FAILURE) {
			ret = lssh_channel_open_failure_parse(payload.data,
			    payload.len, &failure);
			if (ret == LSSH_OK) {
				lssh_logf(LSSH_LOG_ERROR,
				    "channel: open failed reason=%u",
				    (unsigned int)failure.reason);
				ret = LSSH_ERR_STATE;
			}
			break;
		}
		ret = LSSH_ERR_FORMAT;
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_request(lssh_transport *transport,
    lssh_channel *channel, const char *request, int want_reply,
    const void *extra, size_t extra_len)
{
	lssh_channel_event	event;
	lssh_buf		payload;
	uint8_t			type;
	int			ret;

	if (!transport || !channel || !request) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&payload, 256);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lssh_logf(LSSH_LOG_INFO,
	    "channel: request '%s' local=%u remote=%u want_reply=%d",
	    request, (unsigned int)channel->local_id,
	    (unsigned int)channel->remote_id, want_reply);
	ret = lssh_channel_request_encode(&payload, channel, request,
	    want_reply, extra, extra_len);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &payload);
	}
	while (ret == LSSH_OK && want_reply) {
		ret = lssh_channel_recv_typed(transport, &payload, &type);
		if (ret != LSSH_OK) {
			break;
		}
		ret = lssh_channel_event_parse(payload.data, payload.len,
		    &event);
		if (ret != LSSH_OK) {
			break;
		}
		if (event.recipient != channel->local_id) {
			ret = LSSH_ERR_STATE;
			break;
		}
		if (type == LSSH_MSG_CHANNEL_SUCCESS) {
			lssh_logf(LSSH_LOG_INFO,
			    "channel: request '%s' accepted", request);
			ret = LSSH_OK;
			break;
		}
		if (type == LSSH_MSG_CHANNEL_FAILURE) {
			lssh_logf(LSSH_LOG_ERROR,
			    "channel: request '%s' rejected", request);
			ret = LSSH_ERR_STATE;
			break;
		}
		if (type == LSSH_MSG_CHANNEL_WINDOW_ADJUST) {
			ret = lssh_channel_apply_event(channel, &event);
			if (ret != LSSH_OK) {
				break;
			}
			lssh_logf(LSSH_LOG_DEBUG,
			    "channel: request wait applied window adjust=%u "
			    "remote_window=%u",
			    (unsigned int)event.bytes,
			    (unsigned int)channel->remote_window);
			continue;
		}
		ret = LSSH_ERR_FORMAT;
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_request_pty(lssh_transport *transport,
    lssh_channel *channel, const char *term, uint32_t cols,
    uint32_t rows, uint32_t width_px, uint32_t height_px)
{
	lssh_buf	extra;
	int		ret;

	ret = lssh_buf_init(&extra, 128);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (!term) {
		term = "xterm";
	}
	ret = lssh_buf_put_cstring(&extra, term);
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, cols);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, rows);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, width_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_u32(&extra, height_px);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&extra, NULL, 0);
	}
	if (ret == LSSH_OK) {
		ret = lssh_client_channel_request(transport, channel,
		    "pty-req", 1, extra.data, extra.len);
	}
	lssh_buf_free(&extra);
	return (ret);
}

int
lssh_client_channel_request_window_change(lssh_transport *transport,
    const lssh_channel *channel, uint32_t cols, uint32_t rows,
    uint32_t width_px, uint32_t height_px)
{
	lssh_buf	payload;
	int		ret;

	ret = lssh_buf_init(&payload, 64);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_request_window_change_encode(&payload, channel,
	    cols, rows, width_px, height_px);
	if (ret == LSSH_OK) {
		ret = lssh_transport_send_packet(transport, payload.data,
		    payload.len);
	}
	if (ret == LSSH_OK) {
		lssh_logf(LSSH_LOG_INFO,
		    "channel: window-change cols=%u rows=%u",
		    (unsigned int)cols, (unsigned int)rows);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_request_shell(lssh_transport *transport,
    lssh_channel *channel)
{
	return (lssh_client_channel_request(transport, channel, "shell",
	    1, NULL, 0));
}

int
lssh_client_channel_request_exec(lssh_transport *transport,
    lssh_channel *channel, const char *command)
{
	lssh_buf	extra;
	int		ret;

	ret = lssh_buf_init(&extra, 128);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(&extra, command);
	if (ret == LSSH_OK) {
		ret = lssh_client_channel_request(transport, channel,
		    "exec", 1, extra.data, extra.len);
	}
	lssh_buf_free(&extra);
	return (ret);
}

int
lssh_client_channel_send_data(lssh_transport *transport,
    lssh_channel *channel, const void *data, size_t len)
{
	lssh_buf	payload;
	int		ret;

	if (!transport || !channel || (!data && len != 0) ||
	    len > channel->remote_window ||
	    len > channel->remote_max_packet) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&payload, len + 32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_data_encode(&payload, channel, data, len);
	if (ret == LSSH_OK) {
		ret = lssh_transport_send_packet(transport, payload.data,
		    payload.len);
	}
	if (ret == LSSH_OK) {
		channel->remote_window -= (uint32_t)len;
		lssh_logf(LSSH_LOG_DEBUG,
		    "channel: sent data len=%lu remote_window=%u",
		    (unsigned long)len,
		    (unsigned int)channel->remote_window);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_send_eof(lssh_transport *transport,
    lssh_channel *channel)
{
	lssh_buf	payload;
	int		ret;

	if (!transport || !channel) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&payload, 32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_eof_encode(&payload, channel);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &payload);
	}
	if (ret == LSSH_OK) {
		channel->eof_sent = 1;
		lssh_logf(LSSH_LOG_INFO, "channel: sent eof local=%u",
		    (unsigned int)channel->local_id);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_close(lssh_transport *transport, lssh_channel *channel)
{
	lssh_buf	payload;
	int		ret;

	if (!transport || !channel) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&payload, 32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_close_encode(&payload, channel);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &payload);
	}
	if (ret == LSSH_OK) {
		channel->close_sent = 1;
		channel->open = 0;
		lssh_logf(LSSH_LOG_INFO, "channel: sent close local=%u",
		    (unsigned int)channel->local_id);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_adjust_window(lssh_transport *transport,
    lssh_channel *channel, uint32_t bytes)
{
	lssh_buf	payload;
	int		ret;

	if (!transport || !channel || bytes == 0 ||
	    UINT32_MAX - channel->local_window < bytes) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&payload, 32);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_window_adjust_encode(&payload, channel, bytes);
	if (ret == LSSH_OK) {
		ret = lssh_channel_send_encoded(transport, &payload);
	}
	if (ret == LSSH_OK) {
		channel->local_window += bytes;
		lssh_logf(LSSH_LOG_DEBUG,
		    "channel: adjusted local window by=%u window=%u",
		    (unsigned int)bytes,
		    (unsigned int)channel->local_window);
	}
	lssh_buf_free(&payload);
	return (ret);
}

int
lssh_client_channel_recv_event(lssh_transport *transport,
    lssh_channel *channel, lssh_buf *payload, lssh_channel_event *event)
{
	uint8_t	type;
	int	ret;

	if (!transport || !channel || !payload || !event) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_channel_recv_typed(transport, payload, &type);
	if (ret != LSSH_OK) {
		return (ret);
	}
	(void)type;
	ret = lssh_channel_event_parse(payload->data, payload->len, event);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_channel_apply_event(channel, event);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lssh_logf(LSSH_LOG_DEBUG,
	    "channel: event msg=%u(%s) recipient=%u bytes=%u",
	    (unsigned int)event->type,
	    lssh_log_packet_type_name(event->type),
	    (unsigned int)event->recipient, (unsigned int)event->bytes);
	if (event->has_exit_status) {
		lssh_logf(LSSH_LOG_INFO, "channel: exit-status=%u",
		    (unsigned int)event->exit_status);
	}
	if (event->type == LSSH_MSG_CHANNEL_EOF) {
		lssh_logf(LSSH_LOG_INFO, "channel: received eof local=%u",
		    (unsigned int)channel->local_id);
	}
	if (event->type == LSSH_MSG_CHANNEL_CLOSE) {
		lssh_logf(LSSH_LOG_INFO, "channel: received close local=%u",
		    (unsigned int)channel->local_id);
	}
	return (LSSH_OK);
}
