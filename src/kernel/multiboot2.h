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

#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

#include <mlibc/mlibc.h>

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36D76289

#define MULTIBOOT2_TAG_TYPE_END 0
#define MULTIBOOT2_TAG_TYPE_CMDLINE 1
#define MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME 2
#define MULTIBOOT2_TAG_TYPE_MODULE 3
#define MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO 4
#define MULTIBOOT2_TAG_TYPE_BOOTDEV 5
#define MULTIBOOT2_TAG_TYPE_MMAP 6
#define MULTIBOOT2_TAG_TYPE_VBE 7
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER 8
#define MULTIBOOT2_TAG_TYPE_ELF_SECTIONS 9
#define MULTIBOOT2_TAG_TYPE_APM 10
#define MULTIBOOT2_TAG_TYPE_EFI32 11
#define MULTIBOOT2_TAG_TYPE_EFI64 12
#define MULTIBOOT2_TAG_TYPE_SMBIOS 13
#define MULTIBOOT2_TAG_TYPE_ACPI_OLD 14
#define MULTIBOOT2_TAG_TYPE_ACPI_NEW 15
#define MULTIBOOT2_TAG_TYPE_NETWORK 16
#define MULTIBOOT2_TAG_TYPE_EFI_MMAP 17
#define MULTIBOOT2_TAG_TYPE_EFI_BS 18
#define MULTIBOOT2_TAG_TYPE_EFI32_IH 19
#define MULTIBOOT2_TAG_TYPE_EFI64_IH 20
#define MULTIBOOT2_TAG_TYPE_LOAD_BASE_ADDR 21

#define MULTIBOOT2_MEMORY_AVAILABLE 1
#define MULTIBOOT2_MEMORY_RESERVED 2
#define MULTIBOOT2_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT2_MEMORY_NVS 4
#define MULTIBOOT2_MEMORY_BADRAM 5

#define MULTIBOOT2_FRAMEBUFFER_TYPE_INDEXED 0
#define MULTIBOOT2_FRAMEBUFFER_TYPE_RGB 1
#define MULTIBOOT2_FRAMEBUFFER_TYPE_EGA_TEXT 2

typedef struct {
  u32 total_size;
  u32 reserved;
} __attribute__((packed)) multiboot2_info_t;

typedef struct {
  u32 type;
  u32 size;
} __attribute__((packed)) multiboot2_tag_t;

typedef struct {
  u32 type;
  u32 size;
  u32 mem_lower;
  u32 mem_upper;
} __attribute__((packed)) multiboot2_tag_basic_meminfo_t;

typedef struct {
  u64 base_addr;
  u64 length;
  u32 type;
  u32 reserved;
} __attribute__((packed)) multiboot2_mmap_entry_t;

typedef struct {
  u32 type;
  u32 size;
  u32 entry_size;
  u32 entry_version;
} __attribute__((packed)) multiboot2_tag_mmap_t;

typedef struct {
  u32 type;
  u32 size;
  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
  u8 framebuffer_type;
  u16 reserved;
} __attribute__((packed)) multiboot2_tag_framebuffer_t;

typedef struct {
  u32 type;
  u32 size;
  char string[];
} __attribute__((packed)) multiboot2_tag_cmdline_t;

typedef struct {
  u32 type;
  u32 size;
  char string[];
} __attribute__((packed)) multiboot2_tag_boot_loader_name_t;

typedef struct {
  u32 type;
  u32 size;
  u32 mod_start;
  u32 mod_end;
  char cmdline[];
} __attribute__((packed)) multiboot2_tag_module_t;

static inline multiboot2_tag_t *multiboot2_find_tag(multiboot2_info_t *mb_info,
                                                    u32 type) {
  multiboot2_tag_t *tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

  while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
    if (tag->type == type) {
      return tag;
    }
    u64 next_addr = (u64)tag + tag->size;
    next_addr = (next_addr + 7) & ~7;
    tag = (multiboot2_tag_t *)next_addr;
  }
  return 0;
}

static inline u64 multiboot2_get_ram_kb(multiboot2_info_t *mb_info) {
  multiboot2_tag_mmap_t *mmap = (multiboot2_tag_mmap_t *)multiboot2_find_tag(
      mb_info, MULTIBOOT2_TAG_TYPE_MMAP);

  if (!mmap) {
    multiboot2_tag_basic_meminfo_t *basic =
        (multiboot2_tag_basic_meminfo_t *)multiboot2_find_tag(
            mb_info, MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO);
    if (basic) {
      return basic->mem_lower + basic->mem_upper + 1024;
    }
    return 0;
  }

  u64 total = 0;
  u8 *end = (u8 *)mmap + mmap->size;
  multiboot2_mmap_entry_t *entry = (multiboot2_mmap_entry_t *)((u8 *)mmap + 16);

  while ((u8 *)entry < end) {
    if (entry->type == MULTIBOOT2_MEMORY_AVAILABLE) {
      total += entry->length;
    }
    entry = (multiboot2_mmap_entry_t *)((u8 *)entry + mmap->entry_size);
  }

  return total / 1024;
}

#endif
