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
$define %type size_t as unsigned long
$define %type uma_slab_t as slab header embedded at an aligned slab base
$define %type uma_cache_t as per CPU cache for one slab zone
$define %type uma_zone_t as opaque pointer to a slab zone

$define %func uma_intr_save as function with args void
$define %func uma_intr_restore as procedure with args u64
$define %func uma_align_ok as function with args size_t
$define %func uma_slab_of as function with args uma_zone_t, void *
$define %func uma_slab_create as function with args uma_zone_t
$define %func uma_slab_pop as function with args uma_zone_t, uma_slab_t *
$define %func uma_slab_push as procedure with args uma_zone_t, uma_slab_t *, void *
$define %func uma_slab_release as procedure with args uma_zone_t, uma_slab_t *
$define %func uma_refill as function with args uma_zone_t, void **, u32
$define %func uma_drain_items as procedure with args uma_zone_t, void **, u32
$define %func uma_cache_of as function with args uma_zone_t
$define %func uma_zone_geometry as function with args uma_zone_t, size_t
$define %func uma_zone_bootstrap as function with args void
$define %func uma_zone_drain_local as procedure with args uma_zone_t
$define %func uma_zone_empty as function with args uma_zone_t
$define %func uma_registry_remove as procedure with args uma_zone_t
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

*/

/* !SPACE!

$space %internal uma_intr_save, uma_intr_restore, uma_align_ok, uma_slab_of
$space %internal uma_slab_create, uma_slab_pop, uma_slab_push, uma_slab_release
$space %internal uma_refill, uma_drain_items, uma_cache_of, uma_zone_geometry
$space %internal uma_zone_bootstrap, uma_zone_drain_local, uma_zone_empty
$space %internal uma_registry_remove
$space %export uma_init, uma_zcreate, uma_zdestroy, uma_zalloc, uma_zfree
$space %export uma_zfind, uma_zone_item_size, uma_zone_stats, uma_reclaim, uma_dump

*/

#include <mm/uma.h>
#include <mm/vm/vm_page.h>
#include <kernel/smp/pcpu.h>
#include <kernel/smp/smp.h>
#include <kernel/sync/sync.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define UMA_MAGIC		0x554D41534C414231ULL
#define UMA_REFILL		(UMA_CACHE_ITEMS / 2 + 1)
#define UMA_ALIGN_UP(v, a)	(((v) + (a) - 1) & ~((a) - 1))

typedef struct uma_slab uma_slab_t;

typedef struct uma_cache {
	void	*items[UMA_CACHE_ITEMS];
	u32	 count;
	u32	 pad[3];
} __attribute__((aligned(64))) uma_cache_t;

struct uma_slab {
	u64		 magic;
	uma_zone_t	 zone;
	void		*free_head;
	uma_slab_t	*next;
	uma_slab_t	*prev;
	u64		 phys;
	u32		 items_total;
	u32		 items_free;
};

_Static_assert(sizeof(struct uma_slab) <= UMA_SLAB_HDR,
    "uma slab header exceeds its fixed reservation");

struct uma_zone {
	char		 name[UMA_NAME_LEN];
	size_t		 item_size;
	size_t		 alignment;
	u32		 flags;
	int		 order;
	u64		 slab_bytes;
	u32		 items_per_slab;
	uma_slab_t	*partial;
	uma_slab_t	*full;
	u64		 allocs;
	u64		 frees;
	u64		 fails;
	u64		 slab_count;
	spin_t		 spin;
	int		 alive;
	uma_zone_t	 registry_next;
	uma_cache_t	 cache[PCPU_MAX_CPUS];
};

static spin_t		uma_registry_spin =
			    SPIN_INITIALIZER("uma", LO_UMA);
static uma_zone_t	uma_zones;
static u32		uma_zone_count;
static int		uma_ready;
static struct uma_zone	uma_zone_store;

static u64
uma_intr_save(void)
{
	u64	flags;

	__asm__ volatile("pushfq; pop %0; cli" : "=r"(flags) :: "memory");
	return (flags);
}

static void
uma_intr_restore(u64 flags)
{
	__asm__ volatile("push %0; popfq" :: "r"(flags) : "memory", "cc");
}

static int
uma_align_ok(size_t align)
{
	return (align != 0 && (align & (align - 1)) == 0);
}

static uma_slab_t *
uma_slab_of(uma_zone_t zone, void *item)
{
	return ((uma_slab_t *)((u64)item & ~((u64)zone->slab_bytes - 1)));
}

static uma_slab_t *
uma_slab_create(uma_zone_t zone)
{
	uma_slab_t	*slab;
	void		*base;
	void		*item;
	u64		 phys;
	u64		 i;

	phys = vm_page_alloc_contig((u32)(zone->slab_bytes / UMA_PAGE_SIZE),
	    zone->slab_bytes, 0);
	if (phys == 0) {
		return (NULL);
	}
	base = (void *)(phys + DMAP_BASE);
	slab = (uma_slab_t *)base;
	if (sizeof(*slab) > UMA_SLAB_HDR) {
		vm_page_free_contig(phys,
		    (u32)(zone->slab_bytes / UMA_PAGE_SIZE));
		return (NULL);
	}
	memset(slab, 0, UMA_SLAB_HDR);
	slab->magic = UMA_MAGIC;
	slab->zone = zone;
	slab->phys = phys;
	slab->items_total = zone->items_per_slab;
	slab->items_free = slab->items_total;
	for (i = slab->items_total; i > 0; i--) {
		item = (void *)((u8 *)base + UMA_SLAB_HDR +
		    (i - 1) * zone->item_size);
		*(void **)item = slab->free_head;
		slab->free_head = item;
	}
	slab->next = zone->partial;
	if (zone->partial != NULL) {
		zone->partial->prev = slab;
	}
	zone->partial = slab;
	zone->slab_count++;
	return (slab);
}

static void *
uma_slab_pop(uma_zone_t zone, uma_slab_t *slab)
{
	void	*item;

	if (slab == NULL || slab->items_free == 0) {
		return (NULL);
	}
	item = slab->free_head;
	slab->free_head = *(void **)item;
	slab->items_free--;
	if (slab->items_free == 0) {
		if (slab->prev != NULL) {
			slab->prev->next = slab->next;
		} else {
			zone->partial = slab->next;
		}
		if (slab->next != NULL) {
			slab->next->prev = slab->prev;
		}
		slab->prev = NULL;
		slab->next = zone->full;
	if (zone->full != NULL) {
			zone->full->prev = slab;
		}
		zone->full = slab;
	}
	return (item);
}

/* zone lock held */
static void
uma_slab_push(uma_zone_t zone, uma_slab_t *slab, void *item)
{
	if (slab->items_free == 0) {
		if (slab->prev != NULL) {
			slab->prev->next = slab->next;
		} else {
			zone->full = slab->next;
		}
		if (slab->next != NULL) {
			slab->next->prev = slab->prev;
		}
		slab->prev = NULL;
		slab->next = zone->partial;
		if (zone->partial != NULL) {
			zone->partial->prev = slab;
		}
		zone->partial = slab;
	}
	*(void **)item = slab->free_head;
	slab->free_head = item;
	slab->items_free++;
}


static void
uma_slab_release(uma_zone_t zone, uma_slab_t *slab)
{
	if (slab->prev != NULL) {
		slab->prev->next = slab->next;
	} else if (zone->partial == slab) {
		zone->partial = slab->next;
	} else {
		zone->full = slab->next;
	}
	if (slab->next != NULL) {
		slab->next->prev = slab->prev;
	}
	if (zone->slab_count > 0) {
		zone->slab_count--;
	}
	slab->magic = 0;
	vm_page_free_contig(slab->phys,
	    (u32)(zone->slab_bytes / UMA_PAGE_SIZE));
}

static u32
uma_refill(uma_zone_t zone, void **items, u32 want)
{
	uma_slab_t	*slab;
	u32		 n;

	n = 0;
	spin_lock(&zone->spin);
	while (n < want) {
		slab = zone->partial;
		if (slab == NULL) {
			slab = uma_slab_create(zone);
			if (slab == NULL) {
				zone->fails++;
				break;
			}
		}
		items[n] = uma_slab_pop(zone, slab);
		if (items[n] == NULL) {
			break;
		}
		n++;
	}
	spin_unlock(&zone->spin);
	return (n);
}

static void
uma_drain_items(uma_zone_t zone, void **items, u32 count)
{
	u32	i;

	spin_lock(&zone->spin);
	for (i = 0; i < count; i++) {
		uma_slab_push(zone, uma_slab_of(zone, items[i]), items[i]);
	}
	spin_unlock(&zone->spin);
}

static uma_cache_t *
uma_cache_of(uma_zone_t zone)
{
	u32	cpu;

	cpu = pcpu_current()->cpu_index;
	if (cpu >= PCPU_MAX_CPUS) {
		return (NULL);
	}
	return (&zone->cache[cpu]);
}

static int
uma_zone_geometry(uma_zone_t zone, size_t item_size)
{
	u64	bytes;
	int	order;

	bytes = UMA_PAGE_SIZE;
	order = 0;
	while ((bytes - UMA_SLAB_HDR) / item_size < UMA_SLAB_MIN_ITEMS &&
	    order < UMA_SLAB_MAX_ORDER) {
		bytes <<= 1;
		order++;
	}
	if ((bytes - UMA_SLAB_HDR) / item_size < UMA_SLAB_MIN_ITEMS) {
		return (-1);
	}
	zone->order = order;
	zone->slab_bytes = bytes;
	zone->items_per_slab = (u32)((bytes - UMA_SLAB_HDR) / item_size);
	return (0);
}

static int
uma_zone_bootstrap(void)
{
	uma_zone_t	zone;

	zone = &uma_zone_store;
	memset(zone, 0, sizeof(*zone));
	memcpy(zone->name, "uma_zone", 9);
	zone->item_size = UMA_ALIGN_UP(sizeof(*zone), UMA_ALIGN_CACHE);
	zone->alignment = UMA_ALIGN_CACHE;
	if (uma_zone_geometry(zone, zone->item_size) != 0) {
		return (-1);
	}
	spin_init(&zone->spin, zone->name, LO_UMA_ZONE);
	zone->alive = 1;
	return (0);
}


void
uma_init(void)
{
	spin_lock(&uma_registry_spin);
	uma_zones = NULL;
	uma_zone_count = 0;
	uma_ready = (uma_zone_bootstrap() == 0);
	spin_unlock(&uma_registry_spin);
}

uma_zone_t
uma_zcreate(const char *name, size_t size, size_t align, u32 flags)
{
	uma_zone_t	zone;
	size_t		item_size;
	int		i;

	if (!uma_ready || size < UMA_ITEM_MIN || !uma_align_ok(align)) {
		return (NULL);
	}
	if (align < sizeof(void *)) {
		align = sizeof(void *);
	}
	item_size = UMA_ALIGN_UP(size, align);
	if (item_size < size || item_size > UMA_ITEM_MAX) {
		return (NULL);
	}

	zone = (uma_zone_t)uma_zalloc(&uma_zone_store, M_ZERO);
	if (zone == NULL) {
		return (NULL);
	}

	memset(zone, 0, sizeof(*zone));
	if (name != NULL) {
		for (i = 0; i < UMA_NAME_LEN - 1 && name[i] != '\0'; i++) {
			zone->name[i] = name[i];
		}
	}
	zone->item_size = item_size;
	zone->alignment = align;
	zone->flags = flags;
	if (uma_zone_geometry(zone, item_size) != 0) {
		uma_zfree(&uma_zone_store, zone);
		return (NULL);
	}
	spin_init(&zone->spin, zone->name, LO_UMA_ZONE);
	zone->alive = 1;

	spin_lock(&uma_registry_spin);
	if (uma_zone_count >= UMA_ZONE_MAX) {
		spin_unlock(&uma_registry_spin);
		uma_zfree(&uma_zone_store, zone);
		return (NULL);
	}
	zone->registry_next = uma_zones;
	uma_zones = zone;
	uma_zone_count++;
	spin_unlock(&uma_registry_spin);
	return (zone);
}

void *
uma_zalloc(uma_zone_t zone, u32 flags)
{
	uma_cache_t	*cache;
	void		*items[UMA_REFILL];
	void		*item;
	u64		 irq_flags;
	u32		 got;
	u32		 i;

	if (zone == NULL || !zone->alive) {
		return (NULL);
	}

	irq_flags = uma_intr_save();
	cache = uma_cache_of(zone);
	if (cache == NULL) {
		uma_intr_restore(irq_flags);
		return (NULL);
	}
	if (cache->count != 0) {
		item = cache->items[--cache->count];
		uma_intr_restore(irq_flags);
	} else {
		uma_intr_restore(irq_flags);
		got = uma_refill(zone, items, UMA_REFILL);
		if (got == 0) {
			return (NULL);
		}
		item = items[--got];
		irq_flags = uma_intr_save();
		cache = uma_cache_of(zone);
		if (cache == NULL) {
			uma_intr_restore(irq_flags);
			uma_drain_items(zone, items, got);
			uma_drain_items(zone, &item, 1);
			return (NULL);
		}
		for (i = 0; i < got; i++) {
			cache->items[cache->count++] = items[i];
		}
		uma_intr_restore(irq_flags);
	}

	if ((flags & M_ZERO) != 0 || (zone->flags & M_ZERO) != 0) {
		memset(item, 0, zone->item_size);
	}
	__atomic_fetch_add(&zone->allocs, 1, __ATOMIC_RELAXED);
	return (item);
}

void
uma_zfree(uma_zone_t zone, void *item)
{
	uma_cache_t	*cache;
	uma_slab_t	*slab;
	void		*drain[UMA_REFILL];
	u64		 offset;
	u64		 irq_flags;
	u32		 n;

	if (zone == NULL || item == NULL || !zone->alive) {
		return;
	}
	slab = uma_slab_of(zone, item);
	if (slab->magic != UMA_MAGIC || slab->zone != zone) {
		return;
	}
	offset = (u64)item - (u64)slab;
	if (offset < UMA_SLAB_HDR ||
	    offset - UMA_SLAB_HDR >= (u64)slab->items_total * zone->item_size ||
	    (offset - UMA_SLAB_HDR) % zone->item_size != 0) {
		return;
	}

	irq_flags = uma_intr_save();
	cache = uma_cache_of(zone);
	if (cache == NULL) {
		uma_intr_restore(irq_flags);
		uma_drain_items(zone, &item, 1);
		return;
	}
	if (cache->count < UMA_CACHE_ITEMS) {
		cache->items[cache->count++] = item;
		uma_intr_restore(irq_flags);
	} else {
		n = UMA_REFILL - 1;
		while (n > 0) {
			drain[n - 1] = cache->items[--cache->count];
			n--;
		}
		drain[UMA_REFILL - 1] = item;
		uma_intr_restore(irq_flags);
		uma_drain_items(zone, drain, UMA_REFILL);
	}
	__atomic_fetch_add(&zone->frees, 1, __ATOMIC_RELAXED);
}

static void
uma_zone_drain_local(uma_zone_t zone)
{
	uma_cache_t	*cache;
	void		*drain[UMA_CACHE_ITEMS];
	u64		 irq_flags;
	u32		 n;

	irq_flags = uma_intr_save();
	cache = uma_cache_of(zone);
	if (cache == NULL) {
		uma_intr_restore(irq_flags);
		return;
	}
	n = cache->count;
	memcpy(drain, cache->items, n * sizeof(void *));
	cache->count = 0;
	uma_intr_restore(irq_flags);
	uma_drain_items(zone, drain, n);
}

static int
uma_zone_empty(uma_zone_t zone)
{
	uma_slab_t	*slab;

	for (slab = zone->partial; slab != NULL; slab = slab->next) {
		if (slab->items_free != slab->items_total) {
			return (0);
		}
	}
	if (zone->full != NULL) {
		return (0);
	}
	return (1);
}

static void
uma_registry_remove(uma_zone_t zone)
{
	uma_zone_t	prev;

	if (uma_zones == zone) {
		uma_zones = zone->registry_next;
	} else {
		for (prev = uma_zones; prev != NULL; prev = prev->registry_next) {
			if (prev->registry_next == zone) {
				prev->registry_next = zone->registry_next;
				break;
			}
		}
	}
	zone->registry_next = NULL;
	if (uma_zone_count > 0) {
		uma_zone_count--;
	}
}

int
uma_zdestroy(uma_zone_t zone)
{
	uma_slab_t	*slab;
	uma_slab_t	*next;
	int		 empty;

	if (zone == NULL || zone == &uma_zone_store || smp_sched_cpu_count() != 1) {
		return (UMA_DESTROY_BUSY);
	}

	uma_zone_drain_local(zone);
	spin_lock(&zone->spin);
	empty = uma_zone_empty(zone);
	spin_unlock(&zone->spin);
	if (!empty) {
		return (UMA_DESTROY_BUSY);
	}

	spin_lock(&uma_registry_spin);
	if (!zone->alive) {
		spin_unlock(&uma_registry_spin);
		return (UMA_DESTROY_BUSY);
	}
	zone->alive = 0;
	uma_registry_remove(zone);
	spin_unlock(&uma_registry_spin);

	spin_lock(&zone->spin);
	for (slab = zone->partial; slab != NULL; slab = next) {
		next = slab->next;
		uma_slab_release(zone, slab);
	}
	zone->partial = NULL;
	zone->full = NULL;
	spin_unlock(&zone->spin);
	uma_zfree(&uma_zone_store, zone);
	return (UMA_DESTROY_OK);
}

uma_zone_t
uma_zfind(const char *name)
{
	uma_zone_t	zone;

	if (name == NULL) {
		return (NULL);
	}
	spin_lock(&uma_registry_spin);
	for (zone = uma_zones; zone != NULL; zone = zone->registry_next) {
		if (strncmp(zone->name, name, UMA_NAME_LEN) == 0) {
			spin_unlock(&uma_registry_spin);
			return (zone);
		}
	}
	spin_unlock(&uma_registry_spin);
	return (NULL);
}

size_t
uma_zone_item_size(uma_zone_t zone)
{
	return (zone != NULL ? zone->item_size : 0);
}

int
uma_zone_stats(uma_zone_t zone, uma_stat_t *out)
{
	uma_slab_t	*slab;

	if (zone == NULL || out == NULL) {
		return (-1);
	}
	memset(out, 0, sizeof(*out));
	spin_lock(&zone->spin);
	out->allocs = zone->allocs;
	out->frees = zone->frees;
	out->item_size = zone->item_size;
	out->fails = zone->fails;
	out->slabs = zone->slab_count;
	for (slab = zone->partial; slab != NULL; slab = slab->next) {
		out->items_total += slab->items_total;
		out->items_free += slab->items_free;
	}
	for (slab = zone->full; slab != NULL; slab = slab->next) {
		out->items_total += slab->items_total;
	}
	spin_unlock(&zone->spin);
	return (0);
}

u64
uma_reclaim(void)
{
	return (0);
}

void
uma_dump(void)
{
	uma_zone_t	zone;
	uma_stat_t	st;

	spin_lock(&uma_registry_spin);
	for (zone = uma_zones; zone != NULL; zone = zone->registry_next) {
		if (uma_zone_stats(zone, &st) != 0) {
			continue;
		}
		printk("uma %-23s: item %u slabs %u free %u alloc %u freeops %u\n",
		    zone->name, (u32)st.item_size, (u32)st.slabs,
		    (u32)st.items_free, (u32)st.allocs, (u32)st.frees);
	}
	spin_unlock(&uma_registry_spin);
}
