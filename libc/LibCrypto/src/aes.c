/* !DEFINES!

$define %type lc_aes_ctx as AES expanded key schedule
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %func xtime as function with args uint8_t
$define %func gmul as function with args uint8_t, uint8_t
$define %func aes_expand as function with args lc_aes_ctx *, const uint8_t *, uint32_t
$define %func aes_encrypt_block as procedure with args const lc_aes_ctx *, const uint8_t *, uint8_t *
$define %func aes_decrypt_block as procedure with args const lc_aes_ctx *, const uint8_t *, uint8_t *
$define %func xor_block as procedure with args uint8_t *, const uint8_t *, const uint8_t *
$define %func lc_aes_init as function with args lc_aes_ctx *, const void *, uint32_t
$define %func lc_aes_wipe as procedure with args lc_aes_ctx *
$define %func lc_aes_ige_encrypt as function with args ctx, iv, in, out, len
$define %func lc_aes_ige_decrypt as function with args ctx, iv, in, out, len

*/

/* !SPACE!

$space %internal xtime, gmul, aes_expand, aes_encrypt_block
$space %internal aes_decrypt_block, xor_block
$space %export lc_aes_init, lc_aes_wipe
$space %export lc_aes_ige_encrypt, lc_aes_ige_decrypt

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

#include <libcrypto.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const uint8_t	g_sbox[256] = {
	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5,
	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc,
	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b,
	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85,
	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17,
	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88,
	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9,
	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6,
	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94,
	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68,
	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};

static const uint8_t	g_inv_sbox[256] = {
	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38,
	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d,
	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2,
	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda,
	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a,
	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea,
	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85,
	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20,
	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31,
	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0,
	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26,
	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

static const uint8_t	g_rcon[11] = {
	0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
	0x80, 0x1b, 0x36
};

static uint8_t
xtime(uint8_t a)
{
	return ((uint8_t)((a << 1) ^ (((a >> 7) & 1) * 0x1b)));
}

static uint8_t
gmul(uint8_t a, uint8_t b)
{
	uint8_t	result;
	int	i;

	result = 0;
	for (i = 0; i < 8; i++) {
		if ((b & 1) != 0) {
			result ^= a;
		}
		a = xtime(a);
		b >>= 1;
	}
	return (result);
}

static int
aes_expand(lc_aes_ctx *ctx, const uint8_t *key, uint32_t key_len)
{
	uint8_t		tmp[4], t;
	uint32_t	nk, nr, i, j;

	switch (key_len) {
	case 16:
		nk = 4;
		nr = 10;
		break;
	case 24:
		nk = 6;
		nr = 12;
		break;
	case 32:
		nk = 8;
		nr = 14;
		break;
	default:
		return (-1);
	}
	ctx->rounds = nr;

	memcpy(ctx->key, key, key_len);
	for (i = nk; i < 4 * (nr + 1); i++) {
		for (j = 0; j < 4; j++) {
			tmp[j] = ctx->key[(i - 1) * 4 + j];
		}
		if (i % nk == 0) {
			t = tmp[0];
			tmp[0] = (uint8_t)(g_sbox[tmp[1]] ^ g_rcon[i / nk]);
			tmp[1] = g_sbox[tmp[2]];
			tmp[2] = g_sbox[tmp[3]];
			tmp[3] = g_sbox[t];
		} else if (nk > 6 && i % nk == 4) {
			for (j = 0; j < 4; j++) {
				tmp[j] = g_sbox[tmp[j]];
			}
		}
		for (j = 0; j < 4; j++) {
			ctx->key[i * 4 + j] =
			    (uint8_t)(ctx->key[(i - nk) * 4 + j] ^ tmp[j]);
		}
	}
	return (0);
}

static void
aes_encrypt_block(const lc_aes_ctx *ctx, const uint8_t *in, uint8_t *out)
{
	uint8_t		s[16], t[16];
	uint32_t	round, c, i;

	memcpy(s, in, 16);
	for (i = 0; i < 16; i++) {
		s[i] ^= ctx->key[i];
	}

	for (round = 1; round <= ctx->rounds; round++) {
		for (i = 0; i < 16; i++) {
			s[i] = g_sbox[s[i]];
		}
		for (c = 0; c < 4; c++) {
			t[c * 4 + 0] = s[c * 4 + 0];
			t[c * 4 + 1] = s[((c + 1) % 4) * 4 + 1];
			t[c * 4 + 2] = s[((c + 2) % 4) * 4 + 2];
			t[c * 4 + 3] = s[((c + 3) % 4) * 4 + 3];
		}
		if (round != ctx->rounds) {
			for (c = 0; c < 4; c++) {
				s[c * 4 + 0] = (uint8_t)(gmul(t[c * 4 + 0], 2) ^
				    gmul(t[c * 4 + 1], 3) ^ t[c * 4 + 2] ^
				    t[c * 4 + 3]);
				s[c * 4 + 1] = (uint8_t)(t[c * 4 + 0] ^
				    gmul(t[c * 4 + 1], 2) ^
				    gmul(t[c * 4 + 2], 3) ^ t[c * 4 + 3]);
				s[c * 4 + 2] = (uint8_t)(t[c * 4 + 0] ^
				    t[c * 4 + 1] ^ gmul(t[c * 4 + 2], 2) ^
				    gmul(t[c * 4 + 3], 3));
				s[c * 4 + 3] = (uint8_t)(gmul(t[c * 4 + 0], 3) ^
				    t[c * 4 + 1] ^ t[c * 4 + 2] ^
				    gmul(t[c * 4 + 3], 2));
			}
		} else {
			memcpy(s, t, 16);
		}
		for (i = 0; i < 16; i++) {
			s[i] ^= ctx->key[round * 16 + i];
		}
	}
	memcpy(out, s, 16);
}

static void
aes_decrypt_block(const lc_aes_ctx *ctx, const uint8_t *in, uint8_t *out)
{
	uint8_t		s[16], t[16];
	uint32_t	round, c, i;

	memcpy(s, in, 16);
	for (i = 0; i < 16; i++) {
		s[i] ^= ctx->key[ctx->rounds * 16 + i];
	}

	for (round = ctx->rounds; round >= 1; round--) {
		for (c = 0; c < 4; c++) {
			t[c * 4 + 0] = s[c * 4 + 0];
			t[c * 4 + 1] = s[((c + 3) % 4) * 4 + 1];
			t[c * 4 + 2] = s[((c + 2) % 4) * 4 + 2];
			t[c * 4 + 3] = s[((c + 1) % 4) * 4 + 3];
		}
		for (i = 0; i < 16; i++) {
			t[i] = g_inv_sbox[t[i]];
		}
		for (i = 0; i < 16; i++) {
			t[i] ^= ctx->key[(round - 1) * 16 + i];
		}
		if (round != 1) {
			for (c = 0; c < 4; c++) {
				s[c * 4 + 0] = (uint8_t)(
				    gmul(t[c * 4 + 0], 0x0e) ^
				    gmul(t[c * 4 + 1], 0x0b) ^
				    gmul(t[c * 4 + 2], 0x0d) ^
				    gmul(t[c * 4 + 3], 0x09));
				s[c * 4 + 1] = (uint8_t)(
				    gmul(t[c * 4 + 0], 0x09) ^
				    gmul(t[c * 4 + 1], 0x0e) ^
				    gmul(t[c * 4 + 2], 0x0b) ^
				    gmul(t[c * 4 + 3], 0x0d));
				s[c * 4 + 2] = (uint8_t)(
				    gmul(t[c * 4 + 0], 0x0d) ^
				    gmul(t[c * 4 + 1], 0x09) ^
				    gmul(t[c * 4 + 2], 0x0e) ^
				    gmul(t[c * 4 + 3], 0x0b));
				s[c * 4 + 3] = (uint8_t)(
				    gmul(t[c * 4 + 0], 0x0b) ^
				    gmul(t[c * 4 + 1], 0x0d) ^
				    gmul(t[c * 4 + 2], 0x09) ^
				    gmul(t[c * 4 + 3], 0x0e));
			}
		} else {
			memcpy(s, t, 16);
		}
	}
	memcpy(out, s, 16);
}

static void
xor_block(uint8_t *dst, const uint8_t *a, const uint8_t *b)
{
	int	i;

	for (i = 0; i < LC_AES_BLOCK_SIZE; i++) {
		dst[i] = (uint8_t)(a[i] ^ b[i]);
	}
}

int
lc_aes_init(lc_aes_ctx *ctx, const void *key, uint32_t key_len)
{
	if (ctx == NULL || key == NULL) {
		return (-1);
	}
	memset(ctx, 0, sizeof(*ctx));
	return (aes_expand(ctx, (const uint8_t *)key, key_len));
}

void
lc_aes_wipe(lc_aes_ctx *ctx)
{
	if (ctx != NULL) {
		lc_wipe(ctx, sizeof(*ctx));
	}
}


int
lc_aes_ige_encrypt(lc_aes_ctx *ctx, uint8_t iv[LC_AES_IGE_IV_SIZE],
    const void *in, void *out, size_t len)
{
	uint8_t		prev_c[LC_AES_BLOCK_SIZE], prev_p[LC_AES_BLOCK_SIZE];
	uint8_t		cur_p[LC_AES_BLOCK_SIZE], tmp[LC_AES_BLOCK_SIZE];
	const uint8_t	*src;
	uint8_t		*dst;
	size_t		off;

	if (ctx == NULL || iv == NULL || in == NULL || out == NULL) {
		return (-1);
	}
	if (len == 0 || (len % LC_AES_BLOCK_SIZE) != 0) {
		return (-1);
	}

	src = (const uint8_t *)in;
	dst = (uint8_t *)out;
	memcpy(prev_c, iv, LC_AES_BLOCK_SIZE);
	memcpy(prev_p, iv + LC_AES_BLOCK_SIZE, LC_AES_BLOCK_SIZE);

	for (off = 0; off < len; off += LC_AES_BLOCK_SIZE) {
		memcpy(cur_p, src + off, LC_AES_BLOCK_SIZE);
		xor_block(tmp, cur_p, prev_c);
		aes_encrypt_block(ctx, tmp, tmp);
		xor_block(tmp, tmp, prev_p);
		memcpy(dst + off, tmp, LC_AES_BLOCK_SIZE);
		memcpy(prev_c, tmp, LC_AES_BLOCK_SIZE);
		memcpy(prev_p, cur_p, LC_AES_BLOCK_SIZE);
	}

	memcpy(iv, prev_c, LC_AES_BLOCK_SIZE);
	memcpy(iv + LC_AES_BLOCK_SIZE, prev_p, LC_AES_BLOCK_SIZE);
	lc_wipe(tmp, sizeof(tmp));
	lc_wipe(cur_p, sizeof(cur_p));
	return (0);
}

int
lc_aes_ige_decrypt(lc_aes_ctx *ctx, uint8_t iv[LC_AES_IGE_IV_SIZE],
    const void *in, void *out, size_t len)
{
	uint8_t		prev_c[LC_AES_BLOCK_SIZE], prev_p[LC_AES_BLOCK_SIZE];
	uint8_t		cur_c[LC_AES_BLOCK_SIZE], tmp[LC_AES_BLOCK_SIZE];
	const uint8_t	*src;
	uint8_t		*dst;
	size_t		off;

	if (ctx == NULL || iv == NULL || in == NULL || out == NULL) {
		return (-1);
	}
	if (len == 0 || (len % LC_AES_BLOCK_SIZE) != 0) {
		return (-1);
	}

	src = (const uint8_t *)in;
	dst = (uint8_t *)out;
	memcpy(prev_c, iv, LC_AES_BLOCK_SIZE);
	memcpy(prev_p, iv + LC_AES_BLOCK_SIZE, LC_AES_BLOCK_SIZE);

	for (off = 0; off < len; off += LC_AES_BLOCK_SIZE) {
		memcpy(cur_c, src + off, LC_AES_BLOCK_SIZE);
		xor_block(tmp, cur_c, prev_p);
		aes_decrypt_block(ctx, tmp, tmp);
		xor_block(tmp, tmp, prev_c);
		memcpy(dst + off, tmp, LC_AES_BLOCK_SIZE);
		memcpy(prev_p, tmp, LC_AES_BLOCK_SIZE);
		memcpy(prev_c, cur_c, LC_AES_BLOCK_SIZE);
	}

	memcpy(iv, prev_c, LC_AES_BLOCK_SIZE);
	memcpy(iv + LC_AES_BLOCK_SIZE, prev_p, LC_AES_BLOCK_SIZE);
	lc_wipe(tmp, sizeof(tmp));
	lc_wipe(cur_c, sizeof(cur_c));
	return (0);
}
