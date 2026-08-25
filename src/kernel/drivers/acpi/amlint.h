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

$define %type aml_state_t as AML method execution state
$define %type aml_stream_t as AML bytecode cursor

$define %func aml_stream_init as procedure with args aml_stream_t *, const u8 *, u32
$define %func aml_stream_remaining as function with args const aml_stream_t *
$define %func aml_stream_u8 as function with args aml_stream_t *, u8 *
$define %func aml_stream_u16 as function with args aml_stream_t *, u16 *
$define %func aml_stream_u32 as function with args aml_stream_t *, u32 *
$define %func aml_stream_u64 as function with args aml_stream_t *, u64 *
$define %func aml_parse_pkglength as function with args aml_stream_t *, u32 *
$define %func aml_parse_field_length as function with args aml_stream_t *, u32 *
$define %func aml_name_lead_valid as function with args u8
$define %func aml_name_char_valid as function with args u8
$define %func aml_parse_namestring as function with args aml_stream_t *, char *, u32
$define %func aml_node_create as function with args aml_node_t *, const char *
$define %func aml_node_lookup as function with args aml_node_t *, const char *
$define %func aml_node_attach as procedure with args aml_node_t *, aml_object_t *
$define %func aml_object_create as function with args aml_object_type_t
$define %func aml_object_clone as function with args aml_object_t *
$define %func aml_object_deref as function with args aml_object_t *
$define %func aml_store as function with args aml_state_t *, aml_object_t *, aml_object_t *
$define %func aml_load_table as function with args const u8 *, u32
$define %func aml_exec_term_list as function with args aml_state_t *, aml_stream_t *
$define %func aml_exec_term_arg as function with args aml_state_t *, aml_stream_t *, aml_object_t **
$define %func aml_exec_opcode as function with args aml_state_t *, aml_stream_t *, u16, aml_object_t **
$define %func aml_exec_named as function with args aml_state_t *, aml_stream_t *, u16
$define %func aml_field_read as function with args aml_state_t *, aml_object_t *, aml_object_t **
$define %func aml_field_write as function with args aml_state_t *, aml_object_t *, aml_object_t *
$define %func aml_buffer_field_read as function with args aml_object_t *, aml_object_t **
$define %func aml_buffer_field_write as function with args aml_object_t *, aml_object_t *
$define %func aml_region_resolve_pci as function with args aml_node_t *, aml_object_t *
$define %func aml_integer_mask as function with args u64
$define %func aml_stall as procedure with args u64

*/

/* !SPACE!

$space %internal aml_stream_init, aml_stream_remaining
$space %internal aml_stream_u8, aml_stream_u16, aml_stream_u32, aml_stream_u64
$space %internal aml_parse_pkglength, aml_parse_field_length
$space %internal aml_name_lead_valid, aml_name_char_valid
$space %internal aml_parse_namestring
$space %internal aml_node_create, aml_node_lookup, aml_node_attach
$space %internal aml_object_create, aml_object_clone, aml_object_deref
$space %internal aml_store, aml_load_table
$space %internal aml_exec_term_list, aml_exec_term_arg
$space %internal aml_exec_opcode, aml_exec_named
$space %internal aml_field_read, aml_field_write
$space %internal aml_buffer_field_read, aml_buffer_field_write
$space %internal aml_region_resolve_pci
$space %internal aml_integer_mask, aml_stall

*/

#ifndef ACPI_AMLINT_H
#define ACPI_AMLINT_H

#include <kernel/drivers/acpi/aml.h>

#define	AML_OK			0
#define	AML_ERR			(-1)
#define	AML_ERR_NOT_FOUND	(-2)
#define	AML_ERR_TYPE		(-3)
#define	AML_ERR_BOUNDS		(-4)
#define	AML_ERR_NOMEM		(-5)
#define	AML_ERR_DEPTH		(-6)
#define	AML_ERR_UNSUPPORTED	(-7)

#define	AML_FLOW_NORMAL		0
#define	AML_FLOW_RETURN		1
#define	AML_FLOW_BREAK		2
#define	AML_FLOW_CONTINUE	3

#define	AML_OP_ZERO		0x00
#define	AML_OP_ONE		0x01
#define	AML_OP_ALIAS		0x06
#define	AML_OP_NAME		0x08
#define	AML_OP_BYTE		0x0A
#define	AML_OP_WORD		0x0B
#define	AML_OP_DWORD		0x0C
#define	AML_OP_STRING		0x0D
#define	AML_OP_QWORD		0x0E
#define	AML_OP_SCOPE		0x10
#define	AML_OP_BUFFER		0x11
#define	AML_OP_PACKAGE		0x12
#define	AML_OP_VAR_PACKAGE	0x13
#define	AML_OP_METHOD		0x14
#define	AML_OP_EXTERNAL		0x15
#define	AML_OP_DUAL_NAME	0x2E
#define	AML_OP_MULTI_NAME	0x2F
#define	AML_OP_EXT_PREFIX	0x5B
#define	AML_OP_ROOT_CHAR	0x5C
#define	AML_OP_PARENT_CHAR	0x5E
#define	AML_OP_LOCAL0		0x60
#define	AML_OP_LOCAL7		0x67
#define	AML_OP_ARG0		0x68
#define	AML_OP_ARG6		0x6E
#define	AML_OP_STORE		0x70
#define	AML_OP_REF_OF		0x71
#define	AML_OP_ADD		0x72
#define	AML_OP_CONCAT		0x73
#define	AML_OP_SUBTRACT		0x74
#define	AML_OP_INCREMENT	0x75
#define	AML_OP_DECREMENT	0x76
#define	AML_OP_MULTIPLY		0x77
#define	AML_OP_DIVIDE		0x78
#define	AML_OP_SHIFT_LEFT	0x79
#define	AML_OP_SHIFT_RIGHT	0x7A
#define	AML_OP_AND		0x7B
#define	AML_OP_NAND		0x7C
#define	AML_OP_OR		0x7D
#define	AML_OP_NOR		0x7E
#define	AML_OP_XOR		0x7F
#define	AML_OP_NOT		0x80
#define	AML_OP_FIND_SET_LEFT	0x81
#define	AML_OP_FIND_SET_RIGHT	0x82
#define	AML_OP_DEREF_OF		0x83
#define	AML_OP_CONCAT_RES	0x84
#define	AML_OP_MOD		0x85
#define	AML_OP_NOTIFY		0x86
#define	AML_OP_SIZE_OF		0x87
#define	AML_OP_INDEX		0x88
#define	AML_OP_MATCH		0x89
#define	AML_OP_CREATE_DWORD	0x8A
#define	AML_OP_CREATE_WORD	0x8B
#define	AML_OP_CREATE_BYTE	0x8C
#define	AML_OP_CREATE_BIT	0x8D
#define	AML_OP_OBJECT_TYPE	0x8E
#define	AML_OP_CREATE_QWORD	0x8F
#define	AML_OP_LAND		0x90
#define	AML_OP_LOR		0x91
#define	AML_OP_LNOT		0x92
#define	AML_OP_LEQUAL		0x93
#define	AML_OP_LGREATER		0x94
#define	AML_OP_LLESS		0x95
#define	AML_OP_TO_BUFFER	0x96
#define	AML_OP_TO_DEC_STRING	0x97
#define	AML_OP_TO_HEX_STRING	0x98
#define	AML_OP_TO_INTEGER	0x99
#define	AML_OP_TO_STRING	0x9C
#define	AML_OP_COPY_OBJECT	0x9D
#define	AML_OP_MID		0x9E
#define	AML_OP_CONTINUE		0x9F
#define	AML_OP_IF		0xA0
#define	AML_OP_ELSE		0xA1
#define	AML_OP_WHILE		0xA2
#define	AML_OP_NOOP		0xA3
#define	AML_OP_RETURN		0xA4
#define	AML_OP_BREAK		0xA5
#define	AML_OP_BREAKPOINT	0xCC
#define	AML_OP_ONES		0xFF

#define	AML_EXT(op)		(0x5B00 | (op))

#define	AML_OP_MUTEX		AML_EXT(0x01)
#define	AML_OP_EVENT		AML_EXT(0x02)
#define	AML_OP_COND_REF_OF	AML_EXT(0x12)
#define	AML_OP_CREATE_FIELD	AML_EXT(0x13)
#define	AML_OP_LOAD_TABLE	AML_EXT(0x1F)
#define	AML_OP_LOAD		AML_EXT(0x20)
#define	AML_OP_STALL		AML_EXT(0x21)
#define	AML_OP_SLEEP		AML_EXT(0x22)
#define	AML_OP_ACQUIRE		AML_EXT(0x23)
#define	AML_OP_SIGNAL		AML_EXT(0x24)
#define	AML_OP_WAIT		AML_EXT(0x25)
#define	AML_OP_RESET		AML_EXT(0x26)
#define	AML_OP_RELEASE		AML_EXT(0x27)
#define	AML_OP_FROM_BCD		AML_EXT(0x28)
#define	AML_OP_TO_BCD		AML_EXT(0x29)
#define	AML_OP_UNLOAD		AML_EXT(0x2A)
#define	AML_OP_REVISION		AML_EXT(0x30)
#define	AML_OP_DEBUG		AML_EXT(0x31)
#define	AML_OP_FATAL		AML_EXT(0x32)
#define	AML_OP_TIMER		AML_EXT(0x33)
#define	AML_OP_REGION		AML_EXT(0x80)
#define	AML_OP_FIELD		AML_EXT(0x81)
#define	AML_OP_DEVICE		AML_EXT(0x82)
#define	AML_OP_PROCESSOR	AML_EXT(0x83)
#define	AML_OP_POWER_RES	AML_EXT(0x84)
#define	AML_OP_THERMAL_ZONE	AML_EXT(0x85)
#define	AML_OP_INDEX_FIELD	AML_EXT(0x86)
#define	AML_OP_BANK_FIELD	AML_EXT(0x87)

typedef struct {
	const u8	*base;
	u32		length;
	u32		offset;
} aml_stream_t;

typedef struct aml_state {
	aml_node_t	*scope;
	aml_object_t	*args[AML_MAX_ARGS];
	aml_object_t	*locals[AML_MAX_LOCALS];
	aml_object_t	*result;
	u32		depth;
	int		flow;
	aml_node_t	*method_node;
} aml_state_t;

void		aml_stream_init(aml_stream_t *stream, const u8 *base,
		    u32 length);
u32		aml_stream_remaining(const aml_stream_t *stream);
int		aml_stream_u8(aml_stream_t *stream, u8 *value);
int		aml_stream_u16(aml_stream_t *stream, u16 *value);
int		aml_stream_u32(aml_stream_t *stream, u32 *value);
int		aml_stream_u64(aml_stream_t *stream, u64 *value);
int		aml_parse_pkglength(aml_stream_t *stream, u32 *length);
int		aml_parse_field_length(aml_stream_t *stream, u32 *bits);
int		aml_parse_namestring(aml_stream_t *stream, char *out, u32 size);
int		aml_parse_opcode(aml_stream_t *stream, u16 *opcode);
int		aml_name_lead_valid(u8 c);
int		aml_name_char_valid(u8 c);

int		aml_namespace_init(void);
aml_node_t	*aml_node_create(aml_node_t *scope, const char *path);
aml_node_t	*aml_node_lookup(aml_node_t *scope, const char *path);
void		aml_node_attach(aml_node_t *node, aml_object_t *object);
aml_node_t	*aml_node_resolve_ref(aml_node_t *node);

aml_object_t	*aml_object_create(aml_object_type_t type);
aml_object_t	*aml_object_clone(aml_object_t *source);
aml_object_t	*aml_object_deref(aml_object_t *object);
int		aml_object_to_integer(aml_object_t *source,
		    aml_object_t **result);
int		aml_object_to_string(aml_object_t *source, int base,
		    aml_object_t **result);
int		aml_object_to_buffer(aml_object_t *source,
		    aml_object_t **result);
int		aml_object_compare(aml_object_t *left, aml_object_t *right,
		    int *relation);

int	aml_store(aml_state_t *state, aml_object_t *value,
	    aml_object_t *target);
int	aml_store_named(aml_state_t *state, aml_object_t *value,
	    aml_node_t *node);
int	aml_load_table(const u8 *aml, u32 length);

int	aml_operand_integer(aml_state_t *state, aml_stream_t *stream,
	    u64 *value);
int	aml_exec_term_list(aml_state_t *state, aml_stream_t *stream);
int	aml_exec_term_arg(aml_state_t *state, aml_stream_t *stream,
	    aml_object_t **result);
int	aml_exec_super_name(aml_state_t *state, aml_stream_t *stream,
	    aml_object_t **result);
int	aml_exec_opcode(aml_state_t *state, aml_stream_t *stream, u16 opcode,
	    aml_object_t **result);
int	aml_exec_named(aml_state_t *state, aml_stream_t *stream, u16 opcode);
int	aml_exec_method(aml_node_t *node, aml_object_t **args, u32 arg_count,
	    aml_object_t **result, u32 depth);

int	aml_field_read(aml_state_t *state, aml_object_t *field,
	    aml_object_t **result);
int	aml_field_write(aml_state_t *state, aml_object_t *field,
	    aml_object_t *value);
int	aml_buffer_field_read(aml_object_t *field, aml_object_t **result);
int	aml_buffer_field_write(aml_object_t *field, aml_object_t *value);
int	aml_region_read(aml_object_t *region, u64 offset, u32 width,
	    u64 *value);
int	aml_region_write(aml_object_t *region, u64 offset, u32 width,
	    u64 value);
int	aml_region_resolve_pci(aml_node_t *node, aml_object_t *region);

u64	aml_integer_mask(u64 value);
void	aml_set_integer_width(int revision);
int	aml_get_integer_width(void);
void	aml_stall(u64 microseconds);
u64	aml_timer_ticks(void);

int	aml_mutex_acquire(aml_object_t *mutex, u16 timeout);
int	aml_mutex_release(aml_object_t *mutex);
void	aml_global_lock_acquire(void);
void	aml_global_lock_release(void);

int	aml_notify_dispatch(aml_node_t *node, u64 value);

#endif

