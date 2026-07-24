/* !DEFINES!

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %func lc_load32_le as function with args const uint8_t *
$define %func lc_store32_le as procedure with args uint8_t *, uint32_t
$define %func lc_load64_le as function with args const uint8_t *
$define %func lc_store64_le as procedure with args uint8_t *, uint64_t

*/

/* !SPACE!

$space %internal lc_load32_le, lc_store32_le
$space %internal lc_load64_le, lc_store64_le

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

#ifndef LIBCRYPTO_PRIVATE_H
#define LIBCRYPTO_PRIVATE_H

#include <stdint.h>

static inline uint32_t
lc_load32_le(const uint8_t *src)
{
	return ((uint32_t)src[0] | ((uint32_t)src[1] << 8) |
	    ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24));
}

static inline void
lc_store32_le(uint8_t *dst, uint32_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
}

static inline uint64_t
lc_load64_le(const uint8_t *src)
{
	return ((uint64_t)src[0] | ((uint64_t)src[1] << 8) |
	    ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
	    ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) |
	    ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56));
}

static inline void
lc_store64_le(uint8_t *dst, uint64_t value)
{
	dst[0] = (uint8_t)value;
	dst[1] = (uint8_t)(value >> 8);
	dst[2] = (uint8_t)(value >> 16);
	dst[3] = (uint8_t)(value >> 24);
	dst[4] = (uint8_t)(value >> 32);
	dst[5] = (uint8_t)(value >> 40);
	dst[6] = (uint8_t)(value >> 48);
	dst[7] = (uint8_t)(value >> 56);
}

#endif
