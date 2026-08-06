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
$define %type s32 as 32 bit signed
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type process as struct with process control block

$define %func entity_init as procedure with args void
$define %func entity_is_initialized as function with args void
$define %func entity_create as function with args archetype, flags, credentials
$define %func entity_destroy as function with args entity id
$define %func entity_retain as procedure with args entity id
$define %func entity_release as procedure with args entity id
$define %func entity_valid as function with args entity id
$define %func entity_arch as function with args entity id
$define %func entity_state as function with args entity id
$define %func entity_refs as function with args entity id
$define %func entity_flags as function with args entity id
$define %func entity_owner as function with args entity id
$define %func entity_uid as function with args entity id
$define %func entity_gid as function with args entity id
$define %func entity_euid as function with args entity id
$define %func entity_egid as function with args entity id
$define %func entity_size as function with args entity id
$define %func entity_created as function with args entity id
$define %func entity_set_size as function with args entity id, u64
$define %func entity_get_data as function with args entity id, index, u64 *
$define %func entity_set_data as function with args entity id, index, u64
$define %func entity_get_i32 as function with args entity id, index, s32 *
$define %func entity_set_i32 as function with args entity id, index, s32
$define %func entity_name as function with args entity id, char *, u32
$define %func entity_access as function with args process *, entity id, u32
$define %func entity_foreach as function with args archetype, start, callback
$define %func entity_dump as procedure with args void
$define %func entity_event_set_notify as procedure with args callback

$define %func entity_handle_init_process as procedure with args process *
$define %func entity_handle_alloc as function with args process *, entity id, u32
$define %func entity_handle_lookup as function with args process *, int, u64 *, u32 *
$define %func entity_handle_free as function with args process *, int
$define %func entity_handle_dup as function with args process *, int, u32
$define %func entity_handle_copy_all as function with args process *, process *
$define %func entity_handle_release_all as procedure with args process *
$define %func entity_handle_drop as function with args process *, int, int
$define %func entity_handle_foreach as function with args process *, callback

$define %func entity_ns_is_path as function with args const char *
$define %func entity_ns_init as procedure with args void
$define %func entity_ns_name_of as function with args entity id
$define %func entity_ns_bind as function with args const char *, entity id
$define %func entity_ns_unbind as function with args const char *
$define %func entity_ns_unbind_id as function with args entity id
$define %func entity_ns_lookup as function with args const char *, u64 *
$define %func entity_ns_list as function with args path, entries, count

*/

/* !SPACE!

$space %export entity_init, entity_is_initialized
$space %export entity_create, entity_destroy, entity_retain, entity_release
$space %export entity_valid, entity_arch, entity_state, entity_refs
$space %export entity_flags, entity_owner, entity_size, entity_set_size
$space %export entity_uid, entity_gid, entity_euid, entity_egid
$space %export entity_created
$space %export entity_get_data, entity_set_data
$space %export entity_get_i32, entity_set_i32
$space %export entity_name, entity_access, entity_foreach, entity_dump
$space %export entity_event_set_notify
$space %export entity_handle_init_process, entity_handle_alloc
$space %export entity_handle_lookup, entity_handle_free, entity_handle_dup
$space %export entity_handle_copy_all, entity_handle_release_all
$space %export entity_handle_drop
$space %export entity_handle_foreach
$space %export entity_ns_is_path, entity_ns_init, entity_ns_name_of
$space %export entity_ns_bind, entity_ns_unbind
$space %export entity_ns_unbind_id, entity_ns_lookup, entity_ns_list

*/

#ifndef KERNEL_ENTITY_ENTITY_H
#define KERNEL_ENTITY_ENTITY_H

#include <mlibc/mlibc.h>

#define	ENTITY_MAX_ENTITIES		2048
#define	ENTITY_MAX_ARCHETYPES		64
#define	ENTITY_MAX_HANDLES		1024
#define	ENTITY_MAX_NS_NODES		2048
#define	ENTITY_STRING_ARENA_SIZE	(64 * 1024)
#define	ENTITY_NAME_MAX			64
#define	ENTITY_PATH_MAX			256
#define	ENTITY_DATA_COUNT		8
#define	ENTITY_I32_COUNT		8
#define	ENTITY_LIST_MAX_ENTRIES		256

#define	ENTITY_ARCH_GENERIC		1
#define	ENTITY_ARCH_FILE		2
#define	ENTITY_ARCH_PIPE		3
#define	ENTITY_ARCH_VNODE		4
#define	ENTITY_ARCH_NET			5
#define	ENTITY_ARCH_IPC			6
#define	ENTITY_ARCH_REG			7
#define	ENTITY_ARCH_KQUEUE		8
#define	ENTITY_ARCH_SHM			9
#define	ENTITY_ARCH_TRACE		10
#define	ENTITY_ARCH_GEM			11
#define	ENTITY_ARCH_KOFO		12
#define	ENTITY_ARCH_NB_INTERFACE	13
#define	ENTITY_ARCH_PROCESS		14
#define	ENTITY_ARCH_THREAD		15
#define	ENTITY_ARCH_TTY			16
#define	ENTITY_ARCH_DRM			17
#define	ENTITY_ARCH_PTY			18
#define	ENTITY_ARCH_MAX			63

#define	ENTITY_STATE_ACTIVE		1
#define	ENTITY_STATE_DELETED		2

#define	ENTITY_ACCESS_READ		0x00000001
#define	ENTITY_ACCESS_WRITE		0x00000002
#define	ENTITY_ACCESS_EXEC		0x00000004
#define	ENTITY_ACCESS_ALL		(ENTITY_ACCESS_READ | \
					    ENTITY_ACCESS_WRITE | \
					    ENTITY_ACCESS_EXEC)
#define	ENTITY_ACCESS_DEFAULT		(ENTITY_ACCESS_READ | \
					    ENTITY_ACCESS_WRITE)

#define	ENTITY_EVENT_CREATE		0x00000001
#define	ENTITY_EVENT_DESTROY		0x00000002
#define	ENTITY_EVENT_RETAIN		0x00000004
#define	ENTITY_EVENT_RELEASE		0x00000008
#define	ENTITY_EVENT_STATE		0x00000010

#define	ENTITY_NS_PREFIX		"/Entity"
#define	ENTITY_NS_PREFIX_LEN		7

struct process;
struct api_entity_entry;

typedef u64 entity_id_t;
typedef void (*entity_release_fn)(entity_id_t id);

static inline u16
entity_id_archetype(entity_id_t id)
{
	return ((u16)(id >> 48));
}

static inline u16
entity_id_generation(entity_id_t id)
{
	return ((u16)(id >> 32));
}

static inline u32
entity_id_index(entity_id_t id)
{
	return ((u32)id);
}

static inline entity_id_t
entity_id_make(u16 arch, u16 gen, u32 index)
{
	return (((entity_id_t)(arch & 0xFFFF) << 48) |
	    ((entity_id_t)(gen & 0xFFFF) << 32) |
	    (entity_id_t)(index & 0xFFFFFFFF));
}

void	entity_init(void);
int	entity_is_initialized(void);
int	entity_arch_release_register(u16 arch, entity_release_fn fn);

entity_id_t entity_create(u16 arch, u32 flags, u32 owner_pid, u32 uid,
	    u32 gid, u32 euid, u32 egid, int kusr);
int	entity_destroy(entity_id_t id);
void	entity_retain(entity_id_t id);
void	entity_release(entity_id_t id);
int	entity_valid(entity_id_t id);
u16	entity_arch(entity_id_t id);
u32	entity_state(entity_id_t id);
s32	entity_refs(entity_id_t id);
u32	entity_flags(entity_id_t id);
u32	entity_owner(entity_id_t id);
u32	entity_uid(entity_id_t id);
u32	entity_gid(entity_id_t id);
u32	entity_euid(entity_id_t id);
u32	entity_egid(entity_id_t id);
u64	entity_size(entity_id_t id);
u64	entity_created(entity_id_t id);
int	entity_set_size(entity_id_t id, u64 size);
int	entity_get_data(entity_id_t id, u32 index, u64 *value);
int	entity_set_data(entity_id_t id, u32 index, u64 value);
int	entity_get_i32(entity_id_t id, u32 index, s32 *value);
int	entity_set_i32(entity_id_t id, u32 index, s32 value);
int	entity_name(entity_id_t id, char *buf, u32 bufsize);
int	entity_access(const struct process *proc, entity_id_t id, u32 want);
int	entity_foreach(u16 arch, u32 start,
	    int (*cb)(entity_id_t id, void *ctx), void *ctx);
void	entity_dump(void);
void	entity_event_set_notify(void (*fn)(entity_id_t id, u32 fflags));

void	entity_handle_init_process(struct process *proc);
int	entity_handle_alloc(struct process *proc, entity_id_t id, u32 access);
int	entity_handle_lookup(const struct process *proc, int handle,
	    entity_id_t *id, u32 *access);
int	entity_handle_free(struct process *proc, int handle);
int	entity_handle_dup(struct process *proc, int handle, u32 access);
int	entity_handle_copy_all(struct process *dst, const struct process *src);
void	entity_handle_release_all(struct process *proc);
int	entity_handle_drop(struct process *proc, int handle, int release);
int	entity_handle_foreach(const struct process *proc,
	    int (*cb)(int handle, entity_id_t id, u32 access, void *ctx),
	    void *ctx);

int	entity_ns_is_path(const char *name);
void	entity_ns_init(void);
const char *entity_ns_name_of(entity_id_t id);
int	entity_ns_bind(const char *name, entity_id_t id);
int	entity_ns_unbind(const char *name);
int	entity_ns_unbind_id(entity_id_t id);
int	entity_ns_lookup(const char *name, entity_id_t *id);
int	entity_ns_list(const char *path, struct api_entity_entry *entries,
	    u32 max_entries, u32 *count);

#endif
