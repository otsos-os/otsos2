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
$define %type bootpack_t as tar-backed boot payload
$define %type bootpack_file_t as file entry inside boot payload
$define %type bootpack_iter_cb as callback for each file

$define %func bootpack_init as procedure with args bootpack_t *, const void *, u32
$define %func bootpack_find as function with args bootpack_t *, const char *, bootpack_file_t *
$define %func bootpack_foreach as function with args bootpack_t *, bootpack_iter_cb, void *

*/

/* !SPACE!

$space %export bootpack_t, bootpack_file_t, bootpack_iter_cb
$space %export bootpack_init, bootpack_find, bootpack_foreach

*/

#ifndef BOOTLOADER_BOOTPACK_H
#define BOOTLOADER_BOOTPACK_H

#include <boot/bootloader/lib/types.h>

typedef struct {
	const u8	*data;
	u32	size;
} bootpack_t;

typedef struct {
	const char	*name;
	const u8	*data;
	u32		size;
} bootpack_file_t;

typedef int	(*bootpack_iter_cb)(const bootpack_file_t *file, void *ctx);

void	bootpack_init(bootpack_t *pack, const void *data, u32 size);
int	bootpack_find(bootpack_t *pack, const char *name,
	    bootpack_file_t *out);
int	bootpack_foreach(bootpack_t *pack, bootpack_iter_cb cb, void *ctx);

#endif
