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
$define %type size_t as unsigned long
$define %type header_t as struct with magic, is_free, size, payload_size, next, prev

$define %func kmem_init as procedure with args void
$define %func kmem_alloc as function with args size_t
$define %func kmem_free as procedure with args void *
$define %func kmem_calloc as function with args size_t, size_t
$define %func kmem_realloc as function with args void *, size_t
$define %func kmem_alloc_aligned as function with args size_t, size_t
$define %func kmem_usable_size as function with args void *
$define %func kmem_free_bytes as function with args void
$define %func kmem_total_bytes as function with args void
$define %func kmem_used_bytes as function with args void
$define %func kmem_is_initialized as function with args void
$define %func kmem_dump as procedure with args void

*/

/* !SPACE!

$space %export kmem_init, kmem_alloc, kmem_free, kmem_calloc
$space %export kmem_realloc, kmem_alloc_aligned, kmem_usable_size
$space %export kmem_free_bytes, kmem_total_bytes, kmem_used_bytes
$space %export kmem_is_initialized, kmem_dump

*/

#ifndef KMEM_H
#define KMEM_H

#include <mlibc/mlibc.h>

#define KMEM_HEAP_SIZE		(8 * 1024 * 1024)
#define KMEM_REDZONE_SIZE	16
#define KMEM_REDZONE_BYTE	0xCC
#define KMEM_POISON_BYTE	0xAA
#define KMEM_ALIGN		16

void	*kmem_alloc(size_t size);
void	*kmem_calloc(size_t nmemb, size_t size);
void	*kmem_realloc(void *ptr, size_t size);
void	*kmem_alloc_aligned(size_t size, size_t align);
void	kmem_free(void *ptr);
size_t	kmem_usable_size(void *ptr);
size_t	kmem_free_bytes(void);
size_t	kmem_total_bytes(void);
size_t	kmem_used_bytes(void);
int	kmem_is_initialized(void);
void	kmem_init(void);
void	kmem_dump(void);

#endif
