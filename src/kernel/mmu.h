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

#ifndef MMU_H
#define MMU_H

#include <mlibc/mlibc.h>

#define PAGE_SIZE 4096

#define PTE_PRESENT 0x1
#define PTE_RW 0x2
#define PTE_USER 0x4
#define PTE_PWT 0x8
#define PTE_PCD 0x10
#define PTE_ACCESSED 0x20
#define PTE_DIRTY 0x40
#define PTE_HUGE 0x80
#define PTE_GLOBAL 0x100
#define PTE_NX (1ULL << 63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000
#define PTE_FLAGS_MASK 0xFFF0000000000FFF

void mmu_init();
void mmu_map_page(u64 vaddr, u64 paddr, u64 flags);
void mmu_unmap_page(u64 vaddr);
u64 mmu_virt_to_phys(u64 vaddr);
u64 mmu_create_address_space(void);
u64 mmu_clone_user_space(u64 src_cr3);
void mmu_free_user_space(u64 cr3);
void mmu_map_page_in(u64 *pml4, u64 vaddr, u64 paddr, u64 flags);
u64 mmu_kernel_cr3(void);
u64 mmu_get_pte_flags(u64 vaddr);

static inline void mmu_invlpg(u64 vaddr) {
  __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static inline void mmu_write_cr3(u64 pml4_addr) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_addr) : "memory");
}

static inline u64 mmu_read_cr3() {
  u64 cr3;
  __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
  return cr3;
}

#endif
