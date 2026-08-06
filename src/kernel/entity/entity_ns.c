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

$define %func entity_ns_component_valid as function with args const char *, int
$define %func entity_ns_canonical as function with args const char *, char *, u32
$define %func entity_ns_find_name as function with args const char *
$define %func entity_ns_find_entity as function with args entity_id
$define %func entity_ns_free_slot as procedure with args u32
$define %func entity_ns_init as procedure with args void
$define %func entity_ns_is_path as function with args const char *
$define %func entity_ns_name_of as function with args entity_id
$define %func entity_ns_bind as function with args const char *, entity_id
$define %func entity_ns_unbind as function with args const char *
$define %func entity_ns_unbind_id as function with args entity_id
$define %func entity_ns_lookup as function with args const char *, u64 *
$define %func entity_ns_list as function with args path, entries, count

*/

/* !SPACE!

$space %internal entity_ns_component_valid, entity_ns_canonical
$space %internal entity_ns_find_name, entity_ns_find_entity
$space %internal entity_ns_free_slot
$space %export entity_ns_init
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
#define	ENTITY_NS_COMPONENT_MAX	(ENTITY_NAME_MAX - 1)

static u32	entity_ns_used[ENTITY_MAX_NS_NODES];
static u64	entity_ns_entity[ENTITY_MAX_NS_NODES];
static char	entity_ns_names[ENTITY_MAX_NS_NODES][ENTITY_PATH_MAX];
static u32	entity_ns_free_next[ENTITY_MAX_NS_NODES];
static u32	entity_ns_free_head;

static int
entity_ns_component_valid(const char *comp, int len)
{
	if (len <= 0 || len >= ENTITY_NS_COMPONENT_MAX) {
		return (0);
	}
	if ((len == 1 && comp[0] == '.') ||
	    (len == 2 && comp[0] == '.' && comp[1] == '.')) {
		return (0);
	}
	return (1);
}

static int
entity_ns_canonical(const char *path, char *out, u32 out_size)
{
	const char	*p;
	u32		pos;
	int		len;

	if (!path || !out || out_size == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (strncmp(path, ENTITY_NS_PREFIX, ENTITY_NS_PREFIX_LEN) != 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (path[ENTITY_NS_PREFIX_LEN] != '\0' &&
	    path[ENTITY_NS_PREFIX_LEN] != '/') {
		return (-API_ERR_BAD_VALUE);
	}
	p = path + ENTITY_NS_PREFIX_LEN;
	if (*p == '\0') {
		if (ENTITY_NS_PREFIX_LEN + 1 > out_size) {
			return (-API_ERR_TOO_BIG);
		}
		memcpy(out, ENTITY_NS_PREFIX, ENTITY_NS_PREFIX_LEN + 1);
		return (0);
	}
	pos = ENTITY_NS_PREFIX_LEN;
	memcpy(out, ENTITY_NS_PREFIX, ENTITY_NS_PREFIX_LEN);
	out[pos] = '\0';
	while (*p != '\0') {
		const char	*start;

		while (*p == '/') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		start = p;
		while (*p != '\0' && *p != '/') {
			p++;
		}
		len = (int)(p - start);
		if (!entity_ns_component_valid(start, len)) {
			return (-API_ERR_BAD_VALUE);
		}
		if (pos + 1 + (u32)len + 1 > out_size) {
			return (-API_ERR_TOO_BIG);
		}
		out[pos++] = '/';
		memcpy(out + pos, start, (u32)len);
		pos += (u32)len;
		out[pos] = '\0';
	}
	return (0);
}

static int
entity_ns_find_name(const char *name)
{
	u32	i;

	for (i = 0; i < ENTITY_MAX_NS_NODES; i++) {
		if (entity_ns_used[i] &&
		    strcmp(entity_ns_names[i], name) == 0) {
			return ((int)i);
		}
	}
	return (-1);
}

static int
entity_ns_find_entity(entity_id_t id)
{
	u32	i;

	for (i = 0; i < ENTITY_MAX_NS_NODES; i++) {
		if (entity_ns_used[i] && entity_ns_entity[i] == id) {
			return ((int)i);
		}
	}
	return (-1);
}

static void
entity_ns_free_slot(u32 slot)
{
	entity_ns_used[slot] = 0;
	entity_ns_entity[slot] = 0;
	memset(entity_ns_names[slot], 0, sizeof(entity_ns_names[slot]));
	entity_ns_free_next[slot] = entity_ns_free_head;
	entity_ns_free_head = slot;
}

void
entity_ns_init(void)
{
	u32	i;

	memset(entity_ns_used, 0, sizeof(entity_ns_used));
	memset(entity_ns_entity, 0, sizeof(entity_ns_entity));
	memset(entity_ns_names, 0, sizeof(entity_ns_names));
	memset(entity_ns_free_next, 0, sizeof(entity_ns_free_next));
	entity_ns_free_head = 0;
	for (i = 0; i < ENTITY_MAX_NS_NODES - 1; i++) {
		entity_ns_free_next[i] = i + 1;
	}
	entity_ns_free_next[ENTITY_MAX_NS_NODES - 1] = ENTITY_NS_NONE;
}

int
entity_ns_is_path(const char *name)
{
	char	canon[ENTITY_PATH_MAX];

	if (!name) {
		return (0);
	}
	return (entity_ns_canonical(name, canon, sizeof(canon)) == 0);
}

const char *
entity_ns_name_of(entity_id_t id)
{
	int	slot;

	slot = entity_ns_find_entity(id);
	if (slot < 0) {
		return (NULL);
	}
	return (entity_ns_names[(u32)slot]);
}

int
entity_ns_bind(const char *path, entity_id_t id)
{
	char	canon[ENTITY_PATH_MAX];
	u32	slot;
	int	ret;

	if (id == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = entity_ns_canonical(path, canon, sizeof(canon));
	if (ret != 0) {
		return (ret);
	}
	smp_lock();
	if (entity_ns_find_name(canon) >= 0) {
		smp_unlock();
		return (-API_ERR_EXISTS);
	}
	if (entity_ns_free_head == ENTITY_NS_NONE) {
		smp_unlock();
		return (-API_ERR_NO_SPACE);
	}
	slot = entity_ns_free_head;
	entity_ns_free_head = entity_ns_free_next[slot];
	entity_ns_free_next[slot] = ENTITY_NS_NONE;
	entity_ns_used[slot] = 1;
	entity_ns_entity[slot] = id;
	memset(entity_ns_names[slot], 0, sizeof(entity_ns_names[slot]));
	memcpy(entity_ns_names[slot], canon, strlen(canon) + 1);
	smp_unlock();
	return (0);
}

int
entity_ns_unbind(const char *path)
{
	char	canon[ENTITY_PATH_MAX];
	int	slot;
	int	ret;

	ret = entity_ns_canonical(path, canon, sizeof(canon));
	if (ret != 0) {
		return (ret);
	}
	smp_lock();
	slot = entity_ns_find_name(canon);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_NOT_FOUND);
	}
	entity_ns_free_slot((u32)slot);
	smp_unlock();
	return (0);
}

int
entity_ns_unbind_id(entity_id_t id)
{
	int	slot;

	smp_lock();
	slot = entity_ns_find_entity(id);
	if (slot >= 0) {
		entity_ns_free_slot((u32)slot);
	}
	smp_unlock();
	return (0);
}

int
entity_ns_lookup(const char *path, entity_id_t *id)
{
	char	canon[ENTITY_PATH_MAX];
	int	slot;
	int	ret;

	if (!id) {
		return (-API_ERR_BAD_VALUE);
	}
	*id = 0;
	ret = entity_ns_canonical(path, canon, sizeof(canon));
	if (ret != 0) {
		return (ret);
	}
	smp_lock();
	slot = entity_ns_find_name(canon);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_NOT_FOUND);
	}
	*id = entity_ns_entity[(u32)slot];
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
	char		prefix[ENTITY_PATH_MAX + 1];
	u32		prefix_len;
	u32		out;
	u32		i;
	int		ret;

	if (!entries || !count) {
		return (-API_ERR_BAD_VALUE);
	}
	*count = 0;
	if (max_entries == 0) {
		return (0);
	}
	ret = entity_ns_canonical(path, prefix, ENTITY_PATH_MAX);
	if (ret != 0) {
		return (ret);
	}
	prefix_len = (u32)strlen(prefix);
	if (prefix_len + 1 > sizeof(prefix)) {
		return (-API_ERR_TOO_BIG);
	}
	prefix[prefix_len] = '/';
	prefix[prefix_len + 1] = '\0';
	prefix_len++;

	smp_lock();
	out = 0;
	for (i = 0; i < ENTITY_MAX_NS_NODES && out < max_entries; i++) {
		const char	*name;
		const char	*rest;
		const char	*comp_end;
		u32		comp_len;
		u32		j;
		int		seen;

		if (!entity_ns_used[i]) {
			continue;
		}
		name = entity_ns_names[i];
		if (strncmp(name, prefix, prefix_len) != 0) {
			continue;
		}
		rest = name + prefix_len;
		if (*rest == '\0') {
			continue;
		}
		comp_end = rest;
		while (*comp_end != '\0' && *comp_end != '/') {
			comp_end++;
		}
		comp_len = (u32)(comp_end - rest);
		if (comp_len == 0) {
			continue;
		}
		seen = 0;
		for (j = 0; j < out; j++) {
			if (strncmp(entries[j].name, rest, comp_len) == 0 &&
			    entries[j].name[comp_len] == '\0') {
				seen = 1;
				break;
			}
		}
		if (seen) {
			continue;
		}
		memset(&entries[out], 0, sizeof(entries[out]));
		if (*comp_end == '\0') {
			entries[out].id = entity_ns_entity[i];
			if (entries[out].id != 0) {
				entries[out].archetype =
				    entity_arch(entries[out].id);
				entries[out].state =
				    entity_state(entries[out].id);
				entries[out].owner_pid =
				    entity_owner(entries[out].id);
			}
		}
		memcpy(entries[out].name, rest, comp_len);
		entries[out].name[comp_len] = '\0';
		out++;
	}
	smp_unlock();
	*count = out;
	return (0);
}
