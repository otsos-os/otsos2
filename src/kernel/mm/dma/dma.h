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
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed
$define %type dma_tag_t as opaque pointer to a device DMA constraint set
$define %type dma_seg_t as struct with one physical scatter/gather segment
$define %type dma_mem_t as struct with a coherent device-visible allocation
$define %type dma_map_t as struct with one loaded buffer and its segments
$define %type dma_stat_t as struct with framework accounting counters

$define %func dma_init as procedure with args void
$define %func dma_is_initialized as function with args void
$define %func dma_tag_root as function with args void
$define %func dma_tag_create as function with args dma_tag_t, u64, u64, u64, u64, u64, u32, u64, u32, const char *, dma_tag_t *
$define %func dma_tag_destroy as procedure with args dma_tag_t
$define %func dma_tag_name as function with args dma_tag_t
$define %func dma_mem_alloc as function with args dma_tag_t, u64, u32, dma_mem_t *
$define %func dma_mem_free as procedure with args dma_mem_t *
$define %func dma_map_create as function with args dma_tag_t, u32, dma_map_t *
$define %func dma_map_destroy as procedure with args dma_map_t *
$define %func dma_map_load as function with args dma_map_t *, void *, u64, u32
$define %func dma_map_unload as procedure with args dma_map_t *
$define %func dma_map_segs as function with args const dma_map_t *, u32 *
$define %func dma_sync as procedure with args dma_map_t *, u32
$define %func dma_stats as procedure with args dma_stat_t *
$define %func dma_dump as procedure with args void

$const DMA_HIGHADDR_ANY as value meaning no upper physical address bound
$const DMA_BOUNDARY_NONE as value meaning segments may cross any boundary
$const DMA_SEGSZ_MAX as value meaning no per-segment size cap
$const DMA_MAX_SEGMENTS as hard ceiling on segments per map

*/

/* !SPACE!

$space %export dma_init, dma_is_initialized
$space %export dma_tag_root, dma_tag_create, dma_tag_destroy, dma_tag_name
$space %export dma_mem_alloc, dma_mem_free
$space %export dma_map_create, dma_map_destroy
$space %export dma_map_load, dma_map_unload, dma_map_segs
$space %export dma_sync
$space %export dma_stats, dma_dump

*/

#ifndef MM_DMA_DMA_H
#define MM_DMA_DMA_H

#include <mlibc/mlibc.h>


#define	DMA_HIGHADDR_ANY	0ULL
#define	DMA_BOUNDARY_NONE	0ULL
#define	DMA_SEGSZ_MAX		0ULL
#define	DMA_MAX_SEGMENTS	1024
#define	DMA_F_NOWAIT		0x0001
#define	DMA_F_READ		0x0010
#define	DMA_F_WRITE		0x0020
#define	DMA_SYNC_PREREAD	0x01
#define	DMA_SYNC_POSTREAD	0x02
#define	DMA_SYNC_PREWRITE	0x04
#define	DMA_SYNC_POSTWRITE	0x08

struct dma_tag;
struct dma_bounce_slot;

typedef struct dma_tag		*dma_tag_t;

typedef struct dma_seg {
	u64	phys;
	u64	len;
} dma_seg_t;

typedef struct dma_mem {
	void		*virt;
	u64		phys;
	u64		size;
	dma_tag_t	tag;
	u32		pages;
	u32		pad;
} dma_mem_t;

typedef struct dma_map {
	dma_tag_t			tag;
	dma_seg_t			*segs;
	struct dma_bounce_slot		*bounce;
	void				*buf;
	u64				len;
	u32				nsegs;
	u32				maxsegs;
	u32				nbounce;
	u32				maxbounce;
	u32				flags;
	u32				pad;
} dma_map_t;

typedef struct dma_stat {
	u64	tags_live;
	u64	tags_peak;
	u64	mem_allocs;
	u64	mem_frees;
	u64	mem_bytes_inuse;
	u64	map_loads;
	u64	map_unloads;
	u64	bounced_loads;
	u64	bounce_pages_inuse;
	u64	bounce_pages_peak;
	u64	bounce_pages_total;
	u64	fail_nomem;
	u64	fail_nsegs;
	u64	fail_bounce;
	u64	fail_constraint;
	u64	fail_unmapped;
} dma_stat_t;

void		dma_init(void);
int		dma_is_initialized(void);

dma_tag_t	dma_tag_root(void);
int		dma_tag_create(dma_tag_t parent, u64 alignment, u64 boundary,
		    u64 lowaddr, u64 highaddr, u64 maxsize, u32 nsegments,
		    u64 maxsegsz, u32 flags, const char *name,
		    dma_tag_t *tagp);
void		dma_tag_destroy(dma_tag_t tag);
const char	*dma_tag_name(dma_tag_t tag);

int		dma_mem_alloc(dma_tag_t tag, u64 size, u32 flags,
		    dma_mem_t *mem);
void		dma_mem_free(dma_mem_t *mem);

int		dma_map_create(dma_tag_t tag, u32 flags, dma_map_t *map);
void		dma_map_destroy(dma_map_t *map);
int		dma_map_load(dma_map_t *map, void *buf, u64 len, u32 flags);
void		dma_map_unload(dma_map_t *map);
const dma_seg_t	*dma_map_segs(const dma_map_t *map, u32 *nsegs);

void		dma_sync(dma_map_t *map, u32 op);

void		dma_stats(dma_stat_t *out);
void		dma_dump(void);

#endif
