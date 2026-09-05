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
$define %type dma_map_t as struct with one loaded buffer and its segments

$define %func dma_sync as procedure with args dma_map_t *, u32

*/

/* !SPACE!

$space %export dma_sync

*/

#include <mm/dma/dma_internal.h>
#include <mlibc/mlibc.h>


void
dma_sync(dma_map_t *map, u32 op)
{
	if (map == NULL || map->segs == NULL || map->nsegs == 0) {
		return;
	}

	if ((op & DMA_SYNC_PREWRITE) != 0) {
		if ((map->flags & DMA_F_WRITE) != 0) {
			dma_bounce_copy_in(map);
		}
		__asm__ volatile("sfence" ::: "memory");
	}

	if ((op & DMA_SYNC_PREREAD) != 0) {
		__asm__ volatile("mfence" ::: "memory");
	}

	if ((op & DMA_SYNC_POSTREAD) != 0) {
		__asm__ volatile("lfence" ::: "memory");
		if ((map->flags & DMA_F_READ) != 0) {
			dma_bounce_copy_out(map);
		}
	}

	if ((op & DMA_SYNC_POSTWRITE) != 0) {
		__asm__ volatile("" ::: "memory");
	}
}
