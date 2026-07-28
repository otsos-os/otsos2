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
 * LIABLE FOR ANY DIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type kofo_header_t as fixed KOFO file header
$define %type kofo_section_t as fixed KOFO section descriptor
$define %type kofo_symbol_t as fixed KOFO symbol descriptor
$define %type kofo_import_t as fixed KOFO import descriptor
$define %type kofo_reloc_t as fixed KOFO relocation descriptor
$define %type kofo_driver_t as fixed KOFO driver metadata descriptor
$define %type kofo_kernel_symbol_t as generated kernel symbol descriptor

*/

/* !SPACE!

$space %export kofo_header_t, kofo_section_t, kofo_symbol_t
$space %export kofo_import_t, kofo_reloc_t, kofo_driver_t
$space %export kofo_kernel_symbol_t, kofo_kernel_symbols
$space %export kofo_kernel_symbol_count

*/

#ifndef KERNEL_KOFO_H
#define KERNEL_KOFO_H

#include <mlibc/mlibc.h>

#define	KOFO_MAGIC		0x4F464F4B
#define	KOFO_VERSION		1
#define	KOFO_ARCH_X86_64	1
#define	KOFO_ABI_KERNEL		1
#define	KOFO_STR_NONE		0xFFFFFFFFU

#define	KOFO_SEC_NULL		0
#define	KOFO_SEC_TEXT		1
#define	KOFO_SEC_RODATA		2
#define	KOFO_SEC_DATA		3
#define	KOFO_SEC_BSS		4

#define	KOFO_SEC_F_ALLOC	0x00000001
#define	KOFO_SEC_F_READ		0x00000002
#define	KOFO_SEC_F_WRITE	0x00000004
#define	KOFO_SEC_F_EXEC		0x00000008
#define	KOFO_SEC_F_BSS		0x00000010

#define	KOFO_SYM_UNDEF		0xFFFFU
#define	KOFO_SYM_F_GLOBAL	0x00000001
#define	KOFO_SYM_F_EXPORT	0x00000002
#define	KOFO_SYM_F_IMPORT	0x00000004

#define	KOFO_RELOC_NONE		0
#define	KOFO_RELOC_ABS64	1
#define	KOFO_RELOC_ABS32	2
#define	KOFO_RELOC_REL32	3

#define	KOFO_DRIVER_F_PSEUDO	0x00000001

typedef struct kofo_header {
	u32	magic;
	u16	version;
	u16	header_size;
	u16	arch;
	u16	abi;
	u32	flags;
	u32	file_size;
	u32	name;
	u32	version_name;
	u32	abi_name;
	u32	init_name;
	u32	exit_name;
	u32	string_off;
	u32	string_size;
	u32	section_off;
	u32	section_count;
	u32	symbol_off;
	u32	symbol_count;
	u32	import_off;
	u32	import_count;
	u32	reloc_off;
	u32	reloc_count;
	u32	driver_off;
	u32	driver_count;
	u32	reserved[8];
} __attribute__((packed)) kofo_header_t;

typedef struct kofo_section {
	u32	name;
	u32	type;
	u32	flags;
	u32	align;
	u64	size;
	u32	data_off;
	u32	reserved;
} __attribute__((packed)) kofo_section_t;

typedef struct kofo_symbol {
	u32	name;
	u16	section;
	u16	flags;
	u64	value;
	u64	size;
} __attribute__((packed)) kofo_symbol_t;

typedef struct kofo_import {
	u32	name;
	u32	flags;
	u32	version_name;
	u32	reserved;
} __attribute__((packed)) kofo_import_t;

typedef struct kofo_reloc {
	u16	section;
	u16	type;
	u32	symbol;
	u64	offset;
	s64	addend;
} __attribute__((packed)) kofo_reloc_t;

typedef struct kofo_driver {
	u32	name;
	u32	bus;
	u32	class_name;
	u32	pass;
	u32	order;
	u32	flags;
	u32	probe_name;
	u32	attach_name;
	u32	detach_name;
	u32	reserved[5];
} __attribute__((packed)) kofo_driver_t;

typedef struct kofo_kernel_symbol {
	const char	*name;
	const void	*value;
} kofo_kernel_symbol_t;

extern const kofo_kernel_symbol_t	kofo_kernel_symbols[];
extern const u32			kofo_kernel_symbol_count;

#endif
