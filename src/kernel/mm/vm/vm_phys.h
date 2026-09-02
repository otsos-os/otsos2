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
$define %type vm_page_t as struct with the per-physical-page state
$define %type vm_phys_seg_t as struct with start, end, first_page
$define %type vm_phys_stat_t as struct with the allocator counters

$define %func vm_phys_init as procedure with args void
$define %func vm_phys_set_page_array as procedure with args vm_page_t *, u64
$define %func vm_phys_add_seg as function with args u64, u64
$define %func vm_phys_free_range as procedure with args u64, u64
$define %func vm_phys_alloc as function with args int
$define %func vm_phys_free as procedure with args vm_page_t *
$define %func vm_phys_alloc_contig as function with args u64, u64, u64, u64
$define %func vm_phys_free_contig as procedure with args vm_page_t *, u64
$define %func vm_phys_paddr_to_page as function with args u64
$define %func vm_phys_page_to_paddr as function with args vm_page_t *
$define %func vm_phys_managed as function with args u64
$define %func vm_phys_exclude as procedure with args u64, u64
$define %func vm_phys_stats as procedure with args vm_phys_stat_t *
$define %func vm_phys_dump as procedure with args void

$const VM_NFREEORDER as number of buddy orders, block sizes 1..2^(N-1) pages
$const VM_PHYS_MAX_SEG as ceiling on managed physical ranges

*/

/* !SPACE!

$space %export vm_phys_init, vm_phys_set_page_array
$space %export vm_phys_add_seg, vm_phys_exclude
$space %export vm_phys_free_range, vm_phys_alloc, vm_phys_free
$space %export vm_phys_alloc_contig, vm_phys_free_contig
$space %export vm_phys_paddr_to_page, vm_phys_page_to_paddr
$space %export vm_phys_managed, vm_phys_stats, vm_phys_dump

*/

#ifndef VM_PHYS_H
#define VM_PHYS_H

#include <mlibc/mlibc.h>
#include <mm/vm/vm_page.h>


#define VM_NFREEORDER		12
#define VM_FREEORDER_NONE	((u8)VM_NFREEORDER)
#define VM_PHYS_MAX_SEG		64

typedef struct vm_phys_seg {
	u64		start;
	u64		end;
	u64		first_page;
	u64		page_total;
} vm_phys_seg_t;

typedef struct vm_phys_stat {
	u64		page_total;
	u64		page_free;
	u64		seg_total;
	u64		order_blocks[VM_NFREEORDER];
} vm_phys_stat_t;

void		vm_phys_init(void);
void		vm_phys_set_page_array(vm_page_t *array, u64 count);
int		vm_phys_add_seg(u64 start, u64 end);
void		vm_phys_exclude(u64 start, u64 end);
void		vm_phys_free_range(u64 start, u64 end);
vm_page_t	*vm_phys_alloc(int order);
void		vm_phys_free(vm_page_t *page);
vm_page_t	*vm_phys_alloc_contig(u64 page_total, u64 alignment,
		    u64 low, u64 high);
void		vm_phys_free_contig(vm_page_t *page, u64 page_total);
vm_page_t	*vm_phys_paddr_to_page(u64 pa);
u64		vm_phys_page_to_paddr(vm_page_t *page);
int		vm_phys_managed(u64 pa);
void		vm_phys_stats(vm_phys_stat_t *out);
void		vm_phys_dump(void);

#endif
