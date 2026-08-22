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

$define %func pmap_init as procedure with args void
$define %func pmap_is_initialized as function with args void
$define %func pmap_enter as procedure with args u64, u64, u64
$define %func pmap_remove as procedure with args u64
$define %func pmap_extract as function with args u64
$define %func pmap_extract_flags as function with args u64
$define %func pmap_pat_init as procedure with args void
$define %func pmap_pat_is_wc_available as function with args void
$define %func pmap_map_mmio as function with args u64, u64
$define %func pmap_map_framebuffer as function with args u64, u64
$define %func pmap_wc_fence as procedure with args void
$define %func pmap_create as function with args void
$define %func pmap_clone as function with args u64
$define %func pmap_destroy as procedure with args u64
$define %func pmap_destroy_page_tables_only as procedure with args u64
$define %func pmap_enter_in as procedure with args u64 *, u64, u64, u64
$define %func pmap_kernel_cr3 as function with args void
$define %func pmap_clear_user_range as procedure with args u64, u64
$define %func pmap_invlpg as procedure with args u64
$define %func pmap_load as procedure with args u64
$define %func pmap_get_cr3 as function with args void

*/

/* !SPACE!

$space %export pmap_init, pmap_is_initialized, pmap_enter, pmap_remove
$space %export pmap_extract, pmap_extract_flags, pmap_create, pmap_clone
$space %export pmap_destroy, pmap_destroy_page_tables_only, pmap_enter_in
$space %export pmap_kernel_cr3, pmap_clear_user_range
$space %export pmap_invlpg, pmap_load, pmap_get_cr3
$space %export pmap_pat_init, pmap_pat_is_wc_available
$space %export pmap_map_mmio, pmap_map_framebuffer, pmap_wc_fence

*/

#ifndef PMAP_H
#define PMAP_H

#include <mlibc/mlibc.h>

#define PAGE_SIZE		4096

#define PTE_PRESENT		0x1
#define PTE_RW			0x2
#define PTE_USER		0x4
#define PTE_PWT			0x8
#define PTE_PCD			0x10
#define PTE_ACCESSED		0x20
#define PTE_DIRTY		0x40
#define PTE_HUGE		0x80
#define PTE_GLOBAL		0x100
#define PTE_COW			0x200
#define PTE_NX			(1ULL << 63)
#define PTE_PAT			0x80
#define PMAP_CACHE_UC		(PTE_PCD | PTE_PWT)
#define PMAP_CACHE_WC		(PTE_PAT)

#define PTE_ADDR_MASK		0x000FFFFFFFFFF000ULL
#define PTE_FLAGS_MASK		0xFFF0000000000FFFULL

void	pmap_init(void);
int	pmap_is_initialized(void);
void	pmap_enter(u64 vaddr, u64 paddr, u64 flags);
void	pmap_remove(u64 vaddr);
u64	pmap_extract(u64 vaddr);
u64	pmap_extract_flags(u64 vaddr);
void	pmap_pat_init(void);
int	pmap_pat_is_wc_available(void);
void	*pmap_map_mmio(u64 paddr, u64 size);
void	*pmap_map_framebuffer(u64 paddr, u64 size);
u64	pmap_create(void);
u64	pmap_clone(u64 src_cr3);
void	pmap_destroy(u64 cr3);
void	pmap_destroy_page_tables_only(u64 cr3);
void	pmap_enter_in(u64 *pml4, u64 vaddr, u64 paddr, u64 flags);
u64	pmap_kernel_cr3(void);
void	pmap_clear_user_range(u64 start, u64 end);

static inline void
pmap_invlpg(u64 vaddr)
{
	__asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

static inline void
pmap_load(u64 pml4_addr)
{
	__asm__ volatile("mov %0, %%cr3" : : "r"(pml4_addr) :
	    "memory");
}

static inline u64
pmap_get_cr3(void)
{
	u64	cr3;

	__asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
	return (cr3);
}

static inline void
pmap_wc_fence(void)
{
	__asm__ volatile("sfence" ::: "memory");
}

#endif
