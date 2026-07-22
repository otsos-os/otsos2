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
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type elf64_ehdr as ELF64 file header
$define %type elf64_phdr as ELF64 program header
$define %type elf64_shdr as ELF64 section header
$define %type elf64_sym as ELF64 symbol table entry
$define %type elf64_rela as ELF64 relocation with addend
$define %type elf64_section_desc as relocatable object section description
$define %type elf64_symbol_desc as relocatable object symbol description
$define %type elf64_rela_desc as relocatable object relocation description
$define %type elf64_object_desc as relocatable object writer input
$define %type elf64_exec_section_desc as executable section description
$define %type elf64_exec_desc as executable writer input
$define %func elf64_ident_is_valid as function with args const void *, size_t
$define %func elf64_write_relocatable as function with args const char *, const elf64_object_desc *
$define %func elf64_write_executable as function with args const char *, const elf64_exec_desc *

*/

/* !SPACE!

$space %export elf64_ehdr, elf64_phdr, elf64_shdr, elf64_sym, elf64_rela
$space %export elf64_section_desc, elf64_symbol_desc
$space %export elf64_rela_desc, elf64_object_desc
$space %export elf64_exec_section_desc, elf64_exec_desc
$space %export elf64_ident_is_valid, elf64_write_relocatable
$space %export elf64_write_executable

*/

#ifndef LIBELF_H
#define LIBELF_H

#include <stddef.h>
#include <stdint.h>

#define ELF64_EI_NIDENT	16

#define ELF64_ET_REL	1
#define ELF64_ET_EXEC	2
#define ELF64_EM_X86_64	62
#define ELF64_EV_CURRENT 1

#define ELF64_ELFCLASS64 2
#define ELF64_ELFDATA2LSB 1
#define ELF64_ELFOSABI_SYSV 0

#define ELF64_SHT_NULL		0
#define ELF64_SHT_PROGBITS	1
#define ELF64_SHT_SYMTAB	2
#define ELF64_SHT_STRTAB	3
#define ELF64_SHT_NOBITS	8
#define ELF64_SHT_RELA		4

#define ELF64_PT_LOAD		1

#define ELF64_PF_X		0x1
#define ELF64_PF_W		0x2
#define ELF64_PF_R		0x4

#define ELF64_SHF_WRITE		0x1
#define ELF64_SHF_ALLOC		0x2
#define ELF64_SHF_EXECINSTR	0x4

#define ELF64_STB_LOCAL		0
#define ELF64_STB_GLOBAL	1

#define ELF64_STT_NOTYPE	0
#define ELF64_STT_OBJECT	1
#define ELF64_STT_FUNC		2
#define ELF64_STT_SECTION	3

#define ELF64_SHN_UNDEF		0
#define ELF64_SHN_ABS		0xfff1

#define ELF64_R_X86_64_NONE	0
#define ELF64_R_X86_64_64	1
#define ELF64_R_X86_64_PC32	2
#define ELF64_R_X86_64_PLT32	4
#define ELF64_R_X86_64_32	10
#define ELF64_R_X86_64_32S	11

#define ELF64_ST_INFO(bind, type) \
	((uint8_t)((((bind) & 0xf) << 4) | ((type) & 0xf)))
#define ELF64_ST_BIND(info)	((uint8_t)((info) >> 4))
#define ELF64_ST_TYPE(info)	((uint8_t)((info) & 0xf))
#define ELF64_R_INFO(sym, type) \
	((((uint64_t)(sym)) << 32) | ((uint32_t)(type)))
#define ELF64_R_SYM(info)	((uint32_t)((info) >> 32))
#define ELF64_R_TYPE(info)	((uint32_t)(info))

typedef struct elf64_ehdr {
	uint8_t		e_ident[ELF64_EI_NIDENT];
	uint16_t	e_type;
	uint16_t	e_machine;
	uint32_t	e_version;
	uint64_t	e_entry;
	uint64_t	e_phoff;
	uint64_t	e_shoff;
	uint32_t	e_flags;
	uint16_t	e_ehsize;
	uint16_t	e_phentsize;
	uint16_t	e_phnum;
	uint16_t	e_shentsize;
	uint16_t	e_shnum;
	uint16_t	e_shstrndx;
} __attribute__((packed)) elf64_ehdr;

typedef struct elf64_phdr {
	uint32_t	p_type;
	uint32_t	p_flags;
	uint64_t	p_offset;
	uint64_t	p_vaddr;
	uint64_t	p_paddr;
	uint64_t	p_filesz;
	uint64_t	p_memsz;
	uint64_t	p_align;
} __attribute__((packed)) elf64_phdr;

typedef struct elf64_shdr {
	uint32_t	sh_name;
	uint32_t	sh_type;
	uint64_t	sh_flags;
	uint64_t	sh_addr;
	uint64_t	sh_offset;
	uint64_t	sh_size;
	uint32_t	sh_link;
	uint32_t	sh_info;
	uint64_t	sh_addralign;
	uint64_t	sh_entsize;
} __attribute__((packed)) elf64_shdr;

typedef struct elf64_sym {
	uint32_t	st_name;
	uint8_t		st_info;
	uint8_t		st_other;
	uint16_t	st_shndx;
	uint64_t	st_value;
	uint64_t	st_size;
} __attribute__((packed)) elf64_sym;

typedef struct elf64_rela {
	uint64_t	r_offset;
	uint64_t	r_info;
	int64_t		r_addend;
} __attribute__((packed)) elf64_rela;

typedef struct elf64_section_desc {
	const char	*name;
	uint32_t	type;
	uint64_t	flags;
	uint64_t	align;
	uint64_t	entsize;
	const uint8_t	*data;
	uint64_t	size;
} elf64_section_desc;

typedef struct elf64_symbol_desc {
	const char	*name;
	uint8_t		bind;
	uint8_t		type;
	uint16_t	section;
	uint64_t	value;
	uint64_t	size;
} elf64_symbol_desc;

typedef struct elf64_rela_desc {
	uint16_t	section;
	uint64_t	offset;
	uint32_t	type;
	uint32_t	symbol;
	int64_t		addend;
} elf64_rela_desc;

typedef struct elf64_object_desc {
	const elf64_section_desc	*sections;
	size_t			section_count;
	const elf64_symbol_desc	*symbols;
	size_t			symbol_count;
	const elf64_rela_desc	*relocs;
	size_t			reloc_count;
} elf64_object_desc;

typedef struct elf64_exec_section_desc {
	const char	*name;
	uint32_t	type;
	uint64_t	flags;
	uint64_t	align;
	const uint8_t	*data;
	uint64_t	size;
	uint64_t	vaddr;
} elf64_exec_section_desc;

typedef struct elf64_exec_desc {
	uint64_t			entry;
	uint64_t			base_vaddr;
	const elf64_exec_section_desc	*sections;
	size_t				section_count;
} elf64_exec_desc;

int	elf64_ident_is_valid(const void *data, size_t size);
int	elf64_write_relocatable(const char *path,
	    const elf64_object_desc *desc);
int	elf64_write_executable(const char *path, const elf64_exec_desc *desc);

#endif
