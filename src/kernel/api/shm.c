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

$define %type shm_segment_t as shared memory segment metadata
$define %type process_t as struct with mmap state
$define %type vma_t as struct with start, end, next

$define %func align_up as function with args u64, u64
$define %func shm_free_if_dead as procedure with args shm_segment_t *
$define %func shm_range_busy as function with args process_t *, u64, u64
$define %func shm_get as function with args int
$define %func shm_put as procedure with args shm_segment_t *
$define %func shm_get_or_create as function with args u64, u64, int, int *
$define %func shm_remove as function with args int
$define %func shm_attach as function with args int
$define %func shm_detach as procedure with args int
$define %func shm_map as function with args shm_segment_t *, u64, u64, u32, u32
$define %func shm_info as function with args int, struct api_shminfo_args *
$define %func api_shm_get as function with args struct api_shmget_args *
$define %func api_shm_map as function with args struct api_shmmap_args *
$define %func api_shm_ctl as function with args int, int, void *

*/

/* !SPACE!

$space %internal align_up, shm_free_if_dead, shm_range_busy
$space %export shm_get, shm_put, shm_get_or_create, shm_remove
$space %export shm_attach, shm_detach
$space %export shm_map, shm_info
$space %export api_shm_get, api_shm_map, api_shm_ctl

*/

#include <kernel/api/api.h>
#include <kernel/api/shm.h>
#include <kernel/process.h>
#include <kernel/useraddr.h>
#include <mlibc/mlibc.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>

#define PAGE_SIZE	4096

static shm_segment_t shm_segments[SHM_MAX_SEGMENTS];
static int shm_next_id = 1;

static u64
align_up(u64 val, u64 align)
{
	return ((val + align - 1) & ~(align - 1));
}

static void
shm_free_if_dead(shm_segment_t *seg)
{
	if (seg == NULL || !seg->used || !seg->removed || seg->refs != 0 ||
	    seg->attaches != 0) {
		return;
	}

	if (seg->object != NULL) {
		vm_object_unref(seg->object);
	}
	memset(seg, 0, sizeof(*seg));
}

static int
shm_range_busy(process_t *proc, u64 start, u64 end)
{
	vma_t	*v;

	for (v = proc->vma_list; v != NULL; v = v->next) {
		if (start < v->end && end > v->start) {
			return (1);
		}
	}
	return (0);
}

shm_segment_t *
shm_get(int id)
{
	int	i;

	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (shm_segments[i].used && shm_segments[i].id == id &&
		    !shm_segments[i].removed) {
			shm_segments[i].refs++;
			return (&shm_segments[i]);
		}
	}
	return (NULL);
}

void
shm_put(shm_segment_t *seg)
{
	if (seg == NULL || seg->refs == 0) {
		return;
	}
	seg->refs--;
	shm_free_if_dead(seg);
}

shm_segment_t *
shm_get_or_create(u64 key, u64 size, int flags, int *error)
{
	shm_segment_t	*free_seg;
	u64		aligned;
	int		i;

	if (error != NULL) {
		*error = 0;
	}
	if (key != SHM_PRIVATE) {
		for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
			if (shm_segments[i].used &&
			    !shm_segments[i].removed &&
			    shm_segments[i].key == key) {
				if ((flags & SHM_CREAT) &&
				    (flags & SHM_EXCL)) {
					if (error != NULL)
						*error = -API_ERR_EXISTS;
					return (NULL);
				}
				if (size > shm_segments[i].size) {
					if (error != NULL)
						*error = -API_ERR_BAD_VALUE;
					return (NULL);
				}
				shm_segments[i].refs++;
				return (&shm_segments[i]);
			}
		}
		if (!(flags & SHM_CREAT)) {
			if (error != NULL)
				*error = -API_ERR_NOT_FOUND;
			return (NULL);
		}
	}

	if (size == 0) {
		if (error != NULL)
			*error = -API_ERR_BAD_VALUE;
		return (NULL);
	}

	free_seg = NULL;
	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (!shm_segments[i].used) {
			free_seg = &shm_segments[i];
			break;
		}
	}
	if (free_seg == NULL) {
		if (error != NULL)
			*error = -API_ERR_NO_MEMORY;
		return (NULL);
	}

	aligned = align_up(size, PAGE_SIZE);
	memset(free_seg, 0, sizeof(*free_seg));
	free_seg->object = vm_object_create(VM_OBJ_SHM, aligned, NULL);
	if (free_seg->object == NULL) {
		if (error != NULL)
			*error = -API_ERR_NO_MEMORY;
		return (NULL);
	}

	free_seg->used = 1;
	free_seg->id = shm_next_id++;
	free_seg->key = key;
	free_seg->size = aligned;
	free_seg->mode = (u32)(flags & 0777);
	free_seg->refs = 1;
	if (shm_next_id <= 0) {
		shm_next_id = 1;
	}

	return (free_seg);
}

int
shm_remove(int id)
{
	shm_segment_t	*seg;
	int		i;

	seg = NULL;
	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (shm_segments[i].used && shm_segments[i].id == id) {
			seg = &shm_segments[i];
			break;
		}
	}
	if (seg == NULL || seg->removed) {
		return (-API_ERR_NOT_FOUND);
	}

	seg->removed = 1;
	shm_free_if_dead(seg);
	return (0);
}

int
shm_attach(int id)
{
	shm_segment_t	*seg;
	int		i;

	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (shm_segments[i].used && shm_segments[i].id == id) {
			seg = &shm_segments[i];
			seg->attaches++;
			return (0);
		}
	}
	return (-API_ERR_NOT_FOUND);
}

void
shm_detach(int id)
{
	shm_segment_t	*seg;
	int		i;

	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (shm_segments[i].used && shm_segments[i].id == id) {
			seg = &shm_segments[i];
			if (seg->attaches > 0) {
				seg->attaches--;
			}
			shm_free_if_dead(seg);
			return;
		}
	}
}

u64
shm_map(shm_segment_t *seg, u64 addr, u64 size, u32 prot, u32 flags)
{
	process_t	*proc;
	u64		aligned;

	proc = process_current();
	if (proc == NULL || seg == NULL || seg->object == NULL) {
		return ((u64)-API_ERR_BAD_VALUE);
	}

	if (size == 0 || size > seg->size) {
		size = seg->size;
	}
	aligned = align_up(size, PAGE_SIZE);

	if (flags & API_MAP_FIXED) {
		if (addr == 0 || (addr & (PAGE_SIZE - 1)) != 0) {
			return ((u64)-API_ERR_BAD_VALUE);
		}
		if (shm_range_busy(proc, addr, addr + aligned)) {
			return ((u64)-API_ERR_BAD_VALUE);
		}
	} else {
		addr = vm_map_find_free(proc, aligned);
		if (addr == 0) {
			return ((u64)-API_ERR_NO_MEMORY);
		}
	}

	flags |= API_MAP_SHARED;
	if (vm_map_insert(proc, addr, addr + aligned, prot, flags,
	    (u32)seg->id, seg->object, 0) != 0) {
		return ((u64)-API_ERR_NO_MEMORY);
	}
	seg->attaches++;

	return (addr);
}

int
shm_info(int id, struct api_shminfo_args *info)
{
	shm_segment_t	*seg;
	int		i;

	if (info == NULL) {
		return (-API_ERR_BAD_VALUE);
	}

	seg = NULL;
	for (i = 0; i < SHM_MAX_SEGMENTS; i++) {
		if (shm_segments[i].used && shm_segments[i].id == id) {
			seg = &shm_segments[i];
			break;
		}
	}
	if (seg == NULL) {
		return (-API_ERR_NOT_FOUND);
	}

	info->id = (u32)seg->id;
	info->mode = seg->mode;
	info->refs = seg->attaches;
	info->removed = (u32)seg->removed;
	info->key = seg->key;
	info->size = seg->size;
	return (0);
}

int
api_shm_get(struct api_shmget_args *uargs)
{
	struct api_shmget_args	args;
	shm_segment_t		*seg;
	int			error;

	if (!is_user_address(uargs, sizeof(args))) {
		return (-API_ERR_BAD_ADDR);
	}

	memcpy(&args, uargs, sizeof(args));
	seg = shm_get_or_create(args.key, args.size, (int)args.flags,
	    &error);
	if (seg == NULL) {
		return (error);
	}

	args.id = (u32)seg->id;
	memcpy(uargs, &args, sizeof(args));
	shm_put(seg);
	return (0);
}

u64
api_shm_map(struct api_shmmap_args *uargs)
{
	struct api_shmmap_args	args;
	shm_segment_t		*seg;
	u64			addr;

	if (!is_user_address(uargs, sizeof(args))) {
		return ((u64)-API_ERR_BAD_ADDR);
	}

	memcpy(&args, uargs, sizeof(args));
	seg = shm_get((int)args.id);
	if (seg == NULL) {
		return ((u64)-API_ERR_NOT_FOUND);
	}

	addr = shm_map(seg, args.addr, args.size, args.prot, args.flags);
	shm_put(seg);
	return (addr);
}

int
api_shm_ctl(int id, int cmd, void *uarg)
{
	struct api_shminfo_args	info;
	int			ret;

	if (cmd == SHM_CTL_RMID) {
		return (shm_remove(id));
	}
	if (cmd != SHM_CTL_STAT) {
		return (-API_ERR_BAD_VALUE);
	}
	if (!is_user_address(uarg, sizeof(info))) {
		return (-API_ERR_BAD_ADDR);
	}

	ret = shm_info(id, &info);
	if (ret != 0) {
		return (ret);
	}
	memcpy(uarg, &info, sizeof(info));
	return (0);
}
