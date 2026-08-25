/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type aml_stream_t as AML bytecode cursor

$define %func aml_stream_init as procedure with args aml_stream_t *, const u8 *, u32
$define %func aml_stream_remaining as function with args const aml_stream_t *
$define %func aml_stream_u8 as function with args aml_stream_t *, u8 *
$define %func aml_stream_u16 as function with args aml_stream_t *, u16 *
$define %func aml_stream_u32 as function with args aml_stream_t *, u32 *
$define %func aml_stream_u64 as function with args aml_stream_t *, u64 *
$define %func aml_parse_pkglength_raw as function with args aml_stream_t *, u32 *, u32 *
$define %func aml_parse_pkglength as function with args aml_stream_t *, u32 *
$define %func aml_parse_field_length as function with args aml_stream_t *, u32 *
$define %func aml_parse_namestring as function with args aml_stream_t *, char *, u32
$define %func aml_parse_opcode as function with args aml_stream_t *, u16 *
$define %func aml_name_lead_valid as function with args u8
$define %func aml_name_char_valid as function with args u8

*/

/* !SPACE!

$space %internal aml_parse_pkglength_raw
$space %export aml_stream_init, aml_stream_remaining
$space %export aml_stream_u8, aml_stream_u16, aml_stream_u32, aml_stream_u64
$space %export aml_parse_pkglength, aml_parse_field_length
$space %export aml_parse_namestring, aml_parse_opcode
$space %export aml_name_lead_valid, aml_name_char_valid

*/

#include <kernel/drivers/acpi/amlint.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

void
aml_stream_init(aml_stream_t *stream, const u8 *base, u32 length)
{
	stream->base = base;
	stream->length = length;
	stream->offset = 0;
}

u32
aml_stream_remaining(const aml_stream_t *stream)
{
	if (stream->offset >= stream->length) {
		return (0);
	}
	return (stream->length - stream->offset);
}

int
aml_stream_u8(aml_stream_t *stream, u8 *value)
{
	if (aml_stream_remaining(stream) < 1) {
		return (AML_ERR_BOUNDS);
	}
	*value = stream->base[stream->offset++];
	return (AML_OK);
}

int
aml_stream_u16(aml_stream_t *stream, u16 *value)
{
	if (aml_stream_remaining(stream) < 2) {
		return (AML_ERR_BOUNDS);
	}
	*value = (u16)stream->base[stream->offset] |
	    ((u16)stream->base[stream->offset + 1] << 8);
	stream->offset += 2;
	return (AML_OK);
}

int
aml_stream_u32(aml_stream_t *stream, u32 *value)
{
	u32	i;
	u32	result;

	if (aml_stream_remaining(stream) < 4) {
		return (AML_ERR_BOUNDS);
	}
	result = 0;
	for (i = 0; i < 4; i++) {
		result |= (u32)stream->base[stream->offset + i] << (i * 8);
	}
	stream->offset += 4;
	*value = result;
	return (AML_OK);
}

int
aml_stream_u64(aml_stream_t *stream, u64 *value)
{
	u32	i;
	u64	result;

	if (aml_stream_remaining(stream) < 8) {
		return (AML_ERR_BOUNDS);
	}
	result = 0;
	for (i = 0; i < 8; i++) {
		result |= (u64)stream->base[stream->offset + i] << (i * 8);
	}
	stream->offset += 8;
	*value = result;
	return (AML_OK);
}

int
aml_parse_opcode(aml_stream_t *stream, u16 *opcode)
{
	u8	first;
	u8	second;

	if (aml_stream_u8(stream, &first) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (first != AML_OP_EXT_PREFIX) {
		*opcode = first;
		return (AML_OK);
	}
	if (aml_stream_u8(stream, &second) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	*opcode = AML_EXT(second);
	return (AML_OK);
}

static int
aml_parse_pkglength_raw(aml_stream_t *stream, u32 *value, u32 *consumed)
{
	u32	total;
	u32	i;
	u8	lead;
	u8	extra;
	u8	follow;

	if (aml_stream_u8(stream, &lead) != AML_OK) {
		drivers_log("aml: pkglength: no lead byte at +0x%x\n",
		    stream->offset);
		return (AML_ERR_BOUNDS);
	}
	follow = (u8)(lead >> 6);
	if (follow == 0) {
		total = lead & 0x3F;
	} else {
		total = lead & 0x0F;
		for (i = 0; i < follow; i++) {
			if (aml_stream_u8(stream, &extra) != AML_OK) {
				drivers_log("aml: pkglength: follow byte "
				    "%u/%u missing at +0x%x (lead 0x%x)\n",
				    i + 1, follow, stream->offset, lead);
				return (AML_ERR_BOUNDS);
			}
			total |= (u32)extra << (4 + i * 8);
		}
	}
	*value = total;
	if (consumed != NULL) {
		*consumed = 1U + follow;
	}
	return (AML_OK);
}

int
aml_parse_pkglength(aml_stream_t *stream, u32 *length)
{
	u32	total;
	u32	consumed;

	if (aml_parse_pkglength_raw(stream, &total, &consumed) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (total < consumed) {
		drivers_log("aml: pkglength: value %u < consumed %u at "
		    "+0x%x\n", total, consumed, stream->offset);
		return (AML_ERR_BOUNDS);
	}
	total -= consumed;
	if (total > aml_stream_remaining(stream)) {
		drivers_log("aml: pkglength: body %u > remaining %u at "
		    "+0x%x\n", total, aml_stream_remaining(stream),
		    stream->offset);
		return (AML_ERR_BOUNDS);
	}
	*length = total;
	return (AML_OK);
}

int
aml_parse_field_length(aml_stream_t *stream, u32 *bits)
{
	return (aml_parse_pkglength_raw(stream, bits, NULL));
}

int
aml_name_lead_valid(u8 c)
{
	return ((c >= 'A' && c <= 'Z') || c == '_');
}

int
aml_name_char_valid(u8 c)
{
	return (aml_name_lead_valid(c) || (c >= '0' && c <= '9'));
}

int
aml_parse_namestring(aml_stream_t *stream, char *out, u32 size)
{
	u32	used;
	u32	segments;
	u32	i;
	u32	j;
	u8	byte;
	u8	count;

	if (out == NULL || size < 2) {
		return (AML_ERR);
	}
	used = 0;
	if (aml_stream_remaining(stream) == 0) {
		return (AML_ERR_BOUNDS);
	}
	if (stream->base[stream->offset] == AML_OP_ROOT_CHAR) {
		stream->offset++;
		out[used++] = '\\';
	} else {
		while (aml_stream_remaining(stream) != 0 &&
		    stream->base[stream->offset] == AML_OP_PARENT_CHAR) {
			stream->offset++;
			if (used + 1 >= size) {
				return (AML_ERR_BOUNDS);
			}
			out[used++] = '^';
		}
	}
	if (aml_stream_u8(stream, &byte) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (byte == 0x00) {
		out[used] = '\0';
		return (AML_OK);
	}
	if (byte == AML_OP_DUAL_NAME) {
		segments = 2;
	} else if (byte == AML_OP_MULTI_NAME) {
		if (aml_stream_u8(stream, &count) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		if (count == 0) {
			out[used] = '\0';
			return (AML_OK);
		}
		segments = count;
	} else {
		if (!aml_name_lead_valid(byte)) {
			return (AML_ERR);
		}
		stream->offset--;
		segments = 1;
	}
	for (i = 0; i < segments; i++) {
		if (aml_stream_remaining(stream) < AML_NAME_LENGTH) {
			return (AML_ERR_BOUNDS);
		}
		if (used + AML_NAME_LENGTH + 2 > size) {
			return (AML_ERR_BOUNDS);
		}
		if (i != 0) {
			out[used++] = '.';
		}
		for (j = 0; j < AML_NAME_LENGTH; j++) {
			byte = stream->base[stream->offset + j];
			if (j == 0) {
				if (!aml_name_lead_valid(byte)) {
					return (AML_ERR);
				}
			} else if (!aml_name_char_valid(byte)) {
				return (AML_ERR);
			}
			out[used++] = (char)byte;
		}
		stream->offset += AML_NAME_LENGTH;
	}
	out[used] = '\0';
	return (AML_OK);
}

