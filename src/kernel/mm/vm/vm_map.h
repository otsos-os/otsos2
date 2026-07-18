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
$define %type process_t as struct with pid, ppid, state, name, cr3, entry_point, stacks, context, vma_list

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

$space %export vm_map_find_free, vm_map_range_free, vm_map_clip_range
$space %export vm_map_insert, vm_map_create_user_stack
$space %export vm_map_remove, vm_map_remove_range
$space %export vm_map_relocate, vm_map_lookup, vm_map_free_all
$space %export vm_map_fork, vm_map_fault, vm_cow_fault

*/

#ifndef VM_MAP_H
#define VM_MAP_H

#include <mlibc/mlibc.h>
#include <kernel/process.h>
#include <mm/vm/vm_object.h>

#define VM_MAP_READ		0x1
#define VM_MAP_WRITE		0x2
#define VM_MAP_EXEC		0x4

#define VM_MAP_PRIVATE		0x02
#define VM_MAP_FIXED		0x10
#define VM_MAP_ANON		0x20
#define VM_MAP_GEM		0x40

u64		vm_map_find_free(process_t *proc, u64 length);
int		vm_map_range_free(process_t *proc, u64 start, u64 end,
		    vma_t *ignore);
int		vm_map_clip_range(process_t *proc, u64 start, u64 end);
int		vm_map_insert(process_t *proc, u64 start, u64 end,
		    u32 prot, u32 flags, u32 gem_handle,
		    vm_object_t *object, u64 object_offset);
int		vm_map_create_user_stack(process_t *proc);
int		vm_map_remove(process_t *proc, u64 addr);
int		vm_map_remove_range(process_t *proc, u64 start, u64 end);
int		vm_map_relocate(process_t *proc, vma_t *vma, u64 new_start,
		    u64 new_end);
vma_t		*vm_map_lookup(process_t *proc, u64 addr);
void		vm_map_free_all(process_t *proc);
int		vm_map_fork(process_t *parent, process_t *child);
int		vm_map_fault(process_t *proc, u64 addr, u64 err_code);
int		vm_cow_fault(u64 addr, u64 err_code);

#endif
