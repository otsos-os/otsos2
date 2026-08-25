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
$define %type aml_state_t as AML method execution state
$define %type aml_stream_t as AML bytecode cursor

$define %func aml_exec_opcode as function with args aml_state_t *, aml_stream_t *, u16, aml_object_t **
$define %func aml_mutex_acquire as function with args aml_object_t *, u16
$define %func aml_mutex_release as function with args aml_object_t *
$define %func aml_global_lock_acquire as procedure with args void
$define %func aml_global_lock_release as procedure with args void
$define %func aml_stall as procedure with args u64
$define %func aml_timer_ticks as function with args void
$define %func aml_notify_dispatch as function with args aml_node_t *, u64

*/

/* !SPACE!

$space %internal aml_op_binary, aml_op_unary, aml_op_logical
$space %internal aml_op_if, aml_op_while, aml_op_index, aml_op_match
$space %internal aml_op_concat, aml_op_mid, aml_op_size_of
$space %internal aml_write_target, aml_bit_scan
$space %export aml_exec_opcode
$space %export aml_mutex_acquire, aml_mutex_release
$space %export aml_global_lock_acquire, aml_global_lock_release
$space %export aml_stall, aml_timer_ticks, aml_notify_dispatch

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static u32	aml_global_lock_depth = 0;

void
aml_global_lock_acquire(void)
{
	aml_global_lock_depth++;
}

void
aml_global_lock_release(void)
{
	if (aml_global_lock_depth != 0) {
		aml_global_lock_depth--;
	}
}

void
aml_stall(u64 microseconds)
{
	u64	spins;
	u64	i;

	if (microseconds > 1000000ULL) {
		microseconds = 1000000ULL;
	}
	spins = microseconds * 200ULL;
	for (i = 0; i < spins; i++) {
		__asm__ volatile("pause");
	}
}

u64
aml_timer_ticks(void)
{
	u32	low;
	u32	high;

	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return (((u64)high << 32) | low);
}

int
aml_mutex_acquire(aml_object_t *mutex, u16 timeout)
{
	(void)timeout;
	if (mutex == NULL || mutex->type != AML_TYPE_MUTEX) {
		return (AML_ERR_TYPE);
	}
	mutex->u.mutex.owner_depth++;
	return (AML_OK);
}

int
aml_mutex_release(aml_object_t *mutex)
{
	if (mutex == NULL || mutex->type != AML_TYPE_MUTEX) {
		return (AML_ERR_TYPE);
	}
	if (mutex->u.mutex.owner_depth != 0) {
		mutex->u.mutex.owner_depth--;
	}
	return (AML_OK);
}

int
aml_notify_dispatch(aml_node_t *node, u64 value)
{
	char	path[AML_MAX_PATH];

	if (node == NULL) {
		return (AML_ERR);
	}
	if (aml_node_path(node, path, sizeof(path)) != AML_OK) {
		return (AML_OK);
	}
	drivers_log("[AML] notify %s value 0x%llx\n", path,
	    (unsigned long long)value);
	return (AML_OK);
}

static int
aml_write_target(aml_state_t *state, aml_stream_t *stream, aml_object_t *value)
{
	aml_object_t	*target;
	int		status;

	target = NULL;
	status = aml_exec_super_name(state, stream, &target);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_store(state, value, target);
	aml_object_unref(target);
	return (status);
}

static int
aml_op_binary(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*object;
	u64		left;
	u64		right;
	u64		value;
	int		status;

	status = aml_operand_integer(state, stream, &left);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &right);
	if (status != AML_OK) {
		return (status);
	}
	switch (opcode) {
	case AML_OP_ADD:
		value = left + right;
		break;
	case AML_OP_SUBTRACT:
		value = left - right;
		break;
	case AML_OP_MULTIPLY:
		value = left * right;
		break;
	case AML_OP_MOD:
		if (right == 0) {
			return (AML_ERR);
		}
		value = left % right;
		break;
	case AML_OP_SHIFT_LEFT:
		value = (right >= 64) ? 0 : (left << right);
		break;
	case AML_OP_SHIFT_RIGHT:
		value = (right >= 64) ? 0 : (left >> right);
		break;
	case AML_OP_AND:
		value = left & right;
		break;
	case AML_OP_NAND:
		value = ~(left & right);
		break;
	case AML_OP_OR:
		value = left | right;
		break;
	case AML_OP_NOR:
		value = ~(left | right);
		break;
	case AML_OP_XOR:
		value = left ^ right;
		break;
	default:
		return (AML_ERR);
	}
	object = aml_integer_create(value);
	if (object == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_write_target(state, stream, object);
	if (status != AML_OK) {
		aml_object_unref(object);
		return (status);
	}
	*result = object;
	return (AML_OK);
}

static int
aml_op_divide(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*remainder;
	aml_object_t	*quotient;
	u64		dividend;
	u64		divisor;
	int		status;

	status = aml_operand_integer(state, stream, &dividend);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &divisor);
	if (status != AML_OK) {
		return (status);
	}
	if (divisor == 0) {
		return (AML_ERR);
	}
	remainder = aml_integer_create(dividend % divisor);
	if (remainder == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_write_target(state, stream, remainder);
	aml_object_unref(remainder);
	if (status != AML_OK) {
		return (status);
	}
	quotient = aml_integer_create(dividend / divisor);
	if (quotient == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_write_target(state, stream, quotient);
	if (status != AML_OK) {
		aml_object_unref(quotient);
		return (status);
	}
	*result = quotient;
	return (AML_OK);
}

static u64
aml_bit_scan(u64 value, int from_left)
{
	u32	i;

	if (value == 0) {
		return (0);
	}
	if (from_left) {
		for (i = 64; i > 0; i--) {
			if ((value & (1ULL << (i - 1))) != 0) {
				return (i);
			}
		}
		return (0);
	}
	for (i = 0; i < 64; i++) {
		if ((value & (1ULL << i)) != 0) {
			return (i + 1);
		}
	}
	return (0);
}

static int
aml_op_unary(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*object;
	u64		operand;
	u64		value;
	u32		i;
	int		status;

	status = aml_operand_integer(state, stream, &operand);
	if (status != AML_OK) {
		return (status);
	}
	switch (opcode) {
	case AML_OP_NOT:
		value = ~operand;
		break;
	case AML_OP_FIND_SET_LEFT:
		value = aml_bit_scan(operand, 1);
		break;
	case AML_OP_FIND_SET_RIGHT:
		value = aml_bit_scan(operand, 0);
		break;
	case AML_OP_TO_BCD:
		value = 0;
		for (i = 0; i < 16 && operand != 0; i++) {
			value |= (operand % 10ULL) << (i * 4);
			operand /= 10ULL;
		}
		break;
	case AML_OP_FROM_BCD:
		value = 0;
		for (i = 16; i > 0; i--) {
			value = value * 10ULL +
			    ((operand >> ((i - 1) * 4)) & 0x0F);
		}
		break;
	default:
		return (AML_ERR);
	}
	object = aml_integer_create(value);
	if (object == NULL) {
		return (AML_ERR_NOMEM);
	}
	status = aml_write_target(state, stream, object);
	if (status != AML_OK) {
		aml_object_unref(object);
		return (status);
	}
	*result = object;
	return (AML_OK);
}

static int
aml_op_step(aml_state_t *state, aml_stream_t *stream, int delta,
    aml_object_t **result)
{
	aml_object_t	*target;
	aml_object_t	*current;
	aml_object_t	*object;
	u64		value;
	int		status;

	target = NULL;
	status = aml_exec_super_name(state, stream, &target);
	if (status != AML_OK) {
		return (status);
	}
	current = NULL;
	if (target != NULL && target->type == AML_TYPE_REFERENCE &&
	    target->u.reference.kind == AML_REF_NAMED &&
	    target->u.reference.node != NULL) {
		status = aml_evaluate(target->u.reference.node, NULL, 0,
		    &current);
	} else if (target != NULL && target->type == AML_TYPE_REFERENCE &&
	    target->u.reference.kind == AML_REF_LOCAL) {
		current = state->locals[target->u.reference.index];
		aml_object_ref(current);
	} else if (target != NULL && target->type == AML_TYPE_REFERENCE &&
	    target->u.reference.kind == AML_REF_ARG) {
		current = state->args[target->u.reference.index];
		aml_object_ref(current);
	} else {
		status = AML_ERR_TYPE;
	}
	if (status != AML_OK) {
		aml_object_unref(target);
		return (status);
	}
	status = aml_object_as_integer(current, &value);
	aml_object_unref(current);
	if (status != AML_OK) {
		aml_object_unref(target);
		return (status);
	}
	value = (delta > 0) ? (value + 1) : (value - 1);
	object = aml_integer_create(value);
	if (object == NULL) {
		aml_object_unref(target);
		return (AML_ERR_NOMEM);
	}
	status = aml_store(state, object, target);
	aml_object_unref(target);
	if (status != AML_OK) {
		aml_object_unref(object);
		return (status);
	}
	*result = object;
	return (AML_OK);
}

static int
aml_op_logical(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*left;
	aml_object_t	*right;
	u64		lvalue;
	u64		rvalue;
	int		relation;
	int		outcome;
	int		status;

	if (opcode == AML_OP_LNOT) {
		status = aml_operand_integer(state, stream, &lvalue);
		if (status != AML_OK) {
			return (status);
		}
		*result = aml_integer_create((lvalue == 0) ?
		    aml_integer_mask(~0ULL) : 0);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	if (opcode == AML_OP_LAND || opcode == AML_OP_LOR) {
		status = aml_operand_integer(state, stream, &lvalue);
		if (status != AML_OK) {
			return (status);
		}
		status = aml_operand_integer(state, stream, &rvalue);
		if (status != AML_OK) {
			return (status);
		}
		outcome = (opcode == AML_OP_LAND) ?
		    (lvalue != 0 && rvalue != 0) : (lvalue != 0 || rvalue != 0);
		*result = aml_integer_create(outcome ?
		    aml_integer_mask(~0ULL) : 0);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	left = NULL;
	status = aml_exec_term_arg(state, stream, &left);
	if (status != AML_OK) {
		return (status);
	}
	right = NULL;
	status = aml_exec_term_arg(state, stream, &right);
	if (status != AML_OK) {
		aml_object_unref(left);
		return (status);
	}
	status = aml_object_compare(left, right, &relation);
	aml_object_unref(left);
	aml_object_unref(right);
	if (status != AML_OK) {
		return (status);
	}
	switch (opcode) {
	case AML_OP_LEQUAL:
		outcome = (relation == 0);
		break;
	case AML_OP_LGREATER:
		outcome = (relation > 0);
		break;
	case AML_OP_LLESS:
		outcome = (relation < 0);
		break;
	default:
		return (AML_ERR);
	}
	*result = aml_integer_create(outcome ? aml_integer_mask(~0ULL) : 0);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static int
aml_op_if(aml_state_t *state, aml_stream_t *stream)
{
	aml_stream_t	body;
	u64		predicate;
	u32		length;
	u32		initial;
	u32		consumed;
	int		status;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	status = aml_operand_integer(state, stream, &predicate);
	if (status != AML_OK) {
		return (status);
	}
	if (stream->offset < initial) {
		return (AML_ERR_BOUNDS);
	}
	consumed = stream->offset - initial;
	if (consumed > length) {
		return (AML_ERR_BOUNDS);
	}
	aml_stream_init(&body, stream->base + stream->offset,
	    length - consumed);
	stream->offset = initial + length;
	if (predicate != 0) {
		state->depth++;
		status = aml_exec_term_list(state, &body);
		state->depth--;
		if (status != AML_OK) {
			return (status);
		}
		if (aml_stream_remaining(stream) != 0 &&
		    stream->base[stream->offset] == AML_OP_ELSE) {
			stream->offset++;
			if (aml_parse_pkglength(stream, &length) != AML_OK) {
				return (AML_ERR_BOUNDS);
			}
			stream->offset += length;
		}
		return (AML_OK);
	}
	if (aml_stream_remaining(stream) != 0 &&
	    stream->base[stream->offset] == AML_OP_ELSE) {
		stream->offset++;
		if (aml_parse_pkglength(stream, &length) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		aml_stream_init(&body, stream->base + stream->offset, length);
		stream->offset += length;
		state->depth++;
		status = aml_exec_term_list(state, &body);
		state->depth--;
		return (status);
	}
	return (AML_OK);
}

#define	AML_WHILE_LIMIT		1000000U

static int
aml_op_while(aml_state_t *state, aml_stream_t *stream)
{
	aml_stream_t	predicate_stream;
	aml_stream_t	body;
	u64		predicate;
	u32		length;
	u32		initial;
	u32		consumed;
	u32		iterations;
	int		status;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (length > aml_stream_remaining(stream)) {
		return (AML_ERR_BOUNDS);
	}
	stream->offset = initial + length;
	for (iterations = 0; iterations < AML_WHILE_LIMIT; iterations++) {
		aml_stream_init(&predicate_stream, stream->base + initial,
		    length);
		status = aml_operand_integer(state, &predicate_stream, &predicate);
		if (status != AML_OK) {
			return (status);
		}
		if (predicate == 0) {
			return (AML_OK);
		}
		consumed = predicate_stream.offset;
		if (consumed > length) {
			return (AML_ERR_BOUNDS);
		}
		aml_stream_init(&body, stream->base + initial + consumed,
		    length - consumed);
		state->depth++;
		status = aml_exec_term_list(state, &body);
		state->depth--;
		if (status != AML_OK) {
			return (status);
		}
		if (state->flow == AML_FLOW_BREAK) {
			state->flow = AML_FLOW_NORMAL;
			return (AML_OK);
		}
		if (state->flow == AML_FLOW_CONTINUE) {
			state->flow = AML_FLOW_NORMAL;
			continue;
		}
		if (state->flow == AML_FLOW_RETURN) {
			return (AML_OK);
		}
	}
	return (AML_ERR_DEPTH);
}

static int
aml_op_index(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_t	*reference;
	u64		index;
	u32		limit;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &index);
	if (status != AML_OK) {
		aml_object_unref(source);
		return (status);
	}
	source = aml_object_deref(source);
	if (source == NULL) {
		return (AML_ERR);
	}
	reference = aml_object_create(AML_TYPE_REFERENCE);
	if (reference == NULL) {
		return (AML_ERR_NOMEM);
	}
	switch (source->type) {
	case AML_TYPE_PACKAGE:
		reference->u.reference.kind = AML_REF_INDEX_PACKAGE;
		limit = source->u.package.count;
		break;
	case AML_TYPE_BUFFER:
		reference->u.reference.kind = AML_REF_INDEX_BUFFER;
		limit = source->u.buffer.length;
		break;
	case AML_TYPE_STRING:
		reference->u.reference.kind = AML_REF_INDEX_STRING;
		limit = source->u.string.length;
		break;
	default:
		aml_object_unref(reference);
		return (AML_ERR_TYPE);
	}
	if (index >= limit) {
		aml_object_unref(reference);
		return (AML_ERR_BOUNDS);
	}
	aml_object_ref(source);
	reference->u.reference.container = source;
	reference->u.reference.index = (u32)index;
	status = aml_write_target(state, stream, reference);
	if (status != AML_OK) {
		aml_object_unref(reference);
		return (status);
	}
	*result = reference;
	return (AML_OK);
}

static int
aml_op_deref(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_t	*container;
	aml_object_t	*value;
	u32		index;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	if (source->type != AML_TYPE_REFERENCE) {
		*result = source;
		return (AML_OK);
	}
	container = source->u.reference.container;
	index = source->u.reference.index;
	value = NULL;
	switch (source->u.reference.kind) {
	case AML_REF_NAMED:
		status = aml_evaluate(source->u.reference.node, NULL, 0, &value);
		break;
	case AML_REF_LOCAL:
		value = state->locals[index];
		aml_object_ref(value);
		break;
	case AML_REF_ARG:
		value = state->args[index];
		aml_object_ref(value);
		break;
	case AML_REF_INDEX_PACKAGE:
		if (container == NULL || index >= container->u.package.count) {
			status = AML_ERR_BOUNDS;
			break;
		}
		value = container->u.package.elements[index];
		aml_object_ref(value);
		break;
	case AML_REF_INDEX_BUFFER:
		if (container == NULL || index >= container->u.buffer.length) {
			status = AML_ERR_BOUNDS;
			break;
		}
		value = aml_integer_create(container->u.buffer.data[index]);
		break;
	case AML_REF_INDEX_STRING:
		if (container == NULL || index >= container->u.string.length) {
			status = AML_ERR_BOUNDS;
			break;
		}
		value = aml_integer_create(
		    (u8)container->u.string.data[index]);
		break;
	default:
		status = AML_ERR_UNSUPPORTED;
		break;
	}
	aml_object_unref(source);
	if (status != AML_OK) {
		return (status);
	}
	if (value == NULL) {
		return (AML_ERR);
	}
	*result = value;
	return (AML_OK);
}

static int
aml_op_size_of(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*source;
	u64		size;
	int		status;

	source = NULL;
	status = aml_exec_super_name(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	if (source != NULL && source->type == AML_TYPE_REFERENCE &&
	    source->u.reference.kind == AML_REF_NAMED) {
		aml_object_t	*value;

		value = NULL;
		status = aml_evaluate(source->u.reference.node, NULL, 0, &value);
		aml_object_unref(source);
		if (status != AML_OK) {
			return (status);
		}
		source = value;
	}
	source = aml_object_deref(source);
	if (source == NULL) {
		return (AML_ERR);
	}
	switch (source->type) {
	case AML_TYPE_BUFFER:
		size = source->u.buffer.length;
		break;
	case AML_TYPE_STRING:
		size = source->u.string.length;
		break;
	case AML_TYPE_PACKAGE:
		size = source->u.package.count;
		break;
	default:
		return (AML_ERR_TYPE);
	}
	*result = aml_integer_create(size);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static int
aml_op_object_type(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_type_t	type;
	int		status;

	source = NULL;
	status = aml_exec_super_name(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	type = AML_TYPE_UNINITIALIZED;
	if (source != NULL && source->type == AML_TYPE_REFERENCE &&
	    source->u.reference.kind == AML_REF_NAMED &&
	    source->u.reference.node != NULL &&
	    source->u.reference.node->object != NULL) {
		type = source->u.reference.node->object->type;
	} else if (source != NULL) {
		type = source->type;
	}
	aml_object_unref(source);
	*result = aml_integer_create((u64)type);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static int
aml_op_ref_of(aml_state_t *state, aml_stream_t *stream, int conditional,
    aml_object_t **result)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*reference;
	aml_node_t	*node;
	u32		save;
	u8		peek;
	int		status;

	if (aml_stream_remaining(stream) == 0) {
		return (AML_ERR_BOUNDS);
	}
	peek = stream->base[stream->offset];
	node = NULL;
	if (peek == AML_OP_ROOT_CHAR || peek == AML_OP_PARENT_CHAR ||
	    peek == AML_OP_DUAL_NAME || peek == AML_OP_MULTI_NAME ||
	    (peek >= 'A' && peek <= 'Z') || peek == '_') {
		save = stream->offset;
		if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
			stream->offset = save;
			return (AML_ERR);
		}
		node = aml_resolve(state->scope, path);
	} else {
		reference = NULL;
		status = aml_exec_super_name(state, stream, &reference);
		if (status != AML_OK) {
			return (status);
		}
		if (conditional) {
			status = aml_write_target(state, stream, reference);
			aml_object_unref(reference);
			if (status != AML_OK) {
				return (status);
			}
			*result = aml_integer_create(aml_integer_mask(~0ULL));
			return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
		}
		*result = reference;
		return (AML_OK);
	}
	if (node == NULL) {
		if (!conditional) {
			return (AML_ERR_NOT_FOUND);
		}
		reference = aml_integer_create(0);
		if (reference == NULL) {
			return (AML_ERR_NOMEM);
		}
		if (aml_exec_super_name(state, stream, result) != AML_OK) {
			aml_object_unref(reference);
			return (AML_ERR);
		}
		aml_object_unref(*result);
		*result = reference;
		return (AML_OK);
	}
	reference = aml_object_create(AML_TYPE_REFERENCE);
	if (reference == NULL) {
		return (AML_ERR_NOMEM);
	}
	reference->u.reference.kind = AML_REF_NAMED;
	reference->u.reference.node = node;
	if (!conditional) {
		*result = reference;
		return (AML_OK);
	}
	status = aml_write_target(state, stream, reference);
	aml_object_unref(reference);
	if (status != AML_OK) {
		return (status);
	}
	*result = aml_integer_create(aml_integer_mask(~0ULL));
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static int
aml_op_concat(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*left;
	aml_object_t	*right;
	aml_object_t	*lconv;
	aml_object_t	*rconv;
	aml_object_t	*output;
	u32		total;
	int		status;

	left = NULL;
	status = aml_exec_term_arg(state, stream, &left);
	if (status != AML_OK) {
		return (status);
	}
	right = NULL;
	status = aml_exec_term_arg(state, stream, &right);
	if (status != AML_OK) {
		aml_object_unref(left);
		return (status);
	}
	left = aml_object_deref(left);
	right = aml_object_deref(right);
	if (left == NULL || right == NULL) {
		return (AML_ERR);
	}
	lconv = NULL;
	rconv = NULL;
	if (left->type == AML_TYPE_STRING) {
		status = aml_object_to_string(right, 16, &rconv);
		if (status != AML_OK) {
			return (status);
		}
		total = left->u.string.length + rconv->u.string.length;
		output = aml_object_create(AML_TYPE_STRING);
		if (output == NULL) {
			aml_object_unref(rconv);
			return (AML_ERR_NOMEM);
		}
		output->u.string.data = kmem_alloc(total + 1);
		if (output->u.string.data == NULL) {
			aml_object_unref(output);
			aml_object_unref(rconv);
			return (AML_ERR_NOMEM);
		}
		memcpy(output->u.string.data, left->u.string.data,
		    left->u.string.length);
		memcpy(&output->u.string.data[left->u.string.length],
		    rconv->u.string.data, rconv->u.string.length);
		output->u.string.data[total] = '\0';
		output->u.string.length = total;
		aml_object_unref(rconv);
	} else {
		status = aml_object_to_buffer(left, &lconv);
		if (status != AML_OK) {
			return (status);
		}
		status = aml_object_to_buffer(right, &rconv);
		if (status != AML_OK) {
			aml_object_unref(lconv);
			return (status);
		}
		total = lconv->u.buffer.length + rconv->u.buffer.length;
		output = aml_buffer_create(total);
		if (output == NULL) {
			aml_object_unref(lconv);
			aml_object_unref(rconv);
			return (AML_ERR_NOMEM);
		}
		memcpy(output->u.buffer.data, lconv->u.buffer.data,
		    lconv->u.buffer.length);
		memcpy(&output->u.buffer.data[lconv->u.buffer.length],
		    rconv->u.buffer.data, rconv->u.buffer.length);
		aml_object_unref(lconv);
		aml_object_unref(rconv);
	}
	status = aml_write_target(state, stream, output);
	if (status != AML_OK) {
		aml_object_unref(output);
		return (status);
	}
	*result = output;
	return (AML_OK);
}

static int
aml_op_mid(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_t	*output;
	u64		index;
	u64		length;
	u32		available;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &index);
	if (status != AML_OK) {
		aml_object_unref(source);
		return (status);
	}
	status = aml_operand_integer(state, stream, &length);
	if (status != AML_OK) {
		aml_object_unref(source);
		return (status);
	}
	source = aml_object_deref(source);
	if (source == NULL) {
		return (AML_ERR);
	}
	if (source->type == AML_TYPE_STRING) {
		available = source->u.string.length;
	} else if (source->type == AML_TYPE_BUFFER) {
		available = source->u.buffer.length;
	} else {
		return (AML_ERR_TYPE);
	}
	if (index >= available) {
		index = available;
		length = 0;
	} else if (index + length > available) {
		length = available - index;
	}
	if (source->type == AML_TYPE_STRING) {
		output = aml_object_create(AML_TYPE_STRING);
		if (output == NULL) {
			return (AML_ERR_NOMEM);
		}
		output->u.string.data = kmem_alloc((u32)length + 1);
		if (output->u.string.data == NULL) {
			aml_object_unref(output);
			return (AML_ERR_NOMEM);
		}
		if (length != 0) {
			memcpy(output->u.string.data,
			    &source->u.string.data[index], (u32)length);
		}
		output->u.string.data[length] = '\0';
		output->u.string.length = (u32)length;
	} else {
		output = aml_buffer_create((u32)length);
		if (output == NULL) {
			return (AML_ERR_NOMEM);
		}
		if (length != 0) {
			memcpy(output->u.buffer.data,
			    &source->u.buffer.data[index], (u32)length);
		}
	}
	status = aml_write_target(state, stream, output);
	if (status != AML_OK) {
		aml_object_unref(output);
		return (status);
	}
	*result = output;
	return (AML_OK);
}

static int
aml_match_test(u8 operation, int relation)
{
	switch (operation) {
	case 0:
		return (1);
	case 1:
		return (relation == 0);
	case 2:
		return (relation <= 0);
	case 3:
		return (relation < 0);
	case 4:
		return (relation >= 0);
	case 5:
		return (relation > 0);
	default:
		break;
	}
	return (0);
}

static int
aml_op_match(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*package;
	aml_object_t	*first;
	aml_object_t	*second;
	aml_object_t	*element;
	u64		start;
	u32		index;
	u8		op1;
	u8		op2;
	int		relation;
	int		status;

	package = NULL;
	status = aml_exec_term_arg(state, stream, &package);
	if (status != AML_OK) {
		return (status);
	}
	if (aml_stream_u8(stream, &op1) != AML_OK) {
		aml_object_unref(package);
		return (AML_ERR_BOUNDS);
	}
	first = NULL;
	status = aml_exec_term_arg(state, stream, &first);
	if (status != AML_OK) {
		aml_object_unref(package);
		return (status);
	}
	if (aml_stream_u8(stream, &op2) != AML_OK) {
		aml_object_unref(package);
		aml_object_unref(first);
		return (AML_ERR_BOUNDS);
	}
	second = NULL;
	status = aml_exec_term_arg(state, stream, &second);
	if (status != AML_OK) {
		aml_object_unref(package);
		aml_object_unref(first);
		return (status);
	}
	status = aml_operand_integer(state, stream, &start);
	if (status != AML_OK) {
		aml_object_unref(package);
		aml_object_unref(first);
		aml_object_unref(second);
		return (status);
	}
	package = aml_object_deref(package);
	if (package == NULL || package->type != AML_TYPE_PACKAGE) {
		aml_object_unref(first);
		aml_object_unref(second);
		return (AML_ERR_TYPE);
	}
	status = AML_OK;
	for (index = (u32)start; index < package->u.package.count; index++) {
		element = aml_object_deref(package->u.package.elements[index]);
		if (element == NULL ||
		    element->type == AML_TYPE_UNINITIALIZED) {
			continue;
		}
		if (aml_object_compare(element, first, &relation) != AML_OK ||
		    !aml_match_test(op1, relation)) {
			continue;
		}
		if (aml_object_compare(element, second, &relation) != AML_OK ||
		    !aml_match_test(op2, relation)) {
			continue;
		}
		break;
	}
	aml_object_unref(first);
	aml_object_unref(second);
	*result = aml_integer_create((index < package->u.package.count) ?
	    index : aml_integer_mask(~0ULL));
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static int
aml_op_store(aml_state_t *state, aml_stream_t *stream, aml_object_t **result)
{
	aml_object_t	*value;
	int		status;

	value = NULL;
	status = aml_exec_term_arg(state, stream, &value);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_write_target(state, stream, value);
	if (status != AML_OK) {
		aml_object_unref(value);
		return (status);
	}
	*result = value;
	return (AML_OK);
}

static int
aml_op_convert(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_t	*output;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	output = NULL;
	switch (opcode) {
	case AML_OP_TO_INTEGER:
		status = aml_object_to_integer(source, &output);
		break;
	case AML_OP_TO_BUFFER:
		status = aml_object_to_buffer(source, &output);
		break;
	case AML_OP_TO_DEC_STRING:
		status = aml_object_to_string(source, 10, &output);
		break;
	case AML_OP_TO_HEX_STRING:
		status = aml_object_to_string(source, 16, &output);
		break;
	default:
		status = AML_ERR;
		break;
	}
	aml_object_unref(source);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_write_target(state, stream, output);
	if (status != AML_OK) {
		aml_object_unref(output);
		return (status);
	}
	*result = output;
	return (AML_OK);
}

static int
aml_op_to_string(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	aml_object_t	*source;
	aml_object_t	*output;
	u64		length;
	u32		used;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &length);
	if (status != AML_OK) {
		aml_object_unref(source);
		return (status);
	}
	source = aml_object_deref(source);
	if (source == NULL || source->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	if (length > source->u.buffer.length) {
		length = source->u.buffer.length;
	}
	used = 0;
	while (used < (u32)length && source->u.buffer.data[used] != 0) {
		used++;
	}
	output = aml_object_create(AML_TYPE_STRING);
	if (output == NULL) {
		return (AML_ERR_NOMEM);
	}
	output->u.string.data = kmem_alloc(used + 1);
	if (output->u.string.data == NULL) {
		aml_object_unref(output);
		return (AML_ERR_NOMEM);
	}
	if (used != 0) {
		memcpy(output->u.string.data, source->u.buffer.data, used);
	}
	output->u.string.data[used] = '\0';
	output->u.string.length = used;
	status = aml_write_target(state, stream, output);
	if (status != AML_OK) {
		aml_object_unref(output);
		return (status);
	}
	*result = output;
	return (AML_OK);
}

static int
aml_op_sync(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*target;
	aml_object_t	*object;
	u64		value;
	u16		timeout;
	int		status;

	target = NULL;
	status = aml_exec_super_name(state, stream, &target);
	if (status != AML_OK) {
		return (status);
	}
	object = target;
	if (target != NULL && target->type == AML_TYPE_REFERENCE &&
	    target->u.reference.kind == AML_REF_NAMED &&
	    target->u.reference.node != NULL) {
		object = target->u.reference.node->object;
	}
	status = AML_OK;
	switch (opcode) {
	case AML_OP_ACQUIRE:
		if (aml_stream_u16(stream, &timeout) != AML_OK) {
			status = AML_ERR_BOUNDS;
			break;
		}
		status = aml_mutex_acquire(object, timeout);
		if (status == AML_OK) {
			*result = aml_integer_create(0);
			status = (*result != NULL) ? AML_OK : AML_ERR_NOMEM;
		}
		break;
	case AML_OP_RELEASE:
		status = aml_mutex_release(object);
		break;
	case AML_OP_SIGNAL:
		if (object != NULL && object->type == AML_TYPE_EVENT) {
			object->u.event.signals++;
		}
		break;
	case AML_OP_RESET:
		if (object != NULL && object->type == AML_TYPE_EVENT) {
			object->u.event.signals = 0;
		}
		break;
	case AML_OP_WAIT:
		status = aml_operand_integer(state, stream, &value);
		if (status != AML_OK) {
			break;
		}
		if (object != NULL && object->type == AML_TYPE_EVENT &&
		    object->u.event.signals != 0) {
			object->u.event.signals--;
			*result = aml_integer_create(0);
		} else {
			*result = aml_integer_create(aml_integer_mask(~0ULL));
		}
		status = (*result != NULL) ? AML_OK : AML_ERR_NOMEM;
		break;
	default:
		status = AML_ERR;
		break;
	}
	aml_object_unref(target);
	return (status);
}

static int
aml_op_notify(aml_state_t *state, aml_stream_t *stream)
{
	aml_object_t	*target;
	u64		value;
	int		status;

	target = NULL;
	status = aml_exec_super_name(state, stream, &target);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &value);
	if (status != AML_OK) {
		aml_object_unref(target);
		return (status);
	}
	if (target != NULL && target->type == AML_TYPE_REFERENCE &&
	    target->u.reference.kind == AML_REF_NAMED) {
		status = aml_notify_dispatch(target->u.reference.node, value);
	}
	aml_object_unref(target);
	return (status);
}

static int
aml_op_fatal(aml_state_t *state, aml_stream_t *stream)
{
	u64	code;
	u64	argument;
	u32	type_value;
	u8	type;

	if (aml_stream_u8(stream, &type) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (aml_stream_u32(stream, &type_value) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	code = type_value;
	if (aml_operand_integer(state, stream, &argument) != AML_OK) {
		return (AML_ERR);
	}
	drivers_log("[AML] Fatal type %u code 0x%llx arg 0x%llx\n", type,
	    (unsigned long long)code, (unsigned long long)argument);
	return (AML_ERR);
}

int
aml_exec_opcode(aml_state_t *state, aml_stream_t *stream, u16 opcode,
    aml_object_t **result)
{
	aml_object_t	*object;
	u64		value;
	int		status;

	if (state->depth >= AML_MAX_NESTING) {
		return (AML_ERR_DEPTH);
	}
	switch (opcode) {
	case AML_OP_STORE:
		return (aml_op_store(state, stream, result));
	case AML_OP_COPY_OBJECT:
		object = NULL;
		status = aml_exec_term_arg(state, stream, &object);
		if (status != AML_OK) {
			return (status);
		}
		status = aml_write_target(state, stream, object);
		if (status != AML_OK) {
			aml_object_unref(object);
			return (status);
		}
		*result = object;
		return (AML_OK);
	case AML_OP_ADD:
	case AML_OP_SUBTRACT:
	case AML_OP_MULTIPLY:
	case AML_OP_MOD:
	case AML_OP_SHIFT_LEFT:
	case AML_OP_SHIFT_RIGHT:
	case AML_OP_AND:
	case AML_OP_NAND:
	case AML_OP_OR:
	case AML_OP_NOR:
	case AML_OP_XOR:
		return (aml_op_binary(state, stream, opcode, result));
	case AML_OP_DIVIDE:
		return (aml_op_divide(state, stream, result));
	case AML_OP_NOT:
	case AML_OP_FIND_SET_LEFT:
	case AML_OP_FIND_SET_RIGHT:
	case AML_OP_TO_BCD:
	case AML_OP_FROM_BCD:
		return (aml_op_unary(state, stream, opcode, result));
	case AML_OP_INCREMENT:
		return (aml_op_step(state, stream, 1, result));
	case AML_OP_DECREMENT:
		return (aml_op_step(state, stream, -1, result));
	case AML_OP_LAND:
	case AML_OP_LOR:
	case AML_OP_LNOT:
	case AML_OP_LEQUAL:
	case AML_OP_LGREATER:
	case AML_OP_LLESS:
		return (aml_op_logical(state, stream, opcode, result));
	case AML_OP_IF:
		status = aml_op_if(state, stream);
		if (status != AML_OK) {
			return (status);
		}
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_WHILE:
		status = aml_op_while(state, stream);
		if (status != AML_OK) {
			return (status);
		}
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_ELSE:
		return (AML_ERR);
	case AML_OP_RETURN:
		object = NULL;
		status = aml_exec_term_arg(state, stream, &object);
		if (status != AML_OK) {
			return (status);
		}
		aml_object_unref(state->result);
		state->result = object;
		state->flow = AML_FLOW_RETURN;
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_BREAK:
		state->flow = AML_FLOW_BREAK;
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_CONTINUE:
		state->flow = AML_FLOW_CONTINUE;
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_INDEX:
		return (aml_op_index(state, stream, result));
	case AML_OP_DEREF_OF:
		return (aml_op_deref(state, stream, result));
	case AML_OP_REF_OF:
		return (aml_op_ref_of(state, stream, 0, result));
	case AML_OP_COND_REF_OF:
		return (aml_op_ref_of(state, stream, 1, result));
	case AML_OP_SIZE_OF:
		return (aml_op_size_of(state, stream, result));
	case AML_OP_OBJECT_TYPE:
		return (aml_op_object_type(state, stream, result));
	case AML_OP_CONCAT:
		return (aml_op_concat(state, stream, result));
	case AML_OP_MID:
		return (aml_op_mid(state, stream, result));
	case AML_OP_MATCH:
		return (aml_op_match(state, stream, result));
	case AML_OP_TO_INTEGER:
	case AML_OP_TO_BUFFER:
	case AML_OP_TO_DEC_STRING:
	case AML_OP_TO_HEX_STRING:
		return (aml_op_convert(state, stream, opcode, result));
	case AML_OP_TO_STRING:
		return (aml_op_to_string(state, stream, result));
	case AML_OP_CONCAT_RES:
		return (aml_op_concat(state, stream, result));
	case AML_OP_ACQUIRE:
	case AML_OP_RELEASE:
	case AML_OP_SIGNAL:
	case AML_OP_RESET:
	case AML_OP_WAIT:
		status = aml_op_sync(state, stream, opcode, result);
		if (status != AML_OK) {
			return (status);
		}
		if (*result == NULL) {
			*result = aml_object_create(AML_TYPE_UNINITIALIZED);
			return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
		}
		return (AML_OK);
	case AML_OP_NOTIFY:
		status = aml_op_notify(state, stream);
		if (status != AML_OK) {
			return (status);
		}
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_STALL:
	case AML_OP_SLEEP:
		status = aml_operand_integer(state, stream, &value);
		if (status != AML_OK) {
			return (status);
		}
		aml_stall((opcode == AML_OP_SLEEP) ? value * 1000ULL : value);
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_TIMER:
		*result = aml_integer_create(aml_timer_ticks());
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_FATAL:
		return (aml_op_fatal(state, stream));
	case AML_OP_LOAD:
	case AML_OP_UNLOAD:
	case AML_OP_LOAD_TABLE:
		return (AML_ERR_UNSUPPORTED);
	default:
		break;
	}
	return (AML_ERR_UNSUPPORTED);
}

