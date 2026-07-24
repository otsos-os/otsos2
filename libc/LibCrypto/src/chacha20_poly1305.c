/* !DEFINES!

$define %type uint8_t as 8 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type size_t as object size
$define %func lc_aead_mac_update as procedure with args mac buffer, pos, data, len
$define %func lc_aead_mac_lengths as procedure with args mac buffer, pos, aad len, text len
$define %func lc_aead_mac as function with args key, nonce, aad, ciphertext, tag
$define %func lc_chacha20_poly1305_seal as function with args key, nonce, aad, plaintext, ciphertext, tag
$define %func lc_chacha20_poly1305_open as function with args key, nonce, aad, ciphertext, tag, plaintext

*/

/* !SPACE!

$space %internal lc_aead_mac_update, lc_aead_mac_lengths, lc_aead_mac
$space %export lc_chacha20_poly1305_seal
$space %export lc_chacha20_poly1305_open

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

#include <errno.h>
#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "private.h"

static void
lc_aead_mac_update(uint8_t *mac, size_t *pos, const void *data,
    size_t len)
{
	const uint8_t	*p;
	size_t		pad;

	if (len == 0) {
		return;
	}
	p = (const uint8_t *)data;
	memcpy(mac + *pos, p, len);
	*pos += len;
	pad = (16 - (len & 15)) & 15;
	if (pad != 0) {
		memset(mac + *pos, 0, pad);
		*pos += pad;
	}
}

static void
lc_aead_mac_lengths(uint8_t *mac, size_t *pos, size_t aad_len,
    size_t text_len)
{
	lc_store64_le(mac + *pos, (uint64_t)aad_len);
	*pos += 8;
	lc_store64_le(mac + *pos, (uint64_t)text_len);
	*pos += 8;
}

static int
lc_aead_mac(const uint8_t key[LC_CHACHA20_POLY1305_KEY_SIZE],
    const uint8_t nonce[LC_CHACHA20_POLY1305_NONCE_SIZE],
    const void *aad, size_t aad_len, const void *ciphertext,
    size_t ciphertext_len, uint8_t tag[LC_CHACHA20_POLY1305_TAG_SIZE])
{
	uint8_t	block[LC_CHACHA20_BLOCK_SIZE];
	uint8_t	poly_key[LC_POLY1305_KEY_SIZE];
	uint8_t	*mac;
	size_t	mac_len, pos;
	int	ret;

	mac_len = ((aad_len + 15) & ~(size_t)15) +
	    ((ciphertext_len + 15) & ~(size_t)15) + 16;
	mac = malloc(mac_len);
	if (!mac) {
		errno = ENOMEM;
		return (-1);
	}
	ret = lc_chacha20_block(key, nonce, 0, block);
	if (ret != 0) {
		free(mac);
		return (-1);
	}
	memcpy(poly_key, block, sizeof(poly_key));

	pos = 0;
	lc_aead_mac_update(mac, &pos, aad, aad_len);
	lc_aead_mac_update(mac, &pos, ciphertext, ciphertext_len);
	lc_aead_mac_lengths(mac, &pos, aad_len, ciphertext_len);
	lc_poly1305_auth(tag, mac, pos, poly_key);

	lc_wipe(block, sizeof(block));
	lc_wipe(poly_key, sizeof(poly_key));
	lc_wipe(mac, mac_len);
	free(mac);
	return (0);
}

int
lc_chacha20_poly1305_seal(
    const uint8_t key[LC_CHACHA20_POLY1305_KEY_SIZE],
    const uint8_t nonce[LC_CHACHA20_POLY1305_NONCE_SIZE],
    const void *aad, size_t aad_len, const void *plaintext,
    size_t plaintext_len, void *ciphertext,
    uint8_t tag[LC_CHACHA20_POLY1305_TAG_SIZE])
{
	lc_chacha20_ctx	ctx;
	int		ret;

	if (!key || !nonce || (!aad && aad_len != 0) ||
	    (!plaintext && plaintext_len != 0) ||
	    (!ciphertext && plaintext_len != 0) || !tag) {
		errno = EINVAL;
		return (-1);
	}
	if (lc_chacha20_init(&ctx, key, nonce, 1) != 0) {
		return (-1);
	}
	ret = lc_chacha20_xor(&ctx, plaintext, ciphertext, plaintext_len);
	lc_chacha20_wipe(&ctx);
	if (ret != 0) {
		return (-1);
	}
	ret = lc_aead_mac(key, nonce, aad, aad_len, ciphertext,
	    plaintext_len, tag);
	if (ret != 0) {
		lc_wipe(ciphertext, plaintext_len);
		return (-1);
	}
	return (0);
}

int
lc_chacha20_poly1305_open(
    const uint8_t key[LC_CHACHA20_POLY1305_KEY_SIZE],
    const uint8_t nonce[LC_CHACHA20_POLY1305_NONCE_SIZE],
    const void *aad, size_t aad_len, const void *ciphertext,
    size_t ciphertext_len,
    const uint8_t tag[LC_CHACHA20_POLY1305_TAG_SIZE],
    void *plaintext)
{
	lc_chacha20_ctx	ctx;
	uint8_t	calc[LC_CHACHA20_POLY1305_TAG_SIZE];
	int	ret;

	if (!key || !nonce || (!aad && aad_len != 0) ||
	    (!ciphertext && ciphertext_len != 0) ||
	    (!plaintext && ciphertext_len != 0) || !tag) {
		errno = EINVAL;
		return (-1);
	}
	ret = lc_aead_mac(key, nonce, aad, aad_len, ciphertext,
	    ciphertext_len, calc);
	if (ret != 0) {
		return (-1);
	}
	if (!lc_memeq(calc, tag, sizeof(calc))) {
		lc_wipe(calc, sizeof(calc));
		errno = EINVAL;
		return (-1);
	}
	lc_wipe(calc, sizeof(calc));
	if (lc_chacha20_init(&ctx, key, nonce, 1) != 0) {
		return (-1);
	}
	ret = lc_chacha20_xor(&ctx, ciphertext, plaintext, ciphertext_len);
	lc_chacha20_wipe(&ctx);
	if (ret != 0) {
		lc_wipe(plaintext, ciphertext_len);
		return (-1);
	}
	return (0);
}
