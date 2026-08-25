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
$define %type int as 32 bit signed
$define %type aml_node_t as AML namespace node
$define %type aml_object_t as reference counted AML data object

$define %func aml_namespace_init as function with args void
$define %func aml_root as function with args void
$define %func aml_node_new as function with args aml_node_t *, const char *
$define %func aml_node_child_find as function with args aml_node_t *, const char *
$define %func aml_node_create as function with args aml_node_t *, const char *
$define %func aml_node_lookup as function with args aml_node_t *, const char *
$define %func aml_node_child as function with args aml_node_t *, const char *
$define %func aml_node_resolve_ref as function with args aml_node_t *
$define %func aml_node_attach as procedure with args aml_node_t *, aml_object_t *
$define %func aml_resolve as function with args aml_node_t *, const char *
$define %func aml_node_path as function with args aml_node_t *, char *, u32
$define %func aml_walk as function with args aml_node_t *, int (*)(aml_node_t *, void *), void *
$define %func aml_dump_namespace as procedure with args void

*/

/* !SPACE!

$space %internal aml_node_new, aml_node_child_find, aml_segment_normalize
$space %internal aml_path_walk, aml_dump_node
$space %export aml_namespace_init, aml_root
$space %export aml_node_create, aml_node_lookup, aml_node_child
$space %export aml_node_resolve_ref
$space %export aml_node_attach, aml_resolve, aml_node_path
$space %export aml_walk, aml_dump_namespace

*/

#include <kernel/drivers/acpi/amlint.h>
#include <kernel/mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static aml_node_t	*aml_root_node = NULL;

static const char	*aml_predefined[] = {
	"_GPE", "_PR_", "_SB_", "_SI_", "_TZ_", "_OSI"
};

static aml_node_t *
aml_node_new(aml_node_t *parent, const char *name)
{
	aml_node_t	*node;
	u32		i;

	node = kmem_calloc(1, sizeof(*node));
	if (node == NULL) {
		return (NULL);
	}
	for (i = 0; i < AML_NAME_LENGTH; i++) {
		node->name[i] = (name[i] != '\0') ? name[i] : '_';
	}
	node->name[AML_NAME_LENGTH] = '\0';
	node->parent = parent;
	if (parent != NULL) {
		node->sibling = parent->child;
		parent->child = node;
	}
	return (node);
}

static aml_node_t *
aml_node_child_find(aml_node_t *parent, const char *name)
{
	aml_node_t	*child;

	if (parent == NULL) {
		return (NULL);
	}
	for (child = parent->child; child != NULL; child = child->sibling) {
		if (strncmp(child->name, name, AML_NAME_LENGTH) == 0) {
			return (child);
		}
	}
	return (NULL);
}

static void
aml_segment_normalize(const char *source, u32 length, char *out)
{
	u32	i;

	for (i = 0; i < AML_NAME_LENGTH; i++) {
		out[i] = (i < length) ? source[i] : '_';
	}
	out[AML_NAME_LENGTH] = '\0';
}

static aml_node_t *
aml_path_walk(aml_node_t *scope, const char *path, int create)
{
	char		segment[AML_NAME_LENGTH + 1];
	aml_node_t	*node;
	aml_node_t	*next;
	const char	*cursor;
	u32		length;
	u32		guard;

	if (aml_root_node == NULL || path == NULL) {
		return (NULL);
	}
	cursor = path;
	node = (scope != NULL) ? scope : aml_root_node;
	if (*cursor == '\\') {
		node = aml_root_node;
		cursor++;
	}
	guard = 0;
	while (*cursor == '^') {
		if (guard++ >= AML_MAX_NESTING) {
			return (NULL);
		}
		if (node->parent == NULL) {
			return (NULL);
		}
		node = node->parent;
		cursor++;
	}
	if (*cursor == '\0') {
		return (node);
	}
	guard = 0;
	while (*cursor != '\0') {
		if (guard++ >= AML_MAX_NESTING) {
			return (NULL);
		}
		length = 0;
		while (cursor[length] != '\0' && cursor[length] != '.' &&
		    length < AML_NAME_LENGTH) {
			length++;
		}
		if (length == 0) {
			return (NULL);
		}
		aml_segment_normalize(cursor, length, segment);
		cursor += length;
		if (*cursor == '.') {
			cursor++;
		}
		next = aml_node_child_find(node, segment);
		if (next == NULL) {
			if (!create) {
				return (NULL);
			}
			next = aml_node_new(node, segment);
			if (next == NULL) {
				return (NULL);
			}
		}
		node = next;
	}
	return (node);
}

int
aml_namespace_init(void)
{
	aml_node_t	*node;
	u32		i;

	if (aml_root_node != NULL) {
		return (AML_OK);
	}
	aml_root_node = kmem_calloc(1, sizeof(*aml_root_node));
	if (aml_root_node == NULL) {
		return (AML_ERR_NOMEM);
	}
	aml_root_node->name[0] = '\\';
	aml_root_node->name[1] = '\0';
	for (i = 0; i < sizeof(aml_predefined) / sizeof(aml_predefined[0]);
	    i++) {
		node = aml_node_new(aml_root_node, aml_predefined[i]);
		if (node == NULL) {
			return (AML_ERR_NOMEM);
		}
		node->object = aml_object_create(AML_TYPE_SCOPE);
		if (node->object == NULL) {
			return (AML_ERR_NOMEM);
		}
	}
	return (AML_OK);
}

aml_node_t *
aml_root(void)
{
	return (aml_root_node);
}

aml_node_t *
aml_node_create(aml_node_t *scope, const char *path)
{
	return (aml_path_walk(scope, path, 1));
}

aml_node_t *
aml_node_lookup(aml_node_t *scope, const char *path)
{
	aml_node_t	*node;
	aml_node_t	*current;
	u32		guard;

	if (path == NULL || aml_root_node == NULL) {
		return (NULL);
	}
	if (path[0] == '\\' || path[0] == '^' || strchr(path, '.') != NULL) {
		return (aml_path_walk(scope, path, 0));
	}
	current = (scope != NULL) ? scope : aml_root_node;
	guard = 0;
	while (current != NULL) {
		if (guard++ >= AML_MAX_NESTING) {
			return (NULL);
		}
		node = aml_path_walk(current, path, 0);
		if (node != NULL) {
			return (node);
		}
		current = current->parent;
	}
	return (NULL);
}

aml_node_t *
aml_node_child(aml_node_t *parent, const char *name)
{
	char		segment[AML_NAME_LENGTH + 1];
	aml_node_t	*node;
	u32		length;

	if (parent == NULL || name == NULL) {
		return (NULL);
	}
	length = 0;
	while (name[length] != '\0' && length < AML_NAME_LENGTH) {
		length++;
	}
	if (length == 0) {
		return (NULL);
	}
	aml_segment_normalize(name, length, segment);
	node = aml_node_child_find(parent, segment);
	return (aml_node_resolve_ref(node));
}

aml_node_t *
aml_node_resolve_ref(aml_node_t *node)
{
	u32	guard;

	guard = 0;
	while (node != NULL && node->is_alias) {
		if (guard++ >= 16) {
			return (NULL);
		}
		node = node->alias_target;
	}
	return (node);
}

void
aml_node_attach(aml_node_t *node, aml_object_t *object)
{
	if (node == NULL) {
		return;
	}
	if (node->object != NULL) {
		aml_object_unref(node->object);
	}
	node->object = object;
}

aml_node_t *
aml_resolve(aml_node_t *scope, const char *path)
{
	return (aml_node_resolve_ref(aml_node_lookup(scope, path)));
}

int
aml_node_path(aml_node_t *node, char *out, u32 size)
{
	aml_node_t	*chain[AML_MAX_NESTING];
	u32		count;
	u32		used;
	u32		i;
	u32		length;

	if (node == NULL || out == NULL || size == 0) {
		return (AML_ERR);
	}
	count = 0;
	while (node != NULL && node->parent != NULL) {
		if (count >= AML_MAX_NESTING) {
			return (AML_ERR_DEPTH);
		}
		chain[count++] = node;
		node = node->parent;
	}
	if (size < 2) {
		return (AML_ERR_BOUNDS);
	}
	out[0] = '\\';
	used = 1;
	for (i = 0; i < count; i++) {
		length = AML_NAME_LENGTH;
		while (length > 1 && chain[count - 1 - i]->name[length - 1] ==
		    '_') {
			length--;
		}
		if (used + length + 2 > size) {
			return (AML_ERR_BOUNDS);
		}
		if (i != 0) {
			out[used++] = '.';
		}
		memcpy(&out[used], chain[count - 1 - i]->name, length);
		used += length;
	}
	out[used] = '\0';
	return (AML_OK);
}

static int
aml_walk_recurse(aml_node_t *node, int (*fn)(aml_node_t *, void *), void *ctx,
    u32 depth)
{
	aml_node_t	*child;
	aml_node_t	*next;
	int		status;

	if (depth >= AML_MAX_NESTING) {
		return (AML_ERR_DEPTH);
	}
	for (child = node->child; child != NULL; child = next) {
		next = child->sibling;
		status = fn(child, ctx);
		if (status != AML_OK) {
			return (status);
		}
		status = aml_walk_recurse(child, fn, ctx, depth + 1);
		if (status != AML_OK) {
			return (status);
		}
	}
	return (AML_OK);
}

int
aml_walk(aml_node_t *start, int (*fn)(aml_node_t *, void *), void *ctx)
{
	if (fn == NULL) {
		return (AML_ERR);
	}
	if (start == NULL) {
		start = aml_root_node;
	}
	if (start == NULL) {
		return (AML_ERR_NOT_FOUND);
	}
	return (aml_walk_recurse(start, fn, ctx, 0));
}

static const char *
aml_type_name(aml_object_type_t type)
{
	switch (type) {
	case AML_TYPE_INTEGER:		return ("Integer");
	case AML_TYPE_STRING:		return ("String");
	case AML_TYPE_BUFFER:		return ("Buffer");
	case AML_TYPE_PACKAGE:		return ("Package");
	case AML_TYPE_FIELD_UNIT:	return ("FieldUnit");
	case AML_TYPE_DEVICE:		return ("Device");
	case AML_TYPE_EVENT:		return ("Event");
	case AML_TYPE_METHOD:		return ("Method");
	case AML_TYPE_MUTEX:		return ("Mutex");
	case AML_TYPE_REGION:		return ("Region");
	case AML_TYPE_POWER:		return ("Power");
	case AML_TYPE_PROCESSOR:	return ("Processor");
	case AML_TYPE_THERMAL:		return ("ThermalZone");
	case AML_TYPE_BUFFER_FIELD:	return ("BufferField");
	case AML_TYPE_SCOPE:		return ("Scope");
	default:			return ("Uninitialized");
	}
}

static int
aml_dump_node(aml_node_t *node, void *ctx)
{
	char	path[AML_MAX_PATH];

	(void)ctx;
	if (aml_node_path(node, path, sizeof(path)) != AML_OK) {
		return (AML_OK);
	}
	drivers_log("[AML] %s : %s\n", path,
	    aml_type_name((node->object != NULL) ? node->object->type :
	    AML_TYPE_UNINITIALIZED));
	return (AML_OK);
}

void
aml_dump_namespace(void)
{
	if (aml_root_node == NULL) {
		drivers_log("[AML] namespace not initialized\n");
		return;
	}
	(void)aml_walk(aml_root_node, aml_dump_node, NULL);
}

