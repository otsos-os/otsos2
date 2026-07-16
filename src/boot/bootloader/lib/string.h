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
$define %type int as 32 bit signed
$define %type char as 8 bit signed

$define %func bl_memcpy as function with args void *, const void *, u32
$define %func bl_memset as function with args void *, int, u32
$define %func bl_memcmp as function with args const void *, const void *, u32
$define %func bl_strlen as function with args const char *
$define %func bl_strcmp as function with args const char *, const char *

*/

/* !SPACE!

$space %export bl_memcpy, bl_memset, bl_memcmp
$space %export bl_strlen, bl_strcmp

*/

#ifndef BOOTLOADER_STRING_H
#define BOOTLOADER_STRING_H
#include <boot/bootloader/lib/types.h>
void	*bl_memcpy(void *dst, const void *src, u32 len);
void	*bl_memset(void *dst, int value, u32 len);
int	bl_memcmp(const void *a, const void *b, u32 len);
u32	bl_strlen(const char *str);
int	bl_strcmp(const char *a, const char *b);

#endif
