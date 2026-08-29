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
$define %type process as struct with process control block

$define %func entity_handle_unlink as procedure with args process *, u32
$define %func entity_handle_link as procedure with args process *, u32
$define %func entity_handle_slot as function with args process *, int, u64 *, u32 *
$define %func entity_handle_init_process as procedure with args process *
$define %func entity_handle_alloc as function with args process *, entity id, u32
$define %func entity_handle_lookup as function with args process *, int, u64 *, u32 *
$define %func entity_handle_drop as function with args process *, int, int
$define %func entity_handle_foreach as function with args process *, callback
$define %func entity_handle_free as function with args process *, int
$define %func entity_handle_dup as function with args process *, int, u32
$define %func entity_handle_copy_all as function with args process *, process *
$define %func entity_handle_release_all as procedure with args process *
$define %func entity_handle_drop_locked as function with args process *, int, entity id *
$define %func entity_handle_snapshot as function with args process *, snap *, u32, u32 *
$define %func entity_handle_snap_take as function with args process *, u32 *
$define %type entity_handle_snap_t as struct with id, handle, access

*/

/* !SPACE!

$space %internal entity_handle_unlink, entity_handle_link
$space %internal entity_handle_slot, entity_handle_drop_locked
$space %internal entity_handle_snapshot, entity_handle_snap_take
$space %internal entity_handle_spin
$space %export entity_handle_init_process, entity_handle_alloc
$space %export entity_handle_lookup, entity_handle_free, entity_handle_dup
$space %export entity_handle_copy_all, entity_handle_release_all
$space %export entity_handle_drop
$space %export entity_handle_foreach

*/

#include <kernel/api/errno.h>
#include <kernel/entity/entity.h>
#include <kernel/process.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <mm/kmem.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	ENTITY_HANDLE_NONE	0xFFFFFFFFU
#define	ENTITY_HANDLE_MASK	0xFFFF

static u32	entity_handle_used[ENTITY_MAX_HANDLES];
static u32	entity_handle_pid[ENTITY_MAX_HANDLES];
static u64	entity_handle_id[ENTITY_MAX_HANDLES];
static u32	entity_handle_flags[ENTITY_MAX_HANDLES];
static u32	entity_handle_gen[ENTITY_MAX_HANDLES];
static u32	entity_handle_next[ENTITY_MAX_HANDLES];
static u32	entity_handle_prev[ENTITY_MAX_HANDLES];
static u32	entity_handle_free_head;
static u32	entity_handle_count;

static spin_t	entity_handle_spin =
    SPIN_INITIALIZER("entity_handle", LO_HANDLE);

typedef struct entity_handle_snap {
	entity_id_t	id;
	int		handle;
	u32		access;
} entity_handle_snap_t;

void
entity_handle_init(void)
{
	u32	i;

	memset(entity_handle_used, 0, sizeof(entity_handle_used));
	memset(entity_handle_pid, 0, sizeof(entity_handle_pid));
	memset(entity_handle_id, 0, sizeof(entity_handle_id));
	memset(entity_handle_flags, 0, sizeof(entity_handle_flags));
	memset(entity_handle_gen, 0, sizeof(entity_handle_gen));
	memset(entity_handle_prev, 0, sizeof(entity_handle_prev));
	entity_handle_free_head = 0;
	for (i = 0; i < ENTITY_MAX_HANDLES - 1; i++) {
		entity_handle_next[i] = i + 1;
	}
	entity_handle_next[ENTITY_MAX_HANDLES - 1] = ENTITY_HANDLE_NONE;
	entity_handle_count = 0;
}

static void
entity_handle_unlink(process_t *proc, u32 slot)
{
	u32	prev;
	u32	next;

	prev = entity_handle_prev[slot];
	next = entity_handle_next[slot];
	if (prev != ENTITY_HANDLE_NONE) {
		entity_handle_next[prev] = next;
	} else if (proc != NULL &&
	    (u32)proc->entity_handle_head == slot) {
		proc->entity_handle_head = (int)next;
	}
	if (next != ENTITY_HANDLE_NONE) {
		entity_handle_prev[next] = prev;
	}
	entity_handle_prev[slot] = ENTITY_HANDLE_NONE;
	entity_handle_next[slot] = ENTITY_HANDLE_NONE;
}

static void
entity_handle_link(process_t *proc, u32 slot)
{
	entity_handle_prev[slot] = ENTITY_HANDLE_NONE;
	entity_handle_next[slot] = (u32)proc->entity_handle_head;
	if (proc->entity_handle_head != -1) {
		entity_handle_prev[(u32)proc->entity_handle_head] = slot;
	}
	proc->entity_handle_head = (int)slot;
	proc->entity_handle_count++;
}

static int
entity_handle_slot(const struct process *proc, int handle, u64 *id,
    u32 *access)
{
	u32	slot;
	u32	gen;

	if (!proc || handle < 0) {
		if (handle < 0) {
			return (-API_ERR_BAD_HANDLE);
		}
	}
	slot = ((u32)handle) & ENTITY_HANDLE_MASK;
	gen = ((u32)handle) >> 16;
	if (slot >= ENTITY_MAX_HANDLES) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (!entity_handle_used[slot] ||
	    (proc ? entity_handle_pid[slot] != proc->pid :
	    entity_handle_pid[slot] != 0) ||
	    entity_handle_gen[slot] != gen) {
		return (-API_ERR_BAD_HANDLE);
	}
	if (id) {
		*id = entity_handle_id[slot];
	}
	if (access) {
		*access = entity_handle_flags[slot];
	}
	return (0);
}

void
entity_handle_init_process(struct process *proc)
{
	if (!proc) {
		return;
	}
	proc->entity_handle_count = 0;
	proc->entity_handle_head = -1;
}

int
entity_handle_alloc(struct process *proc, entity_id_t id, u32 access)
{
	u32	slot;
	u32	gen;
	int	ret;

	if (id == 0 || access == 0) {
		return (-API_ERR_BAD_VALUE);
	}
	ret = entity_retain_checked(id);
	if (ret != 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	spin_lock(&entity_handle_spin);
	if (entity_handle_free_head == ENTITY_HANDLE_NONE) {
		spin_unlock(&entity_handle_spin);
		entity_release(id);
		return (-API_ERR_HANDLES_FULL);
	}
	slot = entity_handle_free_head;
	entity_handle_free_head = entity_handle_next[slot];
	gen = entity_handle_gen[slot] + 1;
	if (gen == 0) {
		gen = 1;
	}
	entity_handle_used[slot] = 1;
	entity_handle_pid[slot] = proc ? proc->pid : 0;
	entity_handle_id[slot] = id;
	entity_handle_flags[slot] = access;
	entity_handle_gen[slot] = gen;
	entity_handle_count++;
	if (proc) {
		entity_handle_link(proc, slot);
	}
	spin_unlock(&entity_handle_spin);
	return ((int)((gen << 16) | slot));
}

int
entity_handle_lookup(const struct process *proc, int handle, entity_id_t *id,
    u32 *access)
{
	int	ret;

	spin_lock(&entity_handle_spin);
	ret = entity_handle_slot(proc, handle, id, access);
	spin_unlock(&entity_handle_spin);
	return (ret);
}

static int
entity_handle_drop_locked(struct process *proc, int handle, entity_id_t *out_id)
{
	u32		slot;
	u32		gen;

	if (handle < 0) {
		return (-API_ERR_BAD_HANDLE);
	}
	slot = ((u32)handle) & ENTITY_HANDLE_MASK;
	gen = ((u32)handle) >> 16;
	if (slot >= ENTITY_MAX_HANDLES ||
	    !entity_handle_used[slot] ||
	    (proc ? entity_handle_pid[slot] != proc->pid :
	    entity_handle_pid[slot] != 0) ||
	    entity_handle_gen[slot] != gen) {
		return (-API_ERR_BAD_HANDLE);
	}
	*out_id = entity_handle_id[slot];
	if (proc) {
		entity_handle_unlink(proc, slot);
	}
	entity_handle_used[slot] = 0;
	entity_handle_pid[slot] = 0;
	entity_handle_id[slot] = 0;
	entity_handle_flags[slot] = 0;
	entity_handle_next[slot] = entity_handle_free_head;
	entity_handle_free_head = slot;
	entity_handle_count--;
	if (proc) {
		proc->entity_handle_count--;
		if (proc->entity_handle_count < 0) {
			proc->entity_handle_count = 0;
		}
	}
	return (0);
}

int
entity_handle_drop(struct process *proc, int handle, int release)
{
	entity_id_t	id;
	int		ret;

	id = 0;
	spin_lock(&entity_handle_spin);
	ret = entity_handle_drop_locked(proc, handle, &id);
	spin_unlock(&entity_handle_spin);
	if (ret != 0) {
		return (ret);
	}
	if (release) {
		entity_release(id);
	}
	return (0);
}

int
entity_handle_free(struct process *proc, int handle)
{
	return (entity_handle_drop(proc, handle, 1));
}

int
entity_handle_dup(struct process *proc, int handle, u32 access)
{
	entity_id_t	id;
	u32		old_access;
	int		ret;

	ret = entity_handle_lookup(proc, handle, &id, &old_access);
	if (ret != 0) {
		return (ret);
	}
	if (access == 0) {
		access = old_access;
	}
	return (entity_handle_alloc(proc, id, access));
}

static u32
entity_handle_snapshot(const struct process *proc,
    entity_handle_snap_t *out, u32 cap, u32 *total)
{
	u32	slot;
	u32	n;

	n = 0;
	*total = 0;
	slot = (u32)proc->entity_handle_head;
	while (slot != ENTITY_HANDLE_NONE && slot < ENTITY_MAX_HANDLES) {
		(*total)++;
		if (n < cap) {
			out[n].handle = (int)((entity_handle_gen[slot] << 16) |
			    slot);
			out[n].id = entity_handle_id[slot];
			out[n].access = entity_handle_flags[slot];
			n++;
		}
		slot = entity_handle_next[slot];
	}
	return (n);
}

static entity_handle_snap_t *
entity_handle_snap_take(const struct process *proc, u32 *out_n)
{
	entity_handle_snap_t	*snap;
	u32			cap;
	u32			n;
	u32			total;
	int			tries;

	cap = 16;
	for (tries = 0; tries < 8; tries++) {
		snap = (entity_handle_snap_t *)kmem_alloc(
		    sizeof(*snap) * cap);
		if (snap == NULL) {
			return (NULL);
		}
		spin_lock(&entity_handle_spin);
		n = entity_handle_snapshot(proc, snap, cap, &total);
		spin_unlock(&entity_handle_spin);
		if (total <= cap) {
			*out_n = n;
			return (snap);
		}
		kmem_free(snap);
		if (cap >= ENTITY_MAX_HANDLES) {
			return (NULL);
		}
		cap = total + 8;
		if (cap > ENTITY_MAX_HANDLES) {
			cap = ENTITY_MAX_HANDLES;
		}
	}
	return (NULL);
}

int
entity_handle_copy_all(struct process *dst, const struct process *src)
{
	entity_handle_snap_t	*snap;
	u32			n;
	u32			i;
	int			handle;

	if (!dst || !src) {
		return (-API_ERR_BAD_VALUE);
	}
	if (dst->entity_handle_head == 0 &&
	    dst->entity_handle_count == 0) {
		entity_handle_init_process(dst);
	}
	snap = entity_handle_snap_take(src, &n);
	if (snap == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	for (i = 0; i < n; i++) {
		handle = entity_handle_alloc(dst, snap[i].id,
		    snap[i].access);
		if (handle < 0) {
			kmem_free(snap);
			entity_handle_release_all(dst);
			return (handle);
		}
	}
	kmem_free(snap);
	return (0);
}

void
entity_handle_release_all(struct process *proc)
{
	entity_id_t	id;
	int		handle;
	int		ret;

	if (!proc) {
		return;
	}
	if (proc->entity_handle_head == 0 &&
	    proc->entity_handle_count == 0) {
		entity_handle_init_process(proc);
		return;
	}
	for (;;) {
		id = 0;
		spin_lock(&entity_handle_spin);
		handle = proc->entity_handle_head;
		if (handle < 0 || (u32)handle >= ENTITY_MAX_HANDLES) {
			spin_unlock(&entity_handle_spin);
			break;
		}
		handle = (int)((entity_handle_gen[(u32)handle] << 16) |
		    (u32)handle);
		ret = entity_handle_drop_locked(proc, handle, &id);
		spin_unlock(&entity_handle_spin);
		if (ret != 0) {
			break;
		}
		entity_release(id);
	}
}

int
entity_handle_foreach(const struct process *proc,
    int (*cb)(int handle, entity_id_t id, u32 access, void *ctx),
    void *ctx)
{
	entity_handle_snap_t	*snap;
	u32			n;
	u32			i;
	int			ret;

	if (!proc || !cb) {
		return (-API_ERR_BAD_VALUE);
	}
	snap = entity_handle_snap_take(proc, &n);
	if (snap == NULL) {
		return (-API_ERR_NO_MEMORY);
	}
	ret = 0;
	for (i = 0; i < n; i++) {
		ret = cb(snap[i].handle, snap[i].id, snap[i].access, ctx);
		if (ret != 0) {
			break;
		}
	}
	kmem_free(snap);
	return (ret);
}
