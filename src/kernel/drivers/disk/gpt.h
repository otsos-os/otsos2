/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/* !DEFINES!

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type disk_t as registered block device
$define %type gpt_header_t as GPT header on disk
$define %type gpt_entry_t as GPT partition entry on disk

$define %func gpt_probe as function with args disk_t *
$define %func gpt_rescan as function with args disk_t *
$define %func gpt_detach as procedure with args disk_t *
$define %func gpt_layout_init as function with args disk_t *
$define %func gpt_layout_add as function with args disk_t *, struct dioc_gpt_add *
$define %func gpt_layout_commit as function with args disk_t *
$define %func gpt_crc32 as function with args const void *, u32

*/

/* !SPACE!

$space %export gpt_header_t, gpt_entry_t
$space %export gpt_probe, gpt_rescan, gpt_detach
$space %export gpt_layout_init, gpt_layout_add, gpt_layout_commit
$space %export gpt_crc32

*/

#ifndef KERNEL_DRIVERS_DISK_GPT_H
#define KERNEL_DRIVERS_DISK_GPT_H

#include <kernel/drivers/disk/disk.h>

#define	GPT_SIGNATURE		0x5452415020494645ULL
#define	GPT_REVISION		0x00010000U
#define	GPT_HEADER_LBA		1
#define	GPT_ENTRY_LBA		2
#define	GPT_ENTRY_COUNT		128
#define	GPT_ENTRY_SIZE		128
#define	GPT_ALIGN_SECTORS	2048
#define	GPT_MAX_SLICES		8

struct dioc_gpt_add;

typedef struct gpt_header {
	u64	signature;
	u32	revision;
	u32	header_size;
	u32	header_crc32;
	u32	reserved;
	u64	my_lba;
	u64	alternate_lba;
	u64	first_usable_lba;
	u64	last_usable_lba;
	u8	disk_guid[16];
	u64	entry_lba;
	u32	entry_count;
	u32	entry_size;
	u32	entry_crc32;
} __attribute__((packed)) gpt_header_t;

typedef struct gpt_entry {
	u8	type_guid[16];
	u8	unique_guid[16];
	u64	first_lba;
	u64	last_lba;
	u64	attributes;
	u16	name[36];
} __attribute__((packed)) gpt_entry_t;

int	gpt_probe(disk_t *disk);
int	gpt_rescan(disk_t *disk);
void	gpt_detach(disk_t *disk);

int	gpt_layout_init(disk_t *disk);
int	gpt_layout_add(disk_t *disk, struct dioc_gpt_add *add);
int	gpt_layout_commit(disk_t *disk);

u32	gpt_crc32(const void *data, u32 len);

#endif
