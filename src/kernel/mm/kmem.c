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
$define %type header_t as struct with magic, is_free, size, payload_size, next, prev

$define %func align16 as function with args size_t
$define %func split_block as procedure with args header_t *, size_t
$define %func coalesce as function with args header_t *
$define %func kmem_init_internal as procedure with args void
$define %func kmem_alloc_internal as function with args size_t
$define %func kmem_free_internal as procedure with args void *
$define %func kmem_calloc_internal as function with args size_t, size_t
$define %func kmem_realloc_internal as function with args void *, size_t
$define %func kmem_alloc_aligned_internal as function with args size_t, size_t
$define %func kmem_free_bytes_internal as function with args void
$define %func kmem_is_init_internal as function with args void
$define %func kmem_dump_internal as procedure with args void
$define %func kmem_usable_size_internal as function with args void *
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
$define %func kmem_set_growth_pool as procedure with args void *, size_t

*/

/* !SPACE!

$space %internal align16, split_block, coalesce
$space %internal kmem_init_internal, kmem_alloc_internal
$space %internal kmem_free_internal, kmem_calloc_internal
$space %internal kmem_realloc_internal, kmem_alloc_aligned_internal
$space %internal kmem_free_bytes_internal, kmem_is_init_internal
$space %internal kmem_dump_internal, kmem_usable_size_internal
$space %export kmem_init, kmem_alloc, kmem_free, kmem_calloc
$space %export kmem_realloc, kmem_alloc_aligned, kmem_usable_size
$space %export kmem_free_bytes, kmem_total_bytes, kmem_used_bytes
$space %export kmem_is_initialized, kmem_dump
$space %export kmem_set_growth_pool

*/

#include <mlibc/stdio.h>
#include <kernel/bootmem.h>
#include <mm/kmem.h>
#include <mm/vm/vm_page.h>
#include <mlibc/mlibc.h>

#define KMEM_HEAP_SIZE_VAL	(8 * 1024 * 1024)
#define KMEM_MAGIC		0x48454150
#define KMEM_REDZONE_SZ		16
#define KMEM_REDZONE_PAT	0xCC
#define KMEM_POISON_PAT		0xAA

typedef struct header {
	u32			magic;
	u32			is_free;
	u64			size;
	u64			payload_size;
	struct header		*next;
	struct header		*prev;
} header_t;

static char		*kmem_heap_start = NULL;
static char		*kmem_heap_end = NULL;
static header_t		*kmem_heap_head = NULL;
static int		kmem_heap_init = 0;

static char		*growth_pool_ptr = NULL;
static size_t		growth_pool_remaining = 0;
static char		*growth_pool_start = NULL;
static char		*growth_pool_end = NULL;

static size_t
align16(size_t size)
{
	return ((size + 15) & ~15UL);
}

static void
split_block(header_t *current, size_t size)
{
	header_t	*new_block;

	if (current->size >= size + sizeof(header_t) + 16) {
		new_block = (header_t *)((char *)current +
		    sizeof(header_t) + size);
		new_block->magic = KMEM_MAGIC;
		new_block->size = current->size - size -
		    sizeof(header_t);
		new_block->payload_size = 0;
		new_block->is_free = 1;
		new_block->next = current->next;
		new_block->prev = current;
		if (new_block->next != NULL) {
			new_block->next->prev = new_block;
		}
		current->size = size;
		current->next = new_block;
	}
}

static header_t *
coalesce(header_t *block)
{
	header_t	*next;
	header_t	*prev;

	if (block == NULL || !block->is_free) {
		return (block);
	}

	next = block->next;
	if (next != NULL && next->is_free &&
	    next->magic == KMEM_MAGIC &&
	    (char *)next == (char *)block +
	    sizeof(header_t) + block->size) {
		block->size += sizeof(header_t) + next->size;
		block->payload_size = 0;
		block->next = next->next;
		if (block->next != NULL) {
			block->next->prev = block;
		}
	}

	prev = block->prev;
	if (prev != NULL && prev->is_free &&
	    prev->magic == KMEM_MAGIC &&
	    (char *)block == (char *)prev +
	    sizeof(header_t) + prev->size) {
		prev->size += sizeof(header_t) + block->size;
		prev->payload_size = 0;
		prev->next = block->next;
		if (block->next != NULL) {
			block->next->prev = prev;
		}
		return (prev);
	}

	return (block);
}

void
kmem_set_growth_pool(void *addr, size_t size)
{
	growth_pool_ptr = (char *)addr;
	growth_pool_remaining = size;
	growth_pool_start = (char *)addr;
	growth_pool_end = (char *)addr + size;
	printk("[KMEM] growth pool set: %p size=%d\n",
	    addr, (int)size);
}

static void
kmem_init_internal(void)
{
	kmem_heap_start = (char *)bootmem_alloc(
	    KMEM_HEAP_SIZE_VAL, 4096);
	if (kmem_heap_start == NULL) {
		printk("KMEM: bootmem failed to allocate "
		    "heap\n");
		return;
	}
	kmem_heap_end = kmem_heap_start + KMEM_HEAP_SIZE_VAL;

	kmem_heap_head = (header_t *)kmem_heap_start;
	kmem_heap_head->magic = KMEM_MAGIC;
	kmem_heap_head->size = KMEM_HEAP_SIZE_VAL -
	    sizeof(header_t);
	kmem_heap_head->payload_size = 0;
	kmem_heap_head->next = NULL;
	kmem_heap_head->prev = NULL;
	kmem_heap_head->is_free = 1;

	printk("Heap initialized at %p header size: %d\n",
	    kmem_heap_start, (int)sizeof(header_t));
	printk("free block size: %d\n",
	    (int)kmem_heap_head->size);
	kmem_heap_init = 1;
}

static void *
kmem_alloc_internal(size_t size)
{
	header_t	*current;
	u8		*base;
	size_t		payload_size;
	size_t		total_size;

	if (kmem_heap_head == NULL) {
		kmem_init_internal();
	}
	if (size == 0) {
		return (NULL);
	}

	size = align16(size);
	payload_size = size;
	total_size = payload_size + (2 * KMEM_REDZONE_SZ);

	current = kmem_heap_head;
	while (current != NULL) {
		if (current->magic != KMEM_MAGIC) {
			printk("HEAP CORRUPTION at %p "
			    "magic=%x\n", current,
			    current->magic);
			return (NULL);
		}
		if (current->is_free &&
		    current->size >= total_size) {
			split_block(current, total_size);
			current->is_free = 0;
			current->payload_size = payload_size;
			base = (u8 *)current +
			    sizeof(header_t);
			memset(base, KMEM_REDZONE_PAT,
			    KMEM_REDZONE_SZ);
			memset(base + KMEM_REDZONE_SZ +
			    payload_size, KMEM_REDZONE_PAT,
			    KMEM_REDZONE_SZ);
			return (void *)(base +
			    KMEM_REDZONE_SZ);
		}
		current = current->next;
	}

	{
		size_t		grow_needed;
		void		*new_area;
		header_t	*hdr, *last;

		grow_needed = total_size + sizeof(header_t);
		grow_needed = (grow_needed + 4095) &
		    ~(size_t)4095;

		if (growth_pool_remaining >= grow_needed) {
			new_area = growth_pool_ptr;
			growth_pool_ptr += grow_needed;
			growth_pool_remaining -= grow_needed;
		} else {
			new_area = bootmem_alloc(grow_needed,
			    4096);
			if (new_area) {
				vm_page_reserve_range(
				    (u64)new_area -
				    DMAP_BASE, grow_needed);
			}
		}
		if (!new_area) {
			printk("KMALLOC FAILED! request "
			    "size: %d\n", (int)size);
			return (NULL);
		}

		printk("[KMEM] grow: new_area=%p grow_needed=%d "
		    "phys=%p\n", new_area, (int)grow_needed,
		    (void *)((u64)new_area - DMAP_BASE));

		hdr = (header_t *)new_area;
		hdr->magic = KMEM_MAGIC;
		hdr->is_free = 1;
		hdr->size = grow_needed -
		    sizeof(header_t);
		hdr->payload_size = 0;
		hdr->next = NULL;

		last = kmem_heap_head;
		while (last->next) {
			last = last->next;
		}
		hdr->prev = last;
		last->next = hdr;

		if ((char *)new_area < kmem_heap_start) {
			kmem_heap_start = (char *)new_area;
		}
		if ((char *)new_area + grow_needed >
		    kmem_heap_end) {
			kmem_heap_end = (char *)new_area +
			    grow_needed;
		}

		current = kmem_heap_head;
		while (current != NULL) {
			if (current->magic != KMEM_MAGIC) {
				return (NULL);
			}
			if (current->is_free &&
			    current->size >= total_size) {
				split_block(current, total_size);
				current->is_free = 0;
				current->payload_size =
				    payload_size;
				base = (u8 *)current +
				    sizeof(header_t);
				memset(base, KMEM_REDZONE_PAT,
				    KMEM_REDZONE_SZ);
				memset(base + KMEM_REDZONE_SZ +
				    payload_size,
				    KMEM_REDZONE_PAT,
				    KMEM_REDZONE_SZ);
				return (void *)(base +
				    KMEM_REDZONE_SZ);
			}
			current = current->next;
		}

		printk("KMALLOC FAILED! request "
		    "size: %d\n", (int)size);
		return (NULL);
	}
}

static void
kmem_free_internal(void *ptr)
{
	header_t	*header;
	u8		*base;
	u8		*payload;
	u32		i;

	if (ptr == NULL) {
		return;
	}
	if (kmem_heap_start == NULL || kmem_heap_end == NULL) {
		return;
	}

	header = (header_t *)((char *)ptr - KMEM_REDZONE_SZ -
	    sizeof(header_t));

	if (header->magic != KMEM_MAGIC) {
		printk("KFREE: invalid pointer or heap "
		    "corrupt %p\n", ptr);
		return;
	}
	if (header->is_free) {
		printk("KFREE: double free %p\n", ptr);
		return;
	}

	base = (u8 *)header + sizeof(header_t);
	payload = base + KMEM_REDZONE_SZ;
	for (i = 0; i < KMEM_REDZONE_SZ; i++) {
		if (base[i] != KMEM_REDZONE_PAT) {
			printk("KFREE: left redzone "
			    "corrupted %p\n", ptr);
			break;
		}
	}
	for (i = 0; i < KMEM_REDZONE_SZ; i++) {
		if (payload[header->payload_size + i] !=
		    KMEM_REDZONE_PAT) {
			printk("KFREE: right redzone "
			    "corrupted %p\n", ptr);
			break;
		}
	}

	memset(payload, KMEM_POISON_PAT,
	    (size_t)header->payload_size);
	header->payload_size = 0;
	header->is_free = 1;
	coalesce(header);
}

static void *
kmem_calloc_internal(size_t nmemb, size_t size)
{
	void	*ptr;
	size_t	total;

	total = nmemb * size;
	if (nmemb != 0 && total / nmemb != size) {
		return (NULL);
	}

	ptr = kmem_alloc_internal(total);
	if (ptr != NULL) {
		memset(ptr, 0, total);
	}
	return (ptr);
}

static void *
kmem_realloc_internal(void *ptr, size_t size)
{
	header_t	*header;
	header_t	*next;
	void		*new_ptr;

	if (ptr == NULL) {
		return (kmem_alloc_internal(size));
	}
	if (size == 0) {
		kmem_free_internal(ptr);
		return (NULL);
	}

	header = (header_t *)((char *)ptr - KMEM_REDZONE_SZ -
	    sizeof(header_t));
	if (header->magic != KMEM_MAGIC) {
		return (NULL);
	}
	if (header->payload_size >= size) {
		return (ptr);
	}

	next = header->next;
	if (next != NULL && next->is_free &&
	    (header->size + sizeof(header_t) + next->size) >=
	    (align16(size) + (2 * KMEM_REDZONE_SZ))) {
		header->size += sizeof(header_t) +
		    next->size;
		header->next = next->next;
		if (header->next != NULL) {
			header->next->prev = header;
		}
		header->payload_size = align16(size);
		split_block(header, header->payload_size +
		    (2 * KMEM_REDZONE_SZ));
		return (ptr);
	}

	new_ptr = kmem_alloc_internal(size);
	if (new_ptr == NULL) {
		return (NULL);
	}
	memcpy(new_ptr, ptr, (size_t)header->payload_size);
	kmem_free_internal(ptr);
	return (new_ptr);
}

static void *
kmem_alloc_aligned_internal(size_t size, size_t align)
{
	header_t	*current;
	header_t	*aligned_block;
	u8		*base;
	size_t		payload_size;
	size_t		total_size;
	u64		data_start;
	u64		aligned_payload;
	u64		aligned_header;
	u64		padding;

	if (kmem_heap_head == NULL) {
		kmem_init_internal();
	}
	if (size == 0) {
		return (NULL);
	}
	if (align <= 16) {
		return (kmem_alloc_internal(size));
	}

	payload_size = align16(size);
	total_size = payload_size + (2 * KMEM_REDZONE_SZ);
	current = kmem_heap_head;

rescan:
	while (current != NULL) {
		if (current->is_free) {
			data_start = (u64)current +
			    sizeof(header_t) + KMEM_REDZONE_SZ;
			aligned_payload = (data_start + align - 1) &
			    ~(align - 1);
			aligned_header = aligned_payload -
			    KMEM_REDZONE_SZ - sizeof(header_t);
			padding = aligned_header - (u64)current;

			if (padding + total_size <= current->size) {
				if (padding >= sizeof(header_t) + 16) {
					aligned_block = (header_t *)
					    ((char *)current + padding);
					aligned_block->magic =
					    KMEM_MAGIC;
					aligned_block->is_free = 1;
					aligned_block->size =
					    current->size - padding;
					aligned_block->payload_size = 0;
					aligned_block->next =
					    current->next;
					aligned_block->prev = current;
					if (aligned_block->next != NULL)
						aligned_block->next->prev =
						    aligned_block;
					current->next = aligned_block;
					current->size = padding -
					    sizeof(header_t);
					current->payload_size = 0;
					current = aligned_block;
					padding = 0;
				} else if (padding != 0) {
					current = current->next;
					continue;
				}

				if (padding == 0) {
					split_block(current,
					    total_size);
					current->is_free = 0;
					current->payload_size =
					    payload_size;
					base = (u8 *)current +
					    sizeof(header_t);
					memset(base,
					    KMEM_REDZONE_PAT,
					    KMEM_REDZONE_SZ);
					memset(base + KMEM_REDZONE_SZ +
					    current->payload_size,
					    KMEM_REDZONE_PAT,
					    KMEM_REDZONE_SZ);
					return (void *)(base +
					    KMEM_REDZONE_SZ);
				}
			}
		}
		current = current->next;
	}

	{
		size_t		grow_needed;
		size_t		pad;
		void		*new_area;
		header_t	*hdr;

		grow_needed = total_size + sizeof(header_t) + align;
		grow_needed = (grow_needed + 4095) & ~(size_t)4095;
		if (growth_pool_remaining >= grow_needed) {
			new_area = growth_pool_ptr;
			growth_pool_ptr += grow_needed;
			growth_pool_remaining -= grow_needed;
		} else {
			new_area = bootmem_alloc(grow_needed, 4096);
			if (new_area) {
				vm_page_reserve_range(
				    (u64)new_area - DMAP_BASE,
				    grow_needed);
			}
		}
		if (!new_area) {
			printk("KMALLOC_ALIGNED FAILED! size=%d "
			    "align=%d\n", (int)size, (int)align);
			return (NULL);
		}
		printk("[KMEM] grow aligned: new_area=%p "
		    "grow_needed=%d phys=%p\n", new_area,
		    (int)grow_needed,
		    (void *)((u64)new_area - DMAP_BASE));

		pad = (sizeof(header_t) + KMEM_REDZONE_SZ) % align;
		if (pad != 0) {
			pad = align - pad;
		}
		hdr = (header_t *)((u8 *)new_area + pad);
		hdr->magic = KMEM_MAGIC;
		hdr->is_free = 1;
		hdr->size = grow_needed - pad - sizeof(header_t);
		hdr->payload_size = 0;
		hdr->next = kmem_heap_head;
		hdr->prev = NULL;
		if (kmem_heap_head != NULL) {
			kmem_heap_head->prev = hdr;
		}
		kmem_heap_head = hdr;
		current = hdr;
		goto rescan;
	}
}

static size_t
kmem_usable_size_internal(void *ptr)
{
	header_t	*header;

	if (ptr == NULL) {
		return (0);
	}
	header = (header_t *)((char *)ptr - KMEM_REDZONE_SZ -
	    sizeof(header_t));
	if (header->magic != KMEM_MAGIC) {
		return (0);
	}
	return (header->payload_size);
}

static size_t
kmem_free_bytes_internal(void)
{
	header_t	*current;
	size_t		free_mem;

	free_mem = 0;
	current = kmem_heap_head;
	while (current != NULL) {
		if (current->is_free) {
			free_mem += current->size;
		}
		current = current->next;
	}
	return (free_mem);
}

static int
kmem_is_init_internal(void)
{
	if (!kmem_heap_init || kmem_heap_head == NULL) {
		return (0);
	}
	if (kmem_heap_head->magic != KMEM_MAGIC) {
		return (0);
	}
	return (1);
}

static void
kmem_dump_internal(void)
{
	header_t	*current;
	int		i;

	printk("--- HEAP DUMP ---\n");
	current = kmem_heap_head;
	i = 0;
	while (current != NULL) {
		printk("Block %d: %p size=%d free=%d "
		    "next=%p prev=%p\n", i++, current,
		    (int)current->size,
		    (int)current->is_free,
		    current->next, current->prev);
		current = current->next;
	}
	printk("Total free: %d\n",
	    (int)kmem_free_bytes_internal());
	printk("-----------------\n");
}

void
kmem_init(void)
{
	kmem_init_internal();
}

void *
kmem_alloc(size_t size)
{
	return (kmem_alloc_internal(size));
}

void
kmem_free(void *ptr)
{
	kmem_free_internal(ptr);
}

void *
kmem_calloc(size_t nmemb, size_t size)
{
	return (kmem_calloc_internal(nmemb, size));
}

void *
kmem_realloc(void *ptr, size_t size)
{
	return (kmem_realloc_internal(ptr, size));
}

void *
kmem_alloc_aligned(size_t size, size_t align)
{
	return (kmem_alloc_aligned_internal(size, align));
}

size_t
kmem_usable_size(void *ptr)
{
	return (kmem_usable_size_internal(ptr));
}

size_t
kmem_free_bytes(void)
{
	return (kmem_free_bytes_internal());
}

size_t
kmem_total_bytes(void)
{
	return ((size_t)KMEM_HEAP_SIZE_VAL);
}

size_t
kmem_used_bytes(void)
{
	return (KMEM_HEAP_SIZE_VAL -
	    kmem_free_bytes_internal());
}

int
kmem_is_initialized(void)
{
	return (kmem_is_init_internal());
}

void
kmem_dump(void)
{
	kmem_dump_internal();
}
