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
$define %type uma_zone as struct with name, item_size, align, flags, nitems, nfree, free_list, slabs, nslabs, max_slabs, initialized, lock
$define %type spin_t as spin mutex

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
$space %internal uma_reg_spin
$space %export uma_zcreate, uma_zdestroy, uma_zalloc, uma_zfree
$space %export uma_zfind, uma_init, uma_dump

*/

#include <kernel/sync/sync.h>
#include <mm/uma.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdio.h>

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
	spin_t		lock;
};

static struct uma_zone	*g_zones[UMA_ZONE_MAX];
static struct uma_zone	*g_zone_zone;

static spin_t		uma_reg_spin = SPIN_INITIALIZER("uma_reg", LO_UMA);

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
	int	rc;

	rc = -1;
	spin_lock(&uma_reg_spin);
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == NULL) {
			g_zones[i] = zone;
			rc = 0;
			break;
		}
	}
	spin_unlock(&uma_reg_spin);
	return (rc);
}

static struct uma_zone *
uma_zone_alloc_struct(void)
{
	struct uma_zone	*zz;

	zz = __atomic_load_n(&g_zone_zone, __ATOMIC_ACQUIRE);
	if (zz != NULL) {
		return ((struct uma_zone *)uma_zalloc(zz, M_WAITOK));
	}
	return ((struct uma_zone *)kmem_alloc(
	    sizeof(struct uma_zone)));
}

static void
uma_zone_free_struct(struct uma_zone *zone)
{
	struct uma_zone	*zz;

	zz = __atomic_load_n(&g_zone_zone, __ATOMIC_ACQUIRE);
	if (zz != NULL) {
		uma_zfree(zz, zone);
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
	spin_init(&zone->lock, zone->name, LO_UMA_ZONE);
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
	void	**slabs;
	u32	nslabs;
	u32	i;

	if (zone == NULL) {
		return;
	}

	spin_lock(&uma_reg_spin);
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == zone) {
			g_zones[i] = NULL;
			break;
		}
	}
	spin_unlock(&uma_reg_spin);

	spin_lock(&zone->lock);
	zone->initialized = 0;
	slabs = zone->slabs;
	nslabs = zone->nslabs;
	zone->slabs = NULL;
	zone->nslabs = 0;
	zone->max_slabs = 0;
	zone->free_list = NULL;
	zone->nitems = 0;
	zone->nfree = 0;
	spin_unlock(&zone->lock);

	for (i = 0; i < nslabs; i++) {
		kmem_free(slabs[i]);
	}
	if (slabs != NULL) {
		kmem_free(slabs);
	}
	uma_zone_free_struct(zone);
}

void *
uma_zalloc(uma_zone_t zone, u32 flags)
{
	void	*item;

	if (zone == NULL) {
		return (NULL);
	}

	spin_lock(&zone->lock);
	if (!zone->initialized) {
		spin_unlock(&zone->lock);
		return (NULL);
	}

	if (zone->free_list == NULL) {
		if (uma_slab_grow(zone) != 0) {
			spin_unlock(&zone->lock);
			return (NULL);
		}
	}

	item = zone->free_list;
	zone->free_list = *(void **)item;
	zone->nfree--;
	spin_unlock(&zone->lock);

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

	spin_lock(&zone->lock);
	*(void **)item = zone->free_list;
	zone->free_list = item;
	zone->nfree++;
	spin_unlock(&zone->lock);
}

void
uma_init(void)
{
	static const size_t	sizes[] = {16, 32, 64, 128,
	    256, 512, 1024, 2048, 4096};
	static const char	*names[] = {"16", "32", "64",
	    "128", "256", "512", "1024", "2048", "4096"};
	struct uma_zone		*zz;
	u32			i;

	spin_lock(&uma_reg_spin);
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		g_zones[i] = NULL;
	}
	spin_unlock(&uma_reg_spin);

	zz = (struct uma_zone *)kmem_alloc(sizeof(struct uma_zone));
	if (zz == NULL) {
		return;
	}
	memset(zz, 0, sizeof(*zz));
	uma_strncpy(zz->name, "zone", 32);
	zz->align = UMA_ALIGN_CACHE;
	zz->item_size = uma_align_up(sizeof(struct uma_zone),
	    UMA_ALIGN_CACHE);
	zz->flags = 0;
	zz->free_list = NULL;
	zz->slabs = NULL;
	zz->nslabs = 0;
	zz->max_slabs = 0;
	zz->nitems = 0;
	zz->nfree = 0;
	spin_init(&zz->lock, zz->name, LO_UMA_ZONE);
	zz->initialized = 1;

	uma_zone_register(zz);
	__atomic_store_n(&g_zone_zone, zz, __ATOMIC_RELEASE);

	for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]);
	    i++) {
		uma_zcreate(names[i], sizes[i],
		    UMA_ALIGN_CACHE, 0);
	}
}

void
uma_dump(void)
{
	struct uma_zone	*z;
	char		name[32];
	size_t		item_size;
	u32		nitems;
	u32		nfree;
	u32		nslabs;
	u32		i;

	printk("[uma] zone dump:\n");
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		spin_lock(&uma_reg_spin);
		z = g_zones[i];
		if (z == NULL) {
			spin_unlock(&uma_reg_spin);
			continue;
		}
		spin_lock(&z->lock);
		item_size = z->item_size;
		nitems = z->nitems;
		nfree = z->nfree;
		nslabs = z->nslabs;
		uma_strncpy(name, z->name, sizeof(name));
		spin_unlock(&z->lock);
		spin_unlock(&uma_reg_spin);
		printk("  %-8s item_size=%lu nitems=%u "
		    "nfree=%u nslabs=%u\n", name,
		    item_size, nitems, nfree, nslabs);
	}
}

uma_zone_t
uma_zfind(const char *name)
{
	struct uma_zone	*found;
	u32		i;

	if (name == NULL) {
		return (NULL);
	}

	found = NULL;
	spin_lock(&uma_reg_spin);
	for (i = 0; i < UMA_ZONE_MAX; i++) {
		if (g_zones[i] == NULL) {
			continue;
		}
		if (uma_strncmp(g_zones[i]->name, name, 32)
		    == 0) {
			found = g_zones[i];
			break;
		}
	}
	spin_unlock(&uma_reg_spin);

	return (found);
}
