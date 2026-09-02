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

$define %type vm_radix_node_t as one level of a sparse object page index
$define %type vm_object_t as independently reference-counted VM backing object
$define %type vm_page_t as descriptor for one managed physical page
$define %type vm_pager_t as backing pager with getpage and putpage operations
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func vm_radix_node_alloc as function with args void
$define %func vm_radix_node_free as procedure with args vm_radix_node_t *
$define %func vm_radix_slot as function with args u64, int
$define %func vm_radix_lookup_locked as function with args vm_object_t *, u64
$define %func vm_radix_insert_locked as function with args vm_object_t *, u64, vm_page_t *
$define %func vm_radix_remove_locked as function with args vm_object_t *, u64
$define %func vm_radix_destroy as procedure with args vm_radix_node_t *, int, vm_object_t *
$define %func vm_object_destroy as procedure with args vm_object_t *
$define %func vm_object_init as procedure with args void
$define %func vm_object_create as function with args u32, u64, void *
$define %func vm_object_create_shadow as function with args vm_object_t *
$define %func vm_object_ref as procedure with args vm_object_t *
$define %func vm_object_unref as procedure with args vm_object_t *
$define %func vm_object_type as function with args vm_object_t *
$define %func vm_object_page as function with args vm_object_t *, u64
$define %func vm_object_find_page as function with args vm_object_t *, u64
$define %func vm_object_set_page as function with args vm_object_t *, u64, u64
$define %func vm_object_get_page as function with args vm_object_t *, u64, u64
$define %func vm_object_resize as function with args vm_object_t *, u64

*/

/* !SPACE!

$space %internal vm_radix_node_alloc, vm_radix_node_free
$space %internal vm_radix_slot, vm_radix_lookup_locked
$space %internal vm_radix_insert_locked, vm_radix_remove_locked, vm_radix_destroy
$space %internal vm_object_destroy
$space %export vm_object_init, vm_object_create, vm_object_create_shadow
$space %export vm_object_ref, vm_object_unref, vm_object_type
$space %export vm_object_page, vm_object_find_page, vm_object_set_page
$space %export vm_object_get_page, vm_object_resize

*/

#include <mm/kmem.h>
#include <mm/uma.h>
#include <mm/vm/vm_object.h>
#include <mm/vm/vm_page.h>
#include <mm/vm/vm_pager.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define VM_OBJECT_PAGE_SHIFT	12
#define VM_OBJECT_PAGE_SIZE	(1ULL << VM_OBJECT_PAGE_SHIFT)

#define VM_OBJECT_PINDEX_BITS	(VM_RADIX_LEVELS * VM_RADIX_SHIFT)
#define VM_OBJECT_SIZE_MAX	(((u64)1 << VM_OBJECT_PINDEX_BITS) << \
				    VM_OBJECT_PAGE_SHIFT)

static spin_t		vm_object_list_spin =
	SPIN_INITIALIZER("vm_object", LO_KMEM);
static vm_object_t	*vm_object_list;
static uma_zone_t	vm_radix_zone;

static vm_radix_node_t *
vm_radix_node_alloc(void)
{
	if (vm_radix_zone == NULL) {
		return (NULL);
	}
	return ((vm_radix_node_t *)uma_zalloc(vm_radix_zone, M_ZERO));
}

static void
vm_radix_node_free(vm_radix_node_t *node)
{
	if (node != NULL && vm_radix_zone != NULL) {
		uma_zfree(vm_radix_zone, node);
	}
}

static u32
vm_radix_slot(u64 index, int level)
{
	return ((u32)((index >> (level * VM_RADIX_SHIFT)) &
	    (VM_RADIX_SLOTS - 1)));
}

static vm_page_t *
vm_radix_lookup_locked(vm_object_t *obj, u64 index)
{
	vm_radix_node_t	*node;
	int		 level;

	node = obj->root;
	if (node == NULL) {
		return (NULL);
	}
	for (level = VM_RADIX_LEVELS - 1; level > 0; level--) {
		node = (vm_radix_node_t *)node->slots[vm_radix_slot(index, level)];
		if (node == NULL) {
			return (NULL);
		}
	}
	return ((vm_page_t *)node->slots[vm_radix_slot(index, 0)]);
}

static int
vm_radix_insert_locked(vm_object_t *obj, u64 index, vm_page_t *page)
{
	vm_radix_node_t	*node;
	vm_radix_node_t	*child;
	int		 level;

	if (obj->root == NULL) {
		obj->root = vm_radix_node_alloc();
		if (obj->root == NULL) {
			return (-1);
		}
	}
	node = obj->root;
	for (level = VM_RADIX_LEVELS - 1; level > 0; level--) {
		child = (vm_radix_node_t *)node->slots[vm_radix_slot(index, level)];
		if (child == NULL) {
			child = vm_radix_node_alloc();
			if (child == NULL) {
				return (-1);
			}
			node->slots[vm_radix_slot(index, level)] = child;
		}
		node = child;
	}
	if (node->slots[vm_radix_slot(index, 0)] != NULL) {
		return (-1);
	}
	node->slots[vm_radix_slot(index, 0)] = page;
	return (0);
}

static vm_page_t *
vm_radix_remove_locked(vm_object_t *obj, u64 index)
{
	vm_radix_node_t	*node;
	vm_page_t	*page;
	int		 level;

	node = obj->root;
	if (node == NULL) {
		return (NULL);
	}
	for (level = VM_RADIX_LEVELS - 1; level > 0; level--) {
		node = (vm_radix_node_t *)node->slots[vm_radix_slot(index, level)];
		if (node == NULL) {
			return (NULL);
		}
	}
	page = (vm_page_t *)node->slots[vm_radix_slot(index, 0)];
	node->slots[vm_radix_slot(index, 0)] = NULL;
	return (page);
}

static void
vm_radix_destroy(vm_radix_node_t *node, int level, vm_object_t *obj)
{
	vm_page_t	*page;
	u32		 i;

	if (node == NULL) {
		return;
	}
	if (level == 0) {
		for (i = 0; i < VM_RADIX_SLOTS; i++) {
			page = (vm_page_t *)node->slots[i];
			if (page == NULL) {
				continue;
			}
			if (page->object == obj) {
				page->object = NULL;
				page->pindex = 0;
			}
			if (obj->type != VM_OBJ_GEM && obj->type != VM_OBJ_DEVICE) {
				vm_page_free_phys(page->phys_addr);
			}
		}
	} else {
		for (i = 0; i < VM_RADIX_SLOTS; i++) {
			vm_radix_destroy((vm_radix_node_t *)node->slots[i], level - 1,
			    obj);
		}
	}
	vm_radix_node_free(node);
}

static void
vm_object_destroy(vm_object_t *obj)
{
	vm_object_t	*cur;
	vm_object_t	*prev;

	spin_lock(&vm_object_list_spin);
	prev = NULL;
	for (cur = vm_object_list; cur != NULL; cur = cur->next) {
		if (cur == obj) {
			if (prev == NULL) {
				vm_object_list = cur->next;
			} else {
				prev->next = cur->next;
			}
			break;
		}
		prev = cur;
	}
	spin_unlock(&vm_object_list_spin);

	spin_lock(&obj->spin);
	vm_radix_destroy(obj->root, VM_RADIX_LEVELS - 1, obj);
	obj->root = NULL;
	spin_unlock(&obj->spin);
	if (obj->shadow != NULL) {
		vm_object_unref(obj->shadow);
	}
	if (obj->pager != NULL) {
		vm_pager_destroy(obj->pager);
	}
	kmem_free(obj);
}

void
vm_object_init(void)
{
	spin_lock(&vm_object_list_spin);
	vm_object_list = NULL;
	spin_unlock(&vm_object_list_spin);
	vm_radix_zone = uma_zcreate("vm_radix", sizeof(vm_radix_node_t),
	    UMA_ALIGN_CACHE, 0);
	if (vm_radix_zone == NULL) {
		printk("vm_object: cannot create radix node zone\n");
		return;
	}
	printk("vm_object: radix index initialized (%u-way, %d levels)\n",
	    VM_RADIX_SLOTS, VM_RADIX_LEVELS);
}

vm_object_t *
vm_object_create(u32 type, u64 size, void *backing)
{
	vm_object_t	*obj;

	if (size > VM_OBJECT_SIZE_MAX) {
		return (NULL);
	}
	obj = kmem_calloc(1, sizeof(*obj));
	if (obj == NULL) {
		return (NULL);
	}
	obj->type = type;
	obj->ref_count = 1;
	obj->size = (size + VM_OBJECT_PAGE_SIZE - 1) &
	    ~(VM_OBJECT_PAGE_SIZE - 1);
	obj->page_count = obj->size >> VM_OBJECT_PAGE_SHIFT;
	spin_init(&obj->spin, "vm_object", LO_KMEM);

	switch (type) {
	case VM_OBJ_ANON:
	case VM_OBJ_SHM:
		obj->pager = vm_pager_create_default(obj->size);
		break;
	case VM_OBJ_FILE:
		obj->pager = vm_pager_create_vnode((const char *)backing, obj->size);
		break;
	case VM_OBJ_GEM:
	case VM_OBJ_DEVICE:
		obj->pager = vm_pager_create_device(backing, obj->size);
		break;
	default:
		kmem_free(obj);
		return (NULL);
	}
	if (obj->pager == NULL) {
		kmem_free(obj);
		return (NULL);
	}
	spin_lock(&vm_object_list_spin);
	obj->next = vm_object_list;
	vm_object_list = obj;
	spin_unlock(&vm_object_list_spin);
	return (obj);
}

vm_object_t *
vm_object_create_shadow(vm_object_t *backing)
{
	vm_object_t	*obj;

	if (backing == NULL) {
		return (NULL);
	}
	obj = kmem_calloc(1, sizeof(*obj));
	if (obj == NULL) {
		return (NULL);
	}
	obj->type = VM_OBJ_SHADOW;
	obj->ref_count = 1;
	obj->size = backing->size;
	obj->page_count = backing->page_count;
	obj->shadow = backing;
	spin_init(&obj->spin, "vm_shadow", LO_KMEM);
	vm_object_ref(backing);
	spin_lock(&vm_object_list_spin);
	obj->next = vm_object_list;
	vm_object_list = obj;
	spin_unlock(&vm_object_list_spin);
	return (obj);
}

void
vm_object_ref(vm_object_t *obj)
{
	if (obj != NULL) {
		__atomic_fetch_add(&obj->ref_count, 1, __ATOMIC_RELAXED);
	}
}

void
vm_object_unref(vm_object_t *obj)
{
	if (obj == NULL) {
		return;
	}
	if (__atomic_fetch_sub(&obj->ref_count, 1, __ATOMIC_ACQ_REL) == 1) {
		vm_object_destroy(obj);
	}
}

u32
vm_object_type(vm_object_t *obj)
{
	return (obj != NULL ? obj->type : 0);
}

u64
vm_object_page(vm_object_t *obj, u64 index)
{
	vm_page_t	*page;

	if (obj == NULL || index >= obj->page_count) {
		return (0);
	}
	spin_lock(&obj->spin);
	page = vm_radix_lookup_locked(obj, index);
	spin_unlock(&obj->spin);
	return (page != NULL ? page->phys_addr : 0);
}

u64
vm_object_find_page(vm_object_t *obj, u64 index)
{
	u64	phys;

	for (; obj != NULL; obj = obj->shadow) {
		phys = vm_object_page(obj, index);
		if (phys != 0) {
			return (phys);
		}
	}
	return (0);
}

int
vm_object_set_page(vm_object_t *obj, u64 index, u64 phys)
{
	vm_page_t	*page;
	vm_page_t	*old;

	if (obj == NULL || index >= obj->page_count || phys == 0) {
		return (-1);
	}
	page = vm_page_lookup_phys(phys);
	if (page == NULL) {
		return (-1);
	}
	spin_lock(&obj->spin);
	old = vm_radix_lookup_locked(obj, index);
	if (old != NULL) {
		spin_unlock(&obj->spin);
		return (old == page ? 0 : -1);
	}
	if (page->object != NULL && page->object != obj) {
		spin_unlock(&obj->spin);
		return (-1);
	}
	if (vm_radix_insert_locked(obj, index, page) != 0) {
		spin_unlock(&obj->spin);
		return (-1);
	}
	page->object = obj;
	page->pindex = index;
	spin_unlock(&obj->spin);
	return (0);
}

u64
vm_object_get_page(vm_object_t *obj, u64 index, u64 file_offset)
{
	vm_object_t	*pager_obj;
	vm_page_t	*page;
	u64		 phys;

	phys = vm_object_find_page(obj, index);
	if (phys != 0) {
		return (phys);
	}
	for (pager_obj = obj; pager_obj != NULL && pager_obj->pager == NULL;
	    pager_obj = pager_obj->shadow) {
		;
	}
	if (pager_obj == NULL || pager_obj->pager == NULL ||
	    pager_obj->pager->getpage(pager_obj->pager, file_offset, &phys) != 0 ||
	    phys == 0) {
		return (0);
	}
	if (pager_obj->type == VM_OBJ_GEM || pager_obj->type == VM_OBJ_DEVICE) {
		return (phys);
	}
	page = vm_page_lookup_phys(phys);
	if (page == NULL || vm_object_set_page(pager_obj, index, phys) != 0) {
		vm_page_free_phys(phys);
		return (0);
	}
	return (phys);
}

int
vm_object_resize(vm_object_t *obj, u64 new_size)
{
	vm_page_t	*page;
	u64		 new_pages;
	u64		 index;

	if (obj == NULL || new_size > VM_OBJECT_SIZE_MAX) {
		return (-1);
	}
	new_size = (new_size + VM_OBJECT_PAGE_SIZE - 1) &
	    ~(VM_OBJECT_PAGE_SIZE - 1);
	new_pages = new_size >> VM_OBJECT_PAGE_SHIFT;
	spin_lock(&obj->spin);
	if (new_pages < obj->page_count) {
		for (index = new_pages; index < obj->page_count; index++) {
			page = vm_radix_remove_locked(obj, index);
			if (page == NULL) {
				continue;
			}
			if (page->object == obj) {
				page->object = NULL;
				page->pindex = 0;
			}
			if (obj->type != VM_OBJ_GEM && obj->type != VM_OBJ_DEVICE) {
				vm_page_free_phys(page->phys_addr);
			}
		}
	}
	obj->size = new_size;
	obj->page_count = new_pages;
	if (obj->pager != NULL) {
		obj->pager->size = new_size;
	}
	spin_unlock(&obj->spin);
	return (0);
}
