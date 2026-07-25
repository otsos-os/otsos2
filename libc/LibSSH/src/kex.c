/* !DEFINES!

$define %type lssh_slice as borrowed byte span
$define %type lssh_buf as growable SSH byte buffer
$define %type lssh_reader as bounded SSH byte reader
$define %type lssh_curve25519_keypair as client Curve25519 KEX keypair
$define %type lssh_kex_ecdh_reply as parsed SSH_MSG_KEX_ECDH_REPLY payload
$define %type uint8_t as 8 bit unsigned
$define %type size_t as object size
$define %func lssh_kex_slice_eq as function with args lssh_slice, const char *
$define %func lssh_kex_shared_nonzero as function with args const uint8_t *
$define %func lssh_kex_append_mpint as function with args lssh_buf *, const uint8_t *
$define %func lssh_kex_derive_key as function with args out, length, shared, hash, session, letter
$define %func lssh_curve25519_keypair_generate as function with args lssh_curve25519_keypair *
$define %func lssh_curve25519_keypair_free as procedure with args lssh_curve25519_keypair *
$define %func lssh_kex_ecdh_init_encode as function with args lssh_buf *, const uint8_t *
$define %func lssh_kex_ecdh_reply_parse as function with args const void *, size_t, lssh_kex_ecdh_reply *
$define %func lssh_kex_curve25519_shared as function with args keypair, server public, shared secret
$define %func lssh_kex_curve25519_hash as function with args exchange hash, idents, kexinit payloads, host key, public keys, shared secret
$define %func lssh_kex_parse_ed25519_hostkey as function with args host key blob, public key
$define %func lssh_kex_verify_ed25519 as function with args host key blob, signature blob, exchange hash, public key
$define %func lssh_kex_derive_chachapoly as function with args shared secret, exchange hash, session id, side, read key, write key

*/

/* !SPACE!

$space %internal lssh_kex_slice_eq, lssh_kex_shared_nonzero
$space %internal lssh_kex_append_mpint, lssh_kex_derive_key
$space %export lssh_curve25519_keypair_generate
$space %export lssh_curve25519_keypair_free
$space %export lssh_kex_ecdh_init_encode, lssh_kex_ecdh_reply_parse
$space %export lssh_kex_curve25519_shared, lssh_kex_curve25519_hash
$space %export lssh_kex_parse_ed25519_hostkey
$space %export lssh_kex_verify_ed25519, lssh_kex_derive_chachapoly

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

static int
lssh_kex_slice_eq(lssh_slice slice, const char *name)
{
	size_t	len;

	if (!name || (!slice.data && slice.len != 0)) {
		return (0);
	}
	len = strlen(name);
	if (slice.len != len) {
		return (0);
	}
	return (memcmp(slice.data, name, len) == 0);
}

static int
lssh_kex_shared_nonzero(const uint8_t shared[LSSH_CURVE25519_SIZE])
{
	uint8_t	acc;
	size_t	i;

	if (!shared) {
		return (0);
	}
	acc = 0;
	for (i = 0; i < LSSH_CURVE25519_SIZE; i++) {
		acc |= shared[i];
	}
	return (acc != 0);
}

static int
lssh_kex_append_mpint(lssh_buf *out,
    const uint8_t shared[LSSH_CURVE25519_SIZE])
{
	uint8_t	zero;
	size_t	off, len;
	int	ret;

	if (!out || !shared || !lssh_kex_shared_nonzero(shared)) {
		return (LSSH_ERR_INVALID);
	}
	off = 0;
	while (off < LSSH_CURVE25519_SIZE && shared[off] == 0) {
		off++;
	}
	len = LSSH_CURVE25519_SIZE - off;
	if ((shared[off] & 0x80) == 0) {
		return (lssh_buf_put_string(out, shared + off, len));
	}
	ret = lssh_buf_put_u32(out, (uint32_t)(len + 1));
	if (ret != LSSH_OK) {
		return (ret);
	}
	zero = 0;
	ret = lssh_buf_append(out, &zero, 1);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_append(out, shared + off, len));
}

static int
lssh_kex_derive_key(uint8_t *out, size_t out_len,
    const uint8_t shared[LSSH_CURVE25519_SIZE],
    const uint8_t exchange_hash[LSSH_SHA256_SIZE],
    const uint8_t session_id[LSSH_SHA256_SIZE], uint8_t letter)
{
	lssh_buf	buf;
	uint8_t		digest[LSSH_SHA256_SIZE];
	size_t		produced, copy_len;
	int		ret;

	if (!out || !shared || !exchange_hash || !session_id) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&buf, 128);
	if (ret != LSSH_OK) {
		return (ret);
	}
	produced = 0;
	while (produced < out_len) {
		lssh_buf_reset(&buf);
		ret = lssh_kex_append_mpint(&buf, shared);
		if (ret != LSSH_OK) {
			break;
		}
		ret = lssh_buf_append(&buf, exchange_hash, LSSH_SHA256_SIZE);
		if (ret != LSSH_OK) {
			break;
		}
		if (produced == 0) {
			ret = lssh_buf_put_u8(&buf, letter);
			if (ret != LSSH_OK) {
				break;
			}
			ret = lssh_buf_append(&buf, session_id,
			    LSSH_SHA256_SIZE);
			if (ret != LSSH_OK) {
				break;
			}
		} else {
			ret = lssh_buf_append(&buf, out, produced);
			if (ret != LSSH_OK) {
				break;
			}
		}
		lc_sha256(buf.data, buf.len, digest);
		copy_len = out_len - produced;
		if (copy_len > sizeof(digest)) {
			copy_len = sizeof(digest);
		}
		memcpy(out + produced, digest, copy_len);
		produced += copy_len;
	}
	lc_wipe(digest, sizeof(digest));
	lssh_buf_free(&buf);
	if (ret != LSSH_OK) {
		lc_wipe(out, out_len);
		return (ret);
	}
	return (LSSH_OK);
}

int
lssh_curve25519_keypair_generate(lssh_curve25519_keypair *keypair)
{
	if (!keypair) {
		return (LSSH_ERR_INVALID);
	}
	memset(keypair, 0, sizeof(*keypair));
	if (lc_random(keypair->private_key, LSSH_CURVE25519_SIZE) != 0) {
		return (LSSH_ERR_CRYPTO);
	}
	if (lc_curve25519_public(keypair->public_key,
	    keypair->private_key) != 0) {
		lssh_curve25519_keypair_free(keypair);
		return (LSSH_ERR_CRYPTO);
	}
	keypair->valid = 1;
	return (LSSH_OK);
}

void
lssh_curve25519_keypair_free(lssh_curve25519_keypair *keypair)
{
	if (!keypair) {
		return;
	}
	lc_wipe(keypair, sizeof(*keypair));
}

int
lssh_kex_ecdh_init_encode(lssh_buf *out,
    const uint8_t public_key[LSSH_CURVE25519_SIZE])
{
	int	ret;

	if (!out || !public_key) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_put_u8(out, LSSH_MSG_KEX_ECDH_INIT);
	if (ret != LSSH_OK) {
		return (ret);
	}
	return (lssh_buf_put_string(out, public_key, LSSH_CURVE25519_SIZE));
}

int
lssh_kex_ecdh_reply_parse(const void *payload, size_t len,
    lssh_kex_ecdh_reply *out)
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
	if (msg != LSSH_MSG_KEX_ECDH_REPLY) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_string(&reader, &out->host_key_blob);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_string(&reader, &out->server_public);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (out->server_public.len != LSSH_CURVE25519_SIZE) {
		return (LSSH_ERR_FORMAT);
	}
	ret = lssh_reader_string(&reader, &out->signature_blob);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0) {
		return (LSSH_ERR_FORMAT);
	}
	return (LSSH_OK);
}

int
lssh_kex_curve25519_shared(const lssh_curve25519_keypair *keypair,
    lssh_slice server_public,
    uint8_t shared_secret[LSSH_CURVE25519_SIZE])
{
	if (!keypair || !keypair->valid || !server_public.data ||
	    server_public.len != LSSH_CURVE25519_SIZE || !shared_secret) {
		return (LSSH_ERR_INVALID);
	}
	if (lc_curve25519(shared_secret, keypair->private_key,
	    server_public.data) != 0) {
		return (LSSH_ERR_CRYPTO);
	}
	if (!lssh_kex_shared_nonzero(shared_secret)) {
		lc_wipe(shared_secret, LSSH_CURVE25519_SIZE);
		return (LSSH_ERR_VERIFY);
	}
	return (LSSH_OK);
}

int
lssh_kex_curve25519_hash(uint8_t exchange_hash[LSSH_SHA256_SIZE],
    lssh_slice client_ident, lssh_slice server_ident,
    lssh_slice client_kexinit, lssh_slice server_kexinit,
    lssh_slice host_key_blob,
    const uint8_t client_public[LSSH_CURVE25519_SIZE],
    const uint8_t server_public[LSSH_CURVE25519_SIZE],
    const uint8_t shared_secret[LSSH_CURVE25519_SIZE])
{
	lssh_buf	buf;
	int		ret;

	if (!exchange_hash || !client_public || !server_public ||
	    !shared_secret || !lssh_kex_shared_nonzero(shared_secret)) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_buf_init(&buf, 512);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_buf_put_string(&buf, client_ident.data, client_ident.len);
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, server_ident.data,
		    server_ident.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, client_kexinit.data,
		    client_kexinit.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, server_kexinit.data,
		    server_kexinit.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, host_key_blob.data,
		    host_key_blob.len);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, client_public,
		    LSSH_CURVE25519_SIZE);
	}
	if (ret == LSSH_OK) {
		ret = lssh_buf_put_string(&buf, server_public,
		    LSSH_CURVE25519_SIZE);
	}
	if (ret == LSSH_OK) {
		ret = lssh_kex_append_mpint(&buf, shared_secret);
	}
	if (ret == LSSH_OK) {
		lc_sha256(buf.data, buf.len, exchange_hash);
	}
	lssh_buf_free(&buf);
	return (ret);
}

int
lssh_kex_parse_ed25519_hostkey(lssh_slice host_key_blob,
    uint8_t public_key[LSSH_ED25519_PUBLIC_KEY_SIZE])
{
	lssh_reader	reader;
	lssh_slice	alg, key;
	int		ret;

	if (!host_key_blob.data || !public_key) {
		return (LSSH_ERR_INVALID);
	}
	lssh_reader_init(&reader, host_key_blob.data, host_key_blob.len);
	ret = lssh_reader_string(&reader, &alg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_string(&reader, &key);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0 ||
	    !lssh_kex_slice_eq(alg, "ssh-ed25519") ||
	    key.len != LSSH_ED25519_PUBLIC_KEY_SIZE) {
		return (LSSH_ERR_FORMAT);
	}
	memcpy(public_key, key.data, LSSH_ED25519_PUBLIC_KEY_SIZE);
	return (LSSH_OK);
}

int
lssh_kex_verify_ed25519(lssh_slice host_key_blob,
    lssh_slice signature_blob,
    const uint8_t exchange_hash[LSSH_SHA256_SIZE],
    uint8_t public_key[LSSH_ED25519_PUBLIC_KEY_SIZE])
{
	lssh_reader	reader;
	lssh_slice	alg, sig;
	int		ret;

	if (!signature_blob.data || !exchange_hash || !public_key) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_kex_parse_ed25519_hostkey(host_key_blob, public_key);
	if (ret != LSSH_OK) {
		return (ret);
	}
	lssh_reader_init(&reader, signature_blob.data, signature_blob.len);
	ret = lssh_reader_string(&reader, &alg);
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_reader_string(&reader, &sig);
	if (ret != LSSH_OK) {
		return (ret);
	}
	if (lssh_reader_remaining(&reader) != 0 ||
	    !lssh_kex_slice_eq(alg, "ssh-ed25519") ||
	    sig.len != LSSH_ED25519_SIGNATURE_SIZE) {
		return (LSSH_ERR_FORMAT);
	}
	if (lc_ed25519_verify(public_key, exchange_hash, LSSH_SHA256_SIZE,
	    sig.data) != 0) {
		return (LSSH_ERR_VERIFY);
	}
	return (LSSH_OK);
}

int
lssh_kex_derive_chachapoly(
    const uint8_t shared_secret[LSSH_CURVE25519_SIZE],
    const uint8_t exchange_hash[LSSH_SHA256_SIZE],
    const uint8_t session_id[LSSH_SHA256_SIZE], int client_side,
    uint8_t read_key[LSSH_CHACHAPOLY_KEY_SIZE],
    uint8_t write_key[LSSH_CHACHAPOLY_KEY_SIZE])
{
	uint8_t	c2s[LSSH_CHACHAPOLY_KEY_SIZE];
	uint8_t	s2c[LSSH_CHACHAPOLY_KEY_SIZE];
	int	ret;

	if (!read_key || !write_key) {
		return (LSSH_ERR_INVALID);
	}
	ret = lssh_kex_derive_key(c2s, sizeof(c2s), shared_secret,
	    exchange_hash, session_id, 'C');
	if (ret != LSSH_OK) {
		return (ret);
	}
	ret = lssh_kex_derive_key(s2c, sizeof(s2c), shared_secret,
	    exchange_hash, session_id, 'D');
	if (ret != LSSH_OK) {
		lc_wipe(c2s, sizeof(c2s));
		return (ret);
	}
	if (client_side) {
		memcpy(write_key, c2s, sizeof(c2s));
		memcpy(read_key, s2c, sizeof(s2c));
	} else {
		memcpy(write_key, s2c, sizeof(s2c));
		memcpy(read_key, c2s, sizeof(c2s));
	}
	lc_wipe(c2s, sizeof(c2s));
	lc_wipe(s2c, sizeof(s2c));
	return (LSSH_OK);
}
