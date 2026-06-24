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

#include <lib/com1.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>

extern char kernel_end;
#define KMEM_HEAP_SIZE_VAL (8 * 1024 * 1024)
#define KMEM_MAGIC 0x48454150
#define KMEM_REDZONE_SZ 16
#define KMEM_REDZONE_PAT 0xCC
#define KMEM_POISON_PAT 0xAA

static char *kmem_heap_start = 0;
static char *kmem_heap_end = 0;
static int kmem_heap_init = 0;

typedef struct header {
  unsigned int magic;
  unsigned int is_free;
  unsigned long size;
  unsigned long payload_size;
  struct header *next;
  struct header *prev;
} header_t;

static header_t *kmem_heap_head = 0;

static unsigned long align16(unsigned long size) { return (size + 15) & ~15UL; }

static void split_block(header_t *current, unsigned long size) {
  if (current->size >= size + sizeof(header_t) + 16) {
    header_t *new_block =
        (header_t *)((char *)current + sizeof(header_t) + size);
    new_block->magic = KMEM_MAGIC;
    new_block->size = current->size - size - sizeof(header_t);
    new_block->payload_size = 0;
    new_block->is_free = 1;
    new_block->next = current->next;
    new_block->prev = current;

    if (new_block->next) {
      new_block->next->prev = new_block;
    }

    current->size = size;
    current->next = new_block;
  }
}

static header_t *coalesce(header_t *block) {
  if (!block || !block->is_free)
    return block;

  header_t *next = block->next;
  if (next && next->is_free && next->magic == KMEM_MAGIC) {
    block->size += sizeof(header_t) + next->size;
    block->payload_size = 0;
    block->next = next->next;
    if (block->next) {
      block->next->prev = block;
    }
  }

  header_t *prev = block->prev;
  if (prev && prev->is_free && prev->magic == KMEM_MAGIC) {
    prev->size += sizeof(header_t) + block->size;
    prev->payload_size = 0;
    prev->next = block->next;
    if (block->next) {
      block->next->prev = prev;
    }
    return prev;
  }

  return block;
}

void kmem_init_internal() {
  kmem_heap_start = (char *)&kernel_end;
  kmem_heap_start = (char *)align16((unsigned long)kmem_heap_start);
  kmem_heap_end = kmem_heap_start + KMEM_HEAP_SIZE_VAL;

  kmem_heap_head = (header_t *)kmem_heap_start;
  kmem_heap_head->magic = KMEM_MAGIC;
  kmem_heap_head->size = KMEM_HEAP_SIZE_VAL - sizeof(header_t);
  kmem_heap_head->payload_size = 0;
  kmem_heap_head->next = 0;
  kmem_heap_head->prev = 0;
  kmem_heap_head->is_free = 1;

  com1_printf("Heap initialized at %p header size: %d\n", kmem_heap_start,
              (int)sizeof(header_t));
  com1_printf("free block size: %d\n", (int)kmem_heap_head->size);
  kmem_heap_init = 1;
}

void *kmem_alloc_internal(unsigned long size) {
  if (!kmem_heap_head)
    kmem_init_internal();

  if (size == 0)
    return 0;

  size = align16(size);
  unsigned long payload_size = size;
  unsigned long total_size = payload_size + (2 * KMEM_REDZONE_SZ);

  header_t *current = kmem_heap_head;
  while (current) {
    if (current->magic != KMEM_MAGIC) {
      com1_printf("HEAP CORRUPTION DETECTED at %p in %x)\n", current,
                  current->magic);
      return 0;
    }
    if (current->is_free && current->size >= total_size) {
      split_block(current, total_size);
      current->is_free = 0;
      current->payload_size = payload_size;
      u8 *base = (u8 *)current + sizeof(header_t);
      memset(base, KMEM_REDZONE_PAT, KMEM_REDZONE_SZ);
      memset(base + KMEM_REDZONE_SZ + payload_size, KMEM_REDZONE_PAT,
             KMEM_REDZONE_SZ);
      return (void *)(base + KMEM_REDZONE_SZ);
    }
    current = current->next;
  }

  com1_printf("KMALLOC FAILED! request size: %d\n", (int)size);
  return 0;
}

void kmem_free_internal(void *ptr) {
  if (!ptr)
    return;
  if (!kmem_heap_start || !kmem_heap_end)
    return;

  header_t *header =
      (header_t *)((char *)ptr - KMEM_REDZONE_SZ - sizeof(header_t));

  if ((char *)header < kmem_heap_start || (char *)header >= kmem_heap_end) {
    com1_printf("KFREE: invalid pointer %p\n", ptr);
    return;
  }

  if (header->magic != KMEM_MAGIC) {
    com1_printf("KFREE: invalid pointer or heap corupt in %p\n", ptr);
    return;
  }

  if (header->is_free) {
    com1_printf("KFREE: double free in %p\n", ptr);
    return;
  }

  u8 *base = (u8 *)header + sizeof(header_t);
  u8 *payload = base + KMEM_REDZONE_SZ;
  for (u32 i = 0; i < KMEM_REDZONE_SZ; i++) {
    if (base[i] != KMEM_REDZONE_PAT) {
      com1_printf("KFREE: left redzone corrupted at %p\n", ptr);
      break;
    }
  }
  for (u32 i = 0; i < KMEM_REDZONE_SZ; i++) {
    if (payload[header->payload_size + i] != KMEM_REDZONE_PAT) {
      com1_printf("KFREE: right redzone corrupted at %p\n", ptr);
      break;
    }
  }

  memset(payload, KMEM_POISON_PAT, header->payload_size);

  header->payload_size = 0;
  header->is_free = 1;
  coalesce(header);
}

void *kmem_calloc_internal(unsigned long nmemb, unsigned long size) {
  unsigned long total = nmemb * size;
  if (nmemb != 0 && total / nmemb != size)
    return 0;

  void *ptr = kmem_alloc_internal(total);
  if (ptr) {
    memset(ptr, 0, total);
  }
  return ptr;
}

void *kmem_realloc_internal(void *ptr, unsigned long size) {
  if (!ptr)
    return kmem_alloc_internal(size);
  if (size == 0) {
    kmem_free_internal(ptr);
    return 0;
  }

  header_t *header = (header_t *)((char *)ptr - sizeof(header_t));
  if (header->magic != KMEM_MAGIC)
    return 0;

  if (header->payload_size >= size) {
    return ptr;
  }

  header_t *next = header->next;
  if (next && next->is_free &&
      (header->size + sizeof(header_t) + next->size) >=
          (align16(size) + (2 * KMEM_REDZONE_SZ))) {
    header->size += sizeof(header_t) + next->size;
    header->next = next->next;
    if (header->next) {
      header->next->prev = header;
    }
    header->payload_size = align16(size);
    split_block(header, header->payload_size + (2 * KMEM_REDZONE_SZ));
    return ptr;
  }

  void *new_ptr = kmem_alloc_internal(size);
  if (!new_ptr)
    return 0;

  memcpy(new_ptr, ptr, header->payload_size);
  kmem_free_internal(ptr);
  return new_ptr;
}

unsigned long kmem_free_bytes_internal() {
  unsigned long free_mem = 0;
  header_t *current = kmem_heap_head;
  while (current) {
    if (current->is_free) {
      free_mem += current->size;
    }
    current = current->next;
  }
  return free_mem;
}

int kmem_is_init_internal(void) {
  if (!kmem_heap_init || !kmem_heap_head) {
    return 0;
  }
  if (kmem_heap_head->magic != KMEM_MAGIC) {
    return 0;
  }
  return 1;
}

void kmem_dump_internal() {
  com1_printf("--- HEAP DUMP ---\n");
  header_t *current = kmem_heap_head;
  int i = 0;
  while (current) {
    com1_printf("Block %d: %p, size=%d, free=%d, next=%p, prev=%p\n", i++,
                current, (int)current->size, (int)current->is_free,
                current->next, current->prev);
    current = current->next;
  }
  com1_printf("Total free: %d\n", (int)kmem_free_bytes_internal());
  com1_printf("-----------------\n");
}

/*
 * kmem_alloc_aligned_internal: Allocates memory with a specific alignment.
 * Useful for DMA, Page Tables, etc.
 */
void *kmem_alloc_aligned_internal(unsigned long size, unsigned long align) {
  if (!kmem_heap_head)
    kmem_init_internal();
  if (size == 0)
    return 0;
  if (align <= 16)
    return kmem_alloc_internal(size);

  unsigned long payload_size = align16(size);
  unsigned long total_size = payload_size + (2 * KMEM_REDZONE_SZ);
  header_t *current = kmem_heap_head;

  while (current) {
    if (current->is_free) {
      unsigned long data_start =
          (unsigned long)current + sizeof(header_t) + KMEM_REDZONE_SZ;
      unsigned long aligned_payload = (data_start + align - 1) & ~(align - 1);
      unsigned long aligned_header =
          aligned_payload - KMEM_REDZONE_SZ - sizeof(header_t);
      unsigned long padding = aligned_header - (unsigned long)current;

      if (padding + total_size <= current->size) {
        if (padding >= sizeof(header_t) + 16) {
          header_t *aligned_block = (header_t *)((char *)current + padding);
          aligned_block->magic = KMEM_MAGIC;
          aligned_block->is_free = 1;
          aligned_block->size = current->size - padding;
          aligned_block->payload_size = 0;
          aligned_block->next = current->next;
          aligned_block->prev = current;

          if (aligned_block->next)
            aligned_block->next->prev = aligned_block;

          current->next = aligned_block;
          current->size = padding - sizeof(header_t);
          current->payload_size = 0;

          current = aligned_block;
          padding = 0;
        } else if (padding != 0) {
          current = current->next;
          continue;
        }

        if (padding == 0) {
          split_block(current, total_size);
          current->is_free = 0;
          current->payload_size = payload_size;
          u8 *base = (u8 *)current + sizeof(header_t);
          memset(base, KMEM_REDZONE_PAT, KMEM_REDZONE_SZ);
          memset(base + KMEM_REDZONE_SZ + current->payload_size,
                 KMEM_REDZONE_PAT, KMEM_REDZONE_SZ);
          return (void *)(base + KMEM_REDZONE_SZ);
        }
      }
    }
    current = current->next;
  }

  com1_printf("KMALLOC_ALIGNED FAILED! size: %d, align: %d\n", (int)size,
              (int)align);
  return 0;
}

unsigned long kmem_alloc_internal_usable_size(void *ptr) {
  if (!ptr)
    return 0;
  header_t *header =
      (header_t *)((char *)ptr - KMEM_REDZONE_SZ - sizeof(header_t));
  if (header->magic != KMEM_MAGIC)
    return 0;
  return header->payload_size;
}

void kmem_init(void) {
  kmem_init_internal();
}

void *kmem_alloc(unsigned long size) {
  return kmem_alloc_internal(size);
}

void kmem_free(void *ptr) {
  kmem_free_internal(ptr);
}

void *kmem_calloc(unsigned long nmemb, unsigned long size) {
  return kmem_calloc_internal(nmemb, size);
}

void *kmem_realloc(void *ptr, unsigned long size) {
  return kmem_realloc_internal(ptr, size);
}

void *kmem_alloc_aligned(unsigned long size, unsigned long align) {
  return kmem_alloc_aligned_internal(size, align);
}

unsigned long kmem_usable_size(void *ptr) {
  return kmem_alloc_internal_usable_size(ptr);
}

unsigned long kmem_free_bytes(void) {
  return kmem_free_bytes_internal();
}

int kmem_is_initialized(void) {
  return kmem_is_init_internal();
}

void kmem_dump(void) {
  kmem_dump_internal();
}

unsigned long kmem_total_bytes(void) {
  return (unsigned long)KMEM_HEAP_SIZE_VAL;
}

unsigned long kmem_used_bytes(void) {
  return KMEM_HEAP_SIZE_VAL - kmem_free_bytes_internal();
}
