/* !DEFINES!

$define %type dld_object as loaded ELF object
$define %type dld_symbol as resolved symbol address and size
$define %func main as start with args int, char **, char **
$define %func dld_run as function with args int, char **, char **
$define %func load_library as function with args const char *
$define %func relocate_object as function with args dld_object *
$define %func jump_to_entry as procedure with args uintptr_t, uintptr_t

*/

/* !SPACE!

$space %internal align_down, align_up, elf_hash, gnu_hash
$space %internal find_auxv, setup_main_object, parse_dynamic
$space %internal load_library, load_needed, relocate_object
$space %internal lookup_symbol, call_initializers, jump_to_entry
$space %export main

*/

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

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "elf.h"

#define DLD_MAX_OBJECTS		32
#define DLD_MAX_NEEDED		64
#define DLD_MAX_PATH		256
#define DLD_PAGE_SIZE		4096UL

struct dld_symbol {
	struct dld_object	*object;
	const Elf64_Sym		*sym;
	uintptr_t		addr;
	size_t			size;
};

struct dld_object {
	char		path[DLD_MAX_PATH];
	uintptr_t	base;
	uintptr_t	map_start;
	size_t		map_size;
	uintptr_t	entry;
	Elf64_Phdr	*phdrs;
	size_t		phnum;
	int		phdr_owned;

	Elf64_Dyn	*dynamic;
	Elf64_Sym	*symtab;
	const char	*strtab;
	size_t		strsz;
	size_t		syment;

	uint32_t	*sysv_hash;
	uint32_t	*gnu_hash;
	Elf64_Rela	*rela;
	size_t		relasz;
	Elf64_Rela	*jmprel;
	size_t		jmprelsz;
	uint64_t	pltrel;

	void		(*init)(void);
	void		(**init_array)(int, char **, char **);
	size_t		init_arraysz;
	void		(**preinit_array)(int, char **, char **);
	size_t		preinit_arraysz;

	const char	*needed[DLD_MAX_NEEDED];
	size_t		needed_count;
	int		loading_deps;
	int		deps_loaded;
	int		relocated;
	int		initialized;
};

static struct dld_object	g_objects[DLD_MAX_OBJECTS];
static size_t			g_object_count;
static uintptr_t		g_entry;

static uintptr_t
align_down(uintptr_t value, uintptr_t align)
{
	return (value & ~(align - 1));
}

static uintptr_t
align_up(uintptr_t value, uintptr_t align)
{
	return ((value + align - 1) & ~(align - 1));
}

static int
has_slash(const char *s)
{
	while (*s) {
		if (*s == '/') {
			return (1);
		}
		s++;
	}
	return (0);
}

static const char *
base_name(const char *path)
{
	const char	*base;

	base = path;
	while (*path) {
		if (*path == '/') {
			base = path + 1;
		}
		path++;
	}
	return (base);
}

static int
copy_path(char *dst, size_t dstsz, const char *src)
{
	size_t	i;

	if (dstsz == 0) {
		return (-1);
	}
	for (i = 0; i + 1 < dstsz && src[i]; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
	return (src[i] == '\0' ? 0 : -1);
}

static int
make_lib_path(char *dst, size_t dstsz, const char *dir, const char *name)
{
	size_t	di, ni;

	if (dstsz == 0) {
		return (-1);
	}
	di = 0;
	while (dir[di] && di + 1 < dstsz) {
		dst[di] = dir[di];
		di++;
	}
	ni = 0;
	while (name[ni] && di + 1 < dstsz) {
		dst[di++] = name[ni++];
	}
	dst[di] = '\0';
	return (name[ni] == '\0' ? 0 : -1);
}

static int
path_exists(const char *path)
{
	struct api_fs_stat	st;

	return (fsStat(path, &st) == 0 && st.type == API_FS_TYPE_REG);
}

static int
find_library_path(const char *name, char *out, size_t outsz)
{
	if (has_slash(name)) {
		if (path_exists(name)) {
			return (copy_path(out, outsz, name));
		}
		return (-1);
	}
	if (make_lib_path(out, outsz, "/lib/", name) == 0 &&
	    path_exists(out)) {
		return (0);
	}
	if (make_lib_path(out, outsz, "/usr/lib/", name) == 0 &&
	    path_exists(out)) {
		return (0);
	}
	if (path_exists(name)) {
		return (copy_path(out, outsz, name));
	}
	return (-1);
}

static int
read_file(const char *path, void **out_buf, size_t *out_size)
{
	struct api_fs_stat	st;
	unsigned char		*buf;
	size_t			size;
	int			fd;

	if (fsStat(path, &st) != 0 || st.type != API_FS_TYPE_REG) {
		return (-1);
	}
	if (st.size == 0 || st.size > (64UL * 1024UL * 1024UL)) {
		return (-1);
	}
	size = (size_t)st.size;
	buf = malloc(size);
	if (buf == NULL) {
		return (-1);
	}
	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		free(buf);
		return (-1);
	}
	if (dataReadFull(fd, buf, size) != 0) {
		dataClose(fd);
		free(buf);
		return (-1);
	}
	dataClose(fd);
	*out_buf = buf;
	*out_size = size;
	return (0);
}

static int
validate_elf(const void *buf, size_t size)
{
	const Elf64_Ehdr	*eh;

	if (size < sizeof(*eh)) {
		return (-1);
	}
	eh = (const Elf64_Ehdr *)buf;
	if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
	    eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3) {
		return (-1);
	}
	if (eh->e_ident[4] != ELFCLASS64 || eh->e_ident[5] != ELFDATA2LSB) {
		return (-1);
	}
	if (eh->e_machine != EM_X86_64 ||
	    (eh->e_type != ET_DYN && eh->e_type != ET_EXEC)) {
		return (-1);
	}
	if (eh->e_phentsize != sizeof(Elf64_Phdr) || eh->e_phnum == 0) {
		return (-1);
	}
	if (eh->e_phoff + (uint64_t)eh->e_phnum * sizeof(Elf64_Phdr) > size) {
		return (-1);
	}
	return (0);
}

static uintptr_t
dyn_ptr(struct dld_object *obj, uint64_t value)
{
	if (value == 0) {
		return (0);
	}
	return (obj->base + (uintptr_t)value);
}

static uint32_t
elf_hash(const char *name)
{
	uint32_t	h, g;

	h = 0;
	while (*name) {
		h = (h << 4) + (unsigned char)*name++;
		g = h & 0xf0000000U;
		if (g != 0) {
			h ^= g >> 24;
		}
		h &= ~g;
	}
	return (h);
}

static uint32_t
gnu_hash(const char *name)
{
	uint32_t	h;

	h = 5381;
	while (*name) {
		h = h * 33 + (unsigned char)*name++;
	}
	return (h);
}

static Elf64_auxv_t *
find_auxv(char **envp)
{
	while (envp && *envp) {
		envp++;
	}
	if (envp == NULL) {
		return (NULL);
	}
	return ((Elf64_auxv_t *)(envp + 1));
}

static uint64_t
aux_value(Elf64_auxv_t *auxv, uint64_t type)
{
	size_t	i;

	for (i = 0; auxv && auxv[i].a_type != AT_NULL; i++) {
		if (auxv[i].a_type == type) {
			return (auxv[i].a_val);
		}
	}
	return (0);
}

static uintptr_t
compute_main_base(Elf64_Phdr *phdrs, size_t phnum, uintptr_t at_phdr)
{
	size_t	i;

	for (i = 0; i < phnum; i++) {
		if (phdrs[i].p_type == PT_PHDR) {
			return (at_phdr - (uintptr_t)phdrs[i].p_vaddr);
		}
	}
	for (i = 0; i < phnum; i++) {
		if (phdrs[i].p_type == PT_LOAD &&
		    phdrs[i].p_offset <= at_phdr) {
			return (at_phdr - (uintptr_t)phdrs[i].p_vaddr);
		}
	}
	return (0);
}

static int
parse_dynamic(struct dld_object *obj)
{
	Elf64_Dyn	*dyn;
	size_t		i;

	obj->dynamic = NULL;
	for (i = 0; i < obj->phnum; i++) {
		if (obj->phdrs[i].p_type == PT_DYNAMIC) {
			obj->dynamic =
			    (Elf64_Dyn *)(obj->base + obj->phdrs[i].p_vaddr);
			break;
		}
	}
	if (obj->dynamic == NULL) {
		return (0);
	}

	for (dyn = obj->dynamic; dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_STRTAB:
			obj->strtab = (const char *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_STRSZ:
			obj->strsz = (size_t)dyn->d_un.d_val;
			break;
		case DT_SYMTAB:
			obj->symtab = (Elf64_Sym *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_SYMENT:
			obj->syment = (size_t)dyn->d_un.d_val;
			break;
		case DT_HASH:
			obj->sysv_hash = (uint32_t *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_GNU_HASH:
			obj->gnu_hash = (uint32_t *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_RELA:
			obj->rela = (Elf64_Rela *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_RELASZ:
			obj->relasz = (size_t)dyn->d_un.d_val;
			break;
		case DT_JMPREL:
			obj->jmprel = (Elf64_Rela *)dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_PLTRELSZ:
			obj->jmprelsz = (size_t)dyn->d_un.d_val;
			break;
		case DT_PLTREL:
			obj->pltrel = dyn->d_un.d_val;
			break;
		case DT_INIT:
			obj->init = (void (*)(void))dyn_ptr(obj,
			    dyn->d_un.d_ptr);
			break;
		case DT_INIT_ARRAY:
			obj->init_array = (void (**)(int, char **, char **))
			    dyn_ptr(obj, dyn->d_un.d_ptr);
			break;
		case DT_INIT_ARRAYSZ:
			obj->init_arraysz = (size_t)dyn->d_un.d_val;
			break;
		case DT_PREINIT_ARRAY:
			obj->preinit_array = (void (**)(int, char **, char **))
			    dyn_ptr(obj, dyn->d_un.d_ptr);
			break;
		case DT_PREINIT_ARRAYSZ:
			obj->preinit_arraysz = (size_t)dyn->d_un.d_val;
			break;
		case DT_NEEDED:
			if (obj->needed_count < DLD_MAX_NEEDED) {
				obj->needed[obj->needed_count++] =
				    (const char *)(uintptr_t)dyn->d_un.d_val;
			} else {
				printf("drld: too many DT_NEEDED entries\n");
				return (-1);
			}
			break;
		case DT_REL:
		case DT_RELSZ:
		case DT_RELENT:
			printf("drld: REL relocations are not supported\n");
			return (-1);
		default:
			break;
		}
	}

	if (obj->strtab != NULL) {
		for (i = 0; i < obj->needed_count; i++) {
			obj->needed[i] = obj->strtab + (uintptr_t)obj->needed[i];
		}
	} else if (obj->needed_count != 0) {
		printf("drld: DT_NEEDED without DT_STRTAB in %s\n", obj->path);
		return (-1);
	}
	if (obj->syment == 0) {
		obj->syment = sizeof(Elf64_Sym);
	}
	return (0);
}

static struct dld_object *
find_object(const char *name)
{
	struct dld_object	*obj;
	size_t			i;

	for (i = 0; i < g_object_count; i++) {
		obj = &g_objects[i];
		if (strcmp(obj->path, name) == 0 ||
		    strcmp(base_name(obj->path), name) == 0) {
			return (obj);
		}
	}
	return (NULL);
}

static int
setup_main_object(Elf64_auxv_t *auxv)
{
	struct dld_object	*obj;
	uintptr_t		at_phdr;
	uint64_t		at_phent;
	uint64_t		at_phnum;

	at_phdr = (uintptr_t)aux_value(auxv, AT_PHDR);
	at_phent = aux_value(auxv, AT_PHENT);
	at_phnum = aux_value(auxv, AT_PHNUM);
	g_entry = (uintptr_t)aux_value(auxv, AT_ENTRY);

	if (at_phdr == 0 || at_phent != sizeof(Elf64_Phdr) ||
	    at_phnum == 0 || g_entry == 0) {
		return (-1);
	}
	obj = &g_objects[0];
	memset(obj, 0, sizeof(*obj));
	copy_path(obj->path, sizeof(obj->path), "<main>");
	obj->phdrs = (Elf64_Phdr *)at_phdr;
	obj->phnum = (size_t)at_phnum;
	obj->base = compute_main_base(obj->phdrs, obj->phnum, at_phdr);
	obj->entry = g_entry;
	g_object_count = 1;
	return (parse_dynamic(obj));
}

static int
map_one_segment(int fd, struct dld_object *obj, const Elf64_Phdr *ph)
{
	struct mem_map_args	args;
	uintptr_t		seg_page;
	uintptr_t		off_page;
	uintptr_t		delta;
	uintptr_t		addr;
	size_t			length;
	int			prot;
	void			*ret;

	seg_page = align_down((uintptr_t)ph->p_vaddr, DLD_PAGE_SIZE);
	off_page = align_down((uintptr_t)ph->p_offset, DLD_PAGE_SIZE);
	delta = (uintptr_t)ph->p_vaddr - seg_page;
	addr = obj->base + seg_page;
	length = (size_t)align_up(delta + ph->p_memsz, DLD_PAGE_SIZE);
	prot = API_MAP_READ | API_MAP_WRITE;
	if (ph->p_flags & PF_X) {
		prot |= API_MAP_EXEC;
	}

	memset(&args, 0, sizeof(args));
	args.addr = addr;
	args.length = length;
	args.prot = (uint32_t)prot;
	args.flags = API_MAP_PRIVATE | API_MAP_FIXED;
	args.fd = fd;
	args.offset = off_page;
	ret = memMap(&args);
	if (ret != (void *)addr) {
		return (-1);
	}
	if (ph->p_memsz > ph->p_filesz) {
		memset((void *)(obj->base + ph->p_vaddr + ph->p_filesz), 0,
		    (size_t)(ph->p_memsz - ph->p_filesz));
	}
	return (0);
}

static int
map_library(const char *path, const void *buf, size_t size,
    struct dld_object *obj)
{
	const Elf64_Ehdr	*eh;
	const Elf64_Phdr	*phdrs;
	struct mem_map_args	args;
	uintptr_t		min_vaddr;
	uintptr_t		max_vaddr;
	uintptr_t		start;
	uintptr_t		end;
	void			*reserve;
	int			fd;
	size_t			span;
	size_t			i;

	if (validate_elf(buf, size) != 0) {
		printf("drld: bad ELF image: %s\n", path);
		return (-1);
	}
	eh = (const Elf64_Ehdr *)buf;
	phdrs = (const Elf64_Phdr *)((const char *)buf + eh->e_phoff);

	min_vaddr = ~(uintptr_t)0;
	max_vaddr = 0;
	for (i = 0; i < eh->e_phnum; i++) {
		if (phdrs[i].p_type != PT_LOAD) {
			continue;
		}
		start = align_down((uintptr_t)phdrs[i].p_vaddr,
		    DLD_PAGE_SIZE);
		end = align_up((uintptr_t)phdrs[i].p_vaddr +
		    phdrs[i].p_memsz, DLD_PAGE_SIZE);
		if (start < min_vaddr) {
			min_vaddr = start;
		}
		if (end > max_vaddr) {
			max_vaddr = end;
		}
	}
	if (min_vaddr == ~(uintptr_t)0 || max_vaddr <= min_vaddr) {
		return (-1);
	}
	span = (size_t)(max_vaddr - min_vaddr);

	memset(&args, 0, sizeof(args));
	args.length = span;
	args.prot = API_MAP_READ | API_MAP_WRITE;
	args.flags = API_MAP_PRIVATE | API_MAP_ANON;
	args.fd = -1;
	reserve = memMap(&args);
	if (reserve == NULL) {
		return (-1);
	}
	memUnmap(reserve, span);

	memset(obj, 0, sizeof(*obj));
	copy_path(obj->path, sizeof(obj->path), path);
	obj->map_start = (uintptr_t)reserve;
	obj->map_size = span;
	obj->base = (uintptr_t)reserve - min_vaddr;
	obj->entry = obj->base + eh->e_entry;
	obj->phnum = eh->e_phnum;
	obj->phdrs = malloc(sizeof(Elf64_Phdr) * obj->phnum);
	if (obj->phdrs == NULL) {
		return (-1);
	}
	memcpy(obj->phdrs, phdrs, sizeof(Elf64_Phdr) * obj->phnum);
	obj->phdr_owned = 1;

	fd = dataOpen(path, API_OPEN_READ);
	if (fd < 0) {
		return (-1);
	}
	for (i = 0; i < obj->phnum; i++) {
		if (obj->phdrs[i].p_type == PT_LOAD &&
		    map_one_segment(fd, obj, &obj->phdrs[i]) != 0) {
			dataClose(fd);
			return (-1);
		}
	}
	dataClose(fd);
	return (parse_dynamic(obj));
}

static struct dld_object *
load_library(const char *name)
{
	struct dld_object	*obj;
	char			path[DLD_MAX_PATH];
	void			*buf;
	size_t			size;

	obj = find_object(name);
	if (obj != NULL) {
		return (obj);
	}
	if (g_object_count >= DLD_MAX_OBJECTS) {
		printf("drld: too many shared objects\n");
		return (NULL);
	}
	if (find_library_path(name, path, sizeof(path)) != 0) {
		printf("drld: cannot find %s\n", name);
		return (NULL);
	}
	obj = find_object(path);
	if (obj != NULL) {
		return (obj);
	}
	if (read_file(path, &buf, &size) != 0) {
		printf("drld: cannot read %s\n", path);
		return (NULL);
	}
	obj = &g_objects[g_object_count++];
	if (map_library(path, buf, size, obj) != 0) {
		printf("drld: cannot map %s\n", path);
		free(buf);
		g_object_count--;
		return (NULL);
	}
	free(buf);
	return (obj);
}

static int
load_needed(struct dld_object *obj)
{
	struct dld_object	*dep;
	size_t			i;

	if (obj->deps_loaded) {
		return (0);
	}
	if (obj->loading_deps) {
		return (0);
	}
	obj->loading_deps = 1;
	for (i = 0; i < obj->needed_count; i++) {
		dep = load_library(obj->needed[i]);
		if (dep == NULL) {
			obj->loading_deps = 0;
			return (-1);
		}
		if (load_needed(dep) != 0) {
			obj->loading_deps = 0;
			return (-1);
		}
	}
	obj->loading_deps = 0;
	obj->deps_loaded = 1;
	return (0);
}

static int
symbol_usable(const Elf64_Sym *sym)
{
	unsigned int	type;

	if (sym->st_shndx == SHN_UNDEF) {
		return (0);
	}
	type = ELF64_ST_TYPE(sym->st_info);
	if (type == STT_FILE || type == STT_SECTION || type == STT_TLS ||
	    type == STT_GNU_IFUNC) {
		return (0);
	}
	return (1);
}

static int
symbol_addr(struct dld_object *obj, const Elf64_Sym *sym,
    struct dld_symbol *out)
{
	if (!symbol_usable(sym)) {
		return (-1);
	}
	out->object = obj;
	out->sym = sym;
	out->size = (size_t)sym->st_size;
	if (sym->st_shndx == SHN_ABS) {
		out->addr = (uintptr_t)sym->st_value;
	} else {
		out->addr = obj->base + (uintptr_t)sym->st_value;
	}
	return (0);
}

static int
lookup_sysv(struct dld_object *obj, const char *name,
    struct dld_symbol *out)
{
	const Elf64_Sym	*sym;
	uint32_t	*bucket;
	uint32_t	*chain;
	uint32_t	nbucket;
	uint32_t	idx;

	if (obj->sysv_hash == NULL || obj->symtab == NULL ||
	    obj->strtab == NULL) {
		return (-1);
	}
	nbucket = obj->sysv_hash[0];
	if (nbucket == 0) {
		return (-1);
	}
	bucket = obj->sysv_hash + 2;
	chain = bucket + nbucket;
	idx = bucket[elf_hash(name) % nbucket];
	while (idx != 0) {
		sym = &obj->symtab[idx];
		if (symbol_usable(sym) &&
		    strcmp(obj->strtab + sym->st_name, name) == 0) {
			return (symbol_addr(obj, sym, out));
		}
		idx = chain[idx];
	}
	return (-1);
}

static int
lookup_gnu(struct dld_object *obj, const char *name,
    struct dld_symbol *out)
{
	const Elf64_Sym	*sym;
	uint64_t	*bloom;
	uint32_t	*buckets;
	uint32_t	*chains;
	uint32_t	nbuckets;
	uint32_t	symoffset;
	uint32_t	bloom_size;
	uint32_t	bloom_shift;
	uint32_t	hash;
	uint32_t	idx;
	uint64_t	word;
	uint64_t	mask;

	if (obj->gnu_hash == NULL || obj->symtab == NULL ||
	    obj->strtab == NULL) {
		return (-1);
	}
	nbuckets = obj->gnu_hash[0];
	symoffset = obj->gnu_hash[1];
	bloom_size = obj->gnu_hash[2];
	bloom_shift = obj->gnu_hash[3];
	if (nbuckets == 0 || bloom_size == 0) {
		return (-1);
	}
	bloom = (uint64_t *)(obj->gnu_hash + 4);
	buckets = (uint32_t *)(bloom + bloom_size);
	chains = buckets + nbuckets;

	hash = gnu_hash(name);
	word = bloom[(hash / 64) % bloom_size];
	mask = (1UL << (hash % 64)) |
	    (1UL << ((hash >> bloom_shift) % 64));
	if ((word & mask) != mask) {
		return (-1);
	}
	idx = buckets[hash % nbuckets];
	if (idx < symoffset) {
		return (-1);
	}
	for (;;) {
		uint32_t	h2;

		h2 = chains[idx - symoffset];
		if (((h2 ^ hash) >> 1) == 0) {
			sym = &obj->symtab[idx];
			if (symbol_usable(sym) &&
			    strcmp(obj->strtab + sym->st_name, name) == 0) {
				return (symbol_addr(obj, sym, out));
			}
		}
		if (h2 & 1) {
			break;
		}
		idx++;
	}
	return (-1);
}

static int
lookup_in_object(struct dld_object *obj, const char *name,
    struct dld_symbol *out)
{
	if (lookup_gnu(obj, name, out) == 0) {
		return (0);
	}
	return (lookup_sysv(obj, name, out));
}

static int
lookup_symbol(const char *name, struct dld_object *skip,
    struct dld_symbol *out)
{
	size_t	i;

	for (i = 0; i < g_object_count; i++) {
		if (&g_objects[i] == skip) {
			continue;
		}
		if (lookup_in_object(&g_objects[i], name, out) == 0) {
			return (0);
		}
	}
	return (-1);
}

static int
resolve_reloc_symbol(struct dld_object *obj, uint32_t sym_index,
    uintptr_t addend, uintptr_t place, struct dld_symbol *out,
    uintptr_t *value)
{
	const Elf64_Sym	*sym;
	const char	*name;
	unsigned int	bind;

	memset(out, 0, sizeof(*out));
	*value = addend;
	if (sym_index == 0) {
		return (0);
	}
	if (obj->symtab == NULL || obj->strtab == NULL) {
		return (-1);
	}
	sym = &obj->symtab[sym_index];
	bind = ELF64_ST_BIND(sym->st_info);
	if (bind == STB_LOCAL && symbol_addr(obj, sym, out) == 0) {
		*value = out->addr + addend;
		return (0);
	}
	name = obj->strtab + sym->st_name;
	if (lookup_symbol(name, NULL, out) != 0) {
		if (bind == STB_WEAK) {
			*value = addend;
			return (0);
		}
		printf("drld: unresolved symbol %s in %s\n", name, obj->path);
		(void)place;
		return (-1);
	}
	*value = out->addr + addend;
	return (0);
}

static int
apply_rela(struct dld_object *obj, const Elf64_Rela *rela)
{
	struct dld_symbol	sym;
	uintptr_t		*where64;
	uintptr_t		value;
	uintptr_t		place;
	uint32_t		type;
	uint32_t		sym_index;

	type = ELF64_R_TYPE(rela->r_info);
	sym_index = (uint32_t)ELF64_R_SYM(rela->r_info);
	place = obj->base + (uintptr_t)rela->r_offset;
	where64 = (uintptr_t *)place;

	switch (type) {
	case R_X86_64_NONE:
		return (0);
	case R_X86_64_RELATIVE:
		*where64 = obj->base + (uintptr_t)rela->r_addend;
		return (0);
	case R_X86_64_IRELATIVE:
	{
		uintptr_t	(*resolver)(void);

		resolver = (uintptr_t (*)(void))(obj->base +
		    (uintptr_t)rela->r_addend);
		*where64 = resolver();
		return (0);
	}
	case R_X86_64_64:
	case R_X86_64_GLOB_DAT:
	case R_X86_64_JUMP_SLOT:
		if (resolve_reloc_symbol(obj, sym_index,
		    (uintptr_t)rela->r_addend, place, &sym, &value) != 0) {
			return (-1);
		}
		*where64 = value;
		return (0);
	case R_X86_64_COPY:
		if (obj != &g_objects[0]) {
			printf("drld: COPY relocation outside main object\n");
			return (-1);
		}
		if (resolve_reloc_symbol(obj, sym_index, 0, place, &sym,
		    &value) != 0) {
			return (-1);
		}
		if (sym.object == obj &&
		    lookup_symbol(obj->strtab + obj->symtab[sym_index].st_name,
		    obj, &sym) != 0) {
			return (-1);
		}
		memcpy((void *)place, (const void *)sym.addr, sym.size);
		return (0);
	case R_X86_64_PC32:
		if (resolve_reloc_symbol(obj, sym_index,
		    (uintptr_t)rela->r_addend, place, &sym, &value) != 0) {
			return (-1);
		}
		*(int32_t *)place = (int32_t)(value - place);
		return (0);
	case R_X86_64_32:
		if (resolve_reloc_symbol(obj, sym_index,
		    (uintptr_t)rela->r_addend, place, &sym, &value) != 0) {
			return (-1);
		}
		*(uint32_t *)place = (uint32_t)value;
		return (0);
	case R_X86_64_32S:
		if (resolve_reloc_symbol(obj, sym_index,
		    (uintptr_t)rela->r_addend, place, &sym, &value) != 0) {
			return (-1);
		}
		*(int32_t *)place = (int32_t)value;
		return (0);
	default:
		printf("drld: unsupported relocation %u in %s\n",
		    type, obj->path);
		return (-1);
	}
}

static int
relocate_range(struct dld_object *obj, Elf64_Rela *rela, size_t size)
{
	size_t	i;
	size_t	count;

	if (rela == NULL || size == 0) {
		return (0);
	}
	count = size / sizeof(Elf64_Rela);
	for (i = 0; i < count; i++) {
		if (apply_rela(obj, &rela[i]) != 0) {
			return (-1);
		}
	}
	return (0);
}

static int
relocate_object(struct dld_object *obj)
{
	if (obj->relocated) {
		return (0);
	}
	if (relocate_range(obj, obj->rela, obj->relasz) != 0) {
		return (-1);
	}
	if (obj->pltrel != DT_RELA && obj->jmprelsz != 0) {
		printf("drld: PLTREL is not RELA in %s\n", obj->path);
		return (-1);
	}
	if (relocate_range(obj, obj->jmprel, obj->jmprelsz) != 0) {
		return (-1);
	}
	obj->relocated = 1;
	return (0);
}

static int
relocate_all(void)
{
	size_t	i;

	for (i = 0; i < g_object_count; i++) {
		if (relocate_object(&g_objects[i]) != 0) {
			return (-1);
		}
	}
	return (0);
}

static void
call_object_init(struct dld_object *obj, int argc, char **argv, char **envp)
{
	size_t	i;
	size_t	count;

	if (obj->initialized) {
		return;
	}
	obj->initialized = 1;
	if (obj == &g_objects[0] && obj->preinit_array != NULL) {
		count = obj->preinit_arraysz / sizeof(obj->preinit_array[0]);
		for (i = 0; i < count; i++) {
			if (obj->preinit_array[i] != NULL) {
				obj->preinit_array[i](argc, argv, envp);
			}
		}
	}
	if (obj->init != NULL) {
		obj->init();
	}
	if (obj->init_array != NULL) {
		count = obj->init_arraysz / sizeof(obj->init_array[0]);
		for (i = 0; i < count; i++) {
			if (obj->init_array[i] != NULL) {
				obj->init_array[i](argc, argv, envp);
			}
		}
	}
}

static void
call_initializers(int argc, char **argv, char **envp)
{
	size_t	i;

	i = g_object_count;
	while (i > 1) {
		i--;
		call_object_init(&g_objects[i], argc, argv, envp);
	}
	call_object_init(&g_objects[0], argc, argv, envp);
}

static void
jump_to_entry(uintptr_t entry, uintptr_t sp, long argc, char **argv,
    char **envp)
{
	__asm__ volatile(
	    "mov %0, %%rsp\n"
	    "xor %%rbp, %%rbp\n"
	    "jmp *%1\n"
	    :
	    : "r"(sp), "r"(entry), "D"(argc), "S"(argv), "d"(envp)
	    : "memory");
	__builtin_unreachable();
}

static int
dld_run(int argc, char **argv, char **envp)
{
	Elf64_auxv_t	*auxv;
	uintptr_t	sp;

	personality(API_PERSONALITY_NATIVE);
	auxv = find_auxv(envp);
	if (setup_main_object(auxv) != 0) {
		printf("drld: missing ELF auxv, run as PT_INTERP\n");
		return (1);
	}
	if (load_needed(&g_objects[0]) != 0) {
		return (1);
	}
	if (relocate_all() != 0) {
		return (1);
	}
	call_initializers(argc, argv, envp);
	sp = (uintptr_t)(argv - 1);
	jump_to_entry(g_entry, sp, argc, argv, envp);
	return (1);
}

int
main(int argc, char **argv, char **envp)
{
	return (dld_run(argc, argv, envp));
}
