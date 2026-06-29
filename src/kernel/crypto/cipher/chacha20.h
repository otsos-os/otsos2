/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type chacha20_ctx_t as struct with state, counter, keystream, position

$define %func chacha20_init as procedure with args chacha20_ctx_t *, const u8 *, const u8 *
$define %func chacha20_set_counter as procedure with args chacha20_ctx_t *, u64
$define %func chacha20_xor as procedure with args chacha20_ctx_t *, const u8 *, u8 *, u32
$define %func chacha20_encrypt as function with args const u8 *, const u8 *, const u8 *, u32, u8 *
$define %func chacha20_decrypt as function with args const u8 *, const u8 *, const u8 *, u32, u8 *
$define %func chacha20_wipe as procedure with args chacha20_ctx_t *

*/

/* !SPACE!

$space %export chacha20_init, chacha20_set_counter
$space %export chacha20_xor, chacha20_encrypt, chacha20_decrypt
$space %export chacha20_wipe

*/

#ifndef CHACHA20_H
#define CHACHA20_H

#include <mlibc/mlibc.h>

#define CHACHA20_KEY_SIZE	32
#define CHACHA20_NONCE_SIZE	12
#define CHACHA20_BLOCK_SIZE	64

typedef struct {
	u32	state[16];
	u64	counter;
	u8	keystream[CHACHA20_BLOCK_SIZE];
	u32	position;
} chacha20_ctx_t;

void	chacha20_init(chacha20_ctx_t *ctx, const u8 *key,
	    const u8 *nonce);
void	chacha20_set_counter(chacha20_ctx_t *ctx, u64 counter);
void	chacha20_xor(chacha20_ctx_t *ctx, const u8 *in, u8 *out,
	    u32 len);
int	chacha20_encrypt(const u8 *key, const u8 *nonce,
	    const u8 *plaintext, u32 len, u8 *ciphertext);
int	chacha20_decrypt(const u8 *key, const u8 *nonce,
	    const u8 *ciphertext, u32 len, u8 *plaintext);
void	chacha20_wipe(chacha20_ctx_t *ctx);

#endif
