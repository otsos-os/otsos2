/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#ifndef PMAP_H
#define PMAP_H

#include <mlibc/mlibc.h>

#define PAGE_SIZE       4096

#define PTE_PRESENT    0x1
#define PTE_RW         0x2
#define PTE_USER       0x4
#define PTE_PWT        0x8
#define PTE_PCD        0x10
#define PTE_ACCESSED   0x20
#define PTE_DIRTY      0x40
#define PTE_HUGE       0x80
#define PTE_GLOBAL     0x100
#define PTE_COW        0x200  /* bit 9: software marker for copy-on-write */
#define PTE_NX         (1ULL << 63)

#define PTE_ADDR_MASK  0x000FFFFFFFFFF000ULL
#define PTE_FLAGS_MASK 0xFFF0000000000FFFULL

void pmap_init(void);
int  pmap_is_initialized(void);

void pmap_enter(u64 vaddr, u64 paddr, u64 flags);
void pmap_remove(u64 vaddr);
u64  pmap_extract(u64 vaddr);
u64  pmap_extract_flags(u64 vaddr);

u64  pmap_create(void);
u64  pmap_clone(u64 src_cr3);
void pmap_destroy(u64 cr3);
void pmap_enter_in(u64 *pml4, u64 vaddr, u64 paddr, u64 flags);

u64  pmap_kernel_cr3(void);

void pmap_clear_user_range(u64 start, u64 end);

static inline void pmap_invlpg(u64 vaddr) {
  __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static inline void pmap_load(u64 pml4_addr) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_addr) : "memory");
}

static inline u64 pmap_get_cr3(void) {
  u64 cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

#endif
