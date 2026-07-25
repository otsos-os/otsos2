/* !DEFINES!

$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %func lssh_load_u32 as function with args const uint8_t *
$define %func lssh_store_u32 as procedure with args uint8_t *, uint32_t
$define %func lssh_slice_cstr as function with args const char *
$define %func lssh_logf as procedure with args int, const char *, ...
$define %func lssh_log_packet_type_name as function with args uint8_t

*/

/* !SPACE!

$space %internal lssh_load_u32, lssh_store_u32, lssh_slice_cstr
$space %internal lssh_logf, lssh_log_packet_type_name

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
 *    this list of conditions in the documentation and/or other materials
 *    provided with the distribution.
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

#ifndef LIBSSH_PRIVATE_H
#define LIBSSH_PRIVATE_H

#include <libssh.h>
#include <stdint.h>
#include <string.h>

static inline uint32_t
lssh_load_u32(const uint8_t *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static inline void
lssh_store_u32(uint8_t *p, uint32_t value)
{
	p[0] = (uint8_t)(value >> 24);
	p[1] = (uint8_t)(value >> 16);
	p[2] = (uint8_t)(value >> 8);
	p[3] = (uint8_t)value;
}

static inline lssh_slice
lssh_slice_cstr(const char *str)
{
	lssh_slice	slice;

	slice.data = (const uint8_t *)str;
	slice.len = str ? strlen(str) : 0;
	return (slice);
}

void		lssh_logf(int level, const char *fmt, ...);
const char	*lssh_log_packet_type_name(uint8_t type);

#endif
