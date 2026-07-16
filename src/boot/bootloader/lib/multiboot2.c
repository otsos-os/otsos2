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
$define %type mb2_builder_t as mutable multiboot2 info builder
$define %type mb2_framebuffer_t as framebuffer boot info
$define %type mb2_tag_t as multiboot2 tag header
$define %type mb2_mmap_entry_t as multiboot2 memory map entry

$define %func align8 as function with args u32
$define %func tag_alloc as function with args mb2_builder_t *, u32, u32
$define %func mb2_builder_init as procedure with args mb2_builder_t *, void *, u32
$define %func mb2_add_bootloader_name as function with args mb2_builder_t *, const char *
$define %func mb2_add_basic_meminfo as function with args mb2_builder_t *, u32, u32
$define %func mb2_add_simple_mmap as function with args mb2_builder_t *, u32, u32
$define %func mb2_add_mmap_entries as function with args mb2_builder_t *, const mb2_mmap_entry_t *, u32
$define %func mb2_add_framebuffer as function with args mb2_builder_t *, const mb2_framebuffer_t *
$define %func mb2_add_module as function with args mb2_builder_t *, u32, u32, const char *
$define %func mb2_add_acpi as function with args mb2_builder_t *, const void *, u32, int
$define %func mb2_builder_finish as function with args mb2_builder_t *

*/

/* !SPACE!

$space %internal align8, tag_alloc
$space %export mb2_builder_init, mb2_add_bootloader_name
$space %export mb2_add_basic_meminfo, mb2_add_simple_mmap
$space %export mb2_add_mmap_entries
$space %export mb2_add_framebuffer, mb2_add_module, mb2_builder_finish
$space %export mb2_add_acpi

*/

#include <boot/bootloader/lib/multiboot2.h>
#include <boot/bootloader/lib/string.h>

#define MB2_TAG_END		0
#define MB2_TAG_BOOTLOADER	2
#define MB2_TAG_MODULE		3
#define MB2_TAG_BASIC_MEMINFO	4
#define MB2_TAG_MMAP		6
#define MB2_TAG_FRAMEBUFFER	8
#define MB2_TAG_ACPI_OLD	14
#define MB2_TAG_ACPI_NEW	15
#define MB2_MEMORY_AVAILABLE	1
#define MB2_FB_RGB		1

typedef struct {
	u32	type;
	u32	size;
} __attribute__((packed)) mb2_tag_t;

static u32
align8(u32 value)
{
	return ((value + 7U) & ~7U);
}

static void *
tag_alloc(mb2_builder_t *b, u32 type, u32 size)
{
	mb2_tag_t	*tag;
	u32		next;

	if (b->off > b->cap || size > b->cap - b->off) {
		return (NULL);
	}
	next = align8(b->off + size);
	if (next < b->off || next > b->cap) {
		return (NULL);
	}
	tag = (mb2_tag_t *)(b->buf + b->off);
	tag->type = type;
	tag->size = size;
	bl_memset(b->buf + b->off + size, 0, next - b->off - size);
	b->off = next;
	return (tag);
}

void
mb2_builder_init(mb2_builder_t *b, void *buf, u32 cap)
{
	b->buf = (u8 *)buf;
	b->cap = cap;
	b->off = 8;
	bl_memset(buf, 0, cap);
}

int
mb2_add_bootloader_name(mb2_builder_t *b, const char *name)
{
	mb2_tag_t	*tag;
	u32		size;

	size = 8 + bl_strlen(name) + 1;
	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_BOOTLOADER, size);
	if (!tag) {
		return (-1);
	}
	bl_memcpy((u8 *)tag + 8, name, size - 8);
	return (0);
}

int
mb2_add_basic_meminfo(mb2_builder_t *b, u32 lower, u32 upper)
{
	mb2_tag_t	*tag;
	u32		*p;

	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_BASIC_MEMINFO, 16);
	if (!tag) {
		return (-1);
	}
	p = (u32 *)((u8 *)tag + 8);
	p[0] = lower;
	p[1] = upper;
	return (0);
}

int
mb2_add_simple_mmap(mb2_builder_t *b, u32 lower, u32 upper)
{
	mb2_mmap_entry_t	*entry;
	mb2_tag_t		*tag;
	u32			size;
	u32			*p;

	size = 16 + 2 * sizeof(*entry);
	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_MMAP, size);
	if (!tag) {
		return (-1);
	}
	p = (u32 *)((u8 *)tag + 8);
	p[0] = sizeof(*entry);
	p[1] = 0;
	entry = (mb2_mmap_entry_t *)((u8 *)tag + 16);
	entry[0].base_addr = 0;
	entry[0].length = (u64)lower * 1024ULL;
	entry[0].type = MB2_MEMORY_AVAILABLE;
	entry[0].reserved = 0;
	entry[1].base_addr = 0x100000ULL;
	entry[1].length = (u64)upper * 1024ULL;
	entry[1].type = MB2_MEMORY_AVAILABLE;
	entry[1].reserved = 0;
	return (0);
}

int
mb2_add_mmap_entries(mb2_builder_t *b, const mb2_mmap_entry_t *entries,
    u32 count)
{
	mb2_tag_t	*tag;
	u32		size;
	u32		*p;

	if (!entries || count == 0) {
		return (-1);
	}
	if (count > (0xffffffffU - 16U) / sizeof(*entries)) {
		return (-1);
	}
	size = 16 + count * sizeof(*entries);
	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_MMAP, size);
	if (!tag) {
		return (-1);
	}
	p = (u32 *)((u8 *)tag + 8);
	p[0] = sizeof(*entries);
	p[1] = 0;
	bl_memcpy((u8 *)tag + 16, entries, count * sizeof(*entries));
	return (0);
}

int
mb2_add_framebuffer(mb2_builder_t *b, const mb2_framebuffer_t *fb)
{
	mb2_tag_t	*tag;
	u8		*p;

	if (!fb || fb->addr == 0 || fb->bpp == 0) {
		return (0);
	}
	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_FRAMEBUFFER, 32);
	if (!tag) {
		return (-1);
	}
	p = (u8 *)tag + 8;
	*(u64 *)(p + 0) = fb->addr;
	*(u32 *)(p + 8) = fb->pitch;
	*(u32 *)(p + 12) = fb->width;
	*(u32 *)(p + 16) = fb->height;
	*(u8 *)(p + 20) = (u8)fb->bpp;
	*(u8 *)(p + 21) = MB2_FB_RGB;
	*(u16 *)(p + 22) = 0;
	return (0);
}

int
mb2_add_module(mb2_builder_t *b, u32 start, u32 end, const char *name)
{
	mb2_tag_t	*tag;
	u32		size;
	u32		*p;

	size = 16 + bl_strlen(name) + 1;
	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_MODULE, size);
	if (!tag) {
		return (-1);
	}
	p = (u32 *)((u8 *)tag + 8);
	p[0] = start;
	p[1] = end;
	bl_memcpy((u8 *)tag + 16, name, size - 16);
	return (0);
}

int
mb2_add_acpi(mb2_builder_t *b, const void *rsdp, u32 size, int is_new)
{
	mb2_tag_t	*tag;
	u32		type;

	if (!rsdp || size < 20) {
		return (-1);
	}
	type = is_new ? MB2_TAG_ACPI_NEW : MB2_TAG_ACPI_OLD;
	tag = (mb2_tag_t *)tag_alloc(b, type, 8 + size);
	if (!tag) {
		return (-1);
	}
	bl_memcpy((u8 *)tag + 8, rsdp, size);
	return (0);
}

u32
mb2_builder_finish(mb2_builder_t *b)
{
	mb2_tag_t	*tag;

	tag = (mb2_tag_t *)tag_alloc(b, MB2_TAG_END, 8);
	if (!tag) {
		return (0);
	}
	*(u32 *)(b->buf + 0) = b->off;
	*(u32 *)(b->buf + 4) = 0;
	return (b->off);
}
