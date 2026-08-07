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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type vma_t as struct with start, end, prot, flags, gem_handle, object_base, object_offset, object, next
$define %type vm_object_t as struct with type, ref_count, size, pages, page_count, shadow, pager, next
$define %type process_t as struct with pid, ppid, state, name, cr3, vma_list

$define %func align_up as function with args u64, u64
$define %func page_flags_for_prot as function with args u32
$define %func vm_map_shm_attach_vma as procedure with args vma_t *
$define %func vm_map_shm_detach_vma as procedure with args vma_t *
$define %func vm_map_unlink as function with args process_t *, vma_t *
$define %func vm_map_insert_vma as procedure with args process_t *, vma_t *
$define %func vm_map_clip as function with args process_t *, u64
$define %func vm_map_find_free as function with args process_t *, u64
$define %func vm_map_range_free as function with args process_t *, u64, u64, vma_t *
$define %func vm_map_clip_range as function with args process_t *, u64, u64
$define %func vm_map_insert as function with args process_t *, u64, u64, u32, u32, u32, vm_object_t *, u64
$define %func vm_map_create_user_stack as function with args process_t *
$define %func vm_map_remove as function with args process_t *, u64
$define %func vm_map_remove_range as function with args process_t *, u64, u64
$define %func vm_map_relocate as function with args process_t *, vma_t *, u64, u64
$define %func vm_map_lookup as function with args process_t *, u64
$define %func vm_map_free_all as procedure with args process_t *
$define %func vm_map_fork as function with args process_t *, process_t *
$define %func vm_map_fault as function with args process_t *, u64, u64
$define %func vm_cow_fault as function with args u64, u64

*/

/* !SPACE!

$space %internal align_up, page_flags_for_prot
$space %internal vm_map_shm_attach_vma, vm_map_shm_detach_vma
$space %internal vm_map_unlink, vm_map_insert_vma, vm_map_clip
$space %export vm_map_find_free, vm_map_range_free, vm_map_clip_range
$space %export vm_map_insert, vm_map_create_user_stack
$space %export vm_map_remove, vm_map_remove_range
$space %export vm_map_relocate, vm_map_lookup, vm_map_free_all
$space %export vm_map_fork, vm_map_fault, vm_cow_fault

*/

#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_pager.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_page.h>
#include <kernel/api/shm.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>

#define PAGE_SIZE	4096

static u64
align_up(u64 val, u64 align)
{
	return ((val + align - 1) & ~(align - 1));
}

static u64
page_flags_for_prot(u32 prot)
{
	u64	flags;

	flags = PTE_PRESENT | PTE_USER;
	if (prot & API_MAP_WRITE) {
		flags |= PTE_RW;
	}
	if (!(prot & API_MAP_EXEC)) {
		flags |= PTE_NX;
	}
	return (flags);
}

static void
vm_map_shm_attach_vma(vma_t *vma)
{
	if (vma != NULL && vma->object != NULL &&
	    vma->object->type == VM_OBJ_SHM && vma->gem_handle != 0) {
		shm_attach((int)vma->gem_handle);
	}
}

static void
vm_map_shm_detach_vma(vma_t *vma)
{
	if (vma != NULL && vma->object != NULL &&
	    vma->object->type == VM_OBJ_SHM && vma->gem_handle != 0) {
		shm_detach((int)vma->gem_handle);
	}
}

static int
vm_map_unlink(process_t *proc, vma_t *target)
{
	vma_t	**pp;

	if (proc == NULL || target == NULL) {
		return (-1);
	}

	pp = &proc->vma_list;
	while (*pp != NULL) {
		if (*pp == target) {
			*pp = target->next;
			target->next = NULL;
			return (0);
		}
		pp = &(*pp)->next;
	}
	return (-1);
}

static void
vm_map_insert_vma(process_t *proc, vma_t *vma)
{
	vma_t	**pp;

	pp = &proc->vma_list;
	while (*pp != NULL && (*pp)->start < vma->start) {
		pp = &(*pp)->next;
	}
	vma->next = *pp;
	*pp = vma;
}

static int
vm_map_clip(process_t *proc, u64 addr)
{
	vma_t	*v;
	vma_t	*right;
	u64	delta;

	v = vm_map_lookup(proc, addr);
	if (v == NULL || addr == v->start) {
		return (0);
	}
	if (addr >= v->end) {
		return (0);
	}

	right = (vma_t *)kmem_calloc(sizeof(vma_t), 1);
	if (right == NULL) {
		return (-1);
	}

	delta = addr - v->start;
	right->start = addr;
	right->end = v->end;
	right->prot = v->prot;
	right->flags = v->flags;
	right->gem_handle = v->gem_handle;
	right->object_base = v->object_base + delta;
	right->object_offset = v->object_offset + delta;
	right->object = v->object;
	vm_object_ref(right->object);
	vm_map_shm_attach_vma(right);

	right->next = v->next;
	v->next = right;
	v->end = addr;
	return (0);
}

int
vm_map_range_free(process_t *proc, u64 start, u64 end, vma_t *ignore)
{
	vma_t	*v;

	if (proc == NULL || end <= start) {
		return (0);
	}

	for (v = proc->vma_list; v != NULL; v = v->next) {
		if (v == ignore) {
			continue;
		}
		if (start < v->end && end > v->start) {
			return (0);
		}
	}
	return (1);
}

int
vm_map_clip_range(process_t *proc, u64 start, u64 end)
{
	if (proc == NULL || end <= start) {
		return (-1);
	}
	if (vm_map_clip(proc, start) != 0) {
		return (-1);
	}
	if (vm_map_clip(proc, end) != 0) {
		return (-1);
	}
	return (0);
}

u64
vm_map_find_free(process_t *proc, u64 length)
{
	vma_t	*v;
	u64	aligned;
	u64	search_start;
	u64	addr;
	u64	end;
	int	overlap;

	aligned = align_up(length, PAGE_SIZE);
	if (aligned == 0) {
		return (0);
	}

	search_start = proc->mmap_base;
	if (search_start < MMAP_BASE) {
		search_start = MMAP_BASE;
	}
	if (search_start >= MMAP_LIMIT) {
		search_start = MMAP_BASE;
	}

	addr = align_up(search_start, PAGE_SIZE);

	while (addr + aligned <= MMAP_LIMIT) {
		end = addr + aligned;
		overlap = 0;

		for (v = proc->vma_list; v != NULL; v = v->next) {
			if (addr < v->end && end > v->start) {
				addr = align_up(v->end, PAGE_SIZE);
				overlap = 1;
				break;
			}
		}

		if (!overlap) {
			proc->mmap_base = end;
			return (addr);
		}

		if (addr + aligned > MMAP_LIMIT) {
			break;
		}
	}

	addr = MMAP_BASE;
	while (addr + aligned <= search_start) {
		end = addr + aligned;
		overlap = 0;

		for (v = proc->vma_list; v != NULL; v = v->next) {
			if (addr < v->end && end > v->start) {
				addr = align_up(v->end, PAGE_SIZE);
				overlap = 1;
				break;
			}
		}

		if (!overlap) {
			proc->mmap_base = end;
			return (addr);
		}
	}

	return (0);
}

int
vm_map_insert(process_t *proc, u64 start, u64 end, u32 prot,
    u32 flags, u32 gem_handle, vm_object_t *object,
    u64 object_offset)
{
	vma_t	*vma;

	if (proc == NULL || object == NULL || end <= start) {
		return (-1);
	}
	if (!vm_map_range_free(proc, start, end, NULL)) {
		return (-1);
	}

	vma = (vma_t *)kmem_calloc(sizeof(vma_t), 1);
	if (vma == NULL) {
		return (-1);
	}

	vma->start = start;
	vma->end = end;
	vma->prot = prot;
	vma->flags = flags;
	vma->gem_handle = gem_handle;
	vma->object_base = 0;
	vma->object_offset = object_offset;
	vma->object = object;
	vm_object_ref(object);

	vm_map_insert_vma(proc, vma);

	return (0);
}

int
vm_map_create_user_stack(process_t *proc)
{
	vm_object_t	*obj;
	u64		page;
	u64		va;
	u64		index;

	if (proc == NULL) {
		return (-1);
	}
	if (!vm_map_range_free(proc, USER_STACK_LIMIT, USER_STACK_END,
	    NULL)) {
		return (-1);
	}

	obj = vm_object_create(VM_OBJ_ANON, USER_STACK_MAX_SIZE, NULL);
	if (obj == NULL) {
		return (-1);
	}

	if (vm_map_insert(proc, USER_STACK_LIMIT, USER_STACK_END,
	    API_MAP_READ | API_MAP_WRITE,
	    API_MAP_PRIVATE | API_MAP_ANON, 0, obj, 0) != 0) {
		vm_object_unref(obj);
		return (-1);
	}
	vm_object_unref(obj);

	for (va = USER_STACK_TOP; va < USER_STACK_END; va += PAGE_SIZE) {
		page = vm_page_alloc_phys(0);
		if (page == 0) {
			vm_map_remove_range(proc, USER_STACK_LIMIT,
			    USER_STACK_END);
			return (-1);
		}
		memset((void *)(page + DMAP_BASE), 0, PAGE_SIZE);
		pmap_enter(va, page, PTE_PRESENT | PTE_RW | PTE_USER |
		    PTE_NX);

		index = (va - USER_STACK_LIMIT) / PAGE_SIZE;
		if (vm_object_set_page(obj, index, page) != 0) {
			pmap_remove(va);
			vm_page_free_phys(page);
			vm_map_remove_range(proc, USER_STACK_LIMIT,
			    USER_STACK_END);
			return (-1);
		}
	}

	return (0);
}

int
vm_map_remove(process_t *proc, u64 addr)
{
	vma_t	**pp;
	vma_t	*v;

	pp = &proc->vma_list;
	while (*pp != NULL) {
		if (addr >= (*pp)->start && addr < (*pp)->end) {
			v = *pp;
			*pp = v->next;
			vm_map_shm_detach_vma(v);
			vm_object_unref(v->object);
			kmem_free(v);
			return (0);
		}
		pp = &(*pp)->next;
	}
	return (-1);
}

int
vm_map_remove_range(process_t *proc, u64 start, u64 end)
{
	vma_t	*v;
	vma_t	*next;
	int	removed;

	if (proc == NULL || end <= start) {
		return (-1);
	}

	if (vm_map_clip_range(proc, start, end) != 0) {
		return (-1);
	}

	removed = 0;
	v = proc->vma_list;
	while (v != NULL) {
		next = v->next;
		if (v->start >= start && v->end <= end) {
			if (vm_map_unlink(proc, v) == 0) {
				vm_map_shm_detach_vma(v);
				vm_object_unref(v->object);
				kmem_free(v);
				removed = 1;
			}
		}
		v = next;
	}
	return (removed ? 0 : -1);
}

int
vm_map_relocate(process_t *proc, vma_t *vma, u64 new_start, u64 new_end)
{
	if (proc == NULL || vma == NULL || new_end <= new_start) {
		return (-1);
	}
	if (!vm_map_range_free(proc, new_start, new_end, vma)) {
		return (-1);
	}
	if (vm_map_unlink(proc, vma) != 0) {
		return (-1);
	}
	vma->start = new_start;
	vma->end = new_end;
	vm_map_insert_vma(proc, vma);
	return (0);
}

vma_t *
vm_map_lookup(process_t *proc, u64 addr)
{
	vma_t	*v;

	for (v = proc->vma_list; v != NULL; v = v->next) {
		if (addr >= v->start && addr < v->end) {
			return (v);
		}
	}
	return (NULL);
}

void
vm_map_free_all(process_t *proc)
{
	vma_t	*v;
	vma_t	*next;
	u64	va;

	v = proc->vma_list;
	while (v != NULL) {
		next = v->next;
		for (va = v->start; va < v->end; va += PAGE_SIZE) {
			u64	phys;

			if (v->object == NULL) {
				phys = pmap_extract(va);
				if (phys != 0) {
					vm_page_free_phys(phys);
				}
			}
			pmap_remove(va);
		}
		vm_map_shm_detach_vma(v);
		vm_object_unref(v->object);
		kmem_free(v);
		v = next;
	}
	proc->vma_list = NULL;
}

int
vm_map_fork(process_t *parent, process_t *child)
{
	vma_t		**child_tail;
	vma_t		*v;
	vma_t		*child_vma;
	vm_object_t	*child_shadow;
	vm_object_t	*parent_shadow;

	child_tail = &child->vma_list;
	child->vma_list = NULL;

	for (v = parent->vma_list; v != NULL; v = v->next) {
		child_vma = (vma_t *)kmem_calloc(
		    sizeof(vma_t), 1);
		if (child_vma == NULL) {
			vm_map_free_all(child);
			return (-1);
		}
		child_vma->start = v->start;
		child_vma->end = v->end;
		child_vma->prot = v->prot;
		child_vma->flags = v->flags;
		child_vma->gem_handle = v->gem_handle;
		child_vma->object_base = v->object_base;
		child_vma->object_offset = v->object_offset;

		if (v->object != NULL) {
			if (v->flags & (API_MAP_GEM | API_MAP_SHARED)) {
				child_vma->object = v->object;
				vm_object_ref(v->object);
				if (v->object->type == VM_OBJ_SHM &&
				    v->gem_handle != 0) {
					shm_attach((int)v->gem_handle);
				}
			} else {
				child_shadow =
				    vm_object_create_shadow(
				    v->object);
				if (child_shadow == NULL) {
					kmem_free(child_vma);
					vm_map_free_all(child);
					return (-1);
				}

				parent_shadow =
				    vm_object_create_shadow(
				    v->object);
				if (parent_shadow == NULL) {
					vm_object_unref(
					    child_shadow);
					kmem_free(child_vma);
					vm_map_free_all(child);
					return (-1);
				}

				vm_object_unref(v->object);
				v->object = parent_shadow;
				child_vma->object = child_shadow;
			}
		} else {
			child_vma->object = NULL;
		}

		child_vma->next = NULL;
		*child_tail = child_vma;
		child_tail = &child_vma->next;
	}

	return (0);
}

int
vm_map_fault(process_t *proc, u64 addr, u64 err_code)
{
	vma_t	*v;
	u64	page_va;
	u64	map_off;
	u64	object_off;
	u64	file_off;
	u64	index;
	u64	old_phys;
	u64	new_phys;
	u64	phys;
	u32	ref;

	v = vm_map_lookup(proc, addr);
	if (v == NULL || v->object == NULL) {
		return (-1);
	}

	if ((err_code & 0x2) && !(v->prot & API_MAP_WRITE)) {
		return (-1);
	}
	if ((err_code & 0x10) && !(v->prot & API_MAP_EXEC)) {
		return (-1);
	}

	page_va = addr & ~(PAGE_SIZE - 1);
	map_off = page_va - v->start;
	object_off = v->object_base + map_off;
	file_off = v->object_offset + map_off;
	index = object_off / PAGE_SIZE;

	if ((err_code & 0x1) && (err_code & 0x2) &&
	    (v->prot & API_MAP_WRITE)) {
		old_phys = vm_object_find_page(v->object, index);
		if (old_phys == 0) {
			old_phys = pmap_extract(page_va);
		}
		if (old_phys == 0) {
			return (-1);
		}

		ref = vm_page_ref_count(old_phys);
		if (ref > 1) {
			new_phys = vm_page_alloc_phys(0);
			if (new_phys == 0) {
				return (-1);
			}
			memcpy((void *)(new_phys + DMAP_BASE),
			    (void *)((old_phys & ~0xFFF) + DMAP_BASE),
			    PAGE_SIZE);
			vm_page_free_phys(old_phys);
			vm_object_set_page(v->object, index,
			    new_phys);
			pmap_enter(page_va, new_phys,
			    page_flags_for_prot(v->prot));
		} else {
			pmap_enter(page_va, old_phys,
			    page_flags_for_prot(v->prot));
		}
		return (0);
	}

	phys = vm_object_get_page(v->object, index, file_off);
	if (phys == 0) {
		return (-1);
	}

	pmap_enter(page_va, phys,
	    page_flags_for_prot(v->prot));
	return (0);
}

int
vm_cow_fault(u64 addr, u64 err_code)
{
	u64	page_va;
	u64	flags;
	u64	phys;
	u64	new_phys;
	u64	new_flags;
	u32	ref;

	if (!(err_code & 0x1) || !(err_code & 0x2)) {
		return (-1);
	}

	page_va = addr & ~(PAGE_SIZE - 1);
	flags = pmap_extract_flags(page_va);
	if (!(flags & PTE_COW)) {
		return (-1);
	}

	phys = pmap_extract(page_va);
	if (phys == 0) {
		return (-1);
	}

	new_flags = ((flags | PTE_RW) & ~PTE_COW) |
	    PTE_PRESENT;
	ref = vm_page_ref_count(phys);
	if (ref > 1) {
		new_phys = vm_page_alloc_phys(0);
		if (new_phys == 0) {
			return (-1);
		}
		memcpy((void *)(new_phys + DMAP_BASE),
		    (void *)((phys & ~0xFFF) + DMAP_BASE), PAGE_SIZE);
		vm_page_free_phys(phys);
		pmap_enter(page_va, new_phys, new_flags);
	} else {
		pmap_enter(page_va, phys, new_flags);
	}
	return (0);
}
