/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <mlibc/mlibc.h>

#define VM_PAGE_FREE      0x00
#define VM_PAGE_USED      0x01
#define VM_PAGE_RESERVED  0x02
#define VM_PAGE_WIRED     0x04

#define PQ_NONE     0
#define PQ_FREE     1
#define PQ_CACHE    2
#define PQ_ACTIVE   3
#define PQ_INACTIVE 4
#define PQ_LAUNDRY  5
#define PQ_COUNT    6

typedef struct vm_page {
  u64  phys_addr;
  u32  state;
  u32  ref_count;
  u8   queue;
  struct vm_page *queue_next;
  struct vm_page *queue_prev;
} vm_page_t;

void vm_page_init(u64 available_start, u64 available_end);
void vm_page_init_from_bootmem(void);
vm_page_t *vm_page_alloc(u32 flags);
void vm_page_free(vm_page_t *page);
u64 vm_page_alloc_phys(u32 flags);
int vm_page_free_phys(u64 phys_addr);
void vm_page_ref(vm_page_t *page);
void vm_page_unref(vm_page_t *page);
void vm_page_ref_phys(u64 phys_addr);
u32 vm_page_ref_count(u64 phys_addr);

void vm_page_activate(vm_page_t *page);
void vm_page_deactivate(vm_page_t *page);
void vm_page_cache_insert(vm_page_t *page);

u64 vm_page_count_free(void);
u64 vm_page_count_total(void);
void vm_page_queue_counts(u64 *active, u64 *inactive, u64 *cache, u64 *wired);
vm_page_t *vm_page_lookup(u64 phys_addr);

void vm_page_dump(void);

#endif
