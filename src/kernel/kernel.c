#include <kernel/drivers/disk/pata/pata.h>
#include <kernel/drivers/fs/chainFS/chainfs.h>
#include <kernel/drivers/keyboard/keyboard.h>
#include <kernel/drivers/vga.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/interrupts/idt.h>
#include <lib/com1.h>
#include <mlibc/mlibc.h>

// Точка входа(если что-то сломалось то здесь)
void kmain(multiboot_info_t *mb_info) {
  init_idt();

  clear_scr();
  char *msg = "OTSOS started!";
  printf("%s", msg);

  com1_init();

  com1_write_string("OTSOS started at address ");
  com1_write_hex_qword((u64)kmain);
  com1_newline();

  char dir[256] =
      "/"; // Потом когда добавим файловую систему нужнл будет немного изменить

  keyboard_manager_init();
  fb_init(mb_info);

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