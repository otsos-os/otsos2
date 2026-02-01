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

#include <kernel/mmu.h>
#include <lib/com1.h>
#include <mlibc/memory.h>

void mmu_init() {
  u64 cr3 = mmu_read_cr3();
  com1_printf("[MMU] Initialized. Current CR3: %p\n", (void *)cr3);
}

static u64 *get_next_level(u64 *current_table, u16 index, int alloc) {
  u64 entry = current_table[index];

  if (entry & PTE_PRESENT) {

    return (u64 *)(entry & PTE_ADDR_MASK);
  }

  if (!alloc) {
    return NULL;
  }

  u64 *new_table = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
  if (!new_table) {
    com1_printf("[MMU] Error: Failed to allocate page table!\n");
    return NULL;
  }

  memset(new_table, 0, PAGE_SIZE);

  u64 new_entry = (u64)new_table | PTE_PRESENT | PTE_RW | PTE_USER;

  current_table[index] = new_entry;
  return new_table;
}

void mmu_map_page(u64 vaddr, u64 paddr, u64 flags) {
  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);

  u64 *pdpt = get_next_level(pml4, pml4_index, 1);
  if (!pdpt)
    return;

  u64 *pd = get_next_level(pdpt, pdpt_index, 1);
  if (!pd)
    return;

  u64 *pt = get_next_level(pd, pd_index, 1);
  if (!pt)
    return;

  pt[pt_index] =
      (paddr & PTE_ADDR_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;

  mmu_invlpg(vaddr);
}

void mmu_unmap_page(u64 vaddr) {
  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);

  u64 *pdpt = get_next_level(pml4, pml4_index, 0);
  if (!pdpt)
    return;

  u64 *pd = get_next_level(pdpt, pdpt_index, 0);
  if (!pd)
    return;

  u64 *pt = get_next_level(pd, pd_index, 0);
  if (!pt)
    return;

  pt[pt_index] = 0;
  mmu_invlpg(vaddr);
}

u64 mmu_virt_to_phys(u64 vaddr) {
  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);

  u64 *pdpt = get_next_level(pml4, pml4_index, 0);
  if (!pdpt)
    return 0;

  u64 *pd = get_next_level(pdpt, pdpt_index, 0);
  if (!pd)
    return 0;

  u64 pd_entry = ((u64 *)pdpt)[pdpt_index];

  u64 pml4e = pml4[pml4_index];
  if (!(pml4e & PTE_PRESENT))
    return 0;
  pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);

  u64 pdpte = pdpt[pdpt_index];
  if (!(pdpte & PTE_PRESENT))
    return 0;

  if (pdpte & PTE_HUGE) {
    return (pdpte & PTE_ADDR_MASK) + (vaddr & 0x3FFFFFFF);
  }

  pd = (u64 *)(pdpte & PTE_ADDR_MASK);
  u64 pde = pd[pd_index];
  if (!(pde & PTE_PRESENT))
    return 0;

  if (pde & PTE_HUGE) {
    return (pde & PTE_ADDR_MASK) + (vaddr & 0x1FFFFF);
  }

  u64 *pt = (u64 *)(pde & PTE_ADDR_MASK);
  u64 pte = pt[pt_index];
  if (!(pte & PTE_PRESENT))
    return 0;

  return (pte & PTE_ADDR_MASK) + (vaddr & 0xFFF);
}
