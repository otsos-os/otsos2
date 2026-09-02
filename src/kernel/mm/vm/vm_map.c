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

$define %type vm_map_t as a standalone address space, RB-indexed by start address
$define %type vm_map_entry_t as one contiguous mapped range in an address space
$define %type vm_object_t as reference-counted VM backing object
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func align_up as function with args u64, u64
$define %func page_flags_for_prot as function with args u32
$define %func vm_map_entry_alloc as function with args void
$define %func vm_map_entry_release as procedure with args vm_map_entry_t *
$define %func vm_map_shm_attach_entry as procedure with args vm_map_entry_t *
$define %func vm_map_shm_detach_entry as procedure with args vm_map_entry_t *
$define %func rb_rotate_left as procedure with args vm_map_t *, vm_map_entry_t *
$define %func rb_rotate_right as procedure with args vm_map_t *, vm_map_entry_t *
$define %func rb_insert_fixup as procedure with args vm_map_t *, vm_map_entry_t *
$define %func rb_transplant as procedure with args vm_map_t *, vm_map_entry_t *, vm_map_entry_t *
$define %func rb_minimum as function with args vm_map_entry_t *
$define %func rb_is_black as function with args vm_map_entry_t *
$define %func rb_remove_fixup as procedure with args vm_map_t *, vm_map_entry_t *, vm_map_entry_t *
$define %func vm_map_link as procedure with args vm_map_t *, vm_map_entry_t *
$define %func vm_map_unlink as procedure with args vm_map_t *, vm_map_entry_t *
$define %func vm_map_lookup_locked as function with args vm_map_t *, u64
$define %func vm_map_range_free_locked as function with args vm_map_t *, u64, u64, vm_map_entry_t *
$define %func vm_map_clip_locked as function with args vm_map_t *, u64
$define %func vm_map_find_free_locked as function with args vm_map_t *, u64
$define %func vm_map_insert_locked as function with args vm_map_t *, u64, u64, u32, u32, u32, vm_object_t *, u64
$define %func vm_map_entry_destroy as procedure with args vm_map_entry_t *, int
$define %func vm_map_module_init as procedure with args void
$define %func vm_map_init as function with args vm_map_t *, u64, u64
$define %func vm_map_create as function with args u64, u64
$define %func vm_map_destroy as procedure with args vm_map_t *
$define %func vm_map_first as function with args vm_map_t *
$define %func vm_map_next as function with args vm_map_entry_t *
$define %func vm_map_entry_count as function with args vm_map_t *
$define %func vm_map_hint_get as function with args vm_map_t *
$define %func vm_map_hint_set as procedure with args vm_map_t *, u64
$define %func vm_map_find_free as function with args vm_map_t *, u64
$define %func vm_map_range_free as function with args vm_map_t *, u64, u64, vm_map_entry_t *
$define %func vm_map_clip_range as function with args vm_map_t *, u64, u64
$define %func vm_map_insert as function with args vm_map_t *, u64, u64, u32, u32, u32, vm_object_t *, u64
$define %func vm_map_create_user_stack as function with args vm_map_t *
$define %func vm_map_remove as function with args vm_map_t *, u64
$define %func vm_map_remove_range as function with args vm_map_t *, u64, u64
$define %func vm_map_relocate as function with args vm_map_t *, vm_map_entry_t *, u64, u64
$define %func vm_map_lookup as function with args vm_map_t *, u64
$define %func vm_map_free_all as procedure with args vm_map_t *
$define %func vm_map_fork as function with args vm_map_t *, vm_map_t *
$define %func vm_map_fault as function with args vm_map_t *, u64, u64
$define %func vm_cow_fault as function with args u64, u64

*/

/* !SPACE!

$space %internal align_up, page_flags_for_prot
$space %internal vm_map_entry_alloc, vm_map_entry_release
$space %internal vm_map_shm_attach_entry, vm_map_shm_detach_entry
$space %internal rb_rotate_left, rb_rotate_right, rb_insert_fixup
$space %internal rb_transplant, rb_minimum, rb_is_black, rb_remove_fixup
$space %internal vm_map_link, vm_map_unlink
$space %internal vm_map_lookup_locked, vm_map_range_free_locked
$space %internal vm_map_clip_locked, vm_map_find_free_locked
$space %internal vm_map_insert_locked, vm_map_entry_destroy
$space %export vm_map_module_init, vm_map_init, vm_map_create, vm_map_destroy
$space %export vm_map_first, vm_map_next, vm_map_entry_count
$space %export vm_map_hint_get, vm_map_hint_set
$space %export vm_map_find_free, vm_map_range_free, vm_map_clip_range
$space %export vm_map_insert, vm_map_create_user_stack
$space %export vm_map_remove, vm_map_remove_range
$space %export vm_map_relocate, vm_map_lookup, vm_map_free_all
$space %export vm_map_fork, vm_map_fault, vm_cow_fault

*/

#include <kernel/api/api.h>
#include <kernel/api/shm.h>
#include <mm/kmem.h>
#include <mm/uma.h>
#include <mm/vm/pmap.h>
#include <mm/vm/vm_map.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>

#define VM_MAP_PAGE_SIZE	4096
static uma_zone_t	vm_map_entry_zone;
static uma_zone_t	vm_map_zone;

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
	if ((prot & API_MAP_WRITE) != 0) {
		flags |= PTE_RW;
	}
	if ((prot & API_MAP_EXEC) == 0) {
		flags |= PTE_NX;
	}
	return (flags);
}

static vm_map_entry_t *
vm_map_entry_alloc(void)
{
	if (vm_map_entry_zone == NULL) {
		return (NULL);
	}
	return ((vm_map_entry_t *)uma_zalloc(vm_map_entry_zone, M_ZERO));
}

static void
vm_map_entry_release(vm_map_entry_t *entry)
{
	if (entry != NULL && vm_map_entry_zone != NULL) {
		uma_zfree(vm_map_entry_zone, entry);
	}
}

static void
vm_map_shm_attach_entry(vm_map_entry_t *entry)
{
	if (entry->object != NULL && entry->object->type == VM_OBJ_SHM &&
	    entry->gem_handle != 0) {
		shm_attach((int)entry->gem_handle);
	}
}

static void
vm_map_shm_detach_entry(vm_map_entry_t *entry)
{
	if (entry->object != NULL && entry->object->type == VM_OBJ_SHM &&
	    entry->gem_handle != 0) {
		shm_detach((int)entry->gem_handle);
	}
}

static void
rb_rotate_left(vm_map_t *map, vm_map_entry_t *x)
{
	vm_map_entry_t	*y;

	y = x->rb_right;
	x->rb_right = y->rb_left;
	if (y->rb_left != NULL) {
		y->rb_left->rb_parent = x;
	}
	y->rb_parent = x->rb_parent;
	if (x->rb_parent == NULL) {
		map->rb_root = y;
	} else if (x == x->rb_parent->rb_left) {
		x->rb_parent->rb_left = y;
	} else {
		x->rb_parent->rb_right = y;
	}
	y->rb_left = x;
	x->rb_parent = y;
}

static void
rb_rotate_right(vm_map_t *map, vm_map_entry_t *x)
{
	vm_map_entry_t	*y;

	y = x->rb_left;
	x->rb_left = y->rb_right;
	if (y->rb_right != NULL) {
		y->rb_right->rb_parent = x;
	}
	y->rb_parent = x->rb_parent;
	if (x->rb_parent == NULL) {
		map->rb_root = y;
	} else if (x == x->rb_parent->rb_right) {
		x->rb_parent->rb_right = y;
	} else {
		x->rb_parent->rb_left = y;
	}
	y->rb_right = x;
	x->rb_parent = y;
}

static void
rb_insert_fixup(vm_map_t *map, vm_map_entry_t *z)
{
	vm_map_entry_t	*parent;
	vm_map_entry_t	*grand;
	vm_map_entry_t	*uncle;

	while (z->rb_parent != NULL && z->rb_parent->rb_color == VM_MAP_RB_RED) {
		parent = z->rb_parent;
		grand = parent->rb_parent;
		if (grand == NULL) {
			break;
		}
		if (parent == grand->rb_left) {
			uncle = grand->rb_right;
			if (uncle != NULL && uncle->rb_color == VM_MAP_RB_RED) {
				parent->rb_color = VM_MAP_RB_BLACK;
				uncle->rb_color = VM_MAP_RB_BLACK;
				grand->rb_color = VM_MAP_RB_RED;
				z = grand;
				continue;
			}
			if (z == parent->rb_right) {
				z = parent;
				rb_rotate_left(map, z);
				parent = z->rb_parent;
				grand = parent->rb_parent;
			}
			parent->rb_color = VM_MAP_RB_BLACK;
			grand->rb_color = VM_MAP_RB_RED;
			rb_rotate_right(map, grand);
		} else {
			uncle = grand->rb_left;
			if (uncle != NULL && uncle->rb_color == VM_MAP_RB_RED) {
				parent->rb_color = VM_MAP_RB_BLACK;
				uncle->rb_color = VM_MAP_RB_BLACK;
				grand->rb_color = VM_MAP_RB_RED;
				z = grand;
				continue;
			}
			if (z == parent->rb_left) {
				z = parent;
				rb_rotate_right(map, z);
				parent = z->rb_parent;
				grand = parent->rb_parent;
			}
			parent->rb_color = VM_MAP_RB_BLACK;
			grand->rb_color = VM_MAP_RB_RED;
			rb_rotate_left(map, grand);
		}
	}
	map->rb_root->rb_color = VM_MAP_RB_BLACK;
}

static void
rb_transplant(vm_map_t *map, vm_map_entry_t *u, vm_map_entry_t *v)
{
	if (u->rb_parent == NULL) {
		map->rb_root = v;
	} else if (u == u->rb_parent->rb_left) {
		u->rb_parent->rb_left = v;
	} else {
		u->rb_parent->rb_right = v;
	}
	if (v != NULL) {
		v->rb_parent = u->rb_parent;
	}
}

static vm_map_entry_t *
rb_minimum(vm_map_entry_t *node)
{
	while (node->rb_left != NULL) {
		node = node->rb_left;
	}
	return (node);
}

static int
rb_is_black(vm_map_entry_t *node)
{
	return (node == NULL || node->rb_color == VM_MAP_RB_BLACK);
}

static void
rb_remove_fixup(vm_map_t *map, vm_map_entry_t *x, vm_map_entry_t *parent)
{
	vm_map_entry_t	*w;

	while (x != map->rb_root && rb_is_black(x)) {
		if (parent == NULL) {
			break;
		}
		if (x == parent->rb_left) {
			w = parent->rb_right;
			if (w != NULL && w->rb_color == VM_MAP_RB_RED) {
				w->rb_color = VM_MAP_RB_BLACK;
				parent->rb_color = VM_MAP_RB_RED;
				rb_rotate_left(map, parent);
				w = parent->rb_right;
			}
			if (w == NULL) {
				x = parent;
				parent = x->rb_parent;
				continue;
			}
			if (rb_is_black(w->rb_left) && rb_is_black(w->rb_right)) {
				w->rb_color = VM_MAP_RB_RED;
				x = parent;
				parent = x->rb_parent;
				continue;
			}
			if (rb_is_black(w->rb_right)) {
				if (w->rb_left != NULL) {
					w->rb_left->rb_color = VM_MAP_RB_BLACK;
				}
				w->rb_color = VM_MAP_RB_RED;
				rb_rotate_right(map, w);
				w = parent->rb_right;
			}
			if (w != NULL) {
				w->rb_color = parent->rb_color;
				if (w->rb_right != NULL) {
					w->rb_right->rb_color = VM_MAP_RB_BLACK;
				}
			}
			parent->rb_color = VM_MAP_RB_BLACK;
			rb_rotate_left(map, parent);
			x = map->rb_root;
			parent = NULL;
		} else {
			w = parent->rb_left;
			if (w != NULL && w->rb_color == VM_MAP_RB_RED) {
				w->rb_color = VM_MAP_RB_BLACK;
				parent->rb_color = VM_MAP_RB_RED;
				rb_rotate_right(map, parent);
				w = parent->rb_left;
			}
			if (w == NULL) {
				x = parent;
				parent = x->rb_parent;
				continue;
			}
			if (rb_is_black(w->rb_left) && rb_is_black(w->rb_right)) {
				w->rb_color = VM_MAP_RB_RED;
				x = parent;
				parent = x->rb_parent;
				continue;
			}
			if (rb_is_black(w->rb_left)) {
				if (w->rb_right != NULL) {
					w->rb_right->rb_color = VM_MAP_RB_BLACK;
				}
				w->rb_color = VM_MAP_RB_RED;
				rb_rotate_left(map, w);
				w = parent->rb_left;
			}
			if (w != NULL) {
				w->rb_color = parent->rb_color;
				if (w->rb_left != NULL) {
					w->rb_left->rb_color = VM_MAP_RB_BLACK;
				}
			}
			parent->rb_color = VM_MAP_RB_BLACK;
			rb_rotate_right(map, parent);
			x = map->rb_root;
			parent = NULL;
		}
	}
	if (x != NULL) {
		x->rb_color = VM_MAP_RB_BLACK;
	}
}

static void
vm_map_link(vm_map_t *map, vm_map_entry_t *entry)
{
	vm_map_entry_t	*node;
	vm_map_entry_t	*parent;
	int		 left;

	parent = NULL;
	node = map->rb_root;
	left = 0;
	while (node != NULL) {
		parent = node;
		if (entry->start < node->start) {
			node = node->rb_left;
			left = 1;
		} else {
			node = node->rb_right;
			left = 0;
		}
	}

	entry->rb_parent = parent;
	entry->rb_left = NULL;
	entry->rb_right = NULL;
	entry->rb_color = VM_MAP_RB_RED;
	if (parent == NULL) {
		map->rb_root = entry;
		entry->prev = NULL;
		entry->next = NULL;
		map->head = entry;
		map->tail = entry;
	} else {
		if (left) {
			parent->rb_left = entry;
			entry->prev = parent->prev;
			entry->next = parent;
		} else {
			parent->rb_right = entry;
			entry->prev = parent;
			entry->next = parent->next;
		}
		if (entry->prev != NULL) {
			entry->prev->next = entry;
		} else {
			map->head = entry;
		}
		if (entry->next != NULL) {
			entry->next->prev = entry;
		} else {
			map->tail = entry;
		}
	}
	rb_insert_fixup(map, entry);
	map->entry_count++;
}

static void
vm_map_unlink(vm_map_t *map, vm_map_entry_t *entry)
{
	vm_map_entry_t	*x;
	vm_map_entry_t	*y;
	vm_map_entry_t	*parent;
	u32		 removed_color;

	if (entry->prev != NULL) {
		entry->prev->next = entry->next;
	} else {
		map->head = entry->next;
	}
	if (entry->next != NULL) {
		entry->next->prev = entry->prev;
	} else {
		map->tail = entry->prev;
	}

	y = entry;
	removed_color = entry->rb_color;
	if (entry->rb_left == NULL) {
		x = entry->rb_right;
		parent = entry->rb_parent;
		rb_transplant(map, entry, entry->rb_right);
	} else if (entry->rb_right == NULL) {
		x = entry->rb_left;
		parent = entry->rb_parent;
		rb_transplant(map, entry, entry->rb_left);
	} else {
		y = rb_minimum(entry->rb_right);
		removed_color = y->rb_color;
		x = y->rb_right;
		if (y->rb_parent == entry) {
			parent = y;
		} else {
			parent = y->rb_parent;
			rb_transplant(map, y, y->rb_right);
			y->rb_right = entry->rb_right;
			y->rb_right->rb_parent = y;
		}
		rb_transplant(map, entry, y);
		y->rb_left = entry->rb_left;
		y->rb_left->rb_parent = y;
		y->rb_color = entry->rb_color;
	}
	if (removed_color == VM_MAP_RB_BLACK) {
		rb_remove_fixup(map, x, parent);
	}

	entry->rb_left = NULL;
	entry->rb_right = NULL;
	entry->rb_parent = NULL;
	entry->next = NULL;
	entry->prev = NULL;
	if (map->entry_count > 0) {
		map->entry_count--;
	}
}

static vm_map_entry_t *
vm_map_lookup_locked(vm_map_t *map, u64 addr)
{
	vm_map_entry_t	*entry;

	for (entry = map->rb_root; entry != NULL;) {
		if (addr < entry->start) {
			entry = entry->rb_left;
		} else if (addr >= entry->end) {
			entry = entry->rb_right;
		} else {
			return (entry);
		}
	}
	return (NULL);
}

static int
vm_map_range_free_locked(vm_map_t *map, u64 start, u64 end,
    vm_map_entry_t *ignore)
{
	vm_map_entry_t	*entry;

	if (end <= start || start < map->min_addr || end > map->max_addr) {
		return (0);
	}
	for (entry = map->head; entry != NULL; entry = entry->next) {
		if (entry == ignore) {
			continue;
		}
		if (entry->start >= end) {
			break;
		}
		if (entry->end > start) {
			return (0);
		}
	}
	return (1);
}

static int
vm_map_clip_locked(vm_map_t *map, u64 addr)
{
	vm_map_entry_t	*left;
	vm_map_entry_t	*right;

	left = vm_map_lookup_locked(map, addr);
	if (left == NULL || addr == left->start || addr >= left->end) {
		return (0);
	}
	right = vm_map_entry_alloc();
	if (right == NULL) {
		return (-1);
	}
	memcpy(right, left, sizeof(*right));
	right->start = addr;
	right->object_base += addr - left->start;
	left->end = addr;
	if (right->object != NULL) {
		vm_object_ref(right->object);
	}
	vm_map_shm_attach_entry(right);
	vm_map_link(map, right);
	return (0);
}

static u64
vm_map_find_free_locked(vm_map_t *map, u64 length)
{
	vm_map_entry_t	*entry;
	u64		 cursor;
	u64		 end;
	u64		 first;
	u64		 limit;
	int		 pass;

	if (length == 0 || length > map->max_addr - map->min_addr) {
		return (0);
	}
	first = map->hint;
	if (first < map->min_addr || first >= map->max_addr) {
		first = map->min_addr;
	}
	first = align_up(first, VM_MAP_PAGE_SIZE);
	for (pass = 0; pass < 2; pass++) {
		cursor = (pass == 0) ? first : map->min_addr;
		limit = (pass == 0) ? map->max_addr : first;
		if (cursor >= limit || length > limit - cursor) {
			continue;
		}
		for (entry = map->head; entry != NULL; entry = entry->next) {
			if (entry->end <= cursor) {
				continue;
			}
			if (cursor > limit - length) {
				break;
			}
			end = cursor + length;
			if (end <= entry->start) {
				map->hint = (end < map->max_addr) ? end : map->min_addr;
				return (cursor);
			}
			cursor = align_up(entry->end, VM_MAP_PAGE_SIZE);
		}
		if (cursor <= limit - length) {
			end = cursor + length;
			map->hint = (end < map->max_addr) ? end : map->min_addr;
			return (cursor);
		}
	}
	return (0);
}

static int
vm_map_insert_locked(vm_map_t *map, u64 start, u64 end, u32 prot,
    u32 flags, u32 gem_handle, vm_object_t *object, u64 object_offset)
{
	vm_map_entry_t	*entry;

	if (object == NULL || !vm_map_range_free_locked(map, start, end, NULL)) {
		return (-1);
	}
	entry = vm_map_entry_alloc();
	if (entry == NULL) {
		return (-1);
	}
	entry->start = start;
	entry->end = end;
	entry->prot = prot;
	entry->flags = flags;
	entry->gem_handle = gem_handle;
	entry->object_base = 0;
	entry->object_offset = object_offset;
	entry->object = object;
	vm_object_ref(object);
	vm_map_shm_attach_entry(entry);
	vm_map_link(map, entry);
	return (0);
}

static void
vm_map_entry_destroy(vm_map_entry_t *entry, int remove_pmap)
{
	u64	va;
	u64	phys;

	if (remove_pmap) {
		for (va = entry->start; va < entry->end; va += VM_MAP_PAGE_SIZE) {
			if (entry->object == NULL) {
				phys = pmap_extract(va);
				if (phys != 0) {
					vm_page_free_phys(phys);
				}
			}
			pmap_remove(va);
		}
	}
	vm_map_shm_detach_entry(entry);
	if (entry->object != NULL) {
		vm_object_unref(entry->object);
	}
	vm_map_entry_release(entry);
}

void
vm_map_module_init(void)
{
	if (vm_map_entry_zone != NULL && vm_map_zone != NULL) {
		return;
	}
	vm_map_entry_zone = uma_zcreate("vm_map_entry", sizeof(vm_map_entry_t),
	    UMA_ALIGN_CACHE, 0);
	vm_map_zone = uma_zcreate("vm_map", sizeof(vm_map_t), UMA_ALIGN_CACHE,
	    0);
}

int
vm_map_init(vm_map_t *map, u64 min_addr, u64 max_addr)
{
	if (map == NULL || min_addr >= max_addr ||
	    (min_addr & (VM_MAP_PAGE_SIZE - 1)) != 0 ||
	    (max_addr & (VM_MAP_PAGE_SIZE - 1)) != 0) {
		return (-1);
	}
	memset(map, 0, sizeof(*map));
	map->min_addr = min_addr;
	map->max_addr = max_addr;
	map->hint = min_addr;
	spin_init(&map->spin, "vm_map", LO_VM_MAP);
	return (0);
}

vm_map_t *
vm_map_create(u64 min_addr, u64 max_addr)
{
	vm_map_t	*map;

	if (vm_map_zone == NULL) {
		return (NULL);
	}
	map = (vm_map_t *)uma_zalloc(vm_map_zone, M_ZERO);
	if (map == NULL || vm_map_init(map, min_addr, max_addr) != 0) {
		if (map != NULL) {
			uma_zfree(vm_map_zone, map);
		}
		return (NULL);
	}
	return (map);
}

void
vm_map_destroy(vm_map_t *map)
{
	if (map == NULL) {
		return;
	}
	vm_map_free_all(map);
	if (vm_map_zone != NULL) {
		uma_zfree(vm_map_zone, map);
	}
}

vm_map_entry_t *
vm_map_first(vm_map_t *map)
{
	return (map != NULL ? map->head : NULL);
}

vm_map_entry_t *
vm_map_next(vm_map_entry_t *entry)
{
	return (entry != NULL ? entry->next : NULL);
}

u64
vm_map_entry_count(vm_map_t *map)
{
	u64	count;

	if (map == NULL) {
		return (0);
	}
	spin_lock(&map->spin);
	count = map->entry_count;
	spin_unlock(&map->spin);
	return (count);
}

u64
vm_map_hint_get(vm_map_t *map)
{
	u64	hint;

	if (map == NULL) {
		return (0);
	}
	spin_lock(&map->spin);
	hint = map->hint;
	spin_unlock(&map->spin);
	return (hint);
}

void
vm_map_hint_set(vm_map_t *map, u64 hint)
{
	if (map == NULL) {
		return;
	}
	spin_lock(&map->spin);
	map->hint = hint;
	spin_unlock(&map->spin);
}

u64
vm_map_find_free(vm_map_t *map, u64 length)
{
	u64	addr;

	if (map == NULL || length == 0 ||
	    (length & (VM_MAP_PAGE_SIZE - 1)) != 0) {
		return (0);
	}
	spin_lock(&map->spin);
	addr = vm_map_find_free_locked(map, length);
	spin_unlock(&map->spin);
	return (addr);
}

int
vm_map_range_free(vm_map_t *map, u64 start, u64 end, vm_map_entry_t *ignore)
{
	int	free;

	if (map == NULL) {
		return (0);
	}
	spin_lock(&map->spin);
	free = vm_map_range_free_locked(map, start, end, ignore);
	spin_unlock(&map->spin);
	return (free);
}

int
vm_map_clip_range(vm_map_t *map, u64 start, u64 end)
{
	int	result;

	if (map == NULL || end <= start) {
		return (-1);
	}
	spin_lock(&map->spin);
	result = vm_map_clip_locked(map, start);
	if (result == 0) {
		result = vm_map_clip_locked(map, end);
	}
	spin_unlock(&map->spin);
	return (result);
}

int
vm_map_insert(vm_map_t *map, u64 start, u64 end, u32 prot, u32 flags,
    u32 gem_handle, vm_object_t *object, u64 object_offset)
{
	int	result;

	if (map == NULL || object == NULL || end <= start ||
	    (start & (VM_MAP_PAGE_SIZE - 1)) != 0 ||
	    (end & (VM_MAP_PAGE_SIZE - 1)) != 0) {
		return (-1);
	}
	spin_lock(&map->spin);
	result = vm_map_insert_locked(map, start, end, prot, flags, gem_handle,
	    object, object_offset);
	spin_unlock(&map->spin);
	return (result);
}

int
vm_map_create_user_stack(vm_map_t *map)
{
	vm_object_t	*object;
	u64		 page;
	u64		 va;
	u64		 index;

	if (map == NULL || !vm_map_range_free(map, VM_MAP_STACK_LIMIT,
	    VM_MAP_STACK_END, NULL)) {
		return (-1);
	}
	object = vm_object_create(VM_OBJ_ANON, VM_MAP_STACK_MAX, NULL);
	if (object == NULL) {
		return (-1);
	}
	if (vm_map_insert(map, VM_MAP_STACK_LIMIT, VM_MAP_STACK_END,
	    API_MAP_READ | API_MAP_WRITE, API_MAP_PRIVATE | API_MAP_ANON, 0,
	    object, 0) != 0) {
		vm_object_unref(object);
		return (-1);
	}
	vm_object_unref(object);
	for (va = VM_MAP_STACK_TOP; va < VM_MAP_STACK_END;
	    va += VM_MAP_PAGE_SIZE) {
		page = vm_page_alloc_phys(VM_ALLOC_ZERO);
		if (page == 0) {
			vm_map_remove_range(map, VM_MAP_STACK_LIMIT, VM_MAP_STACK_END);
			return (-1);
		}
		pmap_enter(va, page, PTE_PRESENT | PTE_RW | PTE_USER | PTE_NX);
		index = (va - VM_MAP_STACK_LIMIT) / VM_MAP_PAGE_SIZE;
		if (vm_object_set_page(object, index, page) != 0) {
			pmap_remove(va);
			vm_page_free_phys(page);
			vm_map_remove_range(map, VM_MAP_STACK_LIMIT, VM_MAP_STACK_END);
			return (-1);
		}
	}
	return (0);
}

int
vm_map_remove(vm_map_t *map, u64 addr)
{
	vm_map_entry_t	*entry;

	if (map == NULL) {
		return (-1);
	}
	spin_lock(&map->spin);
	entry = vm_map_lookup_locked(map, addr);
	if (entry == NULL) {
		spin_unlock(&map->spin);
		return (-1);
	}
	vm_map_unlink(map, entry);
	vm_map_entry_destroy(entry, 1);
	spin_unlock(&map->spin);
	return (0);
}

int
vm_map_remove_range(vm_map_t *map, u64 start, u64 end)
{
	vm_map_entry_t	*entry;
	vm_map_entry_t	*next;
	int		 removed;

	if (map == NULL || end <= start) {
		return (-1);
	}
	spin_lock(&map->spin);
	if (vm_map_clip_locked(map, start) != 0 ||
	    vm_map_clip_locked(map, end) != 0) {
		spin_unlock(&map->spin);
		return (-1);
	}
	removed = 0;
	for (entry = map->head; entry != NULL; entry = next) {
		next = entry->next;
		if (entry->start >= end) {
			break;
		}
		if (entry->start >= start && entry->end <= end) {
			vm_map_unlink(map, entry);
			vm_map_entry_destroy(entry, 1);
			removed = 1;
		}
	}
	spin_unlock(&map->spin);
	return (removed ? 0 : -1);
}

int
vm_map_relocate(vm_map_t *map, vm_map_entry_t *entry, u64 new_start,
    u64 new_end)
{
	if (map == NULL || entry == NULL || new_end <= new_start ||
	    (new_start & (VM_MAP_PAGE_SIZE - 1)) != 0 ||
	    (new_end & (VM_MAP_PAGE_SIZE - 1)) != 0) {
		return (-1);
	}
	spin_lock(&map->spin);
	if (!vm_map_range_free_locked(map, new_start, new_end, entry)) {
		spin_unlock(&map->spin);
		return (-1);
	}
	vm_map_unlink(map, entry);
	entry->start = new_start;
	entry->end = new_end;
	vm_map_link(map, entry);
	spin_unlock(&map->spin);
	return (0);
}

vm_map_entry_t *
vm_map_lookup(vm_map_t *map, u64 addr)
{
	vm_map_entry_t	*entry;

	if (map == NULL) {
		return (NULL);
	}
	spin_lock(&map->spin);
	entry = vm_map_lookup_locked(map, addr);
	spin_unlock(&map->spin);
	return (entry);
}

void
vm_map_free_all(vm_map_t *map)
{
	vm_map_entry_t	*entry;

	if (map == NULL) {
		return;
	}
	spin_lock(&map->spin);
	while ((entry = map->head) != NULL) {
		vm_map_unlink(map, entry);
		vm_map_entry_destroy(entry, 1);
	}
	spin_unlock(&map->spin);
}


int
vm_map_fork(vm_map_t *parent, vm_map_t *child)
{
	vm_map_entry_t	*entry;
	vm_map_entry_t	*copy;
	vm_map_entry_t	*list;
	vm_map_entry_t	*tail;
	vm_map_entry_t	*next;
	vm_object_t	*child_shadow;
	vm_object_t	*parent_shadow;

	if (parent == NULL || child == NULL || parent == child) {
		return (-1);
	}

	list = NULL;
	tail = NULL;
	spin_lock(&parent->spin);
	for (entry = parent->head; entry != NULL; entry = entry->next) {
		copy = vm_map_entry_alloc();
		if (copy == NULL) {
			goto fail;
		}
		copy->start = entry->start;
		copy->end = entry->end;
		copy->prot = entry->prot;
		copy->flags = entry->flags;
		copy->gem_handle = entry->gem_handle;
		copy->object_base = entry->object_base;
		copy->object_offset = entry->object_offset;

		if (entry->object != NULL) {
			if ((entry->flags & (API_MAP_GEM | API_MAP_SHARED)) != 0) {
				copy->object = entry->object;
				vm_object_ref(entry->object);
				vm_map_shm_attach_entry(copy);
			} else {
				child_shadow =
				    vm_object_create_shadow(entry->object);
				if (child_shadow == NULL) {
					vm_map_entry_release(copy);
					goto fail;
				}
				parent_shadow =
				    vm_object_create_shadow(entry->object);
				if (parent_shadow == NULL) {
					vm_object_unref(child_shadow);
					vm_map_entry_release(copy);
					goto fail;
				}
				vm_object_unref(entry->object);
				entry->object = parent_shadow;
				copy->object = child_shadow;
			}
		}

		copy->next = NULL;
		if (tail == NULL) {
			list = copy;
		} else {
			tail->next = copy;
		}
		tail = copy;
	}
	child->min_addr = parent->min_addr;
	child->max_addr = parent->max_addr;
	child->hint = parent->hint;
	spin_unlock(&parent->spin);

	spin_lock(&child->spin);
	for (copy = list; copy != NULL; copy = next) {
		next = copy->next;
		copy->next = NULL;
		vm_map_link(child, copy);
	}
	spin_unlock(&child->spin);
	return (0);

fail:
	spin_unlock(&parent->spin);
	for (copy = list; copy != NULL; copy = next) {
		next = copy->next;
		copy->next = NULL;
		vm_map_entry_destroy(copy, 0);
	}
	return (-1);
}

int
vm_map_fault(vm_map_t *map, u64 addr, u64 err_code)
{
	vm_map_entry_t	*entry;
	vm_object_t	*object;
	u64		 page_va;
	u64		 map_off;
	u64		 object_base;
	u64		 object_offset;
	u64		 index;
	u64		 old_phys;
	u64		 new_phys;
	u64		 phys;
	u32		 prot;
	u32		 ref;
	int		 result;

	if (map == NULL) {
		return (-1);
	}
	page_va = addr & ~((u64)VM_MAP_PAGE_SIZE - 1);
	result = -1;
	object = NULL;

	spin_lock(&map->spin);
	entry = vm_map_lookup_locked(map, addr);
	if (entry == NULL || entry->object == NULL ||
	    ((err_code & 0x2) != 0 && (entry->prot & API_MAP_WRITE) == 0) ||
	    ((err_code & 0x10) != 0 && (entry->prot & API_MAP_EXEC) == 0)) {
		spin_unlock(&map->spin);
		return (-1);
	}
	prot = entry->prot;
	map_off = page_va - entry->start;
	object_base = entry->object_base;
	object_offset = entry->object_offset;
	object = entry->object;
	vm_object_ref(object);
	spin_unlock(&map->spin);

	index = (object_base + map_off) / VM_MAP_PAGE_SIZE;
	if ((err_code & 0x1) != 0 && (err_code & 0x2) != 0) {
		old_phys = vm_object_find_page(object, index);
		if (old_phys == 0) {
			old_phys = pmap_extract(page_va);
		}
		if (old_phys == 0) {
			goto out;
		}
		ref = vm_page_ref_count(old_phys);
		if (ref > 1) {
			new_phys = vm_page_alloc_phys(VM_ALLOC_NORMAL);
			if (new_phys == 0) {
				goto out;
			}
			memcpy((void *)(new_phys + DMAP_BASE),
			    (void *)((old_phys & ~0xFFFULL) + DMAP_BASE),
			    VM_MAP_PAGE_SIZE);
			if (vm_object_set_page(object, index, new_phys) != 0) {
				vm_page_free_phys(new_phys);
				goto out;
			}
			vm_page_free_phys(old_phys);
			phys = new_phys;
		} else {
			phys = old_phys;
		}
	} else {
		phys = vm_object_get_page(object, index, object_offset + map_off);
		if (phys == 0) {
			goto out;
		}
	}

	spin_lock(&map->spin);
	entry = vm_map_lookup_locked(map, addr);
	if (entry != NULL && entry->object == object && entry->prot == prot &&
	    entry->object_base == object_base &&
	    entry->object_offset == object_offset) {
		pmap_enter(page_va, phys, page_flags_for_prot(prot));
		result = 0;
	}
	spin_unlock(&map->spin);
out:
	vm_object_unref(object);
	return (result);
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

	if ((err_code & 0x1) == 0 || (err_code & 0x2) == 0) {
		return (-1);
	}
	page_va = addr & ~((u64)VM_MAP_PAGE_SIZE - 1);
	flags = pmap_extract_flags(page_va);
	if ((flags & PTE_COW) == 0) {
		return (-1);
	}
	phys = pmap_extract(page_va);
	if (phys == 0) {
		return (-1);
	}
	new_flags = ((flags | PTE_RW) & ~(u64)PTE_COW) | PTE_PRESENT;
	ref = vm_page_ref_count(phys);
	if (ref > 1) {
		new_phys = vm_page_alloc_phys(VM_ALLOC_NORMAL);
		if (new_phys == 0) {
			return (-1);
		}
		memcpy((void *)(new_phys + DMAP_BASE),
		    (void *)((phys & ~0xFFFULL) + DMAP_BASE), VM_MAP_PAGE_SIZE);
		vm_page_free_phys(phys);
		pmap_enter(page_va, new_phys, new_flags);
	} else {
		pmap_enter(page_va, phys, new_flags);
	}
	return (0);
}
