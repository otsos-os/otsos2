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

#define MSR_EFER 0xC0000080
#define EFER_NXE (1ULL << 11)

static u64 g_kernel_cr3 = 0;

static inline void mmu_wrmsr(u32 msr, u64 value) {
  u32 low = value & 0xFFFFFFFF;
  u32 high = value >> 32;
  __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline u64 mmu_rdmsr(u32 msr) {
  u32 low, high;
  __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
  return ((u64)high << 32) | low;
}

static u64 *split_huge_pde(u64 *pd, u16 pd_index, u64 flags) {
  u64 pde = pd[pd_index];
  if (!(pde & PTE_PRESENT) || !(pde & PTE_HUGE)) {
    return NULL;
  }

  u64 base = pde & PTE_ADDR_MASK;
  u64 pde_flags = pde & PTE_FLAGS_MASK;

  u64 *pt = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
  if (!pt) {
    com1_printf("[MMU] Error: Failed to split huge page\n");
    return NULL;
  }
  memset(pt, 0, PAGE_SIZE);

  u64 entry_flags = (pde_flags & ~PTE_HUGE) | PTE_PRESENT;
  for (u64 i = 0; i < 512; i++) {
    pt[i] = (base + (i * PAGE_SIZE)) | entry_flags;
  }

  u64 new_pde = (u64)pt | PTE_PRESENT | PTE_RW;
  if (flags & PTE_USER) {
    new_pde |= PTE_USER;
  } else if (pde_flags & PTE_USER) {
    new_pde |= PTE_USER;
  }
  pd[pd_index] = new_pde;

  return pt;
}

void mmu_init() {
  u64 efer = mmu_rdmsr(MSR_EFER);
  if (!(efer & EFER_NXE)) {
    mmu_wrmsr(MSR_EFER, efer | EFER_NXE);
  }
  u64 cr3 = mmu_read_cr3();
  if (g_kernel_cr3 == 0) {
    g_kernel_cr3 = cr3;
  }
  com1_printf("[MMU] Initialized. Current CR3: %p\n", (void *)cr3);
}

static u64 *get_next_level_from(u64 *current_table, u16 index, int alloc,
                                u64 flags) {
  u64 entry = current_table[index];

  if (entry & PTE_PRESENT) {
    /* If we are mapping a user page, ensure the directory entry has USER.
     * If it was a kernel-only shared table, clone it to avoid global mutation.
     */
    if (flags & PTE_USER) {
      if (!(entry & PTE_USER)) {
        u64 *old_table = (u64 *)(entry & PTE_ADDR_MASK);
        u64 *new_table = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
        if (!new_table) {
          com1_printf("[MMU] Error: Failed to allocate page table!\n");
          return NULL;
        }
        memcpy(new_table, old_table, PAGE_SIZE);
        u64 new_entry = ((u64)new_table) | (entry & PTE_FLAGS_MASK) | PTE_USER |
                        PTE_PRESENT;
        current_table[index] = new_entry;
        entry = new_entry;
      } else {
        current_table[index] |= PTE_USER;
      }
    }
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

  u64 new_entry = (u64)new_table | PTE_PRESENT | PTE_RW;
  if (flags & PTE_USER) {
    new_entry |= PTE_USER;
  }

  current_table[index] = new_entry;
  return new_table;
}

void mmu_map_page(u64 vaddr, u64 paddr, u64 flags) {
  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);

  u64 *pdpt = get_next_level_from(pml4, pml4_index, 1, flags);
  if (!pdpt)
    return;

  u64 *pd = get_next_level_from(pdpt, pdpt_index, 1, flags);
  if (!pd)
    return;

  u64 pde = pd[pd_index];
  if ((pde & PTE_PRESENT) && (pde & PTE_HUGE)) {
    if (!split_huge_pde(pd, pd_index, flags)) {
      return;
    }
  }

  u64 *pt = get_next_level_from(pd, pd_index, 1, flags);
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

  u64 *pdpt = get_next_level_from(pml4, pml4_index, 0, 0);
  if (!pdpt)
    return;

  u64 *pd = get_next_level_from(pdpt, pdpt_index, 0, 0);
  if (!pd)
    return;

  u64 *pt = get_next_level_from(pd, pd_index, 0, 0);
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

  u64 *pdpt = get_next_level_from(pml4, pml4_index, 0, 0);
  if (!pdpt)
    return 0;

  u64 *pd = get_next_level_from(pdpt, pdpt_index, 0, 0);
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

void mmu_map_page_in(u64 *pml4, u64 vaddr, u64 paddr, u64 flags) {
  if (!pml4) {
    return;
  }

  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pdpt = get_next_level_from(pml4, pml4_index, 1, flags);
  if (!pdpt)
    return;

  u64 *pd = get_next_level_from(pdpt, pdpt_index, 1, flags);
  if (!pd)
    return;

  u64 *pt = get_next_level_from(pd, pd_index, 1, flags);
  if (!pt)
    return;

  pt[pt_index] =
      (paddr & PTE_ADDR_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;
}

u64 mmu_create_address_space(void) {
  u64 *src_pml4 = (u64 *)(mmu_kernel_cr3() & PTE_ADDR_MASK);
  u64 *new_pml4 = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
  if (!new_pml4) {
    return 0;
  }
  memset(new_pml4, 0, PAGE_SIZE);

  for (int i = 0; i < 512; i++) {
    u64 entry = src_pml4[i];
    if (!(entry & PTE_PRESENT)) {
      continue;
    }
    new_pml4[i] = entry;
  }

  return (u64)new_pml4;
}

u64 mmu_kernel_cr3(void) { return g_kernel_cr3; }

u64 mmu_get_pte_flags(u64 vaddr) {
  u64 pml4_index = (vaddr >> 39) & 0x1FF;
  u64 pdpt_index = (vaddr >> 30) & 0x1FF;
  u64 pd_index = (vaddr >> 21) & 0x1FF;
  u64 pt_index = (vaddr >> 12) & 0x1FF;

  u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);

  u64 pml4e = pml4[pml4_index];
  if (!(pml4e & PTE_PRESENT)) {
    return 0;
  }
  u64 *pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
  u64 pdpte = pdpt[pdpt_index];
  if (!(pdpte & PTE_PRESENT)) {
    return 0;
  }
  if (pdpte & PTE_HUGE) {
    return pdpte & PTE_FLAGS_MASK;
  }
  u64 *pd = (u64 *)(pdpte & PTE_ADDR_MASK);
  u64 pde = pd[pd_index];
  if (!(pde & PTE_PRESENT)) {
    return 0;
  }
  if (pde & PTE_HUGE) {
    return pde & PTE_FLAGS_MASK;
  }
  u64 *pt = (u64 *)(pde & PTE_ADDR_MASK);
  u64 pte = pt[pt_index];
  if (!(pte & PTE_PRESENT)) {
    return 0;
  }
  return pte & PTE_FLAGS_MASK;
}

void mmu_clear_user_range(u64 start, u64 end) {
  if (end <= start) {
    return;
  }

  u64 vaddr = start & ~(PAGE_SIZE - 1);
  for (; vaddr < end; vaddr += PAGE_SIZE) {
    u64 pml4_index = (vaddr >> 39) & 0x1FF;
    u64 pdpt_index = (vaddr >> 30) & 0x1FF;
    u64 pd_index = (vaddr >> 21) & 0x1FF;
    u64 pt_index = (vaddr >> 12) & 0x1FF;

    u64 *pml4 = (u64 *)(mmu_read_cr3() & PTE_ADDR_MASK);
    u64 pml4e = pml4[pml4_index];
    if (!(pml4e & PTE_PRESENT)) {
      continue;
    }

    u64 *pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
    u64 pdpte = pdpt[pdpt_index];
    if (!(pdpte & PTE_PRESENT)) {
      continue;
    }
    if (pdpte & PTE_HUGE) {
      // 1GB huge page: nothing to do safely here
      continue;
    }

    u64 *pd = (u64 *)(pdpte & PTE_ADDR_MASK);
    u64 pde = pd[pd_index];
    if (!(pde & PTE_PRESENT)) {
      continue;
    }
    if (pde & PTE_HUGE) {
      if (!split_huge_pde(pd, pd_index, pde & PTE_FLAGS_MASK)) {
        continue;
      }
      pde = pd[pd_index];
      if (!(pde & PTE_PRESENT) || (pde & PTE_HUGE)) {
        continue;
      }
    }

    u64 *pt = (u64 *)(pde & PTE_ADDR_MASK);
    u64 *pte = &pt[pt_index];
    if (!(*pte & PTE_PRESENT)) {
      continue;
    }
    if (*pte & PTE_USER) {
      *pte &= ~PTE_USER;
      mmu_invlpg(vaddr);
    }
  }
}

static int mmu_copy_user_pages(u64 *dst_pml4, u64 *src_pml4) {
  for (u64 i = 0; i < 512; i++) {
    u64 pml4e = src_pml4[i];
    if (!(pml4e & PTE_PRESENT)) {
      continue;
    }
    if (!(pml4e & PTE_USER)) {
      dst_pml4[i] = pml4e;
      continue;
    }

    u64 *src_pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
    u64 *dst_pdpt = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
    if (!dst_pdpt) {
      return -1;
    }
    memset(dst_pdpt, 0, PAGE_SIZE);

    u64 pml4_flags = pml4e & PTE_FLAGS_MASK;
    dst_pml4[i] = ((u64)dst_pdpt) | pml4_flags | PTE_PRESENT;

    for (u64 j = 0; j < 512; j++) {
      u64 pdpte = src_pdpt[j];
      if (!(pdpte & PTE_PRESENT)) {
        continue;
      }
      if (pdpte & PTE_HUGE) {
        if (!(pdpte & PTE_USER)) {
          dst_pdpt[j] = pdpte;
          continue;
        }
        com1_printf("[MMU] 1GB huge pages not supported in clone\n");
        return -1;
      }
      if (!(pdpte & PTE_USER)) {
        dst_pdpt[j] = pdpte;
        continue;
      }

      u64 *src_pd = (u64 *)(pdpte & PTE_ADDR_MASK);
      u64 *dst_pd = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
      if (!dst_pd) {
        return -1;
      }
      memset(dst_pd, 0, PAGE_SIZE);

      u64 pdpt_flags = pdpte & PTE_FLAGS_MASK;
      dst_pdpt[j] = ((u64)dst_pd) | pdpt_flags | PTE_PRESENT;

      for (u64 k = 0; k < 512; k++) {
        u64 pde = src_pd[k];
        if (!(pde & PTE_PRESENT)) {
          continue;
        }
        if (pde & PTE_HUGE) {
          if (!(pde & PTE_USER)) {
            dst_pd[k] = pde;
            continue;
          }
          u64 *dst_pt = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
          if (!dst_pt) {
            return -1;
          }
          memset(dst_pt, 0, PAGE_SIZE);

          u64 pd_flags = pde & PTE_FLAGS_MASK;
          dst_pd[k] = ((u64)dst_pt) | (pd_flags & ~PTE_HUGE) | PTE_PRESENT;

          for (u64 l = 0; l < 512; l++) {
            u64 vaddr = (i << 39) | (j << 30) | (k << 21) | (l << 12);
            void *new_page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
            if (!new_page) {
              return -1;
            }
            memcpy(new_page, (void *)vaddr, PAGE_SIZE);
            u64 phys = mmu_virt_to_phys((u64)new_page);
            u64 pte_flags = (pd_flags & PTE_FLAGS_MASK) | PTE_USER;
            dst_pt[l] = (phys & PTE_ADDR_MASK) | pte_flags | PTE_PRESENT;
          }
          continue;
        }
        if (!(pde & PTE_USER)) {
          dst_pd[k] = pde;
          continue;
        }

        u64 *src_pt = (u64 *)(pde & PTE_ADDR_MASK);
        u64 *dst_pt = (u64 *)kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
        if (!dst_pt) {
          return -1;
        }
        memset(dst_pt, 0, PAGE_SIZE);

        u64 pd_flags = pde & PTE_FLAGS_MASK;
        dst_pd[k] = ((u64)dst_pt) | pd_flags | PTE_PRESENT;

        for (u64 l = 0; l < 512; l++) {
          u64 pte = src_pt[l];
          if (!(pte & PTE_PRESENT)) {
            continue;
          }
          if (!(pte & PTE_USER)) {
            dst_pt[l] = pte;
            continue;
          }

          u64 vaddr = (i << 39) | (j << 30) | (k << 21) | (l << 12);
          void *new_page = kmalloc_aligned(PAGE_SIZE, PAGE_SIZE);
          if (!new_page) {
            return -1;
          }
          memcpy(new_page, (void *)vaddr, PAGE_SIZE);

          u64 phys = mmu_virt_to_phys((u64)new_page);
          u64 pte_flags = pte & PTE_FLAGS_MASK;
          dst_pt[l] = (phys & PTE_ADDR_MASK) | pte_flags | PTE_PRESENT;
        }
      }
    }
  }

  return 0;
}

u64 mmu_clone_user_space(u64 src_cr3) {
  u64 *src_pml4 = (u64 *)(src_cr3 & PTE_ADDR_MASK);
  u64 *dst_pml4 = (u64 *)mmu_create_address_space();
  if (!dst_pml4) {
    return 0;
  }

  if (mmu_copy_user_pages(dst_pml4, src_pml4) != 0) {
    mmu_free_user_space((u64)dst_pml4);
    return 0;
  }

  return (u64)dst_pml4;
}

void mmu_free_user_space(u64 cr3) {
  u64 *pml4 = (u64 *)(cr3 & PTE_ADDR_MASK);
  if (!pml4) {
    return;
  }

  for (u64 i = 0; i < 512; i++) {
    u64 pml4e = pml4[i];
    if (!(pml4e & PTE_PRESENT) || !(pml4e & PTE_USER)) {
      continue;
    }
    u64 *pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
    int pdpt_has_present = 0;
    for (u64 j = 0; j < 512; j++) {
      u64 pdpte = pdpt[j];
      if (!(pdpte & PTE_PRESENT)) {
        continue;
      }
      if (!(pdpte & PTE_USER)) {
        pdpt_has_present = 1;
        continue;
      }
      if (pdpte & PTE_HUGE) {
        pdpt_has_present = 1;
        continue;
      }
      u64 *pd = (u64 *)(pdpte & PTE_ADDR_MASK);
      int pd_has_present = 0;
      for (u64 k = 0; k < 512; k++) {
        u64 pde = pd[k];
        if (!(pde & PTE_PRESENT)) {
          continue;
        }
        if (!(pde & PTE_USER)) {
          pd_has_present = 1;
          continue;
        }
        if (pde & PTE_HUGE) {
          pd_has_present = 1;
          continue;
        }
        u64 *pt = (u64 *)(pde & PTE_ADDR_MASK);
        int pt_has_present = 0;
        for (u64 l = 0; l < 512; l++) {
          u64 pte = pt[l];
          if (!(pte & PTE_PRESENT)) {
            continue;
          }
          if (!(pte & PTE_USER)) {
            pt_has_present = 1;
            continue;
          }
          void *page = (void *)(pte & PTE_ADDR_MASK);
          kfree(page);
          pt[l] = 0;
        }
        if (pt_has_present) {
          pd_has_present = 1;
        } else {
          kfree(pt);
          pd[k] = 0;
        }
      }
      if (pd_has_present) {
        pdpt_has_present = 1;
      } else {
        kfree(pd);
        pdpt[j] = 0;
      }
    }
    if (!pdpt_has_present) {
      kfree(pdpt);
      pml4[i] = 0;
    }
  }
}
