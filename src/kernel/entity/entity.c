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
$define %type int as 32 bit signed
$define %type entity_id as 64 bit packed archetype/generation/index
$define %type process as struct with process control block

$define %func entity_slot as function with args entity id
$define %func entity_free_push as procedure with args u32
$define %func entity_free_pop as function with args void
$define %func entity_fill_name as function with args entity id, char *, u32
$define %func entity_init as procedure with args void
$define %func entity_arch_release_register as function with args archetype, callback
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

*/

/* !SPACE!

$space %internal entity_slot, entity_free_push, entity_free_pop
$space %internal entity_fill_name
$space %export entity_init, entity_is_initialized
$space %export entity_arch_release_register
$space %export entity_create, entity_destroy, entity_retain, entity_release
$space %export entity_valid, entity_arch, entity_state, entity_refs
$space %export entity_flags, entity_owner, entity_size, entity_set_size
$space %export entity_uid, entity_gid, entity_euid, entity_egid
$space %export entity_created
$space %export entity_get_data, entity_set_data
$space %export entity_get_i32, entity_set_i32
$space %export entity_name, entity_access, entity_foreach, entity_dump
$space %export entity_event_set_notify

*/

#include <kernel/api/errno.h>
#include <kernel/cm/cm.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/newbus/newbus.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/trace/trace.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	ENTITY_SLOT_NONE	0xFFFFFFFFU
#define	ENTITY_REF_MAX		0x7FFFFFF0

static int		entity_initialized;
static u32		entity_used[ENTITY_MAX_ENTITIES];
static u16		entity_gen[ENTITY_MAX_ENTITIES];
static u16		entity_arch_col[ENTITY_MAX_ENTITIES];
static u32		entity_state_col[ENTITY_MAX_ENTITIES];
static u32		entity_flags_col[ENTITY_MAX_ENTITIES];
static s32		entity_refs_col[ENTITY_MAX_ENTITIES];
static u32		entity_owner_col[ENTITY_MAX_ENTITIES];
static u32		entity_uid_col[ENTITY_MAX_ENTITIES];
static u32		entity_gid_col[ENTITY_MAX_ENTITIES];
static u32		entity_euid_col[ENTITY_MAX_ENTITIES];
static u32		entity_egid_col[ENTITY_MAX_ENTITIES];
static u32		entity_kusr[ENTITY_MAX_ENTITIES];
static u64		entity_size_col[ENTITY_MAX_ENTITIES];
static u64		entity_born_col[ENTITY_MAX_ENTITIES];
static u64		entity_data[ENTITY_DATA_COUNT][ENTITY_MAX_ENTITIES];
static s32		entity_i32[ENTITY_I32_COUNT][ENTITY_MAX_ENTITIES];
static u32		entity_name_off_col[ENTITY_MAX_ENTITIES];
static u32		entity_free_next[ENTITY_MAX_ENTITIES];
static u32		entity_free_head;
static u32		entity_count;
static const char	*entity_arch_names[ENTITY_MAX_ARCHETYPES];
static entity_release_fn	entity_arch_release[ENTITY_MAX_ARCHETYPES];
static void	(*entity_event_notify)(entity_id_t id, u32 fflags);

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
entity_slot(entity_id_t id)
{
	u32	index;

	index = entity_id_index(id);
	if (index >= ENTITY_MAX_ENTITIES) {
		return (-1);
	}
	if (!entity_used[index]) {
		return (-1);
	}
	if (entity_gen[index] != entity_id_generation(id)) {
		return (-1);
	}
	if (entity_arch_col[index] != entity_id_archetype(id)) {
		return (-1);
	}
	return ((int)index);
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

static int
entity_fill_name(entity_id_t id, char *buf, u32 bufsize)
{
	const char	*name;
	u32		len;

	if (!buf || bufsize == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	name = entity_ns_name_of(id);
	if (!name) {
		buf[0] = '\0';
		return (0);
	}
	len = (u32)strlen(name);
	if (len >= bufsize) {
		len = bufsize - 1;
	}
	memcpy(buf, name, len);
	buf[len] = '\0';
	return (0);
}

void
entity_init(void)
{
	u32	i;

	if (entity_initialized) {
		return;
	}
	memset(entity_used, 0, sizeof(entity_used));
	memset(entity_gen, 0, sizeof(entity_gen));
	memset(entity_arch_col, 0, sizeof(entity_arch_col));
	memset(entity_state_col, 0, sizeof(entity_state_col));
	memset(entity_flags_col, 0, sizeof(entity_flags_col));
	memset(entity_refs_col, 0, sizeof(entity_refs_col));
	memset(entity_owner_col, 0, sizeof(entity_owner_col));
	memset(entity_uid_col, 0, sizeof(entity_uid_col));
	memset(entity_gid_col, 0, sizeof(entity_gid_col));
	memset(entity_euid_col, 0, sizeof(entity_euid_col));
	memset(entity_egid_col, 0, sizeof(entity_egid_col));
	memset(entity_kusr, 0, sizeof(entity_kusr));
	memset(entity_size_col, 0, sizeof(entity_size_col));
	memset(entity_born_col, 0, sizeof(entity_born_col));
	memset(entity_data, 0, sizeof(entity_data));
	memset(entity_i32, 0, sizeof(entity_i32));
	memset(entity_name_off_col, 0, sizeof(entity_name_off_col));
	memset(entity_arch_names, 0, sizeof(entity_arch_names));
	memset(entity_arch_release, 0, sizeof(entity_arch_release));

	entity_free_head = 0;
	for (i = 0; i < ENTITY_MAX_ENTITIES; i++) {
		entity_free_next[i] = i + 1;
	}
	entity_free_next[ENTITY_MAX_ENTITIES - 1] = ENTITY_SLOT_NONE;
	entity_count = 0;
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
	entity_initialized = 1;
	drivers_log("[ENTITY] initialized: %d slots, %d handles, "
	    "%d namespace nodes\n", ENTITY_MAX_ENTITIES,
	    ENTITY_MAX_HANDLES, ENTITY_MAX_NS_NODES);
	if (cm_get_bool_default("SYSTEM", "Entity", "Debug", 0)) {
		printk("[ENTITY] debug mode enabled\n");
	}
}

int
entity_arch_release_register(u16 arch, entity_release_fn fn)
{
	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	entity_arch_release[arch] = fn;
	smp_unlock();
	return (0);
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
	u32		index;
	u16		gen;
	u64		created;

	if (arch == 0 || arch > ENTITY_ARCH_MAX) {
		return (0);
	}
	created = timer_is_initialized() ? timer_get_ticks() : 0;
	smp_lock();
	if (!entity_initialized) {
		smp_unlock();
		return (0);
	}
	index = entity_free_pop();
	if (index == ENTITY_SLOT_NONE) {
		smp_unlock();
		return (0);
	}
	gen = entity_gen[index] + 1;
	if (gen == 0) {
		gen = 1;
	}
	entity_gen[index] = gen;
	entity_arch_col[index] = arch;
	entity_used[index] = 1;
	entity_state_col[index] = ENTITY_STATE_ACTIVE;
	entity_flags_col[index] = flags;
	entity_refs_col[index] = 1;
	entity_owner_col[index] = owner_pid;
	entity_uid_col[index] = uid;
	entity_gid_col[index] = gid;
	entity_euid_col[index] = euid;
	entity_egid_col[index] = egid;
	entity_kusr[index] = kusr ? 1 : 0;
	entity_size_col[index] = 0;
	entity_born_col[index] = created;
	entity_name_off_col[index] = 0;
	entity_count++;
	id = entity_id_make(arch, gen, index);
	smp_unlock();
	entity_trace_notify(id, ENTITY_EVENT_CREATE);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_CREATE);
	}
	return (id);
}

void
entity_event_set_notify(void (*fn)(entity_id_t id, u32 fflags))
{
	smp_lock();
	entity_event_notify = fn;
	smp_unlock();
}

int
entity_destroy(entity_id_t id)
{
	int	index;

	smp_lock();
	index = entity_slot(id);
	if (index < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	if (entity_state_col[index] == ENTITY_STATE_ACTIVE) {
		entity_state_col[index] = ENTITY_STATE_DELETED;
		entity_ns_unbind_id(id);
	}
	smp_unlock();
	entity_trace_notify(id, ENTITY_EVENT_STATE | ENTITY_EVENT_DESTROY);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_STATE |
		    ENTITY_EVENT_DESTROY);
	}
	entity_release(id);
	return (0);
}

void
entity_retain(entity_id_t id)
{
	int	index;

	smp_lock();
	index = entity_slot(id);
	if (index >= 0 && entity_refs_col[index] < ENTITY_REF_MAX) {
		entity_refs_col[index]++;
	}
	smp_unlock();
	entity_trace_notify(id, ENTITY_EVENT_RETAIN);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_RETAIN);
	}
}

void
entity_release(entity_id_t id)
{
	int	index;

	smp_lock();
	index = entity_slot(id);
	if (index < 0) {
		smp_unlock();
		return;
	}
	if (entity_refs_col[index] > 0) {
		entity_refs_col[index]--;
	}
	if (entity_refs_col[index] != 0) {
		smp_unlock();
		entity_trace_notify(id, ENTITY_EVENT_RELEASE);
		if (entity_event_notify) {
			entity_event_notify(id, ENTITY_EVENT_RELEASE);
		}
		return;
	}
	if (entity_arch_release[entity_arch_col[index]] != NULL) {
		entity_arch_release[entity_arch_col[index]](id);
	}
	entity_ns_unbind_id(id);
	entity_used[index] = 0;
	entity_count--;
	entity_free_push((u32)index);
	smp_unlock();
	entity_trace_notify(id, ENTITY_EVENT_RELEASE | ENTITY_EVENT_DESTROY);
	if (entity_event_notify) {
		entity_event_notify(id, ENTITY_EVENT_RELEASE |
		    ENTITY_EVENT_DESTROY);
	}
}

int
entity_valid(entity_id_t id)
{
	int	valid;

	smp_lock();
	valid = entity_slot(id) >= 0 ? 1 : 0;
	smp_unlock();
	return (valid);
}

u16
entity_arch(entity_id_t id)
{
	u16	arch;
	int	index;

	smp_lock();
	index = entity_slot(id);
	arch = index >= 0 ? entity_arch_col[index] : 0;
	smp_unlock();
	return (arch);
}

u32
entity_state(entity_id_t id)
{
	u32	state;
	int	index;

	smp_lock();
	index = entity_slot(id);
	state = index >= 0 ? entity_state_col[index] : 0;
	smp_unlock();
	return (state);
}

s32
entity_refs(entity_id_t id)
{
	s32	refs;
	int	index;

	smp_lock();
	index = entity_slot(id);
	refs = index >= 0 ? entity_refs_col[index] : 0;
	smp_unlock();
	return (refs);
}

u32
entity_flags(entity_id_t id)
{
	u32	flags;
	int	index;

	smp_lock();
	index = entity_slot(id);
	flags = index >= 0 ? entity_flags_col[index] : 0;
	smp_unlock();
	return (flags);
}

u32
entity_owner(entity_id_t id)
{
	u32	owner;
	int	index;

	smp_lock();
	index = entity_slot(id);
	owner = index >= 0 ? entity_owner_col[index] : 0;
	smp_unlock();
	return (owner);
}

u32
entity_uid(entity_id_t id)
{
	u32	value;
	int	index;

	smp_lock();
	index = entity_slot(id);
	value = index >= 0 ? entity_uid_col[index] : 0;
	smp_unlock();
	return (value);
}

u32
entity_gid(entity_id_t id)
{
	u32	value;
	int	index;

	smp_lock();
	index = entity_slot(id);
	value = index >= 0 ? entity_gid_col[index] : 0;
	smp_unlock();
	return (value);
}

u32
entity_euid(entity_id_t id)
{
	u32	value;
	int	index;

	smp_lock();
	index = entity_slot(id);
	value = index >= 0 ? entity_euid_col[index] : 0;
	smp_unlock();
	return (value);
}

u32
entity_egid(entity_id_t id)
{
	u32	value;
	int	index;

	smp_lock();
	index = entity_slot(id);
	value = index >= 0 ? entity_egid_col[index] : 0;
	smp_unlock();
	return (value);
}

u64
entity_size(entity_id_t id)
{
	u64	size;
	int	index;

	smp_lock();
	index = entity_slot(id);
	size = index >= 0 ? entity_size_col[index] : 0;
	smp_unlock();
	return (size);
}

u64
entity_created(entity_id_t id)
{
	u64	created;
	int	index;

	smp_lock();
	index = entity_slot(id);
	created = index >= 0 ? entity_born_col[index] : 0;
	smp_unlock();
	return (created);
}

int
entity_set_size(entity_id_t id, u64 size)
{
	int	index;

	smp_lock();
	index = entity_slot(id);
	if (index < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	entity_size_col[index] = size;
	smp_unlock();
	return (0);
}

int
entity_get_data(entity_id_t id, u32 index, u64 *value)
{
	int	slot;

	if (!value) {
		return (-API_ERR_BAD_VALUE);
	}
	if (index >= ENTITY_DATA_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	slot = entity_slot(id);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	*value = entity_data[index][slot];
	smp_unlock();
	return (0);
}

int
entity_set_data(entity_id_t id, u32 index, u64 value)
{
	int	slot;

	if (index >= ENTITY_DATA_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	slot = entity_slot(id);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	entity_data[index][slot] = value;
	smp_unlock();
	return (0);
}

int
entity_get_i32(entity_id_t id, u32 index, s32 *value)
{
	int	slot;

	if (!value) {
		return (-API_ERR_BAD_VALUE);
	}
	if (index >= ENTITY_I32_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	slot = entity_slot(id);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	*value = entity_i32[index][slot];
	smp_unlock();
	return (0);
}

int
entity_set_i32(entity_id_t id, u32 index, s32 value)
{
	int	slot;

	if (index >= ENTITY_I32_COUNT) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	slot = entity_slot(id);
	if (slot < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	entity_i32[index][slot] = value;
	smp_unlock();
	return (0);
}

int
entity_name(entity_id_t id, char *buf, u32 bufsize)
{
	int	ret;

	smp_lock();
	ret = entity_fill_name(id, buf, bufsize);
	smp_unlock();
	return (ret);
}

int
entity_access(const struct process *proc, entity_id_t id, u32 want)
{
	int	index;

	(void)want;
	if (!proc) {
		return (0);
	}
	if (!cm_get_bool_default("SYSTEM", "Entity",
	    "EnforceOwnership", 1)) {
		return (0);
	}
	smp_lock();
	index = entity_slot(id);
	if (index < 0) {
		smp_unlock();
		return (-API_ERR_BAD_HANDLE);
	}
	if (proc->kusr_auth || proc->euid == 0) {
		smp_unlock();
		return (0);
	}
	if (proc->euid == entity_euid_col[index]) {
		smp_unlock();
		return (0);
	}
	if (proc->egid == entity_egid_col[index]) {
		smp_unlock();
		return (0);
	}
	smp_unlock();
	return (-API_ERR_ACCESS);
}

int
entity_foreach(u16 arch, u32 start, int (*cb)(entity_id_t id, void *ctx),
    void *ctx)
{
	u32	index;
	int	ret;

	if (!cb) {
		return (-API_ERR_BAD_VALUE);
	}
	smp_lock();
	ret = 0;
	for (index = start; index < ENTITY_MAX_ENTITIES; index++) {
		entity_id_t	id;

		if (!entity_used[index]) {
			continue;
		}
		if (arch != 0 && entity_arch_col[index] != arch) {
			continue;
		}
		id = entity_id_make(entity_arch_col[index],
		    entity_gen[index], index);
		ret = cb(id, ctx);
		if (ret != 0) {
			break;
		}
	}
	smp_unlock();
	return (ret);
}

void
entity_dump(void)
{
	u32	index;

	smp_lock();
	printk("Entity dump (%u used / %d slots):\n", entity_count,
	    ENTITY_MAX_ENTITIES);
	for (index = 0; index < ENTITY_MAX_ENTITIES; index++) {
		char		name[ENTITY_NAME_MAX];
		const char	*arch_name;

		if (!entity_used[index]) {
			continue;
		}
		name[0] = '\0';
		entity_fill_name(entity_id_make(entity_arch_col[index],
		    entity_gen[index], index), name, sizeof(name));
		arch_name = entity_arch_names[entity_arch_col[index]];
		if (!arch_name) {
			arch_name = "?";
		}
		printk("  [%u] arch=%u(%s) gen=%u state=%u refs=%d "
		    "owner=%u size=%llu name=%s\n", index,
		    (u32)entity_arch_col[index], arch_name,
		    (u32)entity_gen[index], entity_state_col[index],
		    entity_refs_col[index], entity_owner_col[index],
		    (unsigned long long)entity_size_col[index], name);
	}
	smp_unlock();
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
