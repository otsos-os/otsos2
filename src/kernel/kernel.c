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

#include <kernel/drivers/acpi/acpi.h>
#include <kernel/drivers/disk/disk.h>
#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/disk/ramdisk/ramdisk.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/power/power.h>
#include <kernel/drivers/timer.h>
#include <kernel/drivers/tty.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/watchdog/watchdog.h>
#include <kernel/interrupts/idt.h>
#include <kernel/mmu.h>
#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <kernel/panic.h>
#include <kernel/pci/pci.h>
#include <kernel/posix/posix.h>
#include <kernel/syscall.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>
#include <mlibc/stdlib.h>
#include <userland/userspace.h>

extern void cpuid_get(u32 code, u32 *res);
extern void cinfo(char *buf);
extern u64 rinfo(u64 mb_ptr);
extern char start;
extern char kernel_end;

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

static int mb2_find_module(multiboot2_info_t *mb_info, const char *name,
                           void **out_start, u32 *out_size) {
  multiboot2_tag_t *tag = (multiboot2_tag_t *)((u8 *)mb_info + 8);

  while (tag->type != MULTIBOOT2_TAG_TYPE_END) {
    if (tag->type == MULTIBOOT2_TAG_TYPE_MODULE) {
      multiboot2_tag_module_t *mod = (multiboot2_tag_module_t *)tag;
      if (strcmp(mod->cmdline, name) == 0) {
        if (out_start) {
          *out_start = (void *)(u64)mod->mod_start;
        }
        if (out_size) {
          *out_size = mod->mod_end - mod->mod_start;
        }
        return 0;
      }
    }
    u64 next_addr = (u64)tag + tag->size;
    next_addr = (next_addr + 7) & ~7;
    tag = (multiboot2_tag_t *)next_addr;
  }
  return -1;
}

static void status_line(const char *label, int ok) {
  const int pad_col = 32;
  int len = strlen(label);
  printf("%s", label);
  com1_printf("%s", label);
  for (int i = len; i < pad_col; i++) {
    vga_putc(' ');
    com1_printf(" ");
  }
  if (ok) {
    printf("\033[32m[OK]\033[0m\n");

  } else {
    printf("\033[31m[FAILED]\033[0m\n");
  }
}

static int disk_has_type(disk_type_t type) {
  int count = disk_count();
  for (int i = 0; i < count; i++) {
    disk_t *disk = disk_get(i);
    if (disk && disk->type == type) {
      return 1;
    }
  }
  return 0;
}

static disk_t *disk_find_type(disk_type_t type) {
  int count = disk_count();
  for (int i = 0; i < count; i++) {
    disk_t *disk = disk_get(i);
    if (disk && disk->type == type) {
      return disk;
    }
  }
  return NULL;
}

static int timer_sanity_check(void) {
  if (!timer_is_initialized()) {
    return 0;
  }
  u64 start = timer_get_ticks();
  for (u64 i = 0; i < 1000000 && timer_get_ticks() == start; i++) {
    __asm__ volatile("pause");
  }
  return timer_get_ticks() != start;
}

static void enable_sse(void) {
  u64 cr0, cr4;
  __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
  cr0 &= ~(1ULL << 2); // CR0.EM = 0
  cr0 &= ~(1ULL << 3); // CR0.TS = 0
  cr0 |= (1ULL << 1);  // CR0.MP = 1
  __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));

  __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
  cr4 |= (1ULL << 9);  // CR4.OSFXSR = 1
  cr4 |= (1ULL << 10); // CR4.OSXMMEXCPT = 1
  __asm__ volatile("mov %0, %%cr4" : : "r"(cr4));
}

static void ensure_dev_nodes(void) {
  if (g_chainfs.superblock.magic != CHAINFS_MAGIC) {
    return;
  }

  chainfs_file_entry_t entry;
  u32 entry_block, entry_offset;

  if (chainfs_resolve_path("/dev", &entry, &entry_block, &entry_offset) != 0) {
    chainfs_mkdir("/dev");
  } else if (entry.type != CHAINFS_TYPE_DIR) {
    com1_printf("[CHAINFS] /dev exists but is not a directory\n");
    return;
  }

  if (chainfs_resolve_path("/dev/tty", &entry, &entry_block, &entry_offset) !=
      0) {
    chainfs_mknod("/dev/tty", TTY_DEVICE_MAJOR, TTY_DEVICE_MINOR_TTY);
  } else if (entry.type != CHAINFS_TYPE_DEV) {
    if (entry.type == CHAINFS_TYPE_FILE) {
      chainfs_delete_file("/dev/tty");
      chainfs_mknod("/dev/tty", TTY_DEVICE_MAJOR, TTY_DEVICE_MINOR_TTY);
    } else {
      com1_printf("[CHAINFS] /dev/tty exists but is not a device node\n");
    }
  }

  if (chainfs_resolve_path("/dev/console", &entry, &entry_block,
                           &entry_offset) != 0) {
    chainfs_mknod("/dev/console", TTY_DEVICE_MAJOR, TTY_DEVICE_MINOR_CONSOLE);
  } else if (entry.type != CHAINFS_TYPE_DEV) {
    if (entry.type == CHAINFS_TYPE_FILE) {
      chainfs_delete_file("/dev/console");
      chainfs_mknod("/dev/console", TTY_DEVICE_MAJOR, TTY_DEVICE_MINOR_CONSOLE);
    } else {
      com1_printf("[CHAINFS] /dev/console exists but is not a device node\n");
    }
  }
}

void kmain(u64 magic, u64 addr, u64 boot_option) {
  int safe_mode = (boot_option == 1);
  int debug_mode = (boot_option == 2);

  com1_init();
  com1_set_mirror_callback(tty_com1_mirror);
  init_heap();
  init_idt();
  timer_init(1000);
  mmu_init();
  enable_sse();
  __asm__ volatile("sti");

  // posix_init() moved down
  syscall_init();

  disk_manager_init();
  pata_identify(NULL);

  // Create a 4MB ramdisk at 0x4000000 (ensure this doesn't overlap
  // kernel/bootstack)
  ramdisk_init((void *)0x4000000, 4 * 1024 * 1024);

  mmu_clear_user_range((u64)&start, (u64)&kernel_end);

  boot_magic = (u32)magic;

  if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
    is_multiboot2 = 1;
    com1_write_string("boot from mb2 (magic: 0x");
    com1_write_hex_dword(boot_magic);
    com1_write_string(")\n");

    multiboot2_info_t *mboot_ptr = (multiboot2_info_t *)addr;
    debug_multiboot2_tags(mboot_ptr);
    fb_init_mb2(mboot_ptr);

    acpi_init_from_multiboot2(mboot_ptr);

    clear_scr();
    tty_init();

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
    fb_init(mboot_ptr);
    clear_scr();
    tty_init();

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

  pci_set_verbose_scan(debug_mode);
  if (debug_mode) {
    com1_printf("[BOOT] Debug mode: verbose PCI scan enabled\n");
  }

  power_init();
  pci_init();
  watchdog_init();
  if (watchdog_device_count() > 0) {
    if (watchdog_start(WATCHDOG_DEFAULT_TIMEOUT_SEC) != 0) {
      panic("[WDT] failed to start watchdog\n");
    }
  }
  if (acpi_is_initialized()) {
    power_acpi_enable();
  }

  int heap_ok = kheap_is_initialized() && kget_free_memory() > 0;
  int idt_ok = idt_is_loaded();
  int timer_ok = timer_sanity_check();
  int mmu_ok = mmu_is_initialized() && mmu_read_cr3() != 0;
  int syscall_ok = syscall_is_initialized();
  int disk_ok = disk_manager_is_initialized();
  int pata_ok = disk_has_type(DISK_TYPE_PATA);
  int ramdisk_ok = disk_has_type(DISK_TYPE_RAM);
  int fb_ok = is_framebuffer_enabled() != 0;
  int acpi_ok = acpi_is_initialized();
  int power_ok = power_is_initialized();
  int pci_ok = pci_is_initialized();
  int watchdog_ok = watchdog_is_initialized() && watchdog_device_count() > 0;

  status_line("heap", heap_ok);
  status_line("idt", idt_ok);
  status_line("timer", timer_ok);
  status_line("mmu", mmu_ok);
  status_line("syscall", syscall_ok);
  status_line("disk manager", disk_ok);
  status_line("pata identify", pata_ok);
  status_line("ramdisk", ramdisk_ok);
  status_line("framebuffer", fb_ok);
  status_line("acpi", acpi_ok);
  status_line("power", power_ok);
  status_line("pci scan", pci_ok);
  status_line("watchdog", watchdog_ok);

  sleep(430);

  keyboard_manager_init();

  tty_set_active(1);

  printf("\nSelect boot disk:\n");
  printf("1. Hard Drive (PATA)\n");
  printf("2. Live USB (RAM Disk)\n");

  disk_t *selected_disk = NULL;
  disk_t *pata_disk = disk_find_type(DISK_TYPE_PATA);
  disk_t *ram_disk = disk_find_type(DISK_TYPE_RAM);
  while (1) {
    char c = keyboard_getchar();
    if (c == '1') {
      if (!pata_disk) {
        printf("PATA disk not available, fallback to RAM disk\n");
        if (ram_disk) {
          selected_disk = ram_disk;
          printf("Selected RAM Disk (%s)\n", selected_disk->name);
          break;
        }
        printf("RAM disk also not available\n");
        continue;
      }
      selected_disk = pata_disk;
      printf("Selected PATA (%s)\n", selected_disk->name);
      break;
    } else if (c == '2') {
      if (!ram_disk) {
        printf("RAM disk not available\n");
        continue;
      }
      selected_disk = ram_disk;
      printf("Selected RAM Disk (%s)\n", selected_disk->name);
      break;
    }
  }

  int fs_ok = (chainfs_init(selected_disk) == 0);
  status_line("chainfs init", fs_ok);
  if (!fs_ok) {
    com1_printf("[CHAINFS] init failed, formatting disk...\n");
    u32 format_blocks = 64;
    if (selected_disk && selected_disk->total_sectors > 0 &&
        selected_disk->total_sectors < format_blocks) {
      format_blocks = selected_disk->total_sectors;
    }
    int fmt_ok = (chainfs_format(format_blocks, 8) == 0);
    status_line("chainfs format", fmt_ok);
    fs_ok = fmt_ok;
  }
  status_line("chainfs ready", fs_ok);
  if (fs_ok) {
    ensure_dev_nodes();
  } else {
    com1_printf("[CHAINFS] filesystem unavailable, skipping userspace startup\n");
  }

  if (!safe_mode && fs_ok) {
    // pata_identify already called

    while (keyboard_getchar() != 0)
      ;

    userspace_init();

    void *init_module_start = NULL;
    u32 init_module_size = 0;
    void *yes_module_start = NULL;
    u32 yes_module_size = 0;
    void *fetch_module_start = NULL;
    u32 fetch_module_size = 0;

    if (boot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
      multiboot2_info_t *mboot_ptr = (multiboot2_info_t *)addr;
      mb2_find_module(mboot_ptr, "init", &init_module_start, &init_module_size);
      mb2_find_module(mboot_ptr, "yes", &yes_module_start, &yes_module_size);
      mb2_find_module(mboot_ptr, "fetch", &fetch_module_start,
                      &fetch_module_size);
    }

    posix_init();

    chainfs_mkdir("/bin");

    if (yes_module_start && yes_module_size > 0) {
      int res = chainfs_write_file("/bin/yes", (const u8 *)yes_module_start,
                                   yes_module_size);
      if (res == 0) {
        com1_printf("[KERNEL] Installed /bin/yes from module (%u bytes)\n",
                    yes_module_size);
      } else {
        com1_printf("[KERNEL] Failed to install /bin/yes from module\n");
      }
    }

    if (fetch_module_start && fetch_module_size > 0) {
      int res = chainfs_write_file("/bin/fetch", (const u8 *)fetch_module_start,
                                   fetch_module_size);
      if (res == 0) {
        com1_printf("[KERNEL] Installed /bin/fetch from module (%u bytes)\n",
                    fetch_module_size);
      } else {
        com1_printf("[KERNEL] Failed to install /bin/fetch from module\n");
      }
    }

    if (init_module_start && init_module_size > 0) {
      com1_printf(
          "[KERNEL] Found init module at %p, size %d. Starting init...\n",
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
  } else {
    if (!fs_ok) {
      printf("\n\033[31mChainFS unavailable. Staying in kernel console.\033[0m\n");
    }
    if (safe_mode) {
      printf("\n\033[33m--- SAFE MOD ---\033[0m\n");
      printf("init and userlang are disabled.\n");
    } else if (debug_mode) {
      printf("\n\033[36m--- DEBUG MODE ---\033[0m\n");
      printf("userspace disabled because filesystem is unavailable.\n");
    }

    while (1) {
      char c = keyboard_getchar();
      if (c) {
        printf("%c", c);
      }
    }
  }
}
