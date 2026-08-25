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

$define %func aml_exec_term_list as function with args aml_state_t *, aml_stream_t *
$define %func aml_exec_term_arg as function with args aml_state_t *, aml_stream_t *, aml_object_t **
$define %func aml_exec_super_name as function with args aml_state_t *, aml_stream_t *, aml_object_t **
$define %func aml_exec_method as function with args aml_node_t *, aml_object_t **, u32, aml_object_t **, u32
$define %func aml_store as function with args aml_state_t *, aml_object_t *, aml_object_t *
$define %func aml_store_named as function with args aml_state_t *, aml_object_t *, aml_node_t *
$define %func aml_evaluate as function with args aml_node_t *, aml_object_t **, u32, aml_object_t **
$define %func aml_evaluate_path as function with args aml_node_t *, const char *, aml_object_t **
$define %func aml_evaluate_integer as function with args aml_node_t *, const char *, u64 *
$define %func aml_node_status as function with args aml_node_t *
$define %func aml_node_hid as function with args aml_node_t *, char *, u32

*/

/* !SPACE!

$space %internal aml_state_clear, aml_is_data_opcode, aml_exec_data_object
$space %internal aml_exec_name_source, aml_exec_invoke, aml_eid_decode
$space %export aml_exec_term_list, aml_exec_term_arg, aml_exec_super_name
$space %export aml_exec_method, aml_store, aml_store_named
$space %export aml_evaluate, aml_evaluate_path, aml_evaluate_integer
$space %export aml_node_status, aml_node_hid

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static void
aml_state_clear(aml_state_t *state)
{
	u32	i;

	for (i = 0; i < AML_MAX_ARGS; i++) {
		aml_object_unref(state->args[i]);
		state->args[i] = NULL;
	}
	for (i = 0; i < AML_MAX_LOCALS; i++) {
		aml_object_unref(state->locals[i]);
		state->locals[i] = NULL;
	}
}

int
aml_operand_integer(aml_state_t *state, aml_stream_t *stream, u64 *value)
{
	aml_object_t	*object;
	int		status;

	object = NULL;
	status = aml_exec_term_arg(state, stream, &object);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_object_as_integer(object, value);
	aml_object_unref(object);
	return (status);
}

int
aml_exec_term_list(aml_state_t *state, aml_stream_t *stream)
{
	aml_object_t	*result;
	u16		opcode;
	u32		save;
	int		status;

	while (aml_stream_remaining(stream) != 0) {
		if (state->flow != AML_FLOW_NORMAL) {
			return (AML_OK);
		}
		save = stream->offset;
		if (aml_parse_opcode(stream, &opcode) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		stream->offset = save;
		result = NULL;
		status = aml_exec_term_arg(state, stream, &result);
		if (status != AML_OK) {
			return (status);
		}
		aml_object_unref(result);
	}
	return (AML_OK);
}

static int
aml_exec_buffer(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	aml_object_t	*size;
	aml_object_t	*buffer;
	aml_stream_t	body;
	u64		length;
	u32		package;
	u32		initial;
	int		status;

	if (aml_parse_pkglength(stream, &package) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	size = NULL;
	status = aml_exec_term_arg(state, stream, &size);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_object_as_integer(size, &length);
	aml_object_unref(size);
	if (status != AML_OK) {
		return (status);
	}
	if (length > 0x100000ULL) {
		return (AML_ERR_BOUNDS);
	}
	if (stream->offset < initial ||
	    stream->offset - initial > package) {
		return (AML_ERR_BOUNDS);
	}
	aml_stream_init(&body, stream->base + stream->offset,
	    package - (stream->offset - initial));
	stream->offset = initial + package;
	buffer = aml_buffer_create((u32)length);
	if (buffer == NULL) {
		return (AML_ERR_NOMEM);
	}
	if (body.length != 0) {
		memcpy(buffer->u.buffer.data, body.base,
		    (body.length < (u32)length) ? body.length : (u32)length);
	}
	*result = buffer;
	return (AML_OK);
}

static int
aml_exec_package_element(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **element)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*object;
	aml_node_t	*node;
	u32		save;
	u8		peek;

	if (aml_stream_remaining(stream) == 0) {
		return (AML_ERR_BOUNDS);
	}
	peek = stream->base[stream->offset];
	if (peek == AML_OP_ROOT_CHAR || peek == AML_OP_PARENT_CHAR ||
	    peek == AML_OP_DUAL_NAME || peek == AML_OP_MULTI_NAME ||
	    (peek >= 'A' && peek <= 'Z') || peek == '_') {
		save = stream->offset;
		if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
			stream->offset = save;
			return (AML_ERR);
		}
		node = aml_resolve(state->scope, path);
		if (node == NULL) {
			*element = aml_string_create(path);
			return ((*element != NULL) ? AML_OK : AML_ERR_NOMEM);
		}
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_NAMED;
		object->u.reference.node = node;
		*element = object;
		return (AML_OK);
	}
	return (aml_exec_term_arg(state, stream, element));
}

#define	AML_PACKAGE_LIMIT	4096

static int
aml_exec_package(aml_state_t *state, aml_stream_t *stream, int variable,
    aml_object_t **result)
{
	aml_object_t	*package;
	aml_object_t	*count_object;
	aml_object_t	*element;
	aml_stream_t	body;
	u64		count;
	u32		length;
	u32		initial;
	u32		index;
	u8		fixed;
	int		status;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (variable) {
		count_object = NULL;
		status = aml_exec_term_arg(state, stream, &count_object);
		if (status != AML_OK) {
			return (status);
		}
		status = aml_object_as_integer(count_object, &count);
		aml_object_unref(count_object);
		if (status != AML_OK) {
			return (status);
		}
	} else {
		if (aml_stream_u8(stream, &fixed) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		count = fixed;
	}
	if (count > AML_PACKAGE_LIMIT) {
		return (AML_ERR_BOUNDS);
	}
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	aml_stream_init(&body, stream->base + stream->offset,
	    length - (stream->offset - initial));
	stream->offset = initial + length;
	package = aml_package_create((u32)count);
	if (package == NULL) {
		return (AML_ERR_NOMEM);
	}
	for (index = 0; index < (u32)count; index++) {
		if (aml_stream_remaining(&body) == 0) {
			break;
		}
		element = NULL;
		status = aml_exec_package_element(state, &body, &element);
		if (status != AML_OK) {
			aml_object_unref(package);
			return (status);
		}
		aml_object_unref(package->u.package.elements[index]);
		package->u.package.elements[index] = element;
	}
	*result = package;
	return (AML_OK);
}

static int
aml_exec_invoke(aml_state_t *state, aml_stream_t *stream, aml_node_t *node,
    aml_object_t **result)
{
	aml_object_t	*args[AML_MAX_ARGS];
	u32		count;
	u32		i;
	int		status;

	count = node->object->u.method.arg_count;
	if (count > AML_MAX_ARGS) {
		return (AML_ERR);
	}
	for (i = 0; i < AML_MAX_ARGS; i++) {
		args[i] = NULL;
	}
	status = AML_OK;
	for (i = 0; i < count; i++) {
		status = aml_exec_term_arg(state, stream, &args[i]);
		if (status != AML_OK) {
			break;
		}
	}
	if (status == AML_OK) {
		status = aml_exec_method(node, args, count, result,
		    state->depth + 1);
	}
	for (i = 0; i < AML_MAX_ARGS; i++) {
		aml_object_unref(args[i]);
	}
	return (status);
}

static int
aml_exec_name_source(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	char		path[AML_MAX_PATH];
	aml_node_t	*node;
	aml_object_t	*object;

	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	node = aml_resolve(state->scope, path);
	if (node == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	object = node->object;
	if (object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	if (object->type == AML_TYPE_METHOD) {
		return (aml_exec_invoke(state, stream, node, result));
	}
	if (object->type == AML_TYPE_FIELD_UNIT ||
	    object->type == AML_TYPE_BUFFER_FIELD) {
		return (aml_field_read(state, object, result));
	}
	aml_object_ref(object);
	*result = object;
	return (AML_OK);
}

static int
aml_is_named_opcode(u16 opcode)
{
	switch (opcode) {
	case AML_OP_NAME:
	case AML_OP_SCOPE:
	case AML_OP_METHOD:
	case AML_OP_ALIAS:
	case AML_OP_EXTERNAL:
	case AML_OP_REGION:
	case AML_OP_FIELD:
	case AML_OP_INDEX_FIELD:
	case AML_OP_BANK_FIELD:
	case AML_OP_DEVICE:
	case AML_OP_PROCESSOR:
	case AML_OP_POWER_RES:
	case AML_OP_THERMAL_ZONE:
	case AML_OP_MUTEX:
	case AML_OP_EVENT:
	case AML_OP_CREATE_FIELD:
	case AML_OP_CREATE_BIT:
	case AML_OP_CREATE_BYTE:
	case AML_OP_CREATE_WORD:
	case AML_OP_CREATE_DWORD:
	case AML_OP_CREATE_QWORD:
		return (1);
	default:
		break;
	}
	return (0);
}

static int
aml_exec_string(aml_stream_t *stream, aml_object_t **result)
{
	const char	*text;
	u32		length;

	text = (const char *)&stream->base[stream->offset];
	length = 0;
	while (length < aml_stream_remaining(stream) &&
	    text[length] != '\0') {
		length++;
	}
	if (length >= aml_stream_remaining(stream)) {
		return (AML_ERR_BOUNDS);
	}
	stream->offset += length + 1;
	*result = aml_string_create(text);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

int
aml_exec_term_arg(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	aml_object_t	*object;
	u16		opcode;
	u64		value64;
	u32		value32;
	u16		value16;
	u8		value8;
	u8		peek;
	int		status;

	if (state->depth >= AML_MAX_NESTING) {
		return (AML_ERR_DEPTH);
	}
	if (aml_stream_remaining(stream) == 0) {
		return (AML_ERR_BOUNDS);
	}
	peek = stream->base[stream->offset];
	if (peek == AML_OP_ROOT_CHAR || peek == AML_OP_PARENT_CHAR ||
	    peek == AML_OP_DUAL_NAME || peek == AML_OP_MULTI_NAME ||
	    (peek >= 'A' && peek <= 'Z') || peek == '_') {
		return (aml_exec_name_source(state, stream, result));
	}
	if (aml_parse_opcode(stream, &opcode) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (aml_is_named_opcode(opcode)) {
		status = aml_exec_named(state, stream, opcode);
		if (status != AML_OK) {
			return (status);
		}
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	switch (opcode) {
	case AML_OP_ZERO:
		*result = aml_integer_create(0);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_ONE:
		*result = aml_integer_create(1);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_ONES:
		*result = aml_integer_create(aml_integer_mask(~0ULL));
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_BYTE:
		if (aml_stream_u8(stream, &value8) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		*result = aml_integer_create(value8);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_WORD:
		if (aml_stream_u16(stream, &value16) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		*result = aml_integer_create(value16);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_DWORD:
		if (aml_stream_u32(stream, &value32) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		*result = aml_integer_create(value32);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_QWORD:
		if (aml_stream_u64(stream, &value64) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		*result = aml_integer_create(value64);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_STRING:
		return (aml_exec_string(stream, result));
	case AML_OP_BUFFER:
		return (aml_exec_buffer(state, stream, result));
	case AML_OP_PACKAGE:
		return (aml_exec_package(state, stream, 0, result));
	case AML_OP_VAR_PACKAGE:
		return (aml_exec_package(state, stream, 1, result));
	case AML_OP_REVISION:
		*result = aml_integer_create(2);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	case AML_OP_DEBUG:
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_DEBUG;
		*result = object;
		return (AML_OK);
	case AML_OP_NOOP:
	case AML_OP_BREAKPOINT:
		*result = aml_object_create(AML_TYPE_UNINITIALIZED);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	default:
		break;
	}
	if (opcode >= AML_OP_LOCAL0 && opcode <= AML_OP_LOCAL7) {
		object = state->locals[opcode - AML_OP_LOCAL0];
		if (object == NULL) {
			object = aml_object_create(AML_TYPE_UNINITIALIZED);
			if (object == NULL) {
				return (AML_ERR_NOMEM);
			}
			state->locals[opcode - AML_OP_LOCAL0] = object;
		}
		aml_object_ref(object);
		*result = object;
		return (AML_OK);
	}
	if (opcode >= AML_OP_ARG0 && opcode <= AML_OP_ARG6) {
		object = state->args[opcode - AML_OP_ARG0];
		if (object == NULL) {
			object = aml_object_create(AML_TYPE_UNINITIALIZED);
			if (object == NULL) {
				return (AML_ERR_NOMEM);
			}
			state->args[opcode - AML_OP_ARG0] = object;
		}
		aml_object_ref(object);
		*result = object;
		return (AML_OK);
	}
	return (aml_exec_opcode(state, stream, opcode, result));
}

int
aml_exec_super_name(aml_state_t *state, aml_stream_t *stream,
    aml_object_t **result)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*object;
	aml_node_t	*node;
	u16		opcode;
	u8		peek;

	if (aml_stream_remaining(stream) == 0) {
		return (AML_ERR_BOUNDS);
	}
	peek = stream->base[stream->offset];
	if (peek == AML_OP_ROOT_CHAR || peek == AML_OP_PARENT_CHAR ||
	    peek == AML_OP_DUAL_NAME || peek == AML_OP_MULTI_NAME ||
	    (peek >= 'A' && peek <= 'Z') || peek == '_') {
		if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
			return (AML_ERR);
		}
		node = aml_resolve(state->scope, path);
		if (node == NULL) {
			node = aml_node_create(state->scope, path);
			if (node == NULL) {
				return (AML_ERR_NOMEM);
			}
		}
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_NAMED;
		object->u.reference.node = node;
		*result = object;
		return (AML_OK);
	}
	if (peek == AML_OP_ZERO) {
		stream->offset++;
		*result = NULL;
		return (AML_OK);
	}
	if (aml_parse_opcode(stream, &opcode) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (opcode >= AML_OP_LOCAL0 && opcode <= AML_OP_LOCAL7) {
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_LOCAL;
		object->u.reference.index = opcode - AML_OP_LOCAL0;
		*result = object;
		return (AML_OK);
	}
	if (opcode >= AML_OP_ARG0 && opcode <= AML_OP_ARG6) {
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_ARG;
		object->u.reference.index = opcode - AML_OP_ARG0;
		*result = object;
		return (AML_OK);
	}
	if (opcode == AML_OP_DEBUG) {
		object = aml_object_create(AML_TYPE_REFERENCE);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.reference.kind = AML_REF_DEBUG;
		*result = object;
		return (AML_OK);
	}
	return (aml_exec_opcode(state, stream, opcode, result));
}

static int
aml_store_convert(aml_object_t *value, aml_object_type_t type,
    aml_object_t **out)
{
	switch (type) {
	case AML_TYPE_INTEGER:
		return (aml_object_to_integer(value, out));
	case AML_TYPE_STRING:
		return (aml_object_to_string(value, 16, out));
	case AML_TYPE_BUFFER:
		return (aml_object_to_buffer(value, out));
	default:
		break;
	}
	*out = aml_object_clone(value);
	return ((*out != NULL) ? AML_OK : AML_ERR_NOMEM);
}

int
aml_store_named(aml_state_t *state, aml_object_t *value, aml_node_t *node)
{
	aml_object_t	*existing;
	aml_object_t	*converted;
	int		status;

	node = aml_node_resolve_ref(node);
	if (node == NULL || value == NULL) {
		return (AML_ERR);
	}
	existing = node->object;
	if (existing != NULL && (existing->type == AML_TYPE_FIELD_UNIT ||
	    existing->type == AML_TYPE_BUFFER_FIELD)) {
		return (aml_field_write(state, existing, value));
	}
	if (existing != NULL && (existing->type == AML_TYPE_INTEGER ||
	    existing->type == AML_TYPE_STRING ||
	    existing->type == AML_TYPE_BUFFER)) {
		converted = NULL;
		status = aml_store_convert(value, existing->type, &converted);
		if (status != AML_OK) {
			return (status);
		}
		aml_node_attach(node, converted);
		return (AML_OK);
	}
	converted = aml_object_clone(value);
	if (converted == NULL) {
		return (AML_ERR_NOMEM);
	}
	aml_node_attach(node, converted);
	return (AML_OK);
}

int
aml_store(aml_state_t *state, aml_object_t *value, aml_object_t *target)
{
	aml_object_t	*copy;
	aml_object_t	*container;
	u64		integer;
	u32		index;
	int		status;

	if (value == NULL) {
		return (AML_ERR);
	}
	if (target == NULL) {
		return (AML_OK);
	}
	if (target->type == AML_TYPE_FIELD_UNIT ||
	    target->type == AML_TYPE_BUFFER_FIELD) {
		return (aml_field_write(state, target, value));
	}
	if (target->type != AML_TYPE_REFERENCE) {
		return (AML_ERR_TYPE);
	}
	switch (target->u.reference.kind) {
	case AML_REF_DEBUG:
		if (value->type == AML_TYPE_INTEGER) {
			drivers_log("[AML] Debug: 0x%llx\n",
			    (unsigned long long)value->u.integer);
		} else if (value->type == AML_TYPE_STRING) {
			drivers_log("[AML] Debug: %s\n", value->u.string.data);
		} else {
			drivers_log("[AML] Debug: type %d\n",
			    (int)value->type);
		}
		return (AML_OK);
	case AML_REF_NAMED:
		return (aml_store_named(state, value,
		    target->u.reference.node));
	case AML_REF_LOCAL:
		index = target->u.reference.index;
		if (index >= AML_MAX_LOCALS) {
			return (AML_ERR_BOUNDS);
		}
		copy = aml_object_clone(value);
		if (copy == NULL) {
			return (AML_ERR_NOMEM);
		}
		aml_object_unref(state->locals[index]);
		state->locals[index] = copy;
		return (AML_OK);
	case AML_REF_ARG:
		index = target->u.reference.index;
		if (index >= AML_MAX_ARGS) {
			return (AML_ERR_BOUNDS);
		}
		copy = aml_object_clone(value);
		if (copy == NULL) {
			return (AML_ERR_NOMEM);
		}
		aml_object_unref(state->args[index]);
		state->args[index] = copy;
		return (AML_OK);
	default:
		break;
	}
	container = target->u.reference.container;
	index = target->u.reference.index;
	if (container == NULL) {
		return (AML_ERR);
	}
	if (target->u.reference.kind == AML_REF_INDEX_PACKAGE) {
		if (container->type != AML_TYPE_PACKAGE ||
		    index >= container->u.package.count) {
			return (AML_ERR_BOUNDS);
		}
		copy = aml_object_clone(value);
		if (copy == NULL) {
			return (AML_ERR_NOMEM);
		}
		aml_object_unref(container->u.package.elements[index]);
		container->u.package.elements[index] = copy;
		return (AML_OK);
	}
	status = aml_object_as_integer(value, &integer);
	if (status != AML_OK) {
		return (status);
	}
	if (target->u.reference.kind == AML_REF_INDEX_BUFFER) {
		if (container->type != AML_TYPE_BUFFER ||
		    index >= container->u.buffer.length) {
			return (AML_ERR_BOUNDS);
		}
		container->u.buffer.data[index] = (u8)integer;
		return (AML_OK);
	}
	if (target->u.reference.kind == AML_REF_INDEX_STRING) {
		if (container->type != AML_TYPE_STRING ||
		    index >= container->u.string.length) {
			return (AML_ERR_BOUNDS);
		}
		container->u.string.data[index] = (char)integer;
		return (AML_OK);
	}
	return (AML_ERR_UNSUPPORTED);
}

int
aml_exec_method(aml_node_t *node, aml_object_t **args, u32 arg_count,
    aml_object_t **result, u32 depth)
{
	aml_state_t	state;
	aml_stream_t	stream;
	u32		i;
	int		status;

	if (node == NULL || node->object == NULL) {
		return (AML_ERR);
	}
	if (node->object->type != AML_TYPE_METHOD) {
		return (AML_ERR_TYPE);
	}
	if (depth >= AML_MAX_NESTING) {
		return (AML_ERR_DEPTH);
	}
	memset(&state, 0, sizeof(state));
	state.scope = node;
	state.method_node = node;
	state.depth = depth;
	state.flow = AML_FLOW_NORMAL;
	for (i = 0; i < arg_count && i < AML_MAX_ARGS; i++) {
		if (args[i] == NULL) {
			continue;
		}
		state.args[i] = aml_object_clone(args[i]);
		if (state.args[i] == NULL) {
			aml_state_clear(&state);
			return (AML_ERR_NOMEM);
		}
	}
	aml_stream_init(&stream, node->object->u.method.aml,
	    node->object->u.method.length);
	status = aml_exec_term_list(&state, &stream);
	if (status == AML_OK && result != NULL) {
		if (state.result != NULL) {
			*result = state.result;
			state.result = NULL;
		} else {
			*result = aml_integer_create(0);
			if (*result == NULL) {
				status = AML_ERR_NOMEM;
			}
		}
	}
	aml_object_unref(state.result);
	aml_state_clear(&state);
	return (status);
}

int
aml_evaluate(aml_node_t *node, aml_object_t **args, u32 arg_count,
    aml_object_t **result)
{
	aml_state_t	state;
	aml_object_t	*object;

	node = aml_node_resolve_ref(node);
	if (node == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	object = node->object;
	if (object == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	if (object->type == AML_TYPE_METHOD) {
		return (aml_exec_method(node, args, arg_count, result, 0));
	}
	if (object->type == AML_TYPE_FIELD_UNIT ||
	    object->type == AML_TYPE_BUFFER_FIELD) {
		memset(&state, 0, sizeof(state));
		state.scope = node->parent;
		return (aml_field_read(&state, object, result));
	}
	if (result != NULL) {
		aml_object_ref(object);
		*result = object;
	}
	return (AML_OK);
}

int
aml_evaluate_path(aml_node_t *scope, const char *path, aml_object_t **result)
{
	aml_node_t	*node;

	node = aml_resolve(scope, path);
	if (node == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	return (aml_evaluate(node, NULL, 0, result));
}

int
aml_evaluate_integer(aml_node_t *scope, const char *path, u64 *value)
{
	aml_object_t	*result;
	int		status;

	result = NULL;
	status = aml_evaluate_path(scope, path, &result);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_object_as_integer(result, value);
	aml_object_unref(result);
	return (status);
}

int
aml_node_status(aml_node_t *node)
{
	aml_object_t	*result;
	aml_node_t	*child;
	u64		value;

	if (node == NULL) {
		return (0);
	}
	child = aml_node_child(node, "_STA");
	if (child == NULL) {
		return (AML_STA_PRESENT | AML_STA_ENABLED | AML_STA_SHOWN |
		    AML_STA_FUNCTIONING);
	}
	result = NULL;
	if (aml_evaluate(child, NULL, 0, &result) != AML_OK) {
		return (0);
	}
	if (aml_object_as_integer(result, &value) != AML_OK) {
		aml_object_unref(result);
		return (0);
	}
	aml_object_unref(result);
	return ((int)(value & 0xFF));
}

static void
aml_eid_decode(u32 eisa, char *out)
{
	static const char	digits[] = "0123456789ABCDEF";
	u8			bytes[4];
	u32			i;

	for (i = 0; i < 4; i++) {
		bytes[i] = (u8)(eisa >> (i * 8));
	}
	out[0] = (char)(0x40 + ((bytes[0] >> 2) & 0x1F));
	out[1] = (char)(0x40 + (((bytes[0] & 0x03) << 3) |
	    ((bytes[1] >> 5) & 0x07)));
	out[2] = (char)(0x40 + (bytes[1] & 0x1F));
	out[3] = digits[(bytes[2] >> 4) & 0x0F];
	out[4] = digits[bytes[2] & 0x0F];
	out[5] = digits[(bytes[3] >> 4) & 0x0F];
	out[6] = digits[bytes[3] & 0x0F];
	out[7] = '\0';
}

int
aml_node_hid(aml_node_t *node, char *out, u32 size)
{
	aml_object_t	*result;
	aml_node_t	*child;
	u32		length;
	int		status;

	if (node == NULL || out == NULL || size < 8) {
		return (AML_ERR);
	}
	child = aml_node_child(node, "_HID");
	if (child == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	result = NULL;
	status = aml_evaluate(child, NULL, 0, &result);
	if (status != AML_OK) {
		return (status);
	}
	status = AML_OK;
	if (result->type == AML_TYPE_INTEGER) {
		aml_eid_decode((u32)result->u.integer, out);
	} else if (result->type == AML_TYPE_STRING) {
		length = result->u.string.length;
		if (length >= size) {
			length = size - 1;
		}
		memcpy(out, result->u.string.data, length);
		out[length] = '\0';
	} else {
		status = AML_ERR_TYPE;
	}
	aml_object_unref(result);
	return (status);
}

