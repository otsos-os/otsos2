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
$define %type mb2_builder_t as mutable multiboot2 info builder
$define %type mb2_framebuffer_t as framebuffer boot info
$define %func mb2_builder_init as procedure with args mb2_builder_t *, void *, u32
$define %func mb2_add_bootloader_name as function with args mb2_builder_t *, const char *
$define %func mb2_add_basic_meminfo as function with args mb2_builder_t *, u32, u32
$define %func mb2_add_simple_mmap as function with args mb2_builder_t *, u32, u32
$define %func mb2_add_framebuffer as function with args mb2_builder_t *, const mb2_framebuffer_t *
$define %func mb2_add_module as function with args mb2_builder_t *, u32, u32, const char *
$define %func mb2_builder_finish as function with args mb2_builder_t *

*/

/* !SPACE!

$space %export mb2_builder_t, mb2_framebuffer_t
$space %export mb2_builder_init, mb2_add_bootloader_name
$space %export mb2_add_basic_meminfo, mb2_add_simple_mmap
$space %export mb2_add_framebuffer, mb2_add_module, mb2_builder_finish

*/

#ifndef BOOTLOADER_MULTIBOOT2_H
#define BOOTLOADER_MULTIBOOT2_H
#include <boot/bootloader/lib/types.h>
#define MB2_BOOTLOADER_MAGIC	0x36D76289U
typedef struct {
	u8	*buf;
	u32	cap;
	u32	off;
} mb2_builder_t;
typedef struct {
	u64	addr;
	u32	pitch;
	u32	width;
	u32	height;
	u32	bpp;
	u32	type;
} mb2_framebuffer_t;
void	mb2_builder_init(mb2_builder_t *b, void *buf, u32 cap);
int	mb2_add_bootloader_name(mb2_builder_t *b, const char *name);
int	mb2_add_basic_meminfo(mb2_builder_t *b, u32 lower, u32 upper);
int	mb2_add_simple_mmap(mb2_builder_t *b, u32 lower, u32 upper);
int	mb2_add_framebuffer(mb2_builder_t *b, const mb2_framebuffer_t *fb);
int	mb2_add_module(mb2_builder_t *b, u32 start, u32 end,
	    const char *name);
u32	mb2_builder_finish(mb2_builder_t *b);

#endif
