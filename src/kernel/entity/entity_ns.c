/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type api_entity_entry as struct with entity list entry

$define %func entity_str_put as function with args const char *, u32 *
$define %func entity_str_get as function with args u32
$define %func entity_ns_init as procedure with args void
$define %func entity_ns_node_alloc as function with args u32, const char *
$define %func entity_ns_node_free as procedure with args u32
$define %func entity_ns_find_child as function with args u32, const char *
$define %func entity_ns_component as function with args const char **, char *
$define %func entity_ns_skip_prefix as function with args const char *
$define %func entity_ns_resolve_node as function with args const char *, u32 *
$define %func entity_ns_find_by_entity as function with args entity_id
$define %func entity_ns_prune as procedure with args u32
$define %func entity_ns_is_path as function with args const char *
$define %func entity_ns_name_of as function with args entity_id
$define %func entity_ns_bind as function with args const char *, entity_id
$define %func entity_ns_unbind as function with args const char *
$define %func entity_ns_unbind_id as function with args entity_id
$define %func entity_ns_lookup as function with args const char *, u64 *
$define %func entity_ns_list as function with args path, entries, count

*/

/* !SPACE!

$space %internal entity_str_put, entity_str_get
$space %export entity_ns_init
$space %internal entity_ns_node_alloc, entity_ns_node_free
$space %internal entity_ns_find_child, entity_ns_component
$space %internal entity_ns_skip_prefix, entity_ns_resolve_node
$space %internal entity_ns_find_by_entity, entity_ns_prune
$space %export entity_ns_is_path, entity_ns_name_of
$space %export entity_ns_bind, entity_ns_unbind, entity_ns_unbind_id
$space %export entity_ns_lookup, entity_ns_list

*/

#include <kernel/api/api.h>
#include <kernel/api/errno.h>
#include <kernel/entity/entity.h>
#include <kernel/smp/smp.h>
#include <mlibc/mlibc.h>

#define	ENTITY_NS_NONE		0xFFFFFFFFU
#define	ENTITY_NS_ROOT		0
#define	ENTITY_NS_COMPONENT_MAX	(ENTITY_NAME_MAX - 1)

static u32	entity_ns_used[ENTITY_MAX_NS_NODES];
static u32	entity_ns_parent[ENTITY_MAX_NS_NODES];
static u32	entity_ns_first_child[ENTITY_MAX_NS_NODES];
static u32	entity_ns_next_sibling[ENTITY_MAX_NS_NODES];
static u32	entity_ns_name_off[ENTITY_MAX_NS_NODES];
static u64	entity_ns_entity[ENTITY_MAX_NS_NODES];
static u32	entity_ns_free_head;

static char	entity_str_arena[ENTITY_STRING_ARENA_SIZE];
static u32	entity_str_next;

void
entity_ns_init(void)
{
	u32	i;

	memset(entity_ns_used, 0, sizeof(entity_ns_used));
	memset(entity_ns_parent, 0, sizeof(entity_ns_parent));
	memset(entity_ns_first_child, 0,
	    sizeof(entity_ns_first_child));
	memset(entity_ns_next_sibling, 0,
	    sizeof(entity_ns_next_sibling));
	memset(entity_ns_name_off, 0, sizeof(entity_ns_name_off));
	memset(entity_ns_entity, 0, sizeof(entity_ns_entity));
	memset(entity_str_arena, 0, sizeof(entity_str_arena));
	entity_ns_used[ENTITY_NS_ROOT] = 1;
	entity_ns_parent[ENTITY_NS_ROOT] = ENTITY_NS_NONE;
	entity_ns_first_child[ENTITY_NS_ROOT] = ENTITY_NS_NONE;
	entity_ns_next_sibling[ENTITY_NS_ROOT] = ENTITY_NS_NONE;
	entity_ns_name_off[ENTITY_NS_ROOT] = 0;
	entity_ns_entity[ENTITY_NS_ROOT] = 0;
	entity_ns_free_head = 1;
	for (i = 1; i < ENTITY_MAX_NS_NODES; i++) {
		entity_ns_next_sibling[i] = i + 1;
	}
	entity_ns_next_sibling[ENTITY_MAX_NS_NODES - 1] =
	    ENTITY_NS_NONE;
	entity_str_next = 0;
}

static int
entity_str_put(const char *src, u32 *off)
{
	u32	len;

	if (!src || !off) {
		return (-API_ERR_BAD_VALUE);
	}
	len = (u32)strlen(src);
	if (len >= ENTITY_NAME_MAX) {
		return (-API_ERR_TOO_BIG);
	}
	if (entity_str_next + len + 1 > ENTITY_STRING_ARENA_SIZE) {
		return (-API_ERR_NO_SPACE);
	}
	*off = entity_str_next;
	memcpy(entity_str_arena + entity_str_next, src, len);
	entity_str_next += len;
	entity_str_arena[entity_str_next++] = '\0';
	return (0);
}

static const char *
entity_str_get(u32 off)
{
	if (off >= ENTITY_STRING_ARENA_SIZE) {
		return (NULL);
	}
	return (entity_str_arena + off);
}

static u32
entity_ns_node_alloc(u32 parent, const char *name)
{
	u32	node;
	u32	name_off;

	if (entity_ns_free_head == ENTITY_NS_NONE) {
		return (ENTITY_NS_NONE);
	}
	if (entity_str_put(name, &name_off) != 0) {
		return (ENTITY_NS_NONE);
	}
	node = entity_ns_free_head;
	entity_ns_free_head = entity_ns_next_sibling[node];
	entity_ns_used[node] = 1;
	entity_ns_parent[node] = parent;
	entity_ns_name_off[node] = name_off;
	entity_ns_entity[node] = 0;
	entity_ns_next_sibling[node] = entity_ns_first_child[parent];
	entity_ns_first_child[parent] = node;
	return (node);
}

static void
entity_ns_node_free(u32 node)
{
	entity_ns_used[node] = 0;
	entity_ns_parent[node] = ENTITY_NS_NONE;
	entity_ns_first_child[node] = ENTITY_NS_NONE;
	entity_ns_name_off[node] = 0;
	entity_ns_entity[node] = 0;
	entity_ns_next_sibling[node] = entity_ns_free_head;
	entity_ns_free_head = node;
}

static u32
entity_ns_find_child(u32 parent, const char *name)
{
	u32	child;

	for (child = entity_ns_first_child[parent];
	    child != ENTITY_NS_NONE;
	    child = entity_ns_next_sibling[child]) {
		const char	*child_name;

		child_name = entity_str_get(entity_ns_name_off[child]);
		if (child_name && strcmp(child_name, name) == 0) {
			return (child);
		}
	}
	return (ENTITY_NS_NONE);
}

static int
entity_ns_component(const char **path, char *buf, u32 bufsize)
{
	const char	*p;
	u32		len;

	p = *path;
	while (*p == '/') {
		p++;
	}
	if (*p == '\0') {
		*path = p;
		return (0);
	}
	len = 0;
	while (p[len] != '\0' && p[len] != '/') {
		len++;
	}
	if (len == 0 || len >= bufsize || len >= ENTITY_NS_COMPONENT_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	if ((len == 1 && p[0] == '.') ||
	    (len == 2 && p[0] == '.' && p[1] == '.')) {
		return (-API_ERR_BAD_VALUE);
	}
	memcpy(buf, p, len);
	buf[len] = '\0';
	*path = p + len;
	return ((int)len);
}

static const char *
entity_ns_skip_prefix(const char *path)
{
	if (!path) {
		return (NULL);
	}
	if (strncmp(path, ENTITY_NS_PREFIX, ENTITY_NS_PREFIX_LEN) != 0) {
		return (NULL);
	}
	if (path[ENTITY_NS_PREFIX_LEN] != '\0' &&
	    path[ENTITY_NS_PREFIX_LEN] != '/') {
		return (NULL);
	}
	return (path + ENTITY_NS_PREFIX_LEN);
}

static int
entity_ns_resolve_node(const char *path, u32 *out_node)
{
	const char	*p;
	char		comp[ENTITY_NS_COMPONENT_MAX];
	u32		node;
	int		len;

	if (!out_node) {
		return (-API_ERR_BAD_VALUE);
	}
	p = entity_ns_skip_prefix(path);
	if (!p) {
		return (-API_ERR_BAD_VALUE);
	}
	node = ENTITY_NS_ROOT;
	for (;;) {
		len = entity_ns_component(&p, comp, sizeof(comp));
		if (len < 0) {
			return (len);
		}
		if (len == 0) {
			break;
		}
		node = entity_ns_find_child(node, comp);
		if (node == ENTITY_NS_NONE) {
			return (-API_ERR_NOT_FOUND);
		}
	}
	*out_node = node;
	return (0);
}

static u32
entity_ns_find_by_entity(entity_id_t id)
{
	u32	node;

	for (node = 1; node < ENTITY_MAX_NS_NODES; node++) {
		if (entity_ns_used[node] &&
		    entity_ns_entity[node] == id) {
			return (node);
		}
	}
	return (ENTITY_NS_NONE);
}

static void
entity_ns_prune(u32 node)
{
	u32	parent;

	while (node != ENTITY_NS_ROOT && entity_ns_used[node] &&
	    entity_ns_entity[node] == 0 &&
	    entity_ns_first_child[node] == ENTITY_NS_NONE) {
		parent = entity_ns_parent[node];
		if (parent == ENTITY_NS_NONE ||
		    !entity_ns_used[parent]) {
			break;
		}
		if (entity_ns_first_child[parent] == node) {
			entity_ns_first_child[parent] =
			    entity_ns_next_sibling[node];
		} else {
			u32	prev, cur;

			prev = entity_ns_first_child[parent];
			cur = prev != ENTITY_NS_NONE ?
			    entity_ns_next_sibling[prev] : ENTITY_NS_NONE;
			while (cur != ENTITY_NS_NONE && cur != node) {
				prev = cur;
				cur = entity_ns_next_sibling[cur];
			}
			if (cur == node) {
				entity_ns_next_sibling[prev] =
				    entity_ns_next_sibling[node];
			}
		}
		entity_ns_node_free(node);
		node = parent;
	}
}

int
entity_ns_is_path(const char *name)
{
	if (!name) {
		return (0);
	}
	return (entity_ns_skip_prefix(name) != NULL);
}

const char *
entity_ns_name_of(entity_id_t id)
{
	u32	node;

	node = entity_ns_find_by_entity(id);
	if (node == ENTITY_NS_NONE) {
		return (NULL);
	}
	return (entity_str_get(entity_ns_name_off[node]));
}

int
entity_ns_bind(const char *path, entity_id_t id)
{
	const char	*p;
	char		comp[ENTITY_NS_COMPONENT_MAX];
	u32		node;
	u32		child;
	int		len;
	int		components;

	if (id == 0 || !entity_ns_is_path(path)) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	p = entity_ns_skip_prefix(path);
	node = ENTITY_NS_ROOT;
	components = 0;
	for (;;) {
		len = entity_ns_component(&p, comp, sizeof(comp));
		if (len < 0) {
			smp_unlock();
			return (len);
		}
		if (len == 0) {
			break;
		}
		components++;
		child = entity_ns_find_child(node, comp);
		if (child == ENTITY_NS_NONE) {
			child = entity_ns_node_alloc(node, comp);
			if (child == ENTITY_NS_NONE) {
				smp_unlock();
				return (-API_ERR_NO_SPACE);
			}
		}
		node = child;
	}
	if (components == 0) {
		smp_unlock();
		return (-API_ERR_BAD_VALUE);
	}
	if (entity_ns_entity[node] != 0) {
		smp_unlock();
		return (-API_ERR_EXISTS);
	}
	entity_ns_entity[node] = id;
	smp_unlock();
	return (0);
}

int
entity_ns_unbind(const char *path)
{
	u32	node;
	int	ret;

	smp_lock();
	ret = entity_ns_resolve_node(path, &node);
	if (ret != 0) {
		smp_unlock();
		return (ret);
	}
	if (entity_ns_entity[node] == 0) {
		smp_unlock();
		return (-API_ERR_NOT_FOUND);
	}
	entity_ns_entity[node] = 0;
	entity_ns_prune(node);
	smp_unlock();
	return (0);
}

int
entity_ns_unbind_id(entity_id_t id)
{
	u32	node;

	smp_lock();
	node = entity_ns_find_by_entity(id);
	if (node != ENTITY_NS_NONE) {
		entity_ns_entity[node] = 0;
		entity_ns_prune(node);
	}
	smp_unlock();
	return (0);
}

int
entity_ns_lookup(const char *path, entity_id_t *id)
{
	u32	node;
	int	ret;

	if (!id) {
		return (-API_ERR_BAD_VALUE);
	}
	*id = 0;
	smp_lock();
	ret = entity_ns_resolve_node(path, &node);
	if (ret != 0) {
		smp_unlock();
		return (ret);
	}
	*id = entity_ns_entity[node];
	smp_unlock();
	if (*id == 0) {
		return (-API_ERR_NOT_FOUND);
	}
	return (0);
}

int
entity_ns_list(const char *path, struct api_entity_entry *entries,
    u32 max_entries, u32 *count)
{
	const char	*p;
	char		comp[ENTITY_NS_COMPONENT_MAX];
	u32		node;
	u32		child;
	u32		out;
	int		len;

	if (!entries || !count) {
		return (-API_ERR_BAD_VALUE);
	}
	*count = 0;
	if (max_entries == 0) {
		return (0);
	}
	smp_lock();
	p = entity_ns_skip_prefix(path);
	if (!p) {
		smp_unlock();
		return (-API_ERR_BAD_VALUE);
	}
	node = ENTITY_NS_ROOT;
	for (;;) {
		len = entity_ns_component(&p, comp, sizeof(comp));
		if (len < 0) {
			smp_unlock();
			return (len);
		}
		if (len == 0) {
			break;
		}
		node = entity_ns_find_child(node, comp);
		if (node == ENTITY_NS_NONE) {
			smp_unlock();
			return (-API_ERR_NOT_FOUND);
		}
	}
	out = 0;
	for (child = entity_ns_first_child[node];
	    child != ENTITY_NS_NONE && out < max_entries;
	    child = entity_ns_next_sibling[child]) {
		const char	*name;
		u32		name_len;

		name = entity_str_get(entity_ns_name_off[child]);
		if (!name) {
			continue;
		}
		memset(&entries[out], 0, sizeof(entries[out]));
		entries[out].id = entity_ns_entity[child];
		if (entries[out].id != 0) {
			entries[out].archetype = entity_arch(entries[out].id);
			entries[out].state = entity_state(entries[out].id);
			entries[out].owner_pid = entity_owner(entries[out].id);
		}
		name_len = (u32)strlen(name);
		if (name_len >= sizeof(entries[out].name)) {
			name_len = (u32)sizeof(entries[out].name) - 1;
		}
		memcpy(entries[out].name, name, name_len);
		out++;
	}
	smp_unlock();
	*count = out;
	return (0);
}
