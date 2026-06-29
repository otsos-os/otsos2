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
$define %type int as 32 bit signed
$define %type size_t as unsigned long
$define %type uma_zone as struct with name, item_size, align, flags, nitems, nfree, free_list, slabs, nslabs, max_slabs, initialized

$define %func uma_align_up as function with args size_t, size_t
$define %func uma_strncmp as function with args const char *, const char *, size_t
$define %func uma_strncpy as procedure with args char *, const char *, size_t
$define %func uma_strlen as function with args const char *
$define %func uma_zone_register as function with args struct uma_zone *
$define %func uma_zone_alloc_struct as function with args void
$define %func uma_zone_free_struct as procedure with args struct uma_zone *
$define %func uma_slab_grow as function with args struct uma_zone *
$define %func uma_zcreate as function with args const char *, size_t, size_t, u32
$define %func uma_zdestroy as procedure with args uma_zone_t
$define %func uma_zalloc as function with args uma_zone_t, u32
$define %func uma_zfree as procedure with args uma_zone_t, void *
$define %func uma_init as procedure with args void
$define %func uma_dump as procedure with args void
$define %func uma_zfind as function with args const char *

*/

/* !SPACE!

$space %internal uma_align_up, uma_strncmp, uma_strncpy, uma_strlen
$space %internal uma_zone_register, uma_zone_alloc_struct
$space %internal uma_zone_free_struct, uma_slab_grow
$space %export uma_zcreate, uma_zdestroy, uma_zalloc, uma_zfree
$space %export uma_zfind, uma_init, uma_dump

*/

#include <mm/uma.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>

struct uma_zone {
	char		name[32];
	void		*free_list;
	void		**slabs;
	size_t		item_size;
	size_t		align;
	u32		flags;
	u32		nitems;
	u32		nfree;
	u32		nslabs;
	u32		max_slabs;
	int		initialized;
};

static struct uma_zone	*g_zones[UMA_ZONE_MAX];
static struct uma_zone	*g_zone_zone;

static size_t
uma_align_up(size_t value, size_t align)
{
	if (align == 0) {
		align = UMA_ALIGN_CACHE;
	}
	return ((value + align - 1) & ~(align - 1));
}

static int
uma_strncmp(const char *a, const char *b, size_t n)
{
	size_t	i;

	for (i = 0; i < n; i++) {
		if (a[i] != b[i]) {
			return ((int)(u8)a[i] - (u8)b[i]);
		}
		if (a[i] == '\0') {
			return (0);
		}
	}
	return (0);
}

static void
uma_strncpy(char *dst, const char *src, size_t n)
{
	size_t	i;

	for (i = 0; i < n - 1 && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}
	for (; i < n; i++) {
		dst[i] = '\0';
	}
}

static size_t
uma_strlen(const char *s)
{
	size_t	n;

	n = 0;
	while (s[n] != '\0') {
		n++;
	}
	return (n);
}

static int
uma_zone_register(struct uma_zone *zone)
{
	u32	i;

	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == NULL) {
			g_zones[i] = zone;
			return (0);
		}
	}
	return (-1);
}

static struct uma_zone *
uma_zone_alloc_struct(void)
{
	if (g_zone_zone != NULL) {
		return ((struct uma_zone *)uma_zalloc(
		    g_zone_zone, M_WAITOK));
	}
	return ((struct uma_zone *)kmem_alloc(
	    sizeof(struct uma_zone)));
}

static void
uma_zone_free_struct(struct uma_zone *zone)
{
	if (g_zone_zone != NULL) {
		uma_zfree(g_zone_zone, zone);
	} else {
		kmem_free(zone);
	}
}

static int
uma_slab_grow(struct uma_zone *zone)
{
	void		*slab;
	void		**new_slabs;
	u32		new_max;
	u32		i;
	size_t		slab_size;

	if (zone->nslabs >= zone->max_slabs) {
		new_max = zone->max_slabs == 0 ? 8 :
		    zone->max_slabs * 2;
		new_slabs = (void **)kmem_alloc(
		    sizeof(void *) * new_max);
		if (new_slabs == NULL) {
			return (-1);
		}
		for (i = 0; i < zone->nslabs; i++) {
			new_slabs[i] = zone->slabs[i];
		}
		if (zone->slabs != NULL) {
			kmem_free(zone->slabs);
		}
		zone->slabs = new_slabs;
		zone->max_slabs = new_max;
	}

	slab_size = zone->item_size * UMA_BUCKET_SIZE;
	slab = kmem_alloc(slab_size);
	if (slab == NULL) {
		return (-1);
	}

	zone->slabs[zone->nslabs] = slab;
	zone->nslabs++;

	for (i = 0; i < UMA_BUCKET_SIZE; i++) {
		void	*item;

		item = (void *)((u8 *)slab +
		    (size_t)i * zone->item_size);
		*(void **)item = zone->free_list;
		zone->free_list = item;
		zone->nfree++;
		zone->nitems++;
	}

	return (0);
}

uma_zone_t
uma_zcreate(const char *name, size_t size, size_t align,
    u32 flags)
{
	struct uma_zone	*zone;
	size_t		name_len;

	if (size == 0) {
		return (NULL);
	}

	zone = uma_zone_alloc_struct();
	if (zone == NULL) {
		return (NULL);
	}
	memset(zone, 0, sizeof(*zone));

	name_len = uma_strlen(name);
	if (name_len >= 32) {
		name_len = 31;
	}
	uma_strncpy(zone->name, name, 32);

	zone->align = align;
	zone->item_size = uma_align_up(size, align);
	zone->flags = flags;
	zone->free_list = NULL;
	zone->slabs = NULL;
	zone->nslabs = 0;
	zone->max_slabs = 0;
	zone->nitems = 0;
	zone->nfree = 0;
	zone->initialized = 1;

	if (uma_zone_register(zone) != 0) {
		uma_zone_free_struct(zone);
		return (NULL);
	}

	return (zone);
}

void
uma_zdestroy(uma_zone_t zone)
{
	u32	i;

	if (zone == NULL) {
		return;
	}

	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == zone) {
			g_zones[i] = NULL;
			break;
		}
	}

	for (i = 0; i < zone->nslabs; i++) {
		kmem_free(zone->slabs[i]);
	}
	if (zone->slabs != NULL) {
		kmem_free(zone->slabs);
	}
	uma_zone_free_struct(zone);
}

void *
uma_zalloc(uma_zone_t zone, u32 flags)
{
	void	*item;

	if (zone == NULL || !zone->initialized) {
		return (NULL);
	}

	if (zone->free_list == NULL) {
		if (uma_slab_grow(zone) != 0) {
			if (flags & M_NOWAIT) {
				return (NULL);
			}
			return (NULL);
		}
	}

	item = zone->free_list;
	zone->free_list = *(void **)item;
	zone->nfree--;

	if (flags & M_ZERO) {
		memset(item, 0, zone->item_size);
	}

	return (item);
}

void
uma_zfree(uma_zone_t zone, void *item)
{
	if (zone == NULL || item == NULL) {
		return;
	}

	*(void **)item = zone->free_list;
	zone->free_list = item;
	zone->nfree++;
}

void
uma_init(void)
{
	static const size_t	sizes[] = {16, 32, 64, 128,
	    256, 512, 1024, 2048, 4096};
	static const char	*names[] = {"16", "32", "64",
	    "128", "256", "512", "1024", "2048", "4096"};
	u32			i;

	for (i = 0; i < UMA_ZONE_MAX; i++) {
		g_zones[i] = NULL;
	}

	g_zone_zone = (struct uma_zone *)kmem_alloc(
	    sizeof(struct uma_zone));
	if (g_zone_zone == NULL) {
		return;
	}
	memset(g_zone_zone, 0, sizeof(*g_zone_zone));
	uma_strncpy(g_zone_zone->name, "zone", 32);
	g_zone_zone->align = UMA_ALIGN_CACHE;
	g_zone_zone->item_size = uma_align_up(
	    sizeof(struct uma_zone), UMA_ALIGN_CACHE);
	g_zone_zone->flags = 0;
	g_zone_zone->free_list = NULL;
	g_zone_zone->slabs = NULL;
	g_zone_zone->nslabs = 0;
	g_zone_zone->max_slabs = 0;
	g_zone_zone->nitems = 0;
	g_zone_zone->nfree = 0;
	g_zone_zone->initialized = 1;

	uma_zone_register(g_zone_zone);

	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]);
	    i++) {
		uma_zcreate(names[i], sizes[i],
		    UMA_ALIGN_CACHE, 0);
	}
}

void
uma_dump(void)
{
	u32	i;

	com1_printf("[uma] zone dump:\n");
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		struct uma_zone	*z;

		z = g_zones[i];
		if (z == NULL) {
			continue;
		}
		com1_printf("  %-8s item_size=%lu nitems=%u "
		    "nfree=%u nslabs=%u\n", z->name,
		    z->item_size, z->nitems, z->nfree,
		    z->nslabs);
	}
}

uma_zone_t
uma_zfind(const char *name)
{
	u32	i;

	if (name == NULL) {
		return (NULL);
	}

	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == NULL) {
			continue;
		}
		if (uma_strncmp(g_zones[i]->name, name, 32)
		    == 0) {
			return (g_zones[i]);
		}
	}

	return (NULL);
}
