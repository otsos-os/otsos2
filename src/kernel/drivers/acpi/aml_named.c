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

$define %func aml_field_connection as function with args aml_state_t *, aml_stream_t *, aml_field_config_t *
$define %func aml_field_segment as function with args aml_stream_t *, char *
$define %func aml_exec_named as function with args aml_state_t *, aml_stream_t *, u16
$define %func aml_load_table as function with args const u8 *, u32

*/

/* !SPACE!

$space %internal aml_named_scope, aml_named_name, aml_named_method
$space %internal aml_named_region, aml_field_connection, aml_field_segment
$space %internal aml_named_field_list, aml_named_field
$space %internal aml_named_index_field, aml_named_bank_field
$space %internal aml_named_simple, aml_named_create_field
$space %export aml_exec_named, aml_load_table

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/drivers/acpi/acpi.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
aml_named_body(aml_state_t *state, aml_stream_t *stream, u32 length,
    aml_node_t *scope)
{
	aml_stream_t	body;
	aml_node_t	*saved;
	int		status;

	if (length > aml_stream_remaining(stream)) {
		return (AML_ERR_BOUNDS);
	}
	aml_stream_init(&body, stream->base + stream->offset, length);
	stream->offset += length;
	saved = state->scope;
	state->scope = scope;
	state->depth++;
	status = aml_exec_term_list(state, &body);
	state->depth--;
	state->scope = saved;
	return (status);
}

static int
aml_named_scope(aml_state_t *state, aml_stream_t *stream)
{
	char		path[AML_MAX_PATH];
	aml_node_t	*node;
	u32		length;
	u32		initial;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	node = aml_node_lookup(state->scope, path);
	if (node == NULL) {
		node = aml_node_create(state->scope, path);
	}
	if (node == NULL) {
		return (AML_ERR_NOMEM);
	}
	if (node->object == NULL) {
		node->object = aml_object_create(AML_TYPE_SCOPE);
		if (node->object == NULL) {
			return (AML_ERR_NOMEM);
		}
	}
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	return (aml_named_body(state, stream,
	    length - (stream->offset - initial), node));
}

static int
aml_named_name(aml_state_t *state, aml_stream_t *stream)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*value;
	aml_node_t	*node;
	int		status;

	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	value = NULL;
	status = aml_exec_term_arg(state, stream, &value);
	if (status != AML_OK) {
		return (status);
	}
	node = aml_node_create(state->scope, path);
	if (node == NULL) {
		aml_object_unref(value);
		return (AML_ERR_NOMEM);
	}
	aml_node_attach(node, value);
	return (AML_OK);
}

static int
aml_named_method(aml_state_t *state, aml_stream_t *stream)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*method;
	aml_node_t	*node;
	u32		length;
	u32		initial;
	u32		body;
	u8		flags;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	if (aml_stream_u8(stream, &flags) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	body = length - (stream->offset - initial);
	if (body > aml_stream_remaining(stream)) {
		return (AML_ERR_BOUNDS);
	}
	node = aml_node_create(state->scope, path);
	if (node == NULL) {
		return (AML_ERR_NOMEM);
	}
	method = aml_object_create(AML_TYPE_METHOD);
	if (method == NULL) {
		return (AML_ERR_NOMEM);
	}
	method->u.method.aml = stream->base + stream->offset;
	method->u.method.length = body;
	method->u.method.arg_count = (u8)(flags & 0x07);
	method->u.method.serialized = (u8)((flags >> 3) & 0x01);
	method->u.method.sync_level = (u8)((flags >> 4) & 0x0F);
	method->u.method.scope = node;
	aml_node_attach(node, method);
	stream->offset += body;
	return (AML_OK);
}

static int
aml_named_container(aml_state_t *state, aml_stream_t *stream, u16 opcode)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*object;
	aml_node_t	*node;
	u32		length;
	u32		initial;
	u32		block;
	u16		order;
	u8		id;
	u8		block_length;
	u8		level;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	node = aml_node_create(state->scope, path);
	if (node == NULL) {
		return (AML_ERR_NOMEM);
	}
	switch (opcode) {
	case AML_OP_DEVICE:
		object = aml_object_create(AML_TYPE_DEVICE);
		break;
	case AML_OP_THERMAL_ZONE:
		object = aml_object_create(AML_TYPE_THERMAL);
		break;
	case AML_OP_PROCESSOR:
		if (aml_stream_u8(stream, &id) != AML_OK ||
		    aml_stream_u32(stream, &block) != AML_OK ||
		    aml_stream_u8(stream, &block_length) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		object = aml_object_create(AML_TYPE_PROCESSOR);
		if (object != NULL) {
			object->u.processor.id = id;
			object->u.processor.block_address = block;
			object->u.processor.block_length = block_length;
		}
		break;
	case AML_OP_POWER_RES:
		if (aml_stream_u8(stream, &level) != AML_OK ||
		    aml_stream_u16(stream, &order) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		object = aml_object_create(AML_TYPE_POWER);
		if (object != NULL) {
			object->u.power.system_level = level;
			object->u.power.resource_order = order;
		}
		break;
	default:
		return (AML_ERR);
	}
	if (object == NULL) {
		return (AML_ERR_NOMEM);
	}
	aml_node_attach(node, object);
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	return (aml_named_body(state, stream,
	    length - (stream->offset - initial), node));
}

static int
aml_named_region(aml_state_t *state, aml_stream_t *stream)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*region;
	aml_node_t	*node;
	u64		offset;
	u64		length;
	u8		space;
	int		status;

	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	if (aml_stream_u8(stream, &space) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	status = aml_operand_integer(state, stream, &offset);
	if (status != AML_OK) {
		return (status);
	}
	status = aml_operand_integer(state, stream, &length);
	if (status != AML_OK) {
		return (status);
	}
	node = aml_node_create(state->scope, path);
	if (node == NULL) {
		return (AML_ERR_NOMEM);
	}
	region = aml_object_create(AML_TYPE_REGION);
	if (region == NULL) {
		return (AML_ERR_NOMEM);
	}
	region->u.region.space = space;
	region->u.region.offset = offset;
	region->u.region.length = length;
	aml_node_attach(node, region);
	if (space == AML_REGION_PCI_CONFIG) {
		(void)aml_region_resolve_pci(node, region);
	}
	return (AML_OK);
}

typedef struct {
	aml_node_t	*region;
	aml_node_t	*index;
	aml_node_t	*data;
	aml_node_t	*bank;
	u64		bank_value;
	aml_node_t	*connection;
	const u8	*connection_data;
	u32		connection_length;
	u8		access;
	u8		update;
	u8		lock;
	u8		is_index;
	u8		is_bank;
} aml_field_config_t;

#define	AML_FIELD_MAX_BITS	0x20000000U
static int
aml_field_connection(aml_state_t *state, aml_stream_t *list,
    aml_field_config_t *config)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*size;
	u32		package;
	u32		initial;
	u32		consumed;
	int		status;

	if (aml_stream_remaining(list) == 0) {
		return (AML_ERR_BOUNDS);
	}
	if (list->base[list->offset] != AML_OP_BUFFER) {
		if (aml_parse_namestring(list, path, sizeof(path)) != AML_OK) {
			drivers_log("aml: field list: bad Connection name\n");
			return (AML_ERR);
		}
		config->connection = aml_resolve(state->scope, path);
		config->connection_data = NULL;
		config->connection_length = 0;
		return (AML_OK);
	}
	list->offset++;
	if (aml_parse_pkglength(list, &package) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = list->offset;
	size = NULL;
	status = aml_exec_term_arg(state, list, &size);
	if (status != AML_OK) {
		return (status);
	}
	aml_object_unref(size);
	if (list->offset < initial) {
		return (AML_ERR_BOUNDS);
	}
	consumed = list->offset - initial;
	if (consumed > package) {
		return (AML_ERR_BOUNDS);
	}
	config->connection = NULL;
	config->connection_data = list->base + list->offset;
	config->connection_length = package - consumed;
	list->offset = initial + package;
	return (AML_OK);
}

static int
aml_field_segment(aml_stream_t *list, char *segment)
{
	u32	i;
	u8	byte;

	if (list->offset == 0 ||
	    aml_stream_remaining(list) < AML_NAME_LENGTH) {
		return (AML_ERR_BOUNDS);
	}
	list->offset--;
	for (i = 0; i < AML_NAME_LENGTH; i++) {
		byte = list->base[list->offset + i];
		if (i == 0) {
			if (!aml_name_lead_valid(byte)) {
				return (AML_ERR);
			}
		} else if (!aml_name_char_valid(byte)) {
			return (AML_ERR);
		}
		segment[i] = (char)byte;
	}
	segment[AML_NAME_LENGTH] = '\0';
	list->offset += AML_NAME_LENGTH;
	return (AML_OK);
}

static int
aml_named_field_list(aml_state_t *state, aml_stream_t *stream, u32 length,
    aml_field_config_t *config)
{
	char		segment[AML_NAME_LENGTH + 1];
	aml_stream_t	list;
	aml_object_t	*field;
	aml_node_t	*node;
	u32		bit_offset;
	u32		width;
	u8		lead;
	u8		access;
	u8		attribute;
	u8		extra;
	int		status;

	if (length > aml_stream_remaining(stream)) {
		return (AML_ERR_BOUNDS);
	}
	aml_stream_init(&list, stream->base + stream->offset, length);
	stream->offset += length;
	bit_offset = 0;
	while (aml_stream_remaining(&list) != 0) {
		if (aml_stream_u8(&list, &lead) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		if (lead == 0x00) {
			status = aml_parse_field_length(&list, &width);
			if (status != AML_OK) {
				return (AML_ERR_BOUNDS);
			}
			if (width > AML_FIELD_MAX_BITS - bit_offset) {
				drivers_log("aml: field skip %u at bit "
				    "%u out of range\n", width, bit_offset);
				return (AML_ERR_BOUNDS);
			}
			bit_offset += width;
			continue;
		}
		if (lead == 0x01) {
			if (aml_stream_u8(&list, &access) != AML_OK ||
			    aml_stream_u8(&list, &attribute) != AML_OK) {
				return (AML_ERR_BOUNDS);
			}
			config->access = (u8)(access & 0x0F);
			continue;
		}
		if (lead == 0x02) {
			status = aml_field_connection(state, &list,
			    config);
			if (status != AML_OK) {
				return (status);
			}
			continue;
		}
		if (lead == 0x03) {
			if (aml_stream_u8(&list, &access) != AML_OK ||
			    aml_stream_u8(&list, &attribute) != AML_OK ||
			    aml_stream_u8(&list, &extra) != AML_OK) {
				return (AML_ERR_BOUNDS);
			}
			config->access = (u8)(access & 0x0F);
			continue;
		}
		status = aml_field_segment(&list, segment);
		if (status != AML_OK) {
			drivers_log("aml: field list: bad NameSeg at +0x%x "
			    "(lead 0x%x)\n", list.offset, lead);
			return (status);
		}
		status = aml_parse_field_length(&list, &width);
		if (status != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		if (width > AML_FIELD_MAX_BITS - bit_offset) {
			drivers_log("aml: field %s at bit %u width %u "
			    "out of range\n", segment, bit_offset, width);
			return (AML_ERR_BOUNDS);
		}
		node = aml_node_create(state->scope, segment);
		if (node == NULL) {
			return (AML_ERR_NOMEM);
		}
		field = aml_object_create(AML_TYPE_FIELD_UNIT);
		if (field == NULL) {
			return (AML_ERR_NOMEM);
		}
		field->u.field.region = config->region;
		field->u.field.bit_offset = bit_offset;
		field->u.field.bit_length = width;
		field->u.field.access = config->access;
		field->u.field.update = config->update;
		field->u.field.lock = config->lock;
		field->u.field.index = config->index;
		field->u.field.data = config->data;
		field->u.field.bank = config->bank;
		field->u.field.bank_value = config->bank_value;
		field->u.field.is_index = config->is_index;
		field->u.field.is_bank = config->is_bank;
		field->u.field.connection = config->connection;
		field->u.field.connection_data = config->connection_data;
		field->u.field.connection_length = config->connection_length;
		aml_node_attach(node, field);
		bit_offset += width;
	}
	return (AML_OK);
}

static void
aml_field_flags(aml_field_config_t *config, u8 flags)
{
	config->access = (u8)(flags & 0x0F);
	config->lock = (u8)((flags >> 4) & 0x01);
	config->update = (u8)((flags >> 5) & 0x03);
}

static int
aml_named_field(aml_state_t *state, aml_stream_t *stream)
{
	char			path[AML_MAX_PATH];
	aml_field_config_t	config;
	u32			length;
	u32			initial;
	u8			flags;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		return (AML_ERR);
	}
	if (aml_stream_u8(stream, &flags) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	memset(&config, 0, sizeof(config));
	config.region = aml_resolve(state->scope, path);
	if (config.region == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	aml_field_flags(&config, flags);
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	return (aml_named_field_list(state, stream,
	    length - (stream->offset - initial), &config));
}

static int
aml_named_index_field(aml_state_t *state, aml_stream_t *stream)
{
	char			index_path[AML_MAX_PATH];
	char			data_path[AML_MAX_PATH];
	aml_field_config_t	config;
	u32			length;
	u32			initial;
	u8			flags;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, index_path,
	    sizeof(index_path)) != AML_OK ||
	    aml_parse_namestring(stream, data_path,
	    sizeof(data_path)) != AML_OK) {
		return (AML_ERR);
	}
	if (aml_stream_u8(stream, &flags) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	memset(&config, 0, sizeof(config));
	config.index = aml_resolve(state->scope, index_path);
	config.data = aml_resolve(state->scope, data_path);
	if (config.index == NULL || config.data == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	config.is_index = 1;
	aml_field_flags(&config, flags);
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	return (aml_named_field_list(state, stream,
	    length - (stream->offset - initial), &config));
}

static int
aml_named_bank_field(aml_state_t *state, aml_stream_t *stream)
{
	char			region_path[AML_MAX_PATH];
	char			bank_path[AML_MAX_PATH];
	aml_field_config_t	config;
	u32			length;
	u32			initial;
	u8			flags;
	int			status;

	if (aml_parse_pkglength(stream, &length) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	initial = stream->offset;
	if (aml_parse_namestring(stream, region_path,
	    sizeof(region_path)) != AML_OK ||
	    aml_parse_namestring(stream, bank_path,
	    sizeof(bank_path)) != AML_OK) {
		return (AML_ERR);
	}
	memset(&config, 0, sizeof(config));
	status = aml_operand_integer(state, stream, &config.bank_value);
	if (status != AML_OK) {
		return (status);
	}
	if (aml_stream_u8(stream, &flags) != AML_OK) {
		return (AML_ERR_BOUNDS);
	}
	config.region = aml_resolve(state->scope, region_path);
	config.bank = aml_resolve(state->scope, bank_path);
	if (config.region == NULL || config.bank == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	config.is_bank = 1;
	aml_field_flags(&config, flags);
	if (stream->offset < initial || stream->offset - initial > length) {
		return (AML_ERR_BOUNDS);
	}
	return (aml_named_field_list(state, stream,
	    length - (stream->offset - initial), &config));
}

static int
aml_named_simple(aml_state_t *state, aml_stream_t *stream, u16 opcode)
{
	char		path[AML_MAX_PATH];
	char		target[AML_MAX_PATH];
	aml_object_t	*object;
	aml_node_t	*node;
	aml_node_t	*alias;
	u8		flags;
	u8		type;
	u8		count;

	switch (opcode) {
	case AML_OP_MUTEX:
		if (aml_parse_namestring(stream, path,
		    sizeof(path)) != AML_OK) {
			return (AML_ERR);
		}
		if (aml_stream_u8(stream, &flags) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		node = aml_node_create(state->scope, path);
		if (node == NULL) {
			return (AML_ERR_NOMEM);
		}
		object = aml_object_create(AML_TYPE_MUTEX);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		object->u.mutex.sync_level = (u32)(flags & 0x0F);
		aml_node_attach(node, object);
		return (AML_OK);
	case AML_OP_EVENT:
		if (aml_parse_namestring(stream, path,
		    sizeof(path)) != AML_OK) {
			return (AML_ERR);
		}
		node = aml_node_create(state->scope, path);
		if (node == NULL) {
			return (AML_ERR_NOMEM);
		}
		object = aml_object_create(AML_TYPE_EVENT);
		if (object == NULL) {
			return (AML_ERR_NOMEM);
		}
		aml_node_attach(node, object);
		return (AML_OK);
	case AML_OP_ALIAS:
		if (aml_parse_namestring(stream, target,
		    sizeof(target)) != AML_OK ||
		    aml_parse_namestring(stream, path,
		    sizeof(path)) != AML_OK) {
			return (AML_ERR);
		}
		alias = aml_node_lookup(state->scope, target);
		if (alias == NULL) {
			return (AML_ERR_NOT_FOUND);
		}
		node = aml_node_create(state->scope, path);
		if (node == NULL) {
			return (AML_ERR_NOMEM);
		}
		node->is_alias = 1;
		node->alias_target = alias;
		return (AML_OK);
	case AML_OP_EXTERNAL:
		if (aml_parse_namestring(stream, path,
		    sizeof(path)) != AML_OK) {
			return (AML_ERR);
		}
		if (aml_stream_u8(stream, &type) != AML_OK ||
		    aml_stream_u8(stream, &count) != AML_OK) {
			return (AML_ERR_BOUNDS);
		}
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR);
}

static int
aml_named_create_field(aml_state_t *state, aml_stream_t *stream, u16 opcode)
{
	char		path[AML_MAX_PATH];
	aml_object_t	*source;
	aml_object_t	*buffer;
	aml_object_t	*field;
	aml_node_t	*node;
	u64		index;
	u64		count;
	u32		bit_offset;
	u32		bit_length;
	int		status;

	source = NULL;
	status = aml_exec_term_arg(state, stream, &source);
	if (status != AML_OK) {
		return (status);
	}
	buffer = aml_object_deref(source);
	if (buffer == NULL || buffer->type != AML_TYPE_BUFFER) {
		aml_object_unref(source);
		return (AML_ERR_TYPE);
	}
	status = aml_operand_integer(state, stream, &index);
	if (status != AML_OK) {
		aml_object_unref(source);
		return (status);
	}
	count = 0;
	if (opcode == AML_OP_CREATE_FIELD) {
		status = aml_operand_integer(state, stream, &count);
		if (status != AML_OK) {
			aml_object_unref(source);
			return (status);
		}
	}
	if (aml_parse_namestring(stream, path, sizeof(path)) != AML_OK) {
		aml_object_unref(source);
		return (AML_ERR);
	}
	switch (opcode) {
	case AML_OP_CREATE_BIT:
		bit_offset = (u32)index;
		bit_length = 1;
		break;
	case AML_OP_CREATE_BYTE:
		bit_offset = (u32)(index * 8);
		bit_length = 8;
		break;
	case AML_OP_CREATE_WORD:
		bit_offset = (u32)(index * 16);
		bit_length = 16;
		break;
	case AML_OP_CREATE_DWORD:
		bit_offset = (u32)(index * 32);
		bit_length = 32;
		break;
	case AML_OP_CREATE_QWORD:
		bit_offset = (u32)(index * 64);
		bit_length = 64;
		break;
	case AML_OP_CREATE_FIELD:
		bit_offset = (u32)index;
		bit_length = (u32)count;
		break;
	default:
		aml_object_unref(source);
		return (AML_ERR);
	}
	if (bit_length == 0 ||
	    bit_offset + bit_length > buffer->u.buffer.length * 8) {
		aml_object_unref(source);
		return (AML_ERR_BOUNDS);
	}
	node = aml_node_create(state->scope, path);
	if (node == NULL) {
		aml_object_unref(source);
		return (AML_ERR_NOMEM);
	}
	field = aml_object_create(AML_TYPE_BUFFER_FIELD);
	if (field == NULL) {
		aml_object_unref(source);
		return (AML_ERR_NOMEM);
	}
	aml_object_ref(buffer);
	field->u.buffer_field.buffer = buffer;
	field->u.buffer_field.bit_offset = bit_offset;
	field->u.buffer_field.bit_length = bit_length;
	aml_node_attach(node, field);
	aml_object_unref(source);
	return (AML_OK);
}

int
aml_exec_named(aml_state_t *state, aml_stream_t *stream, u16 opcode)
{
	switch (opcode) {
	case AML_OP_SCOPE:
		return (aml_named_scope(state, stream));
	case AML_OP_NAME:
		return (aml_named_name(state, stream));
	case AML_OP_METHOD:
		return (aml_named_method(state, stream));
	case AML_OP_DEVICE:
	case AML_OP_PROCESSOR:
	case AML_OP_POWER_RES:
	case AML_OP_THERMAL_ZONE:
		return (aml_named_container(state, stream, opcode));
	case AML_OP_REGION:
		return (aml_named_region(state, stream));
	case AML_OP_FIELD:
		return (aml_named_field(state, stream));
	case AML_OP_INDEX_FIELD:
		return (aml_named_index_field(state, stream));
	case AML_OP_BANK_FIELD:
		return (aml_named_bank_field(state, stream));
	case AML_OP_MUTEX:
	case AML_OP_EVENT:
	case AML_OP_ALIAS:
	case AML_OP_EXTERNAL:
		return (aml_named_simple(state, stream, opcode));
	case AML_OP_CREATE_FIELD:
	case AML_OP_CREATE_BIT:
	case AML_OP_CREATE_BYTE:
	case AML_OP_CREATE_WORD:
	case AML_OP_CREATE_DWORD:
	case AML_OP_CREATE_QWORD:
		return (aml_named_create_field(state, stream, opcode));
	default:
		break;
	}
	return (AML_ERR_UNSUPPORTED);
}

int
aml_load_table(const u8 *aml, u32 length)
{
	aml_stream_t	stream;
	aml_state_t	state;
	int		status;

	if (aml == NULL || length == 0) {
		return (AML_ERR);
	}
	status = aml_namespace_init();
	if (status != AML_OK) {
		return (status);
	}
	memset(&state, 0, sizeof(state));
	state.scope = aml_root();
	state.flow = AML_FLOW_NORMAL;
	if (state.scope == NULL) {
		return (AML_ERR);
	}
	aml_stream_init(&stream, aml, length);
	return (aml_exec_term_list(&state, &stream));
}
