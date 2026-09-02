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
$define %type size_t as unsigned long
$define %type uma_zone_t as opaque pointer to a slab zone
$define %type uma_stat_t as struct with the per-zone counters

$define %func uma_init as procedure with args void
$define %func uma_zcreate as function with args const char *, size_t, size_t, u32
$define %func uma_zdestroy as function with args uma_zone_t
$define %func uma_zalloc as function with args uma_zone_t, u32
$define %func uma_zfree as procedure with args uma_zone_t, void *
$define %func uma_zfind as function with args const char *
$define %func uma_zone_item_size as function with args uma_zone_t
$define %func uma_zone_stats as function with args uma_zone_t, uma_stat_t *
$define %func uma_reclaim as function with args void
$define %func uma_dump as procedure with args void

$const UMA_ALIGN_CACHE as alignment request meaning one cache line
$const UMA_ITEM_MAX as largest item a zone can serve
$const M_WAITOK as blocking allocation request
$const M_NOWAIT as non blocking allocation request
$const M_ZERO as request for zeroed memory
$const UMA_DESTROY_OK as successful zone destruction
$const UMA_DESTROY_BUSY as destruction refused because the zone is not quiescent

*/

/* !SPACE!

$space %export uma_init, uma_zcreate, uma_zdestroy
$space %export uma_zalloc, uma_zfree, uma_zfind
$space %export uma_zone_item_size, uma_zone_stats
$space %export uma_reclaim, uma_dump

*/

#ifndef UMA_H
#define UMA_H

#include <mlibc/mlibc.h>
#define UMA_SLAB_MAX_ORDER	3
#define UMA_SLAB_MIN_ITEMS	4
#define UMA_PAGE_SIZE		4096
#define UMA_CACHE_ITEMS		6

#define UMA_ZONE_MAX		64
#define UMA_NAME_LEN		24
#define UMA_ALIGN_CACHE		64
#define UMA_ITEM_MIN		8
#define UMA_SLAB_HDR		64

#define UMA_ITEM_MAX		((((size_t)1 << UMA_SLAB_MAX_ORDER) * \
				    UMA_PAGE_SIZE - UMA_SLAB_HDR) / \
				    UMA_SLAB_MIN_ITEMS)

#define M_WAITOK		0x0000
#define M_NOWAIT		0x0001
#define M_ZERO			0x0100

#define UMA_DESTROY_OK		0
#define UMA_DESTROY_BUSY		-1

typedef struct uma_zone *uma_zone_t;

typedef struct uma_stat {
	u64	allocs;
	u64	frees;
	u64	slabs;
	u64	items_total;
	u64	items_free;
	u64	item_size;
	u64	fails;
} uma_stat_t;

void		uma_init(void);
uma_zone_t	uma_zcreate(const char *name, size_t size, size_t align,
		    u32 flags);
int		uma_zdestroy(uma_zone_t zone);
void		*uma_zalloc(uma_zone_t zone, u32 flags);
void		uma_zfree(uma_zone_t zone, void *item);
uma_zone_t	uma_zfind(const char *name);
size_t		uma_zone_item_size(uma_zone_t zone);
int		uma_zone_stats(uma_zone_t zone, uma_stat_t *out);
u64		uma_reclaim(void);
void		uma_dump(void);

#endif
