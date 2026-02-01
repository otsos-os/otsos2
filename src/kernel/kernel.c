#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/interrupts/idt.h>
#include <kernel/multiboot2.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

extern void cpuid_get(u32 code, u32 *res);

extern void cinfo(char *buf);
extern u64 rinfo(u64 mb_ptr);
// Debug: print all Multiboot2 tags
static void debug_multiboot2_tags(multiboot2_info_t *mb_info) {
  com1_init();
  com1_write_string("\n=== Multiboot2 Tags Debug ===\n");

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

    /* Move to next tag (8-byte aligned) */
    u64 next_addr = (u64)tag + tag->size;
    next_addr = (next_addr + 7) & ~7;
    tag = (multiboot2_tag_t *)next_addr;
  }
  com1_write_string("=== End Tags ===\n\n");
}

// Multiboot2 entry point
void kmain(u64 addr) {
  multiboot2_info_t *mboot_ptr = (multiboot2_info_t *)addr;
  init_idt();

  /* Debug: print all tags first */
  debug_multiboot2_tags(mboot_ptr);

  /* Initialize framebuffer using Multiboot2 */
  fb_init_mb2(mboot_ptr);

  clear_scr();

  com1_init();
  com1_write_string("OTSOS started (Multiboot2) at address 0x");
  com1_write_hex_qword(addr);
  com1_newline();

  com1_write_string("Multiboot2 info total_size: ");
  com1_write_dec(mboot_ptr->total_size);
  com1_newline();

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

  char dir[256] =
      "/"; // Потом когда добавим файловую систему нужно будет немного изменить

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
  while (1) {
    char c = keyboard_getchar();
    if (c) {
      printf("\033[31m %c \033[0m", c);
    }
  }
}