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
$define %type dma_global_t as struct with subsystem lock, pool and counters
$define %type dma_stat_t as struct with framework accounting counters

$define %func dma_cm_update as function with args u32
$define %func dma_init as procedure with args void
$define %func dma_is_initialized as function with args void
$define %func dma_stats as procedure with args dma_stat_t *
$define %func dma_dump as procedure with args void

$const DMA_BOUNCE_MAX_KB_LIMIT as hard ceiling on the configurable pool size

*/

/* !SPACE!

$space %internal dma_cm_update
$space %export dma_init, dma_is_initialized, dma_stats, dma_dump

*/

#include <kernel/cm/cm.h>
#include <mm/dma/dma_internal.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define	DMA_BOUNCE_MAX_KB_LIMIT		65536

dma_global_t	dma_g = {
	.lock = SPIN_INITIALIZER("dma", LO_DMA),
};

static int
dma_cm_update(u32 flags)
{
	u32	reserve_kb, max_kb, verbose;

	(void)flags;

	reserve_kb = cm_get_u32_default("SYSTEM", "Dma", "BounceReserveKb",
	    DMA_BOUNCE_RESERVE_KB_DEFAULT);
	max_kb = cm_get_u32_default("SYSTEM", "Dma", "BounceMaxKb",
	    DMA_BOUNCE_MAX_KB_DEFAULT);
	verbose = (u32)cm_get_bool_default("SYSTEM", "Dma", "Verbose", 0);

	if (max_kb > DMA_BOUNCE_MAX_KB_LIMIT) {
		max_kb = DMA_BOUNCE_MAX_KB_LIMIT;
	}
	if (max_kb == 0) {
		max_kb = DMA_BOUNCE_MAX_KB_DEFAULT;
	}
	if (reserve_kb > max_kb) {
		reserve_kb = max_kb;
	}

	dma_bounce_configure(reserve_kb, max_kb);

	if (verbose != 0) {
		drivers_log("[DMA] bounce pool: %llu pages, ceiling %llu "
		    "pages\n", (unsigned long long)dma_g.pool_pages,
		    (unsigned long long)dma_g.pool_max_pages);
	}
	return (0);
}

void
dma_init(void)
{
	if (dma_g.ready) {
		return;
	}

	if (dma_tag_root_create() != 0) {
		printk("dma: cannot create root tag\n");
		return;
	}
	dma_g.pool_max_pages =
	    ((u64)DMA_BOUNCE_MAX_KB_DEFAULT * 1024) >> PAGE_SHIFT;

	__atomic_store_n(&dma_g.ready, 1, __ATOMIC_RELEASE);

	dma_bounce_startup(DMA_BOUNCE_RESERVE_KB_DEFAULT);


	(void)cm_register_consumer(CM_CONSUMER_DMA, "dma", dma_cm_update);
}

int
dma_is_initialized(void)
{
	return (__atomic_load_n(&dma_g.ready, __ATOMIC_ACQUIRE) != 0);
}

void
dma_stats(dma_stat_t *out)
{
	if (out == NULL) {
		return;
	}
	memset(out, 0, sizeof(*out));

	spin_lock(&dma_g.lock);
	out->tags_live = dma_g.tags_live;
	out->bounce_pages_total = dma_g.pool_pages;
	out->bounce_pages_inuse = dma_g.pool_inuse;
	spin_unlock(&dma_g.lock);

	out->tags_peak = __atomic_load_n(&dma_g.tags_peak, __ATOMIC_RELAXED);
	out->mem_allocs = __atomic_load_n(&dma_g.mem_allocs,
	    __ATOMIC_RELAXED);
	out->mem_frees = __atomic_load_n(&dma_g.mem_frees, __ATOMIC_RELAXED);
	out->mem_bytes_inuse = __atomic_load_n(&dma_g.mem_bytes,
	    __ATOMIC_RELAXED);
	out->map_loads = __atomic_load_n(&dma_g.map_loads, __ATOMIC_RELAXED);
	out->map_unloads = __atomic_load_n(&dma_g.map_unloads,
	    __ATOMIC_RELAXED);
	out->bounced_loads = __atomic_load_n(&dma_g.bounced_loads,
	    __ATOMIC_RELAXED);
	out->bounce_pages_peak = __atomic_load_n(&dma_g.pool_peak,
	    __ATOMIC_RELAXED);
	out->fail_nomem = __atomic_load_n(&dma_g.fail_nomem,
	    __ATOMIC_RELAXED);
	out->fail_nsegs = __atomic_load_n(&dma_g.fail_nsegs,
	    __ATOMIC_RELAXED);
	out->fail_bounce = __atomic_load_n(&dma_g.fail_bounce,
	    __ATOMIC_RELAXED);
	out->fail_constraint = __atomic_load_n(&dma_g.fail_constraint,
	    __ATOMIC_RELAXED);
	out->fail_unmapped = __atomic_load_n(&dma_g.fail_unmapped,
	    __ATOMIC_RELAXED);
}

void
dma_dump(void)
{
	dma_stat_t	st;

	dma_stats(&st);

	printk("dma: tags %u (peak %u) coherent %u bytes in %u allocs\n",
	    (u32)st.tags_live, (u32)st.tags_peak, (u32)st.mem_bytes_inuse,
	    (u32)(st.mem_allocs - st.mem_frees));
	printk("dma: loads %u unloads %u bounced %u\n",
	    (u32)st.map_loads, (u32)st.map_unloads, (u32)st.bounced_loads);
	printk("dma: bounce pool %u pages, %u in use, peak %u\n",
	    (u32)st.bounce_pages_total, (u32)st.bounce_pages_inuse,
	    (u32)st.bounce_pages_peak);

	printk("dma: fail nomem %u nsegs %u bounce %u constraint %u "
	    "unmapped %u\n", (u32)st.fail_nomem, (u32)st.fail_nsegs,
	    (u32)st.fail_bounce, (u32)st.fail_constraint,
	    (u32)st.fail_unmapped);
}
