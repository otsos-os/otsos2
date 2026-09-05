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
$define %type spin_t as spin mutex with owner cpu, recursion and lock order
$define %type dma_tag as struct with one resolved constraint set
$define %type dma_bounce_page_t as struct with one poolable bounce page
$define %type dma_bounce_slot_t as struct binding a bounce page to a chunk
$define %type dma_global_t as struct with subsystem lock, pool and counters

$define %func dma_tag_root_create as function with args void
$define %func dma_tag_teardown as procedure with args void
$define %func dma_addr_ok as function with args const struct dma_tag *, u64, u64
$define %func dma_bounce_startup as procedure with args u32
$define %func dma_bounce_configure as procedure with args u32, u32
$define %func dma_bounce_get as function with args u64, u64
$define %func dma_bounce_put as procedure with args dma_bounce_page_t *
$define %func dma_bounce_teardown as procedure with args void
$define %func dma_bounce_copy_in as procedure with args dma_map_t *
$define %func dma_bounce_copy_out as procedure with args dma_map_t *
$define %func dma_peak_update as procedure with args u64 *, u64

$const DMA_BOUNCE_WINDOW as physical ceiling the reserve pool is built under
$const DMA_BOUNCE_RESERVE_KB_DEFAULT as pages preallocated at init, in KiB
$const DMA_BOUNCE_MAX_KB_DEFAULT as pool ceiling, in KiB

*/

/* !SPACE!

$space %export dma_tag_root_create, dma_tag_teardown, dma_addr_ok
$space %export dma_bounce_startup, dma_bounce_configure
$space %export dma_bounce_get, dma_bounce_put, dma_bounce_teardown
$space %export dma_bounce_copy_in, dma_bounce_copy_out
$space %export dma_peak_update

*/

#ifndef MM_DMA_DMA_INTERNAL_H
#define MM_DMA_DMA_INTERNAL_H

#include <kernel/sync/sync.h>
#include <mm/dma/dma.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>

#define	DMA_TAG_NAME_MAX	24
#define	DMA_BOUNCE_WINDOW		0xFFFFFFFFULL
#define	DMA_BOUNCE_RESERVE_KB_DEFAULT	64
#define	DMA_BOUNCE_MAX_KB_DEFAULT	1024

struct dma_tag {
	u64		alignment;
	u64		boundary;
	u64		lowaddr;
	u64		highaddr;
	u64		maxsize;
	u64		maxsegsz;
	dma_tag_t	parent;
	u32		nsegments;
	u32		children;
	u32		flags;
	u32		live;
	char		name[DMA_TAG_NAME_MAX];
};

typedef struct dma_bounce_page {
	struct dma_bounce_page	*next;
	void			*virt;
	u64			phys;
	u32			busy;
	u32			pad;
} dma_bounce_page_t;

typedef struct dma_bounce_slot {
	dma_bounce_page_t	*page;
	void			*orig;
	u64			len;
} dma_bounce_slot_t;

typedef struct dma_global {
	spin_t			lock;
	struct dma_tag		root;
	int			ready;
	dma_bounce_page_t	*free_pages;
	u64			pool_pages;
	u64			pool_inuse;
	u64			pool_peak;
	u64			pool_max_pages;
	u64			tags_live;
	u64			tags_peak;
	u64			mem_allocs;
	u64			mem_frees;
	u64			mem_bytes;
	u64			map_loads;
	u64			map_unloads;
	u64			bounced_loads;
	u64			fail_nomem;
	u64			fail_nsegs;
	u64			fail_bounce;
	u64			fail_constraint;
	u64			fail_unmapped;
} dma_global_t;

extern dma_global_t	dma_g;

int	dma_tag_root_create(void);
void	dma_tag_teardown(void);
int	dma_addr_ok(const struct dma_tag *tag, u64 phys, u64 len);

void	dma_bounce_startup(u32 reserve_kb);
void	dma_bounce_configure(u32 reserve_kb, u32 max_kb);
dma_bounce_page_t	*dma_bounce_get(u64 lowaddr, u64 highaddr);
void	dma_bounce_put(dma_bounce_page_t *page);
void	dma_bounce_teardown(void);

void	dma_bounce_copy_in(dma_map_t *map);
void	dma_bounce_copy_out(dma_map_t *map);

static inline void
dma_peak_update(u64 *peak, u64 value)
{
	u64	seen;

	for (;;) {
		seen = __atomic_load_n(peak, __ATOMIC_RELAXED);
		if (value <= seen) {
			return;
		}
		if (__atomic_compare_exchange_n(peak, &seen, value, 0,
		    __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
			return;
		}
	}
}

#endif
