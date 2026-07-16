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
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type bios_boot_info_t as BIOS data collected before protected mode
$define %type bios_mmap_entry_t as BIOS E820 memory map entry
$define %type bios_layout_t as boot image payload layout sector

$define %func bios_console_init as procedure with args void
$define %func bios_console_putc as procedure with args char
$define %func bios_console_puts as procedure with args const char *
$define %func bios_console_puthex as procedure with args u32
$define %func bios_disk_read as function with args u32, u32, void *
$define %func bios_halt as procedure with args void
$define %func bios_jump_kernel as procedure with args u32, u32, u32

*/

/* !SPACE!

$space %export bios_boot_info_t, bios_mmap_entry_t, bios_layout_t
$space %export bios_console_init, bios_console_putc, bios_console_puts
$space %export bios_console_puthex, bios_disk_read
$space %export bios_halt, bios_jump_kernel

*/

#ifndef BOOTLOADER_BIOS_H
#define BOOTLOADER_BIOS_H

#include <boot/bootloader/lib/types.h>

#define BIOS_SECTOR_SIZE	512U
#define BIOS_LAYOUT_MAGIC	0x3250424fU
#define BIOS_LAYOUT_LBA		257U
#define BIOS_BOOTPACK_LBA	258U
#define BIOS_E820_MAX		64U

typedef struct {
	u32	boot_drive;
	u32	mem_lower_kb;
	u32	mem_upper_kb;
	u32	fb_addr;
	u32	fb_pitch;
	u32	fb_width;
	u32	fb_height;
	u32	fb_bpp;
	u32	fb_type;
} bios_boot_info_t;

typedef struct {
	u64	base_addr;
	u64	length;
	u32	type;
	u32	attr;
} bios_mmap_entry_t;

typedef struct {
	u32	magic;
	u32	bootpack_sectors;
	u32	bootpack_bytes;
	u32	reserved;
} bios_layout_t;

extern bios_boot_info_t	bios_boot_info;
extern u32		bios_mmap_count;
extern bios_mmap_entry_t	bios_mmap_entries[BIOS_E820_MAX];

void	bios_console_init(void);
void	bios_console_putc(char c);
void	bios_console_puts(const char *str);
void	bios_console_puthex(u32 value);
int	bios_disk_read(u32 lba, u32 sectors, void *dst);
void	bios_halt(void);
void	bios_jump_kernel(u32 entry, u32 magic, u32 info);

#endif
