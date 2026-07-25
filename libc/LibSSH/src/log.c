/* !DEFINES!

$define %type lssh_log_callback as library trace callback
$define %type uint8_t as 8 bit unsigned
$define %func lssh_log_set as procedure with args int, callback, void *
$define %func lssh_log_enabled as function with args int
$define %func lssh_log_packet_type_name as function with args uint8_t
$define %func lssh_logf as procedure with args int, const char *, ...

*/

/* !SPACE!

$space %export lssh_log_set, lssh_log_enabled
$space %internal lssh_log_packet_type_name, lssh_logf

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
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include "private.h"

#define LSSH_LOG_LINE_MAX	256

static int			g_lssh_log_level;
static lssh_log_callback	g_lssh_log_callback;
static void			*g_lssh_log_ctx;

void
lssh_log_set(int level, lssh_log_callback callback, void *ctx)
{
	if (level < 0) {
		level = 0;
	}
	g_lssh_log_level = level;
	g_lssh_log_callback = callback;
	g_lssh_log_ctx = ctx;
}

int
lssh_log_enabled(int level)
{
	if (!g_lssh_log_callback || level <= 0) {
		return (0);
	}
	return (level <= g_lssh_log_level);
}

const char *
lssh_log_packet_type_name(uint8_t type)
{
	switch (type) {
	case LSSH_MSG_IGNORE:
		return ("SSH_MSG_IGNORE");
	case LSSH_MSG_UNIMPLEMENTED:
		return ("SSH_MSG_UNIMPLEMENTED");
	case LSSH_MSG_DEBUG:
		return ("SSH_MSG_DEBUG");
	case LSSH_MSG_SERVICE_REQUEST:
		return ("SSH_MSG_SERVICE_REQUEST");
	case LSSH_MSG_SERVICE_ACCEPT:
		return ("SSH_MSG_SERVICE_ACCEPT");
	case LSSH_MSG_EXT_INFO:
		return ("SSH_MSG_EXT_INFO");
	case LSSH_MSG_KEXINIT:
		return ("SSH_MSG_KEXINIT");
	case LSSH_MSG_NEWKEYS:
		return ("SSH_MSG_NEWKEYS");
	case LSSH_MSG_KEX_ECDH_INIT:
		return ("SSH_MSG_KEX_ECDH_INIT");
	case LSSH_MSG_KEX_ECDH_REPLY:
		return ("SSH_MSG_KEX_ECDH_REPLY");
	case LSSH_MSG_USERAUTH_REQUEST:
		return ("SSH_MSG_USERAUTH_REQUEST");
	case LSSH_MSG_USERAUTH_FAILURE:
		return ("SSH_MSG_USERAUTH_FAILURE");
	case LSSH_MSG_USERAUTH_SUCCESS:
		return ("SSH_MSG_USERAUTH_SUCCESS");
	case LSSH_MSG_USERAUTH_BANNER:
		return ("SSH_MSG_USERAUTH_BANNER");
	case LSSH_MSG_GLOBAL_REQUEST:
		return ("SSH_MSG_GLOBAL_REQUEST");
	case LSSH_MSG_REQUEST_SUCCESS:
		return ("SSH_MSG_REQUEST_SUCCESS");
	case LSSH_MSG_REQUEST_FAILURE:
		return ("SSH_MSG_REQUEST_FAILURE");
	case LSSH_MSG_CHANNEL_OPEN:
		return ("SSH_MSG_CHANNEL_OPEN");
	case LSSH_MSG_CHANNEL_OPEN_CONFIRMATION:
		return ("SSH_MSG_CHANNEL_OPEN_CONFIRMATION");
	case LSSH_MSG_CHANNEL_OPEN_FAILURE:
		return ("SSH_MSG_CHANNEL_OPEN_FAILURE");
	case LSSH_MSG_CHANNEL_WINDOW_ADJUST:
		return ("SSH_MSG_CHANNEL_WINDOW_ADJUST");
	case LSSH_MSG_CHANNEL_DATA:
		return ("SSH_MSG_CHANNEL_DATA");
	case LSSH_MSG_CHANNEL_EXTENDED_DATA:
		return ("SSH_MSG_CHANNEL_EXTENDED_DATA");
	case LSSH_MSG_CHANNEL_EOF:
		return ("SSH_MSG_CHANNEL_EOF");
	case LSSH_MSG_CHANNEL_CLOSE:
		return ("SSH_MSG_CHANNEL_CLOSE");
	case LSSH_MSG_CHANNEL_REQUEST:
		return ("SSH_MSG_CHANNEL_REQUEST");
	case LSSH_MSG_CHANNEL_SUCCESS:
		return ("SSH_MSG_CHANNEL_SUCCESS");
	case LSSH_MSG_CHANNEL_FAILURE:
		return ("SSH_MSG_CHANNEL_FAILURE");
	default:
		return ("SSH_MSG_UNKNOWN");
	}
}

void
lssh_logf(int level, const char *fmt, ...)
{
	va_list	ap;
	char	line[LSSH_LOG_LINE_MAX];
	int	ret;

	if (!fmt || !lssh_log_enabled(level)) {
		return;
	}
	va_start(ap, fmt);
	ret = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	if (ret < 0) {
		g_lssh_log_callback(g_lssh_log_ctx, level,
		    "log formatting failed");
		return;
	}
	line[sizeof(line) - 1] = '\0';
	g_lssh_log_callback(g_lssh_log_ctx, level, line);
}
