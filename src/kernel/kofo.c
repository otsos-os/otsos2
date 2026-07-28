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

$define %type kofo_module_t as loaded KOFO module state
$define %type kofo_load_ctx_t as transient KOFO load context
$define %type kofo_entry_fn as module entry function
$define %func api_kofo_load as function with args const char *, u32
$define %func api_kofo_info as function with args u32, api_kofo_info *
$define %func api_kofo_unload as function with args u32, u32

*/

/* !SPACE!

$space %internal kofo_copy_user_path, kofo_read_file
$space %internal kofo_range_ok, kofo_str, kofo_name_copy
$space %internal kofo_table_after_header, kofo_validate_header
$space %internal kofo_align_ok, kofo_align, kofo_layout_sections
$space %internal kofo_resolve_symbol, kofo_apply_relocs
$space %internal kofo_kernel_symbol_lookup, kofo_patch_reloc
$space %internal kofo_find_symbol, kofo_call_entry, kofo_free_ctx
$space %internal kofo_find_slot_by_id, kofo_alloc_slot, kofo_name_loaded
$space %export api_kofo_load, api_kofo_info, api_kofo_unload

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/drivers/fs/vfs/vfs.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/kofo.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>
#include <mm/kmem.h>

#define	KOFO_MAX_MODULES	32
#define	KOFO_MAX_FILE_SIZE	(2 * 1024 * 1024)
#define	KOFO_STATE_EMPTY	API_KOFO_STATE_EMPTY
#define	KOFO_STATE_LOADING	API_KOFO_STATE_LOADING
#define	KOFO_STATE_LOADED	API_KOFO_STATE_LOADED
#define	KOFO_STATE_UNLOADING	API_KOFO_STATE_UNLOADING

typedef int	(*kofo_entry_fn)(void);

typedef struct kofo_module {
	int		used;
	u32		id;
	u32		flags;
	u32		state;
	u32		section_count;
	u32		symbol_count;
	u32		import_count;
	u32		reloc_count;
	u32		driver_count;
	void		*image;
	u64		image_size;
	kofo_entry_fn	exit_fn;
	char		name[API_KOFO_NAME_MAX];
	char		version[API_KOFO_VERSION_MAX];
	char		path[API_KOFO_PATH_MAX];
} kofo_module_t;

typedef struct kofo_load_ctx {
	const kofo_header_t	*hdr;
	const kofo_section_t	*sections;
	const kofo_symbol_t	*symbols;
	const kofo_import_t	*imports;
	const kofo_reloc_t	*relocs;
	const kofo_driver_t	*drivers;
	const char		*strings;
	u8			*file;
	u8			*image;
	u64			*section_base;
	u32			file_size;
	u64			image_size;
} kofo_load_ctx_t;

static kofo_module_t	kofo_modules[KOFO_MAX_MODULES];
static u32		kofo_next_id = 1;

static char *
kofo_copy_user_path(const char *path)
{
	char	*buf;
	int	len;

	if (path == NULL || !is_user_address(path, 1)) {
		return (NULL);
	}
	len = 0;
	while (len < 255) {
		if (!is_user_address(path + len, 1)) {
			return (NULL);
		}
		if (path[len] == '\0') {
			break;
		}
		len++;
	}
	if (len == 255 || path[len] != '\0') {
		return (NULL);
	}
	buf = (char *)kmem_calloc((size_t)len + 1, 1);
	if (buf == NULL) {
		return (NULL);
	}
	memcpy(buf, path, (unsigned long)len);
	buf[len] = '\0';
	return (buf);
}

static int
kofo_read_file(const char *path, u8 **out, u32 *out_size)
{
	posix_stat_t	st;
	vnode_t		*vn;
	u8		*buf;
	u32		size;
	int		n, ret;

	ret = vfs_resolve(path, &vn);
	if (ret != 0 || vn == NULL) {
		return (ret != 0 ? ret : -API_ERR_NOT_FOUND);
	}
	if (!vnode_can_exec(vn)) {
		vnode_release(vn);
		return (-API_ERR_ACCESS);
	}
	if (vn->type == VDIR) {
		vnode_release(vn);
		return (-API_ERR_IS_DIR);
	}
	ret = vnode_stat(vn, &st);
	if (ret != 0) {
		vnode_release(vn);
		return (ret);
	}
	if (st.st_size <= 0 || st.st_size > KOFO_MAX_FILE_SIZE) {
		vnode_release(vn);
		return (-API_ERR_BAD_IMAGE);
	}
	size = (u32)st.st_size;
	buf = (u8 *)kmem_alloc(size);
	if (buf == NULL) {
		vnode_release(vn);
		return (-API_ERR_NO_MEMORY);
	}
	n = vnode_read(vn, buf, size, 0);
	vnode_release(vn);
	if (n < 0 || (u32)n != size) {
		kmem_free(buf);
		return (-API_ERR_IO);
	}
	*out = buf;
	*out_size = size;
	return (0);
}

static int
kofo_range_ok(u32 file_size, u32 off, u32 count, u32 elem_size)
{
	u64	end;

	if (count == 0) {
		return (off <= file_size);
	}
	if (off == KOFO_STR_NONE || elem_size == 0) {
		return (0);
	}
	end = (u64)off + ((u64)count * (u64)elem_size);
	if (end < off || end > file_size) {
		return (0);
	}
	return (1);
}

static int
kofo_table_after_header(const kofo_header_t *hdr, u32 off, u32 count)
{
	if (count == 0) {
		return (1);
	}
	return (off >= hdr->header_size);
}

static const char *
kofo_str(const kofo_load_ctx_t *ctx, u32 off)
{
	u32	i;

	if (off == KOFO_STR_NONE || off >= ctx->hdr->string_size) {
		return (NULL);
	}
	for (i = off; i < ctx->hdr->string_size; i++) {
		if (ctx->strings[i] == '\0') {
			return (ctx->strings + off);
		}
	}
	return (NULL);
}

static void
kofo_name_copy(char *dst, u32 dst_size, const char *src)
{
	u32	i;

	if (dst == NULL || dst_size == 0) {
		return;
	}
	dst[0] = '\0';
	if (src == NULL) {
		return;
	}
	for (i = 0; i + 1 < dst_size && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
	dst[i] = '\0';
}

static int
kofo_validate_header(kofo_load_ctx_t *ctx)
{
	const kofo_header_t	*hdr;

	if (ctx->file_size < sizeof(kofo_header_t)) {
		return (-API_ERR_BAD_IMAGE);
	}
	hdr = (const kofo_header_t *)ctx->file;
	if (hdr->magic != KOFO_MAGIC || hdr->version != KOFO_VERSION ||
	    hdr->header_size < sizeof(kofo_header_t) ||
	    hdr->header_size > ctx->file_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->arch != KOFO_ARCH_X86_64 || hdr->abi != KOFO_ABI_KERNEL) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (hdr->file_size != ctx->file_size) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!kofo_range_ok(ctx->file_size, hdr->string_off,
	    hdr->string_size, 1) ||
	    !kofo_range_ok(ctx->file_size, hdr->section_off,
	    hdr->section_count, sizeof(kofo_section_t)) ||
	    !kofo_range_ok(ctx->file_size, hdr->symbol_off,
	    hdr->symbol_count, sizeof(kofo_symbol_t)) ||
	    !kofo_range_ok(ctx->file_size, hdr->import_off,
	    hdr->import_count, sizeof(kofo_import_t)) ||
	    !kofo_range_ok(ctx->file_size, hdr->reloc_off,
	    hdr->reloc_count, sizeof(kofo_reloc_t)) ||
	    !kofo_range_ok(ctx->file_size, hdr->driver_off,
	    hdr->driver_count, sizeof(kofo_driver_t))) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (!kofo_table_after_header(hdr, hdr->string_off,
	    hdr->string_size) ||
	    !kofo_table_after_header(hdr, hdr->section_off,
	    hdr->section_count) ||
	    !kofo_table_after_header(hdr, hdr->symbol_off,
	    hdr->symbol_count) ||
	    !kofo_table_after_header(hdr, hdr->import_off,
	    hdr->import_count) ||
	    !kofo_table_after_header(hdr, hdr->reloc_off,
	    hdr->reloc_count) ||
	    !kofo_table_after_header(hdr, hdr->driver_off,
	    hdr->driver_count)) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (hdr->section_count == 0 || hdr->symbol_count == 0 ||
	    hdr->string_size == 0) {
		return (-API_ERR_BAD_IMAGE);
	}
	ctx->hdr = hdr;
	ctx->strings = (const char *)(ctx->file + hdr->string_off);
	ctx->sections = (const kofo_section_t *)(ctx->file +
	    hdr->section_off);
	ctx->symbols = (const kofo_symbol_t *)(ctx->file + hdr->symbol_off);
	ctx->imports = (const kofo_import_t *)(ctx->file + hdr->import_off);
	ctx->relocs = (const kofo_reloc_t *)(ctx->file + hdr->reloc_off);
	ctx->drivers = (const kofo_driver_t *)(ctx->file + hdr->driver_off);
	if (kofo_str(ctx, hdr->name) == NULL) {
		return (-API_ERR_BAD_IMAGE);
	}
	return (0);
}

static int
kofo_align_ok(u32 align)
{
	if (align <= 1) {
		return (1);
	}
	if ((align & (align - 1)) != 0 || align > 4096) {
		return (0);
	}
	return (1);
}

static u64
kofo_align(u64 value, u32 align)
{
	u64	mask;

	if (align <= 1) {
		return (value);
	}
	mask = (u64)align - 1;
	return ((value + mask) & ~mask);
}

static int
kofo_layout_sections(kofo_load_ctx_t *ctx)
{
	const kofo_section_t	*sec;
	u64			pos;
	u32			i;

	ctx->section_base = (u64 *)kmem_calloc(ctx->hdr->section_count,
	    sizeof(u64));
	if (ctx->section_base == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	pos = 0;
	for (i = 0; i < ctx->hdr->section_count; i++) {
		sec = &ctx->sections[i];
		if (kofo_str(ctx, sec->name) == NULL) {
			return (-API_ERR_BAD_IMAGE);
		}
		if ((sec->flags & KOFO_SEC_F_ALLOC) == 0) {
			ctx->section_base[i] = 0;
			continue;
		}
		if (!kofo_align_ok(sec->align)) {
			return (-API_ERR_BAD_IMAGE);
		}
		pos = kofo_align(pos, sec->align == 0 ? 1 : sec->align);
		if (sec->size > KOFO_MAX_FILE_SIZE ||
		    pos + sec->size < pos ||
		    pos + sec->size > KOFO_MAX_FILE_SIZE) {
			return (-API_ERR_BAD_IMAGE);
		}
		if ((sec->flags & KOFO_SEC_F_BSS) == 0 && sec->size != 0 &&
		    (sec->data_off < ctx->hdr->header_size ||
		    !kofo_range_ok(ctx->file_size, sec->data_off,
		    (u32)sec->size, 1))) {
			return (-API_ERR_BAD_IMAGE);
		}
		ctx->section_base[i] = pos;
		pos += sec->size;
	}
	ctx->image_size = kofo_align(pos, 16);
	if (ctx->image_size == 0 || ctx->image_size > KOFO_MAX_FILE_SIZE) {
		return (-API_ERR_BAD_IMAGE);
	}
	ctx->image = (u8 *)kmem_alloc_aligned((size_t)ctx->image_size, 16);
	if (ctx->image == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	memset(ctx->image, 0, (unsigned long)ctx->image_size);
	for (i = 0; i < ctx->hdr->section_count; i++) {
		sec = &ctx->sections[i];
		if ((sec->flags & KOFO_SEC_F_ALLOC) == 0 ||
		    (sec->flags & KOFO_SEC_F_BSS) != 0 ||
		    sec->size == 0) {
			continue;
		}
		memcpy(ctx->image + ctx->section_base[i],
		    ctx->file + sec->data_off, (unsigned long)sec->size);
	}
	return (0);
}

static int
kofo_kernel_symbol_lookup(const char *name, u64 *out)
{
	u32	i;

	if (name == NULL || out == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < kofo_kernel_symbol_count; i++) {
		if (kofo_kernel_symbols[i].name == NULL) {
			continue;
		}
		if (strcmp(kofo_kernel_symbols[i].name, name) == 0) {
			*out = (u64)kofo_kernel_symbols[i].value;
			return (0);
		}
	}
	return (-API_ERR_NOT_FOUND);
}

static int
kofo_resolve_symbol(kofo_load_ctx_t *ctx, u32 index, u64 *out)
{
	const kofo_symbol_t	*sym;
	const kofo_section_t	*sec;
	const char		*name;

	if (index >= ctx->hdr->symbol_count) {
		return (-API_ERR_BAD_IMAGE);
	}
	sym = &ctx->symbols[index];
	name = kofo_str(ctx, sym->name);
	if (name == NULL) {
		return (-API_ERR_BAD_IMAGE);
	}
	if (sym->section == KOFO_SYM_UNDEF ||
	    (sym->flags & KOFO_SYM_F_IMPORT) != 0) {
		return (kofo_kernel_symbol_lookup(name, out));
	}
	if (sym->section >= ctx->hdr->section_count) {
		return (-API_ERR_BAD_IMAGE);
	}
	sec = &ctx->sections[sym->section];
	if ((sec->flags & KOFO_SEC_F_ALLOC) == 0 ||
	    sym->value > sec->size || sym->size > sec->size ||
	    sym->value + sym->size < sym->value ||
	    sym->value + sym->size > sec->size) {
		return (-API_ERR_BAD_IMAGE);
	}
	*out = (u64)(ctx->image + ctx->section_base[sym->section] +
	    sym->value);
	return (0);
}

static int
kofo_patch_reloc(void *where, u16 type, u64 symbol, s64 addend)
{
	s64	value;
	u64	place;

	place = (u64)where;
	switch (type) {
	case KOFO_RELOC_ABS64:
		*(u64 *)where = symbol + (u64)addend;
		return (0);
	case KOFO_RELOC_ABS32:
		value = (s64)(symbol + (u64)addend);
		if (value < 0 || value > 0xFFFFFFFFLL) {
			return (-API_ERR_BAD_IMAGE);
		}
		*(u32 *)where = (u32)value;
		return (0);
	case KOFO_RELOC_REL32:
		value = (s64)(symbol + (u64)addend) - (s64)(place + 4);
		if (value < -2147483648LL || value > 2147483647LL) {
			return (-API_ERR_BAD_IMAGE);
		}
		*(u32 *)where = (u32)(s32)value;
		return (0);
	default:
		return (-API_ERR_NOT_SUPPORTED);
	}
}

static int
kofo_apply_relocs(kofo_load_ctx_t *ctx)
{
	const kofo_reloc_t	*rel;
	const kofo_section_t	*sec;
	void			*where;
	u64			symbol;
	u32			i, width;
	int			ret;

	for (i = 0; i < ctx->hdr->reloc_count; i++) {
		rel = &ctx->relocs[i];
		if (rel->section >= ctx->hdr->section_count) {
			return (-API_ERR_BAD_IMAGE);
		}
		sec = &ctx->sections[rel->section];
		width = (rel->type == KOFO_RELOC_ABS64) ? 8 : 4;
		if ((sec->flags & KOFO_SEC_F_ALLOC) == 0 ||
		    rel->offset > sec->size ||
		    rel->offset + width < rel->offset ||
		    rel->offset + width > sec->size) {
			return (-API_ERR_BAD_IMAGE);
		}
		ret = kofo_resolve_symbol(ctx, rel->symbol, &symbol);
		if (ret != 0) {
			return (ret);
		}
		where = ctx->image + ctx->section_base[rel->section] +
		    rel->offset;
		ret = kofo_patch_reloc(where, rel->type, symbol, rel->addend);
		if (ret != 0) {
			return (ret);
		}
	}
	return (0);
}

static int
kofo_find_symbol(kofo_load_ctx_t *ctx, const char *name, u64 *out)
{
	const char	*sym_name;
	int		ret;
	u32		i;

	if (name == NULL || out == NULL) {
		return (-API_ERR_BAD_VALUE);
	}
	for (i = 0; i < ctx->hdr->symbol_count; i++) {
		sym_name = kofo_str(ctx, ctx->symbols[i].name);
		if (sym_name == NULL) {
			return (-API_ERR_BAD_IMAGE);
		}
		if (strcmp(sym_name, name) == 0) {
			ret = kofo_resolve_symbol(ctx, i, out);
			if (ret == -API_ERR_NOT_FOUND &&
			    (ctx->symbols[i].section == KOFO_SYM_UNDEF ||
			    (ctx->symbols[i].flags & KOFO_SYM_F_IMPORT) != 0)) {
				continue;
			}
			return (ret);
		}
	}
	return (-API_ERR_NOT_FOUND);
}

static int
kofo_call_entry(kofo_entry_fn entry)
{
	if (entry == NULL) {
		return (0);
	}
	return (entry());
}

static void
kofo_free_ctx(kofo_load_ctx_t *ctx)
{
	if (ctx->image != NULL) {
		kmem_free(ctx->image);
		ctx->image = NULL;
	}
	if (ctx->section_base != NULL) {
		kmem_free(ctx->section_base);
		ctx->section_base = NULL;
	}
	if (ctx->file != NULL) {
		kmem_free(ctx->file);
		ctx->file = NULL;
	}
}

static kofo_module_t *
kofo_find_slot_by_id(u32 id)
{
	u32	i;

	for (i = 0; i < KOFO_MAX_MODULES; i++) {
		if (kofo_modules[i].used && kofo_modules[i].id == id) {
			return (&kofo_modules[i]);
		}
	}
	return (NULL);
}

static kofo_module_t *
kofo_alloc_slot(void)
{
	u32	i;

	for (i = 0; i < KOFO_MAX_MODULES; i++) {
		if (!kofo_modules[i].used) {
			memset(&kofo_modules[i], 0, sizeof(kofo_modules[i]));
			kofo_modules[i].used = 1;
			kofo_modules[i].id = kofo_next_id++;
			if (kofo_next_id == 0) {
				kofo_next_id = 1;
			}
			return (&kofo_modules[i]);
		}
	}
	return (NULL);
}

static int
kofo_name_loaded(const char *name)
{
	u32	i;

	for (i = 0; i < KOFO_MAX_MODULES; i++) {
		if (kofo_modules[i].used &&
		    strcmp(kofo_modules[i].name, name) == 0) {
			return (1);
		}
	}
	return (0);
}

int
api_kofo_load(const char *path, u32 flags)
{
	kofo_load_ctx_t	ctx;
	kofo_module_t	*mod;
	const char	*name, *version, *init_name, *exit_name;
	char		*kpath;
	u64		init_addr, exit_addr;
	int		ret;

	if (flags != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (!proc_has_privilege(process_current())) {
		return (-API_ERR_PERM);
	}
	memset(&ctx, 0, sizeof(ctx));
	kpath = kofo_copy_user_path(path);
	if (kpath == NULL || kpath[0] == '\0') {
		if (kpath != NULL) {
			kmem_free(kpath);
		}
		return (-API_ERR_BAD_ADDR);
	}
	ret = kofo_read_file(kpath, &ctx.file, &ctx.file_size);
	if (ret != 0) {
		kmem_free(kpath);
		return (ret);
	}
	ret = kofo_validate_header(&ctx);
	if (ret != 0) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (ret);
	}
	name = kofo_str(&ctx, ctx.hdr->name);
	version = kofo_str(&ctx, ctx.hdr->version_name);
	init_name = kofo_str(&ctx, ctx.hdr->init_name);
	exit_name = kofo_str(&ctx, ctx.hdr->exit_name);
	if (name == NULL || name[0] == '\0' ||
	    (ctx.hdr->version_name != KOFO_STR_NONE && version == NULL) ||
	    (ctx.hdr->init_name != KOFO_STR_NONE && init_name == NULL) ||
	    (ctx.hdr->exit_name != KOFO_STR_NONE && exit_name == NULL)) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (-API_ERR_BAD_IMAGE);
	}
	if (init_name == NULL) {
		init_name = "kofo_module_init";
	}
	if (exit_name == NULL) {
		exit_name = "kofo_module_exit";
	}
	if (kofo_name_loaded(name)) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (-API_ERR_EXISTS);
	}
	ret = kofo_layout_sections(&ctx);
	if (ret == 0) {
		ret = kofo_apply_relocs(&ctx);
	}
	if (ret != 0) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (ret);
	}
	init_addr = 0;
	ret = kofo_find_symbol(&ctx, init_name, &init_addr);
	if (ret == -API_ERR_NOT_FOUND) {
		init_addr = 0;
		ret = 0;
	}
	if (ret != 0) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (ret);
	}
	mod = kofo_alloc_slot();
	if (mod == NULL) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		return (-API_ERR_NO_MEMORY);
	}
	mod->state = KOFO_STATE_LOADING;
	mod->flags = ctx.hdr->flags;
	mod->image = ctx.image;
	mod->image_size = ctx.image_size;
	mod->section_count = ctx.hdr->section_count;
	mod->symbol_count = ctx.hdr->symbol_count;
	mod->import_count = ctx.hdr->import_count;
	mod->reloc_count = ctx.hdr->reloc_count;
	mod->driver_count = ctx.hdr->driver_count;
	kofo_name_copy(mod->name, sizeof(mod->name), name);
	kofo_name_copy(mod->version, sizeof(mod->version), version);
	kofo_name_copy(mod->path, sizeof(mod->path), kpath);
	exit_addr = 0;
	ret = kofo_find_symbol(&ctx, exit_name, &exit_addr);
	if (ret == -API_ERR_NOT_FOUND) {
		mod->exit_fn = NULL;
	} else if (ret == 0) {
		mod->exit_fn = (kofo_entry_fn)exit_addr;
	} else {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		memset(mod, 0, sizeof(*mod));
		return (ret);
	}
	if (kofo_call_entry((kofo_entry_fn)init_addr) != 0) {
		kmem_free(kpath);
		kofo_free_ctx(&ctx);
		memset(mod, 0, sizeof(*mod));
		return (-API_ERR_BAD_IMAGE);
	}
	ctx.image = NULL;
	kmem_free(kpath);
	kofo_free_ctx(&ctx);
	mod->state = KOFO_STATE_LOADED;
	drivers_log("[KOFO] loaded %s id=%u image=%p size=%u\n",
	    mod->name, mod->id, mod->image, (u32)mod->image_size);
	return ((int)mod->id);
}

int
api_kofo_info(u32 id, struct api_kofo_info *info)
{
	struct api_kofo_info	out;
	kofo_module_t		*mod;

	if (!proc_has_privilege(process_current())) {
		return (-API_ERR_PERM);
	}
	if (!user_range_fault_in(info, sizeof(*info), 1)) {
		return (-API_ERR_BAD_ADDR);
	}
	mod = kofo_find_slot_by_id(id);
	if (mod == NULL) {
		return (-API_ERR_NOT_FOUND);
	}
	memset(&out, 0, sizeof(out));
	out.size = sizeof(out);
	out.id = mod->id;
	out.state = mod->state;
	out.flags = mod->flags;
	out.image_base = (u64)mod->image;
	out.image_size = mod->image_size;
	out.section_count = mod->section_count;
	out.symbol_count = mod->symbol_count;
	out.import_count = mod->import_count;
	out.reloc_count = mod->reloc_count;
	out.driver_count = mod->driver_count;
	kofo_name_copy(out.name, sizeof(out.name), mod->name);
	kofo_name_copy(out.version, sizeof(out.version), mod->version);
	kofo_name_copy(out.path, sizeof(out.path), mod->path);
	memcpy(info, &out, sizeof(out));
	return (0);
}

int
api_kofo_unload(u32 id, u32 flags)
{
	kofo_module_t	*mod;
	int		ret;

	if (flags != 0) {
		return (-API_ERR_NOT_SUPPORTED);
	}
	if (!proc_has_privilege(process_current())) {
		return (-API_ERR_PERM);
	}
	mod = kofo_find_slot_by_id(id);
	if (mod == NULL) {
		return (-API_ERR_NOT_FOUND);
	}
	if (mod->state != KOFO_STATE_LOADED) {
		return (-API_ERR_BUSY);
	}
	mod->state = KOFO_STATE_UNLOADING;
	ret = kofo_call_entry(mod->exit_fn);
	if (ret != 0) {
		mod->state = KOFO_STATE_LOADED;
		return (-API_ERR_BUSY);
	}
	if (newbus_driver_range_busy(mod->image, (size_t)mod->image_size)) {
		mod->state = KOFO_STATE_LOADED;
		return (-API_ERR_BUSY);
	}
	drivers_log("[KOFO] unloaded %s id=%u\n", mod->name, mod->id);
	kmem_free(mod->image);
	memset(mod, 0, sizeof(*mod));
	return (0);
}
