/* !DEFINES!

$define %type srapi_vm_inst as shader VM instruction
$define %func srapi_vm_run as function with args shader, input, push, output

*/

/* !SPACE!

$space %internal fixed_mul, fixed_div, clamp01
$space %export srapi_vm_run

*/

/*
 * Copyright (c) 2026, otsos team
 */

#include <srapi.h>
#include <stdint.h>
#include <string.h>

#include "srapi_private.h"

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
clamp01(int32_t v)
{
	if (v < 0) {
		return (0);
	}
	if (v > SRAPI_FIXED_ONE) {
		return (SRAPI_FIXED_ONE);
	}
	return (v);
}

int
srapi_vm_run(const srapi_shader_t *shader, const int32_t *input,
    const int32_t *push, int32_t *output)
{
	const struct srapi_vm_inst	*inst;
	int32_t				regs[SRAPI_VM_REGS];
	uint32_t			pc;

	if (!shader || !input || !push || !output) {
		return (SRAPI_ERR_INVALID);
	}
	if (shader->cpu_entry) {
		return (shader->cpu_entry(input, push, output));
	}
	memset(regs, 0, sizeof(regs));
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
			return (SRAPI_OK);
		case SRAPI_VM_MOV:
			if (inst->src0 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0];
			break;
		case SRAPI_VM_MOV_IMM:
			regs[inst->dst] = inst->imm;
			break;
		case SRAPI_VM_LOAD_IN:
			if (inst->src0 >= SRAPI_VM_IO_SLOTS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = input[inst->src0];
			break;
		case SRAPI_VM_LOAD_PUSH:
			if (inst->src0 >= SRAPI_MAX_PUSH_CONSTANTS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = push[inst->src0];
			break;
		case SRAPI_VM_STORE_OUT:
			if (inst->dst >= SRAPI_VM_IO_SLOTS ||
			    inst->src0 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			output[inst->dst] = regs[inst->src0];
			break;
		case SRAPI_VM_ADD:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0] + regs[inst->src1];
			break;
		case SRAPI_VM_SUB:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0] - regs[inst->src1];
			break;
		case SRAPI_VM_MUL:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = fixed_mul(regs[inst->src0],
			    regs[inst->src1]);
			break;
		case SRAPI_VM_DIV:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = fixed_div(regs[inst->src0],
			    regs[inst->src1]);
			break;
		case SRAPI_VM_MIN:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0] < regs[inst->src1] ?
			    regs[inst->src0] : regs[inst->src1];
			break;
		case SRAPI_VM_MAX:
			if (inst->src0 >= SRAPI_VM_REGS ||
			    inst->src1 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = regs[inst->src0] > regs[inst->src1] ?
			    regs[inst->src0] : regs[inst->src1];
			break;
		case SRAPI_VM_CLAMP01:
			if (inst->src0 >= SRAPI_VM_REGS) {
				return (SRAPI_ERR_SHADER);
			}
			regs[inst->dst] = clamp01(regs[inst->src0]);
			break;
		default:
			return (SRAPI_ERR_SHADER);
		}
	}
	return (SRAPI_OK);
}
