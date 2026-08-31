/* !DEFINES!

$define %type mtp_api as Telegram API request and response layer
$define %func mtp_send_init as function with args client
$define %func mtp_dispatch_result as function with args client, pending, reader
$define %func mtp_dispatch_error as function with args client, pending, int32_t, message
$define %func mtp_run_queued as function with args client
$define %func mtp_send_ping as function with args client
$define %func mtp_resend_last as function with args client, message id
$define %func mtp_send_get_password as function with args client
$define %func mtp_send_check_password as function with args client

*/

/* !SPACE!

$space %internal api_send, api_parse_peer, api_parse_cache, api_parse_messages
$space %internal api_parse_dialogs, api_decode_collection, api_flood_seconds
$space %internal api_take_authorization, api_cache_find, api_cache_kind
$space %internal api_row_failed
$space %export mtp_send_init, mtp_dispatch_result, mtp_dispatch_error
$space %export mtp_run_queued, mtp_send_ping, mtp_resend_last
$space %export mtp_send_get_password, mtp_send_check_password

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

#include <string.h>

#include "mtp_internal.h"

#define API_FLOOD_MAX_SEC	(24 * 60 * 60)
#define API_FLOOD_DEFAULT_SEC	30

static const char *const api_flood_prefix[] = {
	"FLOOD_WAIT_",
	"FLOOD_PREMIUM_WAIT_",
	"FLOOD_TEST_PHONE_WAIT_",
	"SLOWMODE_WAIT_",
	"TAKEOUT_INIT_DELAY_",
	"2FA_CONFIRM_WAIT_",
	"PHONE_PASSWORD_FLOOD_"
};

#define API_FLOOD_PREFIX_COUNT \
	(sizeof(api_flood_prefix) / sizeof(api_flood_prefix[0]))

static int
api_flood_seconds(int32_t code, const char *message)
{
	const char	*digits;
	size_t		len, i;
	long		value;

	if (message == NULL || message[0] == '\0') {
		return (code == 420 ? API_FLOOD_DEFAULT_SEC : 0);
	}
	for (i = 0; i < API_FLOOD_PREFIX_COUNT; i++) {
		len = strlen(api_flood_prefix[i]);
		if (strncmp(message, api_flood_prefix[i], len) != 0) {
			continue;
		}
		digits = message + len;
		if (*digits < '0' || *digits > '9') {
			return (API_FLOOD_DEFAULT_SEC);
		}
		value = 0;
		while (*digits >= '0' && *digits <= '9') {
			if (value > API_FLOOD_MAX_SEC) {
				break;
			}
			value = value * 10 + (*digits - '0');
			digits++;
		}
		if (value > API_FLOOD_MAX_SEC) {
			value = API_FLOOD_MAX_SEC;
		}
		return (value < 1 ? 1 : (int)value);
	}
	return (code == 420 ? API_FLOOD_DEFAULT_SEC : 0);
}

static int
api_parse_peer(mtp_reader_t *r, mtp_peer_t *peer)
{
	mtp_object_t	o;

	memset(peer, 0, sizeof(*peer));
	if (mtp_object_parse(r, &o) != 0) {
		return (-1);
	}
	if (strcmp(o.ctor->name, "peerUser") == 0) {
		peer->kind = MTP_PEER_USER;
		peer->id = mtp_object_i64(&o, "user_id", 0);
	} else if (strcmp(o.ctor->name, "peerChat") == 0) {
		peer->kind = MTP_PEER_CHAT;
		peer->id = mtp_object_i64(&o, "chat_id", 0);
	} else if (strcmp(o.ctor->name, "peerChannel") == 0) {
		peer->kind = MTP_PEER_CHANNEL;
		peer->id = mtp_object_i64(&o, "channel_id", 0);
	} else {
		return (-1);
	}
	return (peer->id == 0 ? -1 : 0);
}

static const mtp_peer_name_t *
api_cache_find(const mtp_client_t *c, const mtp_peer_t *peer)
{
	int	i;

	for (i = 0; i < c->name_count; i++) {
		if (c->names[i].kind == peer->kind && c->names[i].id == peer->id) {
			return (&c->names[i]);
		}
	}
	return (NULL);
}


struct api_peer_ctor {
	const char	*name;
	int32_t		kind;
	int		is_user;
	const char	*title_field;
};

static const struct api_peer_ctor api_peer_ctors[] = {
	{ "chat",		MTP_PEER_CHAT,		0, "title" },
	{ "chatForbidden",	MTP_PEER_CHAT,		0, "title" },
	{ "chatEmpty",		MTP_PEER_CHAT,		0, NULL },
	{ "channel",		MTP_PEER_CHANNEL,	0, "title" },
	{ "channelForbidden",	MTP_PEER_CHANNEL,	0, "title" },
	{ "user",		MTP_PEER_USER,		1, "first_name" },
	{ "userEmpty",		MTP_PEER_USER,		1, NULL }
};

#define API_PEER_CTOR_COUNT \
	(sizeof(api_peer_ctors) / sizeof(api_peer_ctors[0]))

static const struct api_peer_ctor *
api_cache_kind(const char *ctor, int users)
{
	size_t	i;

	for (i = 0; i < API_PEER_CTOR_COUNT; i++) {
		if (api_peer_ctors[i].is_user == users &&
		    strcmp(api_peer_ctors[i].name, ctor) == 0) {
			return (&api_peer_ctors[i]);
		}
	}
	return (NULL);
}


static void
api_row_failed(const mtp_reader_t *r, const char *what, uint32_t index,
    uint32_t count)
{
	char	why[MTP_MAX_ERROR];

	(void)mtp_reader_explain(r, why, sizeof(why));
	mtp_logf(MTP_LOG_ERROR, "%s: row %u of %u does not parse: %s", what,
	    (unsigned int)index + 1, (unsigned int)count, why);
}

static int
api_parse_cache(mtp_client_t *c, const mtp_object_t *root, const char *field,
    int users)
{
	const struct api_peer_ctor	*ck;
	mtp_peer_name_t			*n;
	mtp_reader_t			r;
	mtp_object_t			o;
	uint32_t			count, i;

	if (mtp_object_vector(root, field, &r, &count) != 0) {
		return (-1);
	}
	for (i = 0; i < count; i++) {
		if (mtp_object_parse(&r, &o) != 0) {
			api_row_failed(&r, field, i, count);
			return (-1);
		}
		if (c->name_count == MTP_MAX_PEER_CACHE) {
			mtp_logf(MTP_LOG_INFO, "%s: peer cache full at %d, %u "
			    "row(s) keep their id but lose their name", field,
			    MTP_MAX_PEER_CACHE, (unsigned int)(count - i));
			break;
		}
		ck = api_cache_kind(o.ctor->name, users);
		if (ck == NULL) {
			mtp_logf(MTP_LOG_DEBUG, "%s: skipping %s", field,
			    o.ctor->name);
			continue;
		}
		n = &c->names[c->name_count];
		memset(n, 0, sizeof(*n));
		n->kind = ck->kind;
		n->id = mtp_object_i64(&o, "id", 0);
		n->access_hash = mtp_object_i64(&o, "access_hash", 0);
		if (ck->title_field != NULL) {
			(void)mtp_object_str(&o, ck->title_field, n->title,
			    sizeof(n->title));
		}
		if (n->title[0] == '\0' && ck->is_user) {
			(void)mtp_object_str(&o, "username", n->title,
			    sizeof(n->title));
			if (n->title[0] == '\0') {
				(void)mtp_object_str(&o, "last_name", n->title,
				    sizeof(n->title));
			}
		}
		if (n->id == 0) {
			mtp_logf(MTP_LOG_DEBUG, "%s: %s carries id 0, skipped",
			    field, o.ctor->name);
			continue;
		}
		c->name_count++;
	}
	return (r.error ? -1 : 0);
}

static int
api_parse_dialogs(mtp_client_t *c, const mtp_object_t *root)
{
	const mtp_peer_name_t	*n;
	mtp_reader_t		r, peer_r;
	mtp_object_t		o;
	mtp_dialog_t		*d;
	uint32_t		count, i, skipped;

	if (mtp_object_vector(root, "dialogs", &r, &count) != 0) {
		return (-1);
	}
	c->dialog_count = 0;
	skipped = 0;
	for (i = 0; i < count && c->dialog_count < MTP_MAX_DIALOGS; i++) {
		if (mtp_object_parse(&r, &o) != 0) {
			api_row_failed(&r, "dialogs", i, count);
			return (-1);
		}
		if (strcmp(o.ctor->name, "dialog") != 0) {
			mtp_logf(MTP_LOG_DEBUG, "dialogs: skipping %s",
			    o.ctor->name);
			skipped++;
			continue;
		}
		d = &c->dialogs[c->dialog_count];
		memset(d, 0, sizeof(*d));

		if (mtp_object_at(&o, "peer", &peer_r) != 0 ||
		    api_parse_peer(&peer_r, &d->peer) != 0) {
			mtp_logf(MTP_LOG_DEBUG, "dialogs: row %u of %u has no "
			    "usable peer, skipped", (unsigned int)i + 1,
			    (unsigned int)count);
			skipped++;
			continue;
		}
		d->top_message = mtp_object_i32(&o, "top_message", 0);
		d->unread_count = mtp_object_i32(&o, "unread_count", 0);
		d->pinned = mtp_object_has(&o, "pinned");
		n = api_cache_find(c, &d->peer);
		if (n != NULL) {
			d->peer.access_hash = n->access_hash;
			memcpy(d->title, n->title, sizeof(d->title));
		}
		c->dialog_count++;
	}
	if (skipped != 0) {
		mtp_logf(MTP_LOG_INFO, "dialogs: %u of %u row(s) skipped, %d "
		    "kept", (unsigned int)skipped, (unsigned int)count,
		    c->dialog_count);
	}
	return (r.error ? -1 : 0);
}

static int
api_parse_messages(mtp_client_t *c, const mtp_object_t *root)
{
	const mtp_peer_name_t	*n;
	mtp_reader_t		r, peer_r;
	mtp_object_t		o;
	mtp_message_t		*m;
	mtp_peer_t		peer;
	uint32_t		count, i;

	if (mtp_object_vector(root, "messages", &r, &count) != 0) {
		return (-1);
	}
	c->history_count = 0;
	for (i = 0; i < count && c->history_count < MTP_MAX_HISTORY; i++) {
		if (mtp_object_parse(&r, &o) != 0) {
			api_row_failed(&r, "messages", i, count);
			return (-1);
		}
		if (strcmp(o.ctor->name, "message") != 0 &&
		    strcmp(o.ctor->name, "messageService") != 0) {
			mtp_logf(MTP_LOG_DEBUG, "messages: skipping %s",
			    o.ctor->name);
			continue;
		}
		m = &c->history[c->history_count];
		memset(m, 0, sizeof(*m));
		m->id = mtp_object_i32(&o, "id", 0);
		m->date = mtp_object_i32(&o, "date", 0);
		m->out = mtp_object_has(&o, "out");
		m->service = strcmp(o.ctor->name, "messageService") == 0;
		(void)mtp_object_str(&o, "message", m->text, sizeof(m->text));
		if (mtp_object_at(&o, "from_id", &peer_r) == 0 &&
		    api_parse_peer(&peer_r, &peer) == 0) {
			m->from_id = peer.id;
			n = api_cache_find(c, &peer);
			if (n != NULL) {
				memcpy(m->author, n->title, sizeof(m->author));
			}
		}
		c->history_count++;
	}
	return (r.error ? -1 : 0);
}


static int
api_decode_collection(mtp_client_t *c, mtp_pending_t *p, mtp_object_t *o)
{
	const char	*stage;
	int		ret;

	stage = "chats";
	ret = api_parse_cache(c, o, "chats", 0);
	if (ret == 0) {
		stage = "users";
		ret = api_parse_cache(c, o, "users", 1);
	}
	if (ret == 0) {
		if (p->kind == MTP_REQ_DIALOGS) {
			stage = "dialogs";
			ret = api_parse_dialogs(c, o);
		} else {
			stage = "messages";
			ret = api_parse_messages(c, o);
		}
	}
	if (ret != 0) {
		mtp_logf(MTP_LOG_ERROR, "%s: the %s vector stopped the decode",
		    o->ctor->name, stage);
	}
	return (ret);
}

static int
api_send(mtp_client_t *c, const void *body, size_t len, int content,
    uint32_t kind, int64_t aux)
{
	int64_t	msg_id;

	if (mtp_send_encrypted(c, body, len, content, &msg_id) != MTP_OK) {
		return (c->last_error);
	}
	if (len <= sizeof(c->last_req)) {
		memcpy(c->last_req, body, len);
		c->last_req_len = len;
		c->last_req_kind = kind;
		c->last_req_aux = aux;
		c->last_req_msg_id = msg_id;
		c->last_req_content = content;
		c->last_req_resent = 0;
	} else {
		c->last_req_len = 0;
	}
	return (kind == MTP_REQ_NONE ? MTP_OK : mtp_pending_add(c, msg_id, kind,
	    aux));
}

int
mtp_resend_last(mtp_client_t *c, int64_t bad_msg_id)
{
	int64_t	msg_id;
	int	ret;

	if (c->last_req_len == 0 || c->last_req_msg_id != bad_msg_id) {
		mtp_logf(MTP_LOG_ERROR, "bad_server_salt names msg_id=%lld but "
		    "the last request sent was %lld (%u bytes held); it cannot be "
		    "resent and will time out",
		    (long long)bad_msg_id, (long long)c->last_req_msg_id,
		    (unsigned int)c->last_req_len);
		return (MTP_OK);
	}
	if (c->last_req_resent) {
		return (mtp_fail(c, MTP_ERR_PROTO, "DC%d rejected %s twice over "
		    "the server salt; the second salt did not work either",
		    mtp_dc_id(c->dc_index), mtp_log_req_name(c->last_req_kind)));
	}
	c->last_req_resent = 1;
	mtp_pending_clear(c, bad_msg_id);
	mtp_logf(MTP_LOG_INFO, "resend: %s under the new salt %016llx",
	    mtp_log_req_name(c->last_req_kind),
	    (unsigned long long)c->server_salt);
	if (mtp_send_encrypted(c, c->last_req, c->last_req_len,
	    c->last_req_content, &msg_id) != MTP_OK) {
		return (c->last_error);
	}
	c->last_req_msg_id = msg_id;
	if (c->last_req_kind == MTP_REQ_NONE) {
		return (MTP_OK);
	}
	ret = mtp_pending_add(c, msg_id, c->last_req_kind, c->last_req_aux);
	return (ret);
}

int
mtp_api_send(mtp_client_t *c, const uint8_t *query, size_t query_len,
    uint32_t kind, int64_t aux)
{
	mtp_writer_t	w;
	uint8_t		body[MTP_MAX_REQUEST];

	if (c->init_done) {
		return (api_send(c, query, query_len, 1, kind, aux));
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_invokeWithLayer);
	mtp_write_i32(&w, MTP_LAYER);
	mtp_write_u32(&w, MTP_FN_initConnection);
	mtp_write_i32(&w, 0);
	mtp_write_i32(&w, c->cfg.api_id);
	mtp_write_string(&w, c->device_model);
	mtp_write_string(&w, c->system_version);
	mtp_write_string(&w, c->app_version);
	mtp_write_string(&w, c->lang_code);
	mtp_write_string(&w, "");
	mtp_write_string(&w, c->lang_code);
	mtp_write_raw(&w, query, query_len);
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_INVAL, "invokeWithLayer envelope plus "
		    "a %u-byte query exceeds the %u-byte request buffer",
		    (unsigned int)query_len, (unsigned int)sizeof(body)));
	}
	mtp_logf(MTP_LOG_DEBUG, "invokeWithLayer(%d) + initConnection wrapping "
	    "%s", (int)MTP_LAYER, mtp_log_req_name(kind));
	return (api_send(c, body, w.len, 1, kind, aux));
}

int
mtp_send_init(mtp_client_t *c)
{
	uint8_t	body[4];
	mtp_writer_t	w;

	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_help_getNearestDc);
	return (mtp_api_send(c, body, w.len, MTP_REQ_INIT, 0));
}


int
mtp_send_get_password(mtp_client_t *c)
{
	uint8_t		body[4];
	mtp_writer_t	w;

	if (mtp_pending_kind(c, MTP_REQ_GET_PASSWORD) != NULL) {
		return (MTP_OK);
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_account_getPassword);
	return (mtp_api_send(c, body, w.len, MTP_REQ_GET_PASSWORD, 0));
}


int
mtp_send_check_password(mtp_client_t *c)
{
	uint8_t		body[8 + 2 * (MTP_DH_PRIME_SIZE + 8)];
	mtp_writer_t	w;
	int		ret;

	if (mtp_pending_kind(c, MTP_REQ_CHECK_PASSWORD) != NULL) {
		return (MTP_OK);
	}
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_auth_checkPassword);
	ret = mtp_srp_write_check(c, &w);
	if (ret != MTP_OK) {
		return (ret);
	}
	if (w.overflow) {
		return (mtp_fail(c, MTP_ERR_INVAL, "auth.checkPassword does not "
		    "fit its %u-byte buffer", (unsigned int)sizeof(body)));
	}
	mtp_logf(MTP_LOG_INFO, "checkPassword: sending the SRP proof");
	return (mtp_api_send(c, body, w.len, MTP_REQ_CHECK_PASSWORD, 0));
}


static int
api_take_authorization(mtp_client_t *c, const mtp_object_t *o, const char *what)
{
	mtp_reader_t	user_r;
	mtp_object_t	user;

	if (o->ctor->id != MTP_ID_auth_authorization) {
		return (mtp_fail(c, MTP_ERR_PROTO, "%s returned %s, expected "
		    "auth.authorization", what, o->ctor->name));
	}
	if (mtp_object_at(o, "user", &user_r) != 0 ||
	    mtp_object_parse(&user_r, &user) != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "auth.authorization has no "
		    "parsable user"));
	}
	if (strcmp(user.ctor->name, "user") != 0 &&
	    strcmp(user.ctor->name, "userEmpty") != 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "auth.authorization carries %s "
		    "where a user was expected", user.ctor->name));
	}
	c->self_id = mtp_object_i64(&user, "id", 0);
	if (c->self_id == 0) {
		return (mtp_fail(c, MTP_ERR_PROTO, "authorization supplied no "
		    "user id"));
	}
	c->authorized = 1;
	c->password_needed = 0;
	/* The proof and A are spent; nothing in the SRP state is wanted now. */
	mtp_srp_reset(c);
	mtp_logf(MTP_LOG_INFO, "%s: authorized as user %lld", what,
	    (long long)c->self_id);
	if (mtp_store_save(c) != MTP_OK) {
		return (mtp_fail(c, MTP_ERR_STORE, "signed in but cannot save the "
		    "record to %s", c->auth_path));
	}
	return (MTP_OK);
}

int
mtp_send_ping(mtp_client_t *c)
{
	uint8_t	body[12];
	mtp_writer_t	w;

	c->ping_id = mtp_next_msg_id(c);
	mtp_writer_init(&w, body, sizeof(body));
	mtp_write_u32(&w, MTP_FN_ping);
	mtp_write_i64(&w, c->ping_id);
	return (api_send(c, body, w.len, 0, MTP_REQ_PING, 0));
}

int
mtp_dispatch_error(mtp_client_t *c, mtp_pending_t *p, int32_t code,
    const char *message)
{
	int	dc_id, wait;

	mtp_logf(MTP_LOG_ERROR, "rpc_error on %s: %d %s",
	    mtp_log_req_name(p->kind), (int)code,
	    message != NULL ? message : "(no message)");
	wait = api_flood_seconds(code, message);
	if (wait > 0) {
		c->flood_until = mtp_now_ms() + (uint64_t)wait * 1000u;
		c->flood_kind = p->kind;
		return (mtp_soft_fail(c, MTP_ERR_FLOOD, "%s is rate limited for "
		    "%d s (%s)", mtp_log_req_name(p->kind), wait,
		    message != NULL && message[0] != '\0' ? message : "420"));
	}
	if (message != NULL && strncmp(message, "PHONE_MIGRATE_", 14) == 0 &&
	    code == 303) {
		if (message[14] < '0' || message[14] > '9') {
			return (mtp_fail(c, MTP_ERR_RPC, "server sent \"%s\", "
			    "which carries no DC id", message));
		}
		dc_id = message[14] - '0';
		c->migrate_to = mtp_dc_index_of(dc_id);
		if (c->migrate_to >= 0) {
			mtp_logf(MTP_LOG_INFO, "migrating from DC%d to DC%d, "
			    "re-handshaking there", mtp_dc_id(c->dc_index),
			    dc_id);
			c->dc_index = c->migrate_to;
			return (mtp_transport_open(c));
		}
		return (mtp_fail(c, MTP_ERR_RPC, "server redirects to DC%d, "
		    "which this build has no address for", dc_id));
	}
	if (message != NULL && strncmp(message, "SESSION_PASSWORD_NEEDED", 23) == 0) {
		c->password_needed = 1;
		mtp_srp_reset(c);
		if (mtp_send_get_password(c) != MTP_OK) {
			return (c->last_error);
		}
		return (mtp_soft_fail(c, MTP_ERR_AUTH, "this account has a cloud "
		    "password; fetching its SRP parameters"));
	}
	if (p->kind == MTP_REQ_CHECK_PASSWORD) {
		mtp_srp_reset(c);
		c->password_needed = 1;
		if (mtp_send_get_password(c) != MTP_OK) {
			return (c->last_error);
		}
		if (message != NULL &&
		    strncmp(message, "PASSWORD_HASH_INVALID", 21) == 0) {
			return (mtp_soft_fail(c, MTP_ERR_AUTH, "wrong cloud "
			    "password"));
		}
		return (mtp_soft_fail(c, MTP_ERR_RPC, "checkPassword rejected: "
		    "%d %s", (int)code,
		    message != NULL && message[0] != '\0' ? message : "unknown"));
	}
	if (p->kind == MTP_REQ_SEND_CODE || p->kind == MTP_REQ_SIGN_IN ||
	    p->kind == MTP_REQ_GET_PASSWORD) {
		return (mtp_soft_fail(c, MTP_ERR_RPC, "%s rejected: %d %s",
		    mtp_log_req_name(p->kind), (int)code,
		    message != NULL && message[0] != '\0' ? message : "unknown"));
	}
	return (mtp_fail(c, MTP_ERR_RPC, "%s failed: %d %s",
	    mtp_log_req_name(p->kind), (int)code,
	    message != NULL ? message : "unknown"));
}

int
mtp_dispatch_result(mtp_client_t *c, mtp_pending_t *p, mtp_reader_t *r)
{
	mtp_object_t	o;

	if (mtp_object_parse(r, &o) != 0) {
		char	why[MTP_MAX_ERROR];

		(void)mtp_reader_explain(r, why, sizeof(why));
		return (mtp_fail(c, MTP_ERR_PROTO, "%s: reply does not parse: %s",
		    mtp_log_req_name(p->kind), why));
	}
	mtp_logf(MTP_LOG_DEBUG, "result: %s -> %s", mtp_log_req_name(p->kind),
	    o.ctor->name);
	switch (p->kind) {
	case MTP_REQ_INIT:
		c->init_done = 1;
		mtp_set_state(c, MTP_STATE_READY, "initConnection accepted");
		c->next_ping = mtp_now_ms() + MTP_PING_INTERVAL_MS;
		return (MTP_OK);
	case MTP_REQ_SEND_CODE:
		if (o.ctor->id != MTP_ID_auth_sentCode) {
			return (mtp_fail(c, MTP_ERR_PROTO, "sendCode returned %s, "
			    "expected auth.sentCode", o.ctor->name));
		}
		(void)mtp_object_str(&o, "phone_code_hash", c->code_hash,
		    sizeof(c->code_hash));
		if (c->code_hash[0] == '\0') {
			return (mtp_fail(c, MTP_ERR_PROTO, "auth.sentCode carried "
			    "no phone_code_hash, so signIn cannot be built"));
		}
		mtp_logf(MTP_LOG_INFO, "sendCode: accepted, code hash received");
		return (MTP_OK);
	case MTP_REQ_SIGN_IN:
		return (api_take_authorization(c, &o, "signIn"));
	case MTP_REQ_CHECK_PASSWORD:
		return (api_take_authorization(c, &o, "checkPassword"));
	case MTP_REQ_GET_PASSWORD:
		if (mtp_srp_take_params(c, &o) != MTP_OK) {
			return (c->last_error);
		}
		c->password_needed = 1;
		return (MTP_OK);
	case MTP_REQ_DIALOGS:
		if (o.ctor->id != MTP_ID_messages_dialogs &&
		    o.ctor->id != MTP_ID_messages_dialogsSlice) {
			return (mtp_soft_fail(c, MTP_ERR_PROTO, "getDialogs "
			    "returned %s, expected messages.dialogs[Slice]",
			    o.ctor->name));
		}
		c->name_count = 0;
		if (api_decode_collection(c, p, &o) != 0) {
			return (mtp_soft_fail(c, MTP_ERR_PROTO,
			    "cannot decode the %s collection", o.ctor->name));
		}
		mtp_logf(MTP_LOG_INFO, "getDialogs: %u dialogs, %u names cached",
		    (unsigned int)c->dialog_count, (unsigned int)c->name_count);
		return (MTP_OK);
	case MTP_REQ_HISTORY:
		if (o.ctor->id != MTP_ID_messages_messages &&
		    o.ctor->id != MTP_ID_messages_messagesSlice &&
		    o.ctor->id != MTP_ID_messages_channelMessages) {
			return (mtp_soft_fail(c, MTP_ERR_PROTO, "getHistory "
			    "returned %s, expected messages.messages[Slice]",
			    o.ctor->name));
		}
		c->name_count = 0;
		if (api_decode_collection(c, p, &o) != 0) {
			return (mtp_soft_fail(c, MTP_ERR_PROTO,
			    "cannot decode the %s collection", o.ctor->name));
		}
		c->history_peer.id = p->aux;
		mtp_logf(MTP_LOG_INFO, "getHistory: %u messages for peer %lld",
		    (unsigned int)c->history_count, (long long)p->aux);
		return (MTP_OK);
	case MTP_REQ_SEND_MSG:
		if (o.ctor->id != MTP_ID_updateShortSentMessage) {
			return (mtp_fail(c, MTP_ERR_PROTO, "sendMessage returned "
			    "%s, expected updateShortSentMessage", o.ctor->name));
		}
		return (MTP_OK);
	case MTP_REQ_LOGOUT:
		mtp_store_forget(c);
		return (MTP_OK);
	default:
		return (MTP_OK);
	}
}

int
mtp_run_queued(mtp_client_t *c)
{
	uint32_t	kind;

	kind = c->queued_kind;
	c->queued_kind = MTP_REQ_NONE;
	if (kind != MTP_REQ_NONE) {
		mtp_logf(MTP_LOG_INFO, "replaying deferred %s now that the "
		    "session is ready", mtp_log_req_name(kind));
	}
	switch (kind) {
	case MTP_REQ_DIALOGS:
		return (mtpGetDialogs(c, c->queued_i32));
	case MTP_REQ_HISTORY:
		return (mtpGetHistory(c, &c->queued_peer, c->queued_i32,
		    c->queued_i32b));
	case MTP_REQ_SEND_MSG:
		return (mtpSendMessage(c, &c->queued_peer, c->queued_text));
	case MTP_REQ_READ_HISTORY:
		return (mtpReadHistory(c, &c->queued_peer, c->queued_i32));
	default:
		return (MTP_OK);
	}
}
