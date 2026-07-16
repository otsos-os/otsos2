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
$define %type u64 as 64 bit unsigned
$define %type elf64_ehdr_t as ELF64 executable header
$define %type elf64_phdr_t as ELF64 program header

$define %func elf64_load_kernel as function with args const void *, u32, u64 *, u64 *

*/

/* !SPACE!

$space %export elf64_load_kernel

*/

#include <boot/bootloader/lib/elf64.h>
#include <boot/bootloader/lib/string.h>

#define ELF_MAGIC0	0x7f
#define ELF_MAGIC1	'E'
#define ELF_MAGIC2	'L'
#define ELF_MAGIC3	'F'
#define ELFCLASS64	2
#define ELFDATA2LSB	1
#define ET_EXEC		2
#define EM_X86_64	62
#define PT_LOAD		1

typedef struct {
	u8	e_ident[16];
	u16	e_type;
	u16	e_machine;
	u32	e_version;
	u64	e_entry;
	u64	e_phoff;
	u64	e_shoff;
	u32	e_flags;
	u16	e_ehsize;
	u16	e_phentsize;
	u16	e_phnum;
	u16	e_shentsize;
	u16	e_shnum;
	u16	e_shstrndx;
} __attribute__((packed)) elf64_ehdr_t;

typedef struct {
	u32	p_type;
	u32	p_flags;
	u64	p_offset;
	u64	p_vaddr;
	u64	p_paddr;
	u64	p_filesz;
	u64	p_memsz;
	u64	p_align;
} __attribute__((packed)) elf64_phdr_t;

int
elf64_load_kernel(const void *data, u32 size, u64 *entry, u64 *kernel_end)
{
	const elf64_ehdr_t	*eh;
	const elf64_phdr_t	*ph;
	const u8		*src;
	u8			*dst;
	u64			end;
	u16			i;

	if (size < sizeof(*eh)) {
		return (-1);
	}
	eh = (const elf64_ehdr_t *)data;
	if (eh->e_ident[0] != ELF_MAGIC0 || eh->e_ident[1] != ELF_MAGIC1 ||
	    eh->e_ident[2] != ELF_MAGIC2 || eh->e_ident[3] != ELF_MAGIC3) {
		return (-1);
	}
	if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB) {
		return (-1);
	}
	if (eh->e_type != ET_EXEC || eh->e_machine != EM_X86_64) {
		return (-1);
	}
	if (eh->e_phoff + (u64)eh->e_phnum * eh->e_phentsize > size) {
		return (-1);
	}

	end = 0;
	for (i = 0; i < eh->e_phnum; i++) {
		ph = (const elf64_phdr_t *)((const u8 *)data + eh->e_phoff +
		    (u64)i * eh->e_phentsize);
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		if (ph->p_paddr > 0xffffffffULL) {
			return (-1);
		}
		if (ph->p_offset + ph->p_filesz > size) {
			return (-1);
		}
		src = (const u8 *)data + ph->p_offset;
		dst = (u8 *)(u32)ph->p_paddr;
		bl_memcpy(dst, src, (u32)ph->p_filesz);
		if (ph->p_memsz > ph->p_filesz) {
			bl_memset(dst + ph->p_filesz, 0,
			    (u32)(ph->p_memsz - ph->p_filesz));
		}
		if (ph->p_paddr + ph->p_memsz > end) {
			end = ph->p_paddr + ph->p_memsz;
		}
	}

	*entry = eh->e_entry;
	*kernel_end = end;
	return (0);
}
