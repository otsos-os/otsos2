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

$define %type ld_object as loaded ELF64 relocatable object
$define %type ld_global as resolved global symbol
$define %type ld_context as linker state
$define %func main as start with args int, char **, char **
$define %func ld_read_file as function with args const char *, uint8_t **, size_t *
$define %func ld_load_object as function with args ld_context *, const char *
$define %func ld_merge_sections as function with args ld_context *
$define %func ld_apply_relocations as function with args ld_context *
$define %func ld_emit_native_trampoline as function with args ld_context *
$define %func ld_write_output as function with args ld_context *

*/

/* !SPACE!

$space %internal ld_init, ld_free, ld_error, ld_align
$space %internal ld_read_file, ld_section_name, ld_load_object
$space %internal ld_output_buffer, ld_section_size, ld_align_output
$space %internal ld_classify_section, ld_merge_sections, ld_layout
$space %internal ld_resolve_symbol, ld_find_global, ld_add_global
$space %internal ld_collect_globals, ld_patch_value, ld_apply_relocations
$space %internal ld_find_named_symbol, ld_find_entry
$space %internal ld_arg_looks_object, ld_set_native_target
$space %internal ld_emit_native_trampoline, ld_patch_native_trampoline
$space %internal ld_write_output, ld_usage
$space %export main

*/

#include <libelf.h>
#include <libemit.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LD_MAX_OBJECTS	64
#define LD_MAX_GLOBALS	1024

#define LD_OUT_NONE	0
#define LD_OUT_TEXT	1
#define LD_OUT_RODATA	2
#define LD_OUT_DATA	3
#define LD_OUT_BSS	4
#define LD_OUT_COUNT	5

typedef struct ld_object {
	const char	*path;
	uint8_t		*data;
	size_t		size;
	elf64_ehdr	*ehdr;
	elf64_shdr	*shdrs;
	const char	*shstr;
	elf64_sym	*symtab;
	const char	*strtab;
	uint16_t	*section_kind;
	uint64_t	*section_off;
	size_t		sym_count;
} ld_object;

typedef struct ld_global {
	const char	*name;
	ld_object	*obj;
	uint32_t	sym_index;
	uint64_t	value;
} ld_global;

typedef struct ld_context {
	ld_object	objects[LD_MAX_OBJECTS];
	ld_global	globals[LD_MAX_GLOBALS];
	emit_buf	text;
	emit_buf	rodata;
	emit_buf	data;
	const char	*output;
	const char	*entry_name;
	const char	*native_target_name;
	uint64_t	base_vaddr;
	uint64_t	native_target_addr;
	uint64_t	native_trampoline_off;
	uint64_t	native_trampoline_target_off;
	uint64_t	native_trampoline_entry;
	uint64_t	out_vaddr[LD_OUT_COUNT];
	uint64_t	out_align[LD_OUT_COUNT];
	uint64_t	bss_size;
	int		object_count;
	int		global_count;
	int		native_enabled;
	int		native_target_is_addr;
	int		errors;
} ld_context;

static void	ld_init(ld_context *ctx);
static void	ld_free(ld_context *ctx);
static int	ld_error(ld_context *ctx, const char *fmt, ...);
static uint64_t	ld_align(uint64_t value, uint64_t align);
static int	ld_read_file(const char *path, uint8_t **out, size_t *out_size);
static const char *ld_section_name(ld_object *obj, uint16_t section);
static int	ld_load_object(ld_context *ctx, const char *path);
static emit_buf	*ld_output_buffer(ld_context *ctx, uint16_t kind);
static uint64_t	ld_section_size(ld_context *ctx, uint16_t kind);
static int	ld_align_output(ld_context *ctx, uint16_t kind, uint64_t align);
static uint16_t	ld_classify_section(elf64_shdr *shdr);
static int	ld_merge_sections(ld_context *ctx);
static void	ld_layout(ld_context *ctx);
static int	ld_resolve_symbol(ld_context *ctx, ld_object *obj,
		    uint32_t sym_index, uint64_t *out);
static int	ld_find_global(ld_context *ctx, const char *name);
static int	ld_add_global(ld_context *ctx, const char *name,
		    ld_object *obj, uint32_t sym_index, uint64_t value);
static int	ld_collect_globals(ld_context *ctx);
static int	ld_patch_value(ld_context *ctx, uint16_t kind, uint64_t off,
		    uint32_t type, int64_t value);
static int	ld_apply_relocations(ld_context *ctx);
static int	ld_find_named_symbol(ld_context *ctx, const char *name,
		    uint64_t *value);
static int	ld_find_entry(ld_context *ctx, uint64_t *entry);
static int	ld_arg_looks_object(const char *arg);
static int	ld_set_native_target(ld_context *ctx, const char *target);
static int	ld_emit_native_trampoline(ld_context *ctx);
static int	ld_patch_native_trampoline(ld_context *ctx);
static int	ld_write_output(ld_context *ctx);
static void	ld_usage(void);

static void
ld_init(ld_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->output = "a.out";
	ctx->entry_name = "_start";
	ctx->base_vaddr = 0x400000;
	ctx->out_align[LD_OUT_TEXT] = 16;
	ctx->out_align[LD_OUT_RODATA] = 8;
	ctx->out_align[LD_OUT_DATA] = 8;
	ctx->out_align[LD_OUT_BSS] = 8;
	emit_buf_init(&ctx->text);
	emit_buf_init(&ctx->rodata);
	emit_buf_init(&ctx->data);
}

static void
ld_free(ld_context *ctx)
{
	ld_object	*obj;
	int		i;

	for (i = 0; i < ctx->object_count; i++) {
		obj = &ctx->objects[i];
		free(obj->data);
		free(obj->section_kind);
		free(obj->section_off);
	}
	emit_buf_free(&ctx->text);
	emit_buf_free(&ctx->rodata);
	emit_buf_free(&ctx->data);
}

static int
ld_error(ld_context *ctx, const char *fmt, ...)
{
	va_list	ap;

	fprintf(stderr, "ld: error: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	ctx->errors++;
	return (-1);
}

static uint64_t
ld_align(uint64_t value, uint64_t align)
{
	if (align <= 1) {
		return (value);
	}
	return ((value + align - 1) & ~(align - 1));
}

static int
ld_read_file(const char *path, uint8_t **out, size_t *out_size)
{
	FILE	*fp;
	uint8_t	*buf;
	long	size;

	fp = fopen(path, "rb");
	if (!fp) {
		return (-1);
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		return (-1);
	}
	size = ftell(fp);
	if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		return (-1);
	}
	buf = malloc((size_t)size);
	if (!buf) {
		fclose(fp);
		return (-1);
	}
	if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
		free(buf);
		fclose(fp);
		return (-1);
	}
	fclose(fp);
	*out = buf;
	*out_size = (size_t)size;
	return (0);
}

static const char *
ld_section_name(ld_object *obj, uint16_t section)
{
	if (!obj || section >= obj->ehdr->e_shnum) {
		return ("");
	}
	return (obj->shstr + obj->shdrs[section].sh_name);
}

static int
ld_load_object(ld_context *ctx, const char *path)
{
	ld_object	*obj;
	elf64_shdr	*symtab, *strtab, *shstr;
	uint64_t	end;
	int		i;

	if (ctx->object_count >= LD_MAX_OBJECTS) {
		return (ld_error(ctx, "too many input objects"));
	}
	obj = &ctx->objects[ctx->object_count++];
	memset(obj, 0, sizeof(*obj));
	obj->path = path;
	if (ld_read_file(path, &obj->data, &obj->size) != 0) {
		return (ld_error(ctx, "cannot read %s", path));
	}
	if (!elf64_ident_is_valid(obj->data, obj->size) ||
	    obj->size < sizeof(elf64_ehdr)) {
		return (ld_error(ctx, "%s is not ELF64", path));
	}
	obj->ehdr = (elf64_ehdr *)obj->data;
	if (obj->ehdr->e_type != ELF64_ET_REL ||
	    obj->ehdr->e_machine != ELF64_EM_X86_64) {
		return (ld_error(ctx, "%s is not x86_64 relocatable ELF", path));
	}
	if (obj->ehdr->e_shentsize != sizeof(elf64_shdr)) {
		return (ld_error(ctx, "%s has unsupported section headers", path));
	}
	end = obj->ehdr->e_shoff +
	    (uint64_t)obj->ehdr->e_shnum * sizeof(elf64_shdr);
	if (end > obj->size || obj->ehdr->e_shstrndx >= obj->ehdr->e_shnum) {
		return (ld_error(ctx, "%s has bad section headers", path));
	}
	obj->shdrs = (elf64_shdr *)(obj->data + obj->ehdr->e_shoff);
	shstr = &obj->shdrs[obj->ehdr->e_shstrndx];
	if (shstr->sh_offset + shstr->sh_size > obj->size) {
		return (ld_error(ctx, "%s has bad shstrtab", path));
	}
	obj->shstr = (const char *)(obj->data + shstr->sh_offset);
	obj->section_kind = calloc(obj->ehdr->e_shnum, sizeof(uint16_t));
	obj->section_off = calloc(obj->ehdr->e_shnum, sizeof(uint64_t));
	if (!obj->section_kind || !obj->section_off) {
		return (ld_error(ctx, "out of memory"));
	}
	for (i = 0; i < obj->ehdr->e_shnum; i++) {
		if (obj->shdrs[i].sh_type != ELF64_SHT_SYMTAB) {
			continue;
		}
		symtab = &obj->shdrs[i];
		if (symtab->sh_link >= obj->ehdr->e_shnum) {
			return (ld_error(ctx, "%s has bad symtab link", path));
		}
		strtab = &obj->shdrs[symtab->sh_link];
		if (strtab->sh_type != ELF64_SHT_STRTAB ||
		    symtab->sh_entsize != sizeof(elf64_sym)) {
			return (ld_error(ctx, "%s has bad symbol tables", path));
		}
		if (symtab->sh_offset + symtab->sh_size > obj->size ||
		    strtab->sh_offset + strtab->sh_size > obj->size) {
			return (ld_error(ctx, "%s has bad symbol tables", path));
		}
		obj->symtab = (elf64_sym *)(obj->data + symtab->sh_offset);
		obj->sym_count = symtab->sh_size / sizeof(elf64_sym);
		obj->strtab = (const char *)(obj->data + strtab->sh_offset);
		break;
	}
	return (0);
}

static emit_buf *
ld_output_buffer(ld_context *ctx, uint16_t kind)
{
	if (kind == LD_OUT_TEXT) {
		return (&ctx->text);
	}
	if (kind == LD_OUT_RODATA) {
		return (&ctx->rodata);
	}
	if (kind == LD_OUT_DATA) {
		return (&ctx->data);
	}
	return (NULL);
}

static uint64_t
ld_section_size(ld_context *ctx, uint16_t kind)
{
	emit_buf	*buf;

	if (kind == LD_OUT_BSS) {
		return (ctx->bss_size);
	}
	buf = ld_output_buffer(ctx, kind);
	return (buf ? buf->size : 0);
}

static int
ld_align_output(ld_context *ctx, uint16_t kind, uint64_t align)
{
	emit_buf	*buf;

	if (align <= 1) {
		return (0);
	}
	if (kind < LD_OUT_COUNT && align > ctx->out_align[kind]) {
		ctx->out_align[kind] = align;
	}
	if (kind == LD_OUT_BSS) {
		ctx->bss_size = ld_align(ctx->bss_size, align);
		return (0);
	}
	buf = ld_output_buffer(ctx, kind);
	if (!buf) {
		return (-1);
	}
	return (emit_buf_align(buf, (size_t)align, 0));
}

static uint16_t
ld_classify_section(elf64_shdr *shdr)
{
	if ((shdr->sh_flags & ELF64_SHF_ALLOC) == 0) {
		return (LD_OUT_NONE);
	}
	if (shdr->sh_type == ELF64_SHT_NOBITS) {
		return (LD_OUT_BSS);
	}
	if ((shdr->sh_flags & ELF64_SHF_EXECINSTR) != 0) {
		return (LD_OUT_TEXT);
	}
	if ((shdr->sh_flags & ELF64_SHF_WRITE) != 0) {
		return (LD_OUT_DATA);
	}
	return (LD_OUT_RODATA);
}

static int
ld_merge_sections(ld_context *ctx)
{
	ld_object	*obj;
	elf64_shdr	*shdr;
	emit_buf	*buf;
	uint16_t	kind;
	int		i, j;

	for (i = 0; i < ctx->object_count; i++) {
		obj = &ctx->objects[i];
		for (j = 1; j < obj->ehdr->e_shnum; j++) {
			shdr = &obj->shdrs[j];
			kind = ld_classify_section(shdr);
			if (kind == LD_OUT_NONE) {
				continue;
			}
			if (shdr->sh_type != ELF64_SHT_PROGBITS &&
			    shdr->sh_type != ELF64_SHT_NOBITS) {
				continue;
			}
			if (ld_align_output(ctx, kind, shdr->sh_addralign) != 0) {
				return (ld_error(ctx, "out of memory"));
			}
			obj->section_kind[j] = kind;
			obj->section_off[j] = ld_section_size(ctx, kind);
			if (shdr->sh_type == ELF64_SHT_NOBITS) {
				ctx->bss_size += shdr->sh_size;
				continue;
			}
			if (shdr->sh_offset + shdr->sh_size > obj->size) {
				return (ld_error(ctx, "%s: bad section %s",
				    obj->path, ld_section_name(obj, j)));
			}
			buf = ld_output_buffer(ctx, kind);
			if (!buf || emit_buf_write(buf, obj->data +
			    shdr->sh_offset, (size_t)shdr->sh_size) != 0) {
				return (ld_error(ctx, "out of memory"));
			}
		}
	}
	return (0);
}

static void
ld_layout(ld_context *ctx)
{
	ctx->out_vaddr[LD_OUT_TEXT] = ctx->base_vaddr;
	ctx->out_vaddr[LD_OUT_RODATA] = ld_align(ctx->out_vaddr[LD_OUT_TEXT] +
	    ctx->text.size, ctx->out_align[LD_OUT_RODATA]);
	ctx->out_vaddr[LD_OUT_DATA] = ld_align(ctx->out_vaddr[LD_OUT_RODATA] +
	    ctx->rodata.size, ctx->out_align[LD_OUT_DATA]);
	ctx->out_vaddr[LD_OUT_BSS] = ld_align(ctx->out_vaddr[LD_OUT_DATA] +
	    ctx->data.size, ctx->out_align[LD_OUT_BSS]);
}

static int
ld_resolve_symbol(ld_context *ctx, ld_object *obj, uint32_t sym_index,
    uint64_t *out)
{
	elf64_sym	*sym;
	const char	*name;
	uint16_t	kind;
	int		global;

	if (!obj->symtab || sym_index >= obj->sym_count) {
		return (ld_error(ctx, "%s: bad symbol index", obj->path));
	}
	sym = &obj->symtab[sym_index];
	if (sym->st_shndx == ELF64_SHN_ABS) {
		*out = sym->st_value;
		return (0);
	}
	if (sym->st_shndx == ELF64_SHN_UNDEF) {
		name = obj->strtab + sym->st_name;
		global = ld_find_global(ctx, name);
		if (global < 0) {
			return (ld_error(ctx, "undefined symbol '%s'", name));
		}
		*out = ctx->globals[global].value;
		return (0);
	}
	if (sym->st_shndx >= obj->ehdr->e_shnum) {
		return (ld_error(ctx, "%s: bad symbol section", obj->path));
	}
	kind = obj->section_kind[sym->st_shndx];
	if (kind == LD_OUT_NONE) {
		return (ld_error(ctx, "%s: symbol in discarded section",
		    obj->path));
	}
	*out = ctx->out_vaddr[kind] + obj->section_off[sym->st_shndx] +
	    sym->st_value;
	return (0);
}

static int
ld_find_global(ld_context *ctx, const char *name)
{
	int	i;

	for (i = 0; i < ctx->global_count; i++) {
		if (strcmp(ctx->globals[i].name, name) == 0) {
			return (i);
		}
	}
	return (-1);
}

static int
ld_add_global(ld_context *ctx, const char *name, ld_object *obj,
    uint32_t sym_index, uint64_t value)
{
	ld_global	*global;
	int		existing;

	existing = ld_find_global(ctx, name);
	if (existing >= 0) {
		return (ld_error(ctx, "duplicate symbol '%s'", name));
	}
	if (ctx->global_count >= LD_MAX_GLOBALS) {
		return (ld_error(ctx, "too many global symbols"));
	}
	global = &ctx->globals[ctx->global_count++];
	global->name = name;
	global->obj = obj;
	global->sym_index = sym_index;
	global->value = value;
	return (0);
}

static int
ld_collect_globals(ld_context *ctx)
{
	ld_object	*obj;
	elf64_sym	*sym;
	const char	*name;
	uint64_t	value;
	size_t		i;
	int		j;

	for (j = 0; j < ctx->object_count; j++) {
		obj = &ctx->objects[j];
		if (!obj->symtab) {
			continue;
		}
		for (i = 1; i < obj->sym_count; i++) {
			sym = &obj->symtab[i];
			if (ELF64_ST_BIND(sym->st_info) != ELF64_STB_GLOBAL ||
			    sym->st_shndx == ELF64_SHN_UNDEF) {
				continue;
			}
			name = obj->strtab + sym->st_name;
			if (!name || name[0] == '\0') {
				continue;
			}
			if (ld_resolve_symbol(ctx, obj, (uint32_t)i,
			    &value) != 0) {
				return (-1);
			}
			if (ld_add_global(ctx, name, obj, (uint32_t)i,
			    value) != 0) {
				return (-1);
			}
		}
	}
	return (0);
}

static int
ld_patch_value(ld_context *ctx, uint16_t kind, uint64_t off, uint32_t type,
    int64_t value)
{
	emit_buf	*buf;
	uint64_t	v64;
	uint32_t	v32u;
	int32_t		v32;

	if (type == ELF64_R_X86_64_NONE) {
		return (0);
	}
	buf = ld_output_buffer(ctx, kind);
	if (!buf) {
		return (ld_error(ctx, "relocation targets .bss"));
	}
	if (type == ELF64_R_X86_64_64) {
		v64 = (uint64_t)value;
		if (off + sizeof(v64) > buf->size ||
		    emit_buf_write_at(buf, (size_t)off, &v64,
		    sizeof(v64)) != 0) {
			return (ld_error(ctx, "relocation outside section"));
		}
		return (0);
	}
	if (type == ELF64_R_X86_64_32) {
		if (value < 0 || (uint64_t)value > UINT32_MAX) {
			return (ld_error(ctx, "relocation overflow"));
		}
		v32u = (uint32_t)value;
		if (off + sizeof(v32u) > buf->size ||
		    emit_buf_write_at(buf, (size_t)off, &v32u,
		    sizeof(v32u)) != 0) {
			return (ld_error(ctx, "relocation outside section"));
		}
		return (0);
	}
	if (type == ELF64_R_X86_64_PC32 || type == ELF64_R_X86_64_PLT32 ||
	    type == ELF64_R_X86_64_32S) {
		if (value < INT32_MIN || value > INT32_MAX) {
			return (ld_error(ctx, "relocation overflow"));
		}
		v32 = (int32_t)value;
		if (off + sizeof(v32) > buf->size ||
		    emit_buf_write_at(buf, (size_t)off, &v32,
		    sizeof(v32)) != 0) {
			return (ld_error(ctx, "relocation outside section"));
		}
		return (0);
	}
	return (ld_error(ctx, "unsupported relocation type %d", type));
}

static int
ld_apply_relocations(ld_context *ctx)
{
	ld_object	*obj;
	elf64_shdr	*shdr;
	elf64_rela	*rela;
	uint64_t	s, p, place;
	uint32_t	sym, type;
	uint16_t	kind;
	size_t		count, i;
	int		j, k;

	for (j = 0; j < ctx->object_count; j++) {
		obj = &ctx->objects[j];
		for (k = 1; k < obj->ehdr->e_shnum; k++) {
			shdr = &obj->shdrs[k];
			if (shdr->sh_type != ELF64_SHT_RELA) {
				continue;
			}
			if (shdr->sh_info >= obj->ehdr->e_shnum ||
			    shdr->sh_offset + shdr->sh_size > obj->size) {
				return (ld_error(ctx, "%s: bad relocation section",
				    obj->path));
			}
			if (shdr->sh_entsize != sizeof(elf64_rela) ||
			    (shdr->sh_size % sizeof(elf64_rela)) != 0) {
				return (ld_error(ctx, "%s: bad relocation entries",
				    obj->path));
			}
			kind = obj->section_kind[shdr->sh_info];
			if (kind == LD_OUT_NONE) {
				continue;
			}
			rela = (elf64_rela *)(obj->data + shdr->sh_offset);
			count = shdr->sh_size / sizeof(elf64_rela);
			for (i = 0; i < count; i++) {
				sym = ELF64_R_SYM(rela[i].r_info);
				type = ELF64_R_TYPE(rela[i].r_info);
				if (type == ELF64_R_X86_64_NONE) {
					continue;
				}
				if (ld_resolve_symbol(ctx, obj, sym, &s) != 0) {
					return (-1);
				}
				place = obj->section_off[shdr->sh_info] +
				    rela[i].r_offset;
				p = ctx->out_vaddr[kind] + place;
				if (type == ELF64_R_X86_64_PC32 ||
				    type == ELF64_R_X86_64_PLT32) {
					if (ld_patch_value(ctx, kind, place, type,
					    (int64_t)(s + rela[i].r_addend - p)) != 0) {
						return (-1);
					}
				} else {
					if (ld_patch_value(ctx, kind, place, type,
					    (int64_t)(s + rela[i].r_addend)) != 0) {
						return (-1);
					}
				}
			}
		}
	}
	return (0);
}

static int
ld_find_named_symbol(ld_context *ctx, const char *find, uint64_t *value)
{
	ld_object	*obj;
	elf64_sym	*sym;
	const char	*name;
	size_t		i;
	int		j, global;

	global = ld_find_global(ctx, find);
	if (global >= 0) {
		*value = ctx->globals[global].value;
		return (0);
	}
	for (j = 0; j < ctx->object_count; j++) {
		obj = &ctx->objects[j];
		if (!obj->symtab) {
			continue;
		}
		for (i = 1; i < obj->sym_count; i++) {
			sym = &obj->symtab[i];
			name = obj->strtab + sym->st_name;
			if (strcmp(name, find) != 0 ||
			    sym->st_shndx == ELF64_SHN_UNDEF) {
				continue;
			}
			return (ld_resolve_symbol(ctx, obj, (uint32_t)i,
			    value));
		}
	}
	return (-1);
}

static int
ld_find_entry(ld_context *ctx, uint64_t *entry)
{
	int	errors;

	errors = ctx->errors;
	if (ld_find_named_symbol(ctx, ctx->entry_name, entry) == 0) {
		return (0);
	}
	if (ctx->errors != errors) {
		return (-1);
	}
	return (ld_error(ctx, "entry symbol '%s' not found",
	    ctx->entry_name));
}

static int
ld_arg_looks_object(const char *arg)
{
	size_t	len;

	if (!arg) {
		return (0);
	}
	len = strlen(arg);
	if (len >= 2 && strcmp(arg + len - 2, ".o") == 0) {
		return (1);
	}
	if (len >= 4 && strcmp(arg + len - 4, ".obj") == 0) {
		return (1);
	}
	return (0);
}

static int
ld_set_native_target(ld_context *ctx, const char *target)
{
	char		*end;
	unsigned long	addr;

	ctx->native_enabled = 1;
	if (!target || target[0] == '\0') {
		return (0);
	}
	addr = strtoul(target, &end, 0);
	if (end != target && *end == '\0') {
		ctx->native_target_is_addr = 1;
		ctx->native_target_addr = (uint64_t)addr;
		ctx->native_target_name = NULL;
		return (0);
	}
	ctx->native_target_is_addr = 0;
	ctx->native_target_name = target;
	return (0);
}

static int
ld_emit_native_trampoline(ld_context *ctx)
{
	static const uint8_t head[] = {
		0x57,                         /* push %rdi */
		0x56,                         /* push %rsi */
		0x52,                         /* push %rdx */
		0x48, 0xc7, 0xc0, 0xff, 0xff, 0x00, 0x00,
		0x48, 0x31, 0xff,             /* xor %rdi, %rdi */
		0x0f, 0x05,                   /* syscall */
		0x5a,                         /* pop %rdx */
		0x5e,                         /* pop %rsi */
		0x5f,                         /* pop %rdi */
		0x49, 0xbb                    /* movabs target, %r11 */
	};
	static const uint8_t tail[] = {
		0x41, 0xff, 0xe3              /* jmp *%r11 */
	};
	uint64_t	zero;

	zero = 0;
	if (emit_buf_align(&ctx->text, 16, 0) != 0) {
		return (ld_error(ctx, "out of memory"));
	}
	ctx->native_trampoline_off = ctx->text.size;
	ctx->native_trampoline_target_off = ctx->text.size + sizeof(head);
	if (emit_buf_write(&ctx->text, head, sizeof(head)) != 0 ||
	    emit_buf_write(&ctx->text, &zero, sizeof(zero)) != 0 ||
	    emit_buf_write(&ctx->text, tail, sizeof(tail)) != 0) {
		return (ld_error(ctx, "out of memory"));
	}
	return (0);
}

static int
ld_patch_native_trampoline(ld_context *ctx)
{
	const char	*target_name;
	uint64_t	target;
	int		errors;

	if (!ctx->native_enabled) {
		return (0);
	}
	if (ctx->native_target_is_addr) {
		target = ctx->native_target_addr;
	} else {
		target_name = ctx->native_target_name ?
		    ctx->native_target_name : ctx->entry_name;
		errors = ctx->errors;
		if (ld_find_named_symbol(ctx, target_name, &target) != 0) {
			if (ctx->errors != errors) {
				return (-1);
			}
			return (ld_error(ctx, "native target '%s' not found",
			    target_name));
		}
	}
	ctx->native_trampoline_entry = ctx->out_vaddr[LD_OUT_TEXT] +
	    ctx->native_trampoline_off;
	if (emit_buf_write_at(&ctx->text,
	    (size_t)ctx->native_trampoline_target_off, &target,
	    sizeof(target)) != 0) {
		return (ld_error(ctx, "cannot patch native trampoline"));
	}
	return (0);
}

static int
ld_write_output(ld_context *ctx)
{
	elf64_exec_section_desc	sections[4];
	elf64_exec_desc		desc;
	uint64_t		entry;

	if (ctx->native_enabled) {
		entry = ctx->native_trampoline_entry;
	} else {
		if (ld_find_entry(ctx, &entry) != 0) {
			return (-1);
		}
	}
	memset(sections, 0, sizeof(sections));
	sections[0].name = ".text";
	sections[0].type = ELF64_SHT_PROGBITS;
	sections[0].flags = ELF64_SHF_ALLOC | ELF64_SHF_EXECINSTR;
	sections[0].align = ctx->out_align[LD_OUT_TEXT];
	sections[0].data = ctx->text.data;
	sections[0].size = ctx->text.size;
	sections[0].vaddr = ctx->out_vaddr[LD_OUT_TEXT];
	sections[1].name = ".rodata";
	sections[1].type = ELF64_SHT_PROGBITS;
	sections[1].flags = ELF64_SHF_ALLOC;
	sections[1].align = ctx->out_align[LD_OUT_RODATA];
	sections[1].data = ctx->rodata.data;
	sections[1].size = ctx->rodata.size;
	sections[1].vaddr = ctx->out_vaddr[LD_OUT_RODATA];
	sections[2].name = ".data";
	sections[2].type = ELF64_SHT_PROGBITS;
	sections[2].flags = ELF64_SHF_ALLOC | ELF64_SHF_WRITE;
	sections[2].align = ctx->out_align[LD_OUT_DATA];
	sections[2].data = ctx->data.data;
	sections[2].size = ctx->data.size;
	sections[2].vaddr = ctx->out_vaddr[LD_OUT_DATA];
	sections[3].name = ".bss";
	sections[3].type = ELF64_SHT_NOBITS;
	sections[3].flags = ELF64_SHF_ALLOC | ELF64_SHF_WRITE;
	sections[3].align = ctx->out_align[LD_OUT_BSS];
	sections[3].size = ctx->bss_size;
	sections[3].vaddr = ctx->out_vaddr[LD_OUT_BSS];
	desc.entry = entry;
	desc.base_vaddr = ctx->base_vaddr;
	desc.sections = sections;
	desc.section_count = 4;
	return (elf64_write_executable(ctx->output, &desc));
}

static void
ld_usage(void)
{
	fprintf(stderr, "usage: ld [-o output] [-e symbol] [-Ttext addr] "
	    "[-native[=symbol|addr]] [-native-target symbol|addr] "
	    "file.o ...\n");
}

int
main(int argc, char **argv, char **envp)
{
	ld_context	ctx;
	char		*end;
	int		i;

	(void)envp;
	ld_init(&ctx);
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			ctx.output = argv[++i];
		} else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
			ctx.entry_name = argv[++i];
		} else if (strcmp(argv[i], "-native") == 0) {
			ld_set_native_target(&ctx, NULL);
			if (i + 1 < argc && argv[i + 1][0] != '-' &&
			    !ld_arg_looks_object(argv[i + 1])) {
				ld_set_native_target(&ctx, argv[++i]);
			}
		} else if (strncmp(argv[i], "-native=", 8) == 0) {
			ld_set_native_target(&ctx, argv[i] + 8);
		} else if (strcmp(argv[i], "-native-target") == 0 &&
		    i + 1 < argc) {
			ld_set_native_target(&ctx, argv[++i]);
		} else if (strcmp(argv[i], "-Ttext") == 0 && i + 1 < argc) {
			ctx.base_vaddr = strtoul(argv[++i], &end, 0);
			if (*end != '\0') {
				ld_usage();
				ld_free(&ctx);
				return (1);
			}
		} else if (argv[i][0] == '-') {
			ld_usage();
			ld_free(&ctx);
			return (1);
		} else if (ld_load_object(&ctx, argv[i]) != 0) {
			ld_free(&ctx);
			return (1);
		}
	}
	if (ctx.object_count == 0) {
		ld_usage();
		ld_free(&ctx);
		return (1);
	}
	if (ld_merge_sections(&ctx) == 0 && ctx.native_enabled) {
		ld_emit_native_trampoline(&ctx);
	}
	if (ctx.errors == 0) {
		ld_layout(&ctx);
	}
	if (ctx.errors == 0) {
		ld_collect_globals(&ctx);
	}
	if (ctx.errors == 0) {
		ld_patch_native_trampoline(&ctx);
	}
	if (ctx.errors == 0) {
		ld_apply_relocations(&ctx);
	}
	if (ctx.errors == 0 && ld_write_output(&ctx) != 0) {
		ld_error(&ctx, "cannot write %s", ctx.output);
	}
	ld_free(&ctx);
	return (ctx.errors == 0 ? 0 : 1);
}
