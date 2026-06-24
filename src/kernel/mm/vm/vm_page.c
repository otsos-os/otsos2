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

#include <mm/vm/vm_page.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

#define PAGE_SIZE 4096
#define VM_PAGE_MAX 4096

static vm_page_t pages[VM_PAGE_MAX];
static u64 page_count;
static vm_page_t *free_list;

void
vm_page_init(u64 available_start, u64 available_end)
{
    u64 start;
    u64 end;
    u64 addr;
    u64 i;

    memset(pages, 0, sizeof(pages));

    start = (available_start + PAGE_SIZE - 1) & ~((u64)(PAGE_SIZE - 1));
    end = available_end & ~((u64)(PAGE_SIZE - 1));

    page_count = 0;
    free_list = NULL;

    for (i = 0; i < VM_PAGE_MAX; i++) {
        pages[i].phys_addr = 0;
        pages[i].state = VM_PAGE_RESERVED;
        pages[i].ref_count = 0;
        pages[i].next = NULL;
    }

    addr = start;
    for (i = 0; i < VM_PAGE_MAX && addr < end; i++) {
        pages[i].phys_addr = addr;
        pages[i].state = VM_PAGE_FREE;
        pages[i].ref_count = 0;
        pages[i].next = free_list;
        free_list = &pages[i];
        addr += PAGE_SIZE;
        page_count++;
    }
}

vm_page_t *
vm_page_alloc(u32 flags)
{
    vm_page_t *page;

    if (free_list == NULL)
        return NULL;

    page = free_list;
    free_list = page->next;

    if (flags & VM_PAGE_WIRED)
        page->state = VM_PAGE_USED | VM_PAGE_WIRED;
    else
        page->state = VM_PAGE_USED;

    page->ref_count = 1;
    page->next = NULL;

    return page;
}

void
vm_page_free(vm_page_t *page)
{
    if (page == NULL)
        return;

    page->state = VM_PAGE_FREE;
    page->ref_count = 0;
    page->next = free_list;
    free_list = page;
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

u64
vm_page_count_free(void)
{
    u64 count;
    u64 i;

    count = 0;
    for (i = 0; i < page_count; i++) {
        if (pages[i].state == VM_PAGE_FREE)
            count++;
    }
    return count;
}

u64
vm_page_count_total(void)
{
    return page_count;
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
    u64 free_count;
    u64 used_count;
    u64 reserved_count;
    u64 wired_count;
    u64 i;

    free_count = 0;
    used_count = 0;
    reserved_count = 0;
    wired_count = 0;

    for (i = 0; i < page_count; i++) {
        if (pages[i].state == VM_PAGE_FREE)
            free_count++;
        else if (pages[i].state == VM_PAGE_RESERVED)
            reserved_count++;
        else if (pages[i].state & VM_PAGE_WIRED)
            wired_count++;
        else
            used_count++;
    }

    com1_printf("--- vm_page dump ---\n");
    com1_printf("total pages : %u\n", (u32)page_count);
    com1_printf("free pages  : %u\n", (u32)free_count);
    com1_printf("used pages  : %u\n", (u32)used_count);
    com1_printf("wired pages : %u\n", (u32)wired_count);
    com1_printf("reserved    : %u\n", (u32)reserved_count);
    com1_printf("--------------------\n");
}
