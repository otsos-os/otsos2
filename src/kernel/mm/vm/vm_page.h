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
$define %type vm_page_t as struct with phys_addr, ref_count, flags, order, segind, queue, object, pindex and two link pairs
$define %type vm_object_t as struct with the backing store of a page

$define %func vm_page_startup as procedure with args void
$define %func vm_page_alloc as function with args u32
$define %func vm_page_free as procedure with args vm_page_t *
$define %func vm_page_alloc_phys as function with args u32
$define %func vm_page_alloc_contig as function with args u32, u64, u64
$define %func vm_page_free_phys as function with args u64
$define %func vm_page_free_contig as procedure with args u64, u32
$define %func vm_page_reserve_range as procedure with args u64, u64
$define %func vm_page_ref as procedure with args vm_page_t *
$define %func vm_page_unref as function with args vm_page_t *
$define %func vm_page_ref_phys as procedure with args u64
$define %func vm_page_ref_count as function with args u64
$define %func vm_page_wire as procedure with args vm_page_t *
$define %func vm_page_unwire as procedure with args vm_page_t *
$define %func vm_page_activate as procedure with args vm_page_t *
$define %func vm_page_deactivate as procedure with args vm_page_t *
$define %func vm_page_dequeue as procedure with args vm_page_t *
$define %func vm_page_count_free as function with args void
$define %func vm_page_count_total as function with args void
$define %func vm_page_queue_count as function with args int
$define %func vm_page_queue_counts as procedure with args u64 *, u64 *, u64 *, u64 *
$define %func vm_page_lookup_phys as function with args u64
$define %func vm_page_dump as procedure with args void

$const PAGE_SHIFT as log2 of the page size
$const PQ_COUNT as number of paging queues

*/

/* !SPACE!

$space %export vm_page_startup
$space %export vm_page_alloc, vm_page_free, vm_page_alloc_phys
$space %export vm_page_alloc_contig, vm_page_free_phys
$space %export vm_page_free_contig, vm_page_reserve_range
$space %export vm_page_ref, vm_page_unref
$space %export vm_page_ref_phys, vm_page_ref_count
$space %export vm_page_wire, vm_page_unwire
$space %export vm_page_activate, vm_page_deactivate, vm_page_dequeue
$space %export vm_page_count_free, vm_page_count_total
$space %export vm_page_queue_count, vm_page_queue_counts
$space %export vm_page_lookup_phys, vm_page_dump

*/

#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <mlibc/mlibc.h>

#ifndef PAGE_SIZE
#define PAGE_SIZE		4096
#endif
#define PAGE_SHIFT		12
#define PAGE_MASK		((u64)(PAGE_SIZE - 1))

#define VM_ALLOC_NORMAL		0x0000
#define VM_ALLOC_WIRED		0x0001
#define VM_ALLOC_ZERO		0x0002
#define VM_ALLOC_NOWAIT		0x0004

#define PG_FREE			0x0001
#define PG_ALLOCATED		0x0002
#define PG_WIRED		0x0004
#define PG_EXCLUDED		0x0008
#define PG_ZERO			0x0010

#define PQ_NONE			0
#define PQ_ACTIVE		1
#define PQ_INACTIVE		2
#define PQ_LAUNDRY		3
#define PQ_COUNT		4

struct vm_object;

typedef struct vm_page {
	u64			phys_addr;
	struct vm_object	*object;
	u64			pindex;
	struct vm_page		*plinks_next;
	struct vm_page		*plinks_prev;
	struct vm_page		*qlinks_next;
	struct vm_page		*qlinks_prev;
	u32			ref_count;
	u32			wire_count;
	u16			flags;
	u8			order;
	u8			segind;
	u8			queue;
	u8			pad[3];
} vm_page_t;

void		vm_page_startup(void);

vm_page_t	*vm_page_alloc(u32 flags);
void		vm_page_free(vm_page_t *page);
u64		vm_page_alloc_phys(u32 flags);
u64		vm_page_alloc_contig(u32 page_total, u64 alignment,
		    u64 max_address);
int		vm_page_free_phys(u64 phys_addr);
void		vm_page_free_contig(u64 phys_addr, u32 page_total);
void		vm_page_reserve_range(u64 phys_start, u64 size);

void		vm_page_ref(vm_page_t *page);
int		vm_page_unref(vm_page_t *page);
void		vm_page_ref_phys(u64 phys_addr);
u32		vm_page_ref_count(u64 phys_addr);
void		vm_page_wire(vm_page_t *page);
void		vm_page_unwire(vm_page_t *page);

void		vm_page_activate(vm_page_t *page);
void		vm_page_deactivate(vm_page_t *page);
void		vm_page_dequeue(vm_page_t *page);

u64		vm_page_count_free(void);
u64		vm_page_count_total(void);
u64		vm_page_queue_count(int qid);
void		vm_page_queue_counts(u64 *active, u64 *inactive,
		    u64 *cache, u64 *wired);
vm_page_t	*vm_page_lookup_phys(u64 phys_addr);
void		vm_page_dump(void);

#endif
