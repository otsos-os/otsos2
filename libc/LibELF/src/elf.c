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

$define %type elf_buf as growable byte buffer for ELF construction
$define %type elf_out_section as concrete output section metadata
$define %type elf64_object_desc as relocatable object writer input
$define %type elf64_exec_desc as executable writer input
$define %func elf64_ident_is_valid as function with args const void *, size_t
$define %func elf64_write_relocatable as function with args const char *, const elf64_object_desc *
$define %func elf64_write_executable as function with args const char *, const elf64_exec_desc *
$define %func elf_buf_init as procedure with args elf_buf *
$define %func elf_buf_free as procedure with args elf_buf *
$define %func elf_strtab_add as function with args elf_buf *, const char *
$define %func elf_align as function with args uint64_t, uint64_t

*/

/* !SPACE!

$space %internal elf_buf_init, elf_buf_free, elf_buf_reserve
$space %internal elf_buf_set_size, elf_buf_write_at, elf_buf_write
$space %internal elf_buf_u8, elf_strtab_add, elf_align
$space %internal elf_write_sym, elf_make_rela_name
$space %internal elf_build_symtab, elf_build_relas, elf_write_file
$space %internal elf_write_exec_file
$space %export elf64_ident_is_valid, elf64_write_relocatable
$space %export elf64_write_executable

*/

#include <libelf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct elf_buf {
	uint8_t	*data;
	size_t	size;
	size_t	capacity;
} elf_buf;

typedef struct elf_out_section {
	const char	*name;
	uint32_t	name_off;
	uint32_t	type;
	uint64_t	flags;
	uint64_t	offset;
	uint64_t	size;
	uint64_t	align;
	uint64_t	entsize;
	uint32_t	link;
	uint32_t	info;
	const uint8_t	*data;
	uint64_t	vaddr;
	elf_buf		owned;
} elf_out_section;

static void	elf_buf_init(elf_buf *buf);
static void	elf_buf_free(elf_buf *buf);
static int	elf_buf_reserve(elf_buf *buf, size_t extra);
static int	elf_buf_set_size(elf_buf *buf, size_t size);
static int	elf_buf_write(elf_buf *buf, const void *data, size_t size);
static int	elf_buf_write_at(elf_buf *buf, size_t off, const void *data,
		    size_t size);
static int	elf_buf_u8(elf_buf *buf, uint8_t value);
static uint32_t	elf_strtab_add(elf_buf *buf, const char *str);
static uint64_t	elf_align(uint64_t value, uint64_t align);
static int	elf_write_sym(elf_buf *buf, const elf64_symbol_desc *sym,
		    uint32_t name_off);
static char	*elf_make_rela_name(const char *name);
static int	elf_build_symtab(elf_out_section *symtab, elf_out_section *strtab,
		    const elf64_object_desc *desc, uint32_t *sym_map,
		    uint32_t *first_nonlocal);
static int	elf_build_relas(elf_out_section *out, size_t out_count,
		    const elf64_object_desc *desc, uint32_t *sym_map,
		    size_t symtab_index);
static int	elf_write_file(const char *path, elf_out_section *out,
		    size_t out_count, size_t shstrndx);
static int	elf_write_exec_file(const char *path, elf_out_section *out,
		    size_t out_count, size_t shstrndx,
		    const elf64_exec_desc *desc);

static void
elf_buf_init(elf_buf *buf)
{
	if (!buf) {
		return;
	}
	buf->data = NULL;
	buf->size = 0;
	buf->capacity = 0;
}

static void
elf_buf_free(elf_buf *buf)
{
	if (!buf) {
		return;
	}
	free(buf->data);
	elf_buf_init(buf);
}

static int
elf_buf_reserve(elf_buf *buf, size_t extra)
{
	uint8_t	*next;
	size_t	need, cap;

	if (!buf) {
		return (-1);
	}
	need = buf->size + extra;
	if (need <= buf->capacity) {
		return (0);
	}
	cap = buf->capacity ? buf->capacity : 128;
	while (cap < need) {
		cap *= 2;
	}
	next = realloc(buf->data, cap);
	if (!next) {
		return (-1);
	}
	buf->data = next;
	buf->capacity = cap;
	return (0);
}

static int
elf_buf_set_size(elf_buf *buf, size_t size)
{
	size_t	old;

	if (!buf) {
		return (-1);
	}
	old = buf->size;
	if (size > old) {
		if (elf_buf_reserve(buf, size - old) != 0) {
			return (-1);
		}
		memset(buf->data + old, 0, size - old);
	}
	buf->size = size;
	return (0);
}

static int
elf_buf_write(elf_buf *buf, const void *data, size_t size)
{
	if (!buf || (!data && size != 0)) {
		return (-1);
	}
	if (elf_buf_reserve(buf, size) != 0) {
		return (-1);
	}
	memcpy(buf->data + buf->size, data, size);
	buf->size += size;
	return (0);
}

static int
elf_buf_write_at(elf_buf *buf, size_t off, const void *data, size_t size)
{
	if (!buf || (!data && size != 0) || off + size > buf->size) {
		return (-1);
	}
	memcpy(buf->data + off, data, size);
	return (0);
}

static int
elf_buf_u8(elf_buf *buf, uint8_t value)
{
	return (elf_buf_write(buf, &value, sizeof(value)));
}

static uint32_t
elf_strtab_add(elf_buf *buf, const char *str)
{
	uint32_t	off;
	size_t	len;

	if (!buf || !str) {
		return (0);
	}
	off = (uint32_t)buf->size;
	len = strlen(str) + 1;
	if (elf_buf_write(buf, str, len) != 0) {
		return (0);
	}
	return (off);
}

static uint64_t
elf_align(uint64_t value, uint64_t align)
{
	if (align <= 1) {
		return (value);
	}
	return ((value + align - 1) & ~(align - 1));
}

int
elf64_ident_is_valid(const void *data, size_t size)
{
	const uint8_t	*ident;

	if (!data || size < ELF64_EI_NIDENT) {
		return (0);
	}
	ident = data;
	return (ident[0] == 0x7f && ident[1] == 'E' &&
	    ident[2] == 'L' && ident[3] == 'F' &&
	    ident[4] == ELF64_ELFCLASS64 &&
	    ident[5] == ELF64_ELFDATA2LSB);
}

static int
elf_write_sym(elf_buf *buf, const elf64_symbol_desc *sym, uint32_t name_off)
{
	elf64_sym	out;

	memset(&out, 0, sizeof(out));
	out.st_name = name_off;
	out.st_info = ELF64_ST_INFO(sym->bind, sym->type);
	out.st_shndx = sym->section;
	out.st_value = sym->value;
	out.st_size = sym->size;
	return (elf_buf_write(buf, &out, sizeof(out)));
}

static char *
elf_make_rela_name(const char *name)
{
	char	*out;
	size_t	len;

	if (!name) {
		return (NULL);
	}
	len = strlen(name);
	out = malloc(len + 6);
	if (!out) {
		return (NULL);
	}
	strcpy(out, ".rela");
	strcat(out, name);
	return (out);
}

static int
elf_build_symtab(elf_out_section *symtab, elf_out_section *strtab,
    const elf64_object_desc *desc, uint32_t *sym_map,
    uint32_t *first_nonlocal)
{
	elf64_sym	null_sym;
	uint32_t	*name_offs;
	size_t		i;
	uint32_t	index;

	name_offs = NULL;
	elf_buf_init(&symtab->owned);
	elf_buf_init(&strtab->owned);
	if (elf_buf_u8(&strtab->owned, 0) != 0) {
		return (-1);
	}
	if (desc->symbol_count > 0) {
		name_offs = malloc(sizeof(uint32_t) * desc->symbol_count);
		if (!name_offs) {
			return (-1);
		}
	}
	for (i = 0; i < desc->symbol_count; i++) {
		name_offs[i] = elf_strtab_add(&strtab->owned,
		    desc->symbols[i].name ? desc->symbols[i].name : "");
	}

	memset(&null_sym, 0, sizeof(null_sym));
	if (elf_buf_write(&symtab->owned, &null_sym, sizeof(null_sym)) != 0) {
		free(name_offs);
		return (-1);
	}
	index = 1;
	for (i = 0; i < desc->symbol_count; i++) {
		if (desc->symbols[i].bind != ELF64_STB_LOCAL) {
			continue;
		}
		sym_map[i] = index++;
		if (elf_write_sym(&symtab->owned, &desc->symbols[i],
		    name_offs[i]) != 0) {
			free(name_offs);
			return (-1);
		}
	}
	*first_nonlocal = index;
	for (i = 0; i < desc->symbol_count; i++) {
		if (desc->symbols[i].bind == ELF64_STB_LOCAL) {
			continue;
		}
		sym_map[i] = index++;
		if (elf_write_sym(&symtab->owned, &desc->symbols[i],
		    name_offs[i]) != 0) {
			free(name_offs);
			return (-1);
		}
	}
	free(name_offs);
	symtab->data = symtab->owned.data;
	symtab->size = symtab->owned.size;
	strtab->data = strtab->owned.data;
	strtab->size = strtab->owned.size;
	return (0);
}

static int
elf_build_relas(elf_out_section *out, size_t out_count,
    const elf64_object_desc *desc, uint32_t *sym_map, size_t symtab_index)
{
	elf64_rela	rela;
	size_t		i, j;

	for (i = 1; i < out_count; i++) {
		if (out[i].type != ELF64_SHT_RELA) {
			continue;
		}
		elf_buf_init(&out[i].owned);
		out[i].link = (uint32_t)symtab_index;
		for (j = 0; j < desc->reloc_count; j++) {
			if (desc->relocs[j].section != out[i].info) {
				continue;
			}
			if (desc->relocs[j].symbol >= desc->symbol_count) {
				return (-1);
			}
			rela.r_offset = desc->relocs[j].offset;
			rela.r_info = ELF64_R_INFO(sym_map[desc->relocs[j].symbol],
			    desc->relocs[j].type);
			rela.r_addend = desc->relocs[j].addend;
			if (elf_buf_write(&out[i].owned, &rela,
			    sizeof(rela)) != 0) {
				return (-1);
			}
		}
		out[i].data = out[i].owned.data;
		out[i].size = out[i].owned.size;
	}
	return (0);
}

static int
elf_write_file(const char *path, elf_out_section *out, size_t out_count,
    size_t shstrndx)
{
	elf64_ehdr	ehdr;
	elf64_shdr	shdr;
	elf_buf		file;
	FILE		*fp;
	uint64_t	off, shoff;
	size_t		i;
	int		rc;

	elf_buf_init(&file);
	off = sizeof(elf64_ehdr);
	for (i = 1; i < out_count; i++) {
		out[i].offset = elf_align(off, out[i].align ? out[i].align : 1);
		if (out[i].type != ELF64_SHT_NOBITS) {
			off = out[i].offset + out[i].size;
		}
	}
	shoff = elf_align(off, 8);
	if (elf_buf_set_size(&file, shoff +
	    out_count * sizeof(elf64_shdr)) != 0) {
		elf_buf_free(&file);
		return (-1);
	}

	memset(&ehdr, 0, sizeof(ehdr));
	ehdr.e_ident[0] = 0x7f;
	ehdr.e_ident[1] = 'E';
	ehdr.e_ident[2] = 'L';
	ehdr.e_ident[3] = 'F';
	ehdr.e_ident[4] = ELF64_ELFCLASS64;
	ehdr.e_ident[5] = ELF64_ELFDATA2LSB;
	ehdr.e_ident[6] = ELF64_EV_CURRENT;
	ehdr.e_ident[7] = ELF64_ELFOSABI_SYSV;
	ehdr.e_type = ELF64_ET_REL;
	ehdr.e_machine = ELF64_EM_X86_64;
	ehdr.e_version = ELF64_EV_CURRENT;
	ehdr.e_ehsize = sizeof(elf64_ehdr);
	ehdr.e_shentsize = sizeof(elf64_shdr);
	ehdr.e_shnum = (uint16_t)out_count;
	ehdr.e_shstrndx = (uint16_t)shstrndx;
	ehdr.e_shoff = shoff;
	if (elf_buf_write_at(&file, 0, &ehdr, sizeof(ehdr)) != 0) {
		elf_buf_free(&file);
		return (-1);
	}

	for (i = 1; i < out_count; i++) {
		if (out[i].type == ELF64_SHT_NOBITS || out[i].size == 0) {
			continue;
		}
		if (!out[i].data ||
		    elf_buf_write_at(&file, (size_t)out[i].offset, out[i].data,
		    (size_t)out[i].size) != 0) {
			elf_buf_free(&file);
			return (-1);
		}
	}

	for (i = 0; i < out_count; i++) {
		memset(&shdr, 0, sizeof(shdr));
		shdr.sh_name = out[i].name_off;
		shdr.sh_type = out[i].type;
		shdr.sh_flags = out[i].flags;
		shdr.sh_offset = out[i].offset;
		shdr.sh_size = out[i].size;
		shdr.sh_link = out[i].link;
		shdr.sh_info = out[i].info;
		shdr.sh_addralign = out[i].align;
		shdr.sh_entsize = out[i].entsize;
		if (elf_buf_write_at(&file, (size_t)(shoff +
		    i * sizeof(shdr)), &shdr, sizeof(shdr)) != 0) {
			elf_buf_free(&file);
			return (-1);
		}
	}

	fp = fopen(path, "wb");
	if (!fp) {
		elf_buf_free(&file);
		return (-1);
	}
	rc = 0;
	if (fwrite(file.data, 1, file.size, fp) != file.size) {
		rc = -1;
	}
	if (fclose(fp) != 0) {
		rc = -1;
	}
	elf_buf_free(&file);
	return (rc);
}

int
elf64_write_relocatable(const char *path, const elf64_object_desc *desc)
{
	elf_out_section	*out;
	elf_buf		shstr;
	uint32_t	*sym_map;
	uint32_t	first_nonlocal;
	size_t		i, j, out_count, rela_count;
	size_t		symtab_index, strtab_index, shstrtab_index;
	int		rc;

	if (!path || !desc || !desc->sections || desc->section_count == 0) {
		return (-1);
	}
	rela_count = 0;
	for (i = 0; i < desc->section_count; i++) {
		for (j = 0; j < desc->reloc_count; j++) {
			if (desc->relocs[j].section == i + 1) {
				rela_count++;
				break;
			}
		}
	}
	out_count = 1 + desc->section_count + rela_count + 3;
	out = calloc(out_count, sizeof(*out));
	sym_map = calloc(desc->symbol_count ? desc->symbol_count : 1,
	    sizeof(*sym_map));
	if (!out || !sym_map) {
		free(out);
		free(sym_map);
		return (-1);
	}

	elf_buf_init(&shstr);
	elf_buf_u8(&shstr, 0);
	for (i = 0; i < out_count; i++) {
		elf_buf_init(&out[i].owned);
	}
	for (i = 0; i < desc->section_count; i++) {
		out[i + 1].name = desc->sections[i].name;
		out[i + 1].name_off = elf_strtab_add(&shstr,
		    desc->sections[i].name);
		out[i + 1].type = desc->sections[i].type;
		out[i + 1].flags = desc->sections[i].flags;
		out[i + 1].align = desc->sections[i].align ?
		    desc->sections[i].align : 1;
		out[i + 1].entsize = desc->sections[i].entsize;
		out[i + 1].data = desc->sections[i].data;
		out[i + 1].size = desc->sections[i].size;
	}

	j = 1 + desc->section_count;
	for (i = 0; i < desc->section_count; i++) {
		size_t	k;

		for (k = 0; k < desc->reloc_count; k++) {
			if (desc->relocs[k].section != i + 1) {
				continue;
			}
			out[j].name = elf_make_rela_name(desc->sections[i].name);
			out[j].name_off = elf_strtab_add(&shstr, out[j].name);
			out[j].type = ELF64_SHT_RELA;
			out[j].align = 8;
			out[j].entsize = sizeof(elf64_rela);
			out[j].info = (uint32_t)(i + 1);
			j++;
			break;
		}
	}

	symtab_index = out_count - 3;
	strtab_index = out_count - 2;
	shstrtab_index = out_count - 1;
	out[symtab_index].name = ".symtab";
	out[symtab_index].name_off = elf_strtab_add(&shstr, ".symtab");
	out[symtab_index].type = ELF64_SHT_SYMTAB;
	out[symtab_index].align = 8;
	out[symtab_index].entsize = sizeof(elf64_sym);
	out[symtab_index].link = (uint32_t)strtab_index;
	out[strtab_index].name = ".strtab";
	out[strtab_index].name_off = elf_strtab_add(&shstr, ".strtab");
	out[strtab_index].type = ELF64_SHT_STRTAB;
	out[strtab_index].align = 1;
	out[shstrtab_index].name = ".shstrtab";
	out[shstrtab_index].name_off = elf_strtab_add(&shstr, ".shstrtab");
	out[shstrtab_index].type = ELF64_SHT_STRTAB;
	out[shstrtab_index].align = 1;

	rc = -1;
	if (elf_build_symtab(&out[symtab_index], &out[strtab_index],
	    desc, sym_map, &first_nonlocal) == 0 &&
	    elf_build_relas(out, out_count, desc, sym_map, symtab_index) == 0) {
		out[symtab_index].info = first_nonlocal;
		out[shstrtab_index].owned = shstr;
		out[shstrtab_index].data = shstr.data;
		out[shstrtab_index].size = shstr.size;
		rc = elf_write_file(path, out, out_count, shstrtab_index);
	}

	for (i = 1 + desc->section_count; i < out_count; i++) {
		if (out[i].name && strncmp(out[i].name, ".rela", 5) == 0) {
			free((void *)out[i].name);
		}
	}
	for (i = 0; i < out_count; i++) {
		if (i == shstrtab_index) {
			continue;
		}
		elf_buf_free(&out[i].owned);
	}
	if (rc != 0) {
		elf_buf_free(&shstr);
	}
	free(sym_map);
	free(out);
	return (rc);
}

static int
elf_write_exec_file(const char *path, elf_out_section *out, size_t out_count,
    size_t shstrndx, const elf64_exec_desc *desc)
{
	elf64_ehdr	ehdr;
	elf64_phdr	phdr;
	elf64_shdr	shdr;
	elf_buf		file;
	FILE		*fp;
	uint64_t	base, file_base, file_end, load_end, mem_end, shoff;
	size_t		i;
	int		rc;

	base = desc->base_vaddr;
	file_base = 0x1000;
	file_end = file_base;
	mem_end = base;
	for (i = 1; i < out_count; i++) {
		if ((out[i].flags & ELF64_SHF_ALLOC) == 0) {
			continue;
		}
		if (out[i].vaddr + out[i].size > mem_end) {
			mem_end = out[i].vaddr + out[i].size;
		}
		if (out[i].type == ELF64_SHT_NOBITS) {
			out[i].offset = file_base + (out[i].vaddr - base);
			continue;
		}
		out[i].offset = file_base + (out[i].vaddr - base);
		if (out[i].offset + out[i].size > file_end) {
			file_end = out[i].offset + out[i].size;
		}
	}
	load_end = file_end;

	out[shstrndx].offset = file_end;
	file_end += out[shstrndx].size;
	shoff = elf_align(file_end, 8);

	elf_buf_init(&file);
	if (elf_buf_set_size(&file, shoff +
	    out_count * sizeof(elf64_shdr)) != 0) {
		elf_buf_free(&file);
		return (-1);
	}

	memset(&ehdr, 0, sizeof(ehdr));
	ehdr.e_ident[0] = 0x7f;
	ehdr.e_ident[1] = 'E';
	ehdr.e_ident[2] = 'L';
	ehdr.e_ident[3] = 'F';
	ehdr.e_ident[4] = ELF64_ELFCLASS64;
	ehdr.e_ident[5] = ELF64_ELFDATA2LSB;
	ehdr.e_ident[6] = ELF64_EV_CURRENT;
	ehdr.e_ident[7] = ELF64_ELFOSABI_SYSV;
	ehdr.e_type = ELF64_ET_EXEC;
	ehdr.e_machine = ELF64_EM_X86_64;
	ehdr.e_version = ELF64_EV_CURRENT;
	ehdr.e_entry = desc->entry;
	ehdr.e_phoff = sizeof(elf64_ehdr);
	ehdr.e_shoff = shoff;
	ehdr.e_ehsize = sizeof(elf64_ehdr);
	ehdr.e_phentsize = sizeof(elf64_phdr);
	ehdr.e_phnum = 1;
	ehdr.e_shentsize = sizeof(elf64_shdr);
	ehdr.e_shnum = (uint16_t)out_count;
	ehdr.e_shstrndx = (uint16_t)shstrndx;

	memset(&phdr, 0, sizeof(phdr));
	phdr.p_type = ELF64_PT_LOAD;
	phdr.p_flags = ELF64_PF_R | ELF64_PF_W | ELF64_PF_X;
	phdr.p_offset = file_base;
	phdr.p_vaddr = base;
	phdr.p_paddr = base;
	phdr.p_filesz = load_end > file_base ? load_end - file_base : 0;
	phdr.p_memsz = mem_end > base ? mem_end - base : 0;
	phdr.p_align = 0x1000;

	if (elf_buf_write_at(&file, 0, &ehdr, sizeof(ehdr)) != 0 ||
	    elf_buf_write_at(&file, sizeof(ehdr), &phdr, sizeof(phdr)) != 0) {
		elf_buf_free(&file);
		return (-1);
	}
	for (i = 1; i < out_count; i++) {
		if (out[i].type == ELF64_SHT_NOBITS || out[i].size == 0) {
			continue;
		}
		if (!out[i].data ||
		    elf_buf_write_at(&file, (size_t)out[i].offset, out[i].data,
		    (size_t)out[i].size) != 0) {
			elf_buf_free(&file);
			return (-1);
		}
	}
	for (i = 0; i < out_count; i++) {
		memset(&shdr, 0, sizeof(shdr));
		shdr.sh_name = out[i].name_off;
		shdr.sh_type = out[i].type;
		shdr.sh_flags = out[i].flags;
		shdr.sh_addr = out[i].vaddr;
		shdr.sh_offset = out[i].offset;
		shdr.sh_size = out[i].size;
		shdr.sh_addralign = out[i].align;
		if (elf_buf_write_at(&file, (size_t)(shoff +
		    i * sizeof(shdr)), &shdr, sizeof(shdr)) != 0) {
			elf_buf_free(&file);
			return (-1);
		}
	}

	fp = fopen(path, "wb");
	if (!fp) {
		elf_buf_free(&file);
		return (-1);
	}
	rc = 0;
	if (fwrite(file.data, 1, file.size, fp) != file.size) {
		rc = -1;
	}
	if (fclose(fp) != 0) {
		rc = -1;
	}
	elf_buf_free(&file);
	return (rc);
}

int
elf64_write_executable(const char *path, const elf64_exec_desc *desc)
{
	elf_out_section	*out;
	elf_buf		shstr;
	size_t		i, out_count, shstrndx;
	int		rc;

	if (!path || !desc || !desc->sections || desc->section_count == 0) {
		return (-1);
	}
	out_count = 1 + desc->section_count + 1;
	shstrndx = out_count - 1;
	out = calloc(out_count, sizeof(*out));
	if (!out) {
		return (-1);
	}
	elf_buf_init(&shstr);
	elf_buf_u8(&shstr, 0);
	for (i = 1; i < out_count; i++) {
		elf_buf_init(&out[i].owned);
	}
	for (i = 0; i < desc->section_count; i++) {
		out[i + 1].name = desc->sections[i].name;
		out[i + 1].name_off = elf_strtab_add(&shstr,
		    desc->sections[i].name);
		out[i + 1].type = desc->sections[i].type;
		out[i + 1].flags = desc->sections[i].flags;
		out[i + 1].align = desc->sections[i].align ?
		    desc->sections[i].align : 1;
		out[i + 1].data = desc->sections[i].data;
		out[i + 1].size = desc->sections[i].size;
		out[i + 1].vaddr = desc->sections[i].vaddr;
	}
	out[shstrndx].name = ".shstrtab";
	out[shstrndx].name_off = elf_strtab_add(&shstr, ".shstrtab");
	out[shstrndx].type = ELF64_SHT_STRTAB;
	out[shstrndx].align = 1;
	out[shstrndx].owned = shstr;
	out[shstrndx].data = shstr.data;
	out[shstrndx].size = shstr.size;
	rc = elf_write_exec_file(path, out, out_count, shstrndx, desc);
	if (rc != 0) {
		elf_buf_free(&shstr);
	}
	for (i = 0; i < out_count; i++) {
		if (i == shstrndx) {
			continue;
		}
		elf_buf_free(&out[i].owned);
	}
	free(out);
	return (rc);
}
