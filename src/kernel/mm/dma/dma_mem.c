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

$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type dma_tag_t as opaque pointer to a device DMA constraint set
$define %type dma_mem_t as struct with a coherent device-visible allocation

$define %func dma_crosses_boundary as function with args u64, u64, u64
$define %func dma_mem_alloc as function with args dma_tag_t, u64, u32, dma_mem_t *
$define %func dma_mem_free as procedure with args dma_mem_t *

*/

/* !SPACE!

$space %internal dma_crosses_boundary
$space %export dma_mem_alloc, dma_mem_free

*/

#include <mm/dma/dma_internal.h>
#include <mm/vm/vm_page.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>


static int
dma_crosses_boundary(u64 phys, u64 len, u64 boundary)
{
	if (boundary == DMA_BOUNDARY_NONE || len == 0) {
		return (0);
	}
	return ((phys & ~(boundary - 1)) !=
	    ((phys + len - 1) & ~(boundary - 1)));
}

int
dma_mem_alloc(dma_tag_t tag, u64 size, u32 flags, dma_mem_t *mem)
{
	u64	align, phys, bytes, pages;

	(void)flags;

	if (mem == NULL) {
		return (-1);
	}
	memset(mem, 0, sizeof(*mem));
	if (tag == NULL || size == 0 || !dma_g.ready || !tag->live) {
		return (-1);
	}
	if (size > tag->maxsize) {
		__atomic_fetch_add(&dma_g.fail_constraint, 1,
		    __ATOMIC_RELAXED);
		drivers_log("[DMA] %s: coherent request %llu exceeds tag "
		    "maxsize %llu\n", tag->name, (unsigned long long)size,
		    (unsigned long long)tag->maxsize);
		return (-1);
	}

	align = tag->alignment;
	if (tag->boundary != DMA_BOUNDARY_NONE) {
		if (size > tag->boundary) {

			__atomic_fetch_add(&dma_g.fail_constraint, 1,
			    __ATOMIC_RELAXED);
			drivers_log("[DMA] %s: coherent request %llu exceeds "
			    "boundary %llu\n", tag->name,
			    (unsigned long long)size,
			    (unsigned long long)tag->boundary);
			return (-1);
		}

		if (tag->boundary > align) {
			align = tag->boundary;
		}
	}

	bytes = (size + PAGE_SIZE - 1) & ~((u64)PAGE_SIZE - 1);
	pages = bytes >> PAGE_SHIFT;
	if (pages == 0 || pages > 0xFFFFFFFFULL) {
		return (-1);
	}

	phys = vm_page_alloc_contig((u32)pages, align, tag->lowaddr,
	    tag->highaddr);
	if (phys == 0) {
		__atomic_fetch_add(&dma_g.fail_nomem, 1, __ATOMIC_RELAXED);
		drivers_log("[DMA] %s: no contiguous run: %llu pages "
		    "align=%llu window=[0x%llx,0x%llx]\n", tag->name,
		    (unsigned long long)pages, (unsigned long long)align,
		    (unsigned long long)tag->lowaddr,
		    (unsigned long long)tag->highaddr);
		return (-1);
	}


	if ((phys & (align - 1)) != 0 || !dma_addr_ok(tag, phys, bytes) ||
	    dma_crosses_boundary(phys, bytes, tag->boundary)) {
		vm_page_free_contig(phys, (u32)pages);
		__atomic_fetch_add(&dma_g.fail_constraint, 1,
		    __ATOMIC_RELAXED);
		drivers_log("[DMA] %s: allocator returned 0x%llx violating "
		    "align=%llu boundary=%llu window=[0x%llx,0x%llx]\n",
		    tag->name, (unsigned long long)phys,
		    (unsigned long long)align,
		    (unsigned long long)tag->boundary,
		    (unsigned long long)tag->lowaddr,
		    (unsigned long long)tag->highaddr);
		return (-1);
	}

	mem->virt = (void *)(phys + DMAP_BASE);
	mem->phys = phys;
	mem->size = bytes;
	mem->tag = tag;
	mem->pages = (u32)pages;


	memset(mem->virt, 0, bytes);

	__atomic_fetch_add(&dma_g.mem_allocs, 1, __ATOMIC_RELAXED);
	__atomic_fetch_add(&dma_g.mem_bytes, bytes, __ATOMIC_RELAXED);
	return (0);
}

void
dma_mem_free(dma_mem_t *mem)
{
	u64	bytes;
	u32	pages;

	if (mem == NULL || mem->virt == NULL || mem->pages == 0) {
		return;
	}
	bytes = mem->size;
	pages = mem->pages;

	vm_page_free_contig(mem->phys, pages);

	memset(mem, 0, sizeof(*mem));

	__atomic_fetch_add(&dma_g.mem_frees, 1, __ATOMIC_RELAXED);
	if (__atomic_load_n(&dma_g.mem_bytes, __ATOMIC_RELAXED) >= bytes) {
		__atomic_fetch_sub(&dma_g.mem_bytes, bytes, __ATOMIC_RELAXED);
	}
}
