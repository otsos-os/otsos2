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
$define %type vm_map_entry_t as one contiguous mapped range in an address space
$define %type vm_map_t as a standalone address space, RB-indexed by start address
$define %type vm_object_t as reference-counted VM backing object

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

$space %export vm_map_module_init, vm_map_init, vm_map_create, vm_map_destroy
$space %export vm_map_first, vm_map_next, vm_map_entry_count
$space %export vm_map_hint_get, vm_map_hint_set
$space %export vm_map_find_free, vm_map_range_free, vm_map_clip_range
$space %export vm_map_insert, vm_map_create_user_stack
$space %export vm_map_remove, vm_map_remove_range
$space %export vm_map_relocate, vm_map_lookup, vm_map_free_all
$space %export vm_map_fork, vm_map_fault, vm_cow_fault

*/

#ifndef VM_MAP_H
#define VM_MAP_H

#include <kernel/sync/sync.h>
#include <mlibc/mlibc.h>
#include <mm/vm/vm_object.h>

#define VM_MAP_READ		0x1
#define VM_MAP_WRITE		0x2
#define VM_MAP_EXEC		0x4

#define VM_MAP_PRIVATE		0x02
#define VM_MAP_FIXED		0x10
#define VM_MAP_ANON		0x20
#define VM_MAP_GEM		0x40

#define VM_MAP_PAGE_SIZE	4096
#define VM_MAP_STACK_MAX	(8 * 1024 * 1024)
#define VM_MAP_STACK_END	0x0000800000000000ULL
#define VM_MAP_STACK_LIMIT	(VM_MAP_STACK_END - VM_MAP_STACK_MAX)
#define VM_MAP_STACK_INIT	(64 * 1024)
#define VM_MAP_STACK_TOP	(VM_MAP_STACK_END - VM_MAP_STACK_INIT)
#define VM_MAP_RB_RED		0
#define VM_MAP_RB_BLACK	1


typedef struct vm_map_entry {
	u64			 start;
	u64			 end;
	u32			 prot;
	u32			 flags;
	u32			 gem_handle;
	u32			 rb_color;
	u64			 object_base;
	u64			 object_offset;
	vm_object_t		*object;
	struct vm_map_entry	*rb_left;
	struct vm_map_entry	*rb_right;
	struct vm_map_entry	*rb_parent;
	struct vm_map_entry	*next;
	struct vm_map_entry	*prev;
} vm_map_entry_t;


typedef struct vm_map {
	vm_map_entry_t	*rb_root;
	vm_map_entry_t	*head;
	vm_map_entry_t	*tail;
	u64		 min_addr;
	u64		 max_addr;
	u64		 hint;
	u64		 pmap_root;
	u64		 entry_count;
	spin_t		 spin;
} vm_map_t;

void		vm_map_module_init(void);
int		vm_map_init(vm_map_t *map, u64 min_addr, u64 max_addr);
vm_map_t	*vm_map_create(u64 min_addr, u64 max_addr);
void		vm_map_destroy(vm_map_t *map);
vm_map_entry_t	*vm_map_first(vm_map_t *map);
vm_map_entry_t	*vm_map_next(vm_map_entry_t *entry);
u64		vm_map_entry_count(vm_map_t *map);
u64		vm_map_hint_get(vm_map_t *map);
void		vm_map_hint_set(vm_map_t *map, u64 hint);
u64		vm_map_find_free(vm_map_t *map, u64 length);
int		vm_map_range_free(vm_map_t *map, u64 start, u64 end,
		    vm_map_entry_t *ignore);
int		vm_map_clip_range(vm_map_t *map, u64 start, u64 end);
int		vm_map_insert(vm_map_t *map, u64 start, u64 end, u32 prot,
		    u32 flags, u32 gem_handle, vm_object_t *object,
		    u64 object_offset);
int		vm_map_create_user_stack(vm_map_t *map);
int		vm_map_remove(vm_map_t *map, u64 addr);
int		vm_map_remove_range(vm_map_t *map, u64 start, u64 end);
int		vm_map_relocate(vm_map_t *map, vm_map_entry_t *entry,
		    u64 new_start, u64 new_end);
vm_map_entry_t	*vm_map_lookup(vm_map_t *map, u64 addr);
void		vm_map_free_all(vm_map_t *map);
int		vm_map_fork(vm_map_t *parent, vm_map_t *child);
int		vm_map_fault(vm_map_t *map, u64 addr, u64 err_code);
int		vm_cow_fault(u64 addr, u64 err_code);

#endif
