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

typedef struct vm_page {
  u64  phys_addr;
  u32  state;
  u32  ref_count;
  struct vm_page *next;
} vm_page_t;

void vm_page_init(u64 available_start, u64 available_end);
void vm_page_init_from_bootmem(void);
vm_page_t *vm_page_alloc(u32 flags);
void vm_page_free(vm_page_t *page);
u64 vm_page_alloc_phys(u32 flags);
int vm_page_free_phys(u64 phys_addr);
void vm_page_ref(vm_page_t *page);
void vm_page_unref(vm_page_t *page);

u64 vm_page_count_free(void);
u64 vm_page_count_total(void);
vm_page_t *vm_page_lookup(u64 phys_addr);

void vm_page_dump(void);

#endif
