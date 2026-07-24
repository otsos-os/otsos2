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

$define %type as_symbol as assembler symbol table entry
$define %type as_reloc as assembler relocation table entry
$define %type as_context as assembler translation state
$define %func main as start with args int, char **, char **
$define %func as_read_file as function with args const char *, char **, size_t *
$define %func as_parse_line as function with args as_context *, char *, int
$define %func as_write_object as function with args as_context *, const char *
$define %func as_write_binary as function with args as_context *, const char *
$define %func as_assemble_binary as function with args name, source, out data, out size

*/

/* !SPACE!

$space %internal as_init, as_free, as_error, as_strdup
$space %internal as_read_file, as_write_file, as_trim, as_strip_comment
$space %internal as_section_size, as_current_buf, as_switch_section
$space %internal as_find_symbol, as_get_symbol, as_define_symbol
$space %internal as_mark_symbol_global, as_add_reloc, as_parse_number
$space %internal as_parse_symbol_rip, as_split_operands, as_emit_data_value
$space %internal as_emit_string, as_parse_directive, as_parse_instruction
$space %internal as_parse_line, as_assemble, as_write_object
$space %internal as_flatten_binary, as_write_binary, as_usage
$space %export as_assemble_binary, main

*/

#include <ctype.h>
#include <libas.h>
#include <libelf.h>
#include <libemit.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AS_MAX_SYMBOLS	512
#define AS_MAX_RELOCS	1024
#define AS_MAX_OPERANDS	4

#define AS_SEC_TEXT	1
#define AS_SEC_DATA	2
#define AS_SEC_RODATA	3
#define AS_SEC_BSS	4
#define AS_SEC_COUNT	4

typedef struct as_symbol {
	char		*name;
	uint16_t	section;
	uint64_t	value;
	int		defined;
	int		global;
	int		external;
} as_symbol;

typedef struct as_reloc {
	uint16_t	section;
	uint64_t	offset;
	uint32_t	type;
	uint32_t	symbol;
	int64_t		addend;
} as_reloc;

typedef struct as_context {
	emit_buf	sections[AS_SEC_COUNT + 1];
	uint64_t	section_align[AS_SEC_COUNT + 1];
	uint64_t	bss_size;
	as_symbol	symbols[AS_MAX_SYMBOLS];
	as_reloc	relocs[AS_MAX_RELOCS];
	const char	*input_name;
	uint16_t	current_section;
	int		symbol_count;
	int		reloc_count;
	int		errors;
} as_context;

static void	as_init(as_context *ctx, const char *input_name);
static void	as_free(as_context *ctx);
static int	as_error(as_context *ctx, int line_no, const char *fmt, ...);
static char	*as_strdup(const char *str);
static int	as_read_file(const char *path, char **out, size_t *out_size);
static int	as_write_file(const char *path, const void *data, size_t size);
static char	*as_trim(char *str);
static void	as_strip_comment(char *line);
static uint64_t	as_section_size(as_context *ctx, uint16_t section);
static emit_buf	*as_current_buf(as_context *ctx);
static int	as_switch_section(as_context *ctx, const char *name);
static int	as_find_symbol(as_context *ctx, const char *name);
static int	as_get_symbol(as_context *ctx, const char *name);
static int	as_define_symbol(as_context *ctx, const char *name, int line_no);
static int	as_mark_symbol_global(as_context *ctx, const char *name);
static int	as_add_reloc(as_context *ctx, uint16_t section, uint64_t offset,
		    uint32_t type, uint32_t symbol, int64_t addend);
static int	as_parse_number(const char *text, int64_t *out);
static int	as_parse_symbol_rip(const char *op, char *name, size_t name_size);
static int	as_parse_disp_base(const char *op, int32_t *disp, int *base);
static int	as_branch_cc(const char *mnemonic, uint8_t *cc);
static int	as_split_operands(char *rest, char **ops, int max_ops);
static int	as_emit_data_value(as_context *ctx, int line_no, int width,
		    const char *op);
static int	as_emit_string(as_context *ctx, int line_no, const char *op,
		    int nul_term);
static int	as_parse_directive(as_context *ctx, char *line, int line_no);
static int	as_parse_instruction(as_context *ctx, char *line, int line_no);
static int	as_parse_line(as_context *ctx, char *line, int line_no);
static int	as_assemble(as_context *ctx, char *source);
static int	as_write_object(as_context *ctx, const char *path);
static int	as_flatten_binary(as_context *ctx, emit_buf *flat);
static int	as_write_binary(as_context *ctx, const char *path);
static void	as_usage(void);

static void
as_init(as_context *ctx, const char *input_name)
{
	int	i;

	memset(ctx, 0, sizeof(*ctx));
	ctx->input_name = input_name;
	ctx->current_section = AS_SEC_TEXT;
	ctx->section_align[AS_SEC_TEXT] = 16;
	ctx->section_align[AS_SEC_DATA] = 8;
	ctx->section_align[AS_SEC_RODATA] = 8;
	ctx->section_align[AS_SEC_BSS] = 8;
	for (i = 0; i <= AS_SEC_COUNT; i++) {
		emit_buf_init(&ctx->sections[i]);
	}
}

static void
as_free(as_context *ctx)
{
	int	i;

	for (i = 0; i <= AS_SEC_COUNT; i++) {
		emit_buf_free(&ctx->sections[i]);
	}
	for (i = 0; i < ctx->symbol_count; i++) {
		free(ctx->symbols[i].name);
	}
}

static int
as_error(as_context *ctx, int line_no, const char *fmt, ...)
{
	va_list	ap;

	fprintf(stderr, "%s:%d: error: ", ctx->input_name, line_no);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
	ctx->errors++;
	return (-1);
}

static char *
as_strdup(const char *str)
{
	char	*out;
	size_t	len;

	len = strlen(str) + 1;
	out = malloc(len);
	if (!out) {
		return (NULL);
	}
	memcpy(out, str, len);
	return (out);
}

static int
as_read_file(const char *path, char **out, size_t *out_size)
{
	FILE	*fp;
	char	*buf;
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
	buf = malloc((size_t)size + 1);
	if (!buf) {
		fclose(fp);
		return (-1);
	}
	if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
		free(buf);
		fclose(fp);
		return (-1);
	}
	buf[size] = '\0';
	fclose(fp);
	*out = buf;
	if (out_size) {
		*out_size = (size_t)size;
	}
	return (0);
}

static int
as_write_file(const char *path, const void *data, size_t size)
{
	FILE	*fp;
	int	rc;

	fp = fopen(path, "wb");
	if (!fp) {
		return (-1);
	}
	rc = 0;
	if (size != 0 && fwrite(data, 1, size, fp) != size) {
		rc = -1;
	}
	if (fclose(fp) != 0) {
		rc = -1;
	}
	return (rc);
}

static char *
as_trim(char *str)
{
	char	*end;

	while (*str && isspace((unsigned char)*str)) {
		str++;
	}
	end = str + strlen(str);
	while (end > str && isspace((unsigned char)end[-1])) {
		*--end = '\0';
	}
	return (str);
}

static void
as_strip_comment(char *line)
{
	char	*start;
	int	quote;

	start = line;
	quote = 0;
	while (*line) {
		if (*line == '"' && (line == start || line[-1] != '\\')) {
			quote = !quote;
		}
		if (!quote && *line == '#') {
			*line = '\0';
			return;
		}
		line++;
	}
}

static uint64_t
as_section_size(as_context *ctx, uint16_t section)
{
	if (section == AS_SEC_BSS) {
		return (ctx->bss_size);
	}
	if (section == 0 || section > AS_SEC_COUNT) {
		return (0);
	}
	return (ctx->sections[section].size);
}

static emit_buf *
as_current_buf(as_context *ctx)
{
	if (ctx->current_section == AS_SEC_BSS) {
		return (NULL);
	}
	return (&ctx->sections[ctx->current_section]);
}

static int
as_switch_section(as_context *ctx, const char *name)
{
	if (strcmp(name, ".text") == 0 || strcmp(name, "text") == 0) {
		ctx->current_section = AS_SEC_TEXT;
		return (0);
	}
	if (strcmp(name, ".data") == 0 || strcmp(name, "data") == 0) {
		ctx->current_section = AS_SEC_DATA;
		return (0);
	}
	if (strcmp(name, ".rodata") == 0 || strcmp(name, "rodata") == 0) {
		ctx->current_section = AS_SEC_RODATA;
		return (0);
	}
	if (strcmp(name, ".bss") == 0 || strcmp(name, "bss") == 0) {
		ctx->current_section = AS_SEC_BSS;
		return (0);
	}
	return (-1);
}

static int
as_find_symbol(as_context *ctx, const char *name)
{
	int	i;

	for (i = 0; i < ctx->symbol_count; i++) {
		if (strcmp(ctx->symbols[i].name, name) == 0) {
			return (i);
		}
	}
	return (-1);
}

static int
as_get_symbol(as_context *ctx, const char *name)
{
	as_symbol	*sym;
	int		idx;

	idx = as_find_symbol(ctx, name);
	if (idx >= 0) {
		return (idx);
	}
	if (ctx->symbol_count >= AS_MAX_SYMBOLS) {
		return (-1);
	}
	idx = ctx->symbol_count++;
	sym = &ctx->symbols[idx];
	memset(sym, 0, sizeof(*sym));
	sym->name = as_strdup(name);
	if (!sym->name) {
		ctx->symbol_count--;
		return (-1);
	}
	sym->external = 1;
	return (idx);
}

static int
as_define_symbol(as_context *ctx, const char *name, int line_no)
{
	as_symbol	*sym;
	int		idx;

	idx = as_find_symbol(ctx, name);
	if (idx < 0) {
		idx = as_get_symbol(ctx, name);
		if (idx < 0) {
			return (as_error(ctx, line_no, "too many symbols"));
		}
		ctx->symbols[idx].global = 0;
	}
	sym = &ctx->symbols[idx];
	if (sym->defined) {
		return (as_error(ctx, line_no, "redefined symbol '%s'", name));
	}
	sym->defined = 1;
	sym->external = 0;
	sym->section = ctx->current_section;
	sym->value = as_section_size(ctx, ctx->current_section);
	return (0);
}

static int
as_mark_symbol_global(as_context *ctx, const char *name)
{
	int	idx;

	idx = as_get_symbol(ctx, name);
	if (idx < 0) {
		return (-1);
	}
	ctx->symbols[idx].global = 1;
	return (0);
}

static int
as_add_reloc(as_context *ctx, uint16_t section, uint64_t offset,
    uint32_t type, uint32_t symbol, int64_t addend)
{
	as_reloc	*rel;

	if (ctx->reloc_count >= AS_MAX_RELOCS) {
		return (-1);
	}
	rel = &ctx->relocs[ctx->reloc_count++];
	rel->section = section;
	rel->offset = offset;
	rel->type = type;
	rel->symbol = symbol;
	rel->addend = addend;
	return (0);
}

static int
as_parse_number(const char *text, int64_t *out)
{
	char	*end;
	long	value;

	if (!text || !out) {
		return (-1);
	}
	if (*text == '$') {
		text++;
	}
	value = strtol(text, &end, 0);
	if (end == text || *as_trim(end) != '\0') {
		return (-1);
	}
	*out = value;
	return (0);
}

static int
as_parse_symbol_rip(const char *op, char *name, size_t name_size)
{
	const char	*suffix;
	size_t		len;

	suffix = strstr(op, "(%rip)");
	if (!suffix || suffix[6] != '\0' || suffix == op) {
		return (-1);
	}
	len = (size_t)(suffix - op);
	if (len >= name_size) {
		return (-1);
	}
	memcpy(name, op, len);
	name[len] = '\0';
	return (0);
}

static int
as_parse_disp_base(const char *op, int32_t *disp, int *base)
{
	char	buf[64];
	char	*open, *close;
	int64_t	value;
	size_t	len;

	if (!op || !disp || !base) {
		return (-1);
	}
	open = strchr(op, '(');
	close = open ? strchr(open, ')') : NULL;
	if (!open || !close || close[1] != '\0') {
		return (-1);
	}
	len = (size_t)(open - op);
	if (len >= sizeof(buf)) {
		return (-1);
	}
	memcpy(buf, op, len);
	buf[len] = '\0';
	if (len == 0) {
		value = 0;
	} else if (as_parse_number(buf, &value) != 0) {
		return (-1);
	}
	len = (size_t)(close - open - 1);
	if (len >= sizeof(buf)) {
		return (-1);
	}
	memcpy(buf, open + 1, len);
	buf[len] = '\0';
	if (emit_amd64_reg_parse(buf, base) != 0) {
		return (-1);
	}
	*disp = (int32_t)value;
	return (0);
}

static int
as_branch_cc(const char *mnemonic, uint8_t *cc)
{
	if (!mnemonic || !cc) {
		return (-1);
	}
	if (strcmp(mnemonic, "je") == 0 || strcmp(mnemonic, "jz") == 0) {
		*cc = 0x4;
		return (0);
	}
	if (strcmp(mnemonic, "jne") == 0 || strcmp(mnemonic, "jnz") == 0) {
		*cc = 0x5;
		return (0);
	}
	if (strcmp(mnemonic, "jl") == 0 || strcmp(mnemonic, "jnge") == 0) {
		*cc = 0xc;
		return (0);
	}
	if (strcmp(mnemonic, "jge") == 0 || strcmp(mnemonic, "jnl") == 0) {
		*cc = 0xd;
		return (0);
	}
	if (strcmp(mnemonic, "jle") == 0 || strcmp(mnemonic, "jng") == 0) {
		*cc = 0xe;
		return (0);
	}
	if (strcmp(mnemonic, "jg") == 0 || strcmp(mnemonic, "jnle") == 0) {
		*cc = 0xf;
		return (0);
	}
	return (-1);
}

static int
as_split_operands(char *rest, char **ops, int max_ops)
{
	char	*start;
	int	count, paren, quote;

	count = 0;
	paren = 0;
	quote = 0;
	start = rest;
	while (*rest) {
		if (*rest == '"' && (rest == start || rest[-1] != '\\')) {
			quote = !quote;
		} else if (!quote && *rest == '(') {
			paren++;
		} else if (!quote && *rest == ')' && paren > 0) {
			paren--;
		} else if (!quote && paren == 0 && *rest == ',') {
			*rest = '\0';
			if (count < max_ops) {
				ops[count++] = as_trim(start);
			}
			start = rest + 1;
		}
		rest++;
	}
	if (*as_trim(start) != '\0' && count < max_ops) {
		ops[count++] = as_trim(start);
	}
	return (count);
}

static int
as_emit_data_value(as_context *ctx, int line_no, int width, const char *op)
{
	emit_buf	*buf;
	int64_t		value;
	int		sym;

	if (ctx->current_section == AS_SEC_BSS) {
		if (width == 8) {
			ctx->bss_size += 8;
			return (0);
		}
		ctx->bss_size += (uint64_t)width;
		return (0);
	}
	buf = as_current_buf(ctx);
	if (!buf) {
		return (as_error(ctx, line_no, "no output section"));
	}
	if (as_parse_number(op, &value) == 0) {
		if (width == 1) {
			return (emit_buf_u8(buf, (uint8_t)value));
		}
		if (width == 2) {
			return (emit_buf_u16(buf, (uint16_t)value));
		}
		if (width == 4) {
			return (emit_buf_u32(buf, (uint32_t)value));
		}
		return (emit_buf_u64(buf, (uint64_t)value));
	}
	if (width != 8) {
		return (as_error(ctx, line_no,
		    "symbol data relocation needs .quad"));
	}
	sym = as_get_symbol(ctx, op);
	if (sym < 0) {
		return (as_error(ctx, line_no, "too many symbols"));
	}
	if (as_add_reloc(ctx, ctx->current_section, buf->size,
	    ELF64_R_X86_64_64, (uint32_t)sym, 0) != 0) {
		return (as_error(ctx, line_no, "too many relocations"));
	}
	return (emit_buf_u64(buf, 0));
}

static int
as_emit_string(as_context *ctx, int line_no, const char *op, int nul_term)
{
	emit_buf	*buf;
	const char	*p;
	char		c;

	if (ctx->current_section == AS_SEC_BSS) {
		return (as_error(ctx, line_no, "string in .bss"));
	}
	buf = as_current_buf(ctx);
	op = as_trim((char *)op);
	if (*op != '"') {
		return (as_error(ctx, line_no, "expected string literal"));
	}
	p = op + 1;
	while (*p && *p != '"') {
		c = *p++;
		if (c == '\\') {
			c = *p++;
			if (c == 'n') {
				c = '\n';
			} else if (c == 't') {
				c = '\t';
			} else if (c == '0') {
				c = '\0';
			}
		}
		if (emit_buf_u8(buf, (uint8_t)c) != 0) {
			return (as_error(ctx, line_no, "out of memory"));
		}
	}
	if (*p != '"') {
		return (as_error(ctx, line_no, "unterminated string"));
	}
	if (nul_term && emit_buf_u8(buf, 0) != 0) {
		return (as_error(ctx, line_no, "out of memory"));
	}
	return (0);
}

static int
as_parse_directive(as_context *ctx, char *line, int line_no)
{
	char	*dir, *rest, *ops[AS_MAX_OPERANDS];
	int64_t	value, fill;
	int	count, i, width;

	dir = line;
	while (*line && !isspace((unsigned char)*line)) {
		line++;
	}
	if (*line) {
		*line++ = '\0';
	}
	rest = as_trim(line);
	if (strcmp(dir, ".text") == 0 || strcmp(dir, ".data") == 0 ||
	    strcmp(dir, ".rodata") == 0 || strcmp(dir, ".bss") == 0) {
		return (as_switch_section(ctx, dir));
	}
	if (strcmp(dir, ".section") == 0) {
		if (as_switch_section(ctx, rest) != 0) {
			return (as_error(ctx, line_no, "unsupported section '%s'",
			    rest));
		}
		return (0);
	}
	if (strcmp(dir, ".globl") == 0 || strcmp(dir, ".global") == 0) {
		count = as_split_operands(rest, ops, AS_MAX_OPERANDS);
		for (i = 0; i < count; i++) {
			if (as_mark_symbol_global(ctx, ops[i]) != 0) {
				return (as_error(ctx, line_no, "too many symbols"));
			}
		}
		return (0);
	}
	if (strcmp(dir, ".extern") == 0) {
		count = as_split_operands(rest, ops, AS_MAX_OPERANDS);
		for (i = 0; i < count; i++) {
			if (as_mark_symbol_global(ctx, ops[i]) != 0) {
				return (as_error(ctx, line_no, "too many symbols"));
			}
		}
		return (0);
	}
	if (strcmp(dir, ".byte") == 0 || strcmp(dir, ".word") == 0 ||
	    strcmp(dir, ".long") == 0 || strcmp(dir, ".quad") == 0) {
		width = 1;
		if (strcmp(dir, ".word") == 0) {
			width = 2;
		} else if (strcmp(dir, ".long") == 0) {
			width = 4;
		} else if (strcmp(dir, ".quad") == 0) {
			width = 8;
		}
		count = as_split_operands(rest, ops, AS_MAX_OPERANDS);
		for (i = 0; i < count; i++) {
			if (as_emit_data_value(ctx, line_no, width, ops[i]) != 0) {
				return (-1);
			}
		}
		return (0);
	}
	if (strcmp(dir, ".ascii") == 0 || strcmp(dir, ".asciz") == 0) {
		return (as_emit_string(ctx, line_no, rest,
		    strcmp(dir, ".asciz") == 0));
	}
	if (strcmp(dir, ".align") == 0 || strcmp(dir, ".balign") == 0) {
		if (as_parse_number(rest, &value) != 0 || value <= 0 ||
		    (value & (value - 1)) != 0) {
			return (as_error(ctx, line_no, "bad alignment"));
		}
		if ((uint64_t)value > ctx->section_align[ctx->current_section]) {
			ctx->section_align[ctx->current_section] =
			    (uint64_t)value;
		}
		if (ctx->current_section == AS_SEC_BSS) {
			ctx->bss_size = (ctx->bss_size + value - 1) &
			    ~(uint64_t)(value - 1);
			return (0);
		}
		return (emit_buf_align(as_current_buf(ctx), (size_t)value, 0));
	}
	if (strcmp(dir, ".p2align") == 0) {
		if (as_parse_number(rest, &value) != 0 || value < 0 ||
		    value > 30) {
			return (as_error(ctx, line_no, "bad p2 alignment"));
		}
		value = 1L << value;
		if ((uint64_t)value > ctx->section_align[ctx->current_section]) {
			ctx->section_align[ctx->current_section] =
			    (uint64_t)value;
		}
		if (ctx->current_section == AS_SEC_BSS) {
			ctx->bss_size = (ctx->bss_size + value - 1) &
			    ~(uint64_t)(value - 1);
			return (0);
		}
		return (emit_buf_align(as_current_buf(ctx), (size_t)value, 0));
	}
	if (strcmp(dir, ".space") == 0 || strcmp(dir, ".zero") == 0) {
		fill = 0;
		count = as_split_operands(rest, ops, AS_MAX_OPERANDS);
		if (count < 1 || as_parse_number(ops[0], &value) != 0 ||
		    value < 0) {
			return (as_error(ctx, line_no, "bad .space size"));
		}
		if (count > 1 && as_parse_number(ops[1], &fill) != 0) {
			return (as_error(ctx, line_no, "bad .space fill"));
		}
		if (ctx->current_section == AS_SEC_BSS) {
			ctx->bss_size += (uint64_t)value;
			return (0);
		}
		while (value-- > 0) {
			if (emit_buf_u8(as_current_buf(ctx), (uint8_t)fill) != 0) {
				return (as_error(ctx, line_no, "out of memory"));
			}
		}
		return (0);
	}
	if (strcmp(dir, ".type") == 0 || strcmp(dir, ".size") == 0 ||
	    strcmp(dir, ".file") == 0 || strcmp(dir, ".ident") == 0 ||
	    strcmp(dir, ".note.GNU-stack") == 0) {
		return (0);
	}
	return (as_error(ctx, line_no, "unsupported directive '%s'", dir));
}

static int
as_parse_instruction(as_context *ctx, char *line, int line_no)
{
	char		*mnemonic, *rest, *ops[AS_MAX_OPERANDS];
	char		sym_name[128];
	emit_buf	*buf;
	int64_t		imm;
	uint64_t	start;
	int32_t		disp;
	int		count, reg_a, reg_b, base, sym;
	uint8_t		cc;

	if (ctx->current_section == AS_SEC_BSS) {
		return (as_error(ctx, line_no, "instruction in .bss"));
	}
	buf = as_current_buf(ctx);
	mnemonic = line;
	while (*line && !isspace((unsigned char)*line)) {
		line++;
	}
	if (*line) {
		*line++ = '\0';
	}
	rest = as_trim(line);
	count = as_split_operands(rest, ops, AS_MAX_OPERANDS);

	if (strcmp(mnemonic, "ret") == 0 || strcmp(mnemonic, "retq") == 0) {
		return (emit_amd64_ret(buf));
	}
	if (strcmp(mnemonic, "syscall") == 0) {
		return (emit_amd64_syscall(buf));
	}
	if (strcmp(mnemonic, "nop") == 0) {
		return (emit_amd64_nop(buf));
	}
	if (strcmp(mnemonic, "hlt") == 0) {
		return (emit_amd64_hlt(buf));
	}
	if (strcmp(mnemonic, "cli") == 0) {
		return (emit_amd64_cli(buf));
	}
	if (strcmp(mnemonic, "sti") == 0) {
		return (emit_amd64_sti(buf));
	}
	if (strcmp(mnemonic, "iretq") == 0) {
		return (emit_amd64_iretq(buf));
	}
	if (strcmp(mnemonic, "int") == 0) {
		if (count != 1 || as_parse_number(ops[0], &imm) != 0) {
			return (as_error(ctx, line_no, "bad int operand"));
		}
		return (emit_amd64_int_imm8(buf, (uint8_t)imm));
	}
	if (strcmp(mnemonic, "pushq") == 0 || strcmp(mnemonic, "push") == 0) {
		if (count != 1 ||
		    emit_amd64_reg_parse(ops[0], &reg_a) != 0) {
			return (as_error(ctx, line_no, "bad push operand"));
		}
		return (emit_amd64_push_reg(buf, reg_a));
	}
	if (strcmp(mnemonic, "popq") == 0 || strcmp(mnemonic, "pop") == 0) {
		if (count != 1 ||
		    emit_amd64_reg_parse(ops[0], &reg_a) != 0) {
			return (as_error(ctx, line_no, "bad pop operand"));
		}
		return (emit_amd64_pop_reg(buf, reg_a));
	}
	if (strcmp(mnemonic, "movq") == 0 ||
	    strcmp(mnemonic, "movabsq") == 0) {
		if (count != 2) {
			return (as_error(ctx, line_no, "bad mov operands"));
		}
		if (ops[0][0] == '$' &&
		    emit_amd64_reg_parse(ops[1], &reg_b) == 0) {
			start = buf->size;
			if (as_parse_number(ops[0], &imm) == 0) {
				return (emit_amd64_mov_imm64_reg(buf,
				    (uint64_t)imm, reg_b));
			}
			sym = as_get_symbol(ctx, ops[0] + 1);
			if (sym < 0) {
				return (as_error(ctx, line_no, "too many symbols"));
			}
			if (emit_amd64_mov_imm64_reg(buf, 0, reg_b) != 0 ||
			    as_add_reloc(ctx, ctx->current_section, start + 2,
			    ELF64_R_X86_64_64, (uint32_t)sym, 0) != 0) {
				return (as_error(ctx, line_no, "emit failed"));
			}
			return (0);
		}
		if (emit_amd64_reg_parse(ops[0], &reg_a) == 0 &&
		    emit_amd64_reg_parse(ops[1], &reg_b) == 0) {
			return (emit_amd64_mov_reg_reg(buf, reg_a, reg_b));
		}
		if (as_parse_symbol_rip(ops[0], sym_name,
		    sizeof(sym_name)) == 0 &&
		    emit_amd64_reg_parse(ops[1], &reg_b) == 0) {
			start = buf->size;
			sym = as_get_symbol(ctx, sym_name);
			if (sym < 0) {
				return (as_error(ctx, line_no, "too many symbols"));
			}
			if (emit_amd64_mov_rip_reg(buf, reg_b) != 0 ||
			    as_add_reloc(ctx, ctx->current_section, start + 3,
			    ELF64_R_X86_64_PC32, (uint32_t)sym, -4) != 0) {
				return (as_error(ctx, line_no, "emit failed"));
			}
			return (0);
		}
		return (as_error(ctx, line_no, "unsupported mov operands"));
	}
	if (strcmp(mnemonic, "movl") == 0) {
		if (count != 2) {
			return (as_error(ctx, line_no, "bad movl operands"));
		}
		if (as_parse_disp_base(ops[0], &disp, &base) == 0 &&
		    emit_amd64_reg32_parse(ops[1], &reg_b) == 0) {
			return (emit_amd64_mov_mem32_reg(buf, base, disp,
			    reg_b));
		}
		if (emit_amd64_reg32_parse(ops[0], &reg_a) == 0 &&
		    as_parse_disp_base(ops[1], &disp, &base) == 0) {
			return (emit_amd64_mov_reg_mem32(buf, reg_a, base,
			    disp));
		}
		return (as_error(ctx, line_no, "unsupported movl operands"));
	}
	if (strcmp(mnemonic, "movslq") == 0 ||
	    strcmp(mnemonic, "movsxd") == 0) {
		if (count != 2 ||
		    emit_amd64_reg32_parse(ops[0], &reg_a) != 0 ||
		    emit_amd64_reg_parse(ops[1], &reg_b) != 0) {
			return (as_error(ctx, line_no, "bad movslq operands"));
		}
		return (emit_amd64_movsxd_reg_reg(buf, reg_a, reg_b));
	}
	if (strcmp(mnemonic, "leaq") == 0 || strcmp(mnemonic, "lea") == 0) {
		if (count != 2 ||
		    as_parse_symbol_rip(ops[0], sym_name, sizeof(sym_name)) != 0 ||
		    emit_amd64_reg_parse(ops[1], &reg_b) != 0) {
			return (as_error(ctx, line_no, "bad lea operands"));
		}
		start = buf->size;
		sym = as_get_symbol(ctx, sym_name);
		if (sym < 0) {
			return (as_error(ctx, line_no, "too many symbols"));
		}
		if (emit_amd64_lea_rip_reg(buf, reg_b) != 0 ||
		    as_add_reloc(ctx, ctx->current_section, start + 3,
		    ELF64_R_X86_64_PC32, (uint32_t)sym, -4) != 0) {
			return (as_error(ctx, line_no, "emit failed"));
		}
		return (0);
	}
	if (strcmp(mnemonic, "xorq") == 0 || strcmp(mnemonic, "testq") == 0) {
		if (count != 2 ||
		    emit_amd64_reg_parse(ops[0], &reg_a) != 0 ||
		    emit_amd64_reg_parse(ops[1], &reg_b) != 0) {
			return (as_error(ctx, line_no, "bad register operands"));
		}
		if (strcmp(mnemonic, "xorq") == 0) {
			return (emit_amd64_xor_reg_reg(buf, reg_a, reg_b));
		}
		return (emit_amd64_test_reg_reg(buf, reg_a, reg_b));
	}
	if (strcmp(mnemonic, "addq") == 0 || strcmp(mnemonic, "subq") == 0 ||
	    strcmp(mnemonic, "cmpq") == 0) {
		if (count != 2) {
			return (as_error(ctx, line_no, "bad qword operands"));
		}
		if (as_parse_number(ops[0], &imm) == 0 &&
		    emit_amd64_reg_parse(ops[1], &reg_b) == 0) {
			if (strcmp(mnemonic, "addq") == 0) {
				return (emit_amd64_add_imm_reg(buf,
				    (int32_t)imm, reg_b));
			}
			if (strcmp(mnemonic, "subq") == 0) {
				return (emit_amd64_sub_imm_reg(buf,
				    (int32_t)imm, reg_b));
			}
			return (emit_amd64_cmp_imm_reg(buf, (int32_t)imm,
			    reg_b));
		}
		if (emit_amd64_reg_parse(ops[0], &reg_a) == 0 &&
		    emit_amd64_reg_parse(ops[1], &reg_b) == 0) {
			if (strcmp(mnemonic, "addq") == 0) {
				return (emit_amd64_add_reg_reg(buf, reg_a,
				    reg_b));
			}
			if (strcmp(mnemonic, "subq") == 0) {
				return (emit_amd64_sub_reg_reg(buf, reg_a,
				    reg_b));
			}
			return (emit_amd64_cmp_reg_reg(buf, reg_a, reg_b));
		}
		return (as_error(ctx, line_no, "bad qword operands"));
	}
	if (strcmp(mnemonic, "imulq") == 0) {
		if (count != 2 ||
		    emit_amd64_reg_parse(ops[0], &reg_a) != 0 ||
		    emit_amd64_reg_parse(ops[1], &reg_b) != 0) {
			return (as_error(ctx, line_no, "bad imulq operands"));
		}
		return (emit_amd64_imul_reg_reg(buf, reg_a, reg_b));
	}
	if (strcmp(mnemonic, "shlq") == 0 || strcmp(mnemonic, "salq") == 0 ||
	    strcmp(mnemonic, "sarq") == 0) {
		if (count != 2 || as_parse_number(ops[0], &imm) != 0 ||
		    imm < 0 || imm > 63 ||
		    emit_amd64_reg_parse(ops[1], &reg_b) != 0) {
			return (as_error(ctx, line_no, "bad shift operands"));
		}
		if (strcmp(mnemonic, "sarq") == 0) {
			return (emit_amd64_sar_imm_reg(buf, (uint8_t)imm,
			    reg_b));
		}
		return (emit_amd64_shl_imm_reg(buf, (uint8_t)imm, reg_b));
	}
	if (strcmp(mnemonic, "cqto") == 0 || strcmp(mnemonic, "cqo") == 0) {
		if (count != 0) {
			return (as_error(ctx, line_no, "bad cqto operands"));
		}
		return (emit_amd64_cqto(buf));
	}
	if (strcmp(mnemonic, "idivq") == 0) {
		if (count != 1 ||
		    emit_amd64_reg_parse(ops[0], &reg_a) != 0) {
			return (as_error(ctx, line_no, "bad idivq operand"));
		}
		return (emit_amd64_idiv_reg(buf, reg_a));
	}
	if (as_branch_cc(mnemonic, &cc) == 0) {
		if (count != 1 || ops[0][0] == '*') {
			return (as_error(ctx, line_no, "bad branch operand"));
		}
		start = buf->size;
		sym = as_get_symbol(ctx, ops[0]);
		if (sym < 0) {
			return (as_error(ctx, line_no, "too many symbols"));
		}
		if (emit_amd64_jcc_rel32(buf, cc, 0) != 0 ||
		    as_add_reloc(ctx, ctx->current_section, start + 2,
		    ELF64_R_X86_64_PC32, (uint32_t)sym, -4) != 0) {
			return (as_error(ctx, line_no, "emit failed"));
		}
		return (0);
	}
	if (strcmp(mnemonic, "call") == 0 || strcmp(mnemonic, "callq") == 0 ||
	    strcmp(mnemonic, "jmp") == 0 || strcmp(mnemonic, "jmpq") == 0) {
		if (count != 1 || ops[0][0] == '*') {
			return (as_error(ctx, line_no, "bad branch operand"));
		}
		start = buf->size;
		sym = as_get_symbol(ctx, ops[0]);
		if (sym < 0) {
			return (as_error(ctx, line_no, "too many symbols"));
		}
		if (mnemonic[0] == 'c') {
			if (emit_amd64_call_rel32(buf, 0) != 0 ||
			    as_add_reloc(ctx, ctx->current_section, start + 1,
			    ELF64_R_X86_64_PLT32, (uint32_t)sym, -4) != 0) {
				return (as_error(ctx, line_no, "emit failed"));
			}
			return (0);
		}
		if (emit_amd64_jmp_rel32(buf, 0) != 0 ||
		    as_add_reloc(ctx, ctx->current_section, start + 1,
		    ELF64_R_X86_64_PC32, (uint32_t)sym, -4) != 0) {
			return (as_error(ctx, line_no, "emit failed"));
		}
		return (0);
	}
	return (as_error(ctx, line_no, "unsupported instruction '%s'",
	    mnemonic));
}

static int
as_parse_line(as_context *ctx, char *line, int line_no)
{
	char	*colon;
	char	*label;

	as_strip_comment(line);
	line = as_trim(line);
	while (*line) {
		colon = strchr(line, ':');
		if (!colon) {
			break;
		}
		*colon = '\0';
		label = as_trim(line);
		if (*label == '\0') {
			return (as_error(ctx, line_no, "empty label"));
		}
		if (as_define_symbol(ctx, label, line_no) != 0) {
			return (-1);
		}
		line = as_trim(colon + 1);
	}
	if (*line == '\0') {
		return (0);
	}
	if (*line == '.') {
		return (as_parse_directive(ctx, line, line_no));
	}
	return (as_parse_instruction(ctx, line, line_no));
}

static int
as_assemble(as_context *ctx, char *source)
{
	char	*line, *next;
	int	line_no;

	line_no = 1;
	line = source;
	while (*line) {
		next = strchr(line, '\n');
		if (next) {
			*next++ = '\0';
		} else {
			next = line + strlen(line);
		}
		as_parse_line(ctx, line, line_no);
		line = next;
		line_no++;
	}
	return (ctx->errors == 0 ? 0 : -1);
}

static int
as_write_object(as_context *ctx, const char *path)
{
	elf64_section_desc	sections[AS_SEC_COUNT];
	elf64_symbol_desc	symbols[AS_MAX_SYMBOLS];
	elf64_rela_desc		relocs[AS_MAX_RELOCS];
	elf64_object_desc	desc;
	as_symbol		*sym;
	int			i;

	memset(sections, 0, sizeof(sections));
	sections[0].name = ".text";
	sections[0].type = ELF64_SHT_PROGBITS;
	sections[0].flags = ELF64_SHF_ALLOC | ELF64_SHF_EXECINSTR;
	sections[0].align = ctx->section_align[AS_SEC_TEXT];
	sections[0].data = ctx->sections[AS_SEC_TEXT].data;
	sections[0].size = ctx->sections[AS_SEC_TEXT].size;
	sections[1].name = ".data";
	sections[1].type = ELF64_SHT_PROGBITS;
	sections[1].flags = ELF64_SHF_ALLOC | ELF64_SHF_WRITE;
	sections[1].align = ctx->section_align[AS_SEC_DATA];
	sections[1].data = ctx->sections[AS_SEC_DATA].data;
	sections[1].size = ctx->sections[AS_SEC_DATA].size;
	sections[2].name = ".rodata";
	sections[2].type = ELF64_SHT_PROGBITS;
	sections[2].flags = ELF64_SHF_ALLOC;
	sections[2].align = ctx->section_align[AS_SEC_RODATA];
	sections[2].data = ctx->sections[AS_SEC_RODATA].data;
	sections[2].size = ctx->sections[AS_SEC_RODATA].size;
	sections[3].name = ".bss";
	sections[3].type = ELF64_SHT_NOBITS;
	sections[3].flags = ELF64_SHF_ALLOC | ELF64_SHF_WRITE;
	sections[3].align = ctx->section_align[AS_SEC_BSS];
	sections[3].size = ctx->bss_size;

	for (i = 0; i < ctx->symbol_count; i++) {
		sym = &ctx->symbols[i];
		symbols[i].name = sym->name;
		symbols[i].bind = (sym->global || !sym->defined) ?
		    ELF64_STB_GLOBAL : ELF64_STB_LOCAL;
		symbols[i].type = ELF64_STT_NOTYPE;
		symbols[i].section = sym->defined ? sym->section :
		    ELF64_SHN_UNDEF;
		symbols[i].value = sym->defined ? sym->value : 0;
		symbols[i].size = 0;
	}
	for (i = 0; i < ctx->reloc_count; i++) {
		relocs[i].section = ctx->relocs[i].section;
		relocs[i].offset = ctx->relocs[i].offset;
		relocs[i].type = ctx->relocs[i].type;
		relocs[i].symbol = ctx->relocs[i].symbol;
		relocs[i].addend = ctx->relocs[i].addend;
	}
	desc.sections = sections;
	desc.section_count = AS_SEC_COUNT;
	desc.symbols = symbols;
	desc.symbol_count = (size_t)ctx->symbol_count;
	desc.relocs = relocs;
	desc.reloc_count = (size_t)ctx->reloc_count;
	return (elf64_write_relocatable(path, &desc));
}

static int
as_flatten_binary(as_context *ctx, emit_buf *flat)
{
	uint64_t	bases[AS_SEC_COUNT + 1];
	as_reloc	*rel;
	as_symbol	*sym;
	uint64_t	s, p, v64;
	int32_t		v32;
	int		i;

	bases[AS_SEC_TEXT] = flat->size;
	if (emit_buf_write(flat, ctx->sections[AS_SEC_TEXT].data,
	    ctx->sections[AS_SEC_TEXT].size) != 0) {
		return (-1);
	}
	bases[AS_SEC_RODATA] = flat->size;
	if (emit_buf_write(flat, ctx->sections[AS_SEC_RODATA].data,
	    ctx->sections[AS_SEC_RODATA].size) != 0) {
		return (-1);
	}
	bases[AS_SEC_DATA] = flat->size;
	if (emit_buf_write(flat, ctx->sections[AS_SEC_DATA].data,
	    ctx->sections[AS_SEC_DATA].size) != 0) {
		return (-1);
	}
	bases[AS_SEC_BSS] = flat->size;
	for (i = 0; i < (int)ctx->bss_size; i++) {
		if (emit_buf_u8(flat, 0) != 0) {
			return (-1);
		}
	}

	for (i = 0; i < ctx->reloc_count; i++) {
		rel = &ctx->relocs[i];
		sym = &ctx->symbols[rel->symbol];
		if (!sym->defined) {
			fprintf(stderr, "as: unresolved symbol '%s' in raw output\n",
			    sym->name);
			return (-1);
		}
		s = bases[sym->section] + sym->value;
		p = bases[rel->section] + rel->offset;
		if (rel->type == ELF64_R_X86_64_64) {
			v64 = s + rel->addend;
			emit_buf_write_at(flat, (size_t)p, &v64, sizeof(v64));
		} else if (rel->type == ELF64_R_X86_64_PC32 ||
		    rel->type == ELF64_R_X86_64_PLT32) {
			v32 = (int32_t)(s + rel->addend - p);
			emit_buf_write_at(flat, (size_t)p, &v32, sizeof(v32));
		} else {
			return (-1);
		}
	}
	return (0);
}

static int
as_write_binary(as_context *ctx, const char *path)
{
	emit_buf	flat;
	int		rc;

	emit_buf_init(&flat);
	rc = as_flatten_binary(ctx, &flat);
	if (rc == 0) {
		rc = as_write_file(path, flat.data, flat.size);
	}
	emit_buf_free(&flat);
	return (rc);
}

int
as_assemble_binary(const char *name, const char *source, void **out_data,
    size_t *out_size)
{
	as_context	ctx;
	emit_buf	flat;
	char		*copy;
	const char	*input_name;
	size_t		len;
	int		rc;

	if (!source || !out_data || !out_size) {
		return (-1);
	}
	*out_data = NULL;
	*out_size = 0;
	input_name = name ? name : "<memory>";
	len = strlen(source) + 1;
	copy = malloc(len);
	if (!copy) {
		return (-1);
	}
	memcpy(copy, source, len);
	as_init(&ctx, input_name);
	emit_buf_init(&flat);
	rc = as_assemble(&ctx, copy);
	if (rc == 0) {
		rc = as_flatten_binary(&ctx, &flat);
	}
	if (rc == 0) {
		*out_data = flat.data;
		*out_size = flat.size;
		flat.data = NULL;
		flat.size = 0;
		flat.capacity = 0;
	}
	emit_buf_free(&flat);
	as_free(&ctx);
	free(copy);
	return (rc);
}

static void
as_usage(void)
{
	fprintf(stderr, "usage: as [-f elf64|bin] [-o output] input.s\n");
}

#ifndef LIBAS_NO_MAIN
int
main(int argc, char **argv, char **envp)
{
	as_context	ctx;
	char		*source;
	const char	*input, *output, *format;
	size_t		source_size;
	int		i, rc;

	(void)envp;
	input = NULL;
	output = "a.o";
	format = "elf64";
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			output = argv[++i];
		} else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
			format = argv[++i];
		} else if (argv[i][0] == '-') {
			as_usage();
			return (1);
		} else {
			input = argv[i];
		}
	}
	if (!input) {
		as_usage();
		return (1);
	}
	if (as_read_file(input, &source, &source_size) != 0) {
		fprintf(stderr, "as: cannot read %s\n", input);
		return (1);
	}
	(void)source_size;
	as_init(&ctx, input);
	rc = as_assemble(&ctx, source);
	if (rc == 0) {
		if (strcmp(format, "bin") == 0) {
			rc = as_write_binary(&ctx, output);
		} else if (strcmp(format, "elf64") == 0 ||
		    strcmp(format, "elf") == 0 || strcmp(format, "o") == 0) {
			rc = as_write_object(&ctx, output);
		} else {
			fprintf(stderr, "as: unsupported format '%s'\n", format);
			rc = -1;
		}
	}
	as_free(&ctx);
	free(source);
	if (rc != 0) {
		fprintf(stderr, "as: failed\n");
		return (1);
	}
	return (0);
}
#endif
