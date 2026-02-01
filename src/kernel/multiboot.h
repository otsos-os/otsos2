/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <mlibc/mlibc.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_FLAG_MEM (1 << 0)
#define MULTIBOOT_FLAG_BOOTDEV (1 << 1)
#define MULTIBOOT_FLAG_CMDLINE (1 << 2)
#define MULTIBOOT_FLAG_MODS (1 << 3)
#define MULTIBOOT_FLAG_AOUT_SYMS (1 << 4)
#define MULTIBOOT_FLAG_ELF_SHDR (1 << 5)
#define MULTIBOOT_FLAG_MMAP (1 << 6)
#define MULTIBOOT_FLAG_DRIVES (1 << 7)
#define MULTIBOOT_FLAG_CONFIG (1 << 8)
#define MULTIBOOT_FLAG_BOOTLOADER_NAME (1 << 9)
#define MULTIBOOT_FLAG_APM (1 << 10)
#define MULTIBOOT_FLAG_VBE (1 << 11)
#define MULTIBOOT_FLAG_FRAMEBUFFER (1 << 12)

#define MULTIBOOT_MEMORY_AVAILABLE 1
#define MULTIBOOT_MEMORY_RESERVED 2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS 4
#define MULTIBOOT_MEMORY_BADRAM 5

typedef struct {
  u32 flags;
  u32 mem_lower;
  u32 mem_upper;
  u32 boot_device;
  u32 cmdline;
  u32 mods_count;
  u32 mods_addr;
  u32 syms[4];
  u32 mmap_length;
  u32 mmap_addr;
  u32 drives_length;
  u32 drives_addr;
  u32 config_table;
  u32 boot_loader_name;
  u32 apm_table;
  u32 vbe_control_info;
  u32 vbe_mode_info;
  u16 vbe_mode;
  u16 vbe_interface_seg;
  u16 vbe_interface_off;
  u16 vbe_interface_len;
  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
  u8 framebuffer_type;
  u8 color_info[6];
} __attribute__((packed)) multiboot_info_t;

typedef struct {
  u32 size;
  u64 base_addr;
  u64 length;
  u32 type;
} __attribute__((packed)) multiboot_mmap_entry_t;

static inline u64 multiboot_get_ram_kb(multiboot_info_t *mb_info) {
  if (!(mb_info->flags & MULTIBOOT_FLAG_MMAP)) {
    if (mb_info->flags & MULTIBOOT_FLAG_MEM) {
      return mb_info->mem_lower + mb_info->mem_upper + 1024;
    }
    return 0;
  }

  u64 total = 0;
  multiboot_mmap_entry_t *entry =
      (multiboot_mmap_entry_t *)(u64)mb_info->mmap_addr;
  u64 end = (u64)mb_info->mmap_addr + mb_info->mmap_length;

  while ((u64)entry < end) {
    if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      total += entry->length;
    }
    entry = (multiboot_mmap_entry_t *)((u64)entry + entry->size + 4);
  }

  return total / 1024;
}

#endif
