/* !DEFINES!

$define %type lssh_transport as native SSH transport session
$define %type lssh_client_handshake_result as completed client transport handshake data
$define %type lssh_curve25519_keypair as client Curve25519 KEX keypair
$define %type lssh_kex_ecdh_reply as parsed SSH_MSG_KEX_ECDH_REPLY payload
$define %type lssh_kexinit as parsed SSH_MSG_KEXINIT payload
$define %type lssh_kexinit_names as configured KEXINIT name-lists
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_slice as borrowed byte span
$define %type uint8_t as 8 bit unsigned
$define %func lssh_client_slice as function with args const void *, size_t
$define %func lssh_client_expect_newkeys as function with args const lssh_buf *
$define %func lssh_client_init_bufs as function with args lssh_buf *
$define %func lssh_client_free_bufs as procedure with args lssh_buf *
$define %func lssh_client_handshake as function with args lssh_transport *, lssh_client_handshake_result *, const char *, const char *

*/

/* !SPACE!

$space %internal lssh_client_slice, lssh_client_expect_newkeys
$space %internal lssh_client_init_bufs, lssh_client_free_bufs
$space %export lssh_client_handshake

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

#include <libcrypto.h>
#include <libssh.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

static lssh_slice
lssh_client_slice(const void *data, size_t len)
{
	lssh_slice	slice;

	slice.data = (const uint8_t *)data;
	slice.len = len;
	return (slice);
}

static int
lssh_client_expect_newkeys(const lssh_buf *payload)
{
	if (!payload || payload->len != 1 ||
	    payload->data[0] != LSSH_MSG_NEWKEYS) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

static int
lssh_client_init_bufs(lssh_buf bufs[5])
{
	int	i, ret;

	for (i = 0; i < 5; i++) {
		ret = lssh_buf_init(&bufs[i], 512);
		if (ret != LSSH_OK) {
			while (i > 0) {
				i--;
				lssh_buf_free(&bufs[i]);
			}
			return (ret);
		}
		lssh_buf_set_secure(&bufs[i], 1);
	}
	return (LSSH_OK);
}

static void
lssh_client_free_bufs(lssh_buf bufs[5])
{
	int	i;

	for (i = 0; i < 5; i++) {
		lssh_buf_free(&bufs[i]);
	}
}

int
lssh_client_handshake(lssh_transport *transport,
    lssh_client_handshake_result *result,
    const char *software, const char *comment)
{
	lssh_buf			bufs[5];
	lssh_slice			client_ident, server_ident;
	lssh_kexinit_names		names;
	lssh_kexinit			client_kex, server_kex;
	lssh_curve25519_keypair	keypair;
	lssh_kex_ecdh_reply		reply;
	uint8_t				shared[LSSH_CURVE25519_SIZE];
	uint8_t				read_key[LSSH_CHACHAPOLY_KEY_SIZE];
	uint8_t				write_key[LSSH_CHACHAPOLY_KEY_SIZE];
	uint8_t				newkeys_msg;
	int				ret, strict_kex;

	if (!transport || !result) {
		return (LSSH_ERR_INVALID);
	}
	memset(result, 0, sizeof(*result));
	memset(&keypair, 0, sizeof(keypair));
	memset(shared, 0, sizeof(shared));
	memset(read_key, 0, sizeof(read_key));
	memset(write_key, 0, sizeof(write_key));
	strict_kex = 0;
	ret = lssh_client_init_bufs(bufs);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lssh_logf(LSSH_LOG_INFO, "handshake: starting client transport");

	ret = lssh_transport_exchange_ident(transport, software, comment,
	    &result->peer_ident);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_INFO, "ident: local='%s' peer='%s'",
	    transport->local_ident, transport->peer_ident_line);

	lssh_kexinit_default_names(&names);
	ret = lssh_kexinit_encode(&bufs[0], &names, NULL);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_kexinit_parse(bufs[0].data, bufs[0].len, &client_kex);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_transport_send_packet(transport, bufs[0].data,
	    bufs[0].len);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_DEBUG, "kex: sent KEXINIT len=%lu",
	    (unsigned long)bufs[0].len);
	ret = lssh_transport_recv_packet(transport, &bufs[1]);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_DEBUG, "kex: received KEXINIT len=%lu",
	    (unsigned long)bufs[1].len);
	ret = lssh_kexinit_parse(bufs[1].data, bufs[1].len, &server_kex);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_kexinit_negotiate(&client_kex, &server_kex,
	    &result->algorithms);
	if (ret != LSSH_OK) {
		goto out;
	}
	strict_kex = lssh_namelist_contains(client_kex.kex_algorithms,
	    "kex-strict-c-v00@openssh.com") &&
	    lssh_namelist_contains(server_kex.kex_algorithms,
	    "kex-strict-s-v00@openssh.com");
	lssh_logf(LSSH_LOG_INFO,
	    "kex: negotiated kex=%s hostkey=%s c2s=%s s2c=%s strict=%d",
	    result->algorithms.kex, result->algorithms.server_host_key,
	    result->algorithms.encryption_client_to_server,
	    result->algorithms.encryption_server_to_client, strict_kex);

	ret = lssh_curve25519_keypair_generate(&keypair);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_kex_ecdh_init_encode(&bufs[2], keypair.public_key);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_transport_send_packet(transport, bufs[2].data,
	    bufs[2].len);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_transport_recv_packet(transport, &bufs[3]);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_DEBUG, "kex: received ECDH_REPLY len=%lu",
	    (unsigned long)bufs[3].len);
	ret = lssh_kex_ecdh_reply_parse(bufs[3].data, bufs[3].len,
	    &reply);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_kex_curve25519_shared(&keypair, reply.server_public,
	    shared);
	if (ret != LSSH_OK) {
		goto out;
	}
	client_ident = lssh_client_slice(transport->local_ident,
	    transport->local_ident_len);
	server_ident = lssh_client_slice(transport->peer_ident_line,
	    transport->peer_ident_len);
	ret = lssh_kex_curve25519_hash(result->exchange_hash,
	    client_ident, server_ident, lssh_client_slice(bufs[0].data,
	    bufs[0].len), lssh_client_slice(bufs[1].data, bufs[1].len),
	    reply.host_key_blob, keypair.public_key,
	    reply.server_public.data, shared);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_kex_verify_ed25519(reply.host_key_blob,
	    reply.signature_blob, result->exchange_hash, result->host_key);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_INFO, "kex: server host-key signature verified");
	if (reply.host_key_blob.len > sizeof(result->host_key_blob)) {
		ret = LSSH_ERR_RANGE;
		goto out;
	}
	memcpy(result->host_key_blob, reply.host_key_blob.data,
	    reply.host_key_blob.len);
	result->host_key_blob_len = reply.host_key_blob.len;
	memcpy(result->session_id, result->exchange_hash,
	    sizeof(result->session_id));
	ret = lssh_kex_derive_chachapoly(shared, result->exchange_hash,
	    result->session_id, 1, read_key, write_key);
	if (ret != LSSH_OK) {
		goto out;
	}
	lssh_logf(LSSH_LOG_DEBUG, "kex: derived chachapoly keys");

	newkeys_msg = LSSH_MSG_NEWKEYS;
	ret = lssh_transport_send_packet(transport, &newkeys_msg, 1);
	if (ret != LSSH_OK) {
		goto out;
	}
	if (strict_kex) {
		transport->seq_out = 0;
		lssh_logf(LSSH_LOG_INFO, "kex: strict reset seq_out");
	}
	ret = lssh_transport_set_chachapoly_write(transport, write_key);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_transport_recv_packet(transport, &bufs[4]);
	if (ret != LSSH_OK) {
		goto out;
	}
	ret = lssh_client_expect_newkeys(&bufs[4]);
	if (ret != LSSH_OK) {
		goto out;
	}
	if (strict_kex) {
		transport->seq_in = 0;
		lssh_logf(LSSH_LOG_INFO, "kex: strict reset seq_in");
	}
	ret = lssh_transport_set_chachapoly_read(transport, read_key);

out:
	lc_wipe(shared, sizeof(shared));
	lc_wipe(read_key, sizeof(read_key));
	lc_wipe(write_key, sizeof(write_key));
	lssh_curve25519_keypair_free(&keypair);
	lssh_client_free_bufs(bufs);
	if (ret != LSSH_OK) {
		lssh_logf(LSSH_LOG_ERROR, "handshake: failed ret=%d", ret);
		memset(result, 0, sizeof(*result));
	} else {
		lssh_logf(LSSH_LOG_INFO, "handshake: complete");
	}
	return (ret);
}
