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
$define %type vm_phys_seg_t as struct with start, end, first_page, page_total
$define %type vm_phys_stat_t as struct with the allocator counters

$define %func vm_phys_freelist_insert as procedure with args vm_page_t *, int
$define %func vm_phys_freelist_remove as procedure with args vm_page_t *
$define %func vm_phys_seg_of as function with args vm_page_t *
$define %func vm_phys_block_valid as function with args u64, int, u8
$define %func vm_phys_page_at as function with args vm_phys_seg_t *, u64
$define %func vm_phys_mark_alloc as procedure with args vm_page_t *, u64
$define %func vm_phys_split_to as procedure with args vm_page_t *, int, int
$define %func vm_phys_alloc_order as function with args int
$define %func vm_phys_free_block as procedure with args vm_page_t *, int
$define %func vm_phys_free_isolate as procedure with args vm_page_t *
$define %func vm_phys_init as procedure with args void
$define %func vm_phys_set_page_array as procedure with args vm_page_t *, u64
$define %func vm_phys_add_seg as function with args u64, u64
$define %func vm_phys_exclude as procedure with args u64, u64
$define %func vm_phys_free_range as procedure with args u64, u64
$define %func vm_phys_alloc as function with args int
$define %func vm_phys_free as procedure with args vm_page_t *
$define %func vm_phys_alloc_contig as function with args u64, u64, u64, u64
$define %func vm_phys_free_contig as procedure with args vm_page_t *, u64
$define %func vm_phys_paddr_to_page as function with args u64
$define %func vm_phys_page_to_paddr as function with args vm_page_t *
$define %func vm_phys_managed as function with args u64
$define %func vm_phys_stats as procedure with args vm_phys_stat_t *
$define %func vm_phys_dump as procedure with args void

*/

/* !SPACE!

$space %internal vm_phys_freelist_insert, vm_phys_freelist_remove
$space %internal vm_phys_seg_of, vm_phys_block_valid, vm_phys_page_at
$space %internal vm_phys_mark_alloc, vm_phys_split_to, vm_phys_alloc_order
$space %internal vm_phys_free_block, vm_phys_free_isolate
$space %export vm_phys_init, vm_phys_set_page_array
$space %export vm_phys_add_seg, vm_phys_exclude, vm_phys_free_range
$space %export vm_phys_alloc, vm_phys_free
$space %export vm_phys_alloc_contig, vm_phys_free_contig
$space %export vm_phys_paddr_to_page, vm_phys_page_to_paddr
$space %export vm_phys_managed, vm_phys_stats, vm_phys_dump

*/

#include <mm/vm/vm_phys.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>


static vm_page_t	*vm_phys_pages;
static u64		 vm_phys_page_total;
static vm_phys_seg_t	 vm_phys_segs[VM_PHYS_MAX_SEG];
static u32		 vm_phys_seg_count;
static vm_page_t	*vm_phys_freelists[VM_NFREEORDER];
static u64		 vm_phys_order_blocks[VM_NFREEORDER];
static u64		 vm_phys_free_pages;



static void
vm_phys_freelist_insert(vm_page_t *page, int order)
{
	vm_page_t	*head;

	head = vm_phys_freelists[order];
	page->plinks_prev = NULL;
	page->plinks_next = head;
	if (head != NULL) {
		head->plinks_prev = page;
	}
	vm_phys_freelists[order] = page;

	page->order = (u8)order;
	page->flags = (u16)((page->flags & ~PG_ALLOCATED) | PG_FREE);
	vm_phys_order_blocks[order]++;
}

static void
vm_phys_freelist_remove(vm_page_t *page)
{
	int	order;

	order = (int)page->order;
	if (order < 0 || order >= VM_NFREEORDER) {
		return;
	}

	if (page->plinks_prev != NULL) {
		page->plinks_prev->plinks_next = page->plinks_next;
	} else {
		vm_phys_freelists[order] = page->plinks_next;
	}
	if (page->plinks_next != NULL) {
		page->plinks_next->plinks_prev = page->plinks_prev;
	}

	page->plinks_next = NULL;
	page->plinks_prev = NULL;
	page->order = VM_FREEORDER_NONE;
	vm_phys_order_blocks[order]--;
}

static vm_phys_seg_t *
vm_phys_seg_of(vm_page_t *page)
{
	if (page->segind >= vm_phys_seg_count) {
		return (NULL);
	}
	return (&vm_phys_segs[page->segind]);
}


static int
vm_phys_block_valid(u64 pfn, int order, u8 segind)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*p;
	u64		 run, seg_pfn, idx, i;

	if (order < 0 || order >= VM_NFREEORDER ||
	    segind >= vm_phys_seg_count) {
		return (0);
	}
	run = (u64)1 << order;
	if ((pfn & (run - 1)) != 0) {
		return (0);
	}

	seg = &vm_phys_segs[segind];
	seg_pfn = seg->start >> PAGE_SHIFT;
	if (pfn < seg_pfn || pfn + run > seg_pfn + seg->page_total) {
		return (0);
	}

	idx = seg->first_page + (pfn - seg_pfn);
	for (i = 0; i < run; i++) {
		p = &vm_phys_pages[idx + i];
		if ((p->flags & PG_FREE) == 0 ||
		    (p->flags & PG_EXCLUDED) != 0) {
			return (0);
		}
		if (i != 0 && p->order != VM_FREEORDER_NONE) {
			return (0);
		}
	}
	return (1);
}

static vm_page_t *
vm_phys_page_at(vm_phys_seg_t *seg, u64 pfn)
{
	u64	seg_pfn;

	seg_pfn = seg->start >> PAGE_SHIFT;
	return (&vm_phys_pages[seg->first_page + (pfn - seg_pfn)]);
}

static void
vm_phys_mark_alloc(vm_page_t *page, u64 run)
{
	u64	i;

	for (i = 0; i < run; i++) {
		page[i].flags = (u16)((page[i].flags &
		    ~(PG_FREE | PG_ZERO)) | PG_ALLOCATED);
		page[i].order = VM_FREEORDER_NONE;
		page[i].plinks_next = NULL;
		page[i].plinks_prev = NULL;
	}
}

static void
vm_phys_split_to(vm_page_t *page, int from, int to)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*buddy;
	u64		 pfn;
	int		 order;

	seg = vm_phys_seg_of(page);
	if (seg == NULL) {
		return;
	}
	pfn = page->phys_addr >> PAGE_SHIFT;

	for (order = from; order > to; order--) {
		buddy = vm_phys_page_at(seg, pfn + ((u64)1 << (order - 1)));
		buddy->order = VM_FREEORDER_NONE;
		buddy->flags = (u16)(buddy->flags | PG_FREE);
		vm_phys_freelist_insert(buddy, order - 1);
	}
}

static vm_page_t *
vm_phys_alloc_order(int order)
{
	vm_page_t	*page;
	int		 k;

	if (order < 0 || order >= VM_NFREEORDER) {
		return (NULL);
	}

	for (k = order; k < VM_NFREEORDER; k++) {
		page = vm_phys_freelists[k];
		if (page == NULL) {
			continue;
		}
		vm_phys_freelist_remove(page);
		if (k > order) {
			vm_phys_split_to(page, k, order);
		}
		vm_phys_free_pages -= (u64)1 << order;
		vm_phys_mark_alloc(page, (u64)1 << order);
		return (page);
	}
	return (NULL);
}

static void
vm_phys_free_block(vm_page_t *page, int order)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*buddy;
	u64		 pfn, buddy_pfn;

	seg = vm_phys_seg_of(page);
	if (seg == NULL || order < 0 || order >= VM_NFREEORDER) {
		return;
	}

	pfn = page->phys_addr >> PAGE_SHIFT;
	vm_phys_free_pages += (u64)1 << order;

	{
		u64	i, run;

		run = (u64)1 << order;
		for (i = 0; i < run; i++) {
			buddy = vm_phys_page_at(seg, pfn + i);
			buddy->flags = (u16)((buddy->flags & ~PG_ALLOCATED) |
			    PG_FREE);
			buddy->order = VM_FREEORDER_NONE;
			buddy->object = NULL;
			buddy->pindex = 0;
		}
	}

	while (order < VM_NFREEORDER - 1) {
		buddy_pfn = pfn ^ ((u64)1 << order);
		if (!vm_phys_block_valid(buddy_pfn, order, page->segind)) {
			break;
		}
		buddy = vm_phys_page_at(seg, buddy_pfn);
		if (buddy->order != (u8)order) {
			break;
		}

		vm_phys_freelist_remove(buddy);
		if (buddy_pfn < pfn) {
			page = buddy;
			pfn = buddy_pfn;
		}
		order++;
	}

	vm_phys_freelist_insert(page, order);
}


static void
vm_phys_free_isolate(vm_page_t *page)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*head, *other;
	u64		 target, head_pfn, cur, half;
	int		 k, order;

	seg = vm_phys_seg_of(page);
	if (seg == NULL || (page->flags & PG_FREE) == 0) {
		return;
	}
	target = page->phys_addr >> PAGE_SHIFT;

	head = NULL;
	order = 0;
	if (page->order != VM_FREEORDER_NONE) {
		head = page;
		order = (int)page->order;
		head_pfn = target;
	} else {
		head_pfn = target;
		for (k = 1; k < VM_NFREEORDER; k++) {
			head_pfn = target & ~(((u64)1 << k) - 1);
			other = vm_phys_page_at(seg, head_pfn);
			if (other->order == (u8)k) {
				head = other;
				order = k;
				break;
			}
		}
	}
	if (head == NULL) {
		return;
	}

	vm_phys_freelist_remove(head);

	cur = head_pfn;
	while (order > 0) {
		half = (u64)1 << (order - 1);
		if (target >= cur + half) {
			other = vm_phys_page_at(seg, cur);
			cur += half;
		} else {
			other = vm_phys_page_at(seg, cur + half);
		}
		other->order = VM_FREEORDER_NONE;
		other->flags = (u16)(other->flags | PG_FREE);
		vm_phys_freelist_insert(other, order - 1);
		order--;
	}

	vm_phys_mark_alloc(page, 1);
	page->flags = (u16)((page->flags & ~PG_ALLOCATED) | PG_EXCLUDED);
	if (vm_phys_free_pages > 0) {
		vm_phys_free_pages--;
	}
}

void
vm_phys_init(void)
{
	int	i;

	vm_phys_pages = NULL;
	vm_phys_page_total = 0;
	vm_phys_seg_count = 0;
	vm_phys_free_pages = 0;
	for (i = 0; i < VM_NFREEORDER; i++) {
		vm_phys_freelists[i] = NULL;
		vm_phys_order_blocks[i] = 0;
	}
}

void
vm_phys_set_page_array(vm_page_t *array, u64 count)
{
	vm_phys_pages = array;
	vm_phys_page_total = count;
}


int
vm_phys_add_seg(u64 start, u64 end)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*p;
	u64		 pages, i;

	start = (start + PAGE_MASK) & ~PAGE_MASK;
	end &= ~PAGE_MASK;
	if (end <= start || vm_phys_seg_count >= VM_PHYS_MAX_SEG) {
		return (-1);
	}
	if (vm_phys_seg_count > 0 &&
	    start < vm_phys_segs[vm_phys_seg_count - 1].end) {
		return (-1);
	}

	pages = (end - start) >> PAGE_SHIFT;
	if (vm_phys_page_total != 0 &&
	    vm_phys_seg_count > 0 &&
	    vm_phys_segs[vm_phys_seg_count - 1].first_page +
	    vm_phys_segs[vm_phys_seg_count - 1].page_total + pages >
	    vm_phys_page_total) {
		return (-1);
	}

	seg = &vm_phys_segs[vm_phys_seg_count];
	seg->start = start;
	seg->end = end;
	seg->page_total = pages;
	seg->first_page = 0;
	if (vm_phys_seg_count > 0) {
		seg->first_page = vm_phys_segs[vm_phys_seg_count - 1].first_page
		    + vm_phys_segs[vm_phys_seg_count - 1].page_total;
	}

	for (i = 0; i < pages; i++) {
		p = &vm_phys_pages[seg->first_page + i];
		p->phys_addr = start + (i << PAGE_SHIFT);
		p->object = NULL;
		p->pindex = 0;
		p->plinks_next = NULL;
		p->plinks_prev = NULL;
		p->qlinks_next = NULL;
		p->qlinks_prev = NULL;
		p->ref_count = 0;
		p->wire_count = 0;
		p->flags = PG_EXCLUDED;
		p->order = VM_FREEORDER_NONE;
		p->segind = (u8)vm_phys_seg_count;
		p->queue = PQ_NONE;
	}

	vm_phys_seg_count++;
	return (0);
}

void
vm_phys_exclude(u64 start, u64 end)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*p;
	u64		 addr, lo, hi;
	u32		 s;

	start &= ~PAGE_MASK;
	end = (end + PAGE_MASK) & ~PAGE_MASK;
	if (end <= start) {
		return;
	}

	for (s = 0; s < vm_phys_seg_count; s++) {
		seg = &vm_phys_segs[s];
		lo = start > seg->start ? start : seg->start;
		hi = end < seg->end ? end : seg->end;
		if (hi <= lo) {
			continue;
		}
		for (addr = lo; addr < hi; addr += PAGE_SIZE) {
			p = vm_phys_page_at(seg, addr >> PAGE_SHIFT);
			if ((p->flags & PG_FREE) != 0) {
				vm_phys_free_isolate(p);
			}
			p->flags = PG_EXCLUDED;
			p->order = VM_FREEORDER_NONE;
			p->ref_count = 0;
			p->wire_count = 0;
		}
	}
}

void
vm_phys_free_range(u64 start, u64 end)
{
	vm_phys_seg_t	*seg;
	vm_page_t	*p;
	u64		 lo, hi, pfn, end_pfn, run, i;
	u32		 s;
	int		 order;

	start &= ~PAGE_MASK;
	end &= ~PAGE_MASK;
	if (end <= start) {
		return;
	}

	for (s = 0; s < vm_phys_seg_count; s++) {
		seg = &vm_phys_segs[s];
		lo = start > seg->start ? start : seg->start;
		hi = end < seg->end ? end : seg->end;
		if (hi <= lo) {
			continue;
		}

		pfn = lo >> PAGE_SHIFT;
		end_pfn = hi >> PAGE_SHIFT;
		while (pfn < end_pfn) {
			order = 0;
			while (order + 1 < VM_NFREEORDER) {
				run = (u64)1 << (order + 1);
				if ((pfn & (run - 1)) != 0 ||
				    pfn + run > end_pfn) {
					break;
				}
				order++;
			}

			run = (u64)1 << order;
			for (i = 0; i < run; i++) {
				p = vm_phys_page_at(seg, pfn + i);
				p->flags = (u16)(p->flags & ~PG_EXCLUDED);
			}

			p = vm_phys_page_at(seg, pfn);
			vm_phys_free_block(p, order);
			pfn += run;
		}
	}
}

vm_page_t *
vm_phys_alloc(int order)
{
	return (vm_phys_alloc_order(order));
}

void
vm_phys_free(vm_page_t *page)
{
	if (page == NULL || (page->flags & PG_EXCLUDED) != 0) {
		return;
	}
	if ((page->flags & PG_FREE) != 0) {
		return;
	}
	vm_phys_free_block(page, 0);
}

vm_page_t *
vm_phys_alloc_contig(u64 page_total, u64 alignment, u64 low, u64 high)
{
	vm_page_t	*page, *next;
	u64		 base, run, i;
	int		 order, k;

	if (page_total == 0 || alignment == 0 ||
	    (alignment & (alignment - 1)) != 0) {
		return (NULL);
	}

	order = 0;
	while (((u64)1 << order) < page_total) {
		if (++order >= VM_NFREEORDER) {
			return (NULL);
		}
	}

	for (k = order; k < VM_NFREEORDER; k++) {
		for (page = vm_phys_freelists[k]; page != NULL; page = next) {
			next = page->plinks_next;
			base = page->phys_addr;
			run = page_total << PAGE_SHIFT;

			if ((base & (alignment - 1)) != 0) {
				continue;
			}
			if (base < low || base + run - 1 > high) {
				continue;
			}

			vm_phys_freelist_remove(page);
			if (k > order) {
				vm_phys_split_to(page, k, order);
			}
			vm_phys_free_pages -= (u64)1 << order;
			vm_phys_mark_alloc(page, (u64)1 << order);

			for (i = page_total; i < ((u64)1 << order); i++) {
				vm_phys_free_block(&page[i], 0);
			}
			return (page);
		}
	}
	return (NULL);
}

void
vm_phys_free_contig(vm_page_t *page, u64 page_total)
{
	u64	i;

	if (page == NULL) {
		return;
	}
	for (i = 0; i < page_total; i++) {
		if ((page[i].flags & (PG_FREE | PG_EXCLUDED)) != 0) {
			continue;
		}
		vm_phys_free_block(&page[i], 0);
	}
}


vm_page_t *
vm_phys_paddr_to_page(u64 pa)
{
	vm_phys_seg_t	*seg;
	u32		 s;

	pa &= ~PAGE_MASK;
	for (s = 0; s < vm_phys_seg_count; s++) {
		seg = &vm_phys_segs[s];
		if (pa < seg->start) {
			return (NULL);
		}
		if (pa < seg->end) {
			return (vm_phys_page_at(seg, pa >> PAGE_SHIFT));
		}
	}
	return (NULL);
}

u64
vm_phys_page_to_paddr(vm_page_t *page)
{
	if (page == NULL) {
		return (0);
	}
	return (page->phys_addr);
}

int
vm_phys_managed(u64 pa)
{
	return (vm_phys_paddr_to_page(pa) != NULL);
}

void
vm_phys_stats(vm_phys_stat_t *out)
{
	u64	total;
	u32	s;
	int	i;

	if (out == NULL) {
		return;
	}
	total = 0;
	for (s = 0; s < vm_phys_seg_count; s++) {
		total += vm_phys_segs[s].page_total;
	}
	out->page_total = total;
	out->page_free = vm_phys_free_pages;
	out->seg_total = vm_phys_seg_count;
	for (i = 0; i < VM_NFREEORDER; i++) {
		out->order_blocks[i] = vm_phys_order_blocks[i];
	}
}

void
vm_phys_dump(void)
{
	vm_phys_stat_t	st;
	u32		s;
	int		i;

	vm_phys_stats(&st);
	printk("--- vm_phys ---\n");
	printk("segments   : %u\n", (u32)st.seg_total);
	for (s = 0; s < vm_phys_seg_count; s++) {
		printk("  seg%-2u    : %p..%p  %u pages\n", s,
		    (void *)vm_phys_segs[s].start,
		    (void *)vm_phys_segs[s].end,
		    (u32)vm_phys_segs[s].page_total);
	}
	printk("pages      : %u total, %u free\n", (u32)st.page_total,
	    (u32)st.page_free);
	for (i = 0; i < VM_NFREEORDER; i++) {
		if (st.order_blocks[i] == 0) {
			continue;
		}
		printk("  order %-2d : %u blocks (%u KiB each)\n", i,
		    (u32)st.order_blocks[i],
		    (u32)((PAGE_SIZE << i) / 1024));
	}
}
