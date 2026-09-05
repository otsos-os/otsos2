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
$define %type vm_page_t as struct with the per-physical-page state
$define %type bootmem_range_t as struct with start, end
$define %type spin_t as spin lock

$define %func vm_page_queue_insert as procedure with args vm_page_t *, u8
$define %func vm_page_queue_remove as procedure with args vm_page_t *
$define %func vm_page_zero as procedure with args vm_page_t *, u64
$define %func vm_page_alloc_locked as function with args u32, int
$define %func vm_page_free_locked as procedure with args vm_page_t *
$define %func vm_page_reserve_cb as procedure with args u64, u64
$define %func vm_page_startup as procedure with args void
$define %func vm_page_alloc as function with args u32
$define %func vm_page_free as procedure with args vm_page_t *
$define %func vm_page_alloc_phys as function with args u32
$define %func vm_page_alloc_contig as function with args u32, u64, u64, u64
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

*/

/* !SPACE!

$space %internal vm_page_queue_insert, vm_page_queue_remove, vm_page_zero
$space %internal vm_page_alloc_locked, vm_page_free_locked
$space %internal vm_page_reserve_cb, vm_page_spin
$space %export vm_page_startup, vm_page_alloc, vm_page_free
$space %export vm_page_alloc_phys, vm_page_alloc_contig
$space %export vm_page_free_phys, vm_page_free_contig
$space %export vm_page_reserve_range
$space %export vm_page_ref, vm_page_unref
$space %export vm_page_ref_phys, vm_page_ref_count
$space %export vm_page_wire, vm_page_unwire
$space %export vm_page_activate, vm_page_deactivate, vm_page_dequeue
$space %export vm_page_count_free, vm_page_count_total
$space %export vm_page_queue_count, vm_page_queue_counts
$space %export vm_page_lookup_phys, vm_page_dump

*/

#include <mm/vm/vm_page.h>
#include <mm/vm/vm_phys.h>
#include <kernel/bootmem.h>
#include <kernel/sync/sync.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

static spin_t	vm_page_spin = SPIN_INITIALIZER("vm_page", LO_VM_PAGE);

static vm_page_t	*queue_head[PQ_COUNT];
static vm_page_t	*queue_tail[PQ_COUNT];
static u64		 queue_count[PQ_COUNT];
static u64		 vm_page_wired;
static int		 vm_page_ready;

static void
vm_page_queue_insert(vm_page_t *page, u8 qid)
{
	if (qid == PQ_NONE || qid >= PQ_COUNT) {
		return;
	}

	page->queue = qid;
	page->qlinks_next = NULL;
	page->qlinks_prev = queue_tail[qid];
	if (queue_tail[qid] != NULL) {
		queue_tail[qid]->qlinks_next = page;
	} else {
		queue_head[qid] = page;
	}
	queue_tail[qid] = page;
	queue_count[qid]++;
}

static void
vm_page_queue_remove(vm_page_t *page)
{
	u8	qid;

	qid = page->queue;
	if (qid == PQ_NONE || qid >= PQ_COUNT) {
		return;
	}

	if (page->qlinks_prev != NULL) {
		page->qlinks_prev->qlinks_next = page->qlinks_next;
	} else {
		queue_head[qid] = page->qlinks_next;
	}
	if (page->qlinks_next != NULL) {
		page->qlinks_next->qlinks_prev = page->qlinks_prev;
	} else {
		queue_tail[qid] = page->qlinks_prev;
	}

	page->qlinks_next = NULL;
	page->qlinks_prev = NULL;
	page->queue = PQ_NONE;
	if (queue_count[qid] > 0) {
		queue_count[qid]--;
	}
}

static void
vm_page_zero(vm_page_t *page, u64 run)
{
	u64	i;

	for (i = 0; i < run; i++) {
		memset((void *)(page[i].phys_addr + DMAP_BASE), 0, PAGE_SIZE);
		page[i].flags = (u16)(page[i].flags | PG_ZERO);
	}
}

static vm_page_t *
vm_page_alloc_locked(u32 flags, int order)
{
	vm_page_t	*page;
	u64		 run, i;

	page = vm_phys_alloc(order);
	if (page == NULL) {
		return (NULL);
	}

	run = (u64)1 << order;
	for (i = 0; i < run; i++) {
		page[i].ref_count = 1;
		page[i].wire_count = 0;
		page[i].object = NULL;
		page[i].pindex = 0;
		page[i].queue = PQ_NONE;
		page[i].qlinks_next = NULL;
		page[i].qlinks_prev = NULL;
	}

	if ((flags & VM_ALLOC_ZERO) != 0) {
		vm_page_zero(page, run);
	}

	if ((flags & VM_ALLOC_WIRED) != 0) {
		for (i = 0; i < run; i++) {
			page[i].wire_count = 1;
			page[i].flags = (u16)(page[i].flags | PG_WIRED);
		}
		vm_page_wired += run;
	} else {
		for (i = 0; i < run; i++) {
			vm_page_queue_insert(&page[i], PQ_ACTIVE);
		}
	}

	return (page);
}

static void
vm_page_free_locked(vm_page_t *page)
{
	if ((page->flags & PG_WIRED) != 0) {
		page->flags = (u16)(page->flags & ~PG_WIRED);
		page->wire_count = 0;
		if (vm_page_wired > 0) {
			vm_page_wired--;
		}
	}
	vm_page_queue_remove(page);

	page->ref_count = 0;
	page->object = NULL;
	page->pindex = 0;
	vm_phys_free(page);
}


static void
vm_page_reserve_cb(u64 phys_start, u64 size)
{
	if (!vm_page_ready || size == 0) {
		return;
	}

	spin_lock(&vm_page_spin);
	vm_phys_exclude(phys_start, phys_start + size);
	spin_unlock(&vm_page_spin);
}


void
vm_page_startup(void)
{
	bootmem_range_t	 sorted[VM_PHYS_MAX_SEG];
	const bootmem_range_t *ranges;
	vm_page_t	*array;
	u64		 page_total, bytes, i, j;
	u32		 count, n, s;

	ranges = bootmem_ranges();
	count = bootmem_range_count();
	if (count > VM_PHYS_MAX_SEG) {
		count = VM_PHYS_MAX_SEG;
	}

	page_total = 0;
	for (s = 0; s < count; s++) {
		if (ranges[s].end > ranges[s].start) {
			page_total += (ranges[s].end - ranges[s].start) >>
			    PAGE_SHIFT;
		}
	}
	if (page_total == 0) {
		printk("vm_page: no usable memory reported by bootmem\n");
		return;
	}

	bytes = page_total * sizeof(vm_page_t);
	array = (vm_page_t *)bootmem_alloc(bytes, PAGE_SIZE);
	if (array == NULL) {
		printk("vm_page: cannot allocate %u KiB page array\n",
		    (u32)(bytes / 1024));
		return;
	}
	memset(array, 0, bytes);

	ranges = bootmem_ranges();
	count = bootmem_range_count();
	n = 0;
	for (s = 0; s < count && n < VM_PHYS_MAX_SEG; s++) {
		if (ranges[s].end <= ranges[s].start) {
			continue;
		}
		sorted[n] = ranges[s];
		n++;
	}
	for (i = 1; i < n; i++) {
		bootmem_range_t key = sorted[i];

		j = i;
		while (j > 0 && sorted[j - 1].start > key.start) {
			sorted[j] = sorted[j - 1];
			j--;
		}
		sorted[j] = key;
	}

	vm_phys_init();
	vm_phys_set_page_array(array, page_total);

	for (s = 0; s < n; s++) {
		if (vm_phys_add_seg(sorted[s].start, sorted[s].end) != 0) {
			printk("vm_page: dropped segment %p..%p\n",
			    (void *)sorted[s].start, (void *)sorted[s].end);
		}
	}
	for (s = 0; s < n; s++) {
		vm_phys_free_range(sorted[s].start, sorted[s].end);
	}

	vm_page_ready = 1;
	bootmem_set_reserve_cb(vm_page_reserve_cb);

	printk("vm_page: %u pages managed in %u segments (%u KiB of page "
	    "array)\n", (u32)vm_page_count_total(), n, (u32)(bytes / 1024));
}

vm_page_t *
vm_page_alloc(u32 flags)
{
	vm_page_t	*page;

	spin_lock(&vm_page_spin);
	page = vm_page_alloc_locked(flags, 0);
	spin_unlock(&vm_page_spin);
	return (page);
}

void
vm_page_free(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}
	spin_lock(&vm_page_spin);
	vm_page_free_locked(page);
	spin_unlock(&vm_page_spin);
}

u64
vm_page_alloc_phys(u32 flags)
{
	vm_page_t	*page;
	u64		 pa;

	spin_lock(&vm_page_spin);
	page = vm_page_alloc_locked(flags, 0);
	pa = page != NULL ? page->phys_addr : 0;
	spin_unlock(&vm_page_spin);
	return (pa);
}

u64
vm_page_alloc_contig(u32 page_total, u64 alignment, u64 low, u64 high)
{
	vm_page_t	*page;
	u64		 i, pa;

	if (page_total == 0) {
		return (0);
	}
	if (alignment < PAGE_SIZE) {
		alignment = PAGE_SIZE;
	}
	if (high == 0) {
		high = 0xFFFFFFFFFFFFFFFFULL;
	}
	if (low > high) {
		return (0);
	}

	spin_lock(&vm_page_spin);
	page = vm_phys_alloc_contig(page_total, alignment, low, high);
	if (page == NULL) {
		spin_unlock(&vm_page_spin);
		return (0);
	}

	for (i = 0; i < page_total; i++) {
		page[i].ref_count = 1;
		page[i].wire_count = 1;
		page[i].object = NULL;
		page[i].pindex = 0;
		page[i].queue = PQ_NONE;
		page[i].qlinks_next = NULL;
		page[i].qlinks_prev = NULL;
		page[i].flags = (u16)(page[i].flags | PG_WIRED);
	}
	vm_page_wired += page_total;
	pa = page->phys_addr;
	spin_unlock(&vm_page_spin);
	return (pa);
}

int
vm_page_free_phys(u64 phys_addr)
{
	vm_page_t	*page;
	int		 freed;

	freed = 0;
	spin_lock(&vm_page_spin);
	page = vm_phys_paddr_to_page(phys_addr);
	if (page != NULL && (page->flags & PG_ALLOCATED) != 0) {
		if (page->ref_count > 1) {
			page->ref_count--;
		} else {
			vm_page_free_locked(page);
			freed = 1;
		}
	}
	spin_unlock(&vm_page_spin);
	return (freed);
}

void
vm_page_free_contig(u64 phys_addr, u32 page_total)
{
	vm_page_t	*page;
	u64		 i;

	if (page_total == 0) {
		return;
	}

	spin_lock(&vm_page_spin);
	page = vm_phys_paddr_to_page(phys_addr);
	if (page == NULL) {
		spin_unlock(&vm_page_spin);
		return;
	}
	for (i = 0; i < page_total; i++) {
		if ((page[i].flags & PG_ALLOCATED) == 0) {
			continue;
		}
		if ((page[i].flags & PG_WIRED) != 0) {
			page[i].flags = (u16)(page[i].flags & ~PG_WIRED);
			page[i].wire_count = 0;
			if (vm_page_wired > 0) {
				vm_page_wired--;
			}
		}
		vm_page_queue_remove(&page[i]);
		page[i].ref_count = 0;
		page[i].object = NULL;
		page[i].pindex = 0;
	}
	vm_phys_free_contig(page, page_total);
	spin_unlock(&vm_page_spin);
}

void
vm_page_reserve_range(u64 phys_start, u64 size)
{
	if (size == 0) {
		return;
	}
	spin_lock(&vm_page_spin);
	vm_phys_exclude(phys_start, phys_start + size);
	spin_unlock(&vm_page_spin);
}

void
vm_page_ref(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}
	spin_lock(&vm_page_spin);
	page->ref_count++;
	spin_unlock(&vm_page_spin);
}

int
vm_page_unref(vm_page_t *page)
{
	int	freed;

	if (page == NULL) {
		return (0);
	}

	freed = 0;
	spin_lock(&vm_page_spin);
	if (page->ref_count > 1) {
		page->ref_count--;
	} else if (page->ref_count == 1) {
		vm_page_free_locked(page);
		freed = 1;
	}
	spin_unlock(&vm_page_spin);
	return (freed);
}

void
vm_page_ref_phys(u64 phys_addr)
{
	vm_page_t	*page;

	spin_lock(&vm_page_spin);
	page = vm_phys_paddr_to_page(phys_addr);
	if (page != NULL && (page->flags & PG_ALLOCATED) != 0) {
		page->ref_count++;
	}
	spin_unlock(&vm_page_spin);
}

u32
vm_page_ref_count(u64 phys_addr)
{
	vm_page_t	*page;
	u32		 ref;

	spin_lock(&vm_page_spin);
	page = vm_phys_paddr_to_page(phys_addr);
	ref = page != NULL ? page->ref_count : 0;
	spin_unlock(&vm_page_spin);
	return (ref);
}

void
vm_page_wire(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}

	spin_lock(&vm_page_spin);
	if (page->wire_count == 0) {
		vm_page_queue_remove(page);
		page->flags = (u16)(page->flags | PG_WIRED);
		vm_page_wired++;
	}
	page->wire_count++;
	spin_unlock(&vm_page_spin);
}

void
vm_page_unwire(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}

	spin_lock(&vm_page_spin);
	if (page->wire_count > 0) {
		page->wire_count--;
	}
	if (page->wire_count == 0 && (page->flags & PG_WIRED) != 0) {
		page->flags = (u16)(page->flags & ~PG_WIRED);
		if (vm_page_wired > 0) {
			vm_page_wired--;
		}
		vm_page_queue_insert(page, PQ_INACTIVE);
	}
	spin_unlock(&vm_page_spin);
}

void
vm_page_activate(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}
	spin_lock(&vm_page_spin);
	if ((page->flags & PG_WIRED) == 0) {
		vm_page_queue_remove(page);
		vm_page_queue_insert(page, PQ_ACTIVE);
	}
	spin_unlock(&vm_page_spin);
}

void
vm_page_deactivate(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}
	spin_lock(&vm_page_spin);
	if ((page->flags & PG_WIRED) == 0) {
		vm_page_queue_remove(page);
		vm_page_queue_insert(page, PQ_INACTIVE);
	}
	spin_unlock(&vm_page_spin);
}

void
vm_page_dequeue(vm_page_t *page)
{
	if (page == NULL) {
		return;
	}
	spin_lock(&vm_page_spin);
	vm_page_queue_remove(page);
	spin_unlock(&vm_page_spin);
}

u64
vm_page_count_free(void)
{
	vm_phys_stat_t	st;

	spin_lock(&vm_page_spin);
	vm_phys_stats(&st);
	spin_unlock(&vm_page_spin);
	return (st.page_free);
}

u64
vm_page_count_total(void)
{
	vm_phys_stat_t	st;

	spin_lock(&vm_page_spin);
	vm_phys_stats(&st);
	spin_unlock(&vm_page_spin);
	return (st.page_total);
}

u64
vm_page_queue_count(int qid)
{
	u64	n;

	if (qid < 0 || qid >= PQ_COUNT) {
		return (0);
	}
	spin_lock(&vm_page_spin);
	n = queue_count[qid];
	spin_unlock(&vm_page_spin);
	return (n);
}

void
vm_page_queue_counts(u64 *active, u64 *inactive, u64 *cache, u64 *wired)
{
	spin_lock(&vm_page_spin);
	if (active != NULL) {
		*active = queue_count[PQ_ACTIVE];
	}
	if (inactive != NULL) {
		*inactive = queue_count[PQ_INACTIVE];
	}
	if (cache != NULL) {
		*cache = queue_count[PQ_LAUNDRY];
	}
	if (wired != NULL) {
		*wired = vm_page_wired;
	}
	spin_unlock(&vm_page_spin);
}

vm_page_t *
vm_page_lookup_phys(u64 phys_addr)
{
	vm_page_t	*page;

	spin_lock(&vm_page_spin);
	page = vm_phys_paddr_to_page(phys_addr);
	spin_unlock(&vm_page_spin);
	return (page);
}

void
vm_page_dump(void)
{
	u64	act, inact, laundry, wired;

	vm_page_queue_counts(&act, &inact, &laundry, &wired);

	spin_lock(&vm_page_spin);
	vm_phys_dump();
	spin_unlock(&vm_page_spin);

	printk("queues     : active %u, inactive %u, laundry %u, wired %u\n",
	    (u32)act, (u32)inact, (u32)laundry, (u32)wired);
}
