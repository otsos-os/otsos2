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
$define %type dma_bounce_page_t as struct with one poolable bounce page
$define %type dma_map_t as struct with one loaded buffer and its segments

$define %func dma_bounce_page_alloc as function with args u64, u64
$define %func dma_bounce_page_release as procedure with args dma_bounce_page_t *
$define %func dma_bounce_grow_locked as function with args u64, u64, u64
$define %func dma_bounce_startup as procedure with args u32
$define %func dma_bounce_configure as procedure with args u32, u32
$define %func dma_bounce_get as function with args u64, u64
$define %func dma_bounce_put as procedure with args dma_bounce_page_t *
$define %func dma_bounce_teardown as procedure with args void
$define %func dma_bounce_copy_in as procedure with args dma_map_t *
$define %func dma_bounce_copy_out as procedure with args dma_map_t *

*/

/* !SPACE!

$space %internal dma_bounce_page_alloc, dma_bounce_page_release
$space %internal dma_bounce_grow_locked
$space %export dma_bounce_startup, dma_bounce_configure
$space %export dma_bounce_get, dma_bounce_put, dma_bounce_teardown
$space %export dma_bounce_copy_in, dma_bounce_copy_out

*/

#include <mm/dma/dma_internal.h>
#include <mm/vm/vm_page.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>


static dma_bounce_page_t *
dma_bounce_page_alloc(u64 lowaddr, u64 highaddr)
{
	dma_bounce_page_t	*page;
	u64			phys;

	phys = vm_page_alloc_contig(1, PAGE_SIZE, lowaddr, highaddr);
	if (phys == 0) {
		return (NULL);
	}
	page = kmem_alloc(sizeof(*page));
	if (page == NULL) {

		vm_page_free_contig(phys, 1);
		return (NULL);
	}
	page->next = NULL;
	page->virt = (void *)(phys + DMAP_BASE);
	page->phys = phys;
	page->busy = 0;
	page->pad = 0;
	return (page);
}

static void
dma_bounce_page_release(dma_bounce_page_t *page)
{
	if (page == NULL) {
		return;
	}
	vm_page_free_contig(page->phys, 1);
	kmem_free(page);
}


static u64
dma_bounce_grow_locked(u64 target, u64 lowaddr, u64 highaddr)
{
	dma_bounce_page_t	*page;
	u64			added;

	added = 0;
	while (dma_g.pool_pages < target &&
	    dma_g.pool_pages < dma_g.pool_max_pages) {
		page = dma_bounce_page_alloc(lowaddr, highaddr);
		if (page == NULL) {
			break;
		}
		page->next = dma_g.free_pages;
		dma_g.free_pages = page;
		dma_g.pool_pages++;
		added++;
	}
	return (added);
}

void
dma_bounce_startup(u32 reserve_kb)
{
	u64	reserve, added;

	reserve = ((u64)reserve_kb * 1024) >> PAGE_SHIFT;

	spin_lock(&dma_g.lock);
	if (dma_g.pool_max_pages == 0) {
		dma_g.pool_max_pages =
		    ((u64)DMA_BOUNCE_MAX_KB_DEFAULT * 1024) >> PAGE_SHIFT;
	}
	added = dma_bounce_grow_locked(reserve, 0, DMA_BOUNCE_WINDOW);
	spin_unlock(&dma_g.lock);

	if (added < reserve) {
		drivers_log("[DMA] bounce reserve short: %llu of %llu pages "
		    "below 0x%llx\n", (unsigned long long)added,
		    (unsigned long long)reserve,
		    (unsigned long long)DMA_BOUNCE_WINDOW);
	}
}

void
dma_bounce_configure(u32 reserve_kb, u32 max_kb)
{
	u64	reserve, max_pages;

	max_pages = ((u64)max_kb * 1024) >> PAGE_SHIFT;
	reserve = ((u64)reserve_kb * 1024) >> PAGE_SHIFT;
	if (max_pages == 0) {
		max_pages = 1;
	}
	if (reserve > max_pages) {
		reserve = max_pages;
	}

	spin_lock(&dma_g.lock);
	dma_g.pool_max_pages = max_pages;
	(void)dma_bounce_grow_locked(reserve, 0, DMA_BOUNCE_WINDOW);
	spin_unlock(&dma_g.lock);
}

dma_bounce_page_t *
dma_bounce_get(u64 lowaddr, u64 highaddr)
{
	dma_bounce_page_t	*page, *prev;
	u64			inuse;

	spin_lock(&dma_g.lock);

	prev = NULL;
	for (page = dma_g.free_pages; page != NULL; page = page->next) {
		if (page->phys >= lowaddr &&
		    page->phys + PAGE_SIZE - 1 <= highaddr) {
			break;
		}
		prev = page;
	}

	if (page != NULL) {
		if (prev == NULL) {
			dma_g.free_pages = page->next;
		} else {
			prev->next = page->next;
		}
	} else {
		if (dma_g.pool_pages >= dma_g.pool_max_pages) {
			__atomic_fetch_add(&dma_g.fail_bounce, 1,
			    __ATOMIC_RELAXED);
			spin_unlock(&dma_g.lock);
			return (NULL);
		}

		page = dma_bounce_page_alloc(lowaddr, highaddr);
		if (page == NULL) {
			__atomic_fetch_add(&dma_g.fail_bounce, 1,
			    __ATOMIC_RELAXED);
			spin_unlock(&dma_g.lock);
			return (NULL);
		}
		dma_g.pool_pages++;
	}

	page->next = NULL;
	page->busy = 1;
	dma_g.pool_inuse++;
	inuse = dma_g.pool_inuse;
	spin_unlock(&dma_g.lock);

	dma_peak_update(&dma_g.pool_peak, inuse);
	return (page);
}

void
dma_bounce_put(dma_bounce_page_t *page)
{
	if (page == NULL) {
		return;
	}

	spin_lock(&dma_g.lock);
	page->busy = 0;
	page->next = dma_g.free_pages;
	dma_g.free_pages = page;
	if (dma_g.pool_inuse != 0) {
		dma_g.pool_inuse--;
	}
	spin_unlock(&dma_g.lock);
}

void
dma_bounce_teardown(void)
{
	dma_bounce_page_t	*page, *next;

	spin_lock(&dma_g.lock);
	page = dma_g.free_pages;
	dma_g.free_pages = NULL;

	dma_g.pool_pages -= dma_g.pool_inuse < dma_g.pool_pages ?
	    (dma_g.pool_pages - dma_g.pool_inuse) : dma_g.pool_pages;
	spin_unlock(&dma_g.lock);

	while (page != NULL) {
		next = page->next;
		dma_bounce_page_release(page);
		page = next;
	}
}

void
dma_bounce_copy_in(dma_map_t *map)
{
	dma_bounce_slot_t	*slot;
	u32			i;

	if (map == NULL || map->bounce == NULL) {
		return;
	}
	for (i = 0; i < map->nbounce; i++) {
		slot = &map->bounce[i];
		if (slot->page == NULL || slot->orig == NULL) {
			continue;
		}
		memcpy(slot->page->virt, slot->orig, slot->len);
	}
}

void
dma_bounce_copy_out(dma_map_t *map)
{
	dma_bounce_slot_t	*slot;
	u32			i;

	if (map == NULL || map->bounce == NULL) {
		return;
	}
	for (i = 0; i < map->nbounce; i++) {
		slot = &map->bounce[i];
		if (slot->page == NULL || slot->orig == NULL) {
			continue;
		}
		memcpy(slot->orig, slot->page->virt, slot->len);
	}
}
