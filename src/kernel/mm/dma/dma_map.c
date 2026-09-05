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
$define %type dma_map_t as struct with one loaded buffer and its segments
$define %type dma_seg_t as struct with one physical scatter/gather segment

$define %func dma_seg_crosses as function with args u64, u64, u64
$define %func dma_seg_append as function with args dma_map_t *, u64, u64
$define %func dma_map_release_bounce as procedure with args dma_map_t *
$define %func dma_map_create as function with args dma_tag_t, u32, dma_map_t *
$define %func dma_map_destroy as procedure with args dma_map_t *
$define %func dma_map_load as function with args dma_map_t *, void *, u64, u32
$define %func dma_map_unload as procedure with args dma_map_t *
$define %func dma_map_segs as function with args const dma_map_t *, u32 *

*/

/* !SPACE!

$space %internal dma_seg_crosses, dma_seg_append, dma_map_release_bounce
$space %export dma_map_create, dma_map_destroy
$space %export dma_map_load, dma_map_unload, dma_map_segs

*/

#include <mm/dma/dma_internal.h>
#include <mm/vm/pmap.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

static int
dma_seg_crosses(u64 phys, u64 len, u64 boundary)
{
	if (boundary == DMA_BOUNDARY_NONE || len == 0) {
		return (0);
	}
	return ((phys & ~(boundary - 1)) !=
	    ((phys + len - 1) & ~(boundary - 1)));
}


static int
dma_seg_append(dma_map_t *map, u64 phys, u64 len)
{
	dma_seg_t	*last;
	u64		piece, room, merged;

	while (len != 0) {
		piece = len;
		if (piece > map->tag->maxsegsz) {
			piece = map->tag->maxsegsz;
		}
		if (map->tag->boundary != DMA_BOUNDARY_NONE) {
			room = map->tag->boundary -
			    (phys & (map->tag->boundary - 1));
			if (room < piece) {
				piece = room;
			}
		}

		last = map->nsegs != 0 ? &map->segs[map->nsegs - 1] : NULL;
		if (last != NULL && last->phys + last->len == phys) {
			merged = last->len + piece;
			if (merged <= map->tag->maxsegsz &&
			    !dma_seg_crosses(last->phys, merged,
			    map->tag->boundary)) {
				last->len = merged;
				phys += piece;
				len -= piece;
				continue;
			}
		}

		if (map->nsegs >= map->maxsegs) {
			return (-1);
		}
		map->segs[map->nsegs].phys = phys;
		map->segs[map->nsegs].len = piece;
		map->nsegs++;
		phys += piece;
		len -= piece;
	}
	return (0);
}

static void
dma_map_release_bounce(dma_map_t *map)
{
	u32	i;

	if (map->bounce == NULL) {
		return;
	}
	for (i = 0; i < map->nbounce; i++) {
		if (map->bounce[i].page != NULL) {
			dma_bounce_put(map->bounce[i].page);
			map->bounce[i].page = NULL;
		}
		map->bounce[i].orig = NULL;
		map->bounce[i].len = 0;
	}
	map->nbounce = 0;
}

int
dma_map_create(dma_tag_t tag, u32 flags, dma_map_t *map)
{
	if (map == NULL) {
		return (-1);
	}
	memset(map, 0, sizeof(*map));
	if (tag == NULL || !dma_g.ready || !tag->live) {
		return (-1);
	}

	map->segs = kmem_calloc(tag->nsegments, sizeof(dma_seg_t));
	if (map->segs == NULL) {
		__atomic_fetch_add(&dma_g.fail_nomem, 1, __ATOMIC_RELAXED);
		return (-1);
	}
	map->bounce = kmem_calloc(tag->nsegments, sizeof(dma_bounce_slot_t));
	if (map->bounce == NULL) {
		kmem_free(map->segs);
		map->segs = NULL;
		__atomic_fetch_add(&dma_g.fail_nomem, 1, __ATOMIC_RELAXED);
		return (-1);
	}

	map->tag = tag;
	map->maxsegs = tag->nsegments;
	map->maxbounce = tag->nsegments;
	map->flags = flags;
	return (0);
}

void
dma_map_destroy(dma_map_t *map)
{
	if (map == NULL) {
		return;
	}
	dma_map_unload(map);
	if (map->segs != NULL) {
		kmem_free(map->segs);
	}
	if (map->bounce != NULL) {
		kmem_free(map->bounce);
	}
	memset(map, 0, sizeof(*map));
}

int
dma_map_load(dma_map_t *map, void *buf, u64 len, u32 flags)
{
	dma_bounce_page_t	*page;
	dma_tag_t		tag;
	u64			va, phys, left, chunk;
	int			first;

	if (map == NULL || map->segs == NULL || map->tag == NULL) {
		return (-1);
	}
	if (map->nsegs != 0) {
		
		drivers_log("[DMA] %s: load on an already loaded map\n",
		    map->tag->name);
		return (-1);
	}
	tag = map->tag;
	if (buf == NULL || len == 0) {
		return (-1);
	}
	if (len > tag->maxsize) {
		__atomic_fetch_add(&dma_g.fail_constraint, 1,
		    __ATOMIC_RELAXED);
		drivers_log("[DMA] %s: load of %llu exceeds maxsize %llu\n",
		    tag->name, (unsigned long long)len,
		    (unsigned long long)tag->maxsize);
		return (-1);
	}
	if ((flags & (DMA_F_READ | DMA_F_WRITE)) == 0) {

		__atomic_fetch_add(&dma_g.fail_constraint, 1,
		    __ATOMIC_RELAXED);
		drivers_log("[DMA] %s: load without DMA_F_READ/DMA_F_WRITE\n",
		    tag->name);
		return (-1);
	}

	va = (u64)buf;
	left = len;
	first = 1;

	while (left != 0) {
		chunk = PAGE_SIZE - (va & PAGE_MASK);
		if (chunk > left) {
			chunk = left;
		}

		phys = pmap_extract(va & ~PAGE_MASK);
		if (phys == 0) {
			__atomic_fetch_add(&dma_g.fail_unmapped, 1,
			    __ATOMIC_RELAXED);
			drivers_log("[DMA] %s: unmapped va 0x%llx in load\n",
			    tag->name, (unsigned long long)va);
			goto fail;
		}
		phys |= va & PAGE_MASK;

		if (!dma_addr_ok(tag, phys, chunk) ||
		    (first && (phys & (tag->alignment - 1)) != 0)) {
			if (tag->alignment > PAGE_SIZE) {
				__atomic_fetch_add(&dma_g.fail_constraint, 1,
				    __ATOMIC_RELAXED);
				drivers_log("[DMA] %s: alignment %llu cannot "
				    "be met by a bounce page\n", tag->name,
				    (unsigned long long)tag->alignment);
				goto fail;
			}
			if (map->nbounce >= map->maxbounce) {
				__atomic_fetch_add(&dma_g.fail_nsegs, 1,
				    __ATOMIC_RELAXED);
				goto fail;
			}
			page = dma_bounce_get(tag->lowaddr, tag->highaddr);
			if (page == NULL) {
				drivers_log("[DMA] %s: bounce pool exhausted "
				    "(%llu pages, %llu in use)\n", tag->name,
				    (unsigned long long)dma_g.pool_pages,
				    (unsigned long long)dma_g.pool_inuse);
				goto fail;
			}
			map->bounce[map->nbounce].page = page;
			map->bounce[map->nbounce].orig = (void *)va;
			map->bounce[map->nbounce].len = chunk;
			map->nbounce++;
			phys = page->phys;
		}

		if (dma_seg_append(map, phys, chunk) != 0) {
			__atomic_fetch_add(&dma_g.fail_nsegs, 1,
			    __ATOMIC_RELAXED);
			drivers_log("[DMA] %s: buffer needs more than %u "
			    "segments\n", tag->name, map->maxsegs);
			goto fail;
		}

		va += chunk;
		left -= chunk;
		first = 0;
	}

	map->buf = buf;
	map->len = len;
	map->flags = flags;

	__atomic_fetch_add(&dma_g.map_loads, 1, __ATOMIC_RELAXED);
	if (map->nbounce != 0) {
		__atomic_fetch_add(&dma_g.bounced_loads, 1, __ATOMIC_RELAXED);
	}
	return (0);

fail:
	dma_map_release_bounce(map);
	map->nsegs = 0;
	map->buf = NULL;
	map->len = 0;
	return (-1);
}

void
dma_map_unload(dma_map_t *map)
{
	if (map == NULL || map->segs == NULL) {
		return;
	}
	if (map->nsegs == 0 && map->nbounce == 0) {
		return;
	}
	dma_map_release_bounce(map);
	map->nsegs = 0;
	map->buf = NULL;
	map->len = 0;
	__atomic_fetch_add(&dma_g.map_unloads, 1, __ATOMIC_RELAXED);
}

const dma_seg_t *
dma_map_segs(const dma_map_t *map, u32 *nsegs)
{
	if (map == NULL || map->segs == NULL || map->nsegs == 0) {
		if (nsegs != NULL) {
			*nsegs = 0;
		}
		return (NULL);
	}
	if (nsegs != NULL) {
		*nsegs = map->nsegs;
	}
	return (map->segs);
}
