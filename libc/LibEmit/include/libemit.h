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
$define %type emit_amd64_reg as amd64 general purpose register id
$define %func emit_buf_init as procedure with args emit_buf *
$define %func emit_buf_free as procedure with args emit_buf *
$define %func emit_buf_write as function with args emit_buf *, const void *, size_t
$define %func emit_amd64_reg_parse as function with args const char *, int *
$define %func emit_amd64_mov_imm64_reg as function with args emit_buf *, unsigned long, int
$define %func emit_amd64_mov_reg_reg as function with args emit_buf *, int, int
$define %func emit_amd64_mov_mem32_reg as function with args emit_buf *, int, int32, int
$define %func emit_amd64_mov_reg_mem32 as function with args emit_buf *, int, int, int32
$define %func emit_amd64_lea_rip_reg as function with args emit_buf *, int

*/

/* !SPACE!

$space %export emit_buf, emit_buf_init, emit_buf_free
$space %export emit_buf_reserve, emit_buf_write, emit_buf_write_at
$space %export emit_buf_u8, emit_buf_u16, emit_buf_u32, emit_buf_u64
$space %export emit_buf_i32, emit_buf_align
$space %export emit_amd64_reg_parse, emit_amd64_ret, emit_amd64_syscall
$space %export emit_amd64_nop, emit_amd64_hlt, emit_amd64_cli, emit_amd64_sti
$space %export emit_amd64_iretq, emit_amd64_int_imm8
$space %export emit_amd64_push_reg, emit_amd64_pop_reg
$space %export emit_amd64_mov_imm64_reg, emit_amd64_mov_reg_reg
$space %export emit_amd64_reg32_parse, emit_amd64_mov_mem32_reg
$space %export emit_amd64_mov_reg_mem32
$space %export emit_amd64_mov_rip_reg, emit_amd64_lea_rip_reg
$space %export emit_amd64_xor_reg_reg, emit_amd64_test_reg_reg
$space %export emit_amd64_add_imm_reg, emit_amd64_sub_imm_reg
$space %export emit_amd64_cmp_imm_reg, emit_amd64_call_rel32
$space %export emit_amd64_jmp_rel32

*/

#ifndef LIBEMIT_H
#define LIBEMIT_H

#include <stddef.h>
#include <stdint.h>

typedef struct emit_buf {
	uint8_t	*data;
	size_t	size;
	size_t	capacity;
} emit_buf;

enum {
	EMIT_AMD64_RAX = 0,
	EMIT_AMD64_RCX = 1,
	EMIT_AMD64_RDX = 2,
	EMIT_AMD64_RBX = 3,
	EMIT_AMD64_RSP = 4,
	EMIT_AMD64_RBP = 5,
	EMIT_AMD64_RSI = 6,
	EMIT_AMD64_RDI = 7,
	EMIT_AMD64_R8 = 8,
	EMIT_AMD64_R9 = 9,
	EMIT_AMD64_R10 = 10,
	EMIT_AMD64_R11 = 11,
	EMIT_AMD64_R12 = 12,
	EMIT_AMD64_R13 = 13,
	EMIT_AMD64_R14 = 14,
	EMIT_AMD64_R15 = 15
};

void	emit_buf_init(emit_buf *buf);
void	emit_buf_free(emit_buf *buf);
int	emit_buf_reserve(emit_buf *buf, size_t extra);
int	emit_buf_write(emit_buf *buf, const void *data, size_t size);
int	emit_buf_write_at(emit_buf *buf, size_t off, const void *data,
	    size_t size);
int	emit_buf_u8(emit_buf *buf, uint8_t value);
int	emit_buf_u16(emit_buf *buf, uint16_t value);
int	emit_buf_u32(emit_buf *buf, uint32_t value);
int	emit_buf_u64(emit_buf *buf, uint64_t value);
int	emit_buf_i32(emit_buf *buf, int32_t value);
int	emit_buf_align(emit_buf *buf, size_t align, uint8_t fill);

int	emit_amd64_reg_parse(const char *name, int *out_reg);
int	emit_amd64_reg32_parse(const char *name, int *out_reg);
int	emit_amd64_ret(emit_buf *buf);
int	emit_amd64_syscall(emit_buf *buf);
int	emit_amd64_nop(emit_buf *buf);
int	emit_amd64_hlt(emit_buf *buf);
int	emit_amd64_cli(emit_buf *buf);
int	emit_amd64_sti(emit_buf *buf);
int	emit_amd64_iretq(emit_buf *buf);
int	emit_amd64_int_imm8(emit_buf *buf, uint8_t imm);
int	emit_amd64_push_reg(emit_buf *buf, int reg);
int	emit_amd64_pop_reg(emit_buf *buf, int reg);
int	emit_amd64_mov_imm64_reg(emit_buf *buf, uint64_t imm, int reg);
int	emit_amd64_mov_reg_reg(emit_buf *buf, int src, int dst);
int	emit_amd64_mov_mem32_reg(emit_buf *buf, int base, int32_t disp,
	    int dst);
int	emit_amd64_mov_reg_mem32(emit_buf *buf, int src, int base,
	    int32_t disp);
int	emit_amd64_mov_rip_reg(emit_buf *buf, int dst);
int	emit_amd64_lea_rip_reg(emit_buf *buf, int dst);
int	emit_amd64_xor_reg_reg(emit_buf *buf, int src, int dst);
int	emit_amd64_test_reg_reg(emit_buf *buf, int src, int dst);
int	emit_amd64_add_imm_reg(emit_buf *buf, int32_t imm, int reg);
int	emit_amd64_sub_imm_reg(emit_buf *buf, int32_t imm, int reg);
int	emit_amd64_cmp_imm_reg(emit_buf *buf, int32_t imm, int reg);
int	emit_amd64_call_rel32(emit_buf *buf, int32_t rel);
int	emit_amd64_jmp_rel32(emit_buf *buf, int32_t rel);

#endif
