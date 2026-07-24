/* !DEFINES!

$define %type jit_reg_src as tracked VM register value source
$define %func srapiComputeShader as function with args shader

*/

/* !SPACE!

$space %internal fixed_mul, fixed_div, jit_clamp01, asm_append
$space %internal jit_reg_clear, jit_source_store, jit_emit_shader
$space %internal jit_install
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

enum {
	JIT_SRC_INVALID = 0,
	JIT_SRC_INPUT = 1,
	JIT_SRC_PUSH = 2,
	JIT_SRC_IMM = 3
};

struct jit_reg_src {
	int32_t		imm;
	uint32_t	slot;
	uint32_t	kind;
};

static int32_t
fixed_mul(int32_t a, int32_t b)
{
	return ((int32_t)(((int64_t)a * (int64_t)b) >> 16));
}

static int32_t
fixed_div(int32_t a, int32_t b)
{
	if (b == 0) {
		return (0);
	}
	return ((int32_t)(((int64_t)a << 16) / (int64_t)b));
}

static int32_t
jit_clamp01(int32_t v)
{
	if (v < 0) {
		return (0);
	}
	if (v > SRAPI_FIXED_ONE) {
		return (SRAPI_FIXED_ONE);
	}
	return (v);
}

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

static void
jit_reg_clear(struct jit_reg_src *regs)
{
	uint32_t	i;

	for (i = 0; i < SRAPI_VM_REGS; i++) {
		regs[i].kind = JIT_SRC_INVALID;
		regs[i].slot = 0;
		regs[i].imm = 0;
	}
}

static int
jit_source_store(char *asm_source, size_t capacity, size_t *used,
    const struct jit_reg_src *src, uint32_t out_slot)
{
	const char	*base;
	uint32_t	in_off, out_off;
	int		ret;

	if (!src || out_slot >= SRAPI_VM_IO_SLOTS) {
		return (SRAPI_ERR_SHADER);
	}
	out_off = out_slot * (uint32_t)sizeof(int32_t);
	if (src->kind == JIT_SRC_INPUT || src->kind == JIT_SRC_PUSH) {
		base = src->kind == JIT_SRC_INPUT ? "%rdi" : "%rsi";
		in_off = src->slot * (uint32_t)sizeof(int32_t);
		ret = asm_append(asm_source, capacity, used,
		    "\tmovl %u(%s), %%eax\n", in_off, base);
		if (ret != SRAPI_OK) {
			return (ret);
		}
		return (asm_append(asm_source, capacity, used,
		    "\tmovl %%eax, %u(%%rdx)\n", out_off));
	}
	if (src->kind == JIT_SRC_IMM) {
		ret = asm_append(asm_source, capacity, used,
		    "\tmovq $%d, %%rax\n", src->imm);
		if (ret != SRAPI_OK) {
			return (ret);
		}
		return (asm_append(asm_source, capacity, used,
		    "\tmovl %%eax, %u(%%rdx)\n", out_off));
	}
	return (SRAPI_ERR_UNSUPPORTED);
}

static int
jit_emit_shader(srapi_shader_t *shader, char *asm_source, size_t capacity)
{
	const struct srapi_vm_inst	*inst;
	struct jit_reg_src		regs[SRAPI_VM_REGS];
	size_t				used;
	uint32_t			pc;
	int				ret;

	used = 0;
	jit_reg_clear(regs);
	ret = asm_append(asm_source, capacity, &used,
	    ".text\n.globl srapi_shader_main\nsrapi_shader_main:\n");
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
			if (inst->src0 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0];
			break;
		case SRAPI_VM_MOV_IMM:
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].slot = 0;
			regs[inst->dst].imm = inst->imm;
			break;
		case SRAPI_VM_LOAD_IN:
			if (inst->src0 >= SRAPI_VM_IO_SLOTS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst].kind = JIT_SRC_INPUT;
			regs[inst->dst].slot = inst->src0;
			regs[inst->dst].imm = 0;
			break;
		case SRAPI_VM_LOAD_PUSH:
			if (inst->src0 >= SRAPI_MAX_PUSH_CONSTANTS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst].kind = JIT_SRC_PUSH;
			regs[inst->dst].slot = inst->src0;
			regs[inst->dst].imm = 0;
			break;
		case SRAPI_VM_STORE_OUT:
			if (inst->dst >= SRAPI_VM_IO_SLOTS ||
			    inst->src0 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			ret = jit_source_store(asm_source, capacity, &used,
			    &regs[inst->src0], inst->dst);
			if (ret != SRAPI_OK) {
				return (ret);
			}
			break;
		case SRAPI_VM_ADD:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = regs[inst->src0].imm +
			    regs[inst->src1].imm;
			break;
		case SRAPI_VM_SUB:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = regs[inst->src0].imm -
			    regs[inst->src1].imm;
			break;
		case SRAPI_VM_MUL:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = fixed_mul(regs[inst->src0].imm,
			    regs[inst->src1].imm);
			break;
		case SRAPI_VM_DIV:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = fixed_div(regs[inst->src0].imm,
			    regs[inst->src1].imm);
			break;
		case SRAPI_VM_MIN:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = regs[inst->src0].imm <
			    regs[inst->src1].imm ? regs[inst->src0].imm :
			    regs[inst->src1].imm;
			break;
		case SRAPI_VM_MAX:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM ||
			    regs[inst->src1].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = regs[inst->src0].imm >
			    regs[inst->src1].imm ? regs[inst->src0].imm :
			    regs[inst->src1].imm;
			break;
		case SRAPI_VM_CLAMP01:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    regs[inst->src0].kind != JIT_SRC_IMM) {
				return (SRAPI_ERR_UNSUPPORTED);
			}
			regs[inst->dst].kind = JIT_SRC_IMM;
			regs[inst->dst].imm = jit_clamp01(regs[inst->src0].imm);
			break;
		default:
			return (SRAPI_ERR_SHADER);
		}
	}
	ret = asm_append(asm_source, capacity, &used,
	    "\tmovq $0, %%rax\n\tret\n");
	if (ret != SRAPI_OK) {
		return (ret);
	}
	return (SRAPI_OK);
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
	asm_capacity = 256 + (size_t)shader->code_count * 128;
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
