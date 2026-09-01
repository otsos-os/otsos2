/* !DEFINES!

$define %type mtp_client as public nonblocking MTProto client lifecycle
$define %func mtpCreate as function with args config, kqueue, out client
$define %func mtpDestroy as procedure with args client
$define %func mtpStep as function with args client
$define %func mtpTimeout as function with args client
$define %func mtp_set_state as procedure with args client, state, reason
$define %func mtpIsReady as function with args client
$define %func mtpDcId as function with args client
$define %func mtpFloodWait as function with args client
$define %func mtpFloodRequest as function with args client
$define %func mtpResetCode as procedure with args client
$define %func mtpCheckPassword as function with args client, password
$define %func mtpPasswordNeeded as function with args client
$define %func mtpPasswordHint as function with args client
$define %func mtpPasswordBusy as function with args client
$define %func mtpSendTopicMessage as function with args client, peer, topic id, text
$define %func mtpPeerCanWrite as function with args client, peer, topic id
$define %func mtpWriteRestrictionText as function with args reason

*/

/* !SPACE!

$space %internal client_copy, client_queue, client_send, client_expire
$space %internal client_send_message, client_peer_rights
$space %export mtp_set_state
$space %export mtpCreate, mtpDestroy, mtpStep, mtpTimeout
$space %export mtpWatchFd, mtpWatchFilter, mtpState, mtpError
$space %export mtpIsAuthorized, mtpSelfId, mtpPendingCount, mtpCodeHashPresent
$space %export mtpIsReady, mtpDcId, mtpFloodWait, mtpFloodRequest, mtpResetCode
$space %export mtpSendCode, mtpSignIn, mtpLogOut
$space %export mtpGetDialogs, mtpGetHistory, mtpSendMessage, mtpReadHistory
$space %export mtpSendTopicMessage, mtpPeerCanWrite, mtpWriteRestrictionText
$space %export mtpDialogCount, mtpDialogAt, mtpHistoryCount, mtpHistoryAt
$space %export mtpHistoryPeer, mtpStrerror, mtpStateName

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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "mtp_internal.h"

#define CLIENT_HANDSHAKE_DEADLINE	MTP_HANDSHAKE_TIMEOUT_MS
#define CLIENT_DEFAULT_DEVICE		"otsos"
#define CLIENT_DEFAULT_SYSTEM		"otsos"
#define CLIENT_DEFAULT_APP		"tclient"
#define CLIENT_DEFAULT_LANG		"en"

#define CLIENT_SIGNIN_F_PHONE_CODE	(1 << 0)

void
mtp_set_state(mtp_client_t *c, int state, const char *why)
{
	if (c == NULL || c->state == state) {
		return;
	}
	mtp_logf(MTP_LOG_INFO, "state: %s -> %s (%s)", mtpStateName(c->state),
	    mtpStateName(state), why != NULL ? why : "");
	c->state = state;
}

int
mtp_rekey(mtp_client_t *c)
{
	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	if (c->rekey_done) {
		return (mtp_fail(c, MTP_ERR_AUTH, "DC%d rejected a freshly "
		    "negotiated auth key, so this is not a stale %s",
		    mtp_dc_id(c->dc_index), c->auth_path));
	}
	c->rekey_done = 1;
	mtp_logf(MTP_LOG_INFO, "rekey: DC%d does not know auth_key_id %016llx; "
	    "discarding %s and running DH again",
	    mtp_dc_id(c->dc_index), (unsigned long long)c->auth_key_id,
	    c->auth_path);
	mtp_store_forget(c);
	mtp_session_reset(c);
	c->soft_error = MTP_OK;
	c->last_error = MTP_OK;
	c->error[0] = '\0';
	return (mtp_transport_open(c));
}

static int
client_copy(char *dst, size_t cap, const char *src)
{
	size_t	len;

	if (dst == NULL || cap == 0 || src == NULL) {
		return (-1);
	}
	len = strlen(src);
	if (len >= cap) {
		return (-1);
	}
	memcpy(dst, src, len + 1);
	return (0);
}

static int
client_queue(mtp_client_t *c, uint32_t kind, const mtp_peer_t *peer,
    int32_t a, int32_t b, const char *text)
{
	if (c->queued_kind != MTP_REQ_NONE) {
		mtp_logf(MTP_LOG_ERROR, "queue: %s rejected, %s still queued",
		    mtp_log_req_name(kind), mtp_log_req_name(c->queued_kind));
		return (MTP_ERR_BUSY);
	}
	mtp_logf(MTP_LOG_INFO, "queue: %s deferred until state=ready "
	    "(state=%s)", mtp_log_req_name(kind), mtpStateName(c->state));
	c->queued_kind = kind;
	if (peer != NULL) {
		c->queued_peer = *peer;
	}
	c->queued_i32 = a;
	c->queued_i32b = b;
	if (text != NULL && client_copy(c->queued_text, sizeof(c->queued_text),
	    text) != 0) {
		c->queued_kind = MTP_REQ_NONE;
		return (MTP_ERR_INVAL);
	}
	return (MTP_OK);
}


struct client_defer {
	const mtp_peer_t	*peer;
	const char		*text;
	int32_t			a;
	int32_t			b;
};

static int
client_send(mtp_client_t *c, const uint8_t *body, size_t len, uint32_t kind,
    int64_t aux, const struct client_defer *defer)
{
	if (c->state != MTP_STATE_READY) {
		if (defer == NULL) {
			return (mtp_soft_fail(c, MTP_ERR_NOTREADY, "%s needs a "
			    "ready session, the connection is %s",
			    mtp_log_req_name(kind), mtpStateName(c->state)));
		}
		return (client_queue(c, kind, defer->peer, defer->a, defer->b,
		    defer->text));
	}
	return (mtp_api_send(c, body, len, kind, aux));
}

static int
client_auth_gate(mtp_client_t *c, uint32_t kind)
{
	uint64_t	left;

	left = mtp_flood_left(c);
	if (left != 0) {
		return (mtp_soft_fail(c, MTP_ERR_FLOOD, "%s refused: %s is rate "
		    "limited for another %u s", mtp_log_req_name(kind),
		    mtp_log_req_name(c->flood_kind),
		    (unsigned int)((left + 999u) / 1000u)));
	}
	if (c->state != MTP_STATE_READY) {
		return (mtp_soft_fail(c, MTP_ERR_NOTREADY, "%s needs a ready "
		    "session, the connection is %s", mtp_log_req_name(kind),
		    mtpStateName(c->state)));
	}
	return (MTP_OK);
}

static int
client_expire(mtp_client_t *c)
{
	uint64_t	now;
	int		i;

	now = mtp_now_ms();
	if ((c->state == MTP_STATE_CONNECT || c->state == MTP_STATE_HANDSHAKE) &&
	    now >= c->deadline) {
		return (mtp_fail(c, MTP_ERR_TIMEOUT,
		    "connection setup timed out in state %s",
		    mtpStateName(c->state)));
	}
	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (c->pending[i].in_use && now >= c->pending[i].deadline) {
			return (mtp_fail(c, MTP_ERR_TIMEOUT,
			    "%s timed out after %ums",
			    mtp_log_req_name(c->pending[i].kind),
			    (unsigned int)MTP_RPC_TIMEOUT_MS));
		}
	}
	if (c->state == MTP_STATE_READY && now >= c->next_ping &&
	    c->out_len == 0) {
		mtp_logf(MTP_LOG_DEBUG, "keepalive: ping due");
		if (mtp_send_ping(c) != MTP_OK) {
			return (c->last_error);
		}
		c->next_ping = now + MTP_PING_INTERVAL_MS;
	}
	return (MTP_OK);
}

int
mtpCreate(const mtp_config_t *cfg, int kq, mtp_client_t **out)
{
	mtp_client_t	*c;
	int		ret;

	if (out != NULL) {
		*out = NULL;
	}
	if (cfg == NULL || out == NULL || kq < 0 || cfg->api_id <= 0 ||
	    cfg->api_hash == NULL || cfg->api_hash[0] == '\0' ||
	    cfg->auth_path == NULL || cfg->auth_path[0] == '\0' ||
	    mtp_dc_address(cfg->dc_index) == 0) {
		mtp_logf(MTP_LOG_ERROR, "create: rejected config (cfg=%s out=%s "
		    "kq=%d api_id=%d api_hash=%s auth_path=%s dc=%d)",
		    cfg != NULL ? "set" : "null", out != NULL ? "set" : "null",
		    kq, cfg != NULL ? cfg->api_id : 0,
		    (cfg != NULL && cfg->api_hash != NULL &&
		    cfg->api_hash[0] != '\0') ? "set" : "missing",
		    (cfg != NULL && cfg->auth_path != NULL &&
		    cfg->auth_path[0] != '\0') ? cfg->auth_path : "missing",
		    cfg != NULL ? cfg->dc_index : -1);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_INFO, "create: api_id=%d api_hash_len=%u dc_index=%d "
	    "auth_path=%s layer=%d", cfg->api_id,
	    (unsigned int)strlen(cfg->api_hash), cfg->dc_index, cfg->auth_path,
	    mtp_schema_layer());
	c = (mtp_client_t *)calloc(1, sizeof(*c));
	if (c == NULL) {
		mtp_logf(MTP_LOG_ERROR, "create: cannot allocate %u bytes of "
		    "client state", (unsigned int)sizeof(*c));
		return (MTP_ERR_NOMEM);
	}
	c->cfg = *cfg;
	c->kq = kq;
	c->sock = -1;
	c->wait_fd = -1;
	c->dc_index = cfg->dc_index;
	c->migrate_to = -1;
	c->state = MTP_STATE_IDLE;
	if (client_copy(c->api_hash, sizeof(c->api_hash), cfg->api_hash) != 0 ||
	    client_copy(c->auth_path, sizeof(c->auth_path), cfg->auth_path) != 0 ||
	    client_copy(c->device_model, sizeof(c->device_model),
	    cfg->device_model != NULL ? cfg->device_model : CLIENT_DEFAULT_DEVICE) != 0 ||
	    client_copy(c->system_version, sizeof(c->system_version),
	    cfg->system_version != NULL ? cfg->system_version : CLIENT_DEFAULT_SYSTEM) != 0 ||
	    client_copy(c->app_version, sizeof(c->app_version),
	    cfg->app_version != NULL ? cfg->app_version : CLIENT_DEFAULT_APP) != 0 ||
	    client_copy(c->lang_code, sizeof(c->lang_code),
	    cfg->lang_code != NULL ? cfg->lang_code : CLIENT_DEFAULT_LANG) != 0) {
		mtp_logf(MTP_LOG_ERROR, "create: a config string does not fit "
		    "its fixed field (api_hash/auth_path/device/system/app/lang)");
		free(c);
		return (MTP_ERR_INVAL);
	}
	c->cfg.api_hash = c->api_hash;
	ret = mtp_store_load(c);
	if (ret != MTP_OK) {
		mtpDestroy(c);
		return (ret);
	}
	ret = mtp_transport_open(c);
	if (ret != MTP_OK) {
		mtpDestroy(c);
		return (ret);
	}
	*out = c;
	return (MTP_OK);
}

void
mtpDestroy(mtp_client_t *c)
{
	if (c == NULL) {
		return;
	}
	mtp_logf(MTP_LOG_INFO, "destroy: state=%s authorized=%d pending=%d "
	    "last_error=%d (%s)", mtpStateName(c->state), c->authorized,
	    mtpPendingCount(c), c->last_error, c->error);
	mtp_transport_close(c);
	if (c->kq >= 0) {
		mtp_unwatch(c);
	}
	if (c->in_buf != NULL) {
		lc_wipe(c->in_buf, c->in_cap);
		free(c->in_buf);
		c->in_buf = NULL;
	}
	lc_wipe(c, sizeof(*c));
	free(c);
}

int
mtpStep(mtp_client_t *c)
{
	const uint8_t	*frame;
	size_t		frame_len;
	int		ret;

	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	if (c->state == MTP_STATE_FAILED) {
		return (c->last_error);
	}
	if (client_expire(c) != MTP_OK) {
		return (c->last_error);
	}
	if (c->state == MTP_STATE_CONNECT) {
		ret = mtp_transport_check_connect(c);
		if (ret < 0) {
			return (c->last_error);
		}
		if (ret > 0) {
			if (c->auth_key_valid) {
				mtp_logf(MTP_LOG_INFO, "connect: reusing stored "
				    "auth key, skipping handshake");
				mtp_set_state(c, MTP_STATE_INIT,
				    "stored auth key");
				ret = mtp_send_init(c);
			} else {
				mtp_logf(MTP_LOG_INFO, "connect: no stored auth "
				    "key, starting DH handshake");
				ret = mtp_handshake_begin(c);
				c->deadline = mtp_now_ms() + CLIENT_HANDSHAKE_DEADLINE;
			}
			if (ret != MTP_OK) {
				return (c->last_error);
			}
		}
	}
	if (c->out_len != 0 && mtp_transport_flush(c) < 0) {
		return (c->last_error);
	}
	if (c->sock >= 0 && mtp_transport_recv(c) < 0) {
		return (c->last_error);
	}
	while ((ret = mtp_transport_take_frame(c, &frame, &frame_len)) > 0) {
		if (c->state == MTP_STATE_HANDSHAKE) {
			ret = mtp_handshake_step(c, frame, frame_len);
		} else if (c->state == MTP_STATE_INIT || c->state == MTP_STATE_READY) {
			ret = mtp_handle_frame(c, frame, frame_len);
		} else {
			ret = mtp_fail(c, MTP_ERR_PROTO,
			    "%u-byte packet arrived in state %s",
			    (unsigned int)frame_len, mtpStateName(c->state));
		}
		mtp_transport_drop_frame(c, MTP_LEN_PREFIX + frame_len);
		if (ret != MTP_OK) {
			return (c->last_error);
		}
	}
	if (ret < 0) {
		return (c->last_error);
	}
	if (c->state == MTP_STATE_INIT && c->auth_key_valid && c->out_len == 0 &&
	    mtp_pending_kind(c, MTP_REQ_INIT) == NULL) {
		if (mtp_send_init(c) != MTP_OK) {
			return (c->last_error);
		}
	}
	if (mtp_srp_busy(c)) {
		if (mtp_srp_step(c) != MTP_OK && c->state == MTP_STATE_FAILED) {
			return (c->last_error);
		}
	}

	if (c->srp.armed && mtp_srp_ready(c) && c->state == MTP_STATE_READY &&
	    mtp_pending_kind(c, MTP_REQ_CHECK_PASSWORD) == NULL) {
		c->srp.armed = 0;
		if (mtp_send_check_password(c) != MTP_OK &&
		    c->state == MTP_STATE_FAILED) {
			return (c->last_error);
		}
	}
	if (c->state == MTP_STATE_READY && c->authorized && c->out_len == 0 &&
	    mtp_flood_left(c) == 0) {
		if (!c->upd_have_state) {
			if (mtp_send_get_state(c) != MTP_OK &&
			    c->state == MTP_STATE_FAILED) {
				return (c->last_error);
			}
		} else if (c->upd_need_difference) {
			if (mtp_send_get_difference(c) != MTP_OK &&
			    c->state == MTP_STATE_FAILED) {
				return (c->last_error);
			}
		}
	}
	if (c->state == MTP_STATE_READY && c->queued_kind != MTP_REQ_NONE &&
	    c->out_len == 0) {
		if (mtp_run_queued(c) != MTP_OK) {
			return (c->last_error);
		}
	}
	if (c->out_len != 0 && mtp_transport_flush(c) < 0) {
		return (c->last_error);
	}
	if (c->soft_error != MTP_OK) {
		ret = c->soft_error;
		c->soft_error = MTP_OK;
		return (ret);
	}
	return (MTP_OK);
}

int
mtpTimeout(mtp_client_t *c)
{
	uint64_t	deadline, now;
	int		i;

	if (c == NULL || c->state == MTP_STATE_FAILED) {
		return (-1);
	}
	if (mtp_srp_busy(c)) {
		return (0);
	}
	deadline = 0;
	if (c->state == MTP_STATE_CONNECT || c->state == MTP_STATE_HANDSHAKE) {
		deadline = c->deadline;
	}
	for (i = 0; i < MTP_MAX_PENDING; i++) {
		if (c->pending[i].in_use && (deadline == 0 ||
		    c->pending[i].deadline < deadline)) {
			deadline = c->pending[i].deadline;
		}
	}

	if (c->state == MTP_STATE_READY && c->out_len == 0 && (deadline == 0 ||
	    c->next_ping < deadline)) {
		deadline = c->next_ping;
	}
	if (deadline == 0) {
		return (-1);
	}
	now = mtp_now_ms();
	if (now >= deadline) {
		return (0);
	}
	return (deadline - now > 0x7fffffffU ? 0x7fffffff :
	    (int)(deadline - now));
}

int
mtpWatchFd(const mtp_client_t *c)
{
	return (c == NULL ? -1 : c->wait_fd);
}

int
mtpWatchFilter(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->wait_filter);
}

int
mtpState(const mtp_client_t *c)
{
	return (c == NULL ? MTP_STATE_FAILED : c->state);
}

const char *
mtpError(const mtp_client_t *c)
{
	return (c == NULL ? "invalid MTProto client" : c->error);
}

int
mtpIsAuthorized(const mtp_client_t *c)
{
	return (c != NULL && c->authorized);
}

int64_t
mtpSelfId(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->self_id);
}

int
mtpPendingCount(const mtp_client_t *c)
{
	int	count, i;

	if (c == NULL) {
		return (0);
	}
	count = 0;
	for (i = 0; i < MTP_MAX_PENDING; i++) {
		count += c->pending[i].in_use != 0;
	}
	return (count);
}

int
mtpCodeHashPresent(const mtp_client_t *c)
{
	return (c != NULL && c->code_hash[0] != '\0');
}

int
mtpIsReady(const mtp_client_t *c)
{
	return (c != NULL && c->state == MTP_STATE_READY &&
	    mtp_flood_left(c) == 0);
}

int
mtpDcId(const mtp_client_t *c)
{
	return (c == NULL ? 0 : mtp_dc_id(c->dc_index));
}

int
mtpFloodWait(const mtp_client_t *c)
{
	uint64_t	left;

	left = mtp_flood_left(c);
	return (left == 0 ? 0 : (int)((left + 999u) / 1000u));
}

const char *
mtpFloodRequest(const mtp_client_t *c)
{
	if (c == NULL || mtp_flood_left(c) == 0) {
		return ("");
	}
	return (mtp_log_req_name(c->flood_kind));
}

void
mtpResetCode(mtp_client_t *c)
{
	if (c == NULL) {
		return;
	}
	mtp_logf(MTP_LOG_INFO, "resetCode: forgetting the phone_code_hash, "
	    "login restarts at the phone step");
	c->code_hash[0] = '\0';
	mtp_srp_reset(c);
	c->password_needed = 0;
}

int
mtpReconnect(mtp_client_t *c)
{
	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_INFO, "reconnect: from %s (last_error=%d), auth key %s",
	    mtpStateName(c->state), c->last_error,
	    c->auth_key_valid ? "kept" : "absent");
	c->last_error = MTP_OK;
	c->soft_error = MTP_OK;
	c->error[0] = '\0';
	mtp_session_reset(c);
	return (mtp_transport_open(c));
}

int
mtpSendCode(mtp_client_t *c, const char *phone)
{
	mtp_writer_t	w;
	uint8_t		body[512];
	int		ret;

	if (c == NULL || phone == NULL || phone[0] == '\0' || c->cfg.api_id <= 0) {
		mtp_logf(MTP_LOG_ERROR, "sendCode: invalid argument (client=%s "
		    "phone=%s api_id=%d)", c != NULL ? "set" : "null",
		    (phone != NULL && phone[0] != '\0') ? "set" : "empty",
		    c != NULL ? c->cfg.api_id : 0);
		return (MTP_ERR_INVAL);
	}
	if (client_copy(c->phone, sizeof(c->phone), phone) != 0) {
		mtp_logf(MTP_LOG_ERROR, "sendCode: phone longer than %u bytes",
		    (unsigned int)sizeof(c->phone) - 1);
		return (MTP_ERR_INVAL);
	}
	{
		char	masked[MTP_LOG_MASK_MAX];

		mtp_log_mask(masked, sizeof(masked), phone);
		mtp_logf(MTP_LOG_INFO, "sendCode: phone %s", masked);
	}
	ret = client_auth_gate(c, MTP_REQ_SEND_CODE);
	if (ret != MTP_OK) {
		return (ret);
	}
	if (mtp_pending_kind(c, MTP_REQ_SEND_CODE) != NULL) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "auth.sendCode is already "
		    "in flight, waiting for its answer"));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_auth_sendCode);
	mtp_write_string(&w, phone);
	mtp_write_i32(&w, c->cfg.api_id);
	mtp_write_string(&w, c->api_hash);
	mtp_write_u32(&w, MTP_ID_codeSettings);
	mtp_write_i32(&w, 0);
	ret = w.overflow ? MTP_ERR_INVAL : mtp_api_send(c, body, w.len,
	    MTP_REQ_SEND_CODE, 0);
	return (ret);
}

int
mtpSignIn(mtp_client_t *c, const char *phone, const char *code)
{
	mtp_writer_t	w;
	uint8_t		body[512];
	int		ret;

	if (c == NULL || phone == NULL || code == NULL || phone[0] == '\0' ||
	    code[0] == '\0') {
		mtp_logf(MTP_LOG_ERROR, "signIn: invalid argument (client=%s "
		    "phone=%s code=%s)", c != NULL ? "set" : "null",
		    (phone != NULL && phone[0] != '\0') ? "set" : "empty",
		    (code != NULL && code[0] != '\0') ? "set" : "empty");
		return (MTP_ERR_INVAL);
	}
	if (client_copy(c->phone, sizeof(c->phone), phone) != 0) {
		mtp_logf(MTP_LOG_ERROR, "signIn: phone longer than %u bytes",
		    (unsigned int)sizeof(c->phone) - 1);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_INFO, "signIn: code_len=%u code_hash=%s",
	    (unsigned int)strlen(code),
	    c->code_hash[0] != '\0' ? "present" : "absent");
	ret = client_auth_gate(c, MTP_REQ_SIGN_IN);
	if (ret != MTP_OK) {
		return (ret);
	}
	if (c->code_hash[0] == '\0') {
		return (mtp_soft_fail(c, MTP_ERR_AUTH, "auth.signIn needs a "
		    "phone_code_hash; auth.sendCode has to complete first"));
	}
	if (mtp_pending_kind(c, MTP_REQ_SIGN_IN) != NULL) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "auth.signIn is already "
		    "in flight, waiting for its answer"));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_auth_signIn);
	mtp_write_i32(&w, CLIENT_SIGNIN_F_PHONE_CODE);
	mtp_write_string(&w, phone);
	mtp_write_string(&w, c->code_hash);
	mtp_write_string(&w, code);
	ret = w.overflow ? MTP_ERR_INVAL : mtp_api_send(c, body, w.len,
	    MTP_REQ_SIGN_IN, 0);
	return (ret);
}


int
mtpCheckPassword(mtp_client_t *c, const char *password)
{
	int	ret;

	if (c == NULL || password == NULL || password[0] == '\0') {
		mtp_logf(MTP_LOG_ERROR, "checkPassword: invalid argument "
		    "(client=%s password=%s)", c != NULL ? "set" : "null",
		    (password != NULL && password[0] != '\0') ? "set" : "empty");
		return (MTP_ERR_INVAL);
	}
	ret = client_auth_gate(c, MTP_REQ_CHECK_PASSWORD);
	if (ret != MTP_OK) {
		return (ret);
	}
	if (mtp_pending_kind(c, MTP_REQ_CHECK_PASSWORD) != NULL) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "auth.checkPassword is "
		    "already in flight, waiting for its answer"));
	}
	if (mtp_pending_kind(c, MTP_REQ_GET_PASSWORD) != NULL) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "waiting for fresh SRP "
		    "parameters from the DC"));
	}
	if (mtp_srp_busy(c)) {
		return (mtp_soft_fail(c, MTP_ERR_BUSY, "still deriving the proof "
		    "for the previous attempt"));
	}
	mtp_logf(MTP_LOG_INFO, "checkPassword: password_len=%u, starting the SRP "
	    "derivation", (unsigned int)strlen(password));
	ret = mtp_srp_begin(c, password);
	if (ret != MTP_OK) {
		return (ret);
	}
	c->srp.armed = 1;
	return (MTP_OK);
}

int
mtpPasswordNeeded(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return (c->password_needed != 0 && c->authorized == 0);
}

const char *
mtpPasswordHint(const mtp_client_t *c)
{
	return (mtp_srp_hint(c));
}


int
mtpPasswordBusy(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	if (c->srp.armed || mtp_srp_busy(c) || mtp_srp_ready(c)) {
		return (1);
	}
	return (mtp_pending_active(c, MTP_REQ_CHECK_PASSWORD));
}


int
mtpPasswordReady(const mtp_client_t *c)
{
	if (c == NULL) {
		return (0);
	}
	return (mtp_srp_have_params(c) &&
	    !mtp_pending_active(c, MTP_REQ_GET_PASSWORD));
}

int
mtpLogOut(mtp_client_t *c)
{
	mtp_writer_t	w;
	uint8_t		body[4];
	int		ret;

	if (c == NULL) {
		return (MTP_ERR_INVAL);
	}
	ret = client_auth_gate(c, MTP_REQ_LOGOUT);
	if (ret != MTP_OK) {
		return (ret);
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_auth_logOut);
	return (mtp_api_send(c, body, w.len, MTP_REQ_LOGOUT, 0));
}

int
mtpGetDialogs(mtp_client_t *c, int limit)
{
	mtp_peer_t	empty = { MTP_PEER_EMPTY, 0, 0 };
	mtp_writer_t	w;
	uint8_t		body[64];

	if (c == NULL || limit < 1 || limit > MTP_MAX_DIALOGS) {
		mtp_logf(MTP_LOG_ERROR, "getDialogs: limit %d outside 1..%d",
		    limit, MTP_MAX_DIALOGS);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_DEBUG, "getDialogs: limit=%d", limit);
	if (c->state != MTP_STATE_READY) {
		return (client_queue(c, MTP_REQ_DIALOGS, NULL, limit, 0, NULL));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_getDialogs);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_peer(&w, &empty);
	mtp_write_i32(&w, limit);
	mtp_write_i64(&w, 0);
	{
		struct client_defer	defer;

		defer.peer = NULL;
		defer.text = NULL;
		defer.a = limit;
		defer.b = 0;
		return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
		    MTP_REQ_DIALOGS, 0, &defer));
	}
}

int
mtpGetHistory(mtp_client_t *c, const mtp_peer_t *peer, int limit,
    int32_t offset_id)
{
	mtp_writer_t	w;
	uint8_t		body[64];

	if (c == NULL || peer == NULL || peer->kind == MTP_PEER_EMPTY ||
	    limit < 1 || limit > MTP_MAX_HISTORY) {
		mtp_logf(MTP_LOG_ERROR, "getHistory: invalid argument "
		    "(peer_kind=%d limit=%d)",
		    peer != NULL ? (int)peer->kind : -1, limit);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_DEBUG, "getHistory: peer kind=%d id=%lld limit=%d "
	    "offset_id=%d", (int)peer->kind, (long long)peer->id, limit,
	    offset_id);
	if (c->state != MTP_STATE_READY) {
		return (client_queue(c, MTP_REQ_HISTORY, peer, limit, offset_id, NULL));
	}
	c->history_peer = *peer;
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_getHistory);
	mtp_write_peer(&w, peer);
	mtp_write_i32(&w, offset_id);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, limit);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i64(&w, 0);
	{
		struct client_defer	defer;

		defer.peer = peer;
		defer.text = NULL;
		defer.a = limit;
		defer.b = offset_id;
		return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
		    MTP_REQ_HISTORY, peer->id, &defer));
	}
}

int
mtpGetForumTopics(mtp_client_t *c, const mtp_peer_t *peer, int limit)
{
	mtp_writer_t w;
	uint8_t body[96];

	if (c == NULL || peer == NULL || peer->kind != MTP_PEER_CHANNEL ||
	    peer->id <= 0 || peer->access_hash == 0 || limit < 1 ||
	    limit > MTP_MAX_FORUM_TOPICS) {
		mtp_logf(MTP_LOG_ERROR, "getForumTopics: invalid peer or limit "
		    "(kind=%d id=%lld hash=%s limit=%d)",
		    peer != NULL ? (int)peer->kind : -1,
		    peer != NULL ? (long long)peer->id : 0,
		    peer != NULL && peer->access_hash != 0 ? "present" : "absent",
		    limit);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_DEBUG, "getForumTopics: peer id=%lld limit=%d",
	    (long long)peer->id, limit);
	if (c->state != MTP_STATE_READY) {
		return (client_queue(c, MTP_REQ_FORUM_TOPICS, peer, limit, 0, NULL));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_getForumTopics);
	mtp_write_i32(&w, 0);
	mtp_write_peer(&w, peer);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, limit);
	{
		struct client_defer	defer;

		defer.peer = peer;
		defer.text = NULL;
		defer.a = limit;
		defer.b = 0;
		return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
		    MTP_REQ_FORUM_TOPICS, peer->id, &defer));
	}
}

int
mtpGetTopicHistory(mtp_client_t *c, const mtp_peer_t *peer, int32_t topic_id,
    int limit)
{
	mtp_writer_t w;
	uint8_t body[80];

	if (c == NULL || peer == NULL || peer->kind != MTP_PEER_CHANNEL ||
	    topic_id <= 0 || limit < 1 || limit > MTP_MAX_HISTORY) return (MTP_ERR_INVAL);
	if (c->state != MTP_STATE_READY) return (MTP_ERR_NOTREADY);
	c->history_peer = *peer;
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_getReplies);
	mtp_write_peer(&w, peer);
	mtp_write_i32(&w, topic_id);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, limit);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, 0);
	mtp_write_i64(&w, 0);
	return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
	    MTP_REQ_TOPIC_HISTORY, peer->id, NULL));
}


#define CLIENT_SEND_F_REPLY_TO		(1u << 0)
#define CLIENT_REPLY_F_TOP_MSG_ID	(1u << 0)

static int
client_send_message(mtp_client_t *c, const mtp_peer_t *peer, int32_t topic_id,
    const char *text)
{
	mtp_writer_t	w;
	uint8_t		body[MTP_MAX_TEXT + 128];
	int64_t		random_id;

	if (c == NULL || peer == NULL || peer->kind == MTP_PEER_EMPTY ||
	    text == NULL || text[0] == '\0' || strlen(text) >= MTP_MAX_TEXT ||
	    topic_id < 0) {
		mtp_logf(MTP_LOG_ERROR, "sendMessage: invalid argument "
		    "(peer_kind=%d topic=%d text_len=%u max=%u)",
		    peer != NULL ? (int)peer->kind : -1, (int)topic_id,
		    text != NULL ? (unsigned int)strlen(text) : 0,
		    (unsigned int)MTP_MAX_TEXT - 1);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_DEBUG, "sendMessage: peer kind=%d id=%lld topic=%d "
	    "text_len=%u", (int)peer->kind, (long long)peer->id, (int)topic_id,
	    (unsigned int)strlen(text));
	if (c->state != MTP_STATE_READY) {
		return (client_queue(c, MTP_REQ_SEND_MSG, peer, topic_id, 0,
		    text));
	}
	random_id = mtp_next_msg_id(c);
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_sendMessage);
	mtp_write_i32(&w, topic_id != 0 ? (int32_t)CLIENT_SEND_F_REPLY_TO : 0);
	mtp_write_peer(&w, peer);
	if (topic_id != 0) {
		mtp_write_u32(&w, MTP_ID_inputReplyToMessage);
		mtp_write_i32(&w, (int32_t)CLIENT_REPLY_F_TOP_MSG_ID);
		mtp_write_i32(&w, topic_id);
		mtp_write_i32(&w, topic_id);
	}
	mtp_write_string(&w, text);
	mtp_write_i64(&w, random_id);
	{
		struct client_defer	defer;

		defer.peer = peer;
		defer.text = text;
		defer.a = topic_id;
		defer.b = 0;
		return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
		    MTP_REQ_SEND_MSG, peer->id, &defer));
	}
}

int
mtpSendMessage(mtp_client_t *c, const mtp_peer_t *peer, const char *text)
{
	return (client_send_message(c, peer, 0, text));
}

int
mtpSendTopicMessage(mtp_client_t *c, const mtp_peer_t *peer, int32_t topic_id,
    const char *text)
{
	if (peer == NULL || peer->kind != MTP_PEER_CHANNEL || topic_id <= 0) {
		mtp_logf(MTP_LOG_ERROR, "sendTopicMessage: forum topics exist "
		    "only on channels (kind=%d topic=%d)",
		    peer != NULL ? (int)peer->kind : -1, (int)topic_id);
		return (MTP_ERR_INVAL);
	}
	return (client_send_message(c, peer, topic_id, text));
}

int
mtpReadHistory(mtp_client_t *c, const mtp_peer_t *peer, int32_t max_id)
{
	mtp_writer_t	w;
	uint8_t		body[32];

	if (c == NULL || peer == NULL || peer->kind == MTP_PEER_EMPTY || max_id < 0) {
		mtp_logf(MTP_LOG_ERROR, "readHistory: invalid argument "
		    "(peer_kind=%d max_id=%d)",
		    peer != NULL ? (int)peer->kind : -1, max_id);
		return (MTP_ERR_INVAL);
	}
	mtp_logf(MTP_LOG_DEBUG, "readHistory: peer kind=%d id=%lld max_id=%d",
	    (int)peer->kind, (long long)peer->id, max_id);
	if (c->state != MTP_STATE_READY) {
		return (client_queue(c, MTP_REQ_READ_HISTORY, peer, max_id, 0, NULL));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_messages_readHistory);
	mtp_write_peer(&w, peer);
	mtp_write_i32(&w, max_id);
	{
		struct client_defer	defer;

		defer.peer = peer;
		defer.text = NULL;
		defer.a = max_id;
		defer.b = 0;
		return (w.overflow ? MTP_ERR_INVAL : client_send(c, body, w.len,
		    MTP_REQ_READ_HISTORY, peer->id, &defer));
	}
}

static const mtp_peer_name_t *
client_find_name(const mtp_client_t *c, const mtp_peer_t *peer)
{
	int i;

	if (c == NULL || peer == NULL) return (NULL);
	for (i = 0; i < c->name_count; i++) {
		if (c->names[i].kind == peer->kind && c->names[i].id == peer->id)
			return (&c->names[i]);
	}
	return (NULL);
}

int
mtpForumTopicCount(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->forum_topic_count);
}

const mtp_forum_topic_t *
mtpForumTopicAt(const mtp_client_t *c, int index)
{
	return (c == NULL || index < 0 || index >= c->forum_topic_count ? NULL :
	    &c->forum_topics[index]);
}

int
mtpPeerIsForum(const mtp_client_t *c, const mtp_peer_t *peer)
{
	const mtp_peer_name_t *n;

	if (c == NULL || peer == NULL) return (0);
	n = client_find_name(c, peer);
	return (n != NULL && n->forum);
}

static const mtp_forum_topic_t *
client_find_topic(const mtp_client_t *c, int32_t topic_id)
{
	int	i;

	if (topic_id <= 0) {
		return (NULL);
	}
	for (i = 0; i < c->forum_topic_count; i++) {
		if (c->forum_topics[i].id == topic_id) {
			return (&c->forum_topics[i]);
		}
	}
	return (NULL);
}

int
mtpPeerCanWrite(const mtp_client_t *c, const mtp_peer_t *peer,
    int32_t topic_id)
{
	const mtp_forum_topic_t		*topic;
	const mtp_peer_name_t		*n;
	const mtp_peer_rights_t		*p;

	if (c == NULL || peer == NULL || peer->kind == MTP_PEER_EMPTY) {
		return (MTP_WRITE_NO_PEER);
	}
	if (peer->kind == MTP_PEER_USER) {
		return (MTP_WRITE_OK);
	}
	n = client_find_name(c, peer);
	if (n == NULL || !n->rights.known) {
		return (MTP_WRITE_UNKNOWN);
	}
	p = &n->rights;
	if (p->forbidden) {
		return (MTP_WRITE_FORBIDDEN);
	}
	if (p->left) {
		return (MTP_WRITE_LEFT);
	}
	if (p->deactivated) {
		return (MTP_WRITE_DEACTIVATED);
	}
	if (p->creator) {
		return (MTP_WRITE_OK);
	}

	if (p->banned) {
		return (MTP_WRITE_BANNED);
	}
	if (p->broadcast) {
		return (p->can_post ? MTP_WRITE_OK : MTP_WRITE_BROADCAST);
	}
	if (p->restricted) {
		return (MTP_WRITE_RESTRICTED);
	}
	topic = client_find_topic(c, topic_id);
	if (topic != NULL && topic->closed && !p->manage_topics) {
		return (MTP_WRITE_TOPIC_CLOSED);
	}
	return (MTP_WRITE_OK);
}

const char *
mtpWriteRestrictionText(int reason)
{
	switch (reason) {
	case MTP_WRITE_OK:
	case MTP_WRITE_UNKNOWN:
		return ("");
	case MTP_WRITE_NO_PEER:
		return ("Choose a chat first");
	case MTP_WRITE_FORBIDDEN:
		return ("You are not a member of this chat");
	case MTP_WRITE_LEFT:
		return ("You left this chat");
	case MTP_WRITE_DEACTIVATED:
		return ("This group is deactivated");
	case MTP_WRITE_BROADCAST:
		return ("Only admins can post in this channel");
	case MTP_WRITE_BANNED:
		return ("You are restricted from sending messages here");
	case MTP_WRITE_RESTRICTED:
		return ("Sending messages is not allowed in this group");
	case MTP_WRITE_TOPIC_CLOSED:
		return ("This topic is closed");
	default:
		return ("You cannot send messages here");
	}
}

const char *
mtpPeerPresence(const mtp_client_t *c, const mtp_peer_t *peer)
{
	const mtp_peer_name_t *n;
	static char result[64];

	if (c == NULL || peer == NULL || peer->kind != MTP_PEER_USER) return (NULL);
	n = client_find_name(c, peer);
	if (n == NULL) return (NULL);
	if (n->presence == 1) return ("online");
	if (n->presence != 2 || n->last_seen <= 0) return ("last seen recently");
	snprintf(result, sizeof(result), "last seen %d", n->last_seen);
	return (result);
}

uint32_t
mtpUpdateVersion(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->update_version);
}

int
mtpDialogCount(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->dialog_count);
}

const mtp_dialog_t *
mtpDialogAt(const mtp_client_t *c, int index)
{
	return (c == NULL || index < 0 || index >= c->dialog_count ? NULL :
	    &c->dialogs[index]);
}

int
mtpHistoryCount(const mtp_client_t *c)
{
	return (c == NULL ? 0 : c->history_count);
}

const mtp_message_t *
mtpHistoryAt(const mtp_client_t *c, int index)
{
	return (c == NULL || index < 0 || index >= c->history_count ? NULL :
	    &c->history[index]);
}

const mtp_peer_t *
mtpHistoryPeer(const mtp_client_t *c)
{
	return (c == NULL || c->history_peer.kind == MTP_PEER_EMPTY ? NULL :
	    &c->history_peer);
}

const char *
mtpStrerror(int err)
{
	static const char	*const text[] = {
		"success", "invalid argument", "out of memory", "network failure",
		"protocol failure", "cryptographic failure", "authorization failure",
		"RPC failure", "client busy", "timeout", "storage failure",
		"unsupported operation", "rate limited", "session not ready"
	};

	return (err <= 0 && err >= MTP_ERR_NOTREADY ? text[-err] :
	    "unknown MTProto error");
}

const char *
mtpStateName(int state)
{
	switch (state) {
	case MTP_STATE_IDLE: return ("idle");
	case MTP_STATE_CONNECT: return ("connecting");
	case MTP_STATE_HANDSHAKE: return ("authorizing key");
	case MTP_STATE_INIT: return ("initializing");
	case MTP_STATE_READY: return ("ready");
	case MTP_STATE_FAILED: return ("failed");
	default: return ("unknown");
	}
}
