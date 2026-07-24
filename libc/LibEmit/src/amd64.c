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

$define %type emit_buf as growable byte emission buffer
$define %type uint8_t as 8 bit unsigned
$define %type uint32_t as 32 bit unsigned
$define %type uint64_t as 64 bit unsigned
$define %type int32_t as 32 bit signed
$define %func emit_buf_init as procedure with args emit_buf *
$define %func emit_buf_free as procedure with args emit_buf *
$define %func emit_buf_write as function with args emit_buf *, const void *, size_t
$define %func emit_amd64_rex as function with args emit_buf *, int, int, int
$define %func emit_amd64_modrm as function with args emit_buf *, int, int, int
$define %func emit_amd64_modrm_mem as function with args emit_buf *, int, int, int32
$define %func emit_amd64_reg_op_reg as function with args emit_buf *, int, int, int
$define %func emit_amd64_shift_imm_reg as function with args emit_buf *, int, int, int
$define %func emit_amd64_imm_op_reg as function with args emit_buf *, int, int32_t, int

*/

/* !SPACE!

$space %internal emit_amd64_rex, emit_amd64_modrm, emit_amd64_modrm_mem
$space %internal emit_amd64_reg_op_reg, emit_amd64_shift_imm_reg
$space %internal emit_amd64_imm_op_reg
$space %export emit_buf_init, emit_buf_free, emit_buf_reserve
$space %export emit_buf_write, emit_buf_write_at
$space %export emit_buf_u8, emit_buf_u16, emit_buf_u32, emit_buf_u64
$space %export emit_buf_i32, emit_buf_align
$space %export emit_amd64_reg_parse, emit_amd64_ret, emit_amd64_syscall
$space %export emit_amd64_nop, emit_amd64_hlt, emit_amd64_cli, emit_amd64_sti
$space %export emit_amd64_iretq, emit_amd64_int_imm8
$space %export emit_amd64_push_reg, emit_amd64_pop_reg
$space %export emit_amd64_mov_imm64_reg, emit_amd64_mov_reg_reg
$space %export emit_amd64_reg32_parse, emit_amd64_mov_mem32_reg
$space %export emit_amd64_mov_reg_mem32, emit_amd64_movsxd_reg_reg
$space %export emit_amd64_mov_rip_reg, emit_amd64_lea_rip_reg
$space %export emit_amd64_xor_reg_reg, emit_amd64_test_reg_reg
$space %export emit_amd64_add_reg_reg, emit_amd64_sub_reg_reg
$space %export emit_amd64_cmp_reg_reg, emit_amd64_imul_reg_reg
$space %export emit_amd64_shl_imm_reg, emit_amd64_sar_imm_reg
$space %export emit_amd64_cqto, emit_amd64_idiv_reg
$space %export emit_amd64_add_imm_reg, emit_amd64_sub_imm_reg
$space %export emit_amd64_cmp_imm_reg, emit_amd64_call_rel32
$space %export emit_amd64_jmp_rel32, emit_amd64_jcc_rel32

*/

#include <libemit.h>
#include <stdlib.h>
#include <string.h>

void
emit_buf_init(emit_buf *buf)
{
	if (!buf) {
		return;
	}
	buf->data = NULL;
	buf->size = 0;
	buf->capacity = 0;
}

void
emit_buf_free(emit_buf *buf)
{
	if (!buf) {
		return;
	}
	free(buf->data);
	emit_buf_init(buf);
}

int
emit_buf_reserve(emit_buf *buf, size_t extra)
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
	cap = buf->capacity ? buf->capacity : 64;
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

int
emit_buf_write(emit_buf *buf, const void *data, size_t size)
{
	if (!buf || (!data && size != 0)) {
		return (-1);
	}
	if (emit_buf_reserve(buf, size) != 0) {
		return (-1);
	}
	memcpy(buf->data + buf->size, data, size);
	buf->size += size;
	return (0);
}

int
emit_buf_write_at(emit_buf *buf, size_t off, const void *data, size_t size)
{
	if (!buf || (!data && size != 0) || off + size > buf->size) {
		return (-1);
	}
	memcpy(buf->data + off, data, size);
	return (0);
}

int
emit_buf_u8(emit_buf *buf, uint8_t value)
{
	return (emit_buf_write(buf, &value, sizeof(value)));
}

int
emit_buf_u16(emit_buf *buf, uint16_t value)
{
	uint8_t	data[2];

	data[0] = (uint8_t)(value & 0xff);
	data[1] = (uint8_t)(value >> 8);
	return (emit_buf_write(buf, data, sizeof(data)));
}

int
emit_buf_u32(emit_buf *buf, uint32_t value)
{
	uint8_t	data[4];

	data[0] = (uint8_t)(value & 0xff);
	data[1] = (uint8_t)((value >> 8) & 0xff);
	data[2] = (uint8_t)((value >> 16) & 0xff);
	data[3] = (uint8_t)(value >> 24);
	return (emit_buf_write(buf, data, sizeof(data)));
}

int
emit_buf_u64(emit_buf *buf, uint64_t value)
{
	uint8_t	data[8];
	int	i;

	for (i = 0; i < 8; i++) {
		data[i] = (uint8_t)(value >> (i * 8));
	}
	return (emit_buf_write(buf, data, sizeof(data)));
}

int
emit_buf_i32(emit_buf *buf, int32_t value)
{
	return (emit_buf_u32(buf, (uint32_t)value));
}

int
emit_buf_align(emit_buf *buf, size_t align, uint8_t fill)
{
	if (!buf || align == 0) {
		return (-1);
	}
	while ((buf->size % align) != 0) {
		if (emit_buf_u8(buf, fill) != 0) {
			return (-1);
		}
	}
	return (0);
}

int
emit_amd64_reg_parse(const char *name, int *out_reg)
{
	static const char *regs[] = {
		"%rax", "%rcx", "%rdx", "%rbx", "%rsp", "%rbp", "%rsi", "%rdi",
		"%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"
	};
	int	i;

	if (!name || !out_reg) {
		return (-1);
	}
	for (i = 0; i < 16; i++) {
		if (strcmp(name, regs[i]) == 0) {
			*out_reg = i;
			return (0);
		}
	}
	return (-1);
}

int
emit_amd64_reg32_parse(const char *name, int *out_reg)
{
	static const char *regs[] = {
		"%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi",
		"%edi", "%r8d", "%r9d", "%r10d", "%r11d", "%r12d",
		"%r13d", "%r14d", "%r15d"
	};
	int	i;

	if (!name || !out_reg) {
		return (-1);
	}
	for (i = 0; i < 16; i++) {
		if (strcmp(name, regs[i]) == 0) {
			*out_reg = i;
			return (0);
		}
	}
	return (-1);
}

static int
emit_amd64_rex(emit_buf *buf, int w, int reg, int rm)
{
	uint8_t	rex;

	rex = 0x40;
	if (w) {
		rex |= 0x08;
	}
	if (reg & 8) {
		rex |= 0x04;
	}
	if (rm & 8) {
		rex |= 0x01;
	}
	if (rex == 0x40) {
		return (0);
	}
	return (emit_buf_u8(buf, rex));
}

static int
emit_amd64_modrm(emit_buf *buf, int mod, int reg, int rm)
{
	uint8_t	value;

	value = (uint8_t)(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
	return (emit_buf_u8(buf, value));
}

static int
emit_amd64_modrm_mem(emit_buf *buf, int reg, int base, int32_t disp)
{
	int	mod;

	if (base < 0 || base > 15 || reg < 0 || reg > 15) {
		return (-1);
	}
	if (disp == 0 && (base & 7) != EMIT_AMD64_RBP) {
		mod = 0;
	} else if (disp >= -128 && disp <= 127) {
		mod = 1;
	} else {
		mod = 2;
	}
	if (emit_amd64_modrm(buf, mod, reg, base) != 0) {
		return (-1);
	}
	if ((base & 7) == EMIT_AMD64_RSP) {
		if (emit_buf_u8(buf, 0x24) != 0) {
			return (-1);
		}
	}
	if (mod == 1) {
		return (emit_buf_u8(buf, (uint8_t)disp));
	}
	if (mod == 2) {
		return (emit_buf_i32(buf, disp));
	}
	return (0);
}

int
emit_amd64_ret(emit_buf *buf)
{
	return (emit_buf_u8(buf, 0xc3));
}

int
emit_amd64_syscall(emit_buf *buf)
{
	uint8_t	op[] = { 0x0f, 0x05 };

	return (emit_buf_write(buf, op, sizeof(op)));
}

int
emit_amd64_nop(emit_buf *buf)
{
	return (emit_buf_u8(buf, 0x90));
}

int
emit_amd64_hlt(emit_buf *buf)
{
	return (emit_buf_u8(buf, 0xf4));
}

int
emit_amd64_cli(emit_buf *buf)
{
	return (emit_buf_u8(buf, 0xfa));
}

int
emit_amd64_sti(emit_buf *buf)
{
	return (emit_buf_u8(buf, 0xfb));
}

int
emit_amd64_iretq(emit_buf *buf)
{
	uint8_t	op[] = { 0x48, 0xcf };

	return (emit_buf_write(buf, op, sizeof(op)));
}

int
emit_amd64_int_imm8(emit_buf *buf, uint8_t imm)
{
	if (emit_buf_u8(buf, 0xcd) != 0) {
		return (-1);
	}
	return (emit_buf_u8(buf, imm));
}

int
emit_amd64_push_reg(emit_buf *buf, int reg)
{
	if (reg < 0 || reg > 15) {
		return (-1);
	}
	if (reg >= 8 && emit_buf_u8(buf, 0x41) != 0) {
		return (-1);
	}
	return (emit_buf_u8(buf, (uint8_t)(0x50 + (reg & 7))));
}

int
emit_amd64_pop_reg(emit_buf *buf, int reg)
{
	if (reg < 0 || reg > 15) {
		return (-1);
	}
	if (reg >= 8 && emit_buf_u8(buf, 0x41) != 0) {
		return (-1);
	}
	return (emit_buf_u8(buf, (uint8_t)(0x58 + (reg & 7))));
}

int
emit_amd64_mov_imm64_reg(emit_buf *buf, uint64_t imm, int reg)
{
	if (reg < 0 || reg > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, 0, reg) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, (uint8_t)(0xb8 + (reg & 7))) != 0) {
		return (-1);
	}
	return (emit_buf_u64(buf, imm));
}

int
emit_amd64_mov_reg_reg(emit_buf *buf, int src, int dst)
{
	if (src < 0 || src > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, src, dst) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x89) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, src, dst));
}

int
emit_amd64_mov_mem32_reg(emit_buf *buf, int base, int32_t disp, int dst)
{
	if (base < 0 || base > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 0, dst, base) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x8b) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm_mem(buf, dst, base, disp));
}

int
emit_amd64_mov_reg_mem32(emit_buf *buf, int src, int base, int32_t disp)
{
	if (src < 0 || src > 15 || base < 0 || base > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 0, src, base) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x89) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm_mem(buf, src, base, disp));
}

int
emit_amd64_movsxd_reg_reg(emit_buf *buf, int src32, int dst64)
{
	if (src32 < 0 || src32 > 15 || dst64 < 0 || dst64 > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, dst64, src32) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x63) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, dst64, src32));
}

int
emit_amd64_mov_rip_reg(emit_buf *buf, int dst)
{
	if (dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, dst, 0) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x8b) != 0 ||
	    emit_amd64_modrm(buf, 0, dst, 5) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, 0));
}

int
emit_amd64_lea_rip_reg(emit_buf *buf, int dst)
{
	if (dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, dst, 0) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x8d) != 0 ||
	    emit_amd64_modrm(buf, 0, dst, 5) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, 0));
}

int
emit_amd64_xor_reg_reg(emit_buf *buf, int src, int dst)
{
	if (src < 0 || src > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, src, dst) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x31) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, src, dst));
}

int
emit_amd64_test_reg_reg(emit_buf *buf, int src, int dst)
{
	if (src < 0 || src > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, src, dst) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x85) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, src, dst));
}

static int
emit_amd64_reg_op_reg(emit_buf *buf, int opcode, int src, int dst)
{
	if (src < 0 || src > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, src, dst) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, (uint8_t)opcode) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, src, dst));
}

int
emit_amd64_add_reg_reg(emit_buf *buf, int src, int dst)
{
	return (emit_amd64_reg_op_reg(buf, 0x01, src, dst));
}

int
emit_amd64_sub_reg_reg(emit_buf *buf, int src, int dst)
{
	return (emit_amd64_reg_op_reg(buf, 0x29, src, dst));
}

int
emit_amd64_cmp_reg_reg(emit_buf *buf, int src, int dst)
{
	return (emit_amd64_reg_op_reg(buf, 0x39, src, dst));
}

int
emit_amd64_imul_reg_reg(emit_buf *buf, int src, int dst)
{
	if (src < 0 || src > 15 || dst < 0 || dst > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, dst, src) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x0f) != 0 || emit_buf_u8(buf, 0xaf) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, dst, src));
}

static int
emit_amd64_shift_imm_reg(emit_buf *buf, int op, uint8_t imm, int reg)
{
	if (reg < 0 || reg > 15 || op < 0 || op > 7) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, 0, reg) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0xc1) != 0 ||
	    emit_amd64_modrm(buf, 3, op, reg) != 0) {
		return (-1);
	}
	return (emit_buf_u8(buf, imm));
}

int
emit_amd64_shl_imm_reg(emit_buf *buf, uint8_t imm, int reg)
{
	return (emit_amd64_shift_imm_reg(buf, 4, imm, reg));
}

int
emit_amd64_sar_imm_reg(emit_buf *buf, uint8_t imm, int reg)
{
	return (emit_amd64_shift_imm_reg(buf, 7, imm, reg));
}

int
emit_amd64_cqto(emit_buf *buf)
{
	uint8_t	op[] = { 0x48, 0x99 };

	return (emit_buf_write(buf, op, sizeof(op)));
}

int
emit_amd64_idiv_reg(emit_buf *buf, int reg)
{
	if (reg < 0 || reg > 15) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, 0, reg) != 0) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0xf7) != 0) {
		return (-1);
	}
	return (emit_amd64_modrm(buf, 3, 7, reg));
}

static int
emit_amd64_imm_op_reg(emit_buf *buf, int op, int32_t imm, int reg)
{
	if (reg < 0 || reg > 15 || op < 0 || op > 7) {
		return (-1);
	}
	if (emit_amd64_rex(buf, 1, 0, reg) != 0) {
		return (-1);
	}
	if (imm >= -128 && imm <= 127) {
		if (emit_buf_u8(buf, 0x83) != 0 ||
		    emit_amd64_modrm(buf, 3, op, reg) != 0) {
			return (-1);
		}
		return (emit_buf_u8(buf, (uint8_t)imm));
	}
	if (emit_buf_u8(buf, 0x81) != 0 ||
	    emit_amd64_modrm(buf, 3, op, reg) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, imm));
}

int
emit_amd64_add_imm_reg(emit_buf *buf, int32_t imm, int reg)
{
	return (emit_amd64_imm_op_reg(buf, 0, imm, reg));
}

int
emit_amd64_sub_imm_reg(emit_buf *buf, int32_t imm, int reg)
{
	return (emit_amd64_imm_op_reg(buf, 5, imm, reg));
}

int
emit_amd64_cmp_imm_reg(emit_buf *buf, int32_t imm, int reg)
{
	return (emit_amd64_imm_op_reg(buf, 7, imm, reg));
}

int
emit_amd64_call_rel32(emit_buf *buf, int32_t rel)
{
	if (emit_buf_u8(buf, 0xe8) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, rel));
}

int
emit_amd64_jmp_rel32(emit_buf *buf, int32_t rel)
{
	if (emit_buf_u8(buf, 0xe9) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, rel));
}

int
emit_amd64_jcc_rel32(emit_buf *buf, uint8_t cc, int32_t rel)
{
	if (cc > 15) {
		return (-1);
	}
	if (emit_buf_u8(buf, 0x0f) != 0 ||
	    emit_buf_u8(buf, (uint8_t)(0x80 + cc)) != 0) {
		return (-1);
	}
	return (emit_buf_i32(buf, rel));
}
