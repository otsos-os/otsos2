/* !DEFINES!

$define %type mtp_log_callback as callback receiving one formatted trace line
$define %func mtpLogSet as procedure with args level, callback, context
$define %func mtpLogEnabled as function with args level
$define %func mtp_logf as procedure with args level, format
$define %func mtp_log_hex as procedure with args level, label, data, length
$define %func mtp_log_req_name as function with args request kind
$define %func mtp_log_ctor_name as function with args constructor id
$define %func mtp_log_mask as procedure with args out, capacity, text

*/

/* !SPACE!

$space %export mtpLogSet, mtpLogEnabled
$space %export mtp_logf, mtp_log_hex, mtp_log_req_name, mtp_log_ctor_name
$space %export mtp_log_mask

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
 * SUBSTITUTE GOODS OR SERVICES; LOSS, USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */



#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "mtp_internal.h"

#define LOG_LINE_MAX		256
#define LOG_HEX_PER_LINE	16
#define LOG_HEX_MAX		64

static int			g_level;
static mtp_log_callback		g_callback;
static void			*g_ctx;

static const char	g_hex[] = "0123456789abcdef";

void
mtpLogSet(int level, mtp_log_callback callback, void *ctx)
{
	if (level < MTP_LOG_NONE) {
		level = MTP_LOG_NONE;
	}
	if (level > MTP_LOG_TRACE) {
		level = MTP_LOG_TRACE;
	}
	g_level = level;
	g_callback = callback;
	g_ctx = ctx;
}

int
mtpLogEnabled(int level)
{
	if (g_callback == NULL || level <= MTP_LOG_NONE) {
		return (0);
	}
	return (level <= g_level);
}

void
mtp_logf(int level, const char *fmt, ...)
{
	va_list	ap;
	char	line[LOG_LINE_MAX];
	int	ret;

	if (fmt == NULL || !mtpLogEnabled(level)) {
		return;
	}
	va_start(ap, fmt);
	ret = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	if (ret < 0) {
		g_callback(g_ctx, level, "log line formatting failed");
		return;
	}
	line[sizeof(line) - 1] = '\0';
	g_callback(g_ctx, level, line);
}


void
mtp_log_hex(int level, const char *label, const void *data, size_t len)
{
	const uint8_t	*p;
	char		text[LOG_HEX_PER_LINE * 3 + 1];
	size_t		off, i, chunk, pos;

	if (!mtpLogEnabled(level) || data == NULL || label == NULL) {
		return;
	}
	p = (const uint8_t *)data;
	if (len > LOG_HEX_MAX) {
		len = LOG_HEX_MAX;
	}
	for (off = 0; off < len; off += LOG_HEX_PER_LINE) {
		chunk = len - off;
		if (chunk > LOG_HEX_PER_LINE) {
			chunk = LOG_HEX_PER_LINE;
		}
		pos = 0;
		for (i = 0; i < chunk; i++) {
			text[pos++] = g_hex[p[off + i] >> 4];
			text[pos++] = g_hex[p[off + i] & 0x0fu];
			text[pos++] = ' ';
		}
		if (pos != 0) {
			pos--;
		}
		text[pos] = '\0';
		mtp_logf(level, "%s[%02u]: %s", label, (unsigned int)off, text);
	}
}


void
mtp_log_mask(char *out, size_t cap, const char *text)
{
	size_t	len;

	if (out == NULL || cap == 0) {
		return;
	}
	out[0] = '\0';
	if (text == NULL) {
		(void)snprintf(out, cap, "(null)");
		return;
	}
	len = strlen(text);
	if (len == 0) {
		(void)snprintf(out, cap, "(empty)");
		return;
	}
	if (len <= 2) {
		(void)snprintf(out, cap, "len=%u", (unsigned int)len);
		return;
	}
	(void)snprintf(out, cap, "len=%u ...%c%c", (unsigned int)len,
	    text[len - 2], text[len - 1]);
}

const char *
mtp_log_req_name(uint32_t kind)
{
	switch (kind) {
	case MTP_REQ_NONE: return ("none");
	case MTP_REQ_INIT: return ("initConnection");
	case MTP_REQ_SEND_CODE: return ("auth.sendCode");
	case MTP_REQ_SIGN_IN: return ("auth.signIn");
	case MTP_REQ_DIALOGS: return ("messages.getDialogs");
	case MTP_REQ_HISTORY: return ("messages.getHistory");
	case MTP_REQ_SEND_MSG: return ("messages.sendMessage");
	case MTP_REQ_READ_HISTORY: return ("messages.readHistory");
	case MTP_REQ_LOGOUT: return ("auth.logOut");
	case MTP_REQ_PING: return ("ping");
	case MTP_REQ_GET_PASSWORD: return ("account.getPassword");
	case MTP_REQ_CHECK_PASSWORD: return ("auth.checkPassword");
	case MTP_REQ_FORUM_TOPICS: return ("messages.getForumTopics");
	case MTP_REQ_TOPIC_HISTORY: return ("messages.getReplies");
	case MTP_REQ_GET_STATE: return ("updates.getState");
	case MTP_REQ_GET_DIFFERENCE: return ("updates.getDifference");
	default: return ("unknown");
	}
}

const char *
mtp_log_ctor_name(uint32_t id)
{
	const struct mtp_ctor	*ctor;

	switch (id) {
	case MTP_ID_msg_container: return ("msg_container");
	case MTP_ID_rpc_result: return ("rpc_result");
	case MTP_ID_gzip_packed: return ("gzip_packed");
	case MTP_ID_vector: return ("vector");
	default:
		break;
	}
	ctor = mtp_schema_lookup(id);
	return (ctor != NULL ? ctor->name : "unknown");
}
