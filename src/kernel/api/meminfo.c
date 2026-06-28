/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#include <kernel/api/api.h>
#include <kernel/useraddr.h>
#include <kernel/bootmem.h>
#include <kernel/process.h>
#include <mm/vm/vm_page.h>
#include <mm/kmem.h>
#include <mlibc/mlibc.h>

int api_meminfo(struct api_meminfo *buf) {
  if (!is_user_address(buf, sizeof(struct api_meminfo))) {
    return -API_ERR_BAD_ADDR;
  }

  memset(buf, 0, sizeof(struct api_meminfo));

  u64 highest = bootmem_highest_addr();
  buf->ram_total_kb = highest / 1024;

  u64 free_pages = vm_page_count_free();
  u64 total_pages = vm_page_count_total();
  buf->ram_free_kb = (free_pages * 4096) / 1024;

  buf->pages_total = total_pages;
  buf->pages_free = free_pages;

  vm_page_queue_counts(&buf->pages_active, &buf->pages_inactive,
                       &buf->pages_cache, &buf->pages_wired);

  buf->user_heap_base = MMAP_BASE;
  buf->user_heap_size_kb = (MMAP_LIMIT - MMAP_BASE) / 1024;
  buf->mmap_base = MMAP_BASE;
  buf->mmap_limit = MMAP_LIMIT;

  return 0;
}

int api_kmeminfo(struct api_kmeminfo *buf) {
  process_t *proc = process_current();
  if (!proc || !proc->kusr_auth) {
    return -API_ERR_PERM;
  }

  if (!is_user_address(buf, sizeof(struct api_kmeminfo))) {
    return -API_ERR_BAD_ADDR;
  }

  memset(buf, 0, sizeof(struct api_kmeminfo));

  buf->kmem_heap_total_kb = kmem_total_bytes() / 1024;
  buf->kmem_heap_used_kb = kmem_used_bytes() / 1024;
  buf->kmem_heap_free_kb = kmem_free_bytes() / 1024;
  buf->bootmem_free_kb = bootmem_free_bytes() / 1024;

  return 0;
}
