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
 * and/or other materials provided with the distribution.
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
$define %type int as 32 bit signed
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type process as struct with process control block
$define %type entity_meta_block as block of entity metadata SoA columns

$define %func entity_slot as function with args entity id
$define %func entity_free_push as procedure with args u32
$define %func entity_free_pop as function with args void
$define %func entity_fill_name as function with args entity id, char *, u32
$define %func entity_init as procedure with args void
$define %func entity_arch_release_register as function with args archetype, callback
$define %func entity_create as function with args archetype, flags, credentials
$define %func entity_attach as function with args archetype, index, flags, credentials
$define %func entity_meta_register as function with args archetype, meta block, base, count
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
$define %func entity_arch_io_register as function with args archetype, ops
$define %func entity_arch_io_get as function with args archetype

*/

/* !SPACE!

$space %internal entity_slot, entity_free_push, entity_free_pop
$space %internal entity_fill_name
$space %export entity_init, entity_is_initialized
$space %export entity_arch_release_register
$space %export entity_create, entity_attach, entity_meta_register
$space %export entity_destroy, entity_retain, entity_release
$space %export entity_valid, entity_arch, entity_state, entity_refs
$space %export entity_flags, entity_owner, entity_size, entity_set_size
$space %export entity_uid, entity_gid, entity_euid, entity_egid
$space %export entity_created
$space %export entity_get_data, entity_set_data
$space %export entity_get_i32, entity_set_i32
$space %export entity_name, entity_access, entity_foreach, entity_dump
$space %export entity_event_set_notify
$space %export entity_arch_io_register
$space %export entity_arch_io_get

*/

#include <kernel/api/errno.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <kernel/trace/trace.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	ENTITY_SLOT_NONE	0xFFFFFFFFU
#define	ENTITY_REF_MAX		0x7FFFFFF0

static int		entity_initialized;
static entity_meta_block_t	entity_blocks[ENTITY_BLOCK_COUNT];
static u32		entity_free_next[ENTITY_MAX_ENTITIES];
static u32		entity_free_head;
static u32		entity_count;
static u64		entity_refs_saturated;
static const char	*entity_arch_names[ENTITY_MAX_ARCHETYPES];
static entity_release_fn	entity_arch_release[ENTITY_MAX_ARCHETYPES];
static const entity_io_ops_t	*entity_arch_io[ENTITY_MAX_ARCHETYPES];
static void		(*entity_event_notify)(entity_id_t id, u32 fflags);

typedef struct entity_arch_block {
	entity_meta_block_t	*meta;
	u32			base;
	u32			count;
} entity_arch_block_t;

static entity_arch_block_t	entity_arch_blocks[ENTITY_MAX_ARCHETYPES];

static spin_t		entity_spin = SPIN_INITIALIZER("entity", LO_ENTITY);

static void
entity_trace_notify(entity_id_t id, u32 fflags)
{
	u64	args[TRACE_MAX_ARGS];
	u32	probe;

	if (!trace_is_initialized()) {
		return;
	}
	memset(args, 0, sizeof(args));
	args[0] = id;
	args[1] = entity_arch(id);
	args[2] = fflags;
	args[3] = (u64)entity_refs(id);
	probe = TRACE_PROBE_ENTITY_RELEASE;
	if (fflags & ENTITY_EVENT_CREATE) {
		probe = TRACE_PROBE_ENTITY_CREATE;
	} else if (fflags & ENTITY_EVENT_DESTROY) {
		probe = TRACE_PROBE_ENTITY_DESTROY;
	} else if (fflags & ENTITY_EVENT_RETAIN) {
		probe = TRACE_PROBE_ENTITY_RETAIN;
	}
	trace_probe_fire(probe, 0, NULL, args);
}

static int
entity_slot(entity_id_t id, entity_meta_block_t **out_block, u32 *out_slot)
{
	u16		arch;
	u32		index, slot, base, count;
	entity_meta_block_t	*block;

	arch = entity_id_archetype(id);
	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (-1);
	}
	index = entity_id_index(id);
	block = entity_arch_blocks[arch].meta;
	base = entity_arch_blocks[arch].base;
	count = entity_arch_blocks[arch].count;
	if (count == 0 || index < base || index - base >= count) {
		return (-1);
	}
	if (block == NULL) {
		block = &entity_blocks[index >> ENTITY_BLOCK_SHIFT];
		slot = index & (ENTITY_BLOCK_ENTRIES - 1);
	} else {
		slot = index - base;
	}
	if (!block->used[slot]) {
		return (-1);
	}
	if (block->gen[slot] != entity_id_generation(id)) {
		return (-1);
	}
	if (block->arch[slot] != arch) {
		return (-1);
	}
	*out_block = block;
	*out_slot = slot;
	return (0);
}

static void
entity_free_push(u32 index)
{
	entity_free_next[index] = entity_free_head;
	entity_free_head = index;
}

static u32
entity_free_pop(void)
{
	u32	index;

	if (entity_free_head == ENTITY_SLOT_NONE) {
		return (ENTITY_SLOT_NONE);
	}
	index = entity_free_head;
	entity_free_head = entity_free_next[index];
	entity_free_next[index] = ENTITY_SLOT_NONE;
	return (index);
}

static void
entity_meta_init(u16 arch, entity_meta_block_t *meta, u32 base, u32 count)
{
	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return;
	}
	entity_arch_blocks[arch].meta = meta;
	entity_arch_blocks[arch].base = base;
	entity_arch_blocks[arch].count = count;
}

int
entity_meta_register(u16 arch, entity_meta_block_t *meta, u32 base,
    u32 count)
{
	if (arch == 0 || arch > ENTITY_ARCH_MAX || meta == NULL ||
	    count == 0 || count > ENTITY_BLOCK_ENTRIES) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	entity_meta_init(arch, meta, base, count);
	spin_unlock(&entity_spin);
	return (0);
}

static int
entity_fill_name(entity_id_t id, char *buf, u32 bufsize)
{
	if (!buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	if (entity_ns_name_copy(id, buf, bufsize) != 0) {
		buf[0] = '\0';
	}
	return (0);
}

void
entity_init(void)
{
	u32	i;

	if (entity_initialized) {
		return;
	}
	memset(entity_blocks, 0, sizeof(entity_blocks));
	memset(entity_arch_names, 0, sizeof(entity_arch_names));
	memset(entity_arch_release, 0, sizeof(entity_arch_release));
	memset(entity_arch_io, 0, sizeof(entity_arch_io));
	memset(entity_arch_blocks, 0, sizeof(entity_arch_blocks));
	entity_handle_init();

	entity_free_head = 0;
	for (i = 0; i < ENTITY_MAX_ENTITIES; i++) {
		entity_free_next[i] = i + 1;
	}
	entity_free_next[ENTITY_MAX_ENTITIES - 1] = ENTITY_SLOT_NONE;
	entity_count = 0;
	for (i = 1; i <= ENTITY_ARCH_MAX; i++) {
		entity_meta_init(i, NULL, 0, ENTITY_MAX_ENTITIES);
	}
	entity_ns_init();
	entity_arch_names[0] = "none";
	entity_arch_names[ENTITY_ARCH_GENERIC] = "generic";
	entity_arch_names[ENTITY_ARCH_FILE] = "file";
	entity_arch_names[ENTITY_ARCH_PIPE] = "pipe";
	entity_arch_names[ENTITY_ARCH_VNODE] = "vnode";
	entity_arch_names[ENTITY_ARCH_NET] = "net";
	entity_arch_names[ENTITY_ARCH_IPC] = "ipc";
	entity_arch_names[ENTITY_ARCH_REG] = "reg";
	entity_arch_names[ENTITY_ARCH_KQUEUE] = "kqueue";
	entity_arch_names[ENTITY_ARCH_SHM] = "shm";
	entity_arch_names[ENTITY_ARCH_TRACE] = "trace";
	entity_arch_names[ENTITY_ARCH_GEM] = "gem";
	entity_arch_names[ENTITY_ARCH_KOFO] = "kofo";
	entity_arch_names[ENTITY_ARCH_NB_INTERFACE] = "nb_interface";
	entity_arch_names[ENTITY_ARCH_PROCESS] = "process";
	entity_arch_names[ENTITY_ARCH_THREAD] = "thread";
	entity_arch_names[ENTITY_ARCH_TTY] = "tty";
	entity_arch_names[ENTITY_ARCH_DRM] = "drm";
	entity_arch_names[ENTITY_ARCH_PTY] = "pty";
	entity_arch_names[ENTITY_ARCH_NB_DEVICE] = "nb_device";
	entity_initialized = 1;
	drivers_log("[ENTITY] initialized: %d slots (%d blocks x %d), "
	    "%d handles, %d namespace nodes\n", ENTITY_MAX_ENTITIES,
	    ENTITY_BLOCK_COUNT, ENTITY_BLOCK_ENTRIES,
	    ENTITY_MAX_HANDLES, ENTITY_MAX_NS_NODES);
}

int
entity_slot_used(u16 arch, u32 index)
{
	entity_meta_block_t	*block;
	u32			slot, base, count;
	int			used;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (0);
	}
	spin_lock(&entity_spin);
	block = entity_arch_blocks[arch].meta;
	base = entity_arch_blocks[arch].base;
	count = entity_arch_blocks[arch].count;
	if (block == NULL || index < base || index - base >= count) {
		spin_unlock(&entity_spin);
		return (0);
	}
	slot = index - base;
	used = block->used[slot] ? 1 : 0;
	spin_unlock(&entity_spin);
	return (used);
}

entity_id_t
entity_id_at(u16 arch, u32 index)
{
	entity_meta_block_t	*block;
	entity_id_t		id;
	u32			slot, base, count;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (0);
	}
	spin_lock(&entity_spin);
	block = entity_arch_blocks[arch].meta;
	base = entity_arch_blocks[arch].base;
	count = entity_arch_blocks[arch].count;
	if (block == NULL || index < base || index - base >= count) {
		spin_unlock(&entity_spin);
		return (0);
	}
	slot = index - base;
	if (!block->used[slot] || block->arch[slot] != arch) {
		spin_unlock(&entity_spin);
		return (0);
	}
	id = entity_id_make(arch, block->gen[slot], index);
	spin_unlock(&entity_spin);
	return (id);
}

u64
entity_saturations(void)
{
	u64	value;

	spin_lock(&entity_spin);
	value = entity_refs_saturated;
	spin_unlock(&entity_spin);
	return (value);
}

int
entity_arch_release_register(u16 arch, entity_release_fn fn)
{
	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	entity_arch_release[arch] = fn;
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_arch_io_register(u16 arch, const entity_io_ops_t *ops)
{
	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	entity_arch_io[arch] = ops;
	spin_unlock(&entity_spin);
	return (0);
}

const entity_io_ops_t *
entity_arch_io_get(u16 arch)
{
	const entity_io_ops_t	*ops;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (NULL);
	}
	spin_lock(&entity_spin);
	ops = entity_arch_io[arch];
	spin_unlock(&entity_spin);
	return (ops);
}

int
entity_is_initialized(void)
{
	return (entity_initialized);
}

entity_id_t
entity_create(u16 arch, u32 flags, u32 owner_pid, u32 uid, u32 gid,
    u32 euid, u32 egid, int kusr)
{
	entity_id_t	id;
	u32		index, block_idx, slot;
	entity_meta_block_t	*block;
	u16		gen;
	u64		created;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (0);
	}
	created = timer_is_initialized() ? timer_get_ticks() : 0;
	spin_lock(&entity_spin);
	if (!entity_initialized) {
		spin_unlock(&entity_spin);
		return (0);
	}
	/* Archetypes with inline blocks are owned by their subsystem. */
	if (entity_arch_blocks[arch].meta != NULL) {
		spin_unlock(&entity_spin);
		return (0);
	}
	index = entity_free_pop();
	if (index == ENTITY_SLOT_NONE) {
		spin_unlock(&entity_spin);
		return (0);
	}
	block_idx = index >> ENTITY_BLOCK_SHIFT;
	slot = index & (ENTITY_BLOCK_ENTRIES - 1);
	block = &entity_blocks[block_idx];
	gen = block->gen[slot] + 1;
	if (gen == 0) {
		gen = 1;
	}
	block->gen[slot] = gen;
	block->arch[slot] = arch;
	block->used[slot] = 1;
	block->state[slot] = ENTITY_STATE_ACTIVE;
	block->flags[slot] = flags;
	block->refs[slot] = 1;
	block->owner[slot] = owner_pid;
	block->uid[slot] = uid;
	block->gid[slot] = gid;
	block->euid[slot] = euid;
	block->egid[slot] = egid;
	block->kusr[slot] = kusr ? 1 : 0;
	block->size[slot] = 0;
	block->born[slot] = created;
	block->name_off[slot] = 0;
	entity_count++;
	id = entity_id_make(arch, gen, index);
	spin_unlock(&entity_spin);
	entity_trace_notify(id, ENTITY_EVENT_CREATE);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_CREATE);
	}
	return (id);
}

entity_id_t
entity_attach(u16 arch, u32 index, u32 flags, u32 owner_pid, u32 uid,
    u32 gid, u32 euid, u32 egid, int kusr)
{
	entity_id_t	id;
	entity_meta_block_t	*block;
	u32		slot, base, count;
	u16		gen;
	u64		created;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (0);
	}
	created = timer_is_initialized() ? timer_get_ticks() : 0;
	spin_lock(&entity_spin);
	if (!entity_initialized) {
		spin_unlock(&entity_spin);
		return (0);
	}
	block = entity_arch_blocks[arch].meta;
	base = entity_arch_blocks[arch].base;
	count = entity_arch_blocks[arch].count;
	if (block == NULL || index < base || index - base >= count) {
		spin_unlock(&entity_spin);
		return (0);
	}
	slot = index - base;
	if (block->used[slot]) {
		spin_unlock(&entity_spin);
		return (0);
	}
	gen = block->gen[slot] + 1;
	if (gen == 0) {
		gen = 1;
	}
	block->gen[slot] = gen;
	block->arch[slot] = arch;
	block->used[slot] = 1;
	block->state[slot] = ENTITY_STATE_ACTIVE;
	block->flags[slot] = flags;
	block->refs[slot] = 1;
	block->owner[slot] = owner_pid;
	block->uid[slot] = uid;
	block->gid[slot] = gid;
	block->euid[slot] = euid;
	block->egid[slot] = egid;
	block->kusr[slot] = kusr ? 1 : 0;
	block->size[slot] = 0;
	block->born[slot] = created;
	block->name_off[slot] = 0;
	entity_count++;
	id = entity_id_make(arch, gen, index);
	spin_unlock(&entity_spin);
	entity_trace_notify(id, ENTITY_EVENT_CREATE);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_CREATE);
	}
	return (id);
}

void
entity_event_set_notify(void (*fn)(entity_id_t id, u32 fflags))
{
	spin_lock(&entity_spin);
	entity_event_notify = fn;
	spin_unlock(&entity_spin);
}

int
entity_destroy(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			slot;
	int			unbind;

	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	unbind = 0;
	if (block->state[slot] == ENTITY_STATE_ACTIVE) {
		block->state[slot] = ENTITY_STATE_DELETED;
		unbind = 1;
	}
	spin_unlock(&entity_spin);
	if (unbind) {
		entity_ns_unbind_all_id(id);
	}
	entity_trace_notify(id, ENTITY_EVENT_STATE | ENTITY_EVENT_DESTROY);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_STATE |
		    ENTITY_EVENT_DESTROY);
	}
	entity_release(id);
	return (0);
}

int
entity_retain_checked(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			slot;
	int			saturated;

	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	if (block->state[slot] == ENTITY_STATE_DYING) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	if (block->refs[slot] < ENTITY_REF_MAX) {
		block->refs[slot]++;
		saturated = 0;
	} else {
		entity_refs_saturated++;
		saturated = 1;
	}
	spin_unlock(&entity_spin);
	if (saturated) {
		printk("[ENTITY] refcount saturated on id=%p arch=%u: "
		    "slot leaked for the lifetime of the system\n",
		    (void *)id, (unsigned)entity_id_archetype(id));
		return (-API_ERR_RETRY);
	}
	entity_trace_notify(id, ENTITY_EVENT_RETAIN);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_RETAIN);
	}
	return (0);
}

void
entity_retain(entity_id_t id)
{
	(void)entity_retain_checked(id);
}

void
entity_release(entity_id_t id)
{
	entity_release_fn	hook;
	entity_meta_block_t	*block;
	u32			slot, index;
	u16			arch;

	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return;
	}
	arch = entity_id_archetype(id);
	index = entity_id_index(id);
	if (block->refs[slot] > 0) {
		block->refs[slot]--;
	}
	if (block->refs[slot] != 0) {
		spin_unlock(&entity_spin);
		entity_trace_notify(id, ENTITY_EVENT_RELEASE);
		if (entity_event_notify) {
			entity_event_notify(id, ENTITY_EVENT_RELEASE);
		}
		return;
	}
	if (block->state[slot] == ENTITY_STATE_DYING) {
		spin_unlock(&entity_spin);
		return;
	}
	block->state[slot] = ENTITY_STATE_DYING;
	hook = entity_arch_release[arch];
	spin_unlock(&entity_spin);

	if (hook != NULL) {
		hook(id);
	}
	entity_ns_unbind_all_id(id);

	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) == 0 &&
	    block->state[slot] == ENTITY_STATE_DYING) {
		block->used[slot] = 0;
		entity_count--;
		if (entity_arch_blocks[arch].meta == NULL) {
			entity_free_push(index);
		}
	}
	spin_unlock(&entity_spin);
	entity_trace_notify(id, ENTITY_EVENT_RELEASE | ENTITY_EVENT_DESTROY);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_RELEASE |
		    ENTITY_EVENT_DESTROY);
	}
}

int
entity_valid(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			slot;
	int			valid;

	spin_lock(&entity_spin);
	valid = entity_slot(id, &block, &slot) == 0 ? 1 : 0;
	spin_unlock(&entity_spin);
	return (valid);
}

u16
entity_arch(entity_id_t id)
{
	entity_meta_block_t	*block;
	u16			arch;
	u32			slot;

	spin_lock(&entity_spin);
	arch = entity_slot(id, &block, &slot) == 0 ?
	    block->arch[slot] : 0;
	spin_unlock(&entity_spin);
	return (arch);
}

u32
entity_state(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			state;
	u32			slot;

	spin_lock(&entity_spin);
	state = entity_slot(id, &block, &slot) == 0 ?
	    block->state[slot] : 0;
	spin_unlock(&entity_spin);
	return (state);
}

s32
entity_refs(entity_id_t id)
{
	entity_meta_block_t	*block;
	s32			refs;
	u32			slot;

	spin_lock(&entity_spin);
	refs = entity_slot(id, &block, &slot) == 0 ?
	    block->refs[slot] : 0;
	spin_unlock(&entity_spin);
	return (refs);
}

u32
entity_flags(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			flags;
	u32			slot;

	spin_lock(&entity_spin);
	flags = entity_slot(id, &block, &slot) == 0 ?
	    block->flags[slot] : 0;
	spin_unlock(&entity_spin);
	return (flags);
}

u32
entity_owner(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			owner;
	u32			slot;

	spin_lock(&entity_spin);
	owner = entity_slot(id, &block, &slot) == 0 ?
	    block->owner[slot] : 0;
	spin_unlock(&entity_spin);
	return (owner);
}

u32
entity_uid(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			value;
	u32			slot;

	spin_lock(&entity_spin);
	value = entity_slot(id, &block, &slot) == 0 ?
	    block->uid[slot] : 0;
	spin_unlock(&entity_spin);
	return (value);
}

u32
entity_gid(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			value;
	u32			slot;

	spin_lock(&entity_spin);
	value = entity_slot(id, &block, &slot) == 0 ?
	    block->gid[slot] : 0;
	spin_unlock(&entity_spin);
	return (value);
}

u32
entity_euid(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			value;
	u32			slot;

	spin_lock(&entity_spin);
	value = entity_slot(id, &block, &slot) == 0 ?
	    block->euid[slot] : 0;
	spin_unlock(&entity_spin);
	return (value);
}

u32
entity_egid(entity_id_t id)
{
	entity_meta_block_t	*block;
	u32			value;
	u32			slot;

	spin_lock(&entity_spin);
	value = entity_slot(id, &block, &slot) == 0 ?
	    block->egid[slot] : 0;
	spin_unlock(&entity_spin);
	return (value);
}

u64
entity_size(entity_id_t id)
{
	entity_meta_block_t	*block;
	u64			size;
	u32			slot;

	spin_lock(&entity_spin);
	size = entity_slot(id, &block, &slot) == 0 ?
	    block->size[slot] : 0;
	spin_unlock(&entity_spin);
	return (size);
}

u64
entity_created(entity_id_t id)
{
	entity_meta_block_t	*block;
	u64			created;
	u32			slot;

	spin_lock(&entity_spin);
	created = entity_slot(id, &block, &slot) == 0 ?
	    block->born[slot] : 0;
	spin_unlock(&entity_spin);
	return (created);
}

int
entity_set_size(entity_id_t id, u64 size)
{
	entity_meta_block_t	*block;
	u32			slot;

	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	block->size[slot] = size;
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_get_data(entity_id_t id, u32 index, u64 *value)
{
	entity_meta_block_t	*block;
	u32			slot;

	if (!value) {
		return (-API_ERR_BAD_VALUE);
	}
	if (index >= ENTITY_DATA_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	*value = block->data[index][slot];
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_set_data(entity_id_t id, u32 index, u64 value)
{
	entity_meta_block_t	*block;
	u32			slot;

	if (index >= ENTITY_DATA_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	block->data[index][slot] = value;
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_get_i32(entity_id_t id, u32 index, s32 *value)
{
	entity_meta_block_t	*block;
	u32			slot;

	if (!value) {
		return (-API_ERR_BAD_VALUE);
	}
	if (index >= ENTITY_I32_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	*value = block->i32[index][slot];
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_set_i32(entity_id_t id, u32 index, s32 value)
{
	entity_meta_block_t	*block;
	u32			slot;

	if (index >= ENTITY_I32_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	block->i32[index][slot] = value;
	spin_unlock(&entity_spin);
	return (0);
}

int
entity_name(entity_id_t id, char *buf, u32 bufsize)
{
	int	ret;

	spin_lock(&entity_spin);
	ret = entity_fill_name(id, buf, bufsize);
	spin_unlock(&entity_spin);
	return (ret);
}

int
entity_access(const struct process *proc, entity_id_t id, u32 want)
{
	entity_meta_block_t	*block;
	u32			slot;

	(void)want;
	if (!proc) {
		return (0);
	}
	spin_lock(&entity_spin);
	if (entity_slot(id, &block, &slot) < 0) {
		spin_unlock(&entity_spin);
		return (-API_ERR_BAD_HANDLE);
	}
	if (proc->kusr_auth || proc->euid == 0) {
		spin_unlock(&entity_spin);
		return (0);
	}
	if (proc->euid == block->euid[slot]) {
		spin_unlock(&entity_spin);
		return (0);
	}
	if (proc->egid == block->egid[slot]) {
		spin_unlock(&entity_spin);
		return (0);
	}
	spin_unlock(&entity_spin);
	return (-API_ERR_ACCESS);
}

int
entity_foreach(u16 arch, u32 start, int (*cb)(entity_id_t id, void *ctx),
    void *ctx)
{
	u32	index, slot, a;
	int	ret;

	if (!cb) {
		return (-API_ERR_BAD_VALUE);
	}
	spin_lock(&entity_spin);
	ret = 0;
	if (arch == 0 || arch > ENTITY_ARCH_MAX ||
	    entity_arch_blocks[arch].meta == NULL) {
		for (index = start; index < ENTITY_MAX_ENTITIES; index++) {
			entity_meta_block_t	*block;
			entity_id_t		id;

			block = &entity_blocks[index >>
			    ENTITY_BLOCK_SHIFT];
			slot = index & (ENTITY_BLOCK_ENTRIES - 1);
			if (!block->used[slot]) {
				continue;
			}
			if (arch != 0 && block->arch[slot] != arch) {
				continue;
			}
			id = entity_id_make(block->arch[slot],
			    block->gen[slot], index);
			ret = cb(id, ctx);
			if (ret != 0) {
				break;
			}
		}
	}
	if (ret != 0) {
		spin_unlock(&entity_spin);
		return (ret);
	}
	if (arch == 0) {
		for (a = 1; a <= ENTITY_ARCH_MAX; a++) {
			entity_meta_block_t	*block;

			block = entity_arch_blocks[a].meta;
			if (block == NULL) {
				continue;
			}
			for (slot = 0; slot < entity_arch_blocks[a].count;
			    slot++) {
				entity_id_t	id;

				if (!block->used[slot]) {
					continue;
				}
				id = entity_id_make((u16)a,
				    block->gen[slot], slot);
				ret = cb(id, ctx);
				if (ret != 0) {
					break;
				}
			}
			if (ret != 0) {
				break;
			}
		}
	} else if (entity_arch_blocks[arch].meta != NULL) {
		entity_meta_block_t	*block;

		block = entity_arch_blocks[arch].meta;
		for (slot = start; slot < entity_arch_blocks[arch].count;
		    slot++) {
			entity_id_t	id;

			if (!block->used[slot]) {
				continue;
			}
			id = entity_id_make(arch, block->gen[slot], slot);
			ret = cb(id, ctx);
			if (ret != 0) {
				break;
			}
		}
	}
	spin_unlock(&entity_spin);
	return (ret);
}

void
entity_dump(void)
{
	u32	index, slot, a;

	spin_lock(&entity_spin);
	printk("Entity dump (%u used / %d slots, %llu refcount "
	    "saturations):\n", entity_count, ENTITY_MAX_ENTITIES,
	    (unsigned long long)entity_refs_saturated);
	for (index = 0; index < ENTITY_MAX_ENTITIES; index++) {
		entity_meta_block_t	*block;
		char			name[ENTITY_NAME_MAX];
		const char		*arch_name;

		block = &entity_blocks[index >> ENTITY_BLOCK_SHIFT];
		slot = index & (ENTITY_BLOCK_ENTRIES - 1);
		if (!block->used[slot]) {
			continue;
		}
		name[0] = '\0';
		entity_fill_name(entity_id_make(block->arch[slot],
		    block->gen[slot], index), name, sizeof(name));
		arch_name = entity_arch_names[block->arch[slot]];
		if (!arch_name) {
			arch_name = "?";
		}
		printk("  [%u] arch=%u(%s) gen=%u state=%u refs=%d "
		    "owner=%u size=%llu name=%s\n", index,
		    (u32)block->arch[slot], arch_name,
		    (u32)block->gen[slot], block->state[slot],
		    block->refs[slot], block->owner[slot],
		    (unsigned long long)block->size[slot], name);
	}
	for (a = 1; a <= ENTITY_ARCH_MAX; a++) {
		entity_meta_block_t	*block;

		block = entity_arch_blocks[a].meta;
		if (block == NULL) {
			continue;
		}
		for (slot = 0; slot < entity_arch_blocks[a].count; slot++) {
			char		name[ENTITY_NAME_MAX];
			const char	*arch_name;

			if (!block->used[slot]) {
				continue;
			}
			name[0] = '\0';
			entity_fill_name(entity_id_make(a, block->gen[slot],
			    slot), name, sizeof(name));
			arch_name = entity_arch_names[a];
			if (!arch_name) {
				arch_name = "?";
			}
			printk("  [%u] arch=%u(%s) gen=%u state=%u refs=%d "
			    "owner=%u size=%llu name=%s\n", slot,
			    (u32)a, arch_name, (u32)block->gen[slot],
			    block->state[slot], block->refs[slot],
			    block->owner[slot],
			    (unsigned long long)block->size[slot], name);
		}
	}
	spin_unlock(&entity_spin);
}

static void
entity_core_identify(driver_t *driver, device_t parent)
{
	(void)driver;
	if (device_find_child(parent, "entity_core", 0) == NULL) {
		device_add_child(parent, "entity_core", 0);
	}
}

static int
entity_core_attach(device_t dev)
{
	(void)dev;
	entity_init();
	return (entity_is_initialized() ? 0 : -1);
}

static devclass_t entity_core_devclass = {
	.name		= "entity",
	.maxunit	= 1,
};

static driver_t entity_core_driver = {
	.name		= "entity_core",
	.identify	= entity_core_identify,
	.probe		= NULL,
	.attach		= entity_core_attach,
};

PSEUDO_DRIVER_MODULE(entity_core, entity_core_driver,
    entity_core_devclass, NEWBUS_PASS_CORE, NEWBUS_ORDER_FIRST);
