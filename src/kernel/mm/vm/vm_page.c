/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
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
$define %type char as 8 bit signed
$define %type vm_page_t as struct with phys_addr, state, ref_count, queue, queue_next, queue_prev
$define %type bootmem_range_t as struct with start, end

$define %func round_page as function with args u64
$define %func trunc_page as function with args u64
$define %func atop as function with args u64
$define %func vm_page_queue_insert as procedure with args vm_page_t *, u8
$define %func vm_page_queue_remove as procedure with args vm_page_t *
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
$define %func vm_page_lookup_locked as function with args u64
$define %func vm_page_alloc_locked as function with args u32
$define %func vm_page_free_locked as procedure with args vm_page_t *
$define %func vm_page_reserve_locked as procedure with args u64, u64
$define %func vm_page_queue_count_locked as function with args int

*/

/* !SPACE!

$space %internal round_page, trunc_page, atop
$space %internal vm_page_queue_insert, vm_page_queue_remove
$space %internal vm_page_lookup_locked, vm_page_alloc_locked
$space %internal vm_page_free_locked, vm_page_reserve_locked
$space %internal vm_page_queue_count_locked, vm_page_spin
$space %export vm_page_init, vm_page_init_from_bootmem
$space %export vm_page_alloc, vm_page_free, vm_page_alloc_phys
$space %export vm_page_alloc_contig, vm_page_free_phys
$space %export vm_page_free_contig, vm_page_ref, vm_page_unref
$space %export vm_page_ref_phys, vm_page_ref_count
$space %export vm_page_activate, vm_page_deactivate, vm_page_cache_insert
$space %export vm_page_count_free, vm_page_count_total
$space %export vm_page_queue_count, vm_page_queue_counts
$space %export vm_page_lookup, vm_page_dump
$space %export vm_page_reserve_range

*/

#include <mm/vm/vm_page.h>
#include <kernel/bootmem.h>
#include <kernel/sync/sync.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define PAGE_SIZE 4096
#define VM_PAGE_MAX 4096

static vm_page_t bootstrap_pages[VM_PAGE_MAX];
static vm_page_t *pages = bootstrap_pages;
static u64 page_capacity = VM_PAGE_MAX;
static u64 page_count;

static vm_page_t *queue_head[PQ_COUNT];
static vm_page_t *queue_tail[PQ_COUNT];

static spin_t vm_page_spin = SPIN_INITIALIZER("vm_page", LO_VM_PAGE);

static u64
round_page(u64 value)
{
	return (value & ~((u64)(PAGE_SIZE - 1)));
}

static u64
trunc_page(u64 value)
{
	return (value & ~((u64)(PAGE_SIZE - 1)));
}

static u64
atop(u64 bytes)
{
	return (bytes / PAGE_SIZE);
}

static void
vm_page_queue_insert(vm_page_t *page, u8 qid)
{
	page->queue = qid;
	page->queue_next = NULL;
	page->queue_prev = queue_tail[qid];
	if (queue_tail[qid] != NULL) {
		queue_tail[qid]->queue_next = page;
	} else {
		queue_head[qid] = page;
	}
	queue_tail[qid] = page;
}

static void
vm_page_queue_remove(vm_page_t *page)
{
	u8 qid;

	qid = page->queue;
	if (qid == PQ_NONE || qid >= PQ_COUNT)
		return;

	if (page->queue_prev != NULL) {
		page->queue_prev->queue_next = page->queue_next;
	} else {
		queue_head[qid] = page->queue_next;
	}

	if (page->queue_next != NULL) {
		page->queue_next->queue_prev = page->queue_prev;
	} else {
		queue_tail[qid] = page->queue_prev;
	}

	page->queue_next = NULL;
	page->queue_prev = NULL;
	page->queue = PQ_NONE;
}

void
vm_page_init(u64 available_start, u64 available_end)
{
	u64 start, end, addr, i;

	pages = bootstrap_pages;
	page_capacity = VM_PAGE_MAX;
	memset(pages, 0, sizeof(bootstrap_pages));

	for (i = 0; i < PQ_COUNT; i++) {
		queue_head[i] = NULL;
		queue_tail[i] = NULL;
	}

	start = round_page(available_start + PAGE_SIZE - 1);
	end = trunc_page(available_end);

	page_count = 0;

	for (i = 0; i < page_capacity; i++) {
		pages[i].phys_addr = 0;
		pages[i].state = VM_PAGE_RESERVED;
		pages[i].ref_count = 0;
		pages[i].queue = PQ_NONE;
		pages[i].queue_next = NULL;
		pages[i].queue_prev = NULL;
	}

	addr = start;
	for (i = 0; i < page_capacity && addr < end; i++) {
		pages[i].phys_addr = addr;
		pages[i].state = VM_PAGE_FREE;
		pages[i].ref_count = 0;
		vm_page_queue_insert(&pages[i], PQ_FREE);
		addr += PAGE_SIZE;
		page_count++;
	}
}

void
vm_page_init_from_bootmem(void)
{
	const bootmem_range_t *ranges;
	u64 metadata_bytes, managed_pages;
	u64 i, start, end, addr;
	u32 range_count, r;

	ranges = bootmem_ranges();
	range_count = bootmem_range_count();
	managed_pages = 0;

	for (r = 0; r < range_count; r++) {
		start = (ranges[r].start + PAGE_SIZE - 1) &
		    ~((u64)(PAGE_SIZE - 1));
		end = ranges[r].end & ~((u64)(PAGE_SIZE - 1));
		if (end > start)
			managed_pages += atop(end - start);
	}

	metadata_bytes = managed_pages * sizeof(vm_page_t);
	pages = NULL;
	if (metadata_bytes != 0)
		pages = (vm_page_t *)bootmem_alloc(metadata_bytes,
		    PAGE_SIZE);
	if (pages == NULL) {
		pages = bootstrap_pages;
		page_capacity = VM_PAGE_MAX;
		metadata_bytes = sizeof(bootstrap_pages);
		printk("[VM_PAGE] metadata bootmem "
		    "allocation failed, using %u pages\n",
		    (u32)page_capacity);
	} else {
		page_capacity = managed_pages;
	}

	memset(pages, 0, metadata_bytes);
	for (i = 0; i < PQ_COUNT; i++) {
		queue_head[i] = NULL;
		queue_tail[i] = NULL;
	}
	for (i = 0; i < page_capacity; i++) {
		pages[i].phys_addr = 0;
		pages[i].state = VM_PAGE_RESERVED;
		pages[i].ref_count = 0;
		pages[i].queue = PQ_NONE;
		pages[i].queue_next = NULL;
		pages[i].queue_prev = NULL;
	}

	page_count = 0;
	ranges = bootmem_ranges();
	range_count = bootmem_range_count();

	for (r = 0; r < range_count; r++) {
		addr = (ranges[r].start + PAGE_SIZE - 1) &
		    ~((u64)(PAGE_SIZE - 1));
		end = ranges[r].end & ~((u64)(PAGE_SIZE - 1));

		while (addr < end && page_count < page_capacity) {
			pages[page_count].phys_addr = addr;
			pages[page_count].state = VM_PAGE_FREE;
			pages[page_count].ref_count = 0;
			vm_page_queue_insert(&pages[page_count],
			    PQ_FREE);
			page_count++;
			addr += PAGE_SIZE;
		}
	}

	printk("[VM_PAGE] initialized: %u pages from "
	    "bootmem (%u KB metadata)\n",
	    (u32)page_count, (u32)(metadata_bytes / 1024));
	bootmem_set_reserve_cb(vm_page_reserve_range);
}

static vm_page_t *
vm_page_lookup_locked(u64 phys_addr)
{
	u64 i;

	for (i = 0; i < page_count; i++) {
		if (pages[i].phys_addr == phys_addr)
			return (&pages[i]);
	}
	return (NULL);
}

static vm_page_t *
vm_page_alloc_locked(u32 flags)
{
	vm_page_t *page;

	if (queue_head[PQ_FREE] != NULL) {
		page = queue_head[PQ_FREE];
	} else if (queue_head[PQ_CACHE] != NULL) {
		page = queue_head[PQ_CACHE];
	} else {
		return (NULL);
	}

	vm_page_queue_remove(page);
	bootmem_reserve_phys(page->phys_addr, PAGE_SIZE);

	if (flags & VM_PAGE_WIRED) {
		page->state = VM_PAGE_USED | VM_PAGE_WIRED;
	} else {
		page->state = VM_PAGE_USED;
	}

	page->ref_count = 1;

	if (!(flags & VM_PAGE_WIRED))
		vm_page_queue_insert(page, PQ_ACTIVE);

	return (page);
}

static void
vm_page_free_locked(vm_page_t *page)
{
	if (page == NULL)
		return;

	vm_page_queue_remove(page);
	page->state = VM_PAGE_FREE;
	page->ref_count = 0;
	vm_page_queue_insert(page, PQ_FREE);
}

vm_page_t *
vm_page_alloc(u32 flags)
{
	vm_page_t *page;

	spin_lock(&vm_page_spin);
	page = vm_page_alloc_locked(flags);
	spin_unlock(&vm_page_spin);
	return (page);
}

void
vm_page_free(vm_page_t *page)
{
	spin_lock(&vm_page_spin);
	vm_page_free_locked(page);
	spin_unlock(&vm_page_spin);
}

u64
vm_page_alloc_phys(u32 flags)
{
	vm_page_t *page;
	u64	phys;

	spin_lock(&vm_page_spin);
	page = vm_page_alloc_locked(flags);
	phys = page != NULL ? page->phys_addr : 0;
	spin_unlock(&vm_page_spin);
	return (phys);
}

u64
vm_page_alloc_contig(u32 page_total, u64 alignment, u64 max_address)
{
	vm_page_t	*page;
	u64		base;
	u64		i, j;

	if (page_total == 0 || alignment == 0 ||
	    (alignment & (alignment - 1)) != 0) {
		return (0);
	}
	spin_lock(&vm_page_spin);
	for (i = 0; i + page_total <= page_count; i++) {
		base = pages[i].phys_addr;
		if ((base & (alignment - 1)) != 0 ||
		    base + (u64)page_total * PAGE_SIZE - 1 > max_address) {
			continue;
		}
		for (j = 0; j < page_total; j++) {
			page = &pages[i + j];
			if (page->phys_addr != base + j * PAGE_SIZE ||
			    page->state != VM_PAGE_FREE) {
				break;
			}
		}
		if (j != page_total) {
			continue;
		}
		for (j = 0; j < page_total; j++) {
			page = &pages[i + j];
			vm_page_queue_remove(page);
			bootmem_reserve_phys(page->phys_addr, PAGE_SIZE);
			page->state = VM_PAGE_USED | VM_PAGE_WIRED;
			page->ref_count = 1;
		}
		spin_unlock(&vm_page_spin);
		return (base);
	}
	spin_unlock(&vm_page_spin);
	return (0);
}

void
vm_page_free_contig(u64 phys_addr, u32 page_total)
{
	vm_page_t	*page;
	u32		i;

	spin_lock(&vm_page_spin);
	for (i = 0; i < page_total; i++) {
		page = vm_page_lookup_locked(phys_addr +
		    (u64)i * PAGE_SIZE);
		if (page != NULL) {
			vm_page_free_locked(page);
		}
	}
	spin_unlock(&vm_page_spin);
}

int
vm_page_free_phys(u64 phys_addr)
{
	vm_page_t *page;

	spin_lock(&vm_page_spin);
	page = vm_page_lookup_locked(phys_addr & ~((u64)PAGE_SIZE - 1));
	if (page == NULL) {
		spin_unlock(&vm_page_spin);
		return (-1);
	}
	if (page->ref_count > 1) {
		page->ref_count--;
		spin_unlock(&vm_page_spin);
		return (0);
	}
	vm_page_free_locked(page);
	spin_unlock(&vm_page_spin);
	return (0);
}

void
vm_page_ref(vm_page_t *page)
{
	if (page == NULL)
		return;
	__atomic_fetch_add(&page->ref_count, 1, __ATOMIC_ACQ_REL);
}

void
vm_page_unref(vm_page_t *page)
{
	u32	old;

	if (page == NULL)
		return;
	do {
		old = __atomic_load_n(&page->ref_count, __ATOMIC_RELAXED);
		if (old == 0)
			return;
	} while (!__atomic_compare_exchange_n(&page->ref_count, &old,
	    old - 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED));
}

void
vm_page_ref_phys(u64 phys_addr)
{
	vm_page_t *page;

	spin_lock(&vm_page_spin);
	page = vm_page_lookup_locked(phys_addr & ~((u64)PAGE_SIZE - 1));
	if (page != NULL)
		page->ref_count++;
	spin_unlock(&vm_page_spin);
}

u32
vm_page_ref_count(u64 phys_addr)
{
	vm_page_t *page;
	u32	count;

	spin_lock(&vm_page_spin);
	page = vm_page_lookup_locked(phys_addr & ~((u64)PAGE_SIZE - 1));
	count = page != NULL ? page->ref_count : 0;
	spin_unlock(&vm_page_spin);
	return (count);
}

void
vm_page_activate(vm_page_t *page)
{
	if (page == NULL)
		return;
	spin_lock(&vm_page_spin);
	if (!(page->state & VM_PAGE_WIRED)) {
		vm_page_queue_remove(page);
		vm_page_queue_insert(page, PQ_ACTIVE);
	}
	spin_unlock(&vm_page_spin);
}

void
vm_page_deactivate(vm_page_t *page)
{
	if (page == NULL)
		return;
	spin_lock(&vm_page_spin);
	if (!(page->state & VM_PAGE_WIRED)) {
		vm_page_queue_remove(page);
		vm_page_queue_insert(page, PQ_INACTIVE);
	}
	spin_unlock(&vm_page_spin);
}

void
vm_page_cache_insert(vm_page_t *page)
{
	if (page == NULL)
		return;
	spin_lock(&vm_page_spin);
	if (!(page->state & VM_PAGE_WIRED)) {
		vm_page_queue_remove(page);
		page->state = VM_PAGE_FREE;
		page->ref_count = 0;
		vm_page_queue_insert(page, PQ_CACHE);
	}
	spin_unlock(&vm_page_spin);
}

static void
vm_page_reserve_locked(u64 phys_start, u64 size)
{
	u64	addr;
	u64	end;
	vm_page_t	*vp;

	end = phys_start + size;
	for (addr = phys_start; addr < end; addr += PAGE_SIZE) {
		vp = vm_page_lookup_locked(addr);
		if (vp == NULL) {
			continue;
		}
		if (vp->state == VM_PAGE_FREE) {
			vm_page_queue_remove(vp);
			vp->state = VM_PAGE_RESERVED;
		}
	}
}

void
vm_page_reserve_range(u64 phys_start, u64 size)
{
	spin_lock(&vm_page_spin);
	vm_page_reserve_locked(phys_start, size);
	spin_unlock(&vm_page_spin);
}

u64
vm_page_count_free(void)
{
	vm_page_t *p;
	u64 count;

	count = 0;
	spin_lock(&vm_page_spin);
	for (p = queue_head[PQ_FREE]; p != NULL; p = p->queue_next)
		count++;
	for (p = queue_head[PQ_CACHE]; p != NULL; p = p->queue_next)
		count++;
	spin_unlock(&vm_page_spin);
	return (count);
}

static u64
vm_page_queue_count_locked(int qid)
{
	vm_page_t *p;
	u64 count;

	count = 0;
	if (qid < 0 || qid >= PQ_COUNT)
		return (0);
	for (p = queue_head[qid]; p != NULL; p = p->queue_next)
		count++;
	return (count);
}

u64
vm_page_queue_count(int qid)
{
	u64	count;

	spin_lock(&vm_page_spin);
	count = vm_page_queue_count_locked(qid);
	spin_unlock(&vm_page_spin);
	return (count);
}

u64
vm_page_count_total(void)
{
	return (__atomic_load_n(&page_count, __ATOMIC_ACQUIRE));
}

void
vm_page_queue_counts(u64 *active, u64 *inactive, u64 *cache, u64 *wired)
{
	u64 act, inact, cach, wir, i;

	spin_lock(&vm_page_spin);
	act = vm_page_queue_count_locked(PQ_ACTIVE);
	inact = vm_page_queue_count_locked(PQ_INACTIVE);
	cach = vm_page_queue_count_locked(PQ_CACHE);
	wir = 0;
	for (i = 0; i < page_count; i++) {
		if (pages[i].state & VM_PAGE_WIRED)
			wir++;
	}
	spin_unlock(&vm_page_spin);

	if (active != NULL)
		*active = act;
	if (inactive != NULL)
		*inactive = inact;
	if (cache != NULL)
		*cache = cach;
	if (wired != NULL)
		*wired = wir;
}

vm_page_t *
vm_page_lookup(u64 phys_addr)
{
	vm_page_t *page;

	spin_lock(&vm_page_spin);
	page = vm_page_lookup_locked(phys_addr);
	spin_unlock(&vm_page_spin);
	return (page);
}

void
vm_page_dump(void)
{
	const char *qnames[] = {"NONE", "FREE", "CACHE",
	    "ACTIVE", "INACTIVE", "LAUNDRY"};
	u64 qcount[PQ_COUNT];
	u64 reserved_count, wired_count, i;

	spin_lock(&vm_page_spin);
	for (i = 0; i < PQ_COUNT; i++) {
		qcount[i] = vm_page_queue_count_locked((int)i);
	}

	reserved_count = 0;
	wired_count = 0;
	for (i = 0; i < page_count; i++) {
		if (pages[i].state == VM_PAGE_RESERVED) {
			reserved_count++;
		} else if (pages[i].state & VM_PAGE_WIRED) {
			wired_count++;
		}
	}
	spin_unlock(&vm_page_spin);

	printk("--- vm_page dump ---\n");
	printk("total pages : %u\n", (u32)page_count);
	for (i = 0; i < PQ_COUNT; i++) {
		if (i == PQ_NONE)
			continue;
		printk("%-10s  : %u\n", qnames[i],
		    (u32)qcount[i]);
	}
	printk("wired       : %u\n", (u32)wired_count);
	printk("reserved    : %u\n", (u32)reserved_count);
	printk("--------------------\n");
}
