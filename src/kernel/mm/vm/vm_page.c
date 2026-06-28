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

#include <mm/vm/vm_page.h>
#include <kernel/bootmem.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

#define PAGE_SIZE 4096
#define VM_PAGE_MAX 4096

static vm_page_t bootstrap_pages[VM_PAGE_MAX];
static vm_page_t *pages = bootstrap_pages;
static u64 page_capacity = VM_PAGE_MAX;
static u64 page_count;

static vm_page_t *queue_head[PQ_COUNT];
static vm_page_t *queue_tail[PQ_COUNT];

static u64
round_page(u64 value)
{
    return value & ~((u64)(PAGE_SIZE - 1));
}

static u64
trunc_page(u64 value)
{
    return value & ~((u64)(PAGE_SIZE - 1));
}

static u64
atop(u64 bytes)
{
    return bytes / PAGE_SIZE;
}

static void
vm_page_queue_insert(vm_page_t *page, u8 qid)
{
    page->queue = qid;
    page->queue_next = NULL;
    page->queue_prev = queue_tail[qid];
    if (queue_tail[qid] != NULL)
        queue_tail[qid]->queue_next = page;
    else
        queue_head[qid] = page;
    queue_tail[qid] = page;
}

static void
vm_page_queue_remove(vm_page_t *page)
{
    u8 qid;

    qid = page->queue;
    if (qid == PQ_NONE || qid >= PQ_COUNT)
        return;

    if (page->queue_prev != NULL)
        page->queue_prev->queue_next = page->queue_next;
    else
        queue_head[qid] = page->queue_next;

    if (page->queue_next != NULL)
        page->queue_next->queue_prev = page->queue_prev;
    else
        queue_tail[qid] = page->queue_prev;

    page->queue_next = NULL;
    page->queue_prev = NULL;
    page->queue = PQ_NONE;
}

void
vm_page_init(u64 available_start, u64 available_end)
{
    u64 start;
    u64 end;
    u64 addr;
    u64 i;

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
    u32 range_count;
    u64 metadata_bytes;
    u64 managed_pages;
    u64 i;

    ranges = bootmem_ranges();
    range_count = bootmem_range_count();
    managed_pages = 0;

    for (u32 r = 0; r < range_count; r++) {
        u64 start = (ranges[r].start + PAGE_SIZE - 1) & ~((u64)(PAGE_SIZE - 1));
        u64 end = ranges[r].end & ~((u64)(PAGE_SIZE - 1));
        if (end > start)
            managed_pages += atop(end - start);
    }

    metadata_bytes = managed_pages * sizeof(vm_page_t);
    pages = NULL;
    if (metadata_bytes != 0)
        pages = (vm_page_t *)bootmem_alloc(metadata_bytes, PAGE_SIZE);
    if (pages == NULL) {
        pages = bootstrap_pages;
        page_capacity = VM_PAGE_MAX;
        metadata_bytes = sizeof(bootstrap_pages);
        com1_printf("[VM_PAGE] metadata bootmem allocation failed, using %u pages\n",
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

    for (u32 r = 0; r < range_count; r++) {
        u64 addr = (ranges[r].start + PAGE_SIZE - 1) & ~((u64)(PAGE_SIZE - 1));
        u64 end = ranges[r].end & ~((u64)(PAGE_SIZE - 1));

        while (addr < end && page_count < page_capacity) {
            pages[page_count].phys_addr = addr;
            pages[page_count].state = VM_PAGE_FREE;
            pages[page_count].ref_count = 0;
            vm_page_queue_insert(&pages[page_count], PQ_FREE);
            page_count++;
            addr += PAGE_SIZE;
        }
    }

    com1_printf("[VM_PAGE] initialized: %u pages from bootmem (%u KB metadata)\n",
                (u32)page_count, (u32)(metadata_bytes / 1024));
}

vm_page_t *
vm_page_alloc(u32 flags)
{
    vm_page_t *page;

    if (queue_head[PQ_FREE] != NULL)
        page = queue_head[PQ_FREE];
    else if (queue_head[PQ_CACHE] != NULL)
        page = queue_head[PQ_CACHE];
    else
        return NULL;

    vm_page_queue_remove(page);

    if (flags & VM_PAGE_WIRED)
        page->state = VM_PAGE_USED | VM_PAGE_WIRED;
    else
        page->state = VM_PAGE_USED;

    page->ref_count = 1;

    if (!(flags & VM_PAGE_WIRED))
        vm_page_queue_insert(page, PQ_ACTIVE);

    return page;
}

void
vm_page_free(vm_page_t *page)
{
    if (page == NULL)
        return;

    vm_page_queue_remove(page);
    page->state = VM_PAGE_FREE;
    page->ref_count = 0;
    vm_page_queue_insert(page, PQ_FREE);
}

u64
vm_page_alloc_phys(u32 flags)
{
    vm_page_t *page = vm_page_alloc(flags);
    if (page == NULL)
        return 0;
    return page->phys_addr;
}

int
vm_page_free_phys(u64 phys_addr)
{
    vm_page_t *page = vm_page_lookup(phys_addr & ~((u64)PAGE_SIZE - 1));
    if (page == NULL)
        return -1;
    if (page->ref_count > 1) {
        page->ref_count--;
        return 0;
    }
    vm_page_free(page);
    return 0;
}

void
vm_page_ref(vm_page_t *page)
{
    if (page == NULL)
        return;
    page->ref_count++;
}

void
vm_page_unref(vm_page_t *page)
{
    if (page == NULL)
        return;
    if (page->ref_count > 0)
        page->ref_count--;
}

void
vm_page_ref_phys(u64 phys_addr)
{
    vm_page_t *page = vm_page_lookup(phys_addr & ~((u64)PAGE_SIZE - 1));
    if (page == NULL)
        return;
    vm_page_ref(page);
}

u32
vm_page_ref_count(u64 phys_addr)
{
    vm_page_t *page = vm_page_lookup(phys_addr & ~((u64)PAGE_SIZE - 1));
    if (page == NULL)
        return 0;
    return page->ref_count;
}

void
vm_page_activate(vm_page_t *page)
{
    if (page == NULL)
        return;
    if (page->state & VM_PAGE_WIRED)
        return;
    vm_page_queue_remove(page);
    vm_page_queue_insert(page, PQ_ACTIVE);
}

void
vm_page_deactivate(vm_page_t *page)
{
    if (page == NULL)
        return;
    if (page->state & VM_PAGE_WIRED)
        return;
    vm_page_queue_remove(page);
    vm_page_queue_insert(page, PQ_INACTIVE);
}

void
vm_page_cache_insert(vm_page_t *page)
{
    if (page == NULL)
        return;
    if (page->state & VM_PAGE_WIRED)
        return;
    vm_page_queue_remove(page);
    page->state = VM_PAGE_FREE;
    page->ref_count = 0;
    vm_page_queue_insert(page, PQ_CACHE);
}

u64
vm_page_count_free(void)
{
    u64 count;
    vm_page_t *p;

    count = 0;
    for (p = queue_head[PQ_FREE]; p != NULL; p = p->queue_next)
        count++;
    for (p = queue_head[PQ_CACHE]; p != NULL; p = p->queue_next)
        count++;
    return count;
}

u64
vm_page_queue_count(int qid)
{
    u64 count;
    vm_page_t *p;

    count = 0;
    if (qid < 0 || qid >= PQ_COUNT)
        return 0;
    for (p = queue_head[qid]; p != NULL; p = p->queue_next)
        count++;
    return count;
}

u64
vm_page_count_total(void)
{
    return page_count;
}

void
vm_page_queue_counts(u64 *active, u64 *inactive, u64 *cache, u64 *wired)
{
    u64 act, inact, cach, wir;
    vm_page_t *p;
    u64 i;

    act = 0;
    for (p = queue_head[PQ_ACTIVE]; p != NULL; p = p->queue_next)
        act++;
    inact = 0;
    for (p = queue_head[PQ_INACTIVE]; p != NULL; p = p->queue_next)
        inact++;
    cach = 0;
    for (p = queue_head[PQ_CACHE]; p != NULL; p = p->queue_next)
        cach++;
    wir = 0;
    for (i = 0; i < page_count; i++) {
        if (pages[i].state & VM_PAGE_WIRED)
            wir++;
    }

    if (active) *active = act;
    if (inactive) *inactive = inact;
    if (cache) *cache = cach;
    if (wired) *wired = wir;
}

vm_page_t *
vm_page_lookup(u64 phys_addr)
{
    u64 i;

    for (i = 0; i < page_count; i++) {
        if (pages[i].phys_addr == phys_addr)
            return &pages[i];
    }
    return NULL;
}

void
vm_page_dump(void)
{
    u64 qcount[PQ_COUNT];
    u64 reserved_count;
    u64 wired_count;
    u64 i;
    vm_page_t *p;
    const char *qnames[] = {"NONE", "FREE", "CACHE", "ACTIVE",
                            "INACTIVE", "LAUNDRY"};

    for (i = 0; i < PQ_COUNT; i++) {
        qcount[i] = 0;
        for (p = queue_head[i]; p != NULL; p = p->queue_next)
            qcount[i]++;
    }

    reserved_count = 0;
    wired_count = 0;
    for (i = 0; i < page_count; i++) {
        if (pages[i].state == VM_PAGE_RESERVED)
            reserved_count++;
        else if (pages[i].state & VM_PAGE_WIRED)
            wired_count++;
    }

    com1_printf("--- vm_page dump ---\n");
    com1_printf("total pages : %u\n", (u32)page_count);
    for (i = 0; i < PQ_COUNT; i++) {
        if (i == PQ_NONE) continue;
        com1_printf("%-10s  : %u\n", qnames[i], (u32)qcount[i]);
    }
    com1_printf("wired       : %u\n", (u32)wired_count);
    com1_printf("reserved    : %u\n", (u32)reserved_count);
    com1_printf("--------------------\n");
}
