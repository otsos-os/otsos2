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

$define %type u8 as 8 bit unsigned
$define %type u16 as 16 bit unsigned
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func pmap_alloc_table as function with args void
$define %func pmap_alloc_zeroed_page as function with args void
$define %func pmap_free_phys_page as procedure with args u64
$define %func pmap_wrmsr as procedure with args u32, u64
$define %func pmap_rdmsr as function with args u32
$define %func split_huge_pde as function with args u64 *, u16, u64
$define %func get_next_level_from as function with args u64 *, u16, int, u64
$define %func pmap_share_user_pages_cow as function with args u64 *, u64 *
$define %func pmap_init as procedure with args void
$define %func pmap_is_initialized as function with args void
$define %func pmap_enter as procedure with args u64, u64, u64
$define %func pmap_remove as procedure with args u64
$define %func pmap_extract as function with args u64
$define %func pmap_enter_in as procedure with args u64 *, u64, u64, u64
$define %func pmap_create as function with args void
$define %func pmap_kernel_cr3 as function with args void
$define %func pmap_extract_flags as function with args u64
$define %func pmap_clear_user_range as procedure with args u64, u64
$define %func pmap_clone as function with args u64
$define %func pmap_destroy as procedure with args u64
$define %func pmap_destroy_page_tables_only as procedure with args u64

*/

/* !SPACE!

$space %internal pmap_alloc_table, pmap_alloc_zeroed_page
$space %internal pmap_free_phys_page, pmap_wrmsr, pmap_rdmsr
$space %internal split_huge_pde, get_next_level_from
$space %internal pmap_share_user_pages_cow
$space %export pmap_init, pmap_is_initialized, pmap_enter
$space %export pmap_remove, pmap_extract, pmap_enter_in
$space %export pmap_create, pmap_kernel_cr3, pmap_extract_flags
$space %export pmap_clear_user_range, pmap_clone, pmap_destroy
$space %export pmap_destroy_page_tables_only

*/

#include <mm/vm/pmap.h>
#include <lib/com1.h>
#include <mm/kmem.h>
#include <mm/vm/vm_page.h>

#define MSR_EFER		0xC0000080
#define EFER_NXE		(1ULL << 11)

static u64	g_kernel_cr3 = 0;
static int	pmap_initialized = 0;

static u64 *
pmap_alloc_table(void)
{
	u64	phys;

	phys = vm_page_alloc_phys(VM_PAGE_WIRED);
	if (phys == 0) {
		return (NULL);
	}
	memset((void *)phys, 0, PAGE_SIZE);
	return ((u64 *)phys);
}

static u64
pmap_alloc_zeroed_page(void)
{
	u64	phys;

	phys = vm_page_alloc_phys(0);
	if (phys == 0) {
		return (0);
	}
	memset((void *)phys, 0, PAGE_SIZE);
	return (phys);
}

static void
pmap_free_phys_page(u64 phys)
{
	if (phys != 0) {
		vm_page_free_phys(phys & PTE_ADDR_MASK);
	}
}

static void
pmap_wrmsr(u32 msr, u64 value)
{
	u32	low;
	u32	high;

	low = value & 0xFFFFFFFF;
	high = value >> 32;
	__asm__ volatile("wrmsr" : : "c"(msr), "a"(low),
	    "d"(high));
}

static u64
pmap_rdmsr(u32 msr)
{
	u32	low;
	u32	high;

	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) :
	    "c"(msr));
	return (((u64)high << 32) | low);
}

static u64 *
split_huge_pde(u64 *pd, u16 pd_index, u64 flags)
{
	u64	*pt;
	u64	base;
	u64	pde_flags;
	u64	entry_flags;
	u64	new_pde;
	u64	pde;
	u64	i;

	pde = pd[pd_index];
	if (!(pde & PTE_PRESENT) || !(pde & PTE_HUGE)) {
		return (NULL);
	}

	base = pde & PTE_ADDR_MASK;
	pde_flags = pde & PTE_FLAGS_MASK;

	pt = pmap_alloc_table();
	if (pt == NULL) {
		com1_printf("[PMAP] split huge page failed\n");
		return (NULL);
	}
	memset(pt, 0, PAGE_SIZE);

	entry_flags = (pde_flags & ~PTE_HUGE) | PTE_PRESENT;
	for (i = 0; i < 512; i++) {
		pt[i] = (base + (i * PAGE_SIZE)) | entry_flags;
	}

	new_pde = (u64)pt | PTE_PRESENT | PTE_RW;
	if (flags & PTE_USER) {
		new_pde |= PTE_USER;
	} else if (pde_flags & PTE_USER) {
		new_pde |= PTE_USER;
	}
	pd[pd_index] = new_pde;

	return (pt);
}

void
pmap_init(void)
{
	u64	efer;
	u64	cr3;

	efer = pmap_rdmsr(MSR_EFER);
	if (!(efer & EFER_NXE)) {
		pmap_wrmsr(MSR_EFER, efer | EFER_NXE);
	}
	cr3 = pmap_get_cr3();
	if (g_kernel_cr3 == 0) {
		g_kernel_cr3 = cr3;
	}
	com1_printf("[PMAP] Initialized. CR3: %p\n",
	    (void *)cr3);
	pmap_initialized = 1;
}

int
pmap_is_initialized(void)
{
	u64	efer;

	if (!pmap_initialized) {
		return (0);
	}
	efer = pmap_rdmsr(MSR_EFER);
	if (!(efer & EFER_NXE)) {
		return (0);
	}
	return (g_kernel_cr3 != 0);
}

static u64 *
get_next_level_from(u64 *current_table, u16 index, int alloc,
    u64 flags)
{
	u64	*new_table;
	u64	*old_table;
	u64	entry;
	u64	new_entry;

	entry = current_table[index];

	if (entry & PTE_PRESENT) {
		if (flags & PTE_USER) {
			if (!(entry & PTE_USER)) {
				old_table = (u64 *)(entry &
				    PTE_ADDR_MASK);
				new_table = pmap_alloc_table();
				if (new_table == NULL) {
					com1_printf("[PMAP] alloc "
					    "table failed\n");
					return (NULL);
				}
				memcpy(new_table, old_table,
				    PAGE_SIZE);
				new_entry = ((u64)new_table) |
				    (entry & PTE_FLAGS_MASK) |
				    PTE_USER | PTE_PRESENT;
				current_table[index] = new_entry;
				entry = new_entry;
			} else {
				current_table[index] |= PTE_USER;
			}
		}
		return ((u64 *)(entry & PTE_ADDR_MASK));
	}

	if (!alloc) {
		return (NULL);
	}

	new_table = pmap_alloc_table();
	if (new_table == NULL) {
		com1_printf("[PMAP] alloc table failed\n");
		return (NULL);
	}

	new_entry = (u64)new_table | PTE_PRESENT | PTE_RW;
	if (flags & PTE_USER) {
		new_entry |= PTE_USER;
	}

	current_table[index] = new_entry;
	return (new_table);
}

void
pmap_enter(u64 vaddr, u64 paddr, u64 flags)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;
	u64	pde;

	pml4_index = (vaddr >> 39) & 0x1FF;
	pdpt_index = (vaddr >> 30) & 0x1FF;
	pd_index = (vaddr >> 21) & 0x1FF;
	pt_index = (vaddr >> 12) & 0x1FF;

	pml4 = (u64 *)(pmap_get_cr3() & PTE_ADDR_MASK);

	pdpt = get_next_level_from(pml4, (u16)pml4_index, 1,
	    flags);
	if (pdpt == NULL) {
		return;
	}

	pd = get_next_level_from(pdpt, (u16)pdpt_index, 1,
	    flags);
	if (pd == NULL) {
		return;
	}

	pde = pd[pd_index];
	if ((pde & PTE_PRESENT) && (pde & PTE_HUGE)) {
		if (split_huge_pde(pd, (u16)pd_index, flags)
		    == NULL) {
			return;
		}
	}

	pt = get_next_level_from(pd, (u16)pd_index, 1, flags);
	if (pt == NULL) {
		return;
	}

	pt[pt_index] = (paddr & PTE_ADDR_MASK) |
	    (flags & PTE_FLAGS_MASK) | PTE_PRESENT;

	pmap_invlpg(vaddr);
}

void
pmap_remove(u64 vaddr)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;

	pml4_index = (vaddr >> 39) & 0x1FF;
	pdpt_index = (vaddr >> 30) & 0x1FF;
	pd_index = (vaddr >> 21) & 0x1FF;
	pt_index = (vaddr >> 12) & 0x1FF;

	pml4 = (u64 *)(pmap_get_cr3() & PTE_ADDR_MASK);

	pdpt = get_next_level_from(pml4, (u16)pml4_index, 0, 0);
	if (pdpt == NULL) {
		return;
	}

	pd = get_next_level_from(pdpt, (u16)pdpt_index, 0, 0);
	if (pd == NULL) {
		return;
	}

	pt = get_next_level_from(pd, (u16)pd_index, 0, 0);
	if (pt == NULL) {
		return;
	}

	pt[pt_index] = 0;
	pmap_invlpg(vaddr);
}

u64
pmap_extract(u64 vaddr)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;
	u64	pml4e;
	u64	pdpte;
	u64	pde;
	u64	pte;

	pml4_index = (vaddr >> 39) & 0x1FF;
	pdpt_index = (vaddr >> 30) & 0x1FF;
	pd_index = (vaddr >> 21) & 0x1FF;
	pt_index = (vaddr >> 12) & 0x1FF;

	pml4 = (u64 *)(pmap_get_cr3() & PTE_ADDR_MASK);

	pml4e = pml4[pml4_index];
	if (!(pml4e & PTE_PRESENT)) {
		return (0);
	}
	pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);

	pdpte = pdpt[pdpt_index];
	if (!(pdpte & PTE_PRESENT)) {
		return (0);
	}

	if (pdpte & PTE_HUGE) {
		return ((pdpte & PTE_ADDR_MASK) +
		    (vaddr & 0x3FFFFFFF));
	}

	pd = (u64 *)(pdpte & PTE_ADDR_MASK);
	pde = pd[pd_index];
	if (!(pde & PTE_PRESENT)) {
		return (0);
	}

	if (pde & PTE_HUGE) {
		return ((pde & PTE_ADDR_MASK) +
		    (vaddr & 0x1FFFFF));
	}

	pt = (u64 *)(pde & PTE_ADDR_MASK);
	pte = pt[pt_index];
	if (!(pte & PTE_PRESENT)) {
		return (0);
	}

	return ((pte & PTE_ADDR_MASK) + (vaddr & 0xFFF));
}

void
pmap_enter_in(u64 *pml4, u64 vaddr, u64 paddr, u64 flags)
{
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;

	if (pml4 == NULL) {
		return;
	}

	pml4_index = (vaddr >> 39) & 0x1FF;
	pdpt_index = (vaddr >> 30) & 0x1FF;
	pd_index = (vaddr >> 21) & 0x1FF;
	pt_index = (vaddr >> 12) & 0x1FF;

	pdpt = get_next_level_from(pml4, (u16)pml4_index, 1,
	    flags);
	if (pdpt == NULL) {
		return;
	}

	pd = get_next_level_from(pdpt, (u16)pdpt_index, 1,
	    flags);
	if (pd == NULL) {
		return;
	}

	pt = get_next_level_from(pd, (u16)pd_index, 1, flags);
	if (pt == NULL) {
		return;
	}

	pt[pt_index] = (paddr & PTE_ADDR_MASK) |
	    (flags & PTE_FLAGS_MASK) | PTE_PRESENT;
}

u64
pmap_create(void)
{
	u64	*src_pml4;
	u64	*new_pml4;
	int	i;

	src_pml4 = (u64 *)(pmap_kernel_cr3() & PTE_ADDR_MASK);
	new_pml4 = pmap_alloc_table();
	if (new_pml4 == NULL) {
		return (0);
	}
	for (i = 0; i < 512; i++) {
		if (i > 0 && i < 256) {
			continue;
		}
		if (!(src_pml4[i] & PTE_PRESENT)) {
			continue;
		}
		new_pml4[i] = src_pml4[i];
	}

	return ((u64)new_pml4);
}

u64
pmap_kernel_cr3(void)
{
	return (g_kernel_cr3);
}

u64
pmap_extract_flags(u64 vaddr)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;
	u64	pml4e;
	u64	pdpte;
	u64	pde;
	u64	pte;

	pml4_index = (vaddr >> 39) & 0x1FF;
	pdpt_index = (vaddr >> 30) & 0x1FF;
	pd_index = (vaddr >> 21) & 0x1FF;
	pt_index = (vaddr >> 12) & 0x1FF;

	pml4 = (u64 *)(pmap_get_cr3() & PTE_ADDR_MASK);

	pml4e = pml4[pml4_index];
	if (!(pml4e & PTE_PRESENT)) {
		return (0);
	}
	pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
	pdpte = pdpt[pdpt_index];
	if (!(pdpte & PTE_PRESENT)) {
		return (0);
	}
	if (pdpte & PTE_HUGE) {
		return (pdpte & PTE_FLAGS_MASK);
	}
	pd = (u64 *)(pdpte & PTE_ADDR_MASK);
	pde = pd[pd_index];
	if (!(pde & PTE_PRESENT)) {
		return (0);
	}
	if (pde & PTE_HUGE) {
		return (pde & PTE_FLAGS_MASK);
	}
	pt = (u64 *)(pde & PTE_ADDR_MASK);
	pte = pt[pt_index];
	if (!(pte & PTE_PRESENT)) {
		return (0);
	}
	return (pte & PTE_FLAGS_MASK);
}

void
pmap_clear_user_range(u64 start, u64 end)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	*pte_ptr;
	u64	vaddr;
	u64	pml4_index;
	u64	pdpt_index;
	u64	pd_index;
	u64	pt_index;
	u64	pml4e;
	u64	pdpte;
	u64	pde;

	if (end <= start) {
		return;
	}

	vaddr = start & ~(PAGE_SIZE - 1);
	for (; vaddr < end; vaddr += PAGE_SIZE) {
		pml4_index = (vaddr >> 39) & 0x1FF;
		pdpt_index = (vaddr >> 30) & 0x1FF;
		pd_index = (vaddr >> 21) & 0x1FF;
		pt_index = (vaddr >> 12) & 0x1FF;

		pml4 = (u64 *)(pmap_get_cr3() & PTE_ADDR_MASK);
		pml4e = pml4[pml4_index];
		if (!(pml4e & PTE_PRESENT)) {
			continue;
		}

		pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
		pdpte = pdpt[pdpt_index];
		if (!(pdpte & PTE_PRESENT)) {
			continue;
		}
		if (pdpte & PTE_HUGE) {
			continue;
		}

		pd = (u64 *)(pdpte & PTE_ADDR_MASK);
		pde = pd[pd_index];
		if (!(pde & PTE_PRESENT)) {
			continue;
		}
		if (pde & PTE_HUGE) {
			if (split_huge_pde(pd,
			    (u16)pd_index,
			    pde & PTE_FLAGS_MASK) == NULL) {
				continue;
			}
			pde = pd[pd_index];
			if (!(pde & PTE_PRESENT) ||
			    (pde & PTE_HUGE)) {
				continue;
			}
		}

		pt = (u64 *)(pde & PTE_ADDR_MASK);
		pte_ptr = &pt[pt_index];
		if (!(*pte_ptr & PTE_PRESENT)) {
			continue;
		}
		if (*pte_ptr & PTE_USER) {
			*pte_ptr &= ~PTE_USER;
			pmap_invlpg(vaddr);
		}
	}
}

static int
pmap_share_user_pages_cow(u64 *dst_pml4, u64 *src_pml4)
{
	u64	*src_pdpt;
	u64	*dst_pdpt;
	u64	*src_pd;
	u64	*dst_pd;
	u64	*src_pt;
	u64	*dst_pt;
	u64	pml4e;
	u64	pdpte;
	u64	pde;
	u64	pte;
	u64	phys;
	u64	ro_flags;
	u64	pml4_flags;
	u64	pdpt_flags;
	u64	pd_flags;
	u64	vaddr;
	u64	i;
	u64	j;
	u64	k;
	u64	l;

	for (i = 0; i < 512; i++) {
		pml4e = src_pml4[i];
		if (!(pml4e & PTE_PRESENT)) {
			continue;
		}
		if (!(pml4e & PTE_USER)) {
			dst_pml4[i] = pml4e;
			continue;
		}

		src_pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
		dst_pdpt = pmap_alloc_table();
		if (dst_pdpt == NULL) {
			return (-1);
		}
		pml4_flags = pml4e & PTE_FLAGS_MASK;
		dst_pml4[i] = ((u64)dst_pdpt) | pml4_flags |
		    PTE_PRESENT;

		for (j = 0; j < 512; j++) {
			pdpte = src_pdpt[j];
			if (!(pdpte & PTE_PRESENT)) {
				continue;
			}
			if (pdpte & PTE_HUGE) {
				if (!(pdpte & PTE_USER)) {
					dst_pdpt[j] = pdpte;
					continue;
				}
				com1_printf("[PMAP] 1GB huge not "
				    "supported in clone\n");
				return (-1);
			}
			if (!(pdpte & PTE_USER)) {
				dst_pdpt[j] = pdpte;
				continue;
			}

			src_pd = (u64 *)(pdpte & PTE_ADDR_MASK);
			dst_pd = pmap_alloc_table();
			if (dst_pd == NULL) {
				return (-1);
			}
			pdpt_flags = pdpte & PTE_FLAGS_MASK;
			dst_pdpt[j] = ((u64)dst_pd) | pdpt_flags |
			    PTE_PRESENT;

			for (k = 0; k < 512; k++) {
				pde = src_pd[k];
				if (!(pde & PTE_PRESENT)) {
					continue;
				}
				if (pde & PTE_HUGE) {
					if (!(pde & PTE_USER)) {
						dst_pd[k] = pde;
						continue;
					}
					if (split_huge_pde(src_pd,
					    (u16)k,
					    pde & PTE_FLAGS_MASK)
					    == NULL) {
						return (-1);
					}
					pde = src_pd[k];
					if (!(pde & PTE_PRESENT) ||
					    (pde & PTE_HUGE)) {
						continue;
					}
				}
				if (!(pde & PTE_USER)) {
					dst_pd[k] = pde;
					continue;
				}

				src_pt = (u64 *)(pde &
				    PTE_ADDR_MASK);
				dst_pt = pmap_alloc_table();
				if (dst_pt == NULL) {
					return (-1);
				}
				pd_flags = pde & PTE_FLAGS_MASK;
				dst_pd[k] = ((u64)dst_pt) |
				    (pd_flags & ~PTE_HUGE) |
				    PTE_PRESENT;

				for (l = 0; l < 512; l++) {
					pte = src_pt[l];
					if (!(pte & PTE_PRESENT)) {
						continue;
					}
					if (!(pte & PTE_USER)) {
						dst_pt[l] = pte;
						continue;
					}

					phys = pte & PTE_ADDR_MASK;
					vm_page_ref_phys(phys);

					ro_flags = (pte &
					    PTE_FLAGS_MASK) & ~PTE_RW;
					if (pte & PTE_RW) {
						ro_flags |= PTE_COW;
					}

					dst_pt[l] = (phys &
					    PTE_ADDR_MASK) |
					    ro_flags | PTE_PRESENT;
					src_pt[l] = (phys &
					    PTE_ADDR_MASK) |
					    ro_flags | PTE_PRESENT;

					vaddr = (i << 39) |
					    (j << 30) |
					    (k << 21) |
					    (l << 12);
					pmap_invlpg(vaddr);
				}
			}
		}
	}

	return (0);
}

u64
pmap_clone(u64 src_cr3)
{
	u64	*src_pml4;
	u64	*dst_pml4;

	src_pml4 = (u64 *)(src_cr3 & PTE_ADDR_MASK);
	dst_pml4 = (u64 *)pmap_create();
	if (dst_pml4 == NULL) {
		return (0);
	}

	if (pmap_share_user_pages_cow(dst_pml4, src_pml4)
	    != 0) {
		pmap_destroy((u64)dst_pml4);
		return (0);
	}

	return ((u64)dst_pml4);
}

void
pmap_destroy(u64 cr3)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	*pt;
	u64	pml4e;
	u64	pdpte;
	u64	pde;
	u64	pte;
	u64	i;
	u64	j;
	u64	k;
	u64	l;
	int	pdpt_has_present;
	int	pd_has_present;
	int	pt_has_present;

	pml4 = (u64 *)(cr3 & PTE_ADDR_MASK);
	if (pml4 == NULL || (cr3 & PTE_ADDR_MASK) ==
	    (g_kernel_cr3 & PTE_ADDR_MASK)) {
		return;
	}

	for (i = 0; i < 512; i++) {
		pml4e = pml4[i];
		if (!(pml4e & PTE_PRESENT) ||
		    !(pml4e & PTE_USER)) {
			continue;
		}
		pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
		pdpt_has_present = 0;
		for (j = 0; j < 512; j++) {
			pdpte = pdpt[j];
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
			pd = (u64 *)(pdpte & PTE_ADDR_MASK);
			pd_has_present = 0;
			for (k = 0; k < 512; k++) {
				pde = pd[k];
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
				pt = (u64 *)(pde & PTE_ADDR_MASK);
				pt_has_present = 0;
				for (l = 0; l < 512; l++) {
					pte = pt[l];
					if (!(pte & PTE_PRESENT)) {
						continue;
					}
					if (!(pte & PTE_USER)) {
						pt_has_present = 1;
						continue;
					}
					pmap_free_phys_page(pte &
					    PTE_ADDR_MASK);
					pt[l] = 0;
				}
				if (pt_has_present) {
					pd_has_present = 1;
				} else {
					pmap_free_phys_page((u64)pt);
					pd[k] = 0;
				}
			}
			if (pd_has_present) {
				pdpt_has_present = 1;
			} else {
				pmap_free_phys_page((u64)pd);
				pdpt[j] = 0;
			}
		}
		if (!pdpt_has_present) {
			pmap_free_phys_page((u64)pdpt);
			pml4[i] = 0;
		}
	}
	pmap_free_phys_page((u64)pml4);
}

void
pmap_destroy_page_tables_only(u64 cr3)
{
	u64	*pml4;
	u64	*pdpt;
	u64	*pd;
	u64	pml4e;
	u64	pdpte;
	u64	pde;
	u64	i;
	u64	j;
	u64	k;

	pml4 = (u64 *)(cr3 & PTE_ADDR_MASK);
	if (pml4 == NULL || (cr3 & PTE_ADDR_MASK) ==
	    (g_kernel_cr3 & PTE_ADDR_MASK)) {
		return;
	}

	for (i = 0; i < 512; i++) {
		pml4e = pml4[i];
		if (!(pml4e & PTE_PRESENT) ||
		    !(pml4e & PTE_USER)) {
			continue;
		}
		pdpt = (u64 *)(pml4e & PTE_ADDR_MASK);
		for (j = 0; j < 512; j++) {
			pdpte = pdpt[j];
			if (!(pdpte & PTE_PRESENT) ||
			    !(pdpte & PTE_USER) ||
			    (pdpte & PTE_HUGE)) {
				continue;
			}
			pd = (u64 *)(pdpte & PTE_ADDR_MASK);
			for (k = 0; k < 512; k++) {
				pde = pd[k];
				if (!(pde & PTE_PRESENT) ||
				    !(pde & PTE_USER) ||
				    (pde & PTE_HUGE)) {
					continue;
				}
				pmap_free_phys_page((u64)(u64 *)
				    (pde & PTE_ADDR_MASK));
			}
			pmap_free_phys_page((u64)pd);
		}
		pmap_free_phys_page((u64)pdpt);
	}
	pmap_free_phys_page((u64)pml4);
}
