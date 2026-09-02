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
$define %type vm_object_t as independently reference-counted VM backing object
$define %type vm_radix_node_t as 512-way internal node in an object page index
$define %type vm_pager_t as backing pager with getpage and putpage operations

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
$define %func vm_object_init as procedure with args void

*/

/* !SPACE!

$space %export vm_object_create, vm_object_create_shadow
$space %export vm_object_ref, vm_object_unref, vm_object_type
$space %export vm_object_page, vm_object_find_page, vm_object_set_page
$space %export vm_object_get_page, vm_object_resize, vm_object_init

*/

#ifndef VM_OBJECT_H
#define VM_OBJECT_H

#include <kernel/sync/sync.h>
#include <mlibc/mlibc.h>

#define VM_OBJ_ANON		0x01
#define VM_OBJ_FILE		0x02
#define VM_OBJ_GEM		0x04
#define VM_OBJ_SHADOW		0x08
#define VM_OBJ_SHM		0x10
#define VM_OBJ_DEVICE		0x20

#define VM_RADIX_SHIFT		9
#define VM_RADIX_SLOTS		(1U << VM_RADIX_SHIFT)
#define VM_RADIX_LEVELS	4

struct vm_pager;
struct vm_page;

typedef struct vm_radix_node {
	void	*slots[VM_RADIX_SLOTS];
} vm_radix_node_t;

typedef struct vm_object {
	u32			 type;
	u32			 ref_count;
	u64			 size;
	u64			 page_count;
	vm_radix_node_t		*root;
	struct vm_object	*shadow;
	struct vm_pager		*pager;
	struct vm_object	*next;
	spin_t			 spin;
} vm_object_t;

vm_object_t	*vm_object_create(u32 type, u64 size, void *backing);
vm_object_t	*vm_object_create_shadow(vm_object_t *backing);
void		vm_object_ref(vm_object_t *obj);
void		vm_object_unref(vm_object_t *obj);
u32		vm_object_type(vm_object_t *obj);
u64		vm_object_page(vm_object_t *obj, u64 index);
u64		vm_object_find_page(vm_object_t *obj, u64 index);
int		vm_object_set_page(vm_object_t *obj, u64 index, u64 phys);
u64		vm_object_get_page(vm_object_t *obj, u64 index, u64 file_offset);
int		vm_object_resize(vm_object_t *obj, u64 new_size);
void		vm_object_init(void);

#endif
