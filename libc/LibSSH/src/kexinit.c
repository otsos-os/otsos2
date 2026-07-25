/* !DEFINES!

$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_slice as borrowed byte span
$define %type lssh_kexinit as parsed SSH_MSG_KEXINIT payload
$define %type lssh_kexinit_names as configured KEXINIT name-lists
$define %type lssh_algorithms as negotiated SSH transport algorithms
$define %type uint8_t as 8 bit unsigned
$define %func lssh_kexinit_name_valid as function with args const uint8_t *, size_t
$define %func lssh_kexinit_list_valid as function with args lssh_slice
$define %func lssh_kexinit_put_namelist as function with args lssh_buf *, const char *
$define %func lssh_kexinit_read_namelist as function with args lssh_reader *, lssh_slice *
$define %func lssh_kexinit_choose as function with args lssh_slice, lssh_slice, char *, size_t
$define %func lssh_kexinit_supported as function with args const lssh_algorithms *
$define %func lssh_kexinit_default_names as procedure with args lssh_kexinit_names *
$define %func lssh_kexinit_encode as function with args lssh_buf *, const lssh_kexinit_names *, const uint8_t *
$define %func lssh_kexinit_parse as function with args const void *, size_t, lssh_kexinit *
$define %func lssh_kexinit_negotiate as function with args const lssh_kexinit *, const lssh_kexinit *, lssh_algorithms *

*/

/* !SPACE!

$space %internal lssh_kexinit_name_valid, lssh_kexinit_list_valid
$space %internal lssh_kexinit_put_namelist, lssh_kexinit_read_namelist
$space %internal lssh_kexinit_choose, lssh_kexinit_supported
$space %export lssh_kexinit_default_names, lssh_kexinit_encode
$space %export lssh_kexinit_parse, lssh_kexinit_negotiate

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

static int
lssh_kexinit_name_valid(const uint8_t *data, size_t len)
{
	size_t	i;
	uint8_t	ch;

	if (!data || len == 0) {
		return (0);
	}
	for (i = 0; i < len; i++) {
		ch = data[i];
		if (ch <= 32 || ch >= 127 || ch == ',') {
			return (0);
		}
	}
	return (1);
}

static int
lssh_kexinit_list_valid(lssh_slice list)
{
	size_t	start, i;

	if (!list.data && list.len != 0) {
		return (0);
	}
	if (list.len == 0) {
		return (1);
	}
	start = 0;
	for (i = 0; i <= list.len; i++) {
		if (i != list.len && list.data[i] != ',') {
			continue;
		}
		if (!lssh_kexinit_name_valid(list.data + start,
		    i - start)) {
			return (0);
		}
		start = i + 1;
	}
	return (1);
}

static int
lssh_kexinit_put_namelist(lssh_buf *out, const char *list)
{
	lssh_slice	slice;

	if (!list) {
		return (LSSH_ERR_INVALID);
	}
	slice = lssh_slice_cstr(list);
	if (!lssh_kexinit_list_valid(slice)) {
		return (LSSH_ERR_FORMAT);
	}
	return (lssh_buf_put_string(out, list, slice.len));
}

static int
lssh_kexinit_read_namelist(lssh_reader *reader, lssh_slice *out)
{
	int	ret;

	ret = lssh_reader_string(reader, out);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (!lssh_kexinit_list_valid(*out)) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

static int
lssh_kexinit_choose(lssh_slice client, lssh_slice server,
    char *out, size_t out_size)
{
	if (!lssh_kexinit_list_valid(client) ||
	    !lssh_kexinit_list_valid(server)) {
		return (LSSH_ERR_FORMAT);
	}
	return (lssh_namelist_first_match(client, server, out, out_size));
}

static int
lssh_kexinit_supported(const lssh_algorithms *alg)
{
	if (!alg) {
		return (0);
	}
	if (!(strcmp(alg->kex, "curve25519-sha256") == 0 ||
	    strcmp(alg->kex, "curve25519-sha256@libssh.org") == 0)) {
		return (0);
	}
	if (strcmp(alg->server_host_key, "ssh-ed25519") != 0) {
		return (0);
	}
	if (strcmp(alg->encryption_client_to_server,
	    "chacha20-poly1305@openssh.com") != 0) {
		return (0);
	}
	if (strcmp(alg->encryption_server_to_client,
	    "chacha20-poly1305@openssh.com") != 0) {
		return (0);
	}
	if (strcmp(alg->compression_client_to_server, "none") != 0 ||
	    strcmp(alg->compression_server_to_client, "none") != 0) {
		return (0);
	}
	return (1);
}

void
lssh_kexinit_default_names(lssh_kexinit_names *names)
{
	if (!names) {
		return;
	}
	names->kex_algorithms = LSSH_KEX_DEFAULT;
	names->server_host_key_algorithms = LSSH_HOSTKEY_DEFAULT;
	names->encryption_algorithms_client_to_server = LSSH_CIPHER_DEFAULT;
	names->encryption_algorithms_server_to_client = LSSH_CIPHER_DEFAULT;
	names->mac_algorithms_client_to_server = LSSH_MAC_DEFAULT;
	names->mac_algorithms_server_to_client = LSSH_MAC_DEFAULT;
	names->compression_algorithms_client_to_server = LSSH_COMP_DEFAULT;
	names->compression_algorithms_server_to_client = LSSH_COMP_DEFAULT;
	names->languages_client_to_server = "";
	names->languages_server_to_client = "";
}

int
lssh_kexinit_encode(lssh_buf *out, const lssh_kexinit_names *names,
    const uint8_t cookie[16])
{
	lssh_kexinit_names	defaults;
	uint8_t			generated_cookie[16];
	const uint8_t		*use_cookie;
	int			ret;

	if (!out) {
		return (LSSH_ERR_INVALID);
	}
	if (!names) {
		lssh_kexinit_default_names(&defaults);
		names = &defaults;
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_KEXINIT);
	if (ret != LSSH_OK) {
		return (ret);
	}
	use_cookie = cookie;
	if (!use_cookie) {
		if (lc_random(generated_cookie, sizeof(generated_cookie)) != 0) {
			return (LSSH_ERR_CRYPTO);
		}
		use_cookie = generated_cookie;
	}
	ret = lssh_buf_append(out, use_cookie, 16);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out, names->kex_algorithms);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->server_host_key_algorithms);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->encryption_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->encryption_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->mac_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->mac_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->compression_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->compression_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->languages_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_put_namelist(out,
	    names->languages_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_u8(out, 0);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_u32(out, 0));
}

int
lssh_kexinit_parse(const void *payload, size_t len, lssh_kexinit *out)
{
	lssh_reader	reader;
	uint8_t		msg, first_kex_packet_follows;
	uint32_t	reserved;
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
	if (msg != LSSH_MSG_KEXINIT) {
		return (LSSH_ERR_FORMAT);
	}
	if (lssh_reader_remaining(&reader) < 16) {
		return (LSSH_ERR_FORMAT);
	}
	memcpy(out->cookie, reader.data + reader.off, 16);
	reader.off += 16;
	ret = lssh_kexinit_read_namelist(&reader, &out->kex_algorithms);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->server_host_key_algorithms);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->encryption_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->encryption_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->mac_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->mac_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->compression_algorithms_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->compression_algorithms_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->languages_client_to_server);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_read_namelist(&reader,
	    &out->languages_server_to_client);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_u8(&reader, &first_kex_packet_follows);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (first_kex_packet_follows > 1) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_u32(&reader, &reserved);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (reserved != 0 || lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	out->first_kex_packet_follows = first_kex_packet_follows;
	out->reserved = reserved;
	return (LSSH_OK);
}

int
lssh_kexinit_negotiate(const lssh_kexinit *client,
    const lssh_kexinit *server, lssh_algorithms *out)
{
	int	ret;

	if (!client || !server || !out) {
		return (LSSH_ERR_INVALID);
	}
	memset(out, 0, sizeof(*out));
	ret = lssh_kexinit_choose(client->kex_algorithms,
	    server->kex_algorithms, out->kex, sizeof(out->kex));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(client->server_host_key_algorithms,
	    server->server_host_key_algorithms, out->server_host_key,
	    sizeof(out->server_host_key));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(
	    client->encryption_algorithms_client_to_server,
	    server->encryption_algorithms_client_to_server,
	    out->encryption_client_to_server,
	    sizeof(out->encryption_client_to_server));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(
	    client->encryption_algorithms_server_to_client,
	    server->encryption_algorithms_server_to_client,
	    out->encryption_server_to_client,
	    sizeof(out->encryption_server_to_client));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(client->mac_algorithms_client_to_server,
	    server->mac_algorithms_client_to_server,
	    out->mac_client_to_server, sizeof(out->mac_client_to_server));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(client->mac_algorithms_server_to_client,
	    server->mac_algorithms_server_to_client,
	    out->mac_server_to_client, sizeof(out->mac_server_to_client));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(
	    client->compression_algorithms_client_to_server,
	    server->compression_algorithms_client_to_server,
	    out->compression_client_to_server,
	    sizeof(out->compression_client_to_server));
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kexinit_choose(
	    client->compression_algorithms_server_to_client,
	    server->compression_algorithms_server_to_client,
	    out->compression_server_to_client,
	    sizeof(out->compression_server_to_client));
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (!lssh_kexinit_supported(out)) {
		return (LSSH_ERR_UNSUPPORTED);
	}
	return (LSSH_OK);
}
