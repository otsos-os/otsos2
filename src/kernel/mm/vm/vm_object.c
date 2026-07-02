/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* !DEFINES!

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type vm_object_t as struct with type, ref_count, size, pages, page_count, shadow, pager, next
$define %type vm_pager_t as struct with type, size, handle, path, getpage, putpage, haspage

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

*/

/* !SPACE!

$space %export vm_object_init, vm_object_create
$space %export vm_object_create_shadow, vm_object_ref
$space %export vm_object_unref, vm_object_type
$space %export vm_object_page, vm_object_find_page
$space %export vm_object_set_page, vm_object_get_page

*/

#include <mm/vm/vm_object.h>
#include <mm/vm/vm_pager.h>
#include <mm/vm/vm_page.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

#define PAGE_SIZE 4096

static vm_object_t *vm_object_list = NULL;

void
vm_object_init(void)
{
	vm_object_list = NULL;
	com1_printf("[vm_object] initialized\n");
}

vm_object_t *
vm_object_create(u32 type, u64 size, void *backing)
{
	vm_object_t *obj;

	obj = kmem_calloc(1, sizeof(vm_object_t));
	if (obj == NULL)
		return (NULL);

	obj->type = type;
	obj->ref_count = 1;
	obj->size = size;
	obj->shadow = NULL;

	switch (type) {
	case VM_OBJ_ANON:
		obj->pager = vm_pager_create_default(size);
		break;
	case VM_OBJ_FILE:
		obj->pager = vm_pager_create_vnode(
		    (const char *)backing, size);
		break;
	case VM_OBJ_GEM:
		obj->pager = vm_pager_create_device(backing, size);
		break;
	default:
		obj->pager = NULL;
		break;
	}

	if (type != VM_OBJ_SHADOW && obj->pager == NULL) {
		kmem_free(obj);
		return (NULL);
	}

	obj->page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	if (obj->page_count != 0) {
		obj->pages = kmem_calloc(obj->page_count,
		    sizeof(u64));
		if (obj->pages == NULL) {
			if (obj->pager != NULL)
				vm_pager_destroy(obj->pager);
			kmem_free(obj);
			return (NULL);
		}
	}
	obj->next = vm_object_list;
	vm_object_list = obj;

	return (obj);
}

vm_object_t *
vm_object_create_shadow(vm_object_t *backing)
{
	vm_object_t *obj;

	if (backing == NULL)
		return (NULL);

	obj = kmem_calloc(1, sizeof(vm_object_t));
	if (obj == NULL)
		return (NULL);

	obj->type = VM_OBJ_SHADOW;
	obj->ref_count = 1;
	obj->size = backing->size;
	obj->shadow = backing;
	obj->pager = NULL;
	vm_object_ref(backing);

	obj->page_count = backing->page_count;
	if (obj->page_count != 0) {
		obj->pages = kmem_calloc(obj->page_count,
		    sizeof(u64));
		if (obj->pages == NULL) {
			vm_object_unref(backing);
			kmem_free(obj);
			return (NULL);
		}
	}
	obj->next = vm_object_list;
	vm_object_list = obj;

	return (obj);
}

void
vm_object_ref(vm_object_t *obj)
{
	if (obj == NULL)
		return;
	obj->ref_count++;
}

void
vm_object_unref(vm_object_t *obj)
{
	vm_object_t *cur, *prev;
	u64 i;

	if (obj == NULL)
		return;

	obj->ref_count--;

	if (obj->ref_count > 0)
		return;

	prev = NULL;
	cur = vm_object_list;
	while (cur != NULL) {
		if (cur == obj) {
			if (prev != NULL)
				prev->next = cur->next;
			else
				vm_object_list = cur->next;
			break;
		}
		prev = cur;
		cur = cur->next;
	}

	if (obj->pages != NULL) {
		for (i = 0; i < obj->page_count; i++) {
			if (obj->pages[i] != 0)
				vm_page_free_phys(obj->pages[i]);
		}
		kmem_free(obj->pages);
	}

	if (obj->shadow != NULL)
		vm_object_unref(obj->shadow);

	if (obj->pager != NULL)
		vm_pager_destroy(obj->pager);

	kmem_free(obj);
}

u32
vm_object_type(vm_object_t *obj)
{
	if (obj == NULL)
		return (0);
	return (obj->type);
}

u64
vm_object_page(vm_object_t *obj, u64 index)
{
	if (obj == NULL || index >= obj->page_count)
		return (0);
	return (obj->pages[index]);
}

u64
vm_object_find_page(vm_object_t *obj, u64 index)
{
	while (obj != NULL) {
		if (index < obj->page_count &&
		    obj->pages[index] != 0)
			return (obj->pages[index]);
		obj = obj->shadow;
	}
	return (0);
}

int
vm_object_set_page(vm_object_t *obj, u64 index, u64 phys)
{
	if (obj == NULL || index >= obj->page_count)
		return (-1);
	obj->pages[index] = phys;
	return (0);
}

u64
vm_object_get_page(vm_object_t *obj, u64 index, u64 file_offset)
{
	vm_object_t *p;
	u64 phys, page_phys;

	phys = vm_object_find_page(obj, index);
	if (phys != 0)
		return (phys);

	p = obj;
	while (p != NULL && p->pager == NULL)
		p = p->shadow;

	if (p == NULL || p->pager == NULL)
		return (0);

	if (p->pager->getpage(p->pager, file_offset,
	    &page_phys) != 0)
		return (0);

	if (index < p->page_count)
		p->pages[index] = page_phys;

	return (page_phys);
}

int
vm_object_resize(vm_object_t *obj, u64 new_size)
{
	u64	*new_pages;
	u64	new_page_count;
	u64	i;
	u64	copy_count;

	if (obj == NULL)
		return (-1);

	new_size = (new_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	if (new_size == obj->size)
		return (0);

	new_page_count = new_size / PAGE_SIZE;
	if (new_size < obj->size) {
		for (i = new_page_count; i < obj->page_count; i++) {
			if (obj->pages[i] != 0)
				vm_page_free_phys(obj->pages[i]);
		}
	}

	new_pages = kmem_calloc(new_page_count, sizeof(u64));
	if (new_pages == NULL)
		return (-1);

	copy_count = obj->page_count;
	if (copy_count > new_page_count)
		copy_count = new_page_count;
	for (i = 0; i < copy_count; i++)
		new_pages[i] = obj->pages[i];

	kmem_free(obj->pages);
	obj->pages = new_pages;
	obj->page_count = new_page_count;
	obj->size = new_size;
	if (obj->pager != NULL)
		obj->pager->size = new_size;

	return (0);
}
