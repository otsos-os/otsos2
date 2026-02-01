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

#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/interrupts/idt.h>
#include <kernel/mmu.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdlib.h>
#include <userland/userspace.h>
#include <kernel/posix/posix.h>

extern void cpuid_get(u32 code, u32 *res);
extern void cinfo(char *buf);
extern u64 rinfo(u64 mb_ptr);

static u32 boot_magic = 0;
static int is_multiboot2 = 0;

static void debug_multiboot_info(multiboot_info_t *mb_info) {
  com1_write_string("Flags: 0x");
  com1_write_hex_dword(mb_info->flags);
  com1_newline();

  if (mb_info->flags & MULTIBOOT_FLAG_MEM) {
    com1_write_string("mem_lower: ");
    com1_write_dec(mb_info->mem_lower);
    com1_write_string(" KB, mem_upper: ");
    com1_write_dec(mb_info->mem_upper);
    com1_write_string(" KB\n");
  }

  if (mb_info->flags & MULTIBOOT_FLAG_CMDLINE) {
    com1_write_string("cmdline: ");
    com1_write_string((const char *)(u64)mb_info->cmdline);
    com1_newline();
  }

  if (mb_info->flags & MULTIBOOT_FLAG_BOOTLOADER_NAME) {
    com1_write_string("bootloader: ");
    com1_write_string((const char *)(u64)mb_info->boot_loader_name);
    com1_newline();
  }

  if (mb_info->flags & MULTIBOOT_FLAG_FRAMEBUFFER) {
    com1_write_string("framebuffer: ");
    com1_write_dec(mb_info->framebuffer_width);
    com1_write_string("x");
    com1_write_dec(mb_info->framebuffer_height);
    com1_write_string("x");
    com1_write_dec(mb_info->framebuffer_bpp);
    com1_newline();
  }
}

static void debug_multiboot2_tags(multiboot2_info_t *mb_info) {
  multiboot2_tag_t *tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

  while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
    com1_write_string("Tag type: ");
    com1_write_dec(tag->type);
    com1_write_string(", size: ");
    com1_write_dec(tag->size);

    switch (tag->type) {
    case MULTIBOOT2_TAG_TYPE_CMDLINE:
      com1_write_string(" (CMDLINE)");
      break;
    case MULTIBOOT2_TAG_TYPE_BOOT_LOADER_NAME:
      com1_write_string(" (BOOT_LOADER_NAME)");
      break;
    case MULTIBOOT2_TAG_TYPE_BASIC_MEMINFO:
      com1_write_string(" (BASIC_MEMINFO)");
      break;
    case MULTIBOOT2_TAG_TYPE_MMAP:
      com1_write_string(" (MMAP)");
      break;
    case MULTIBOOT2_TAG_TYPE_FRAMEBUFFER:
      com1_write_string(" (FRAMEBUFFER)");
      break;
    case MULTIBOOT2_TAG_TYPE_ELF_SECTIONS:
      com1_write_string(" (ELF_SECTIONS)");
      break;
    case MULTIBOOT2_TAG_TYPE_ACPI_OLD:
      com1_write_string(" (ACPI_OLD)");
      break;
    case MULTIBOOT2_TAG_TYPE_ACPI_NEW:
      com1_write_string(" (ACPI_NEW)");
      break;
    default:
      com1_write_string(" (other)");
      break;
    }
    com1_newline();

    u64 next_addr = (u64)tag + tag->size;
    next_addr = (next_addr + 7) & ~7;
    tag = (multiboot2_tag_t *)next_addr;
  }
}

void kmain(u64 magic, u64 addr) {
  com1_init();
  com1_set_mirror_callback(vga_putc);
  init_idt();
  timer_init(1000);
  mmu_init();

  boot_magic = (u32)magic;

  if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
    is_multiboot2 = 1;
    com1_write_string("boot from mb2 (magic: 0x");
    com1_write_hex_dword(boot_magic);
    com1_write_string(")\n");

    multiboot2_info_t *mboot_ptr = (multiboot2_info_t *)addr;
    debug_multiboot2_tags(mboot_ptr);
    fb_init_mb2(mboot_ptr);

    clear_scr();

    char cpu_buf[64];
    cinfo(cpu_buf);
    char *p = cpu_buf;
    while (*p == ' ')
      p++;

    u64 ram_kb = multiboot2_get_ram_kb(mboot_ptr);

    com1_printf("CPU: %s\n", p);
    com1_printf("RAM: %u MB (%u KB)\n", ram_kb / 1024, ram_kb);

    if (is_framebuffer_enabled()) {
      printf("CPU: %s\n", p);
      printf("RAM: %u MB\n", ram_kb / 1024);
    }

  } else if (boot_magic == MULTIBOOT_BOOTLOADER_MAGIC) {
    is_multiboot2 = 0;
    com1_write_string("boot from mb1 (magic: 0x");
    com1_write_hex_dword(boot_magic);
    com1_write_string(")\n");

    multiboot_info_t *mboot_ptr = (multiboot_info_t *)addr;
    debug_multiboot_info(mboot_ptr);
    com1_off_mirror_callback();

    fb_init(mboot_ptr);

    char cpu_buf[64];
    cinfo(cpu_buf);
    char *p = cpu_buf;
    while (*p == ' ')
      p++;

    u64 ram_kb = multiboot_get_ram_kb(mboot_ptr);

    com1_printf("CPU: %s\n", p);
    com1_printf("RAM: %u MB (%u KB)\n", ram_kb / 1024, ram_kb);

    if (is_framebuffer_enabled()) {
      printf("CPU: %s\n", p);
      printf("RAM: %u MB\n", ram_kb / 1024);
    }

  } else {
    panic("ERROR: Unknown bootloader magic: 0x%x\nExpected MB1: 0x%x or MB2: "
          "0x%x\n",
          boot_magic, MULTIBOOT_BOOTLOADER_MAGIC, MULTIBOOT2_BOOTLOADER_MAGIC);
  }

  sleep(430);
  com1_off_mirror_callback();
  clear_scr();

  keyboard_manager_init();

  printf("\nDo you want to enable debug mode (dont use for default use it make "
         "you screen dirty)? [y/n]\n");
  while (1) {
    char c = keyboard_getchar();
    if (c == 'y') {
      com1_set_mirror_callback(vga_putc);
      clear_scr();
      com1_write_string("test debug mod\n");
      break;
    } else if (c == 'n') {
      break;
    }
  }

  init_heap();
  pata_identify(NULL);
  posix_init();

  userspace_init();

  void *init_module_start = NULL;
  u32 init_module_size = 0;

  if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
    multiboot2_info_t *mboot_ptr = (multiboot2_info_t *)addr;
    multiboot2_tag_module_t *mod =
        (multiboot2_tag_module_t *)multiboot2_find_tag(
            mboot_ptr, MULTIBOOT2_TAG_TYPE_MODULE);

    if (mod) {
      init_module_start = (void *)(u64)mod->mod_start;
      init_module_size = mod->mod_end - mod->mod_start;
    }
  }

  if (init_module_start && init_module_size > 0) {
    com1_printf("[KERNEL] Found init module at %p, size %d. Starting init...\n",
                init_module_start, init_module_size);
    userspace_load_init(init_module_start, (u64)init_module_size);
  } else {
    com1_printf(
        "[KERNEL] Init module not found! Falling back to kernel loop...\n");
    while (1) {
      char c = keyboard_getchar();
      if (c) {
        printf("\033[31m %c \033[0m", c);
      }
    }
  }
}