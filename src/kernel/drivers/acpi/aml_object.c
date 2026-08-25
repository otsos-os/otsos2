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
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type aml_object_t as reference counted AML data object
$define %type aml_object_type_t as enum with AML object types

$define %func aml_integer_mask as function with args u64
$define %func aml_set_integer_width as procedure with args int
$define %func aml_get_integer_width as function with args void
$define %func aml_object_create as function with args aml_object_type_t
$define %func aml_object_free as procedure with args aml_object_t *
$define %func aml_object_ref as procedure with args aml_object_t *
$define %func aml_object_unref as procedure with args aml_object_t *
$define %func aml_integer_create as function with args u64
$define %func aml_string_create as function with args const char *
$define %func aml_buffer_create as function with args u32
$define %func aml_package_create as function with args u32
$define %func aml_object_clone as function with args aml_object_t *
$define %func aml_object_deref as function with args aml_object_t *
$define %func aml_object_as_integer as function with args aml_object_t *, u64 *
$define %func aml_object_to_integer as function with args aml_object_t *, aml_object_t **
$define %func aml_object_to_string as function with args aml_object_t *, int, aml_object_t **
$define %func aml_object_to_buffer as function with args aml_object_t *, aml_object_t **
$define %func aml_object_compare as function with args aml_object_t *, aml_object_t *, int *

*/

/* !SPACE!

$space %internal aml_object_free, aml_hex_digit, aml_parse_integer_text
$space %export aml_integer_mask, aml_set_integer_width, aml_get_integer_width
$space %export aml_object_create, aml_object_ref, aml_object_unref
$space %export aml_integer_create, aml_string_create
$space %export aml_buffer_create, aml_package_create
$space %export aml_object_clone, aml_object_deref, aml_object_as_integer
$space %export aml_object_to_integer, aml_object_to_string
$space %export aml_object_to_buffer, aml_object_compare

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int	aml_integer_width = 8;

u64
aml_integer_mask(u64 value)
{
	if (aml_integer_width == 4) {
		return (value & 0xFFFFFFFFULL);
	}
	return (value);
}

void
aml_set_integer_width(int revision)
{
	aml_integer_width = (revision >= 2) ? 8 : 4;
}

int
aml_get_integer_width(void)
{
	return (aml_integer_width);
}

aml_object_t *
aml_object_create(aml_object_type_t type)
{
	aml_object_t	*object;

	object = kmem_calloc(1, sizeof(*object));
	if (object == NULL) {
		return (NULL);
	}
	object->type = type;
	object->refcount = 1;
	return (object);
}

static void
aml_object_free(aml_object_t *object)
{
	u32	i;

	switch (object->type) {
	case AML_TYPE_STRING:
		if (object->u.string.data != NULL) {
			kmem_free(object->u.string.data);
		}
		break;
	case AML_TYPE_BUFFER:
		if (object->u.buffer.data != NULL) {
			kmem_free(object->u.buffer.data);
		}
		break;
	case AML_TYPE_PACKAGE:
		if (object->u.package.elements != NULL) {
			for (i = 0; i < object->u.package.count; i++) {
				aml_object_unref(object->u.package.elements[i]);
			}
			kmem_free(object->u.package.elements);
		}
		break;
	case AML_TYPE_BUFFER_FIELD:
		aml_object_unref(object->u.buffer_field.buffer);
		break;
	case AML_TYPE_REFERENCE:
		aml_object_unref(object->u.reference.container);
		break;
	default:
		break;
	}
	kmem_free(object);
}

void
aml_object_ref(aml_object_t *object)
{
	if (object == NULL) {
		return;
	}
	if (object->refcount == 0xFFFFFFFFU) {
		return;
	}
	object->refcount++;
}

void
aml_object_unref(aml_object_t *object)
{
	if (object == NULL) {
		return;
	}
	if (object->refcount == 0xFFFFFFFFU) {
		return;
	}
	if (object->refcount == 0) {
		return;
	}
	object->refcount--;
	if (object->refcount == 0) {
		aml_object_free(object);
	}
}

aml_object_t *
aml_integer_create(u64 value)
{
	aml_object_t	*object;

	object = aml_object_create(AML_TYPE_INTEGER);
	if (object == NULL) {
		return (NULL);
	}
	object->u.integer = aml_integer_mask(value);
	return (object);
}

aml_object_t *
aml_string_create(const char *text)
{
	aml_object_t	*object;
	u32		length;

	length = (text != NULL) ? (u32)strlen(text) : 0;
	object = aml_object_create(AML_TYPE_STRING);
	if (object == NULL) {
		return (NULL);
	}
	object->u.string.data = kmem_alloc(length + 1);
	if (object->u.string.data == NULL) {
		kmem_free(object);
		return (NULL);
	}
	if (length != 0) {
		memcpy(object->u.string.data, text, length);
	}
	object->u.string.data[length] = '\0';
	object->u.string.length = length;
	return (object);
}

aml_object_t *
aml_buffer_create(u32 length)
{
	aml_object_t	*object;

	object = aml_object_create(AML_TYPE_BUFFER);
	if (object == NULL) {
		return (NULL);
	}
	object->u.buffer.data = kmem_calloc(1, (length != 0) ? length : 1);
	if (object->u.buffer.data == NULL) {
		kmem_free(object);
		return (NULL);
	}
	object->u.buffer.length = length;
	return (object);
}

aml_object_t *
aml_package_create(u32 count)
{
	aml_object_t	*object;
	u32		i;

	object = aml_object_create(AML_TYPE_PACKAGE);
	if (object == NULL) {
		return (NULL);
	}
	object->u.package.elements = kmem_calloc((count != 0) ? count : 1,
	    sizeof(aml_object_t *));
	if (object->u.package.elements == NULL) {
		kmem_free(object);
		return (NULL);
	}
	for (i = 0; i < count; i++) {
		object->u.package.elements[i] =
		    aml_object_create(AML_TYPE_UNINITIALIZED);
		if (object->u.package.elements[i] == NULL) {
			object->u.package.count = i;
			aml_object_unref(object);
			return (NULL);
		}
	}
	object->u.package.count = count;
	return (object);
}

aml_object_t *
aml_object_deref(aml_object_t *object)
{
	u32	guard;

	guard = 0;
	while (object != NULL && object->type == AML_TYPE_REFERENCE &&
	    object->u.reference.kind == AML_REF_NAMED) {
		if (guard++ >= 16) {
			return (NULL);
		}
		if (object->u.reference.node == NULL) {
			return (NULL);
		}
		object = object->u.reference.node->object;
	}
	return (object);
}

aml_object_t *
aml_object_clone(aml_object_t *source)
{
	aml_object_t	*copy;
	u32		i;

	if (source == NULL) {
		return (NULL);
	}
	switch (source->type) {
	case AML_TYPE_INTEGER:
		return (aml_integer_create(source->u.integer));
	case AML_TYPE_STRING:
		return (aml_string_create(source->u.string.data));
	case AML_TYPE_BUFFER:
		copy = aml_buffer_create(source->u.buffer.length);
		if (copy == NULL) {
			return (NULL);
		}
		if (source->u.buffer.length != 0) {
			memcpy(copy->u.buffer.data, source->u.buffer.data,
			    source->u.buffer.length);
		}
		return (copy);
	case AML_TYPE_PACKAGE:
		copy = aml_package_create(source->u.package.count);
		if (copy == NULL) {
			return (NULL);
		}
		for (i = 0; i < source->u.package.count; i++) {
			aml_object_unref(copy->u.package.elements[i]);
			copy->u.package.elements[i] =
			    aml_object_clone(source->u.package.elements[i]);
			if (copy->u.package.elements[i] == NULL) {
				copy->u.package.elements[i] =
				    aml_object_create(AML_TYPE_UNINITIALIZED);
			}
		}
		return (copy);
	default:
		break;
	}
	copy = aml_object_create(source->type);
	if (copy == NULL) {
		return (NULL);
	}
	copy->u = source->u;
	if (copy->type == AML_TYPE_BUFFER_FIELD) {
		aml_object_ref(copy->u.buffer_field.buffer);
	}
	if (copy->type == AML_TYPE_REFERENCE) {
		aml_object_ref(copy->u.reference.container);
	}
	return (copy);
}

static int
aml_hex_digit(char c)
{
	if (c >= '0' && c <= '9') {
		return (c - '0');
	}
	if (c >= 'a' && c <= 'f') {
		return (c - 'a' + 10);
	}
	if (c >= 'A' && c <= 'F') {
		return (c - 'A' + 10);
	}
	return (-1);
}

static u64
aml_parse_integer_text(const char *text, u32 length, int base)
{
	u64	value;
	u32	i;
	u32	limit;
	int	digit;

	value = 0;
	i = 0;
	while (i < length && (text[i] == ' ' || text[i] == '\t')) {
		i++;
	}
	if (base == 0) {
		base = 10;
		if (i + 1 < length && text[i] == '0' &&
		    (text[i + 1] == 'x' || text[i + 1] == 'X')) {
			base = 16;
			i += 2;
		}
	} else if (base == 16 && i + 1 < length && text[i] == '0' &&
	    (text[i + 1] == 'x' || text[i + 1] == 'X')) {
		i += 2;
	}
	limit = (aml_get_integer_width() == 4) ? 8 : 16;
	if (base == 10) {
		limit = 20;
	}
	for (; i < length && limit != 0; i++, limit--) {
		digit = aml_hex_digit(text[i]);
		if (digit < 0 || digit >= base) {
			break;
		}
		value = value * (u64)base + (u64)digit;
	}
	return (aml_integer_mask(value));
}

int
aml_object_as_integer(aml_object_t *object, u64 *value)
{
	u64	result;
	u32	i;
	u32	count;

	object = aml_object_deref(object);
	if (object == NULL || value == NULL) {
		return (AML_ERR);
	}
	switch (object->type) {
	case AML_TYPE_INTEGER:
		*value = object->u.integer;
		return (AML_OK);
	case AML_TYPE_STRING:
		*value = aml_parse_integer_text(object->u.string.data,
		    object->u.string.length, 16);
		return (AML_OK);
	case AML_TYPE_BUFFER:
		result = 0;
		count = (u32)aml_get_integer_width();
		if (object->u.buffer.length < count) {
			count = object->u.buffer.length;
		}
		for (i = 0; i < count; i++) {
			result |= (u64)object->u.buffer.data[i] << (i * 8);
		}
		*value = aml_integer_mask(result);
		return (AML_OK);
	default:
		break;
	}
	return (AML_ERR_TYPE);
}

int
aml_object_to_integer(aml_object_t *source, aml_object_t **result)
{
	u64	value;

	source = aml_object_deref(source);
	if (source == NULL || result == NULL) {
		return (AML_ERR);
	}
	if (source->type == AML_TYPE_STRING) {
		value = aml_parse_integer_text(source->u.string.data,
		    source->u.string.length, 0);
	} else if (aml_object_as_integer(source, &value) != AML_OK) {
		return (AML_ERR_TYPE);
	}
	*result = aml_integer_create(value);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

static u32
aml_u64_to_text(u64 value, int base, char *out, u32 size)
{
	char	tmp[24];
	u32	length;
	u32	i;
	u32	digit;

	length = 0;
	if (value == 0) {
		tmp[length++] = '0';
	}
	while (value != 0 && length < sizeof(tmp)) {
		digit = (u32)(value % (u64)base);
		tmp[length++] = (char)((digit < 10) ? ('0' + digit) :
		    ('A' + digit - 10));
		value /= (u64)base;
	}
	if (length + 1 > size) {
		return (0);
	}
	for (i = 0; i < length; i++) {
		out[i] = tmp[length - 1 - i];
	}
	out[length] = '\0';
	return (length);
}

int
aml_object_to_string(aml_object_t *source, int base, aml_object_t **result)
{
	char	text[24];
	char	*buffer;
	u64	value;
	u32	total;
	u32	used;
	u32	i;
	u32	piece;

	source = aml_object_deref(source);
	if (source == NULL || result == NULL) {
		return (AML_ERR);
	}
	if (base != 10 && base != 16) {
		return (AML_ERR);
	}
	if (source->type == AML_TYPE_STRING) {
		*result = aml_string_create(source->u.string.data);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	if (source->type == AML_TYPE_INTEGER) {
		if (aml_u64_to_text(source->u.integer, base, text,
		    sizeof(text)) == 0) {
			return (AML_ERR);
		}
		*result = aml_string_create(text);
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	if (source->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	if (source->u.buffer.length == 0) {
		*result = aml_string_create("");
		return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
	}
	total = source->u.buffer.length * 5 + 1;
	buffer = kmem_alloc(total);
	if (buffer == NULL) {
		return (AML_ERR_NOMEM);
	}
	used = 0;
	for (i = 0; i < source->u.buffer.length; i++) {
		if (i != 0) {
			buffer[used++] = ',';
		}
		if (base == 16) {
			buffer[used++] = '0';
			buffer[used++] = 'x';
		}
		value = source->u.buffer.data[i];
		piece = aml_u64_to_text(value, base, text, sizeof(text));
		if (piece == 0 || used + piece + 1 > total) {
			kmem_free(buffer);
			return (AML_ERR_BOUNDS);
		}
		memcpy(&buffer[used], text, piece);
		used += piece;
	}
	buffer[used] = '\0';
	*result = aml_string_create(buffer);
	kmem_free(buffer);
	return ((*result != NULL) ? AML_OK : AML_ERR_NOMEM);
}

int
aml_object_to_buffer(aml_object_t *source, aml_object_t **result)
{
	aml_object_t	*buffer;
	u32		width;
	u32		i;

	source = aml_object_deref(source);
	if (source == NULL || result == NULL) {
		return (AML_ERR);
	}
	switch (source->type) {
	case AML_TYPE_BUFFER:
		buffer = aml_buffer_create(source->u.buffer.length);
		if (buffer == NULL) {
			return (AML_ERR_NOMEM);
		}
		if (source->u.buffer.length != 0) {
			memcpy(buffer->u.buffer.data, source->u.buffer.data,
			    source->u.buffer.length);
		}
		break;
	case AML_TYPE_INTEGER:
		width = (u32)aml_get_integer_width();
		buffer = aml_buffer_create(width);
		if (buffer == NULL) {
			return (AML_ERR_NOMEM);
		}
		for (i = 0; i < width; i++) {
			buffer->u.buffer.data[i] =
			    (u8)(source->u.integer >> (i * 8));
		}
		break;
	case AML_TYPE_STRING:
		buffer = aml_buffer_create(source->u.string.length + 1);
		if (buffer == NULL) {
			return (AML_ERR_NOMEM);
		}
		if (source->u.string.length != 0) {
			memcpy(buffer->u.buffer.data, source->u.string.data,
			    source->u.string.length);
		}
		break;
	default:
		return (AML_ERR_TYPE);
	}
	*result = buffer;
	return (AML_OK);
}

int
aml_object_compare(aml_object_t *left, aml_object_t *right, int *relation)
{
	aml_object_t	*converted;
	const u8	*lbytes;
	const u8	*rbytes;
	u64		lvalue, rvalue;
	u32		llength, rlength, common, i;
	int		status;

	left = aml_object_deref(left);
	right = aml_object_deref(right);
	if (left == NULL || right == NULL || relation == NULL) {
		return (AML_ERR);
	}
	if (left->type == AML_TYPE_INTEGER) {
		if (aml_object_as_integer(left, &lvalue) != AML_OK ||
		    aml_object_as_integer(right, &rvalue) != AML_OK) {
			return (AML_ERR_TYPE);
		}
		*relation = (lvalue == rvalue) ? 0 : ((lvalue < rvalue) ? -1 : 1);
		return (AML_OK);
	}
	if (left->type != AML_TYPE_STRING && left->type != AML_TYPE_BUFFER) {
		return (AML_ERR_TYPE);
	}
	converted = NULL;
	if (right->type != left->type) {
		status = (left->type == AML_TYPE_STRING) ?
		    aml_object_to_string(right, 16, &converted) :
		    aml_object_to_buffer(right, &converted);
		if (status != AML_OK) {
			return (status);
		}
		right = converted;
	}
	if (left->type == AML_TYPE_STRING) {
		lbytes = (const u8 *)left->u.string.data;
		llength = left->u.string.length;
		rbytes = (const u8 *)right->u.string.data;
		rlength = right->u.string.length;
	} else {
		lbytes = left->u.buffer.data;
		llength = left->u.buffer.length;
		rbytes = right->u.buffer.data;
		rlength = right->u.buffer.length;
	}
	common = (llength < rlength) ? llength : rlength;
	*relation = 0;
	for (i = 0; i < common; i++) {
		if (lbytes[i] != rbytes[i]) {
			*relation = (lbytes[i] < rbytes[i]) ? -1 : 1;
			break;
		}
	}
	if (*relation == 0 && llength != rlength) {
		*relation = (llength < rlength) ? -1 : 1;
	}
	aml_object_unref(converted);
	return (AML_OK);
}

