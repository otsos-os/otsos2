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
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type vm_page_t as struct with phys_addr, state, ref_count, queue, queue_next, queue_prev

$define %func vm_page_init as procedure with args u64, u64
$define %func vm_page_init_from_bootmem as procedure with args void
$define %func vm_page_alloc as function with args u32
$define %func vm_page_free as procedure with args vm_page_t *
$define %func vm_page_alloc_phys as function with args u32
$define %func vm_page_alloc_contig as function with args u32, u64, u64
$define %func vm_page_free_phys as function with args u64
$define %func vm_page_free_contig as procedure with args u64, u32
$define %func vm_page_ref as procedure with args vm_page_t *
$define %func vm_page_unref as procedure with args vm_page_t *
$define %func vm_page_ref_phys as procedure with args u64
$define %func vm_page_ref_count as function with args u64
$define %func vm_page_activate as procedure with args vm_page_t *
$define %func vm_page_deactivate as procedure with args vm_page_t *
$define %func vm_page_cache_insert as procedure with args vm_page_t *
$define %func vm_page_count_free as function with args void
$define %func vm_page_count_total as function with args void
$define %func vm_page_queue_count as function with args int
$define %func vm_page_queue_counts as procedure with args u64 *, u64 *, u64 *, u64 *
$define %func vm_page_lookup as function with args u64
$define %func vm_page_dump as procedure with args void

*/

/* !SPACE!

$space %export vm_page_init, vm_page_init_from_bootmem
$space %export vm_page_alloc, vm_page_free, vm_page_alloc_phys
$space %export vm_page_alloc_contig, vm_page_free_phys
$space %export vm_page_free_contig, vm_page_ref, vm_page_unref
$space %export vm_page_ref_phys, vm_page_ref_count
$space %export vm_page_activate, vm_page_deactivate, vm_page_cache_insert
$space %export vm_page_count_free, vm_page_count_total
$space %export vm_page_queue_count, vm_page_queue_counts
$space %export vm_page_lookup, vm_page_dump

*/

#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <mlibc/mlibc.h>

#define VM_PAGE_FREE		0x00
#define VM_PAGE_USED		0x01
#define VM_PAGE_RESERVED	0x02
#define VM_PAGE_WIRED		0x04

#define PQ_NONE			0
#define PQ_FREE			1
#define PQ_CACHE		2
#define PQ_ACTIVE		3
#define PQ_INACTIVE		4
#define PQ_LAUNDRY		5
#define PQ_COUNT		6

typedef struct vm_page {
	u64			phys_addr;
	u32			state;
	u32			ref_count;
	u8			queue;
	struct vm_page		*queue_next;
	struct vm_page		*queue_prev;
} vm_page_t;

void		vm_page_init(u64 available_start, u64 available_end);
void		vm_page_init_from_bootmem(void);
vm_page_t	*vm_page_alloc(u32 flags);
void		vm_page_free(vm_page_t *page);
u64		vm_page_alloc_phys(u32 flags);
u64		vm_page_alloc_contig(u32 page_total, u64 alignment,
		    u64 max_address);
int		vm_page_free_phys(u64 phys_addr);
void		vm_page_free_contig(u64 phys_addr, u32 page_total);
void		vm_page_reserve_range(u64 phys_start, u64 size);
void		vm_page_ref(vm_page_t *page);
void		vm_page_unref(vm_page_t *page);
void		vm_page_ref_phys(u64 phys_addr);
u32		vm_page_ref_count(u64 phys_addr);
void		vm_page_activate(vm_page_t *page);
void		vm_page_deactivate(vm_page_t *page);
void		vm_page_cache_insert(vm_page_t *page);
u64		vm_page_count_free(void);
u64		vm_page_count_total(void);
u64		vm_page_queue_count(int qid);
void		vm_page_queue_counts(u64 *active, u64 *inactive,
		    u64 *cache, u64 *wired);
vm_page_t	*vm_page_lookup(u64 phys_addr);
void		vm_page_dump(void);

#endif
