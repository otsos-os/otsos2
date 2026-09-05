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

$define %type kmem_hdr_t as allocation header immediately before a client pointer
$define %type uma_zone_t as opaque slab allocation zone
$define %type size_t as unsigned long
$define %type u32 as 32 bit unsigned
$define %type u64 as 64 bit unsigned
$define %type int as 32 bit signed

$define %func kmem_class_index as function with args size_t
$define %func kmem_header as function with args void *
$define %func kmem_account_alloc as procedure with args size_t
$define %func kmem_account_free as procedure with args size_t
$define %func kmem_alloc_large as function with args size_t, size_t
$define %func kmem_alloc_small as function with args size_t
$define %func kmem_alloc_internal as function with args size_t, size_t
$define %func kmem_init as procedure with args void
$define %func kmem_alloc as function with args size_t
$define %func kmem_free as procedure with args void *
$define %func kmem_calloc as function with args size_t, size_t
$define %func kmem_realloc as function with args void *, size_t
$define %func kmem_alloc_aligned as function with args size_t, size_t
$define %func kmem_usable_size as function with args void *
$define %func kmem_free_bytes as function with args void
$define %func kmem_total_bytes as function with args void
$define %func kmem_used_bytes as function with args void
$define %func kmem_is_initialized as function with args void
$define %func kmem_dump as procedure with args void

*/

/* !SPACE!

$space %internal kmem_class_index, kmem_header
$space %internal kmem_account_alloc, kmem_account_free
$space %internal kmem_alloc_large, kmem_alloc_small, kmem_alloc_internal
$space %export kmem_init, kmem_alloc, kmem_free, kmem_calloc
$space %export kmem_realloc, kmem_alloc_aligned, kmem_usable_size
$space %export kmem_free_bytes, kmem_total_bytes, kmem_used_bytes
$space %export kmem_is_initialized, kmem_dump

*/

#include <kernel/sync/sync.h>
#include <mm/kmem.h>
#include <mm/uma.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

#define KMEM_MAGIC		0x4B4D454D48445231ULL
#define KMEM_DEAD		0x4B4D454D44454144ULL
#define KMEM_KIND_SLAB		1U
#define KMEM_KIND_LARGE		2U
#define KMEM_CLASS_COUNT	13
#define KMEM_LARGE_ALIGN_MAX	((size_t)1 << 30)
#define KMEM_ALIGN_UP(v, a)	(((v) + (a) - 1) & ~((a) - 1))

static const size_t kmem_class_size[KMEM_CLASS_COUNT] = {
	64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096
};

typedef struct kmem_hdr {
	u64		magic;
	size_t		usable;
	union {
		uma_zone_t	zone;
		u64		phys;
	} owner;
	u32		pages;
	u32		kind;
} kmem_hdr_t;

_Static_assert(sizeof(kmem_hdr_t) == 32,
    "kmem header must preserve KMEM_ALIGN payload alignment");

static const char *kmem_class_name[KMEM_CLASS_COUNT] = {
	"kmem-64", "kmem-96", "kmem-128", "kmem-192", "kmem-256",
	"kmem-384", "kmem-512", "kmem-768", "kmem-1k", "kmem-1536",
	"kmem-2k", "kmem-3k", "kmem-4k"
};

static uma_zone_t	kmem_zones[KMEM_CLASS_COUNT];
static spin_t		kmem_spin = SPIN_INITIALIZER("kmem", LO_KMEM);
static u64		kmem_allocations;
static u64		kmem_frees;
static u64		kmem_bytes_inuse;
static u64		kmem_bytes_peak;
static u64		kmem_failures;
static int		kmem_ready;

static int
kmem_class_index(size_t size)
{
	int	index;

	for (index = 0; index < KMEM_CLASS_COUNT; index++) {
		if (size <= kmem_class_size[index]) {
			return (index);
		}
	}
	return (-1);
}

static kmem_hdr_t *
kmem_header(void *ptr)
{
	if (ptr == NULL) {
		return (NULL);
	}
	return ((kmem_hdr_t *)ptr - 1);
}

static void
kmem_account_alloc(size_t size)
{
	u64	used;

	used = __atomic_add_fetch(&kmem_bytes_inuse, size, __ATOMIC_RELAXED);
	__atomic_fetch_add(&kmem_allocations, 1, __ATOMIC_RELAXED);
	while (used > __atomic_load_n(&kmem_bytes_peak, __ATOMIC_RELAXED)) {
		u64	peak;

		peak = __atomic_load_n(&kmem_bytes_peak, __ATOMIC_RELAXED);
		if (__atomic_compare_exchange_n(&kmem_bytes_peak, &peak, used, 0,
		    __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {
			break;
		}
	}
}

static void
kmem_account_free(size_t size)
{
	__atomic_fetch_sub(&kmem_bytes_inuse, size, __ATOMIC_RELAXED);
	__atomic_fetch_add(&kmem_frees, 1, __ATOMIC_RELAXED);
}


static void *
kmem_alloc_large(size_t size, size_t align)
{
	kmem_hdr_t	*header;
	u64		base, phys, bytes, pages, payload;

	if (align < KMEM_ALIGN) {
		align = KMEM_ALIGN;
	}
	if (align > KMEM_LARGE_ALIGN_MAX ||
	    size > (size_t)-1 - sizeof(*header) - align) {
		return (NULL);
	}
	bytes = size + sizeof(*header) + align - 1;
	pages = (bytes + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (pages == 0 || pages > 0xFFFFFFFFU) {
		return (NULL);
	}
	phys = vm_page_alloc_contig((u32)pages, align > PAGE_SIZE ? align : PAGE_SIZE,
	    0, 0);
	if (phys == 0) {
		return (NULL);
	}
	base = phys + DMAP_BASE;
	payload = KMEM_ALIGN_UP(base + sizeof(*header), align);
	header = (kmem_hdr_t *)(payload - sizeof(*header));
	header->magic = KMEM_MAGIC;
	header->usable = size;
	header->owner.phys = phys;
	header->pages = (u32)pages;
	header->kind = KMEM_KIND_LARGE;
	return ((void *)payload);
}

static void *
kmem_alloc_small(size_t size)
{
	kmem_hdr_t	*header;
	void		*item;
	int		 index;

	if (size > KMEM_SMALL_MAX - sizeof(*header)) {
		return (NULL);
	}
	index = kmem_class_index(size + sizeof(*header));
	if (index < 0 || kmem_zones[index] == NULL) {
		return (NULL);
	}
	item = uma_zalloc(kmem_zones[index], M_WAITOK);
	if (item == NULL) {
		return (NULL);
	}
	header = (kmem_hdr_t *)item;
	header->magic = KMEM_MAGIC;
	header->usable = kmem_class_size[index] - sizeof(*header);
	header->owner.zone = kmem_zones[index];
	header->pages = 0;
	header->kind = KMEM_KIND_SLAB;
	return ((void *)(header + 1));
}

static void *
kmem_alloc_internal(size_t size, size_t align)
{
	void	*ptr;

	if (!kmem_ready || size == 0) {
		return (NULL);
	}
	if (align == 0 || (align & (align - 1)) != 0) {
		return (NULL);
	}
	if (align <= KMEM_ALIGN && size <= KMEM_SMALL_MAX - sizeof(kmem_hdr_t)) {
		ptr = kmem_alloc_small(size);
	} else {
		ptr = kmem_alloc_large(size, align);
	}
	if (ptr == NULL) {
		__atomic_fetch_add(&kmem_failures, 1, __ATOMIC_RELAXED);
		return (NULL);
	}
	kmem_account_alloc(kmem_header(ptr)->usable);
	return (ptr);
}

void
kmem_init(void)
{
	int	index;

	spin_lock(&kmem_spin);
	if (kmem_ready) {
		spin_unlock(&kmem_spin);
		return;
	}
	for (index = 0; index < KMEM_CLASS_COUNT; index++) {
		kmem_zones[index] = uma_zcreate(kmem_class_name[index],
		    kmem_class_size[index], KMEM_ALIGN, 0);
		if (kmem_zones[index] == NULL) {
			printk("kmem: cannot create %s zone\n", kmem_class_name[index]);
			spin_unlock(&kmem_spin);
			return;
		}
	}
	__atomic_store_n(&kmem_ready, 1, __ATOMIC_RELEASE);
	spin_unlock(&kmem_spin);
}

void *
kmem_alloc(size_t size)
{
	return (kmem_alloc_internal(size, KMEM_ALIGN));
}

void *
kmem_calloc(size_t nmemb, size_t size)
{
	void	*ptr;
	size_t	 total;

	if (nmemb != 0 && size > (size_t)-1 / nmemb) {
		return (NULL);
	}
	total = nmemb * size;
	ptr = kmem_alloc_internal(total, KMEM_ALIGN);
	if (ptr != NULL) {
		memset(ptr, 0, total);
	}
	return (ptr);
}

void *
kmem_realloc(void *ptr, size_t size)
{
	void	*new_ptr;
	size_t	 old_size;

	if (ptr == NULL) {
		return (kmem_alloc(size));
	}
	if (size == 0) {
		kmem_free(ptr);
		return (NULL);
	}
	old_size = kmem_usable_size(ptr);
	if (old_size == 0) {
		return (NULL);
	}
	if (size <= old_size) {
		return (ptr);
	}
	new_ptr = kmem_alloc(size);
	if (new_ptr == NULL) {
		return (NULL);
	}
	memcpy(new_ptr, ptr, old_size);
	kmem_free(ptr);
	return (new_ptr);
}

void *
kmem_alloc_aligned(size_t size, size_t align)
{
	return (kmem_alloc_internal(size, align));
}

void
kmem_free(void *ptr)
{
	kmem_hdr_t	*header;
	size_t		 usable;

	header = kmem_header(ptr);
	if (header == NULL || header->magic != KMEM_MAGIC) {
		return;
	}
	usable = header->usable;
	if (header->kind != KMEM_KIND_SLAB && header->kind != KMEM_KIND_LARGE) {
		return;
	}
	if (header->kind == KMEM_KIND_SLAB && header->owner.zone == NULL) {
		return;
	}
	if (header->kind == KMEM_KIND_LARGE && header->pages == 0) {
		return;
	}
	header->magic = KMEM_DEAD;
	if (header->kind == KMEM_KIND_SLAB) {
		uma_zfree(header->owner.zone, header);
	} else {
		vm_page_free_contig(header->owner.phys, header->pages);
	}
	kmem_account_free(usable);
}

size_t
kmem_usable_size(void *ptr)
{
	kmem_hdr_t	*header;

	header = kmem_header(ptr);
	if (header == NULL || header->magic != KMEM_MAGIC) {
		return (0);
	}
	return (header->usable);
}

size_t
kmem_free_bytes(void)
{
	return ((size_t)vm_page_count_free() * PAGE_SIZE);
}

size_t
kmem_total_bytes(void)
{
	return ((size_t)vm_page_count_total() * PAGE_SIZE);
}

size_t
kmem_used_bytes(void)
{
	return ((size_t)__atomic_load_n(&kmem_bytes_inuse, __ATOMIC_RELAXED));
}

int
kmem_is_initialized(void)
{
	return (__atomic_load_n(&kmem_ready, __ATOMIC_ACQUIRE) != 0);
}

void
kmem_dump(void)
{
	printk("kmem: alloc %u free %u used %u peak %u free-pages %u fail %u\n",
	    (u32)__atomic_load_n(&kmem_allocations, __ATOMIC_RELAXED),
	    (u32)__atomic_load_n(&kmem_frees, __ATOMIC_RELAXED),
	    (u32)__atomic_load_n(&kmem_bytes_inuse, __ATOMIC_RELAXED),
	    (u32)__atomic_load_n(&kmem_bytes_peak, __ATOMIC_RELAXED),
	    (u32)vm_page_count_free(),
	    (u32)__atomic_load_n(&kmem_failures, __ATOMIC_RELAXED));
}
