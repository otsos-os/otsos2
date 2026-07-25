/* !DEFINES!

$define %type lssh_slice as borrowed byte span
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_ident as parsed SSH identification line
$define %type lssh_kexinit as parsed SSH_MSG_KEXINIT payload
$define %type lssh_kexinit_names as configured KEXINIT name-lists
$define %type lssh_algorithms as negotiated SSH transport algorithms
$define %type lssh_curve25519_keypair as client Curve25519 KEX keypair
$define %type lssh_kex_ecdh_reply as parsed SSH_MSG_KEX_ECDH_REPLY payload
$define %type lssh_client_handshake_result as completed client transport handshake data
$define %type lssh_userauth_failure as parsed SSH_MSG_USERAUTH_FAILURE
$define %type lssh_knownhost_result as known-hosts verification result
$define %type lssh_channel as SSH connection channel state
$define %type lssh_channel_open_confirmation as parsed channel open confirmation
$define %type lssh_channel_open_failure as parsed channel open failure
$define %type lssh_channel_event as parsed channel message event
$define %type lssh_chachapoly as OpenSSH ChaCha20-Poly1305 packet keys
$define %type lssh_transport as native SSH transport session
$define %type lssh_log_callback as library trace callback
$define %func lssh_log_set as procedure with args int, callback, void *
$define %func lssh_log_enabled as function with args int
$define %func lssh_buf_init as function with args lssh_buf *, size_t
$define %func lssh_buf_free as procedure with args lssh_buf *
$define %func lssh_buf_reset as procedure with args lssh_buf *
$define %func lssh_buf_set_secure as procedure with args lssh_buf *, int
$define %func lssh_buf_reserve as function with args lssh_buf *, size_t
$define %func lssh_buf_append as function with args lssh_buf *, const void *, size_t
$define %func lssh_buf_put_u8 as function with args lssh_buf *, uint8_t
$define %func lssh_buf_put_u32 as function with args lssh_buf *, uint32_t
$define %func lssh_buf_put_string as function with args lssh_buf *, const void *, size_t
$define %func lssh_buf_put_cstring as function with args lssh_buf *, const char *
$define %func lssh_reader_init as procedure with args lssh_reader *, const void *, size_t
$define %func lssh_reader_remaining as function with args const lssh_reader *
$define %func lssh_reader_u8 as function with args lssh_reader *, uint8_t *
$define %func lssh_reader_u32 as function with args lssh_reader *, uint32_t *
$define %func lssh_reader_string as function with args lssh_reader *, lssh_slice *
$define %func lssh_namelist_contains as function with args lssh_slice, const char *
$define %func lssh_namelist_first_match as function with args preferred, available, out, out size
$define %func lssh_ident_make as function with args lssh_buf *, const char *, const char *
$define %func lssh_ident_parse_line as function with args const void *, size_t, lssh_ident *
$define %func lssh_ident_scan as function with args const void *, size_t, size_t *, lssh_ident *
$define %func lssh_packet_plain_encode as function with args lssh_buf *, const void *, size_t, size_t
$define %func lssh_packet_plain_decode as function with args const void *, size_t, lssh_slice *, size_t *
$define %func lssh_chachapoly_init as procedure with args lssh_chachapoly *, const uint8_t *
$define %func lssh_chachapoly_free as procedure with args lssh_chachapoly *
$define %func lssh_packet_chachapoly_encode as function with args lssh_buf *, const lssh_chachapoly *, uint32_t, const void *, size_t
$define %func lssh_packet_chachapoly_peek_len as function with args const lssh_chachapoly *, uint32_t, const void *, size_t, uint32_t *
$define %func lssh_packet_chachapoly_decode as function with args const void *, size_t, const lssh_chachapoly *, uint32_t, lssh_buf *, size_t *
$define %func lssh_kexinit_default_names as procedure with args lssh_kexinit_names *
$define %func lssh_kexinit_encode as function with args lssh_buf *, const lssh_kexinit_names *, const uint8_t *
$define %func lssh_kexinit_parse as function with args const void *, size_t, lssh_kexinit *
$define %func lssh_kexinit_negotiate as function with args const lssh_kexinit *, const lssh_kexinit *, lssh_algorithms *
$define %func lssh_curve25519_keypair_generate as function with args lssh_curve25519_keypair *
$define %func lssh_curve25519_keypair_free as procedure with args lssh_curve25519_keypair *
$define %func lssh_kex_ecdh_init_encode as function with args lssh_buf *, const uint8_t *
$define %func lssh_kex_ecdh_reply_parse as function with args const void *, size_t, lssh_kex_ecdh_reply *
$define %func lssh_kex_curve25519_shared as function with args keypair, server public, shared secret
$define %func lssh_kex_curve25519_hash as function with args exchange hash, idents, kexinit payloads, host key, public keys, shared secret
$define %func lssh_kex_parse_ed25519_hostkey as function with args host key blob, public key
$define %func lssh_kex_verify_ed25519 as function with args host key blob, signature blob, exchange hash, public key
$define %func lssh_kex_derive_chachapoly as function with args shared secret, exchange hash, session id, side, read key, write key
$define %func lssh_client_handshake as function with args lssh_transport *, lssh_client_handshake_result *, const char *, const char *
$define %func lssh_service_request_encode as function with args lssh_buf *, const char *
$define %func lssh_service_accept_parse as function with args const void *, size_t, lssh_slice *
$define %func lssh_userauth_password_encode as function with args lssh_buf *, const char *, const char *
$define %func lssh_userauth_failure_parse as function with args const void *, size_t, lssh_userauth_failure *
$define %func lssh_client_request_service as function with args lssh_transport *, const char *
$define %func lssh_client_auth_password as function with args lssh_transport *, const char *, const char *
$define %func lssh_knownhost_format_host as function with args char *, size_t, const char *, uint32_t
$define %func lssh_knownhost_fingerprint as function with args lssh_slice, uint8_t *
$define %func lssh_knownhost_fingerprint_hex as function with args lssh_slice, char *, size_t
$define %func lssh_knownhosts_check as function with args path, host, port, host key, result
$define %func lssh_knownhosts_add as function with args path, host, port, host key
$define %func lssh_channel_init as procedure with args lssh_channel *, uint32_t, uint32_t, uint32_t
$define %func lssh_channel_open_session_encode as function with args lssh_buf *, const lssh_channel *
$define %func lssh_channel_open_confirmation_parse as function with args const void *, size_t, lssh_channel_open_confirmation *
$define %func lssh_channel_open_failure_parse as function with args const void *, size_t, lssh_channel_open_failure *
$define %func lssh_channel_apply_open_confirmation as function with args lssh_channel *, const lssh_channel_open_confirmation *
$define %func lssh_channel_request_encode as function with args lssh_buf *, const lssh_channel *, const char *, int, const void *, size_t
$define %func lssh_channel_request_pty_encode as function with args lssh_buf *, const lssh_channel *, term, cols, rows, pixels, want reply
$define %func lssh_channel_request_window_change_encode as function with args lssh_buf *, const lssh_channel *, cols, rows, pixels
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
$define %func lssh_transport_init as function with args lssh_transport *
$define %func lssh_transport_free as procedure with args lssh_transport *
$define %func lssh_transport_attach as function with args lssh_transport *, int, int
$define %func lssh_transport_close as procedure with args lssh_transport *
$define %func lssh_transport_connect_ipv4 as function with args lssh_transport *, uint32_t, uint32_t
$define %func lssh_transport_send_ident as function with args lssh_transport *, const char *, const char *
$define %func lssh_transport_recv_ident as function with args lssh_transport *, lssh_ident *
$define %func lssh_transport_exchange_ident as function with args lssh_transport *, const char *, const char *, lssh_ident *
$define %func lssh_transport_send_plain as function with args lssh_transport *, const void *, size_t
$define %func lssh_transport_recv_plain as function with args lssh_transport *, lssh_buf *
$define %func lssh_transport_send_packet as function with args lssh_transport *, const void *, size_t
$define %func lssh_transport_recv_packet as function with args lssh_transport *, lssh_buf *
$define %func lssh_transport_packet_pending as function with args const lssh_transport *
$define %func lssh_transport_set_chachapoly as function with args lssh_transport *, const uint8_t *, const uint8_t *
$define %func lssh_transport_set_chachapoly_read as function with args lssh_transport *, const uint8_t *
$define %func lssh_transport_set_chachapoly_write as function with args lssh_transport *, const uint8_t *
$define %func lssh_transport_clear_crypto as procedure with args lssh_transport *
$define %func lssh_transport_send_kexinit as function with args lssh_transport *, const lssh_kexinit_names *, const uint8_t *
$define %func lssh_transport_recv_kexinit as function with args lssh_transport *, lssh_buf *, lssh_kexinit *

*/

/* !SPACE!

$space %export lssh_slice, lssh_buf, lssh_reader, lssh_ident
$space %export lssh_kexinit, lssh_kexinit_names, lssh_algorithms
$space %export lssh_curve25519_keypair, lssh_kex_ecdh_reply
$space %export lssh_client_handshake_result
$space %export lssh_userauth_failure
$space %export lssh_knownhost_result
$space %export lssh_channel, lssh_channel_open_confirmation
$space %export lssh_channel_open_failure, lssh_channel_event
$space %export lssh_chachapoly, lssh_transport
$space %export lssh_log_callback, lssh_log_set, lssh_log_enabled
$space %export lssh_buf_init, lssh_buf_free, lssh_buf_reset
$space %export lssh_buf_set_secure, lssh_buf_reserve, lssh_buf_append
$space %export lssh_buf_put_u8, lssh_buf_put_u32
$space %export lssh_buf_put_string, lssh_buf_put_cstring
$space %export lssh_reader_init, lssh_reader_remaining
$space %export lssh_reader_u8, lssh_reader_u32, lssh_reader_string
$space %export lssh_namelist_contains, lssh_namelist_first_match
$space %export lssh_ident_make, lssh_ident_parse_line, lssh_ident_scan
$space %export lssh_packet_plain_encode, lssh_packet_plain_decode
$space %export lssh_chachapoly_init, lssh_chachapoly_free
$space %export lssh_packet_chachapoly_encode
$space %export lssh_packet_chachapoly_peek_len
$space %export lssh_packet_chachapoly_decode
$space %export lssh_kexinit_default_names, lssh_kexinit_encode
$space %export lssh_kexinit_parse, lssh_kexinit_negotiate
$space %export lssh_curve25519_keypair_generate
$space %export lssh_curve25519_keypair_free
$space %export lssh_kex_ecdh_init_encode, lssh_kex_ecdh_reply_parse
$space %export lssh_kex_curve25519_shared, lssh_kex_curve25519_hash
$space %export lssh_kex_parse_ed25519_hostkey
$space %export lssh_kex_verify_ed25519, lssh_kex_derive_chachapoly
$space %export lssh_client_handshake
$space %export lssh_service_request_encode, lssh_service_accept_parse
$space %export lssh_userauth_password_encode
$space %export lssh_userauth_failure_parse
$space %export lssh_client_request_service, lssh_client_auth_password
$space %export lssh_knownhost_format_host, lssh_knownhost_fingerprint
$space %export lssh_knownhost_fingerprint_hex
$space %export lssh_knownhosts_check, lssh_knownhosts_add
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
$space %export lssh_transport_init, lssh_transport_free
$space %export lssh_transport_attach, lssh_transport_close
$space %export lssh_transport_connect_ipv4
$space %export lssh_transport_send_ident, lssh_transport_recv_ident
$space %export lssh_transport_exchange_ident
$space %export lssh_transport_send_plain, lssh_transport_recv_plain
$space %export lssh_transport_send_packet, lssh_transport_recv_packet
$space %export lssh_transport_packet_pending
$space %export lssh_transport_set_chachapoly
$space %export lssh_transport_set_chachapoly_read
$space %export lssh_transport_set_chachapoly_write
$space %export lssh_transport_clear_crypto
$space %export lssh_transport_send_kexinit, lssh_transport_recv_kexinit

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

#ifndef LIBSSH_H
#define LIBSSH_H

#include <stddef.h>
#include <stdint.h>

#define LSSH_IDENT_MAX		255
#define LSSH_SOFTWARE_MAX	96
#define LSSH_COMMENT_MAX	128
#define LSSH_ALG_NAME_MAX	64
#define LSSH_KNOWNHOST_HOST_MAX	256
#define LSSH_KNOWNHOST_LINE_MAX	2048
#define LSSH_HOSTKEY_BLOB_MAX	256
#define LSSH_FINGERPRINT_HEX_MAX	65
#define LSSH_CHANNEL_DEFAULT_WINDOW	2097152U
#define LSSH_CHANNEL_DEFAULT_MAX_PACKET	32768U
#define LSSH_PACKET_MAX		35000
#define LSSH_PACKET_MIN_PAD	4
#define LSSH_SHA256_SIZE	32
#define LSSH_CURVE25519_SIZE	32
#define LSSH_ED25519_PUBLIC_KEY_SIZE	32
#define LSSH_ED25519_SIGNATURE_SIZE	64
#define LSSH_MSG_IGNORE		2
#define LSSH_MSG_UNIMPLEMENTED	3
#define LSSH_MSG_DEBUG		4
#define LSSH_MSG_SERVICE_REQUEST	5
#define LSSH_MSG_SERVICE_ACCEPT	6
#define LSSH_MSG_EXT_INFO	7
#define LSSH_MSG_NEWKEYS	21
#define LSSH_MSG_KEXINIT	20
#define LSSH_MSG_KEX_ECDH_INIT	30
#define LSSH_MSG_KEX_ECDH_REPLY	31
#define LSSH_MSG_USERAUTH_REQUEST	50
#define LSSH_MSG_USERAUTH_FAILURE	51
#define LSSH_MSG_USERAUTH_SUCCESS	52
#define LSSH_MSG_USERAUTH_BANNER	53
#define LSSH_MSG_GLOBAL_REQUEST	80
#define LSSH_MSG_REQUEST_SUCCESS	81
#define LSSH_MSG_REQUEST_FAILURE	82
#define LSSH_MSG_CHANNEL_OPEN	90
#define LSSH_MSG_CHANNEL_OPEN_CONFIRMATION	91
#define LSSH_MSG_CHANNEL_OPEN_FAILURE	92
#define LSSH_MSG_CHANNEL_WINDOW_ADJUST	93
#define LSSH_MSG_CHANNEL_DATA	94
#define LSSH_MSG_CHANNEL_EXTENDED_DATA	95
#define LSSH_MSG_CHANNEL_EOF	96
#define LSSH_MSG_CHANNEL_CLOSE	97
#define LSSH_MSG_CHANNEL_REQUEST	98
#define LSSH_MSG_CHANNEL_SUCCESS	99
#define LSSH_MSG_CHANNEL_FAILURE	100
#define LSSH_CHACHAPOLY_KEY_SIZE	64
#define LSSH_CHACHAPOLY_HALF_KEY_SIZE	32
#define LSSH_CHACHAPOLY_TAG_SIZE	16

#define LSSH_DEFAULT_SOFTWARE	"otsos2_ssh_0.1"
#define LSSH_KEX_DEFAULT	\
	"curve25519-sha256,curve25519-sha256@libssh.org," \
	"kex-strict-c-v00@openssh.com,ext-info-c"
#define LSSH_HOSTKEY_DEFAULT	"ssh-ed25519"
#define LSSH_CIPHER_DEFAULT	"chacha20-poly1305@openssh.com"
#define LSSH_MAC_DEFAULT	"hmac-sha2-256,hmac-sha2-512"
#define LSSH_COMP_DEFAULT	"none"

#define LSSH_KNOWNHOST_TRUSTED	0
#define LSSH_KNOWNHOST_NEW	1
#define LSSH_KNOWNHOST_CHANGED	2

#define LSSH_CHANNEL_OPEN_ADMIN_PROHIBITED	1
#define LSSH_CHANNEL_OPEN_CONNECT_FAILED	2
#define LSSH_CHANNEL_OPEN_UNKNOWN_TYPE	3
#define LSSH_CHANNEL_OPEN_RESOURCE_SHORTAGE	4

#define LSSH_EXTENDED_DATA_STDERR	1

#define LSSH_LOG_ERROR	1
#define LSSH_LOG_INFO	2
#define LSSH_LOG_DEBUG	3

enum lssh_result {
	LSSH_OK = 0,
	LSSH_ERR_INVALID = -1,
	LSSH_ERR_NO_MEMORY = -2,
	LSSH_ERR_RANGE = -3,
	LSSH_ERR_FORMAT = -4,
	LSSH_ERR_UNSUPPORTED = -5,
	LSSH_ERR_CRYPTO = -6,
	LSSH_ERR_STATE = -7,
	LSSH_ERR_IO = -8,
	LSSH_ERR_AUTH = -9,
	LSSH_ERR_VERIFY = -10,
	LSSH_ERR_AGAIN = -11,
	LSSH_ERR_NO_MATCH = -12
};

typedef void (*lssh_log_callback)(void *ctx, int level,
    const char *message);

typedef struct lssh_slice {
	const uint8_t	*data;
	size_t		len;
} lssh_slice;

typedef struct lssh_buf {
	uint8_t	*data;
	size_t	len;
	size_t	capacity;
	int	secure;
} lssh_buf;

typedef struct lssh_reader {
	const uint8_t	*data;
	size_t		len;
	size_t		off;
} lssh_reader;

typedef struct lssh_ident {
	char	protocol[16];
	char	software[LSSH_SOFTWARE_MAX];
	char	comment[LSSH_COMMENT_MAX];
	int	has_comment;
} lssh_ident;

typedef struct lssh_kexinit {
	uint8_t		cookie[16];
	lssh_slice	kex_algorithms;
	lssh_slice	server_host_key_algorithms;
	lssh_slice	encryption_algorithms_client_to_server;
	lssh_slice	encryption_algorithms_server_to_client;
	lssh_slice	mac_algorithms_client_to_server;
	lssh_slice	mac_algorithms_server_to_client;
	lssh_slice	compression_algorithms_client_to_server;
	lssh_slice	compression_algorithms_server_to_client;
	lssh_slice	languages_client_to_server;
	lssh_slice	languages_server_to_client;
	uint8_t		first_kex_packet_follows;
	uint32_t	reserved;
} lssh_kexinit;

typedef struct lssh_kexinit_names {
	const char	*kex_algorithms;
	const char	*server_host_key_algorithms;
	const char	*encryption_algorithms_client_to_server;
	const char	*encryption_algorithms_server_to_client;
	const char	*mac_algorithms_client_to_server;
	const char	*mac_algorithms_server_to_client;
	const char	*compression_algorithms_client_to_server;
	const char	*compression_algorithms_server_to_client;
	const char	*languages_client_to_server;
	const char	*languages_server_to_client;
} lssh_kexinit_names;

typedef struct lssh_algorithms {
	char	kex[LSSH_ALG_NAME_MAX];
	char	server_host_key[LSSH_ALG_NAME_MAX];
	char	encryption_client_to_server[LSSH_ALG_NAME_MAX];
	char	encryption_server_to_client[LSSH_ALG_NAME_MAX];
	char	mac_client_to_server[LSSH_ALG_NAME_MAX];
	char	mac_server_to_client[LSSH_ALG_NAME_MAX];
	char	compression_client_to_server[LSSH_ALG_NAME_MAX];
	char	compression_server_to_client[LSSH_ALG_NAME_MAX];
} lssh_algorithms;

typedef struct lssh_curve25519_keypair {
	uint8_t	private_key[LSSH_CURVE25519_SIZE];
	uint8_t	public_key[LSSH_CURVE25519_SIZE];
	int	valid;
} lssh_curve25519_keypair;

typedef struct lssh_kex_ecdh_reply {
	lssh_slice	host_key_blob;
	lssh_slice	server_public;
	lssh_slice	signature_blob;
} lssh_kex_ecdh_reply;

typedef struct lssh_client_handshake_result {
	lssh_ident	peer_ident;
	lssh_algorithms	algorithms;
	uint8_t		exchange_hash[LSSH_SHA256_SIZE];
	uint8_t		session_id[LSSH_SHA256_SIZE];
	uint8_t		host_key[LSSH_ED25519_PUBLIC_KEY_SIZE];
	uint8_t		host_key_blob[LSSH_HOSTKEY_BLOB_MAX];
	size_t		host_key_blob_len;
} lssh_client_handshake_result;

typedef struct lssh_userauth_failure {
	lssh_slice	methods;
	uint8_t		partial_success;
} lssh_userauth_failure;

typedef struct lssh_knownhost_result {
	int	status;
	uint32_t	line;
	char	host[LSSH_KNOWNHOST_HOST_MAX];
	uint8_t	fingerprint[LSSH_SHA256_SIZE];
} lssh_knownhost_result;

typedef struct lssh_channel {
	uint32_t	local_id;
	uint32_t	remote_id;
	uint32_t	local_window;
	uint32_t	remote_window;
	uint32_t	local_max_packet;
	uint32_t	remote_max_packet;
	int	open;
	int	eof_sent;
	int	eof_received;
	int	close_sent;
	int	close_received;
} lssh_channel;

typedef struct lssh_channel_open_confirmation {
	uint32_t	recipient;
	uint32_t	sender;
	uint32_t	initial_window;
	uint32_t	max_packet;
} lssh_channel_open_confirmation;

typedef struct lssh_channel_open_failure {
	uint32_t	recipient;
	uint32_t	reason;
	lssh_slice	description;
	lssh_slice	language;
} lssh_channel_open_failure;

typedef struct lssh_channel_event {
	uint8_t		type;
	uint32_t	recipient;
	uint32_t	bytes;
	uint32_t	reason;
	uint32_t	extended_type;
	lssh_slice	data;
	lssh_slice	request_type;
	lssh_slice	request_data;
	uint8_t		want_reply;
	int		has_exit_status;
	uint32_t	exit_status;
} lssh_channel_event;

typedef struct lssh_chachapoly {
	uint8_t	main_key[LSSH_CHACHAPOLY_HALF_KEY_SIZE];
	uint8_t	header_key[LSSH_CHACHAPOLY_HALF_KEY_SIZE];
	int	valid;
} lssh_chachapoly;

typedef struct lssh_transport {
	int		handle;
	int		owns_handle;
	int		closed;
	int		read_encrypted;
	int		write_encrypted;
	int		userauth_service;
	uint32_t	seq_in;
	uint32_t	seq_out;
	lssh_buf	rx;
	lssh_buf	tx;
	lssh_chachapoly read_cipher;
	lssh_chachapoly write_cipher;
	char		local_ident[LSSH_IDENT_MAX + 1];
	char		peer_ident_line[LSSH_IDENT_MAX + 1];
	size_t		local_ident_len;
	size_t		peer_ident_len;
	lssh_ident	peer_ident;
} lssh_transport;

void	lssh_log_set(int level, lssh_log_callback callback, void *ctx);
int	lssh_log_enabled(int level);

int	lssh_buf_init(lssh_buf *buf, size_t initial_capacity);
void	lssh_buf_free(lssh_buf *buf);
void	lssh_buf_reset(lssh_buf *buf);
void	lssh_buf_set_secure(lssh_buf *buf, int secure);
int	lssh_buf_reserve(lssh_buf *buf, size_t extra);
int	lssh_buf_append(lssh_buf *buf, const void *data, size_t len);
int	lssh_buf_put_u8(lssh_buf *buf, uint8_t value);
int	lssh_buf_put_u32(lssh_buf *buf, uint32_t value);
int	lssh_buf_put_string(lssh_buf *buf, const void *data, size_t len);
int	lssh_buf_put_cstring(lssh_buf *buf, const char *str);

void	lssh_reader_init(lssh_reader *reader, const void *data, size_t len);
size_t	lssh_reader_remaining(const lssh_reader *reader);
int	lssh_reader_u8(lssh_reader *reader, uint8_t *out);
int	lssh_reader_u32(lssh_reader *reader, uint32_t *out);
int	lssh_reader_string(lssh_reader *reader, lssh_slice *out);

int	lssh_namelist_contains(lssh_slice list, const char *name);
int	lssh_namelist_first_match(lssh_slice preferred, lssh_slice available,
	    char *out, size_t out_size);

int	lssh_ident_make(lssh_buf *out, const char *software,
	    const char *comment);
int	lssh_ident_parse_line(const void *line, size_t len,
	    lssh_ident *out);
int	lssh_ident_scan(const void *data, size_t len, size_t *consumed,
	    lssh_ident *out);

int	lssh_packet_plain_encode(lssh_buf *out, const void *payload,
	    size_t payload_len, size_t block_size);
int	lssh_packet_plain_decode(const void *packet, size_t len,
	    lssh_slice *payload, size_t *consumed);
void	lssh_chachapoly_init(lssh_chachapoly *cipher,
	    const uint8_t key[LSSH_CHACHAPOLY_KEY_SIZE]);
void	lssh_chachapoly_free(lssh_chachapoly *cipher);
int	lssh_packet_chachapoly_encode(lssh_buf *out,
	    const lssh_chachapoly *cipher, uint32_t seq,
	    const void *payload, size_t payload_len);
int	lssh_packet_chachapoly_peek_len(const lssh_chachapoly *cipher,
	    uint32_t seq, const void *packet, size_t len,
	    uint32_t *packet_len);
int	lssh_packet_chachapoly_decode(const void *packet, size_t len,
	    const lssh_chachapoly *cipher, uint32_t seq,
	    lssh_buf *payload, size_t *consumed);

void	lssh_kexinit_default_names(lssh_kexinit_names *names);
int	lssh_kexinit_encode(lssh_buf *out, const lssh_kexinit_names *names,
	    const uint8_t cookie[16]);
int	lssh_kexinit_parse(const void *payload, size_t len,
	    lssh_kexinit *out);
int	lssh_kexinit_negotiate(const lssh_kexinit *client,
	    const lssh_kexinit *server, lssh_algorithms *out);
int	lssh_curve25519_keypair_generate(lssh_curve25519_keypair *keypair);
void	lssh_curve25519_keypair_free(lssh_curve25519_keypair *keypair);
int	lssh_kex_ecdh_init_encode(lssh_buf *out,
	    const uint8_t public_key[LSSH_CURVE25519_SIZE]);
int	lssh_kex_ecdh_reply_parse(const void *payload, size_t len,
	    lssh_kex_ecdh_reply *out);
int	lssh_kex_curve25519_shared(
	    const lssh_curve25519_keypair *keypair,
	    lssh_slice server_public,
	    uint8_t shared_secret[LSSH_CURVE25519_SIZE]);
int	lssh_kex_curve25519_hash(
	    uint8_t exchange_hash[LSSH_SHA256_SIZE],
	    lssh_slice client_ident, lssh_slice server_ident,
	    lssh_slice client_kexinit, lssh_slice server_kexinit,
	    lssh_slice host_key_blob,
	    const uint8_t client_public[LSSH_CURVE25519_SIZE],
	    const uint8_t server_public[LSSH_CURVE25519_SIZE],
	    const uint8_t shared_secret[LSSH_CURVE25519_SIZE]);
int	lssh_kex_parse_ed25519_hostkey(lssh_slice host_key_blob,
	    uint8_t public_key[LSSH_ED25519_PUBLIC_KEY_SIZE]);
int	lssh_kex_verify_ed25519(lssh_slice host_key_blob,
	    lssh_slice signature_blob,
	    const uint8_t exchange_hash[LSSH_SHA256_SIZE],
	    uint8_t public_key[LSSH_ED25519_PUBLIC_KEY_SIZE]);
int	lssh_kex_derive_chachapoly(
	    const uint8_t shared_secret[LSSH_CURVE25519_SIZE],
	    const uint8_t exchange_hash[LSSH_SHA256_SIZE],
	    const uint8_t session_id[LSSH_SHA256_SIZE], int client_side,
	    uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE],
	    uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE]);
int	lssh_client_handshake(lssh_transport *transport,
	    lssh_client_handshake_result *result,
	    const char *software, const char *comment);
int	lssh_service_request_encode(lssh_buf *out, const char *service);
int	lssh_service_accept_parse(const void *payload, size_t len,
	    lssh_slice *service);
int	lssh_userauth_password_encode(lssh_buf *out, const char *username,
	    const char *password);
int	lssh_userauth_failure_parse(const void *payload, size_t len,
	    lssh_userauth_failure *failure);
int	lssh_client_request_service(lssh_transport *transport,
	    const char *service);
int	lssh_client_auth_password(lssh_transport *transport,
	    const char *username, const char *password);
int	lssh_knownhost_format_host(char *out, size_t out_size,
	    const char *host, uint32_t port);
int	lssh_knownhost_fingerprint(lssh_slice host_key_blob,
	    uint8_t fingerprint[LSSH_SHA256_SIZE]);
int	lssh_knownhost_fingerprint_hex(lssh_slice host_key_blob,
	    char *out, size_t out_size);
int	lssh_knownhosts_check(const char *path, const char *host,
	    uint32_t port, lssh_slice host_key_blob,
	    lssh_knownhost_result *result);
int	lssh_knownhosts_add(const char *path, const char *host,
	    uint32_t port, lssh_slice host_key_blob);
void	lssh_channel_init(lssh_channel *channel, uint32_t local_id,
	    uint32_t window, uint32_t max_packet);
int	lssh_channel_open_session_encode(lssh_buf *out,
	    const lssh_channel *channel);
int	lssh_channel_open_confirmation_parse(const void *payload,
	    size_t len, lssh_channel_open_confirmation *out);
int	lssh_channel_open_failure_parse(const void *payload, size_t len,
	    lssh_channel_open_failure *out);
int	lssh_channel_apply_open_confirmation(lssh_channel *channel,
	    const lssh_channel_open_confirmation *confirmation);
int	lssh_channel_request_encode(lssh_buf *out,
	    const lssh_channel *channel, const char *request,
	    int want_reply, const void *extra, size_t extra_len);
int	lssh_channel_request_pty_encode(lssh_buf *out,
	    const lssh_channel *channel, const char *term,
	    uint32_t cols, uint32_t rows, uint32_t width_px,
	    uint32_t height_px, int want_reply);
int	lssh_channel_request_window_change_encode(lssh_buf *out,
	    const lssh_channel *channel, uint32_t cols, uint32_t rows,
	    uint32_t width_px, uint32_t height_px);
int	lssh_channel_request_shell_encode(lssh_buf *out,
	    const lssh_channel *channel, int want_reply);
int	lssh_channel_request_exec_encode(lssh_buf *out,
	    const lssh_channel *channel, const char *command,
	    int want_reply);
int	lssh_channel_data_encode(lssh_buf *out,
	    const lssh_channel *channel, const void *data, size_t len);
int	lssh_channel_eof_encode(lssh_buf *out,
	    const lssh_channel *channel);
int	lssh_channel_close_encode(lssh_buf *out,
	    const lssh_channel *channel);
int	lssh_channel_window_adjust_encode(lssh_buf *out,
	    const lssh_channel *channel, uint32_t bytes);
int	lssh_channel_event_parse(const void *payload, size_t len,
	    lssh_channel_event *event);
int	lssh_client_open_session(lssh_transport *transport,
	    lssh_channel *channel, uint32_t local_id);
int	lssh_client_channel_request(lssh_transport *transport,
	    lssh_channel *channel, const char *request,
	    int want_reply, const void *extra, size_t extra_len);
int	lssh_client_channel_request_pty(lssh_transport *transport,
	    lssh_channel *channel, const char *term,
	    uint32_t cols, uint32_t rows, uint32_t width_px,
	    uint32_t height_px);
int	lssh_client_channel_request_window_change(lssh_transport *transport,
	    const lssh_channel *channel, uint32_t cols, uint32_t rows,
	    uint32_t width_px, uint32_t height_px);
int	lssh_client_channel_request_shell(lssh_transport *transport,
	    lssh_channel *channel);
int	lssh_client_channel_request_exec(lssh_transport *transport,
	    lssh_channel *channel, const char *command);
int	lssh_client_channel_send_data(lssh_transport *transport,
	    lssh_channel *channel, const void *data, size_t len);
int	lssh_client_channel_send_eof(lssh_transport *transport,
	    lssh_channel *channel);
int	lssh_client_channel_close(lssh_transport *transport,
	    lssh_channel *channel);
int	lssh_client_channel_adjust_window(lssh_transport *transport,
	    lssh_channel *channel, uint32_t bytes);
int	lssh_client_channel_recv_event(lssh_transport *transport,
	    lssh_channel *channel, lssh_buf *payload,
	    lssh_channel_event *event);

int	lssh_transport_init(lssh_transport *transport);
void	lssh_transport_free(lssh_transport *transport);
int	lssh_transport_attach(lssh_transport *transport, int handle,
	    int owns_handle);
void	lssh_transport_close(lssh_transport *transport);
int	lssh_transport_connect_ipv4(lssh_transport *transport,
	    uint32_t ip, uint32_t port);
int	lssh_transport_send_ident(lssh_transport *transport,
	    const char *software, const char *comment);
int	lssh_transport_recv_ident(lssh_transport *transport,
	    lssh_ident *out);
int	lssh_transport_exchange_ident(lssh_transport *transport,
	    const char *software, const char *comment, lssh_ident *out);
int	lssh_transport_send_plain(lssh_transport *transport,
	    const void *payload, size_t payload_len);
int	lssh_transport_recv_plain(lssh_transport *transport,
	    lssh_buf *payload);
int	lssh_transport_send_packet(lssh_transport *transport,
	    const void *payload, size_t payload_len);
int	lssh_transport_recv_packet(lssh_transport *transport,
	    lssh_buf *payload);
int	lssh_transport_packet_pending(const lssh_transport *transport);
int	lssh_transport_set_chachapoly(lssh_transport *transport,
	    const uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE],
	    const uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE]);
int	lssh_transport_set_chachapoly_read(lssh_transport *transport,
	    const uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE]);
int	lssh_transport_set_chachapoly_write(lssh_transport *transport,
	    const uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE]);
void	lssh_transport_clear_crypto(lssh_transport *transport);
int	lssh_transport_send_kexinit(lssh_transport *transport,
	    const lssh_kexinit_names *names, const uint8_t cookie[16]);
int	lssh_transport_recv_kexinit(lssh_transport *transport,
	    lssh_buf *payload, lssh_kexinit *out);

#endif
