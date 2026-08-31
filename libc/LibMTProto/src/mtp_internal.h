/* !DEFINES!

$define %type mtp_writer as bounded TL serialisation cursor
$define %type mtp_reader as bounded TL deserialisation cursor
$define %type mtp_object as one parsed TL constructor with field offsets
$define %type mtp_transport as TCP intermediate framing state
$define %type mtp_pending as one outstanding RPC
$define %type mtp_srp as resumable SRP-6a cloud-password derivation state
$define %type mtp_client as full connection, session and result state

*/

/* !SPACE!

$space %export mtp_writer_t, mtp_reader_t, mtp_object_t
$space %export mtp_transport_t, mtp_pending_t, mtp_srp_t
$space %internal mtp_client

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

#ifndef LIBMTPROTO_INTERNAL_H
#define LIBMTPROTO_INTERNAL_H

#include <libcrypto.h>
#include <mtproto.h>
#include <stddef.h>
#include <stdint.h>

#include "mtp_schema.h"

#define MTP_AUTH_KEY_SIZE	256
#define MTP_MSG_KEY_SIZE	16
#define MTP_NONCE_SIZE		16
#define MTP_NEW_NONCE_SIZE	32
#define MTP_RSA_SIZE		256
#define MTP_DH_PRIME_SIZE	256
#define MTP_RSA_BLOB		255
#define MTP_TRANSPORT_MAGIC	0xEEEEEEEEu
#define MTP_LEN_PREFIX		4
#define MTP_MAX_FRAME		(4u * 1024u * 1024u)
#define MTP_MAX_REQUEST		(64u * 1024u)
#define MTP_MAX_UNPACK		MTP_MAX_FRAME
#define MTP_MAX_FIELDS		64
#define MTP_MAX_DEPTH		24
#define MTP_OFF_ABSENT		0xFFFFFFFFu
#define MTP_OFF_FLAG_TRUE	0xFFFFFFFEu
#define MTP_MAX_PENDING		16
#define MTP_MAX_ACK		32
#define MTP_MAX_PEER_CACHE	512
#define MTP_CONNECT_TIMEOUT_MS	15000
#define MTP_RPC_TIMEOUT_MS	30000
#define MTP_HANDSHAKE_TIMEOUT_MS 20000
#define MTP_PING_INTERVAL_MS	25000
#define MTP_ID_msg_container	0x73f1f8dcU
#define MTP_ID_rpc_result	0xf35c6d01U
#define MTP_ID_gzip_packed	0x3072cfa1U
#define MTP_ID_vector		0x1cb5c415U
#define MTP_ID_codeSettings	0xad253d78U
#define MTP_ID_inputCheckPasswordSRP	0xd27ff082U
#define MTP_ID_p_q_inner_data_dc	0xa9f55f95U
#define MTP_ID_client_DH_inner_data	0x6643b654U
#define MTP_REQ_NONE		0
#define MTP_REQ_INIT		1
#define MTP_REQ_SEND_CODE	2
#define MTP_REQ_SIGN_IN		3
#define MTP_REQ_DIALOGS		4
#define MTP_REQ_HISTORY		5
#define MTP_REQ_SEND_MSG	6
#define MTP_REQ_READ_HISTORY	7
#define MTP_REQ_LOGOUT		8
#define MTP_REQ_PING		9
#define MTP_REQ_GET_PASSWORD	10
#define MTP_REQ_CHECK_PASSWORD	11
#define MTP_SRP_MAX_SALT	128

typedef struct mtp_writer {
	uint8_t		*buf;
	size_t		cap;
	size_t		len;
	int		overflow;
} mtp_writer_t;


#define MTP_RE_OK		0
#define MTP_RE_SHORT		1
#define MTP_RE_UNKNOWN_CTOR	2
#define MTP_RE_BAD_VECTOR	3
#define MTP_RE_DEPTH		4
#define MTP_RE_TOO_MANY_FIELDS	5
#define MTP_RE_BAD_FLAG_WORD	6

typedef struct mtp_reader {
	const uint8_t	*buf;
	size_t		len;
	size_t		pos;
	int		error;
	int		reason;
	uint32_t	bad_id;
	const char	*ctor_name;
	const char	*field_name;
	size_t		fail_pos;
} mtp_reader_t;


typedef struct mtp_object {
	const struct mtp_ctor	*ctor;
	const uint8_t		*base;
	size_t			base_len;
	uint32_t		off[MTP_MAX_FIELDS];
	uint32_t		flags[2];
	size_t			end;
} mtp_object_t;

typedef struct mtp_pending {
	int64_t		msg_id;
	uint32_t	kind;
	uint64_t	deadline;
	int64_t		aux;
	int		in_use;
} mtp_pending_t;


typedef struct mtp_srp {
	int			state;
	int			have_params;
	int			armed;
	uint32_t		g;
	int64_t			srp_id;
	uint8_t			p[MTP_DH_PRIME_SIZE];
	uint8_t			g_b[MTP_DH_PRIME_SIZE];
	uint8_t			a_pub[MTP_DH_PRIME_SIZE];
	uint8_t			m1[LC_SHA256_DIGEST_SIZE];
	uint8_t			salt1[MTP_SRP_MAX_SALT];
	size_t			salt1_len;
	uint8_t			salt2[MTP_SRP_MAX_SALT];
	size_t			salt2_len;
	char			hint[MTP_MAX_NAME];
	char			password[MTP_MAX_PASSWORD];
	size_t			password_len;
	uint8_t			pbkdf2_out[LC_SHA512_DIGEST_SIZE];
	lc_bn			bn_p;
	lc_bn			bn_b;
	lc_bn			bn_q;		/* (p-1)/2, only while it is tested */
	lc_bn_prime_ctx		prime;
	lc_pbkdf2_sha512_ctx	kdf;
} mtp_srp_t;

typedef struct mtp_peer_name {
	int64_t		id;
	int64_t		access_hash;
	int32_t		kind;
	char		title[MTP_MAX_NAME];
} mtp_peer_name_t;

struct mtp_client {
	mtp_config_t	cfg;
	char		api_hash[64];
	char		device_model[64];
	char		system_version[64];
	char		app_version[32];
	char		lang_code[16];
	char		auth_path[256];

	int		kq;
	int		sock;
	int		wait_fd;
	int16_t		wait_filter;
	int		state;
	int		last_error;
	char		error[MTP_MAX_ERROR];
	int		dc_index;
	int		migrate_to;

	uint8_t		out_buf[MTP_MAX_REQUEST];
	size_t		out_len;
	size_t		out_sent;
	int		magic_sent;
	uint8_t		*in_buf;
	size_t		in_cap;
	size_t		in_len;
	uint64_t	deadline;
	int		hs_step;
	uint8_t		hs_nonce[MTP_NONCE_SIZE];
	uint8_t		hs_server_nonce[MTP_NONCE_SIZE];
	uint8_t		hs_new_nonce[MTP_NEW_NONCE_SIZE];
	uint8_t		hs_tmp_aes_key[32];
	uint8_t		hs_tmp_aes_iv[32];
	uint8_t		hs_g_a[MTP_DH_PRIME_SIZE];
	uint8_t	hs_g_b[MTP_DH_PRIME_SIZE];
	uint8_t		hs_dh_prime[MTP_DH_PRIME_SIZE];
	uint8_t		hs_b[MTP_DH_PRIME_SIZE];
	uint32_t	hs_g;
	int		hs_retry;

	uint8_t		auth_key[MTP_AUTH_KEY_SIZE];
	int		auth_key_valid;
	int64_t		auth_key_id;
	int64_t		session_id;
	int64_t		server_salt;
	int32_t		seq_no;
	int32_t		time_offset;
	int64_t		last_msg_id;
	int		init_done;

	mtp_pending_t	pending[MTP_MAX_PENDING];
	int64_t		ack[MTP_MAX_ACK];
	int		ack_count;
	uint64_t	next_ping;
	int64_t		ping_id;

	uint8_t		last_req[MTP_MAX_REQUEST];
	size_t		last_req_len;
	uint32_t	last_req_kind;
	int64_t		last_req_aux;
	int64_t		last_req_msg_id;
	int		last_req_content;
	int		last_req_resent;

	int		authorized;
	int64_t		self_id;
	char		phone[MTP_MAX_PHONE];
	char		code_hash[128];
	mtp_srp_t	srp;
	int		password_needed;


	uint64_t	flood_until;
	uint32_t	flood_kind;
	int		rekey_done;
	int		soft_error;

	uint32_t	queued_kind;
	mtp_peer_t	queued_peer;
	int32_t		queued_i32;
	int32_t		queued_i32b;
	char		queued_text[MTP_MAX_TEXT];

	mtp_dialog_t	dialogs[MTP_MAX_DIALOGS];
	int		dialog_count;
	mtp_message_t	history[MTP_MAX_HISTORY];
	int		history_count;
	mtp_peer_t	history_peer;
	mtp_peer_name_t	names[MTP_MAX_PEER_CACHE];
	int		name_count;
};

void		mtp_writer_init(mtp_writer_t *w, void *buf, size_t cap);
void		mtp_write_u32(mtp_writer_t *w, uint32_t v);
void		mtp_write_i32(mtp_writer_t *w, int32_t v);
void		mtp_write_i64(mtp_writer_t *w, int64_t v);
void		mtp_write_double(mtp_writer_t *w, double v);
void		mtp_write_raw(mtp_writer_t *w, const void *data, size_t len);
void		mtp_write_bytes(mtp_writer_t *w, const void *data, size_t len);
void		mtp_write_string(mtp_writer_t *w, const char *s);
void		mtp_write_bool(mtp_writer_t *w, int v);
void		mtp_write_peer(mtp_writer_t *w, const mtp_peer_t *peer);
void		mtp_write_pad_to(mtp_writer_t *w, size_t multiple);

void		mtp_reader_init(mtp_reader_t *r, const void *buf, size_t len);
uint32_t	mtp_read_u32(mtp_reader_t *r);
uint32_t	mtp_reader_peek_u32(const mtp_reader_t *r);
int32_t		mtp_read_i32(mtp_reader_t *r);
int64_t		mtp_read_i64(mtp_reader_t *r);
double		mtp_read_double(mtp_reader_t *r);
const uint8_t	*mtp_read_raw(mtp_reader_t *r, size_t len);
const uint8_t	*mtp_read_bytes(mtp_reader_t *r, size_t *out_len);
size_t		mtp_read_string(mtp_reader_t *r, char *out, size_t cap);
int		mtp_skip_object(mtp_reader_t *r, int depth);
int		mtp_object_parse(mtp_reader_t *r, mtp_object_t *out);
int		mtp_object_at(const mtp_object_t *o, const char *field,
		    mtp_reader_t *out);
int		mtp_object_has(const mtp_object_t *o, const char *field);
int32_t		mtp_object_i32(const mtp_object_t *o, const char *field,
		    int32_t def);
int64_t		mtp_object_i64(const mtp_object_t *o, const char *field,
		    int64_t def);
size_t		mtp_object_str(const mtp_object_t *o, const char *field,
		    char *out, size_t cap);
int		mtp_object_vector(const mtp_object_t *o, const char *field,
		    mtp_reader_t *out, uint32_t *out_count);
const char	*mtp_reader_reason(const mtp_reader_t *r);
size_t		mtp_reader_explain(const mtp_reader_t *r, char *out, size_t cap);

int		mtp_transport_open(mtp_client_t *c);
void		mtp_transport_close(mtp_client_t *c);
int		mtp_transport_check_connect(mtp_client_t *c);
int		mtp_transport_flush(mtp_client_t *c);
int		mtp_transport_queue(mtp_client_t *c, const void *data,
		    size_t len);
int		mtp_transport_recv(mtp_client_t *c);
int		mtp_transport_take_frame(mtp_client_t *c, const uint8_t **out,
		    size_t *out_len);
void		mtp_transport_drop_frame(mtp_client_t *c, size_t len);
int		mtp_wait(mtp_client_t *c, int fd, int16_t filter);
void		mtp_unwatch(mtp_client_t *c);
uint32_t	mtp_dc_address(int index);
int		mtp_dc_id(int index);
int		mtp_dc_index_of(int id);
uint64_t	mtp_now_ms(void);
int64_t		mtp_unix_time(const mtp_client_t *c);
int64_t		mtp_unix_time_ns(const mtp_client_t *c, uint32_t *out_nsec);

int64_t		mtp_next_msg_id(mtp_client_t *c);
int32_t		mtp_next_seqno(mtp_client_t *c, int content_related);
int		mtp_send_plain(mtp_client_t *c, const void *body, size_t len);
int		mtp_recv_plain(mtp_client_t *c, const uint8_t *frame,
		    size_t frame_len, mtp_reader_t *out);
int		mtp_send_encrypted(mtp_client_t *c, const void *body,
		    size_t len, int content_related, int64_t *out_msg_id);
int		mtp_decrypt_frame(mtp_client_t *c, const uint8_t *frame,
		    size_t frame_len, uint8_t *out, size_t out_cap,
		    size_t *out_len, int64_t *out_msg_id, int32_t *out_seqno);
void		mtp_derive_auth_key_id(mtp_client_t *c);
void		mtp_session_reset(mtp_client_t *c);

int		mtp_handshake_begin(mtp_client_t *c);
int		mtp_handshake_step(mtp_client_t *c, const uint8_t *frame,
		    size_t frame_len);
int		mtp_factorize(uint64_t pq, uint64_t *out_p, uint64_t *out_q);
int		mtp_dh_generator_ok(uint32_t g, const uint8_t *prime,
		    size_t prime_len);

void		mtp_srp_reset(mtp_client_t *c);
int		mtp_srp_take_params(mtp_client_t *c, const mtp_object_t *o);
int		mtp_srp_begin(mtp_client_t *c, const char *password);
int		mtp_srp_step(mtp_client_t *c);
int		mtp_srp_ready(const mtp_client_t *c);
int		mtp_srp_busy(const mtp_client_t *c);
int		mtp_srp_write_check(mtp_client_t *c, mtp_writer_t *w);
const char	*mtp_srp_hint(const mtp_client_t *c);
int		mtp_srp_have_params(const mtp_client_t *c);
int		mtp_send_get_password(mtp_client_t *c);
int		mtp_send_check_password(mtp_client_t *c);

void		mtp_set_state(mtp_client_t *c, int state, const char *why);

int		mtp_rekey(mtp_client_t *c);

int		mtp_pending_add(mtp_client_t *c, int64_t msg_id, uint32_t kind,
		    int64_t aux);
mtp_pending_t	*mtp_pending_find(mtp_client_t *c, int64_t msg_id);
mtp_pending_t	*mtp_pending_kind(mtp_client_t *c, uint32_t kind);
int		mtp_pending_active(const mtp_client_t *c, uint32_t kind);
void		mtp_pending_clear(mtp_client_t *c, int64_t msg_id);
void		mtp_ack_add(mtp_client_t *c, int64_t msg_id);
int		mtp_flush_acks(mtp_client_t *c);
int		mtp_handle_frame(mtp_client_t *c, const uint8_t *frame,
		    size_t frame_len);
int		mtp_fail(mtp_client_t *c, int err, const char *fmt, ...);

int		mtp_soft_fail(mtp_client_t *c, int err, const char *fmt, ...);
uint64_t	mtp_flood_left(const mtp_client_t *c);

int		mtp_api_send(mtp_client_t *c, const uint8_t *body, size_t len,
		    uint32_t kind, int64_t aux);
int		mtp_send_init(mtp_client_t *c);
int		mtp_dispatch_result(mtp_client_t *c, mtp_pending_t *p,
		    mtp_reader_t *r);
int		mtp_dispatch_error(mtp_client_t *c, mtp_pending_t *p,
		    int32_t code, const char *message);
int		mtp_run_queued(mtp_client_t *c);
int		mtp_send_ping(mtp_client_t *c);


int		mtp_resend_last(mtp_client_t *c, int64_t bad_msg_id);

int		mtp_store_load(mtp_client_t *c);
int		mtp_store_save(const mtp_client_t *c);
void		mtp_store_forget(mtp_client_t *c);

void		mtp_logf(int level, const char *fmt, ...);
void		mtp_log_hex(int level, const char *label, const void *data,
		    size_t len);
void		mtp_log_mask(char *out, size_t cap, const char *text);
const char	*mtp_log_req_name(uint32_t kind);
const char	*mtp_log_ctor_name(uint32_t id);

#define MTP_LOG_MASK_MAX	32

#endif
