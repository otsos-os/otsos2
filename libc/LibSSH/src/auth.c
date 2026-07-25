/* !DEFINES!

$define %type lssh_transport as native SSH transport session
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_slice as borrowed byte span
$define %type lssh_userauth_failure as parsed SSH_MSG_USERAUTH_FAILURE
$define %type uint8_t as 8 bit unsigned
$define %func lssh_auth_slice_eq as function with args lssh_slice, const char *
$define %func lssh_auth_packet_type as function with args const lssh_buf *, uint8_t *
$define %func lssh_auth_skip_packet as function with args uint8_t
$define %func lssh_auth_recv_interesting as function with args lssh_transport *, lssh_buf *, uint8_t *, int
$define %func lssh_service_request_encode as function with args lssh_buf *, const char *
$define %func lssh_service_accept_parse as function with args const void *, size_t, lssh_slice *
$define %func lssh_userauth_password_encode as function with args lssh_buf *, const char *, const char *
$define %func lssh_userauth_failure_parse as function with args const void *, size_t, lssh_userauth_failure *
$define %func lssh_client_request_service as function with args lssh_transport *, const char *
$define %func lssh_client_auth_password as function with args lssh_transport *, const char *, const char *

*/

/* !SPACE!

$space %internal lssh_auth_slice_eq, lssh_auth_packet_type
$space %internal lssh_auth_skip_packet, lssh_auth_recv_interesting
$space %export lssh_service_request_encode, lssh_service_accept_parse
$space %export lssh_userauth_password_encode
$space %export lssh_userauth_failure_parse
$space %export lssh_client_request_service, lssh_client_auth_password

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
lssh_auth_slice_eq(lssh_slice slice, const char *text)
{
	size_t	len;

	if (!text || (!slice.data && slice.len != 0)) {
		return (0);
	}
	len = strlen(text);
	if (slice.len != len) {
		return (0);
	}
	return (memcmp(slice.data, text, len) == 0);
}

static int
lssh_auth_packet_type(const lssh_buf *payload, uint8_t *type)
{
	if (!payload || !type || payload->len == 0) {
		return (LSSH_ERR_FORMAT);
	}
	*type = payload->data[0];
	return (LSSH_OK);
}

static int
lssh_auth_skip_packet(uint8_t type)
{
	if (type == LSSH_MSG_IGNORE || type == LSSH_MSG_DEBUG ||
	    type == LSSH_MSG_UNIMPLEMENTED || type == LSSH_MSG_EXT_INFO ||
	    type == LSSH_MSG_USERAUTH_BANNER) {
		return (1);
	}
	return (0);
}

static int
lssh_auth_recv_interesting(lssh_transport *transport, lssh_buf *payload,
    uint8_t *type, int skip_banner)
{
	int	ret;

	for (;;) {
		ret = lssh_transport_recv_packet(transport, payload);
		if (ret != LSSH_OK) {
			return (ret);
		}
		ret = lssh_auth_packet_type(payload, type);
		if (ret != LSSH_OK) {
			return (ret);
		}
		lssh_logf(LSSH_LOG_DEBUG,
		    "auth: received msg=%u(%s) len=%lu",
		    (unsigned int)*type, lssh_log_packet_type_name(*type),
		    (unsigned long)payload->len);
		if (!lssh_auth_skip_packet(*type)) {
			return (LSSH_OK);
		}
		if (*type == LSSH_MSG_USERAUTH_BANNER && !skip_banner) {
			return (LSSH_OK);
		}
		lssh_logf(LSSH_LOG_DEBUG, "auth: skipped msg=%u(%s)",
		    (unsigned int)*type, lssh_log_packet_type_name(*type));
	}
}

int
lssh_service_request_encode(lssh_buf *out, const char *service)
{
	int	ret;

	if (!out || !service) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_SERVICE_REQUEST);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_cstring(out, service));
}

int
lssh_service_accept_parse(const void *payload, size_t len,
    lssh_slice *service)
{
	lssh_reader	reader;
	uint8_t		msg;
	int		ret;

	if (!payload || !service) {
		return (LSSH_ERR_INVALID);
	}
	lssh_reader_init(&reader, payload, len);
	ret = lssh_reader_u8(&reader, &msg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (msg != LSSH_MSG_SERVICE_ACCEPT) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_string(&reader, service);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_userauth_password_encode(lssh_buf *out, const char *username,
    const char *password)
{
	int	ret;

	if (!out || !username || !password) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_USERAUTH_REQUEST);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(out, username);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(out, "ssh-connection");
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_cstring(out, "password");
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u8(out, 0);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_cstring(out, password));
}

int
lssh_userauth_failure_parse(const void *payload, size_t len,
    lssh_userauth_failure *failure)
{
	lssh_reader	reader;
	uint8_t		msg, partial;
	int		ret;

	if (!payload || !failure) {
		return (LSSH_ERR_INVALID);
	}
	memset(failure, 0, sizeof(*failure));
	lssh_reader_init(&reader, payload, len);
	ret = lssh_reader_u8(&reader, &msg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (msg != LSSH_MSG_USERAUTH_FAILURE) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_string(&reader, &failure->methods);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u8(&reader, &partial);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (partial > 1 || lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	failure->partial_success = partial;
	return (LSSH_OK);
}

int
lssh_client_request_service(lssh_transport *transport, const char *service)
{
	lssh_buf	request, response;
	lssh_slice	accepted;
	uint8_t		type;
	int		ret;

	if (!transport || !service) {
		return (LSSH_ERR_INVALID);
	}
	if (strcmp(service, "ssh-userauth") == 0 &&
	    transport->userauth_service) {
		lssh_logf(LSSH_LOG_DEBUG,
		    "auth: service '%s' already accepted", service);
		return (LSSH_OK);
	}
	ret = lssh_buf_init(&request, 128);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&response, 256);
	if (ret != LSSH_OK) {
		lssh_buf_free(&request);
		return (ret);
	}
	lssh_buf_set_secure(&request, 1);
	lssh_buf_set_secure(&response, 1);
	lssh_logf(LSSH_LOG_INFO, "auth: requesting service '%s'",
	    service);
	ret = lssh_service_request_encode(&request, service);
	if (ret == LSSH_OK) {
		ret = lssh_transport_send_packet(transport, request.data,
		    request.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_auth_recv_interesting(transport, &response,
		    &type, 1);
	}
	if (ret == LSSH_OK && type != LSSH_MSG_SERVICE_ACCEPT) {
		ret = LSSH_ERR_FORMAT;
	}
	if (ret == LSSH_OK) {
		ret = lssh_service_accept_parse(response.data,
		    response.len, &accepted);
	}
	if (ret == LSSH_OK && !lssh_auth_slice_eq(accepted, service)) {
		ret = LSSH_ERR_FORMAT;
	}
	if (ret == LSSH_OK && strcmp(service, "ssh-userauth") == 0) {
		transport->userauth_service = 1;
	}
	if (ret == LSSH_OK) {
		lssh_logf(LSSH_LOG_INFO, "auth: service '%s' accepted",
		    service);
	} else {
		lssh_logf(LSSH_LOG_ERROR,
		    "auth: service '%s' failed ret=%d", service, ret);
	}
	lssh_buf_free(&request);
	lssh_buf_free(&response);
	return (ret);
}

int
lssh_client_auth_password(lssh_transport *transport,
    const char *username, const char *password)
{
	lssh_userauth_failure	failure;
	lssh_buf		request, response;
	uint8_t			type;
	int			ret;

	if (!transport || !username || !password) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_client_request_service(transport, "ssh-userauth");
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&request, 256);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_init(&response, 256);
	if (ret != LSSH_OK) {
		lssh_buf_free(&request);
		return (ret);
	}
	lssh_buf_set_secure(&request, 1);
	lssh_buf_set_secure(&response, 1);
	lssh_logf(LSSH_LOG_INFO,
	    "auth: sending password request for user '%s' (redacted)",
	    username);
	ret = lssh_userauth_password_encode(&request, username, password);
	if (ret == LSSH_OK) {
		ret = lssh_transport_send_packet(transport, request.data,
		    request.len);
	}
	while (ret == LSSH_OK) {
		ret = lssh_auth_recv_interesting(transport, &response,
		    &type, 1);
		if (ret != LSSH_OK) {
			break;
		}
		if (type == LSSH_MSG_USERAUTH_SUCCESS) {
			lssh_logf(LSSH_LOG_INFO, "auth: password accepted");
			ret = LSSH_OK;
			break;
		}
		if (type == LSSH_MSG_USERAUTH_FAILURE) {
			ret = lssh_userauth_failure_parse(response.data,
			    response.len, &failure);
			if (ret == LSSH_OK) {
				lssh_logf(LSSH_LOG_INFO,
				    "auth: password rejected partial=%u "
				    "methods_len=%lu",
				    (unsigned int)failure.partial_success,
				    (unsigned long)failure.methods.len);
				ret = LSSH_ERR_AUTH;
			}
			break;
		}
		ret = LSSH_ERR_FORMAT;
	}
	if (ret != LSSH_OK && ret != LSSH_ERR_AUTH) {
		lssh_logf(LSSH_LOG_ERROR,
		    "auth: password exchange failed ret=%d", ret);
	}
	lssh_buf_free(&request);
	lssh_buf_free(&response);
	return (ret);
}
