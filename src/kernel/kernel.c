#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/interrupts/idt.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

extern void cpuid_get(u32 code, u32 *res);

extern void cinfo(char *buf);
extern u64 rinfo(u64 mb_ptr);
// Точка входа(если что-то сломалось то здесь)

void kmain(u64 addr) {
  multiboot_info_t *mboot_ptr = (multiboot_info_t *)addr;
  init_idt();

  fb_init(mboot_ptr);

  clear_scr();

  com1_init();
  com1_write_string("OTSOS started at address 0x");
  com1_write_hex_qword(addr);
  com1_newline();

  char cpu_buf[64];
  cinfo(cpu_buf);

  char *p = cpu_buf;
  while (*p == ' ')
    p++;

  u64 ram_kb = rinfo(addr);

  com1_printf("CPU: %s\n", p);
  com1_printf("RAM: %u MB (%u KB)\n", ram_kb / 1024, ram_kb);

  if (is_framebuffer_enabled()) {
    printf("CPU: %s\n", p);
    printf("RAM: %u MB\n", ram_kb / 1024);
  }

  char dir[256] =
      "/"; // Потом когда добавим файловую систему нужнл будет немного изменить

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
      printf("%c", c);
    }
  }
}