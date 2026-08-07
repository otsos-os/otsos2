/*
 * Copyright (c) 2026, otsos team
 *
 * BSD 2-clause license.
 */

#include <kernel/bootmem.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <mlibc/stdio.h>
#include <mlibc/mlibc.h>

#define BOOTMEM_MAX_RANGES 64
#define PAGE_SIZE 4096

typedef struct {
  u32 mod_start;
  u32 mod_end;
  u32 string;
  u32 reserved;
} __attribute__((packed)) multiboot_module_t;

static bootmem_range_t free_ranges[BOOTMEM_MAX_RANGES];
static u32 free_count;
static u64 highest_addr;
static int initialized;
static void (*reserve_cb)(u64 phys_start, u64 size);

static u64 align_up(u64 val, u64 align) {
  if (align == 0)
    return val;
  return (val + align - 1) & ~(align - 1);
}

static u64 align_down(u64 val, u64 align) {
  if (align == 0)
    return val;
  return val & ~(align - 1);
}

static void add_free_range(u64 start, u64 end) {
  start = align_up(start, PAGE_SIZE);
  end = align_down(end, PAGE_SIZE);
  if (end <= start || free_count >= BOOTMEM_MAX_RANGES)
    return;

  free_ranges[free_count].start = start;
  free_ranges[free_count].end = end;
  free_count++;

  if (end > highest_addr)
    highest_addr = end;
}

static void reserve_range(u64 start, u64 end) {
  start = align_down(start, PAGE_SIZE);
  end = align_up(end, PAGE_SIZE);
  if (end <= start)
    return;

  for (u32 i = 0; i < free_count; i++) {
    bootmem_range_t r = free_ranges[i];
    if (end <= r.start || start >= r.end)
      continue;

    if (start <= r.start && end >= r.end) {
      free_ranges[i] = free_ranges[free_count - 1];
      free_count--;
      i--;
      continue;
    }

    if (start <= r.start) {
      free_ranges[i].start = end;
      continue;
    }

    if (end >= r.end) {
      free_ranges[i].end = start;
      continue;
    }

    if (free_count < BOOTMEM_MAX_RANGES) {
      free_ranges[i].end = start;
      free_ranges[free_count].start = end;
      free_ranges[free_count].end = r.end;
      free_count++;
    } else {
      free_ranges[i].end = start;
    }
  }
}

static void load_mb1_mmap(multiboot_info_t *mb) {
  if (mb->flags & MULTIBOOT_FLAG_MMAP) {
    multiboot_mmap_entry_t *entry =
        (multiboot_mmap_entry_t *)(u64)mb->mmap_addr;
    u64 end = (u64)mb->mmap_addr + mb->mmap_length;

    while ((u64)entry < end) {
      if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
        add_free_range(entry->base_addr, entry->base_addr + entry->length);
      entry = (multiboot_mmap_entry_t *)((u64)entry + entry->size + 4);
    }
    return;
  }

  if (mb->flags & MULTIBOOT_FLAG_MEM)
    add_free_range(0x100000, 0x100000 + ((u64)mb->mem_upper * 1024));
}

static void reserve_mb1(multiboot_info_t *mb) {
  reserve_range((u64)mb, (u64)mb + sizeof(*mb));

  if (mb->flags & MULTIBOOT_FLAG_MMAP)
    reserve_range((u64)mb->mmap_addr, (u64)mb->mmap_addr + mb->mmap_length);

  if (mb->flags & MULTIBOOT_FLAG_MODS) {
    multiboot_module_t *mods = (multiboot_module_t *)(u64)mb->mods_addr;
    reserve_range((u64)mods, (u64)mods + mb->mods_count * sizeof(*mods));
    for (u32 i = 0; i < mb->mods_count; i++)
      reserve_range(mods[i].mod_start, mods[i].mod_end);
  }
}

static void load_mb2_mmap(multiboot2_info_t *mb) {
  multiboot2_tag_mmap_t *mmap = (multiboot2_tag_mmap_t *)multiboot2_find_tag(
      mb, MULTIBOOT2_TAG_TYPE_MMAP);

  if (!mmap) {
    multiboot2_tag_basic_meminfo_t *basic =
        (multiboot2_tag_basic_meminfo_t *)multiboot2_find_tag(
            mb, MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO);
    if (basic)
      add_free_range(0x100000, 0x100000 + ((u64)basic->mem_upper * 1024));
    return;
  }

  u8 *end = (u8 *)mmap + mmap->size;
  multiboot2_mmap_entry_t *entry = (multiboot2_mmap_entry_t *)((u8 *)mmap + 16);

  while ((u8 *)entry < end) {
    if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE)
      add_free_range(entry->base_addr, entry->base_addr + entry->length);
    entry = (multiboot2_mmap_entry_t *)((u8 *)entry + mmap->entry_size);
  }
}

static void reserve_mb2(multiboot2_info_t *mb) {
  reserve_range((u64)mb, (u64)mb + mb->total_size);

  multiboot2_tag_t *tag = (multiboot2_tag_t *)((u8 *)mb + 8);
  while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
    if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
      multiboot2_tag_module_t *mod = (multiboot2_tag_module_t *)tag;
      reserve_range(mod->mod_start, mod->mod_end);
    }

    u64 next = (u64)tag + tag->size;
    next = (next + 7) & ~7ULL;
    tag = (multiboot2_tag_t *)next;
  }
}

void bootmem_init(u64 magic, u64 info_addr, u64 kernel_start, u64 kernel_end) {
  memset(free_ranges, 0, sizeof(free_ranges));
  free_count = 0;
  highest_addr = 0;
  initialized = 0;

  if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
    multiboot2_info_t *mb = (multiboot2_info_t *)info_addr;
    load_mb2_mmap(mb);
    reserve_mb2(mb);
  } else if (magic == MULTIBOOT_BOOTLOADER_MAGIC) {
    multiboot_info_t *mb = (multiboot_info_t *)info_addr;
    load_mb1_mmap(mb);
    reserve_mb1(mb);
  }

  reserve_range(0, 0x100000);
  reserve_range(kernel_start, kernel_end);

  initialized = 1;
  printk("[BOOTMEM] initialized: %u ranges, free=%u KB\n", free_count,
              bootmem_free_bytes() / 1024);
}

void *bootmem_alloc(u64 size, u64 align) {
  if (!initialized)
    return NULL;
  if (align < PAGE_SIZE)
    align = PAGE_SIZE;
  size = align_up(size, PAGE_SIZE);

  for (u32 i = 0; i < free_count; i++) {
    u64 addr = align_up(free_ranges[i].start, align);
    u64 end = addr + size;
    if (end > free_ranges[i].end)
      continue;

    free_ranges[i].start = end;
    if (free_ranges[i].start >= free_ranges[i].end) {
      free_ranges[i] = free_ranges[free_count - 1];
      free_count--;
    }
    if (reserve_cb)
      reserve_cb(addr, size);
    return (void *)(addr + DMAP_BASE);
  }

  return NULL;
}

u64 bootmem_free_bytes(void) {
  u64 total = 0;
  for (u32 i = 0; i < free_count; i++)
    total += free_ranges[i].end - free_ranges[i].start;
  return total;
}

u64 bootmem_highest_addr(void) { return highest_addr; }

u32 bootmem_range_count(void) { return free_count; }

const bootmem_range_t *bootmem_ranges(void) { return free_ranges; }
void bootmem_set_reserve_cb(void (*cb)(u64 phys_start, u64 size)) {
  reserve_cb = cb;
}

void bootmem_dump(void) {
  printk("--- bootmem ranges ---\n");
  for (u32 i = 0; i < free_count; i++) {
    printk("%u: %p - %p (%u KB)\n", i, (void *)free_ranges[i].start,
                (void *)free_ranges[i].end,
                (free_ranges[i].end - free_ranges[i].start) / 1024);
  }
  printk("----------------------\n");
}
