/* !DEFINES!

$define %func srapiComputeShader as function with args shader

*/

/* !SPACE!

$space %internal asm_append, jit_reg_offset, jit_emit_epilogue
$space %internal jit_emit_load64, jit_emit_store32, jit_emit_copy
$space %internal jit_emit_load_input, jit_emit_binary, jit_emit_mul
$space %internal jit_emit_div, jit_emit_minmax, jit_emit_clamp01
$space %internal jit_emit_store_output, jit_emit_shader, jit_install
$space %export srapiComputeShader

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <libas.h>
#include <native.h>
#include <srapi.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srapi_private.h"

static int
asm_append(char *asm_source, size_t capacity, size_t *used,
    const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	if (!asm_source || !used || *used >= capacity) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	va_start(ap, fmt);
	ret = vsnprintf(asm_source + *used, capacity - *used, fmt, ap);
	va_end(ap);
	if (ret < 0 || (size_t)ret >= capacity - *used) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	*used += (size_t)ret;
	return (SRAPI_OK);
}

static int32_t
jit_reg_offset(uint32_t reg)
{
	return (-((int32_t)reg + 1) * (int32_t)sizeof(int32_t));
}

static int
jit_emit_epilogue(char *asm_source, size_t capacity, size_t *used)
{
	return (asm_append(asm_source, capacity, used,
	    "\tmovq %%rbp, %%rsp\n"
	    "\tpopq %%rbp\n"
	    "\tmovq $0, %%rax\n"
	    "\tret\n"));
}

static int
jit_emit_load64(char *asm_source, size_t capacity, size_t *used,
    const uint8_t *written, uint32_t src, const char *reg32,
    const char *reg64)
{
	int32_t	off;
	int	ret;

	if (src >= SRAPI_VM_REGS) {
		return (SRAPI_ERR_SHADER);
	}
	if (!written[src]) {
		return (asm_append(asm_source, capacity, used,
		    "\tmovq $0, %s\n", reg64));
	}
	off = jit_reg_offset(src);
	ret = asm_append(asm_source, capacity, used,
	    "\tmovl %d(%%rbp), %s\n", off, reg32);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	return (asm_append(asm_source, capacity, used,
	    "\tmovslq %s, %s\n", reg32, reg64));
}

static int
jit_emit_store32(char *asm_source, size_t capacity, size_t *used,
    uint32_t dst)
{
	if (dst >= SRAPI_VM_REGS) {
		return (SRAPI_ERR_SHADER);
	}
	return (asm_append(asm_source, capacity, used,
	    "\tmovl %%eax, %d(%%rbp)\n", jit_reg_offset(dst)));
}

static int
jit_emit_copy(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src)
{
	int32_t	src_off;
	int	ret;

	if (dst >= SRAPI_VM_REGS || src >= SRAPI_VM_REGS) {
		return (SRAPI_ERR_SHADER);
	}
	if (!written[src]) {
		written[dst] = 0;
		return (SRAPI_OK);
	}
	src_off = jit_reg_offset(src);
	ret = asm_append(asm_source, capacity, used,
	    "\tmovl %d(%%rbp), %%eax\n", src_off);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_load_input(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t slot, const char *base)
{
	int	ret;

	if (dst >= SRAPI_VM_REGS) {
		return (SRAPI_ERR_SHADER);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\tmovl %u(%s), %%eax\n",
	    slot * (uint32_t)sizeof(int32_t), base);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_binary(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src0, uint32_t src1,
    const char *op)
{
	int	ret;

	ret = jit_emit_load64(asm_source, capacity, used, written, src0,
	    "%eax", "%rax");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_load64(asm_source, capacity, used, written, src1,
	    "%ecx", "%rcx");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used, "\t%s %%rcx, %%rax\n",
	    op);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_mul(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src0, uint32_t src1)
{
	int	ret;

	ret = jit_emit_load64(asm_source, capacity, used, written, src0,
	    "%eax", "%rax");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_load64(asm_source, capacity, used, written, src1,
	    "%ecx", "%rcx");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\timulq %%rcx, %%rax\n"
	    "\tsarq $16, %%rax\n");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_div(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src0, uint32_t src1,
    uint32_t label_id)
{
	int	ret;

	ret = jit_emit_load64(asm_source, capacity, used, written, src1,
	    "%ecx", "%rcx");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\ttestq %%rcx, %%rcx\n"
	    "\tje .Ljit_div_zero_%u\n", label_id);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_load64(asm_source, capacity, used, written, src0,
	    "%eax", "%rax");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\tshlq $16, %%rax\n"
	    "\tcqto\n"
	    "\tidivq %%rcx\n"
	    "\tjmp .Ljit_div_done_%u\n"
	    ".Ljit_div_zero_%u:\n"
	    "\tmovq $0, %%rax\n"
	    ".Ljit_div_done_%u:\n",
	    label_id, label_id, label_id);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_minmax(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src0, uint32_t src1,
    uint32_t label_id, int is_max)
{
	const char	*jcc;
	int		ret;

	jcc = is_max ? "jge" : "jle";
	ret = jit_emit_load64(asm_source, capacity, used, written, src0,
	    "%eax", "%rax");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_load64(asm_source, capacity, used, written, src1,
	    "%ecx", "%rcx");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\tcmpq %%rcx, %%rax\n"
	    "\t%s .Ljit_minmax_done_%u\n"
	    "\tmovq %%rcx, %%rax\n"
	    ".Ljit_minmax_done_%u:\n",
	    jcc, label_id, label_id);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_clamp01(char *asm_source, size_t capacity, size_t *used,
    uint8_t *written, uint32_t dst, uint32_t src, uint32_t label_id)
{
	int	ret;

	ret = jit_emit_load64(asm_source, capacity, used, written, src,
	    "%eax", "%rax");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = asm_append(asm_source, capacity, used,
	    "\tcmpq $0, %%rax\n"
	    "\tjge .Ljit_clamp_nonneg_%u\n"
	    "\tmovq $0, %%rax\n"
	    "\tjmp .Ljit_clamp_done_%u\n"
	    ".Ljit_clamp_nonneg_%u:\n"
	    "\tcmpq $%d, %%rax\n"
	    "\tjle .Ljit_clamp_done_%u\n"
	    "\tmovq $%d, %%rax\n"
	    ".Ljit_clamp_done_%u:\n",
	    label_id, label_id, label_id, SRAPI_FIXED_ONE, label_id,
	    SRAPI_FIXED_ONE, label_id);
	if (ret != SRAPI_OK) {
		return (ret);
	}
	ret = jit_emit_store32(asm_source, capacity, used, dst);
	if (ret == SRAPI_OK) {
		written[dst] = 1;
	}
	return (ret);
}

static int
jit_emit_store_output(char *asm_source, size_t capacity, size_t *used,
    const uint8_t *written, uint32_t out_slot, uint32_t src)
{
	int	ret;

	if (out_slot >= SRAPI_VM_IO_SLOTS || src >= SRAPI_VM_REGS) {
		return (SRAPI_ERR_SHADER);
	}
	if (written[src]) {
		ret = asm_append(asm_source, capacity, used,
		    "\tmovl %d(%%rbp), %%eax\n", jit_reg_offset(src));
	} else {
		ret = asm_append(asm_source, capacity, used,
		    "\tmovq $0, %%rax\n");
	}
	if (ret != SRAPI_OK) {
		return (ret);
	}
	return (asm_append(asm_source, capacity, used,
	    "\tmovl %%eax, %u(%%r8)\n",
	    out_slot * (uint32_t)sizeof(int32_t)));
}

static int
jit_emit_shader(srapi_shader_t *shader, char *asm_source, size_t capacity)
{
	const struct srapi_vm_inst	*inst;
	size_t				used;
	uint32_t			pc, label_id;
	uint8_t				written[SRAPI_VM_REGS];
	int				ret;

	used = 0;
	label_id = 0;
	memset(written, 0, sizeof(written));
	ret = asm_append(asm_source, capacity, &used,
	    ".text\n"
	    ".globl srapi_shader_main\n"
	    "srapi_shader_main:\n"
	    "\tpushq %%rbp\n"
	    "\tmovq %%rsp, %%rbp\n"
	    "\tsubq $%u, %%rsp\n"
	    "\tmovq %%rdx, %%r8\n",
	    (uint32_t)(SRAPI_VM_REGS * sizeof(int32_t)));
	if (ret != SRAPI_OK) {
		return (ret);
	}
	for (pc = 0; pc < shader->code_count; pc++) {
		inst = &shader->code[pc];
		if (inst->dst >= SRAPI_VM_REGS &&
		    inst->op != SRAPI_VM_STORE_OUT) {
			return (SRAPI_ERR_SHADER);
		}
		switch (inst->op) {
		case SRAPI_VM_NOP:
			break;
		case SRAPI_VM_END:
			pc = shader->code_count;
			break;
		case SRAPI_VM_MOV:
			ret = jit_emit_copy(asm_source, capacity, &used,
			    written, inst->dst, inst->src0);
			break;
		case SRAPI_VM_MOV_IMM:
			ret = asm_append(asm_source, capacity, &used,
			    "\tmovq $%d, %%rax\n", inst->imm);
			if (ret == SRAPI_OK) {
				ret = jit_emit_store32(asm_source, capacity,
				    &used, inst->dst);
			}
			if (ret == SRAPI_OK) {
				written[inst->dst] = 1;
			}
			break;
		case SRAPI_VM_LOAD_IN:
			if (inst->src0 >= SRAPI_VM_IO_SLOTS) {
				return (SRAPI_ERR_SHADER);
			}
			ret = jit_emit_load_input(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, "%rdi");
			break;
		case SRAPI_VM_LOAD_PUSH:
			if (inst->src0 >= SRAPI_MAX_PUSH_CONSTANTS) {
				return (SRAPI_ERR_SHADER);
			}
			ret = jit_emit_load_input(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, "%rsi");
			break;
		case SRAPI_VM_STORE_OUT:
			ret = jit_emit_store_output(asm_source, capacity, &used,
			    written, inst->dst, inst->src0);
			break;
		case SRAPI_VM_ADD:
			ret = jit_emit_binary(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1, "addq");
			break;
		case SRAPI_VM_SUB:
			ret = jit_emit_binary(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1, "subq");
			break;
		case SRAPI_VM_MUL:
			ret = jit_emit_mul(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1);
			break;
		case SRAPI_VM_DIV:
			ret = jit_emit_div(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1,
			    label_id++);
			break;
		case SRAPI_VM_MIN:
			ret = jit_emit_minmax(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1,
			    label_id++, 0);
			break;
		case SRAPI_VM_MAX:
			ret = jit_emit_minmax(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, inst->src1,
			    label_id++, 1);
			break;
		case SRAPI_VM_CLAMP01:
			ret = jit_emit_clamp01(asm_source, capacity, &used,
			    written, inst->dst, inst->src0, label_id++);
			break;
		default:
			return (SRAPI_ERR_SHADER);
		}
		if (ret != SRAPI_OK) {
			return (ret);
		}
	}
	return (jit_emit_epilogue(asm_source, capacity, &used));
}

static int
jit_install(srapi_shader_t *shader, const void *code, size_t code_size)
{
	struct mem_map_args	args;
	void			*exec;

	if (!shader || !code || code_size == 0) {
		return (SRAPI_ERR_INVALID);
	}
	memset(&args, 0, sizeof(args));
	args.length = code_size;
	args.prot = API_MAP_READ | API_MAP_WRITE | API_MAP_EXEC;
	args.flags = API_MAP_ANON | API_MAP_PRIVATE;
	exec = memMap(&args);
	if (!exec) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	memcpy(exec, code, code_size);
	if (shader->cpu_code && shader->cpu_code_size != 0) {
		(void)memUnmap(shader->cpu_code, shader->cpu_code_size);
	}
	shader->cpu_code = exec;
	shader->cpu_code_size = code_size;
	shader->cpu_entry = (srapi_shader_cpu_fn)exec;
	shader->cpu_flags |= SRAPI_SHADER_CPU_COMPILED;
	return (SRAPI_OK);
}

int
srapiComputeShader(srapi_shader_t *shader)
{
	char	*asm_source;
	void	*binary;
	size_t	asm_capacity, binary_size;
	int	ret;

	if (!shader) {
		return (SRAPI_ERR_INVALID);
	}
	if (shader->cpu_entry) {
		return (SRAPI_OK);
	}
	asm_capacity = 512 + (size_t)shader->code_count * 512;
	asm_source = malloc(asm_capacity);
	if (!asm_source) {
		return (SRAPI_ERR_NO_MEMORY);
	}
	ret = jit_emit_shader(shader, asm_source, asm_capacity);
	if (ret != SRAPI_OK) {
		free(asm_source);
		return (ret);
	}
	binary = NULL;
	binary_size = 0;
	ret = as_assemble_binary("srapi_shader.s", asm_source, &binary,
	    &binary_size);
	free(asm_source);
	if (ret != 0) {
		return (SRAPI_ERR_SHADER);
	}
	ret = jit_install(shader, binary, binary_size);
	free(binary);
	return (ret);
}
